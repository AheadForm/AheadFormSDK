#include "Video.h"
#include "Time.h"

#include <gst/app/gstappsink.h>
#include <sys/eventfd.h>
#include <unistd.h>

static int NotifyVideoReaders(AHEADFORM_VIDEO_CTRL *pstVideoCtrl)
{
	AHEADFORM_VIDEO_READER *pstReader = NULL;
	int ret;

	if ( (ret = pthread_mutex_lock(&pstVideoCtrl->readerListMutex)) != RET_SUCCESS )
	{
		printf("%s() call pthread_mutex_lock(video reader list) failed: %s\r\n", __FUNCTION__, strerror(ret));
		return ERR_MUTEX_OP_FAILED;
	}

	for (pstReader = pstVideoCtrl->pstReaders; pstReader != NULL; pstReader = pstReader->pNext)
	{
		if (pstReader->rxDataUpdateEventfd > 0 && eventfd_write(pstReader->rxDataUpdateEventfd, 1) < 0 && errno != EAGAIN)
		{
			printf("%s() call eventfd_write(video fd = %d) failed: %s\r\n", __FUNCTION__, pstReader->rxDataUpdateEventfd, strerror(errno));
			ret = ERR_EVENTFD_IO_FAILED;
		}
	}
	pthread_mutex_unlock(&pstVideoCtrl->readerListMutex);
	return ret;
}

static GstBusSyncReply CallBack_OnVideoCaptureBusMessageReceived(GstBus *pBus, GstMessage *pMessage, gpointer pUserData)
{
	AHEADFORM_VIDEO_CTRL *pstVideoCtrl = (AHEADFORM_VIDEO_CTRL*)pUserData;
	GError *pError = NULL;
	gchar *pDebug = NULL;

	if (GST_MESSAGE_TYPE(pMessage) == GST_MESSAGE_ERROR)
	{
		gst_message_parse_error(pMessage, &pError, &pDebug);
		printf("%s() video pipeline error: %s\r\n", __FUNCTION__, pError != NULL ? pError->message : "no GStreamer error detail");
		g_clear_error(&pError);
		g_free(pDebug);
		__atomic_store_n(&pstVideoCtrl->lastError, ERRCODE_RX_FAILED, __ATOMIC_SEQ_CST);
		NotifyVideoReaders(pstVideoCtrl);
	}
	else if (GST_MESSAGE_TYPE(pMessage) == GST_MESSAGE_EOS)
	{
		printf("%s() video pipeline reached unexpected EOS.\r\n", __FUNCTION__);
		__atomic_store_n(&pstVideoCtrl->lastError, ERRCODE_RX_FAILED, __ATOMIC_SEQ_CST);
		NotifyVideoReaders(pstVideoCtrl);
	}
	gst_message_unref(pMessage);
	return GST_BUS_DROP;
}

static unsigned char IsRecoverableH264Frame(const unsigned char *pData, size_t dataLen)
{
	size_t i, prefixLen;
	unsigned char hasSps = 0, hasPps = 0, hasIdr = 0;

	for (i = 0; i + 3 < dataLen; i++)
	{
		if (pData[i] != 0 || pData[i + 1] != 0)
			continue;

		prefixLen = 0;
		if (pData[i + 2] == 1)
			prefixLen = 3;
		else if (i + 4 < dataLen && pData[i + 2] == 0 && pData[i + 3] == 1)
			prefixLen = 4;

		if (prefixLen == 0)
			continue;

		switch (pData[i + prefixLen] & 0x1f)
		{
			case 5:
				hasIdr = 1;
				break;

			case 7:
				hasSps = 1;
				break;

			case 8:
				hasPps = 1;
				break;
		}
		i += prefixLen;
	}

	return hasSps && hasPps && hasIdr;
}

static GstFlowReturn CallBack_RxParseVideoNewSampleToH264AuFifo(GstAppSink *pSink, gpointer pUserData)
{
	AHEADFORM_VIDEO_CTRL *pstVideoCtrl = (AHEADFORM_VIDEO_CTRL*)pUserData;
	AHEADFORM_VIDEO_FRAME_INFO stInfo;
	GstSample *pSample = NULL;
	GstBuffer *pBuffer = NULL;
	GstCaps *pCaps = NULL;
	const GstStructure *pStructure = NULL;
	GstMapInfo stMap;
	int width, height, ret = RET_SUCCESS;

	if ( (pSample = gst_app_sink_pull_sample(pSink)) == NULL )
		return GST_FLOW_EOS;

	pBuffer = gst_sample_get_buffer(pSample);
	pCaps = gst_sample_get_caps(pSample);
	if (pBuffer == NULL || pCaps == NULL || !gst_buffer_map(pBuffer, &stMap, GST_MAP_READ))
	{
		printf("%s() cannot map the video sample.\r\n", __FUNCTION__);
		gst_sample_unref(pSample);
		__atomic_store_n(&pstVideoCtrl->lastError, ERRCODE_RX_FAILED, __ATOMIC_SEQ_CST);
		NotifyVideoReaders(pstVideoCtrl);
		return GST_FLOW_ERROR;
	}
	pStructure = gst_caps_get_structure(pCaps, 0);
	if (!gst_structure_get_int(pStructure, "width", &width) || !gst_structure_get_int(pStructure, "height", &height) || width <= 0 || height <= 0)
	{
		printf("%s() video caps have no valid width or height.\r\n", __FUNCTION__);
		ret = ERR_MEDIA_FORMAT_UNSUPPORTED;
	}
	else if (stMap.size == 0 || stMap.size > pstVideoCtrl->stCfg.videoFrameSizeMax || GST_BUFFER_FLAG_IS_SET(pBuffer, GST_BUFFER_FLAG_CORRUPTED) || GST_BUFFER_FLAG_IS_SET(pBuffer, GST_BUFFER_FLAG_GAP))
	{
		if (stMap.size > pstVideoCtrl->stCfg.videoFrameSizeMax)
			printf("%s() drop video AU: bytes = %zu, videoFrameSizeMax = %u.\r\n", __FUNCTION__, stMap.size, pstVideoCtrl->stCfg.videoFrameSizeMax);

		pstVideoCtrl->nextFrameSeq++;
		pstVideoCtrl->isDiscontinuous = 1;
	}
	else
	{
		memcpy(&stInfo, pstVideoCtrl->pElemBuf, sizeof(stInfo));
		if (stInfo.width != (unsigned int)width || stInfo.height != (unsigned int)height || GST_BUFFER_FLAG_IS_SET(pBuffer, GST_BUFFER_FLAG_DISCONT))
			pstVideoCtrl->isDiscontinuous = 1;

		stInfo.frameSeq = pstVideoCtrl->nextFrameSeq++;
		stInfo.pts_ns = GST_BUFFER_PTS_IS_VALID(pBuffer) ? GST_BUFFER_PTS(pBuffer) : ULLONG_MAX;
		stInfo.ReceiveTime_us = GetTimeUsCnt_UpToNow();
		stInfo.dataLen = (unsigned int)stMap.size;
		stInfo.width = (unsigned int)width;
		stInfo.height = (unsigned int)height;
		stInfo.isKeyFrame = IsRecoverableH264Frame(stMap.data, stMap.size);
		stInfo.isDiscontinuous = pstVideoCtrl->isDiscontinuous;
		memcpy(pstVideoCtrl->pElemBuf, &stInfo, sizeof(stInfo));
		memcpy(pstVideoCtrl->pElemBuf + sizeof(stInfo), stMap.data, stMap.size);
		if (SPMCRingFifo_Writer_PushElemAndGetAddr(&pstVideoCtrl->stFifo, pstVideoCtrl->pElemBuf) == NULL)
		{
			ret = ERRCODE_COMM_RING_OP_FAIL;
		}
		else
		{
			pstVideoCtrl->isDiscontinuous = 0;
			ret = NotifyVideoReaders(pstVideoCtrl);
		}
	}
	gst_buffer_unmap(pBuffer, &stMap);
	gst_sample_unref(pSample);
	if (ret != RET_SUCCESS)
	{
		__atomic_store_n(&pstVideoCtrl->lastError, ret, __ATOMIC_SEQ_CST);
		NotifyVideoReaders(pstVideoCtrl);
		return GST_FLOW_ERROR;
	}

	return GST_FLOW_OK;
}

int Video_Init(AHEADFORM_VIDEO_CTRL *pstVideoCtrl, const AHEADFORM_INIT_CONFIG *pstCfg)
{
	int ret;

	if (pstVideoCtrl == NULL)
		return ERR_ARGV_1;

	if (pstCfg == NULL)
		return ERR_ARGV_2;

	memset(pstVideoCtrl, 0, sizeof(*pstVideoCtrl));
	pstVideoCtrl->stCfg = pstCfg->stMedia;
	strcpy(pstVideoCtrl->strNetworkInterface, pstCfg->stDds.strNetworkInterface);
	__atomic_store_n(&pstVideoCtrl->lastError, ERRCODE_MEDIA_CHANNEL_NOT_READY, __ATOMIC_SEQ_CST);
	if ( (ret = pthread_mutex_init(&pstVideoCtrl->readerListMutex, NULL)) != RET_SUCCESS )
	{
		printf("%s() call pthread_mutex_init(video reader list) failed: %s\r\n", __FUNCTION__, strerror(ret));
		return ERR_MUTEX_INIT;
	}

	if (pstVideoCtrl->stCfg.isVideoCaptureOn)
		return Video_StartCapture(pstVideoCtrl);

	return RET_SUCCESS;
}

int Video_StartCapture(AHEADFORM_VIDEO_CTRL *pstVideoCtrl)
{
	GstElement *pSource = NULL, *pSink = NULL, *pJitter = NULL;
	GstBus *pBus = NULL;
	GstCaps *pCaps = NULL;
	GError *pError = NULL;
	GstAppSinkCallbacks stCallbacks = {0};
	unsigned int sizeOf1Elem;
	int cleanupRet, ret = RET_SUCCESS;

	if (pstVideoCtrl == NULL)
		return ERR_ARGV_1;

	if (pstVideoCtrl->pPipeline != NULL)
		return Video_GetCaptureError(pstVideoCtrl);

	if (pstVideoCtrl->stCfg.videoFrameSizeMax == 0 || pstVideoCtrl->stCfg.videoFrameSizeMax > UINT_MAX / SPMC_RING_FIFO_MIN_ELEM_CNT - sizeof(AHEADFORM_VIDEO_FRAME_INFO))
		return ERR_CONFIG_VALUE_INVALID;

	if (!gst_init_check(NULL, NULL, &pError))
	{
		printf("%s() call gst_init_check() failed: %s\r\n", __FUNCTION__, pError != NULL ? pError->message : "no GStreamer error detail");
		g_clear_error(&pError);
		return ERR_INIT_FAIL;
	}
	sizeOf1Elem = sizeof(AHEADFORM_VIDEO_FRAME_INFO) + pstVideoCtrl->stCfg.videoFrameSizeMax;
	if (pstVideoCtrl->pElemAryBuf == NULL)
	{
		if ( (pstVideoCtrl->pElemBuf = (unsigned char*)calloc(1, sizeOf1Elem)) == NULL )
			return ERR_MEMORY_ALLOC;

		if ( (pstVideoCtrl->pElemAryBuf = (unsigned char*)malloc((size_t)sizeOf1Elem * SPMC_RING_FIFO_MIN_ELEM_CNT)) == NULL )
		{
			free(pstVideoCtrl->pElemBuf);
			pstVideoCtrl->pElemBuf = NULL;
			return ERR_MEMORY_ALLOC;
		}

		if ( (ret = SPMCRingFifo_Init(&pstVideoCtrl->stFifo, pstVideoCtrl->pElemAryBuf, sizeOf1Elem, SPMC_RING_FIFO_MIN_ELEM_CNT)) != RET_SUCCESS )
		{
			free(pstVideoCtrl->pElemAryBuf);
			free(pstVideoCtrl->pElemBuf);
			pstVideoCtrl->pElemAryBuf = NULL;
			pstVideoCtrl->pElemBuf = NULL;
			return ret;
		}
	}
	__atomic_store_n(&pstVideoCtrl->lastError, RET_SUCCESS, __ATOMIC_SEQ_CST);
	pstVideoCtrl->isDiscontinuous = 1;
	pstVideoCtrl->pPipeline = gst_parse_launch("udpsrc name=source ! rtpjitterbuffer name=jitter drop-on-latency=true do-lost=true ! rtph264depay wait-for-keyframe=true ! h264parse config-interval=-1 "
		"! video/x-h264,stream-format=byte-stream,alignment=au ! appsink name=video sync=false max-buffers=1 drop=false wait-on-eos=false enable-last-sample=false", &pError);
	if (pstVideoCtrl->pPipeline == NULL || pError != NULL)
	{
		printf("%s() call gst_parse_launch(video capture) failed: %s\r\n", __FUNCTION__, pError != NULL ? pError->message : "no video pipeline");
		g_clear_error(&pError);
		ret = ERR_INIT_FAIL;
	}
	else
	{
		pSource = gst_bin_get_by_name(GST_BIN(pstVideoCtrl->pPipeline), "source");
		pSink = gst_bin_get_by_name(GST_BIN(pstVideoCtrl->pPipeline), "video");
		pJitter = gst_bin_get_by_name(GST_BIN(pstVideoCtrl->pPipeline), "jitter");
		pBus = gst_element_get_bus(pstVideoCtrl->pPipeline);
		if (pSource == NULL || pSink == NULL || pJitter == NULL || pBus == NULL)
		{
			printf("%s() cannot get video source, sink, jitter buffer or bus.\r\n", __FUNCTION__);
			ret = ERR_INIT_FAIL;
		}
		else
		{
			pCaps = gst_caps_new_simple("application/x-rtp", "media", G_TYPE_STRING, "video", "encoding-name", G_TYPE_STRING, "H264", "clock-rate", G_TYPE_INT, 90000, "payload", G_TYPE_INT, (int)pstVideoCtrl->stCfg.videoPayloadType, NULL);
			g_object_set(pSource, "address", pstVideoCtrl->stCfg.multicastAddress, "port", (int)pstVideoCtrl->stCfg.videoPort, "multicast-iface", pstVideoCtrl->strNetworkInterface, "auto-multicast", TRUE, "reuse", TRUE, NULL);
			g_object_set(pSource, "caps", pCaps, "mtu", pstVideoCtrl->stCfg.rtpMtu, NULL);
			g_object_set(pJitter, "latency", pstVideoCtrl->stCfg.jitterBufferLatencyMax_ms, NULL);
			gst_caps_unref(pCaps);
			stCallbacks.new_sample = CallBack_RxParseVideoNewSampleToH264AuFifo;
			gst_app_sink_set_callbacks(GST_APP_SINK(pSink), &stCallbacks, pstVideoCtrl, NULL);
			gst_bus_set_sync_handler(pBus, CallBack_OnVideoCaptureBusMessageReceived, pstVideoCtrl, NULL);
			if (gst_element_set_state(pstVideoCtrl->pPipeline, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE)
			{
				printf("%s() call gst_element_set_state(video, PLAYING) failed.\r\n", __FUNCTION__);
				ret = ERR_INIT_FAIL;
			}
		}
	}

	if (pSource != NULL)
		gst_object_unref(pSource);

	if (pSink != NULL)
		gst_object_unref(pSink);

	if (pJitter != NULL)
		gst_object_unref(pJitter);

	if (pBus != NULL)
		gst_object_unref(pBus);

	if (ret == RET_SUCCESS)
		ret = Video_GetCaptureError(pstVideoCtrl);

	if (ret != RET_SUCCESS)
	{
		__atomic_store_n(&pstVideoCtrl->lastError, ret, __ATOMIC_SEQ_CST);
		if ( (cleanupRet = Video_StopCapture(pstVideoCtrl)) != RET_SUCCESS )
			return cleanupRet;
	}

	return ret;
}

int Video_StopCapture(AHEADFORM_VIDEO_CTRL *pstVideoCtrl)
{
	GstBus *pBus = NULL;

	if (pstVideoCtrl == NULL)
		return ERR_ARGV_1;

	if (pstVideoCtrl->pPipeline == NULL)
		return RET_SUCCESS;

	if (gst_element_set_state(pstVideoCtrl->pPipeline, GST_STATE_NULL) == GST_STATE_CHANGE_FAILURE)
	{
		printf("%s() call gst_element_set_state(video, NULL) failed.\r\n", __FUNCTION__);
		__atomic_store_n(&pstVideoCtrl->lastError, ERRCODE_STATE_TRANSITION_INVALID, __ATOMIC_SEQ_CST);
		return ERR_STATE_TRANSITION_INVALID;
	}
	pBus = gst_element_get_bus(pstVideoCtrl->pPipeline);
	if (pBus != NULL)
	{
		gst_bus_set_sync_handler(pBus, NULL, NULL, NULL);
		gst_object_unref(pBus);
	}
	gst_object_unref(pstVideoCtrl->pPipeline);
	pstVideoCtrl->pPipeline = NULL;
	pstVideoCtrl->isDiscontinuous = 1;
	return RET_SUCCESS;
}

int Video_GetCaptureError(const AHEADFORM_VIDEO_CTRL *pstVideoCtrl)
{
	if (pstVideoCtrl == NULL)
		return ERR_ARGV_1;

	return __atomic_load_n(&pstVideoCtrl->lastError, __ATOMIC_SEQ_CST);
}

int Video_Deinit(AHEADFORM_VIDEO_CTRL *pstVideoCtrl)
{
	int ret;

	if (pstVideoCtrl == NULL)
		return ERR_ARGV_1;

	if (pstVideoCtrl->pstReaders != NULL)
		return ERRCODE_RESOURCE_BUSY;

	if ( (ret = Video_StopCapture(pstVideoCtrl)) != RET_SUCCESS )
		return ret;

	if ( (ret = pthread_mutex_destroy(&pstVideoCtrl->readerListMutex)) != RET_SUCCESS )
	{
		printf("%s() call pthread_mutex_destroy(video reader list) failed: %s\r\n", __FUNCTION__, strerror(ret));
		return ERR_MUTEX_OP_FAILED;
	}
	SPMCRingFifo_Deinit(&pstVideoCtrl->stFifo);
	free(pstVideoCtrl->pElemAryBuf);
	free(pstVideoCtrl->pElemBuf);
	memset(pstVideoCtrl, 0, sizeof(*pstVideoCtrl));
	return RET_SUCCESS;
}

AHEADFORM_VIDEO_READER* Video_RegisterReader(AHEADFORM_VIDEO_CTRL *pstVideoCtrl)
{
	AHEADFORM_VIDEO_READER *pstReader = NULL;
	int ret;

	if (pstVideoCtrl == NULL)
	{
		PRINT_FUNCTION_ERRINFO(ERR_ARGV_1, __FUNCTION__);
		return NULL;
	}

	if (pstVideoCtrl->pElemAryBuf == NULL || Video_GetCaptureError(pstVideoCtrl) != RET_SUCCESS)
	{
		PRINT_FUNCTION_ERRINFO(ERR_SDK_NOT_READY, __FUNCTION__);
		return NULL;
	}

	if ( (pstReader = (AHEADFORM_VIDEO_READER*)calloc(1, sizeof(*pstReader))) == NULL )
	{
		PRINT_FUNCTION_ERRINFO(ERR_MEMORY_ALLOC, __FUNCTION__);
		return NULL;
	}

	if ( (pstReader->pElemBuf = (unsigned char*)malloc(pstVideoCtrl->stFifo.sizeOf1Elem)) == NULL )
	{
		free(pstReader);
		PRINT_FUNCTION_ERRINFO(ERR_MEMORY_ALLOC, __FUNCTION__);
		return NULL;
	}

	if ( (pstReader->rxDataUpdateEventfd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC)) <= 0 )
	{
		printf("%s() call eventfd(video Reader) failed: %s\r\n", __FUNCTION__, strerror(errno));
		free(pstReader->pElemBuf);
		free(pstReader);
		PRINT_FUNCTION_ERRINFO(ERR_EVENTFD_CREATE_FAILED, __FUNCTION__);
		return NULL;
	}

	if ( (ret = pthread_mutex_lock(&pstVideoCtrl->readerListMutex)) != RET_SUCCESS )
	{
		printf("%s() call pthread_mutex_lock(video reader list) failed: %s\r\n", __FUNCTION__, strerror(ret));
		close(pstReader->rxDataUpdateEventfd);
		free(pstReader->pElemBuf);
		free(pstReader);
		PRINT_FUNCTION_ERRINFO(ERR_MUTEX_OP_FAILED, __FUNCTION__);
		return NULL;
	}
	ret = SPMCRingFifo_Reader_Init(&pstReader->stFifoReader, &pstVideoCtrl->stFifo);
	if (ret == RET_SUCCESS)
	{
		pstReader->isWaitingKeyFrame = 1;
		pstReader->pNext = pstVideoCtrl->pstReaders;
		pstVideoCtrl->pstReaders = pstReader;
	}
	pthread_mutex_unlock(&pstVideoCtrl->readerListMutex);
	if (ret != RET_SUCCESS)
	{
		close(pstReader->rxDataUpdateEventfd);
		free(pstReader->pElemBuf);
		free(pstReader);
		PRINT_FUNCTION_ERRINFO(ret, __FUNCTION__);
		return NULL;
	}

	return pstReader;
}

int Video_UnregisterReader(AHEADFORM_VIDEO_CTRL *pstVideoCtrl, AHEADFORM_VIDEO_READER *pstReader)
{
	AHEADFORM_VIDEO_READER **ppReader = NULL;
	int ret;

	if (pstVideoCtrl == NULL)
		return ERR_ARGV_1;

	if (pstReader == NULL)
		return ERR_ARGV_2;

	if ( (ret = pthread_mutex_lock(&pstVideoCtrl->readerListMutex)) != RET_SUCCESS )
	{
		printf("%s() call pthread_mutex_lock(video reader list) failed: %s\r\n", __FUNCTION__, strerror(ret));
		return ERR_MUTEX_OP_FAILED;
	}
	ppReader = &pstVideoCtrl->pstReaders;
	while (*ppReader != NULL && *ppReader != pstReader)
		ppReader = &(*ppReader)->pNext;

	if (*ppReader == NULL)
	{
		pthread_mutex_unlock(&pstVideoCtrl->readerListMutex);
		return ERR_SUBSCRIPTION_NOT_FOUND;
	}

	if (pstReader->rxDataUpdateEventfd > 0 && close(pstReader->rxDataUpdateEventfd) < 0)
	{
		printf("%s() call close(video fd = %d) failed: %s\r\n", __FUNCTION__, pstReader->rxDataUpdateEventfd, strerror(errno));
		pstReader->rxDataUpdateEventfd = 0;
		pthread_mutex_unlock(&pstVideoCtrl->readerListMutex);
		return ERR_EVENTFD_IO_FAILED;
	}
	*ppReader = pstReader->pNext;
	pthread_mutex_unlock(&pstVideoCtrl->readerListMutex);
	free(pstReader->pElemBuf);
	free(pstReader);
	return RET_SUCCESS;
}

int Video_Reader_PopFrame(AHEADFORM_VIDEO_READER *pstReader, unsigned char *pRetDataBuf, size_t dataBufSize, AHEADFORM_VIDEO_FRAME_INFO *pstRetInfo)
{
	AHEADFORM_VIDEO_FRAME_INFO stInfo;
	unsigned long long missedFrameCnt;
	unsigned int i, readableFrameCnt;
	int ret;

	if (pstReader == NULL)
		return ERR_ARGV_1;

	if (pRetDataBuf == NULL)
		return ERR_ARGV_2;

	if (pstRetInfo == NULL)
		return ERR_ARGV_4;

	if (dataBufSize < pstReader->stFifoReader.pstFifo->sizeOf1Elem - sizeof(stInfo))
		return ERR_OVER_BUF;

	readableFrameCnt = SPMCRingFifo_Reader_GetReadableElemCnt(&pstReader->stFifoReader);
	for (i = 0; i < readableFrameCnt; i++)
	{
		if ( (ret = SPMCRingFifo_Reader_PopElem(&pstReader->stFifoReader, pstReader->pElemBuf)) <= 0 )
			return ret;

		missedFrameCnt = SPMCRingFifo_Reader_GetMissedElemCnt(&pstReader->stFifoReader);
		if (missedFrameCnt != pstReader->lastMissedFrameCnt)
		{
			pstReader->lastMissedFrameCnt = missedFrameCnt;
			pstReader->isWaitingKeyFrame = 1;
		}
		memcpy(&stInfo, pstReader->pElemBuf, sizeof(stInfo));
		if (stInfo.isDiscontinuous)
			pstReader->isWaitingKeyFrame = 1;

		if (pstReader->isWaitingKeyFrame && !stInfo.isKeyFrame)
			continue;

		if (pstReader->isWaitingKeyFrame)
		{
			stInfo.isDiscontinuous = 1;
			pstReader->isWaitingKeyFrame = 0;
		}
		memcpy(pRetDataBuf, pstReader->pElemBuf + sizeof(stInfo), stInfo.dataLen);
		*pstRetInfo = stInfo;
		return 1;
	}

	return 0;
}

int Video_Reader_DropOldFrames(AHEADFORM_VIDEO_READER *pstReader, unsigned int retainedFreshFrameCnt)
{
	int ret;

	if (pstReader == NULL)
		return ERR_ARGV_1;

	if ( (ret = SPMCRingFifo_Reader_DropOldElems(&pstReader->stFifoReader, retainedFreshFrameCnt)) > 0 )
		pstReader->isWaitingKeyFrame = 1;

	return ret;
}

unsigned int Video_Reader_GetReadableFrameCnt(AHEADFORM_VIDEO_READER *pstReader)
{
	if (pstReader == NULL)
	{
		PRINT_FUNCTION_ERRINFO(ERR_ARGV_1, __FUNCTION__);
		return 0;
	}

	return SPMCRingFifo_Reader_GetReadableElemCnt(&pstReader->stFifoReader);
}

unsigned long long Video_Reader_GetMissedFrameCnt(AHEADFORM_VIDEO_READER *pstReader)
{
	if (pstReader == NULL)
	{
		PRINT_FUNCTION_ERRINFO(ERR_ARGV_1, __FUNCTION__);
		return 0;
	}

	return SPMCRingFifo_Reader_GetMissedElemCnt(&pstReader->stFifoReader);
}
