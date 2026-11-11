#include "Media.h"
#include "Time.h"

#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <gst/app/gstappsrc.h>
#include <ifaddrs.h>
#include <sys/eventfd.h>
#include <unistd.h>

typedef struct MEDIA_RECEIVE_PATH MEDIA_RECEIVE_PATH;

struct AHEADFORM_MEDIA_READER
{
	MEDIA_RECEIVE_PATH *pstPath;
	SPMC_RING_FIFO_READER stReader;
	int event_fd;
	unsigned long long missedBeforePop;
	unsigned long long callbackNextSeq;
	unsigned char isWaitingForIdr;
	unsigned char isCallbackWaitingForIdr;
	AHEADFORM_MEDIA_FRAME_CALLBACK ReaderCallback;
	void *pUserArg;
	struct AHEADFORM_MEDIA_READER *pNext;
};

struct MEDIA_RECEIVE_PATH
{
	ENUM_AHEADFORM_MEDIA_STREAM stream;
	GstElement *pPipeline;
	SPMC_RING_FIFO stFifo;
	AHEADFORM_MEDIA_FRAME *pstFrame;
	AHEADFORM_MEDIA_READER *pstReaders;
	pthread_mutex_t mutex;
	unsigned char isMutexInitialized;
	unsigned char isWaitingForIdr;
	unsigned char isDiscontinuous;
	unsigned long long nextSeq;
	unsigned long long ptsEpoch_us;
	int lastResult;
};

struct AHEADFORM_MEDIA_STREAMS
{
	MEDIA_RECEIVE_PATH szstPath[AHEADFORM_MEDIA_STREAM_CNT];
	GstElement *pSpeakerPipeline;
	GstElement *pAppSrc;
	pthread_mutex_t speakerMutex;
	unsigned char isSpeakerMutexInitialized;
	unsigned char isSpeakerEos;
	unsigned long long speakerSampleCnt;
	GstClockTime speakerPtsBase_ns;
	int speakerResult;
};

static void SignalFrameReaders(MEDIA_RECEIVE_PATH *pstPath)
{
	AHEADFORM_MEDIA_READER *pstReader = NULL;

	for (pstReader = pstPath->pstReaders; pstReader != NULL; pstReader = pstReader->pNext)
	{
		if (eventfd_write(pstReader->event_fd, 1) < 0 && errno != EAGAIN)
			printf("%s() call eventfd_write(fd = %d) failed: %s\r\n", __FUNCTION__, pstReader->event_fd, strerror(errno));
	}
}

static GstBusSyncReply ReceiveBusMessage(GstBus *pBus, GstMessage *pMessage, gpointer pUserArg)
{
	MEDIA_RECEIVE_PATH *pstPath = (MEDIA_RECEIVE_PATH*)pUserArg;
	GError *pError = NULL;
	gchar *strDebug = NULL;
	int ret;

	if (GST_MESSAGE_TYPE(pMessage) == GST_MESSAGE_ERROR)
	{
		gst_message_parse_error(pMessage, &pError, &strDebug);
		printf("%s() GStreamer stream = %u error: %s (%s)\r\n", __FUNCTION__, pstPath->stream, pError != NULL ? pError->message : "unknown", strDebug != NULL ? strDebug : "");
		g_clear_error(&pError);
		g_free(strDebug);
		__atomic_store_n(&pstPath->lastResult, ERRCODE_RX_FAILED, __ATOMIC_RELEASE);
		if ( (ret = pthread_mutex_lock(&pstPath->mutex)) != RET_SUCCESS )
		{
			printf("%s() call pthread_mutex_lock(stream = %u) failed: %s\r\n", __FUNCTION__, pstPath->stream, strerror(ret));
		}
		else
		{
			SignalFrameReaders(pstPath);
			pthread_mutex_unlock(&pstPath->mutex);
		}
	}
	//A sync handler returning DROP owns the message reference.
	gst_message_unref(pMessage);
	return GST_BUS_DROP;
}

static GstBusSyncReply SpeakerBusMessage(GstBus *pBus, GstMessage *pMessage, gpointer pUserArg)
{
	struct AHEADFORM_MEDIA_STREAMS *pstStreams = (struct AHEADFORM_MEDIA_STREAMS*)pUserArg;
	GError *pError = NULL;
	gchar *strDebug = NULL;

	if (GST_MESSAGE_TYPE(pMessage) == GST_MESSAGE_ERROR)
	{
		gst_message_parse_error(pMessage, &pError, &strDebug);
		printf("%s() GStreamer speaker error: %s (%s)\r\n", __FUNCTION__, pError != NULL ? pError->message : "unknown", strDebug != NULL ? strDebug : "");
		g_clear_error(&pError);
		g_free(strDebug);
		__atomic_store_n(&pstStreams->speakerResult, ERRCODE_RX_FAILED, __ATOMIC_RELEASE);
	}

	if (GST_MESSAGE_TYPE(pMessage) == GST_MESSAGE_ERROR || GST_MESSAGE_TYPE(pMessage) == GST_MESSAGE_EOS)
		return GST_BUS_PASS;

	gst_message_unref(pMessage);
	return GST_BUS_DROP;
}

static void InspectH264AccessUnit(AHEADFORM_MEDIA_FRAME *pstFrame)
{
	unsigned int i = 0, nalHeaderPos;
	unsigned char type, hasSps = 0, hasPps = 0;

	while (i + 3 < pstFrame->dataSize)
	{
		if (pstFrame->data[i] == 0 && pstFrame->data[i + 1] == 0 && pstFrame->data[i + 2] == 0 && pstFrame->data[i + 3] == 1)
		{
			nalHeaderPos = i + 4;
		}
		else if (pstFrame->data[i] == 0 && pstFrame->data[i + 1] == 0 && pstFrame->data[i + 2] == 1)
		{
			nalHeaderPos = i + 3;
		}
		else
		{
			i++;
			continue;
		}

		if (nalHeaderPos >= pstFrame->dataSize)
			break;

		type = pstFrame->data[nalHeaderPos] & 31;
		if (type == 5)
			pstFrame->isKeyframe = 1;
		else if (type == 7)
			hasSps = 1;
		else if (type == 8)
			hasPps = 1;

		i = nalHeaderPos + 1;
	}
	pstFrame->hasParameterSets = hasSps && hasPps;
}

static GstFlowReturn ReceiveFrame(GstAppSink *pSink, gpointer pUserArg)
{
	MEDIA_RECEIVE_PATH *pstPath = (MEDIA_RECEIVE_PATH*)pUserArg;
	AHEADFORM_MEDIA_READER *pstReader = NULL;
	AHEADFORM_MEDIA_FRAME *pstFrame = pstPath->pstFrame;
	const AHEADFORM_MEDIA_FRAME *pstStoredFrame = NULL;
	GstSample *pSample = NULL;
	GstBuffer *pBuffer = NULL;
	GstCaps *pCaps = NULL;
	const GstSegment *pSegment = NULL;
	GstStructure *pStructure = NULL;
	GstMapInfo stMap;
	GstClockTime pts;
	int ret, width = 0, height = 0;
	unsigned char isReaderDiscontinuous;

	if ( (pSample = gst_app_sink_pull_sample(pSink)) == NULL )
		return GST_FLOW_EOS;

	pBuffer = gst_sample_get_buffer(pSample);
	if (pBuffer == NULL || !gst_buffer_map(pBuffer, &stMap, GST_MAP_READ))
	{
		gst_sample_unref(pSample);
		return GST_FLOW_ERROR;
	}

	if ( (ret = pthread_mutex_lock(&pstPath->mutex)) != RET_SUCCESS )
	{
		printf("%s() call pthread_mutex_lock(stream = %u) failed: %s\r\n", __FUNCTION__, pstPath->stream, strerror(ret));
		gst_buffer_unmap(pBuffer, &stMap);
		gst_sample_unref(pSample);
		return GST_FLOW_ERROR;
	}
	memset(pstFrame, 0, sizeof(*pstFrame));
	pstFrame->sequence = pstPath->nextSeq++;
	pstFrame->ReceiveTime_us = GetTimeUsCnt_UpToNow();
	pstFrame->ptsEpoch_us = pstPath->ptsEpoch_us;
	if (stMap.size > pstPath->stFifo.sizeOf1Elem - sizeof(*pstFrame) || (pstPath->stream == AHEADFORM_MEDIA_STREAM_MIC && stMap.size % 2 != 0))
	{
		__atomic_store_n(&pstPath->lastResult, ERRCODE_OVER_BUF, __ATOMIC_RELEASE);
		pstPath->isWaitingForIdr = 1;
		pstPath->isDiscontinuous = 1;
		SignalFrameReaders(pstPath);
		pthread_mutex_unlock(&pstPath->mutex);
		gst_buffer_unmap(pBuffer, &stMap);
		gst_sample_unref(pSample);
		return GST_FLOW_OK;
	}
	pstFrame->dataSize = (unsigned int)stMap.size;
	memcpy(pstFrame->data, stMap.data, stMap.size);
	pSegment = gst_sample_get_segment(pSample);
	if (GST_BUFFER_PTS_IS_VALID(pBuffer) && pSegment != NULL && pSegment->format == GST_FORMAT_TIME)
	{
		pts = gst_segment_to_running_time(pSegment, GST_FORMAT_TIME, GST_BUFFER_PTS(pBuffer));
		if (GST_CLOCK_TIME_IS_VALID(pts))
		{
			pstFrame->isPtsValid = 1;
			pstFrame->pts_ns = pts;
		}
	}
	pstFrame->isDiscontinuous = pstPath->isDiscontinuous || GST_BUFFER_FLAG_IS_SET(pBuffer, GST_BUFFER_FLAG_DISCONT) || GST_BUFFER_FLAG_IS_SET(pBuffer, GST_BUFFER_FLAG_CORRUPTED);
	if (pstPath->stream == AHEADFORM_MEDIA_STREAM_VIDEO)
	{
		pstFrame->format = AHEADFORM_MEDIA_FORMAT_H264_ANNEX_B;
		pCaps = gst_sample_get_caps(pSample);
		if (pCaps != NULL && gst_caps_get_size(pCaps) != 0)
		{
			pStructure = gst_caps_get_structure(pCaps, 0);
			gst_structure_get_int(pStructure, "width", &width);
			gst_structure_get_int(pStructure, "height", &height);
		}
		pstFrame->width = width > 0 ? width : 0;
		pstFrame->height = height > 0 ? height : 0;
		InspectH264AccessUnit(pstFrame);
		if (pstFrame->isDiscontinuous)
			pstPath->isWaitingForIdr = 1;
	}
	else
	{
		pstFrame->format = AHEADFORM_MEDIA_FORMAT_PCM_S16LE;
		pstFrame->sampleRate = AHEADFORM_PCM_SAMPLE_RATE;
		pstFrame->channelCnt = AHEADFORM_PCM_CHANNEL_CNT;
		pstFrame->sampleCnt = pstFrame->dataSize / 2;
	}

	if (GST_BUFFER_FLAG_IS_SET(pBuffer, GST_BUFFER_FLAG_CORRUPTED) || (pstPath->stream == AHEADFORM_MEDIA_STREAM_VIDEO && pstPath->isWaitingForIdr && (!pstFrame->isKeyframe || !pstFrame->hasParameterSets)))
	{
		pstPath->isDiscontinuous = 1;
		__atomic_store_n(&pstPath->lastResult, ERRCODE_NO_FRESH_FEEDBACK, __ATOMIC_RELEASE);
		SignalFrameReaders(pstPath);
	}
	else
	{
		pstPath->isWaitingForIdr = 0;
		pstPath->isDiscontinuous = 0;
		pstStoredFrame = (const AHEADFORM_MEDIA_FRAME*)SPMCRingFifo_Writer_PushElemAndGetAddr(&pstPath->stFifo, pstFrame);
		__atomic_store_n(&pstPath->lastResult, pstStoredFrame != NULL ? RET_SUCCESS : ERRCODE_COMM_RING_OP_FAIL, __ATOMIC_RELEASE);
		SignalFrameReaders(pstPath);
		for (pstReader = pstPath->pstReaders; pstStoredFrame != NULL && pstReader != NULL; pstReader = pstReader->pNext)
		{
			if (pstReader->ReaderCallback == NULL)
				continue;

			isReaderDiscontinuous = pstStoredFrame->isDiscontinuous || pstReader->callbackNextSeq != pstStoredFrame->sequence;
			pstReader->callbackNextSeq = pstStoredFrame->sequence + 1;
			if (isReaderDiscontinuous)
				pstReader->isCallbackWaitingForIdr = 1;

			if (pstPath->stream == AHEADFORM_MEDIA_STREAM_VIDEO && pstReader->isCallbackWaitingForIdr && (!pstStoredFrame->isKeyframe || !pstStoredFrame->hasParameterSets))
				continue;

			isReaderDiscontinuous |= pstReader->isCallbackWaitingForIdr;
			pstReader->isCallbackWaitingForIdr = 0;
			pstReader->ReaderCallback(pstStoredFrame, isReaderDiscontinuous, pstReader->pUserArg);
		}
	}
	pthread_mutex_unlock(&pstPath->mutex);
	gst_buffer_unmap(pBuffer, &stMap);
	gst_sample_unref(pSample);
	return GST_FLOW_OK;
}

int Media_InitStreams(AHEADFORM_MEDIA_CTRL *pstMediaCtrl)
{
	struct AHEADFORM_MEDIA_STREAMS *pstStreams = NULL;
	MEDIA_RECEIVE_PATH *pstPath = NULL;
	unsigned int i, elemSize, dataSize;
	GError *pError = NULL;
	int ret;

	if (pstMediaCtrl == NULL || pstMediaCtrl->pstStreams != NULL)
		return ERR_ARGV_1;

	if (!gst_init_check(NULL, NULL, &pError))
	{
		printf("%s() call gst_init_check() failed: %s\r\n", __FUNCTION__, pError->message);
		g_error_free(pError);
		return ERR_INIT_FAIL;
	}

	if (AHEADFORM_VIDEO_FRAME_SIZE_MAX == 0 || AHEADFORM_VIDEO_FRAME_SIZE_MAX > UINT_MAX - sizeof(AHEADFORM_MEDIA_FRAME))
		return ERR_OVER_BUF;

	if ( (pstStreams = calloc(1, sizeof(*pstStreams))) == NULL )
		return ERR_MEMORY_ALLOC;

	pstMediaCtrl->pstStreams = pstStreams;
	if ( (ret = pthread_mutex_init(&pstStreams->speakerMutex, NULL)) != RET_SUCCESS )
	{
		printf("%s() call pthread_mutex_init(speaker) failed: %s\r\n", __FUNCTION__, strerror(ret));
		return ERR_MUTEX_INIT;
	}
	pstStreams->isSpeakerMutexInitialized = 1;
	for (i = 0; i < AHEADFORM_MEDIA_STREAM_CNT; i++)
	{
		pstPath = &pstStreams->szstPath[i];
		pstPath->stream = (ENUM_AHEADFORM_MEDIA_STREAM)i;
		pstPath->lastResult = ERRCODE_NO_FRESH_FEEDBACK;
		dataSize = i == AHEADFORM_MEDIA_STREAM_VIDEO ? AHEADFORM_VIDEO_FRAME_SIZE_MAX : AHEADFORM_MIC_FRAME_SIZE_MAX;
		elemSize = sizeof(AHEADFORM_MEDIA_FRAME) + dataSize;
		if ((size_t)elemSize > SIZE_MAX / AHEADFORM_MEDIA_FIFO_ELEM_CNT)
			return ERR_OVER_BUF;

		pstPath->stFifo.pElemAryBuf = calloc(AHEADFORM_MEDIA_FIFO_ELEM_CNT, elemSize);
		pstPath->pstFrame = calloc(1, elemSize);
		if (pstPath->stFifo.pElemAryBuf == NULL || pstPath->pstFrame == NULL)
			return ERR_MEMORY_ALLOC;

		if ( (ret = SPMCRingFifo_Init(&pstPath->stFifo, pstPath->stFifo.pElemAryBuf, elemSize, AHEADFORM_MEDIA_FIFO_ELEM_CNT)) != RET_SUCCESS )
			return ret;

		if ( (ret = pthread_mutex_init(&pstPath->mutex, NULL)) != RET_SUCCESS )
		{
			printf("%s() call pthread_mutex_init(stream = %u) failed: %s\r\n", __FUNCTION__, i, strerror(ret));
			return ERR_MUTEX_INIT;
		}
		pstPath->isMutexInitialized = 1;
	}

	return RET_SUCCESS;
}

static int StartReceive(AHEADFORM_MEDIA_CTRL *pstMediaCtrl, ENUM_AHEADFORM_MEDIA_STREAM stream)
{
	MEDIA_RECEIVE_PATH *pstPath = NULL;
	GstElement *pSource = NULL, *pSink = NULL;
	GstBus *pBus = NULL;
	GError *pError = NULL;
	gchar *strPipeline = NULL;
	GstAppSinkCallbacks stCallbacks = {0};
	GstStateChangeReturn stateRet;
	int cleanupRet;

	if (pstMediaCtrl == NULL || pstMediaCtrl->pstStreams == NULL)
		return ERR_ARGV_1;

	pstPath = &pstMediaCtrl->pstStreams->szstPath[stream];
	if (pstPath->pPipeline != NULL)
		return ERR_RESOURCE_BUSY;

	if (stream == AHEADFORM_MEDIA_STREAM_VIDEO)
	{
		strPipeline = g_strdup_printf("udpsrc name=source caps=\"application/x-rtp,media=video,encoding-name=H264,clock-rate=90000,payload=%u\" ! "
			"rtpjitterbuffer latency=%u do-lost=true ! rtph264depay wait-for-keyframe=true ! h264parse config-interval=-1 ! "
			"video/x-h264,stream-format=byte-stream,alignment=au ! appsink name=sink sync=false max-buffers=1 drop=false", pstMediaCtrl->stCfg.videoPayloadType, pstMediaCtrl->stCfg.jitterLatency_ms);
	}
	else
	{
		strPipeline = g_strdup_printf("udpsrc name=source caps=\"application/x-rtp,media=audio,encoding-name=L16,clock-rate=16000,channels=1,payload=%u\" ! "
			"rtpjitterbuffer latency=%u do-lost=true ! rtpL16depay ! audioconvert ! audio/x-raw,format=S16LE,rate=16000,channels=1,layout=interleaved ! "
			"appsink name=sink sync=false max-buffers=1 drop=false", pstMediaCtrl->stCfg.micPayloadType, pstMediaCtrl->stCfg.jitterLatency_ms);
	}
	if (strPipeline == NULL)
		return ERR_MEMORY_ALLOC;

	pstPath->pPipeline = gst_parse_launch(strPipeline, &pError);
	g_free(strPipeline);
	if (pError != NULL || pstPath->pPipeline == NULL)
	{
		printf("%s() call gst_parse_launch(stream = %u) failed: %s\r\n", __FUNCTION__, stream, pError != NULL ? pError->message : "no pipeline");
		g_clear_error(&pError);
		if (pstPath->pPipeline != NULL)
			gst_object_unref(pstPath->pPipeline);

		pstPath->pPipeline = NULL;
		return ERR_INIT_FAIL;
	}
	pSource = gst_bin_get_by_name(GST_BIN(pstPath->pPipeline), "source");
	pSink = gst_bin_get_by_name(GST_BIN(pstPath->pPipeline), "sink");
	if (pSource == NULL || pSink == NULL)
	{
		printf("%s() GStreamer stream = %u pipeline has no source or sink element.\r\n", __FUNCTION__, stream);
		if (pSource != NULL)
			gst_object_unref(pSource);

		if (pSink != NULL)
			gst_object_unref(pSink);

		gst_object_unref(pstPath->pPipeline);
		pstPath->pPipeline = NULL;
		return ERR_INIT_FAIL;
	}
	g_object_set(pSource, "address", pstMediaCtrl->stCfg.strMulticastAddress, "port", stream == AHEADFORM_MEDIA_STREAM_VIDEO ? pstMediaCtrl->stCfg.videoPort : pstMediaCtrl->stCfg.micPort,
		"auto-multicast", TRUE, "multicast-iface", pstMediaCtrl->pstTopics->stDdsCtrl.strNetworkInterface, NULL);
	stCallbacks.new_sample = ReceiveFrame;
	gst_app_sink_set_callbacks(GST_APP_SINK(pSink), &stCallbacks, pstPath, NULL);
	gst_object_unref(pSink);
	gst_object_unref(pSource);
	pBus = gst_element_get_bus(pstPath->pPipeline);
	gst_bus_set_sync_handler(pBus, ReceiveBusMessage, pstPath, NULL);
	gst_object_unref(pBus);
	pstPath->ptsEpoch_us = GetTimeUsCnt_UpToNow();
	pstPath->isWaitingForIdr = 1;
	pstPath->isDiscontinuous = 1;
	__atomic_store_n(&pstPath->lastResult, ERRCODE_NO_FRESH_FEEDBACK, __ATOMIC_RELEASE);
	stateRet = gst_element_set_state(pstPath->pPipeline, GST_STATE_PLAYING);
	if (stateRet == GST_STATE_CHANGE_FAILURE)
	{
		printf("%s() call gst_element_set_state(stream = %u, PLAYING) failed.\r\n", __FUNCTION__, stream);
		cleanupRet = stream == AHEADFORM_MEDIA_STREAM_VIDEO ? Media_StopVideoReceive(pstMediaCtrl) : Media_StopMicReceive(pstMediaCtrl);
		if (cleanupRet != RET_SUCCESS)
			return cleanupRet;

		return ERR_INIT_FAIL;
	}

	return RET_SUCCESS;
}

static int StopReceive(AHEADFORM_MEDIA_CTRL *pstMediaCtrl, ENUM_AHEADFORM_MEDIA_STREAM stream)
{
	MEDIA_RECEIVE_PATH *pstPath = NULL;
	GstStateChangeReturn stateRet;

	if (pstMediaCtrl == NULL)
		return ERR_ARGV_1;

	if (pstMediaCtrl->pstStreams == NULL)
		return RET_SUCCESS;

	pstPath = &pstMediaCtrl->pstStreams->szstPath[stream];
	if (pstPath->pPipeline == NULL)
		return RET_SUCCESS;

	stateRet = gst_element_set_state(pstPath->pPipeline, GST_STATE_NULL);
	if (stateRet == GST_STATE_CHANGE_FAILURE)
	{
		printf("%s() call gst_element_set_state(stream = %u, NULL) failed.\r\n", __FUNCTION__, stream);
		return ERR_SHUT_DOWN;
	}

	stateRet = gst_element_get_state(pstPath->pPipeline, NULL, NULL, 5 * GST_SECOND);
	if (stateRet == GST_STATE_CHANGE_FAILURE || stateRet == GST_STATE_CHANGE_ASYNC)
	{
		printf("%s() call gst_element_get_state(stream = %u, NULL) failed or timed out: stateRet=%d\r\n", __FUNCTION__, stream, stateRet);
		return ERR_SHUT_DOWN;
	}

	gst_object_unref(pstPath->pPipeline);
	pstPath->pPipeline = NULL;
	return RET_SUCCESS;
}

int Media_StartVideoReceive(AHEADFORM_MEDIA_CTRL *pstMediaCtrl)
{
	return StartReceive(pstMediaCtrl, AHEADFORM_MEDIA_STREAM_VIDEO);
}

int Media_StopVideoReceive(AHEADFORM_MEDIA_CTRL *pstMediaCtrl)
{
	return StopReceive(pstMediaCtrl, AHEADFORM_MEDIA_STREAM_VIDEO);
}

int Media_StartMicReceive(AHEADFORM_MEDIA_CTRL *pstMediaCtrl)
{
	return StartReceive(pstMediaCtrl, AHEADFORM_MEDIA_STREAM_MIC);
}

int Media_StopMicReceive(AHEADFORM_MEDIA_CTRL *pstMediaCtrl)
{
	return StopReceive(pstMediaCtrl, AHEADFORM_MEDIA_STREAM_MIC);
}

int Media_StartSpeakerSend(AHEADFORM_MEDIA_CTRL *pstMediaCtrl)
{
	struct AHEADFORM_MEDIA_STREAMS *pstStreams = NULL;
	struct ifaddrs *pstAddresses = NULL, *pstAddress = NULL;
	GstElement *pSink = NULL;
	GstBus *pBus = NULL;
	GError *pError = NULL;
	gchar *strPipeline = NULL;
	char strLocalAddress[INET_ADDRSTRLEN] = {0};
	GstStateChangeReturn stateRet;
	int cleanupRet;

	if (pstMediaCtrl == NULL || pstMediaCtrl->pstStreams == NULL)
		return ERR_ARGV_1;

	pstStreams = pstMediaCtrl->pstStreams;
	if (pstStreams->pSpeakerPipeline != NULL)
		return ERR_RESOURCE_BUSY;

	if (getifaddrs(&pstAddresses) < 0)
	{
		printf("%s() call getifaddrs(interface = %s) failed: %s\r\n", __FUNCTION__, pstMediaCtrl->pstTopics->stDdsCtrl.strNetworkInterface, strerror(errno));
		return ERR_NETWORK_INTERFACE_INVALID;
	}

	for (pstAddress = pstAddresses; pstAddress != NULL; pstAddress = pstAddress->ifa_next)
	{
		if (pstAddress->ifa_addr != NULL && pstAddress->ifa_addr->sa_family == AF_INET && strcmp(pstAddress->ifa_name, pstMediaCtrl->pstTopics->stDdsCtrl.strNetworkInterface) == 0)
		{
			if (inet_ntop(AF_INET, &((struct sockaddr_in*)pstAddress->ifa_addr)->sin_addr, strLocalAddress, sizeof(strLocalAddress)) == NULL)
				printf("%s() call inet_ntop(interface = %s) failed: %s\r\n", __FUNCTION__, pstMediaCtrl->pstTopics->stDdsCtrl.strNetworkInterface, strerror(errno));

			break;
		}
	}
	freeifaddrs(pstAddresses);
	if (strLocalAddress[0] == 0)
		return ERR_NETWORK_INTERFACE_INVALID;

	strPipeline = g_strdup_printf("appsrc name=source is-live=true format=time block=false max-buffers=10 max-bytes=6400 "
		"caps=\"audio/x-raw,format=S16LE,rate=16000,channels=1,layout=interleaved\" ! audioconvert ! audio/x-raw,format=S16BE ! "
		"rtpL16pay pt=%u mtu=%u ! udpsink name=sink sync=true async=false", pstMediaCtrl->stCfg.spkPayloadType, pstMediaCtrl->stCfg.rtpMtu);
	if (strPipeline == NULL)
		return ERR_MEMORY_ALLOC;

	pstStreams->pSpeakerPipeline = gst_parse_launch(strPipeline, &pError);
	g_free(strPipeline);
	if (pError != NULL || pstStreams->pSpeakerPipeline == NULL)
	{
		printf("%s() call gst_parse_launch(speaker) failed: %s\r\n", __FUNCTION__, pError != NULL ? pError->message : "no pipeline");
		g_clear_error(&pError);
		if (pstStreams->pSpeakerPipeline != NULL)
			gst_object_unref(pstStreams->pSpeakerPipeline);

		pstStreams->pSpeakerPipeline = NULL;
		return ERR_INIT_FAIL;
	}
	pstStreams->pAppSrc = gst_bin_get_by_name(GST_BIN(pstStreams->pSpeakerPipeline), "source");
	pSink = gst_bin_get_by_name(GST_BIN(pstStreams->pSpeakerPipeline), "sink");
	if (pstStreams->pAppSrc == NULL || pSink == NULL)
	{
		printf("%s() GStreamer speaker pipeline has no source or sink element.\r\n", __FUNCTION__);
		if (pstStreams->pAppSrc != NULL)
			gst_object_unref(pstStreams->pAppSrc);

		if (pSink != NULL)
			gst_object_unref(pSink);

		gst_object_unref(pstStreams->pSpeakerPipeline);
		pstStreams->pAppSrc = NULL;
		pstStreams->pSpeakerPipeline = NULL;
		return ERR_INIT_FAIL;
	}
	g_object_set(pSink, "host", pstMediaCtrl->stCfg.strRobotIPv4, "port", pstMediaCtrl->stCfg.spkPort, "bind-address", strLocalAddress, NULL);
	gst_object_unref(pSink);
	pBus = gst_element_get_bus(pstStreams->pSpeakerPipeline);
	gst_bus_set_sync_handler(pBus, SpeakerBusMessage, pstStreams, NULL);
	gst_object_unref(pBus);
	pstStreams->speakerSampleCnt = 0;
	pstStreams->speakerPtsBase_ns = 0;
	pstStreams->isSpeakerEos = 0;
	__atomic_store_n(&pstStreams->speakerResult, RET_SUCCESS, __ATOMIC_RELEASE);
	stateRet = gst_element_set_state(pstStreams->pSpeakerPipeline, GST_STATE_PLAYING);
	if (stateRet == GST_STATE_CHANGE_FAILURE)
	{
		printf("%s() call gst_element_set_state(speaker, PLAYING) failed.\r\n", __FUNCTION__);
		if ( (cleanupRet = Media_StopSpeakerSend(pstMediaCtrl)) != RET_SUCCESS )
			return cleanupRet;

		return ERR_INIT_FAIL;
	}

	return RET_SUCCESS;
}

int Media_WriteSpeakerPcmFrame(AHEADFORM_MEDIA_CTRL *pstMediaCtrl, const void *pPcm, unsigned int pcmSize)
{
	struct AHEADFORM_MEDIA_STREAMS *pstStreams = NULL;
	GstBuffer *pBuffer = NULL;
	GstClock *pClock = NULL;
	GstFlowReturn flowRet;
	guint64 queuedFrameCnt;
	int ret;

	if (pstMediaCtrl == NULL || pstMediaCtrl->pstStreams == NULL)
		return ERR_ARGV_1;

	if (pPcm == NULL)
		return ERR_ARGV_2;

	if (pcmSize != AHEADFORM_PCM_FRAME_SIZE)
		return ERR_ARGV_3;

	pstStreams = pstMediaCtrl->pstStreams;
	if ( (ret = pthread_mutex_lock(&pstStreams->speakerMutex)) != RET_SUCCESS )
	{
		printf("%s() call pthread_mutex_lock(speaker) failed: %s\r\n", __FUNCTION__, strerror(ret));
		return ERR_MUTEX_OP_FAILED;
	}
	ret = __atomic_load_n(&pstStreams->speakerResult, __ATOMIC_ACQUIRE);
	if (pstStreams->pAppSrc == NULL || pstStreams->isSpeakerEos)
		ret = ERR_SDK_NOT_READY;

	if (ret == RET_SUCCESS)
	{
		g_object_get(pstStreams->pAppSrc, "current-level-buffers", &queuedFrameCnt, NULL);
		if (queuedFrameCnt >= AHEADFORM_SPEAKER_QUEUED_FRAME_CNT_MAX)
		{
			ret = ERR_RESOURCE_BUSY;
		}
		else if ( (pBuffer = gst_buffer_new_allocate(NULL, pcmSize, NULL)) == NULL )
		{
			ret = ERR_MEMORY_ALLOC;
		}
		else
		{
			gst_buffer_fill(pBuffer, 0, pPcm, pcmSize);
			if (pstStreams->speakerSampleCnt == 0 && (pClock = gst_element_get_clock(pstStreams->pSpeakerPipeline)) != NULL)
			{
				pstStreams->speakerPtsBase_ns = gst_clock_get_time(pClock) - gst_element_get_base_time(pstStreams->pSpeakerPipeline);
				gst_object_unref(pClock);
			}
			GST_BUFFER_PTS(pBuffer) = pstStreams->speakerPtsBase_ns + gst_util_uint64_scale(pstStreams->speakerSampleCnt, GST_SECOND, AHEADFORM_PCM_SAMPLE_RATE);
			GST_BUFFER_DURATION(pBuffer) = 20 * GST_MSECOND;
			flowRet = gst_app_src_push_buffer(GST_APP_SRC(pstStreams->pAppSrc), pBuffer);
			if (flowRet == GST_FLOW_OK)
				pstStreams->speakerSampleCnt += AHEADFORM_PCM_FRAME_SAMPLE_CNT;
			else
				ret = ERR_RX_FAILED;
		}
	}
	pthread_mutex_unlock(&pstStreams->speakerMutex);
	return ret;
}

int Media_GetSpeakerSendResult(AHEADFORM_MEDIA_CTRL *pstMediaCtrl)
{
	if (pstMediaCtrl == NULL || pstMediaCtrl->pstStreams == NULL)
		return ERR_ARGV_1;

	return __atomic_load_n(&pstMediaCtrl->pstStreams->speakerResult, __ATOMIC_ACQUIRE);
}

int Media_FinishSpeakerSend(AHEADFORM_MEDIA_CTRL *pstMediaCtrl, unsigned int timeout_ms)
{
	struct AHEADFORM_MEDIA_STREAMS *pstStreams = NULL;
	GstBus *pBus = NULL;
	GstMessage *pMessage = NULL;
	int ret;

	if (pstMediaCtrl == NULL || pstMediaCtrl->pstStreams == NULL)
		return ERR_ARGV_1;

	pstStreams = pstMediaCtrl->pstStreams;
	if ( (ret = pthread_mutex_lock(&pstStreams->speakerMutex)) != RET_SUCCESS )
	{
		printf("%s() call pthread_mutex_lock(speaker) failed: %s\r\n", __FUNCTION__, strerror(ret));
		return ERR_MUTEX_OP_FAILED;
	}

	if (pstStreams->pAppSrc == NULL)
	{
		pthread_mutex_unlock(&pstStreams->speakerMutex);
		return ERR_SDK_NOT_READY;
	}
	pstStreams->isSpeakerEos = 1;
	ret = gst_app_src_end_of_stream(GST_APP_SRC(pstStreams->pAppSrc));
	pBus = gst_element_get_bus(pstStreams->pSpeakerPipeline);
	pthread_mutex_unlock(&pstStreams->speakerMutex);
	if (ret != GST_FLOW_OK)
	{
		gst_object_unref(pBus);
		return ERR_RX_FAILED;
	}
	pMessage = gst_bus_timed_pop_filtered(pBus, (GstClockTime)timeout_ms * GST_MSECOND, GST_MESSAGE_EOS | GST_MESSAGE_ERROR);
	ret = pMessage == NULL ? ERRCODE_REQUEST_TIMEOUT : (GST_MESSAGE_TYPE(pMessage) == GST_MESSAGE_EOS ? RET_SUCCESS : ERRCODE_RX_FAILED);
	if (pMessage != NULL)
		gst_message_unref(pMessage);

	gst_object_unref(pBus);
	return ret;
}

int Media_StopSpeakerSend(AHEADFORM_MEDIA_CTRL *pstMediaCtrl)
{
	struct AHEADFORM_MEDIA_STREAMS *pstStreams = NULL;
	GstStateChangeReturn stateRet;
	int ret;

	if (pstMediaCtrl == NULL)
		return ERR_ARGV_1;

	if (pstMediaCtrl->pstStreams == NULL)
		return RET_SUCCESS;

	pstStreams = pstMediaCtrl->pstStreams;
	if (pstStreams->pSpeakerPipeline == NULL)
		return RET_SUCCESS;

	if ( (ret = pthread_mutex_lock(&pstStreams->speakerMutex)) != RET_SUCCESS )
	{
		printf("%s() call pthread_mutex_lock(speaker) failed: %s\r\n", __FUNCTION__, strerror(ret));
		return ERR_MUTEX_OP_FAILED;
	}

	stateRet = gst_element_set_state(pstStreams->pSpeakerPipeline, GST_STATE_NULL);
	if (stateRet != GST_STATE_CHANGE_FAILURE)
		stateRet = gst_element_get_state(pstStreams->pSpeakerPipeline, NULL, NULL, 5 * GST_SECOND);

	if (stateRet == GST_STATE_CHANGE_FAILURE || stateRet == GST_STATE_CHANGE_ASYNC)
	{
		printf("%s() stop GStreamer speaker failed or timed out: stateRet=%d\r\n", __FUNCTION__, stateRet);
		pthread_mutex_unlock(&pstStreams->speakerMutex);
		return ERR_SHUT_DOWN;
	}
	gst_object_unref(pstStreams->pAppSrc);
	gst_object_unref(pstStreams->pSpeakerPipeline);
	pstStreams->pAppSrc = NULL;
	pstStreams->pSpeakerPipeline = NULL;
	pthread_mutex_unlock(&pstStreams->speakerMutex);
	return RET_SUCCESS;
}

AHEADFORM_MEDIA_READER* Media_RegisterFrameReader(AHEADFORM_MEDIA_CTRL *pstMediaCtrl, ENUM_AHEADFORM_MEDIA_STREAM stream, const AHEADFORM_MEDIA_READER_PARA *pstReaderPara)
{
	AHEADFORM_MEDIA_READER *pstReader = NULL, *pstTail = NULL;
	MEDIA_RECEIVE_PATH *pstPath = NULL;
	int ret;

	if (pstMediaCtrl == NULL || pstMediaCtrl->pstStreams == NULL || stream < 0 || stream >= AHEADFORM_MEDIA_STREAM_CNT || pstReaderPara == NULL)
	{
		PRINT_FUNCTION_ERRINFO(ERR_ARGV, __FUNCTION__);
		return NULL;
	}
	pstPath = &pstMediaCtrl->pstStreams->szstPath[stream];
	if (!pstPath->isMutexInitialized || pstPath->stFifo.pElemAryBuf == NULL)
	{
		PRINT_FUNCTION_ERRINFO(ERR_SDK_NOT_READY, __FUNCTION__);
		return NULL;
	}
	if ( (pstReader = calloc(1, sizeof(*pstReader))) == NULL )
	{
		PRINT_FUNCTION_ERRINFO(ERR_MEMORY_ALLOC, __FUNCTION__);
		return NULL;
	}

	if ( (pstReader->event_fd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC)) <= 0 )
	{
		printf("%s() call eventfd(stream = %u) failed: %s\r\n", __FUNCTION__, stream, strerror(errno));
		free(pstReader);
		return NULL;
	}
	pstReader->pstPath = pstPath;
	pstReader->ReaderCallback = pstReaderPara->ReaderCallback;
	pstReader->pUserArg = pstReaderPara->pUserArg;
	pstReader->isWaitingForIdr = 1;
	pstReader->isCallbackWaitingForIdr = 1;
	if ( (ret = pthread_mutex_lock(&pstPath->mutex)) != RET_SUCCESS )
	{
		printf("%s() call pthread_mutex_lock(stream = %u) failed: %s\r\n", __FUNCTION__, stream, strerror(ret));
		if (close(pstReader->event_fd) < 0)
			printf("%s() call close(eventfd = %d) failed: %s\r\n", __FUNCTION__, pstReader->event_fd, strerror(errno));

		free(pstReader);
		return NULL;
	}
	SPMCRingFifo_Reader_Init(&pstReader->stReader, &pstPath->stFifo);
	pstReader->callbackNextSeq = pstPath->nextSeq;
	if (pstPath->pstReaders == NULL)
	{
		pstPath->pstReaders = pstReader;
	}
	else
	{
		pstTail = pstPath->pstReaders;
		while (pstTail->pNext != NULL)
			pstTail = pstTail->pNext;

		pstTail->pNext = pstReader;
	}
	pthread_mutex_unlock(&pstPath->mutex);
	return pstReader;
}

int Media_UnregisterFrameReader(AHEADFORM_MEDIA_CTRL *pstMediaCtrl, AHEADFORM_MEDIA_READER *pstReader)
{
	MEDIA_RECEIVE_PATH *pstPath = NULL;
	AHEADFORM_MEDIA_READER **ppReader = NULL;
	unsigned int i;
	int ret;

	if (pstMediaCtrl == NULL || pstMediaCtrl->pstStreams == NULL)
		return ERR_ARGV_1;

	if (pstReader == NULL)
		return ERR_ARGV_2;

	for (i = 0; i < AHEADFORM_MEDIA_STREAM_CNT; i++)
	{
		pstPath = &pstMediaCtrl->pstStreams->szstPath[i];
		if ( (ret = pthread_mutex_lock(&pstPath->mutex)) != RET_SUCCESS )
		{
			printf("%s() call pthread_mutex_lock(stream = %u) failed: %s\r\n", __FUNCTION__, i, strerror(ret));
			return ERR_MUTEX_OP_FAILED;
		}

		for (ppReader = &pstPath->pstReaders; *ppReader != NULL && *ppReader != pstReader; ppReader = &(*ppReader)->pNext)
		{
		}

		if (*ppReader != NULL)
		{
			if (close(pstReader->event_fd) < 0)
			{
				printf("%s() call close(eventfd = %d) failed: %s\r\n", __FUNCTION__, pstReader->event_fd, strerror(errno));
				pthread_mutex_unlock(&pstPath->mutex);
				return ERR_EVENTFD_IO_FAILED;
			}
			*ppReader = pstReader->pNext;
			pthread_mutex_unlock(&pstPath->mutex);
			free(pstReader);
			return RET_SUCCESS;
		}
		pthread_mutex_unlock(&pstPath->mutex);
	}

	return ERR_SUBSCRIPTION_NOT_FOUND;
}

int MediaReader_GetEventfd(AHEADFORM_MEDIA_READER *pstReader)
{
	return pstReader != NULL ? pstReader->event_fd : ERRCODE_ARGV_1;
}

unsigned int MediaReader_GetElemSize(AHEADFORM_MEDIA_READER *pstReader)
{
	return pstReader != NULL ? pstReader->stReader.pstFifo->sizeOf1Elem : 0;
}

int MediaReader_GetLastReceiveResult(AHEADFORM_MEDIA_READER *pstReader)
{
	if (pstReader == NULL)
		return ERR_ARGV_1;

	return __atomic_load_n(&pstReader->pstPath->lastResult, __ATOMIC_ACQUIRE);
}

unsigned int MediaReader_GetReadableElemCnt(AHEADFORM_MEDIA_READER *pstReader)
{
	return pstReader != NULL ? SPMCRingFifo_Reader_GetReadableElemCnt(&pstReader->stReader) : 0;
}

unsigned long long MediaReader_GetMissedElemCnt(AHEADFORM_MEDIA_READER *pstReader)
{
	return pstReader != NULL ? SPMCRingFifo_Reader_GetMissedElemCnt(&pstReader->stReader) : 0;
}

int MediaReader_PopFrame(AHEADFORM_MEDIA_READER *pstReader, AHEADFORM_MEDIA_FRAME *pstFrame, unsigned int frameBufSize)
{
	unsigned long long missed;
	unsigned int budget;
	int ret = 0;

	if (pstReader == NULL)
		return ERR_ARGV_1;

	if (pstFrame == NULL)
		return ERR_ARGV_2;

	if (frameBufSize < MediaReader_GetElemSize(pstReader))
		return ERR_ARGV_3;

	budget = SPMCRingFifo_Reader_GetReadableElemCnt(&pstReader->stReader);
	while (budget-- != 0 && (ret = SPMCRingFifo_Reader_PopElem(&pstReader->stReader, pstFrame)) == 1)
	{
		missed = SPMCRingFifo_Reader_GetMissedElemCnt(&pstReader->stReader);
		if (missed != pstReader->missedBeforePop || pstFrame->isDiscontinuous)
			pstReader->isWaitingForIdr = 1;

		pstReader->missedBeforePop = missed;
		if (pstReader->pstPath->stream == AHEADFORM_MEDIA_STREAM_VIDEO && pstReader->isWaitingForIdr && (!pstFrame->isKeyframe || !pstFrame->hasParameterSets))
			continue;

		pstFrame->isDiscontinuous |= pstReader->isWaitingForIdr;
		pstReader->isWaitingForIdr = 0;
		return 1;
	}

	return ret < 0 ? ret : 0;
}

int MediaReader_ReadLatestFrame(AHEADFORM_MEDIA_READER *pstReader, AHEADFORM_MEDIA_FRAME *pstFrame, unsigned int frameBufSize)
{
	int ret;

	if (pstReader == NULL)
		return ERR_ARGV_1;

	if (pstFrame == NULL)
		return ERR_ARGV_2;

	if (frameBufSize < MediaReader_GetElemSize(pstReader))
		return ERR_ARGV_3;

	if ( (ret = MediaReader_GetLastReceiveResult(pstReader)) != RET_SUCCESS )
		return ret;

	if ( (ret = SPMCRingFifo_ReadLatest1Elem(pstReader->stReader.pstFifo, pstFrame)) != 1 )
		return ret;

	if ( (ret = MediaReader_GetLastReceiveResult(pstReader)) != RET_SUCCESS )
		return ret;

	if (pstReader->pstPath->stream == AHEADFORM_MEDIA_STREAM_VIDEO)
		pstFrame->isDiscontinuous = 1;

	return 1;
}

int Media_DeinitStreams(AHEADFORM_MEDIA_CTRL *pstMediaCtrl)
{
	struct AHEADFORM_MEDIA_STREAMS *pstStreams = NULL;
	MEDIA_RECEIVE_PATH *pstPath = NULL;
	unsigned int i;
	int ret;

	if (pstMediaCtrl == NULL)
		return ERR_ARGV_1;

	if (pstMediaCtrl->pstStreams == NULL)
		return RET_SUCCESS;

	pstStreams = pstMediaCtrl->pstStreams;
	for (i = 0; i < AHEADFORM_MEDIA_STREAM_CNT; i++)
	{
		if (pstStreams->szstPath[i].pstReaders != NULL)
			return ERR_RESOURCE_BUSY;
	}

	if ( (ret = Media_StopSpeakerSend(pstMediaCtrl)) != RET_SUCCESS )
		return ret;

	for (i = 0; i < AHEADFORM_MEDIA_STREAM_CNT; i++)
	{
		if ( (ret = StopReceive(pstMediaCtrl, (ENUM_AHEADFORM_MEDIA_STREAM)i)) != RET_SUCCESS )
			return ret;

		pstPath = &pstStreams->szstPath[i];
		if (pstPath->isMutexInitialized)
		{
			if ( (ret = pthread_mutex_destroy(&pstPath->mutex)) != RET_SUCCESS )
			{
				printf("%s() call pthread_mutex_destroy(stream = %u) failed: %s\r\n", __FUNCTION__, i, strerror(ret));
				return ERR_MUTEX_OP_FAILED;
			}
			pstPath->isMutexInitialized = 0;
		}
		free(pstPath->stFifo.pElemAryBuf);
		SPMCRingFifo_Deinit(&pstPath->stFifo);
		free(pstPath->pstFrame);
		pstPath->pstFrame = NULL;
	}

	if (pstStreams->isSpeakerMutexInitialized)
	{
		if ( (ret = pthread_mutex_destroy(&pstStreams->speakerMutex)) != RET_SUCCESS )
		{
			printf("%s() call pthread_mutex_destroy(speaker) failed: %s\r\n", __FUNCTION__, strerror(ret));
			return ERR_MUTEX_OP_FAILED;
		}
		pstStreams->isSpeakerMutexInitialized = 0;
	}
	free(pstStreams);
	pstMediaCtrl->pstStreams = NULL;
	return RET_SUCCESS;
}
