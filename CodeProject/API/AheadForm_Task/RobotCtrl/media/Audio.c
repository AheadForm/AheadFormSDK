#include "Audio.h"
#include "AheadFormDdsMsg.h"
#include "Time.h"

#include <arpa/inet.h>
#include <ifaddrs.h>
#include <gst/app/gstappsink.h>
#include <gst/app/gstappsrc.h>

#include <math.h>
#include <time.h>
#include <sys/eventfd.h>
#include <sys/select.h>
#include <unistd.h>

static int BuildVolumeRequestIdentity(AHEADFORM_AUDIO_CTRL *pstAudioCtrl, AHEADFORM_VOLUME_ASK *pstRequest);
static int BuildAudioDeviceRequestIdentity(AHEADFORM_AUDIO_CTRL *pstAudioCtrl, AHEADFORM_AUDIO_DEVICE_ASK *pstRequest);
static int SubmitVolumeAsyncRequest(AHEADFORM_AUDIO_CTRL *pstAudioCtrl, unsigned long long MatchReplyTimeout_us, int notifyEventfd, const AHEADFORM_VOLUME_ASK *pstRequest, AHEADFORM_VOLUME_REPLY *pstReply);


int Audio_Init(AHEADFORM_AUDIO_CTRL *pstAudioCtrl, AHEADFORM_TOPICS *pstTopics, const AHEADFORM_INIT_CONFIG *pstCfg)
{
	AHEADFORM_TOPIC_READER_PARA stReaderPara = {0};
	int ret;

	if (pstAudioCtrl == NULL)
		return ERR_ARGV_1;

	if (pstTopics == NULL)
		return ERR_ARGV_2;

	if (pstTopics->stDdsCtrl.ddsParticipant <= 0)
		return ERR_ARGV_2;

	if (pstCfg == NULL)
		return ERR_ARGV_3;

	memset(pstAudioCtrl, 0, sizeof(*pstAudioCtrl));
	pstAudioCtrl->pstTopics = pstTopics;
	pstAudioCtrl->stCfg = pstCfg->stMedia;
	strcpy(pstAudioCtrl->strNetworkInterface, pstCfg->stDds.strNetworkInterface);
	if ( (pstAudioCtrl->pstVolumeReplyReader = Audio_RegisterVolumeReader(pstAudioCtrl, &stReaderPara)) == NULL )
		return ERRCODE_INIT_FAIL;

	if ( (pstAudioCtrl->pstVolumeAskWriter = AheadFormTopics_RegisterWriter(pstTopics, AHEADFORM_TOPIC_ID_VOLUME_ASK)) == NULL )
		return ERRCODE_INIT_FAIL;

	if ( (pstAudioCtrl->pstAudioDeviceReplyReader = Audio_RegisterAudioDeviceReader(pstAudioCtrl, &stReaderPara)) == NULL )
		return ERRCODE_INIT_FAIL;

	if ( (pstAudioCtrl->pstAudioDeviceAskWriter = AheadFormTopics_RegisterWriter(pstTopics, AHEADFORM_TOPIC_ID_AUDIO_DEVICE_ASK)) == NULL )
		return ERRCODE_INIT_FAIL;

	__atomic_store_n(&pstAudioCtrl->stMicCapture.lastError, ERRCODE_MEDIA_CHANNEL_NOT_READY, __ATOMIC_SEQ_CST);
	__atomic_store_n(&pstAudioCtrl->stSdkPlayAudio.lastError, ERRCODE_MEDIA_CHANNEL_NOT_READY, __ATOMIC_SEQ_CST);
	if ( (ret = pthread_mutex_init(&pstAudioCtrl->stMicCapture.readerListMutex, NULL)) != RET_SUCCESS )
	{
		printf("%s() call pthread_mutex_init(MIC reader list) failed: %s\r\n", __FUNCTION__, strerror(ret));
		return ERR_MUTEX_INIT;
	}

	if ( (ret = pthread_mutex_init(&pstAudioCtrl->stSdkPlayAudio.writeMutex, NULL)) != RET_SUCCESS )
	{
		printf("%s() call pthread_mutex_init(SDK audio write) failed: %s\r\n", __FUNCTION__, strerror(ret));
		return ERR_MUTEX_INIT;
	}

	if (pstAudioCtrl->stCfg.isMicCaptureOn)
	{
		if ( (ret = Audio_StartMicCapture(pstAudioCtrl)) != RET_SUCCESS )
			return ret;
	}

	if (pstAudioCtrl->stCfg.isSdkPlayAudioOn)
		return Audio_StartSdkPlayAudio(pstAudioCtrl);

	return RET_SUCCESS;
}

int Audio_Deinit(AHEADFORM_AUDIO_CTRL *pstAudioCtrl)
{
	int ret;

	if (pstAudioCtrl == NULL)
		return ERR_ARGV_1;

	if (pstAudioCtrl->stMicCapture.pstReaders != NULL)
		return ERRCODE_RESOURCE_BUSY;

	if ( (ret = Audio_StopSdkPlayAudio(pstAudioCtrl)) != RET_SUCCESS )
		return ret;

	if ( (ret = Audio_StopMicCapture(pstAudioCtrl)) != RET_SUCCESS )
		return ret;

	if (pstAudioCtrl->pstAudioDeviceReplyReader != NULL)
	{
		if ( (ret = AheadFormTopics_UnregisterReader(pstAudioCtrl->pstTopics, pstAudioCtrl->pstAudioDeviceReplyReader)) != RET_SUCCESS )
			return ret;

		pstAudioCtrl->pstAudioDeviceReplyReader = NULL;
	}

	if (pstAudioCtrl->pstAudioDeviceAskWriter != NULL)
	{
		if ( (ret = AheadFormTopics_UnregisterWriter(pstAudioCtrl->pstTopics, pstAudioCtrl->pstAudioDeviceAskWriter)) != RET_SUCCESS )
			return ret;

		pstAudioCtrl->pstAudioDeviceAskWriter = NULL;
	}

	if (pstAudioCtrl->pstVolumeReplyReader != NULL)
	{
		if ( (ret = AheadFormTopics_UnregisterReader(pstAudioCtrl->pstTopics, pstAudioCtrl->pstVolumeReplyReader)) != RET_SUCCESS )
			return ret;

		pstAudioCtrl->pstVolumeReplyReader = NULL;
	}

	if (pstAudioCtrl->pstVolumeAskWriter != NULL)
	{
		if ( (ret = AheadFormTopics_UnregisterWriter(pstAudioCtrl->pstTopics, pstAudioCtrl->pstVolumeAskWriter)) != RET_SUCCESS )
			return ret;

		pstAudioCtrl->pstVolumeAskWriter = NULL;
	}

	if ( (ret = pthread_mutex_destroy(&pstAudioCtrl->stSdkPlayAudio.writeMutex)) != RET_SUCCESS )
	{
		printf("%s() call pthread_mutex_destroy(SDK audio write) failed: %s\r\n", __FUNCTION__, strerror(ret));
		return ERR_MUTEX_OP_FAILED;
	}

	if ( (ret = pthread_mutex_destroy(&pstAudioCtrl->stMicCapture.readerListMutex)) != RET_SUCCESS )
	{
		printf("%s() call pthread_mutex_destroy(MIC reader list) failed: %s\r\n", __FUNCTION__, strerror(ret));
		return ERR_MUTEX_OP_FAILED;
	}
	SPMCRingFifo_Deinit(&pstAudioCtrl->stMicCapture.stFifo);
	free(pstAudioCtrl->stMicCapture.pstElemAry);
	memset(pstAudioCtrl, 0, sizeof(*pstAudioCtrl));
	return RET_SUCCESS;
}

AHEADFORM_TOPIC_READER* Audio_RegisterVolumeReader(AHEADFORM_AUDIO_CTRL *pstAudioCtrl, const AHEADFORM_TOPIC_READER_PARA *pstReaderPara)
{
	if (pstAudioCtrl == NULL)
	{
		PRINT_FUNCTION_ERRINFO(ERR_ARGV_1, __FUNCTION__);
		return NULL;
	}

	if (pstReaderPara == NULL)
	{
		PRINT_FUNCTION_ERRINFO(ERR_ARGV_2, __FUNCTION__);
		return NULL;
	}

	if (pstAudioCtrl->pstTopics == NULL)
	{
		PRINT_FUNCTION_ERRINFO(ERR_SDK_NOT_READY, __FUNCTION__);
		return NULL;
	}

	return AheadFormTopics_RegisterReader(pstAudioCtrl->pstTopics, AHEADFORM_TOPIC_ID_VOLUME_REPLY, pstReaderPara);
}

int Audio_UnregisterVolumeReader(AHEADFORM_AUDIO_CTRL *pstAudioCtrl, AHEADFORM_TOPIC_READER *pstReader)
{
	if (pstAudioCtrl == NULL)
		return ERR_ARGV_1;

	if (pstReader == NULL)
		return ERR_ARGV_2;

	if (pstAudioCtrl->pstTopics == NULL)
		return ERR_SDK_NOT_READY;

	if (pstReader->topicId != AHEADFORM_TOPIC_ID_VOLUME_REPLY || pstReader == pstAudioCtrl->pstVolumeReplyReader)
		return ERR_ARGV_2;

	return AheadFormTopics_UnregisterReader(pstAudioCtrl->pstTopics, pstReader);
}

int Audio_BuildQueryVolumeRequest(AHEADFORM_AUDIO_CTRL *pstAudioCtrl, AHEADFORM_VOLUME_ASK *pstRequest)
{
	int ret;

	if ( (ret = BuildVolumeRequestIdentity(pstAudioCtrl, pstRequest)) != RET_SUCCESS )
		return ret;

	pstRequest->operation = AHEADFORM_VOLUME_OPERATION_GET;
	return RET_SUCCESS;
}

int Audio_BuildSetVolumeRequest(AHEADFORM_AUDIO_CTRL *pstAudioCtrl, const AHEADFORM_VOLUME_SET_PARA *pstSetPara, AHEADFORM_VOLUME_ASK *pstRequest)
{
	int ret;

	if (pstAudioCtrl == NULL)
		return ERR_ARGV_1;

	if (pstSetPara == NULL)
		return ERR_ARGV_2;

	if (pstRequest == NULL)
		return ERR_ARGV_3;

	if (!pstSetPara->isSetMicVolume && !pstSetPara->isSetSpkVolume)
		return ERR_ARGV_2;

	if (pstSetPara->isSetMicVolume && (!isfinite(pstSetPara->stVolume.micVolumePercent) || pstSetPara->stVolume.micVolumePercent < 0.0f || pstSetPara->stVolume.micVolumePercent > 100.0f))
		return ERR_ARGV_2;

	if (pstSetPara->isSetSpkVolume && (!isfinite(pstSetPara->stVolume.spkVolumePercent) || pstSetPara->stVolume.spkVolumePercent < 0.0f || pstSetPara->stVolume.spkVolumePercent > 100.0f))
		return ERR_ARGV_2;

	if ( (ret = BuildVolumeRequestIdentity(pstAudioCtrl, pstRequest)) != RET_SUCCESS )
		return ret;

	pstRequest->operation = AHEADFORM_VOLUME_OPERATION_SET;
	pstRequest->isSetMicVolume = pstSetPara->isSetMicVolume != 0;
	pstRequest->isSetSpkVolume = pstSetPara->isSetSpkVolume != 0;
	if (pstRequest->isSetMicVolume)
		pstRequest->stVolume.micVolumePercent = pstSetPara->stVolume.micVolumePercent;

	if (pstRequest->isSetSpkVolume)
		pstRequest->stVolume.spkVolumePercent = pstSetPara->stVolume.spkVolumePercent;

	return RET_SUCCESS;
}

int Audio_SendVolumeRequest(AHEADFORM_AUDIO_CTRL *pstAudioCtrl, const AHEADFORM_VOLUME_ASK *pstAsk)
{
	arbitration_msgs_msg_MediaVolumeAsk stDdsAsk;
	struct timespec stTime;

	if (pstAudioCtrl == NULL)
		return ERR_ARGV_1;

	if (pstAsk == NULL)
		return ERR_ARGV_2;

	if (pstAudioCtrl->pstTopics == NULL || pstAudioCtrl->pstVolumeAskWriter == NULL)
		return ERR_SDK_NOT_READY;

	if (pstAsk->askId == 0 || pstAsk->sourceName[0] == 0 || strnlen(pstAsk->sourceName, sizeof(pstAsk->sourceName)) == sizeof(pstAsk->sourceName))
		return ERR_ARGV_2;

	if (pstAsk->operation != AHEADFORM_VOLUME_OPERATION_GET && pstAsk->operation != AHEADFORM_VOLUME_OPERATION_SET)
		return ERR_ARGV_2;

	if (pstAsk->operation == AHEADFORM_VOLUME_OPERATION_SET && (!pstAsk->isSetMicVolume && !pstAsk->isSetSpkVolume))
		return ERR_ARGV_2;

	if (pstAsk->isSetMicVolume && (!isfinite(pstAsk->stVolume.micVolumePercent) || pstAsk->stVolume.micVolumePercent < 0.0f || pstAsk->stVolume.micVolumePercent > 100.0f))
		return ERR_ARGV_2;

	if (pstAsk->isSetSpkVolume && (!isfinite(pstAsk->stVolume.spkVolumePercent) || pstAsk->stVolume.spkVolumePercent < 0.0f || pstAsk->stVolume.spkVolumePercent > 100.0f))
		return ERR_ARGV_2;

	if (clock_gettime(CLOCK_REALTIME, &stTime) < 0)
	{
		printf("%s() call clock_gettime(CLOCK_REALTIME) failed: %s\r\n", __FUNCTION__, strerror(errno));
		return ERR_CLOCK;
	}
	stDdsAsk.stamp.sec = stTime.tv_sec;
	stDdsAsk.stamp.nanosec = stTime.tv_nsec;
	stDdsAsk.ask_id = pstAsk->askId;
	stDdsAsk.source_name = (char*)pstAsk->sourceName;
	stDdsAsk.operation = pstAsk->operation;
	stDdsAsk.has_mic_volume = pstAsk->isSetMicVolume != 0;
	stDdsAsk.mic_volume_percent = pstAsk->stVolume.micVolumePercent;
	stDdsAsk.has_spk_volume = pstAsk->isSetSpkVolume != 0;
	stDdsAsk.spk_volume_percent = pstAsk->stVolume.spkVolumePercent;
	return TopicWriter_Write(pstAudioCtrl->pstVolumeAskWriter, &stDdsAsk);
}

int Audio_QueryVolumeAsync(AHEADFORM_AUDIO_CTRL *pstAudioCtrl, unsigned long long MatchReplyTimeout_us, int notifyEventfd, AHEADFORM_VOLUME_REPLY *pstReply)
{
	AHEADFORM_VOLUME_ASK stRequest;
	int ret;

	if (pstAudioCtrl == NULL)
		return ERR_ARGV_1;

	if (MatchReplyTimeout_us == 0)
		return ERR_ARGV_2;

	if (notifyEventfd <= 0)
		return ERR_ARGV_3;

	if (pstReply == NULL)
		return ERR_ARGV_4;

	if ( (ret = Audio_BuildQueryVolumeRequest(pstAudioCtrl, &stRequest)) != RET_SUCCESS )
		return ret;

	return SubmitVolumeAsyncRequest(pstAudioCtrl, MatchReplyTimeout_us, notifyEventfd, &stRequest, pstReply);
}

int Audio_SetVolumeAsync(AHEADFORM_AUDIO_CTRL *pstAudioCtrl, const AHEADFORM_VOLUME_SET_PARA *pstSetPara, unsigned long long MatchReplyTimeout_us, int notifyEventfd, AHEADFORM_VOLUME_REPLY *pstReply)
{
	AHEADFORM_VOLUME_ASK stRequest;
	int ret;

	if (pstAudioCtrl == NULL)
		return ERR_ARGV_1;

	if (pstSetPara == NULL)
		return ERR_ARGV_2;

	if (MatchReplyTimeout_us == 0)
		return ERR_ARGV_3;

	if (notifyEventfd <= 0)
		return ERR_ARGV_4;

	if (pstReply == NULL)
		return ERR_ARGV_5;

	if ( (ret = Audio_BuildSetVolumeRequest(pstAudioCtrl, pstSetPara, &stRequest)) != RET_SUCCESS )
		return ret;

	return SubmitVolumeAsyncRequest(pstAudioCtrl, MatchReplyTimeout_us, notifyEventfd, &stRequest, pstReply);
}

int Audio_CheckIsVolumeAsyncRequestReplied(AHEADFORM_AUDIO_CTRL *pstAudioCtrl, int notifyEventfd)
{
	if (pstAudioCtrl == NULL)
		return ERR_ARGV_1;

	if (pstAudioCtrl->pstTopics == NULL || pstAudioCtrl->pstVolumeReplyReader == NULL)
		return ERR_SDK_NOT_READY;

	return TopicReader_CheckIsAsyncRequestReplied(pstAudioCtrl->pstVolumeReplyReader, notifyEventfd);
}

int Audio_CancelVolumeAsyncRequest(AHEADFORM_AUDIO_CTRL *pstAudioCtrl, int notifyEventfd)
{
	if (pstAudioCtrl == NULL)
		return ERR_ARGV_1;

	if (pstAudioCtrl->pstTopics == NULL || pstAudioCtrl->pstVolumeReplyReader == NULL)
		return ERR_SDK_NOT_READY;

	return TopicReader_CancelAsyncRequest(pstAudioCtrl->pstVolumeReplyReader, notifyEventfd);
}

int Audio_MatchVolumeReplyWithReq(const void *pReplyElem, const void *pRequest)
{
	const AHEADFORM_VOLUME_REPLY *pstReply = (const AHEADFORM_VOLUME_REPLY*)pReplyElem;
	AHEADFORM_VOLUME_ASK stRequest;

	if (pstReply == NULL)
		return ERR_ARGV_1;

	if (pRequest == NULL)
		return ERR_ARGV_2;

	memcpy(&stRequest, pRequest, sizeof(stRequest));
	if (pstReply->askId != stRequest.askId || pstReply->operation != stRequest.operation || strcmp(pstReply->sourceName, stRequest.sourceName) != 0)
		return ERRCODE_MATCH_REPLY_WITH_REQ_FAIL;

	return RET_SUCCESS;
}

int Audio_GetVolumeFromReply(const AHEADFORM_VOLUME_REPLY *pstReply, AHEADFORM_VOLUME *pstVolume)
{
	if (pstReply == NULL)
		return ERR_ARGV_1;

	if (pstVolume == NULL)
		return ERR_ARGV_2;

	if (pstReply->retCode != RET_SUCCESS)
		return ERR_REQUEST_REJECTED;

	if (pstReply->operation != AHEADFORM_VOLUME_OPERATION_GET || !pstReply->queryDataValid)
		return ERR_DATA_PACKET_FORMAT;

	if (!isfinite(pstReply->stVolume.micVolumePercent) || pstReply->stVolume.micVolumePercent < 0.0f || pstReply->stVolume.micVolumePercent > 100.0f)
		return ERR_DATA_PACKET_FORMAT;

	if (!isfinite(pstReply->stVolume.spkVolumePercent) || pstReply->stVolume.spkVolumePercent < 0.0f || pstReply->stVolume.spkVolumePercent > 100.0f)
		return ERR_DATA_PACKET_FORMAT;

	*pstVolume = pstReply->stVolume;
	return RET_SUCCESS;
}

int Audio_QueryVolumeAndWaitReply(AHEADFORM_AUDIO_CTRL *pstAudioCtrl, AHEADFORM_VOLUME_REPLY *pstReply, int timeout_ms)
{
	eventfd_t eventCnt;
	struct timeval stTimeout;
	fd_set rdFdSet;
	int notifyEventfd, max_fd, cleanupRet, ret;

	if (pstAudioCtrl == NULL)
		return ERR_ARGV_1;

	if (pstReply == NULL)
		return ERR_ARGV_2;

	if (timeout_ms <= 0)
		return ERR_ARGV_3;

	if ( (notifyEventfd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC)) <= 0 )
	{
		printf("%s() call eventfd(volume completion) failed: %s\r\n", __FUNCTION__, strerror(errno));
		return ERR_EVENTFD_CREATE_FAILED;
	}

	if ( (ret = Audio_QueryVolumeAsync(pstAudioCtrl, (unsigned long long)timeout_ms * 1000, notifyEventfd, pstReply)) != RET_SUCCESS )
	{
		if (close(notifyEventfd) < 0)
		{
			printf("%s() call close(notifyEventfd = %d) failed: %s\r\n", __FUNCTION__, notifyEventfd, strerror(errno));
			return ERR_EVENTFD_IO_FAILED;
		}

		return ret;
	}
	stTimeout.tv_sec = timeout_ms / 1000;
	stTimeout.tv_usec = timeout_ms % 1000 * 1000;
	max_fd = notifyEventfd;
	FD_ZERO(&rdFdSet);
	FD_SET(notifyEventfd, &rdFdSet);
	if ( (ret = select(max_fd + 1, &rdFdSet, NULL, NULL, &stTimeout)) < 0 )
	{
		printf("%s() call select(notifyEventfd = %d) failed: %s\r\n", __FUNCTION__, notifyEventfd, strerror(errno));
		ret = ERR_SELECT;
	}
	else if (ret == 0)
	{
		ret = ERRCODE_REQUEST_TIMEOUT;
	}
	else if (FD_ISSET(notifyEventfd, &rdFdSet))
	{
		ret = RET_SUCCESS;
	}

	if (ret != RET_SUCCESS)
	{
		if ( (cleanupRet = Audio_CancelVolumeAsyncRequest(pstAudioCtrl, notifyEventfd)) != RET_SUCCESS )
		{
			ret = cleanupRet;
		}
		else if (ret == ERRCODE_REQUEST_TIMEOUT)
		{
			if (eventfd_read(notifyEventfd, &eventCnt) == RET_SUCCESS)
			{
				ret = RET_SUCCESS;
			}
			else if (errno != EAGAIN)
			{
				printf("%s() call eventfd_read(notifyEventfd = %d) failed: %s\r\n", __FUNCTION__, notifyEventfd, strerror(errno));
				ret = ERR_EVENTFD_IO_FAILED;
			}
		}
	}

	if (close(notifyEventfd) < 0)
	{
		printf("%s() call close(notifyEventfd = %d) failed: %s\r\n", __FUNCTION__, notifyEventfd, strerror(errno));
		return ERR_EVENTFD_IO_FAILED;
	}

	return ret;
}

AHEADFORM_TOPIC_READER* Audio_RegisterAudioDeviceReader(AHEADFORM_AUDIO_CTRL *pstAudioCtrl, const AHEADFORM_TOPIC_READER_PARA *pstReaderPara)
{
	if (pstAudioCtrl == NULL)
	{
		PRINT_FUNCTION_ERRINFO(ERR_ARGV_1, __FUNCTION__);
		return NULL;
	}

	if (pstReaderPara == NULL)
	{
		PRINT_FUNCTION_ERRINFO(ERR_ARGV_2, __FUNCTION__);
		return NULL;
	}

	if (pstAudioCtrl->pstTopics == NULL)
	{
		PRINT_FUNCTION_ERRINFO(ERR_SDK_NOT_READY, __FUNCTION__);
		return NULL;
	}

	return AheadFormTopics_RegisterReader(pstAudioCtrl->pstTopics, AHEADFORM_TOPIC_ID_AUDIO_DEVICE_REPLY, pstReaderPara);
}

int Audio_UnregisterAudioDeviceReader(AHEADFORM_AUDIO_CTRL *pstAudioCtrl, AHEADFORM_TOPIC_READER *pstReader)
{
	if (pstAudioCtrl == NULL)
		return ERR_ARGV_1;

	if (pstReader == NULL)
		return ERR_ARGV_2;

	if (pstAudioCtrl->pstTopics == NULL)
		return ERR_SDK_NOT_READY;

	if (pstReader->topicId != AHEADFORM_TOPIC_ID_AUDIO_DEVICE_REPLY || pstReader == pstAudioCtrl->pstAudioDeviceReplyReader)
		return ERR_ARGV_2;

	return AheadFormTopics_UnregisterReader(pstAudioCtrl->pstTopics, pstReader);
}

int Audio_BuildQueryAudioDeviceRequest(AHEADFORM_AUDIO_CTRL *pstAudioCtrl, ENUM_AHEADFORM_AUDIO_DEVICE_OPERATION operation, ENUM_AHEADFORM_AUDIO_DEVICE_TYPE deviceType, AHEADFORM_AUDIO_DEVICE_ASK *pstRequest)
{
	int ret;

	if (pstAudioCtrl == NULL)
		return ERR_ARGV_1;

	if (operation != AHEADFORM_AUDIO_DEVICE_OPERATION_GET_LIST && operation != AHEADFORM_AUDIO_DEVICE_OPERATION_GET_CURRENT)
		return ERR_ARGV_2;

	if (deviceType != AHEADFORM_AUDIO_DEVICE_TYPE_MIC && deviceType != AHEADFORM_AUDIO_DEVICE_TYPE_SPK)
		return ERR_ARGV_3;

	if (pstRequest == NULL)
		return ERR_ARGV_4;

	if ( (ret = BuildAudioDeviceRequestIdentity(pstAudioCtrl, pstRequest)) != RET_SUCCESS )
		return ret;

	pstRequest->operation = operation;
	pstRequest->deviceType = deviceType;
	pstRequest->stRoute.micDeviceId[0] = 0;
	pstRequest->stRoute.spkDeviceId[0] = 0;
	return RET_SUCCESS;
}

int Audio_BuildRouteAudioDeviceRequest(AHEADFORM_AUDIO_CTRL *pstAudioCtrl, const AHEADFORM_AUDIO_DEVICE_ROUTE *pstRoute, AHEADFORM_AUDIO_DEVICE_ASK *pstRequest)
{
	int ret;

	if (pstAudioCtrl == NULL)
		return ERR_ARGV_1;

	if (pstRoute == NULL)
		return ERR_ARGV_2;

	if (pstRequest == NULL)
		return ERR_ARGV_3;

	if (strnlen(pstRoute->micDeviceId, sizeof(pstRoute->micDeviceId)) == sizeof(pstRoute->micDeviceId))
		return ERR_ARGV_2;

	if (strnlen(pstRoute->spkDeviceId, sizeof(pstRoute->spkDeviceId)) == sizeof(pstRoute->spkDeviceId))
		return ERR_ARGV_2;

	if (pstRoute->micDeviceId[0] == 0 && pstRoute->spkDeviceId[0] == 0)
		return ERR_ARGV_2;

	if ( (ret = BuildAudioDeviceRequestIdentity(pstAudioCtrl, pstRequest)) != RET_SUCCESS )
		return ret;

	pstRequest->operation = AHEADFORM_AUDIO_DEVICE_OPERATION_ROUTE;
	pstRequest->deviceType = AHEADFORM_AUDIO_DEVICE_TYPE_MIC;
	strcpy(pstRequest->stRoute.micDeviceId, pstRoute->micDeviceId);
	strcpy(pstRequest->stRoute.spkDeviceId, pstRoute->spkDeviceId);
	return RET_SUCCESS;
}

int Audio_SendAudioDeviceRequest(AHEADFORM_AUDIO_CTRL *pstAudioCtrl, const AHEADFORM_AUDIO_DEVICE_ASK *pstAsk)
{
	arbitration_msgs_msg_MediaAudioDeviceAsk stDdsAsk;
	struct timespec stTime;

	if (pstAudioCtrl == NULL)
		return ERR_ARGV_1;

	if (pstAsk == NULL)
		return ERR_ARGV_2;

	if (pstAudioCtrl->pstTopics == NULL || pstAudioCtrl->pstAudioDeviceAskWriter == NULL)
		return ERR_SDK_NOT_READY;

	if (pstAsk->askId == 0 || pstAsk->sourceName[0] == 0 || strnlen(pstAsk->sourceName, sizeof(pstAsk->sourceName)) == sizeof(pstAsk->sourceName))
		return ERR_ARGV_2;

	if (pstAsk->operation == AHEADFORM_AUDIO_DEVICE_OPERATION_ROUTE)
	{
		if (strnlen(pstAsk->stRoute.micDeviceId, sizeof(pstAsk->stRoute.micDeviceId)) == sizeof(pstAsk->stRoute.micDeviceId))
			return ERR_ARGV_2;

		if (strnlen(pstAsk->stRoute.spkDeviceId, sizeof(pstAsk->stRoute.spkDeviceId)) == sizeof(pstAsk->stRoute.spkDeviceId))
			return ERR_ARGV_2;

		if (pstAsk->stRoute.micDeviceId[0] == 0 && pstAsk->stRoute.spkDeviceId[0] == 0)
			return ERR_ARGV_2;
	}
	else if (pstAsk->operation == AHEADFORM_AUDIO_DEVICE_OPERATION_GET_LIST || pstAsk->operation == AHEADFORM_AUDIO_DEVICE_OPERATION_GET_CURRENT)
	{
		if (pstAsk->deviceType != AHEADFORM_AUDIO_DEVICE_TYPE_MIC && pstAsk->deviceType != AHEADFORM_AUDIO_DEVICE_TYPE_SPK)
			return ERR_ARGV_2;
	}
	else
	{
		return ERR_ARGV_2;
	}

	if (clock_gettime(CLOCK_REALTIME, &stTime) < 0)
	{
		printf("%s() call clock_gettime(CLOCK_REALTIME) failed: %s\r\n", __FUNCTION__, strerror(errno));
		return ERR_CLOCK;
	}
	stDdsAsk.stamp.sec = stTime.tv_sec;
	stDdsAsk.stamp.nanosec = stTime.tv_nsec;
	stDdsAsk.ask_id = pstAsk->askId;
	stDdsAsk.source_name = (char*)pstAsk->sourceName;
	stDdsAsk.operation = pstAsk->operation;
	stDdsAsk.device_type = pstAsk->operation == AHEADFORM_AUDIO_DEVICE_OPERATION_ROUTE ? AHEADFORM_AUDIO_DEVICE_TYPE_MIC : pstAsk->deviceType;
	stDdsAsk.has_mic_device = pstAsk->operation == AHEADFORM_AUDIO_DEVICE_OPERATION_ROUTE && pstAsk->stRoute.micDeviceId[0] != 0;
	stDdsAsk.mic_device_id = pstAsk->operation == AHEADFORM_AUDIO_DEVICE_OPERATION_ROUTE ? (char*)pstAsk->stRoute.micDeviceId : "";
	stDdsAsk.has_spk_device = pstAsk->operation == AHEADFORM_AUDIO_DEVICE_OPERATION_ROUTE && pstAsk->stRoute.spkDeviceId[0] != 0;
	stDdsAsk.spk_device_id = pstAsk->operation == AHEADFORM_AUDIO_DEVICE_OPERATION_ROUTE ? (char*)pstAsk->stRoute.spkDeviceId : "";
	return TopicWriter_Write(pstAudioCtrl->pstAudioDeviceAskWriter, &stDdsAsk);
}

int Audio_SubmitAudioDeviceAsyncRequest(AHEADFORM_AUDIO_CTRL *pstAudioCtrl, const AHEADFORM_AUDIO_DEVICE_ASK *pstAsk, unsigned long long MatchReplyTimeout_us, int notifyEventfd, AHEADFORM_AUDIO_DEVICE_REPLY *pstReply)
{
	int cancelRet, ret;

	if (pstAudioCtrl == NULL)
		return ERR_ARGV_1;

	if (pstAsk == NULL)
		return ERR_ARGV_2;

	if (MatchReplyTimeout_us == 0)
		return ERR_ARGV_3;

	if (notifyEventfd <= 0)
		return ERR_ARGV_4;

	if (pstReply == NULL)
		return ERR_ARGV_5;

	if (pstAudioCtrl->pstTopics == NULL || pstAudioCtrl->pstAudioDeviceAskWriter == NULL || pstAudioCtrl->pstAudioDeviceReplyReader == NULL)
		return ERR_SDK_NOT_READY;

	if ( (ret = TopicReader_RegisterAsyncRequest(pstAudioCtrl->pstAudioDeviceReplyReader, Audio_MatchAudioDeviceReplyWithReq, pstAsk, sizeof(*pstAsk), MatchReplyTimeout_us, notifyEventfd, pstReply, sizeof(*pstReply))) != RET_SUCCESS )
		return ret;

	if ( (ret = Audio_SendAudioDeviceRequest(pstAudioCtrl, pstAsk)) != RET_SUCCESS )
	{
		if ( (cancelRet = TopicReader_CancelAsyncRequest(pstAudioCtrl->pstAudioDeviceReplyReader, notifyEventfd)) != RET_SUCCESS )
			return cancelRet;

		return ret;
	}

	return RET_SUCCESS;
}

int Audio_CheckIsAudioDeviceAsyncRequestReplied(AHEADFORM_AUDIO_CTRL *pstAudioCtrl, int notifyEventfd)
{
	if (pstAudioCtrl == NULL)
		return ERR_ARGV_1;

	if (pstAudioCtrl->pstTopics == NULL || pstAudioCtrl->pstAudioDeviceReplyReader == NULL)
		return ERR_SDK_NOT_READY;

	return TopicReader_CheckIsAsyncRequestReplied(pstAudioCtrl->pstAudioDeviceReplyReader, notifyEventfd);
}

int Audio_CancelAudioDeviceAsyncRequest(AHEADFORM_AUDIO_CTRL *pstAudioCtrl, int notifyEventfd)
{
	if (pstAudioCtrl == NULL)
		return ERR_ARGV_1;

	if (pstAudioCtrl->pstTopics == NULL || pstAudioCtrl->pstAudioDeviceReplyReader == NULL)
		return ERR_SDK_NOT_READY;

	return TopicReader_CancelAsyncRequest(pstAudioCtrl->pstAudioDeviceReplyReader, notifyEventfd);
}

int Audio_MatchAudioDeviceReplyWithReq(const void *pReplyElem, const void *pRequest)
{
	const AHEADFORM_AUDIO_DEVICE_REPLY *pstReply = (const AHEADFORM_AUDIO_DEVICE_REPLY*)pReplyElem;
	AHEADFORM_AUDIO_DEVICE_ASK stRequest;

	if (pstReply == NULL)
		return ERR_ARGV_1;

	if (pRequest == NULL)
		return ERR_ARGV_2;

	memcpy(&stRequest, pRequest, sizeof(stRequest));
	if (pstReply->askId != stRequest.askId || pstReply->operation != stRequest.operation || strcmp(pstReply->sourceName, stRequest.sourceName) != 0)
		return ERRCODE_MATCH_REPLY_WITH_REQ_FAIL;

	if (stRequest.operation != AHEADFORM_AUDIO_DEVICE_OPERATION_ROUTE && pstReply->deviceType != stRequest.deviceType)
		return ERRCODE_MATCH_REPLY_WITH_REQ_FAIL;

	return RET_SUCCESS;
}

int Audio_SubmitAudioDeviceAsyncRequestAndWaitReply(AHEADFORM_AUDIO_CTRL *pstAudioCtrl, const AHEADFORM_AUDIO_DEVICE_ASK *pstAsk, AHEADFORM_AUDIO_DEVICE_REPLY *pstReply, int timeout_ms)
{
	eventfd_t eventCnt;
	struct timeval stTimeout;
	fd_set rdFdSet;
	int notifyEventfd, max_fd, cleanupRet, ret;

	if (pstAudioCtrl == NULL)
		return ERR_ARGV_1;

	if (pstAsk == NULL)
		return ERR_ARGV_2;

	if (pstReply == NULL)
		return ERR_ARGV_3;

	if (timeout_ms <= 0)
		return ERR_ARGV_4;

	if (pstAudioCtrl->pstTopics == NULL || pstAudioCtrl->pstAudioDeviceReplyReader == NULL || pstAudioCtrl->pstAudioDeviceAskWriter == NULL)
		return ERR_SDK_NOT_READY;

	if ( (notifyEventfd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC)) <= 0 )
	{
		printf("%s() call eventfd(audio device completion) failed: %s\r\n", __FUNCTION__, strerror(errno));
		return ERR_EVENTFD_CREATE_FAILED;
	}

	if ( (ret = Audio_SubmitAudioDeviceAsyncRequest(pstAudioCtrl, pstAsk, (unsigned long long)timeout_ms * 1000, notifyEventfd, pstReply)) != RET_SUCCESS )
	{
		if (close(notifyEventfd) < 0)
		{
			printf("%s() call close(notifyEventfd = %d) failed: %s\r\n", __FUNCTION__, notifyEventfd, strerror(errno));
			return ERR_EVENTFD_IO_FAILED;
		}

		return ret;
	}
	stTimeout.tv_sec = timeout_ms / 1000;
	stTimeout.tv_usec = timeout_ms % 1000 * 1000;
	max_fd = notifyEventfd;
	FD_ZERO(&rdFdSet);
	FD_SET(notifyEventfd, &rdFdSet);
	if ( (ret = select(max_fd + 1, &rdFdSet, NULL, NULL, &stTimeout)) < 0 )
	{
		printf("%s() call select(notifyEventfd = %d) failed: %s\r\n", __FUNCTION__, notifyEventfd, strerror(errno));
		ret = ERR_SELECT;
	}
	else if (ret == 0)
	{
		ret = ERRCODE_REQUEST_TIMEOUT;
	}
	else if (FD_ISSET(notifyEventfd, &rdFdSet))
	{
		ret = RET_SUCCESS;
	}

	if (ret != RET_SUCCESS)
	{
		if ( (cleanupRet = Audio_CancelAudioDeviceAsyncRequest(pstAudioCtrl, notifyEventfd)) != RET_SUCCESS )
		{
			ret = cleanupRet;
		}
		else if (ret == ERRCODE_REQUEST_TIMEOUT)
		{
			if (eventfd_read(notifyEventfd, &eventCnt) == RET_SUCCESS)
			{
				ret = RET_SUCCESS;
			}
			else if (errno != EAGAIN)
			{
				printf("%s() call eventfd_read(notifyEventfd = %d) failed: %s\r\n", __FUNCTION__, notifyEventfd, strerror(errno));
				ret = ERR_EVENTFD_IO_FAILED;
			}
		}
	}

	if (close(notifyEventfd) < 0)
	{
		printf("%s() call close(notifyEventfd = %d) failed: %s\r\n", __FUNCTION__, notifyEventfd, strerror(errno));
		return ERR_EVENTFD_IO_FAILED;
	}

	return ret;
}

static int BuildVolumeRequestIdentity(AHEADFORM_AUDIO_CTRL *pstAudioCtrl, AHEADFORM_VOLUME_ASK *pstRequest)
{
	dds_guid_t stGuid;
	dds_return_t ddsRet;
	unsigned int i;

	if (pstAudioCtrl == NULL)
		return ERR_ARGV_1;

	if (pstRequest == NULL)
		return ERR_ARGV_2;

	if (pstAudioCtrl->pstTopics == NULL || pstAudioCtrl->pstVolumeAskWriter == NULL || pstAudioCtrl->pstVolumeReplyReader == NULL)
		return ERR_SDK_NOT_READY;

	if ( (ddsRet = dds_get_guid(*pstAudioCtrl->pstVolumeAskWriter->pDdsWriter, &stGuid)) < 0 )
	{
		printf("%s() call dds_get_guid(volume ask writer = %d) failed: ddsRet=%d\r\n", __FUNCTION__, *pstAudioCtrl->pstVolumeAskWriter->pDdsWriter, ddsRet);
		return ERR_EVENT;
	}
	memset(pstRequest, 0, sizeof(*pstRequest));
	do
	{
		pstRequest->askId = __atomic_add_fetch(&pstAudioCtrl->pstTopics->nextAskId, 1, __ATOMIC_RELAXED);
	} while (pstRequest->askId == 0);

	strcpy(pstRequest->sourceName, "aheadform_sdk_");
	for (i = 0; i < sizeof(stGuid.v); i++)
		snprintf(pstRequest->sourceName + sizeof("aheadform_sdk_") - 1 + i * 2, 3, "%02x", (unsigned int)stGuid.v[i]);

	return RET_SUCCESS;
}

static int BuildAudioDeviceRequestIdentity(AHEADFORM_AUDIO_CTRL *pstAudioCtrl, AHEADFORM_AUDIO_DEVICE_ASK *pstRequest)
{
	dds_guid_t stGuid;
	dds_return_t ddsRet;
	unsigned int i;

	if (pstAudioCtrl == NULL)
		return ERR_ARGV_1;

	if (pstRequest == NULL)
		return ERR_ARGV_2;

	if (pstAudioCtrl->pstTopics == NULL || pstAudioCtrl->pstAudioDeviceAskWriter == NULL || pstAudioCtrl->pstAudioDeviceReplyReader == NULL)
		return ERR_SDK_NOT_READY;

	if ( (ddsRet = dds_get_guid(*pstAudioCtrl->pstAudioDeviceAskWriter->pDdsWriter, &stGuid)) < 0 )
	{
		printf("%s() call dds_get_guid(audio device ask writer = %d) failed: %s\r\n", __FUNCTION__, *pstAudioCtrl->pstAudioDeviceAskWriter->pDdsWriter, dds_strretcode(-ddsRet));
		return ERR_EVENT;
	}

	do
	{
		pstRequest->askId = __atomic_add_fetch(&pstAudioCtrl->pstTopics->nextAskId, 1, __ATOMIC_RELAXED);
	} while (pstRequest->askId == 0);

	strcpy(pstRequest->sourceName, "aheadform_sdk_");
	for (i = 0; i < sizeof(stGuid.v); i++)
		snprintf(pstRequest->sourceName + sizeof("aheadform_sdk_") - 1 + i * 2, 3, "%02x", (unsigned int)stGuid.v[i]);

	return RET_SUCCESS;
}

static int SubmitVolumeAsyncRequest(AHEADFORM_AUDIO_CTRL *pstAudioCtrl, unsigned long long MatchReplyTimeout_us, int notifyEventfd, const AHEADFORM_VOLUME_ASK *pstRequest, AHEADFORM_VOLUME_REPLY *pstReply)
{
	int cancelRet, ret;

	if ( (ret = TopicReader_RegisterAsyncRequest(pstAudioCtrl->pstVolumeReplyReader, Audio_MatchVolumeReplyWithReq, pstRequest, sizeof(*pstRequest), MatchReplyTimeout_us, notifyEventfd, pstReply, sizeof(*pstReply))) != RET_SUCCESS )
		return ret;

	if ( (ret = Audio_SendVolumeRequest(pstAudioCtrl, pstRequest)) != RET_SUCCESS )
	{
		if ( (cancelRet = TopicReader_CancelAsyncRequest(pstAudioCtrl->pstVolumeReplyReader, notifyEventfd)) != RET_SUCCESS )
			return cancelRet;

		return ret;
	}

	return RET_SUCCESS;
}

static int NotifyMicReaders(AHEADFORM_MIC_CAPTURE *pstCapture)
{
	AHEADFORM_MIC_READER *pstReader = NULL;
	int ret;

	if ( (ret = pthread_mutex_lock(&pstCapture->readerListMutex)) != RET_SUCCESS )
	{
		printf("%s() call pthread_mutex_lock(MIC reader list) failed: %s\r\n", __FUNCTION__, strerror(ret));
		return ERR_MUTEX_OP_FAILED;
	}

	for (pstReader = pstCapture->pstReaders; pstReader != NULL; pstReader = pstReader->pNext)
	{
		if (pstReader->rxDataUpdateEventfd > 0 && eventfd_write(pstReader->rxDataUpdateEventfd, 1) < 0 && errno != EAGAIN)
		{
			printf("%s() call eventfd_write(MIC fd = %d) failed: %s\r\n", __FUNCTION__, pstReader->rxDataUpdateEventfd, strerror(errno));
			ret = ERR_EVENTFD_IO_FAILED;
		}
	}
	pthread_mutex_unlock(&pstCapture->readerListMutex);
	return ret;
}

static GstBusSyncReply CallBack_OnMicCaptureBusMessageReceived(GstBus *pBus, GstMessage *pMessage, gpointer pUserData)
{
	AHEADFORM_MIC_CAPTURE *pstCapture = (AHEADFORM_MIC_CAPTURE*)pUserData;
	GError *pError = NULL;
	gchar *pDebug = NULL;

	if (GST_MESSAGE_TYPE(pMessage) == GST_MESSAGE_ERROR)
	{
		gst_message_parse_error(pMessage, &pError, &pDebug);
		printf("%s() MIC pipeline error: %s\r\n", __FUNCTION__, pError != NULL ? pError->message : "no GStreamer error detail");
		g_clear_error(&pError);
		g_free(pDebug);
		__atomic_store_n(&pstCapture->lastError, ERRCODE_RX_FAILED, __ATOMIC_SEQ_CST);
		NotifyMicReaders(pstCapture);
	}
	else if (GST_MESSAGE_TYPE(pMessage) == GST_MESSAGE_EOS)
	{
		printf("%s() MIC pipeline reached unexpected EOS.\r\n", __FUNCTION__);
		__atomic_store_n(&pstCapture->lastError, ERRCODE_RX_FAILED, __ATOMIC_SEQ_CST);
		NotifyMicReaders(pstCapture);
	}
	gst_message_unref(pMessage);
	return GST_BUS_DROP;
}

static GstFlowReturn CallBack_RxParseMicNewSampleToPcmFrameFifo(GstAppSink *pSink, gpointer pUserData)
{
	AHEADFORM_MIC_CAPTURE *pstCapture = (AHEADFORM_MIC_CAPTURE*)pUserData;
	GstSample *pSample = NULL;
	GstBuffer *pBuffer = NULL;
	GstCaps *pCaps = NULL;
	const GstStructure *pStructure = NULL;
	const char *format = NULL;
	GstMapInfo stMap;
	GstClockTime pts_ns;
	gint64 ptsDiff_ns;
	size_t offset, copyLen;
	unsigned long long receiveTime_us;
	int rate, channels, ret = RET_SUCCESS;

	if ( (pSample = gst_app_sink_pull_sample(pSink)) == NULL )
		return GST_FLOW_EOS;

	pBuffer = gst_sample_get_buffer(pSample);
	pCaps = gst_sample_get_caps(pSample);
	if (pBuffer == NULL || pCaps == NULL || !gst_buffer_map(pBuffer, &stMap, GST_MAP_READ))
	{
		printf("%s() cannot map the MIC sample.\r\n", __FUNCTION__);
		gst_sample_unref(pSample);
		__atomic_store_n(&pstCapture->lastError, ERRCODE_RX_FAILED, __ATOMIC_SEQ_CST);
		NotifyMicReaders(pstCapture);
		return GST_FLOW_ERROR;
	}
	pStructure = gst_caps_get_structure(pCaps, 0);
	format = gst_structure_get_string(pStructure, "format");
	if (format == NULL || strcmp(format, "S16LE") != 0 || !gst_structure_get_int(pStructure, "rate", &rate) || !gst_structure_get_int(pStructure, "channels", &channels) || rate != AHEADFORM_AUDIO_SAMPLE_RATE_HZ || channels != 1)
	{
		printf("%s() unsupported MIC sample format.\r\n", __FUNCTION__);
		ret = ERR_MEDIA_FORMAT_UNSUPPORTED;
	}
	else if (stMap.size == 0 || stMap.size % 2 != 0 || GST_BUFFER_FLAG_IS_SET(pBuffer, GST_BUFFER_FLAG_CORRUPTED) || GST_BUFFER_FLAG_IS_SET(pBuffer, GST_BUFFER_FLAG_GAP))
	{
		pstCapture->pcmByteCnt = 0;
		pstCapture->expectedPts_ns = GST_CLOCK_TIME_NONE;
		pstCapture->isDiscontinuous = 1;
	}
	else
	{
		pts_ns = GST_BUFFER_PTS(pBuffer);
		ptsDiff_ns = GST_CLOCK_TIME_IS_VALID(pts_ns) && GST_CLOCK_TIME_IS_VALID(pstCapture->expectedPts_ns) ? GST_CLOCK_DIFF(pstCapture->expectedPts_ns, pts_ns) : 0;
		//Jitter-buffer clock recovery can skew adjacent PTS; a sub-half-frame correction alone is not missing PCM.
		if (GST_BUFFER_FLAG_IS_SET(pBuffer, GST_BUFFER_FLAG_DISCONT) || ptsDiff_ns > AHEADFORM_AUDIO_FRAME_DURATION_NS / 2 || ptsDiff_ns < -(AHEADFORM_AUDIO_FRAME_DURATION_NS / 2))
		{
			pstCapture->pcmByteCnt = 0;
			pstCapture->isDiscontinuous = 1;
		}
		receiveTime_us = GetTimeUsCnt_UpToNow();
		for (offset = 0; offset < stMap.size; offset += copyLen)
		{
			if (pstCapture->pcmByteCnt == 0)
			{
				pstCapture->stFrame.pts_ns = GST_CLOCK_TIME_IS_VALID(pts_ns) ? pts_ns + gst_util_uint64_scale(offset, GST_SECOND, AHEADFORM_AUDIO_SAMPLE_RATE_HZ * 2) : ULLONG_MAX;
				pstCapture->stFrame.ReceiveTime_us = receiveTime_us;
				pstCapture->stFrame.isDiscontinuous = pstCapture->isDiscontinuous;
			}
			copyLen = AHEADFORM_AUDIO_PCM_FRAME_SIZE - pstCapture->pcmByteCnt;
			if (copyLen > stMap.size - offset)
				copyLen = stMap.size - offset;

			memcpy(pstCapture->stFrame.pcmData + pstCapture->pcmByteCnt, stMap.data + offset, copyLen);
			pstCapture->pcmByteCnt += copyLen;
			if (pstCapture->pcmByteCnt == AHEADFORM_AUDIO_PCM_FRAME_SIZE)
			{
				if (SPMCRingFifo_Writer_PushElemAndGetAddr(&pstCapture->stFifo, &pstCapture->stFrame) == NULL)
				{
					ret = ERRCODE_COMM_RING_OP_FAIL;
					break;
				}
				pstCapture->stFrame.frameSeq++;
				pstCapture->pcmByteCnt = 0;
				pstCapture->isDiscontinuous = 0;
				if ( (ret = NotifyMicReaders(pstCapture)) != RET_SUCCESS )
					break;
			}
		}
		pstCapture->expectedPts_ns = GST_CLOCK_TIME_IS_VALID(pts_ns) ? pts_ns + gst_util_uint64_scale(stMap.size, GST_SECOND, AHEADFORM_AUDIO_SAMPLE_RATE_HZ * 2) : GST_CLOCK_TIME_NONE;
	}
	gst_buffer_unmap(pBuffer, &stMap);
	gst_sample_unref(pSample);
	if (ret != RET_SUCCESS)
	{
		__atomic_store_n(&pstCapture->lastError, ret, __ATOMIC_SEQ_CST);
		NotifyMicReaders(pstCapture);
		return GST_FLOW_ERROR;
	}

	return GST_FLOW_OK;
}

int Audio_StartMicCapture(AHEADFORM_AUDIO_CTRL *pstAudioCtrl)
{
	GstElement *pSource = NULL, *pSink = NULL, *pJitter = NULL;
	GstBus *pBus = NULL;
	GstCaps *pCaps = NULL;
	GError *pError = NULL;
	GstAppSinkCallbacks stCallbacks = {0};
	int cleanupRet, ret = RET_SUCCESS;

	if (pstAudioCtrl == NULL)
		return ERR_ARGV_1;

	if (pstAudioCtrl->stMicCapture.pPipeline != NULL)
		return Audio_GetMicCaptureError(pstAudioCtrl);

	if (!gst_init_check(NULL, NULL, &pError))
	{
		printf("%s() call gst_init_check() failed: %s\r\n", __FUNCTION__, pError != NULL ? pError->message : "no GStreamer error detail");
		g_clear_error(&pError);
		return ERR_INIT_FAIL;
	}
	if (pstAudioCtrl->stMicCapture.pstElemAry == NULL)
	{
		if ( (pstAudioCtrl->stMicCapture.pstElemAry = (AHEADFORM_MIC_FRAME*)calloc(SPMC_RING_FIFO_MIN_ELEM_CNT, sizeof(AHEADFORM_MIC_FRAME))) == NULL )
			return ERR_MEMORY_ALLOC;

		if ( (ret = SPMCRingFifo_Init(&pstAudioCtrl->stMicCapture.stFifo, pstAudioCtrl->stMicCapture.pstElemAry, sizeof(AHEADFORM_MIC_FRAME), SPMC_RING_FIFO_MIN_ELEM_CNT)) != RET_SUCCESS )
		{
			free(pstAudioCtrl->stMicCapture.pstElemAry);
			pstAudioCtrl->stMicCapture.pstElemAry = NULL;
			return ret;
		}
	}
	pstAudioCtrl->stMicCapture.pcmByteCnt = 0;
	pstAudioCtrl->stMicCapture.expectedPts_ns = GST_CLOCK_TIME_NONE;
	__atomic_store_n(&pstAudioCtrl->stMicCapture.lastError, RET_SUCCESS, __ATOMIC_SEQ_CST);
	pstAudioCtrl->stMicCapture.isDiscontinuous = 1;
	pstAudioCtrl->stMicCapture.pPipeline = gst_parse_launch("udpsrc name=source ! rtpjitterbuffer name=jitter drop-on-latency=true do-lost=true ! rtpL16depay ! audioconvert "
		"! audio/x-raw,format=S16LE,rate=16000,channels=1,layout=interleaved ! appsink name=mic sync=false max-buffers=1 drop=false wait-on-eos=false enable-last-sample=false", &pError);
	if (pstAudioCtrl->stMicCapture.pPipeline == NULL || pError != NULL)
	{
		printf("%s() call gst_parse_launch(MIC capture) failed: %s\r\n", __FUNCTION__, pError != NULL ? pError->message : "no MIC pipeline");
		g_clear_error(&pError);
		ret = ERR_INIT_FAIL;
	}
	else
	{
		pSource = gst_bin_get_by_name(GST_BIN(pstAudioCtrl->stMicCapture.pPipeline), "source");
		pSink = gst_bin_get_by_name(GST_BIN(pstAudioCtrl->stMicCapture.pPipeline), "mic");
		pJitter = gst_bin_get_by_name(GST_BIN(pstAudioCtrl->stMicCapture.pPipeline), "jitter");
		pBus = gst_element_get_bus(pstAudioCtrl->stMicCapture.pPipeline);
		if (pSource == NULL || pSink == NULL || pJitter == NULL || pBus == NULL)
		{
			printf("%s() cannot get MIC source, sink, jitter buffer or bus.\r\n", __FUNCTION__);
			ret = ERR_INIT_FAIL;
		}
		else
		{
			pCaps = gst_caps_new_simple("application/x-rtp", "media", G_TYPE_STRING, "audio", "encoding-name", G_TYPE_STRING, "L16", "clock-rate", G_TYPE_INT, AHEADFORM_AUDIO_SAMPLE_RATE_HZ,
				"channels", G_TYPE_INT, AHEADFORM_AUDIO_CHANNEL_CNT, "payload", G_TYPE_INT, (int)pstAudioCtrl->stCfg.micPayloadType, NULL);
			g_object_set(pSource, "address", pstAudioCtrl->stCfg.multicastAddress, "port", (int)pstAudioCtrl->stCfg.micPort, "multicast-iface", pstAudioCtrl->strNetworkInterface, "auto-multicast", TRUE, "reuse", TRUE, NULL);
			g_object_set(pSource, "caps", pCaps, "mtu", pstAudioCtrl->stCfg.rtpMtu, NULL);
			g_object_set(pJitter, "latency", pstAudioCtrl->stCfg.jitterBufferLatencyMax_ms, NULL);
			gst_caps_unref(pCaps);
			stCallbacks.new_sample = CallBack_RxParseMicNewSampleToPcmFrameFifo;
			gst_app_sink_set_callbacks(GST_APP_SINK(pSink), &stCallbacks, &pstAudioCtrl->stMicCapture, NULL);
			gst_bus_set_sync_handler(pBus, CallBack_OnMicCaptureBusMessageReceived, &pstAudioCtrl->stMicCapture, NULL);
			if (gst_element_set_state(pstAudioCtrl->stMicCapture.pPipeline, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE)
			{
				printf("%s() call gst_element_set_state(MIC, PLAYING) failed.\r\n", __FUNCTION__);
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
		ret = Audio_GetMicCaptureError(pstAudioCtrl);

	if (ret != RET_SUCCESS)
	{
		__atomic_store_n(&pstAudioCtrl->stMicCapture.lastError, ret, __ATOMIC_SEQ_CST);
		if ( (cleanupRet = Audio_StopMicCapture(pstAudioCtrl)) != RET_SUCCESS )
			return cleanupRet;
	}

	return ret;
}

int Audio_StopMicCapture(AHEADFORM_AUDIO_CTRL *pstAudioCtrl)
{
	GstBus *pBus = NULL;

	if (pstAudioCtrl == NULL)
		return ERR_ARGV_1;

	if (pstAudioCtrl->stMicCapture.pPipeline == NULL)
		return RET_SUCCESS;

	if (gst_element_set_state(pstAudioCtrl->stMicCapture.pPipeline, GST_STATE_NULL) == GST_STATE_CHANGE_FAILURE)
	{
		printf("%s() call gst_element_set_state(MIC, NULL) failed.\r\n", __FUNCTION__);
		__atomic_store_n(&pstAudioCtrl->stMicCapture.lastError, ERRCODE_STATE_TRANSITION_INVALID, __ATOMIC_SEQ_CST);
		return ERR_STATE_TRANSITION_INVALID;
	}
	pBus = gst_element_get_bus(pstAudioCtrl->stMicCapture.pPipeline);
	if (pBus != NULL)
	{
		gst_bus_set_sync_handler(pBus, NULL, NULL, NULL);
		gst_object_unref(pBus);
	}
	gst_object_unref(pstAudioCtrl->stMicCapture.pPipeline);
	pstAudioCtrl->stMicCapture.pPipeline = NULL;
	pstAudioCtrl->stMicCapture.isDiscontinuous = 1;
	pstAudioCtrl->stMicCapture.pcmByteCnt = 0;
	return RET_SUCCESS;
}

int Audio_GetMicCaptureError(const AHEADFORM_AUDIO_CTRL *pstAudioCtrl)
{
	if (pstAudioCtrl == NULL)
		return ERR_ARGV_1;

	return __atomic_load_n(&pstAudioCtrl->stMicCapture.lastError, __ATOMIC_SEQ_CST);
}

AHEADFORM_MIC_READER* Audio_RegisterMicReader(AHEADFORM_AUDIO_CTRL *pstAudioCtrl)
{
	AHEADFORM_MIC_READER *pstReader = NULL;
	int ret;

	if (pstAudioCtrl == NULL)
	{
		PRINT_FUNCTION_ERRINFO(ERR_ARGV_1, __FUNCTION__);
		return NULL;
	}

	if (pstAudioCtrl->stMicCapture.pstElemAry == NULL || Audio_GetMicCaptureError(pstAudioCtrl) != RET_SUCCESS)
	{
		PRINT_FUNCTION_ERRINFO(ERR_SDK_NOT_READY, __FUNCTION__);
		return NULL;
	}

	if ( (pstReader = (AHEADFORM_MIC_READER*)calloc(1, sizeof(*pstReader))) == NULL )
	{
		PRINT_FUNCTION_ERRINFO(ERR_MEMORY_ALLOC, __FUNCTION__);
		return NULL;
	}

	if ( (pstReader->rxDataUpdateEventfd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC)) <= 0 )
	{
		printf("%s() call eventfd(MIC Reader) failed: %s\r\n", __FUNCTION__, strerror(errno));
		free(pstReader);
		PRINT_FUNCTION_ERRINFO(ERR_EVENTFD_CREATE_FAILED, __FUNCTION__);
		return NULL;
	}

	if ( (ret = pthread_mutex_lock(&pstAudioCtrl->stMicCapture.readerListMutex)) != RET_SUCCESS )
	{
		printf("%s() call pthread_mutex_lock(MIC reader list) failed: %s\r\n", __FUNCTION__, strerror(ret));
		close(pstReader->rxDataUpdateEventfd);
		free(pstReader);
		PRINT_FUNCTION_ERRINFO(ERR_MUTEX_OP_FAILED, __FUNCTION__);
		return NULL;
	}
	ret = SPMCRingFifo_Reader_Init(&pstReader->stFifoReader, &pstAudioCtrl->stMicCapture.stFifo);
	if (ret == RET_SUCCESS)
	{
		pstReader->pNext = pstAudioCtrl->stMicCapture.pstReaders;
		pstAudioCtrl->stMicCapture.pstReaders = pstReader;
	}
	pthread_mutex_unlock(&pstAudioCtrl->stMicCapture.readerListMutex);
	if (ret != RET_SUCCESS)
	{
		close(pstReader->rxDataUpdateEventfd);
		free(pstReader);
		PRINT_FUNCTION_ERRINFO(ret, __FUNCTION__);
		return NULL;
	}

	return pstReader;
}

int Audio_UnregisterMicReader(AHEADFORM_AUDIO_CTRL *pstAudioCtrl, AHEADFORM_MIC_READER *pstReader)
{
	AHEADFORM_MIC_READER **ppReader = NULL;
	int ret;

	if (pstAudioCtrl == NULL)
		return ERR_ARGV_1;

	if (pstReader == NULL)
		return ERR_ARGV_2;

	if ( (ret = pthread_mutex_lock(&pstAudioCtrl->stMicCapture.readerListMutex)) != RET_SUCCESS )
	{
		printf("%s() call pthread_mutex_lock(MIC reader list) failed: %s\r\n", __FUNCTION__, strerror(ret));
		return ERR_MUTEX_OP_FAILED;
	}
	ppReader = &pstAudioCtrl->stMicCapture.pstReaders;
	while (*ppReader != NULL && *ppReader != pstReader)
		ppReader = &(*ppReader)->pNext;

	if (*ppReader == NULL)
	{
		pthread_mutex_unlock(&pstAudioCtrl->stMicCapture.readerListMutex);
		return ERR_SUBSCRIPTION_NOT_FOUND;
	}

	if (pstReader->rxDataUpdateEventfd > 0 && close(pstReader->rxDataUpdateEventfd) < 0)
	{
		printf("%s() call close(MIC fd = %d) failed: %s\r\n", __FUNCTION__, pstReader->rxDataUpdateEventfd, strerror(errno));
		pstReader->rxDataUpdateEventfd = 0;
		pthread_mutex_unlock(&pstAudioCtrl->stMicCapture.readerListMutex);
		return ERR_EVENTFD_IO_FAILED;
	}
	*ppReader = pstReader->pNext;
	pthread_mutex_unlock(&pstAudioCtrl->stMicCapture.readerListMutex);
	free(pstReader);
	return RET_SUCCESS;
}

int Audio_MicReader_PopFrame(AHEADFORM_MIC_READER *pstReader, AHEADFORM_MIC_FRAME *pstRetFrame)
{
	unsigned long long missedFrameCnt;
	int ret;

	if (pstReader == NULL)
		return ERR_ARGV_1;

	if (pstRetFrame == NULL)
		return ERR_ARGV_2;

	if ( (ret = SPMCRingFifo_Reader_PopElem(&pstReader->stFifoReader, pstRetFrame)) != 1 )
		return ret;

	missedFrameCnt = SPMCRingFifo_Reader_GetMissedElemCnt(&pstReader->stFifoReader);
	if (missedFrameCnt != pstReader->lastMissedFrameCnt)
	{
		pstRetFrame->isDiscontinuous = 1;
		pstReader->lastMissedFrameCnt = missedFrameCnt;
	}

	return 1;
}

int Audio_MicReader_DropOldFrames(AHEADFORM_MIC_READER *pstReader, unsigned int retainedFreshFrameCnt)
{
	if (pstReader == NULL)
		return ERR_ARGV_1;

	return SPMCRingFifo_Reader_DropOldElems(&pstReader->stFifoReader, retainedFreshFrameCnt);
}

unsigned int Audio_MicReader_GetReadableFrameCnt(AHEADFORM_MIC_READER *pstReader)
{
	if (pstReader == NULL)
	{
		PRINT_FUNCTION_ERRINFO(ERR_ARGV_1, __FUNCTION__);
		return 0;
	}

	return SPMCRingFifo_Reader_GetReadableElemCnt(&pstReader->stFifoReader);
}

unsigned long long Audio_MicReader_GetMissedFrameCnt(AHEADFORM_MIC_READER *pstReader)
{
	if (pstReader == NULL)
	{
		PRINT_FUNCTION_ERRINFO(ERR_ARGV_1, __FUNCTION__);
		return 0;
	}

	return SPMCRingFifo_Reader_GetMissedElemCnt(&pstReader->stFifoReader);
}

static GstBusSyncReply CallBack_OnSdkPlayAudioBusMessageReceived(GstBus *pBus, GstMessage *pMessage, gpointer pUserData)
{
	AHEADFORM_SDK_PLAY_AUDIO *pstPlayback = (AHEADFORM_SDK_PLAY_AUDIO*)pUserData;
	GError *pError = NULL;
	gchar *pDebug = NULL;

	if (GST_MESSAGE_TYPE(pMessage) == GST_MESSAGE_ERROR)
	{
		gst_message_parse_error(pMessage, &pError, &pDebug);
		printf("%s() SDK audio pipeline error: %s\r\n", __FUNCTION__, pError != NULL ? pError->message : "no GStreamer error detail");
		g_clear_error(&pError);
		g_free(pDebug);
		__atomic_store_n(&pstPlayback->lastError, ERRCODE_SOCKET_IO, __ATOMIC_SEQ_CST);
	}
	else if (GST_MESSAGE_TYPE(pMessage) == GST_MESSAGE_EOS)
	{
		printf("%s() SDK audio pipeline reached unexpected EOS.\r\n", __FUNCTION__);
		__atomic_store_n(&pstPlayback->lastError, ERRCODE_SOCKET_IO, __ATOMIC_SEQ_CST);
	}
	gst_message_unref(pMessage);
	return GST_BUS_DROP;
}

int Audio_StartSdkPlayAudio(AHEADFORM_AUDIO_CTRL *pstAudioCtrl)
{
	GstElement *pSink = NULL, *pPay = NULL;
	GstBus *pBus = NULL;
	GstCaps *pCaps = NULL;
	GError *pError = NULL;
	struct ifaddrs *pstInterfaces = NULL, *pstInterface = NULL;
	char sourceIPv4[AHEADFORM_IPV4_ADDR_LEN_MAX] = {0};
	int cleanupRet, ret = RET_SUCCESS;

	if (pstAudioCtrl == NULL)
		return ERR_ARGV_1;

	if (pstAudioCtrl->stSdkPlayAudio.pPipeline != NULL)
		return Audio_GetSdkPlayAudioError(pstAudioCtrl);

	if (strcmp(pstAudioCtrl->stCfg.strRobotIPv4, "0.0.0.0") == 0)
		return ERR_CONFIG_VALUE_INVALID;

	if (getifaddrs(&pstInterfaces) < 0)
	{
		printf("%s() call getifaddrs(SDK audio interface = %s) failed: %s\r\n", __FUNCTION__, pstAudioCtrl->strNetworkInterface, strerror(errno));
		return ERR_NETWORK_INTERFACE_INVALID;
	}

	for (pstInterface = pstInterfaces; pstInterface != NULL; pstInterface = pstInterface->ifa_next)
	{
		if (pstInterface->ifa_addr != NULL && pstInterface->ifa_addr->sa_family == AF_INET && strcmp(pstInterface->ifa_name, pstAudioCtrl->strNetworkInterface) == 0)
		{
			if (inet_ntop(AF_INET, &((struct sockaddr_in*)pstInterface->ifa_addr)->sin_addr, sourceIPv4, sizeof(sourceIPv4)) == NULL)
			{
				printf("%s() call inet_ntop(SDK audio interface = %s) failed: %s\r\n", __FUNCTION__, pstAudioCtrl->strNetworkInterface, strerror(errno));
				ret = ERR_NETWORK_INTERFACE_INVALID;
			}
			break;
		}
	}
	freeifaddrs(pstInterfaces);
	if (ret != RET_SUCCESS)
		return ret;

	if (sourceIPv4[0] == 0)
	{
		printf("%s() interface %s has no IPv4 address.\r\n", __FUNCTION__, pstAudioCtrl->strNetworkInterface);
		return ERR_NETWORK_INTERFACE_INVALID;
	}

	if (!gst_init_check(NULL, NULL, &pError))
	{
		printf("%s() call gst_init_check() failed: %s\r\n", __FUNCTION__, pError != NULL ? pError->message : "no GStreamer error detail");
		g_clear_error(&pError);
		return ERR_INIT_FAIL;
	}
	__atomic_store_n(&pstAudioCtrl->stSdkPlayAudio.lastError, RET_SUCCESS, __ATOMIC_SEQ_CST);
	pstAudioCtrl->stSdkPlayAudio.nextPts_ns = GST_CLOCK_TIME_NONE;
	pstAudioCtrl->stSdkPlayAudio.pPipeline = gst_parse_launch("appsrc name=source is-live=true format=time block=false max-bytes=0 max-time=0 "
		"! audioconvert ! audio/x-raw,format=S16BE,rate=16000,channels=1,layout=interleaved ! rtpL16pay name=pay ! udpsink name=sink sync=true async=false", &pError);
	if (pstAudioCtrl->stSdkPlayAudio.pPipeline == NULL || pError != NULL)
	{
		printf("%s() call gst_parse_launch(SDK audio playback) failed: %s\r\n", __FUNCTION__, pError != NULL ? pError->message : "no SDK audio pipeline");
		g_clear_error(&pError);
		ret = ERR_INIT_FAIL;
	}
	else
	{
		pstAudioCtrl->stSdkPlayAudio.pSource = gst_bin_get_by_name(GST_BIN(pstAudioCtrl->stSdkPlayAudio.pPipeline), "source");
		pSink = gst_bin_get_by_name(GST_BIN(pstAudioCtrl->stSdkPlayAudio.pPipeline), "sink");
		pPay = gst_bin_get_by_name(GST_BIN(pstAudioCtrl->stSdkPlayAudio.pPipeline), "pay");
		pBus = gst_element_get_bus(pstAudioCtrl->stSdkPlayAudio.pPipeline);
		if (pstAudioCtrl->stSdkPlayAudio.pSource == NULL || pSink == NULL || pPay == NULL || pBus == NULL)
		{
			printf("%s() cannot get SDK audio source, sink, payloader or bus.\r\n", __FUNCTION__);
			ret = ERR_INIT_FAIL;
		}
		else
		{
			pCaps = gst_caps_new_simple("audio/x-raw", "format", G_TYPE_STRING, "S16LE", "rate", G_TYPE_INT, AHEADFORM_AUDIO_SAMPLE_RATE_HZ, "channels", G_TYPE_INT, AHEADFORM_AUDIO_CHANNEL_CNT, "layout", G_TYPE_STRING, "interleaved", NULL);
			g_object_set(pstAudioCtrl->stSdkPlayAudio.pSource, "caps", pCaps, "max-buffers", (guint64)AHEADFORM_SDK_PLAY_AUDIO_QUEUE_FRAME_CNT_MAX, NULL);
			gst_caps_unref(pCaps);
			g_object_set(pPay, "pt", pstAudioCtrl->stCfg.spkPayloadType, "mtu", pstAudioCtrl->stCfg.rtpMtu, "min-ptime", (gint64)AHEADFORM_AUDIO_FRAME_DURATION_NS, "max-ptime", (gint64)AHEADFORM_AUDIO_FRAME_DURATION_NS, NULL);
			g_object_set(pSink, "host", pstAudioCtrl->stCfg.strRobotIPv4, "port", (int)pstAudioCtrl->stCfg.spkPort, "bind-address", sourceIPv4, NULL);
			gst_bus_set_sync_handler(pBus, CallBack_OnSdkPlayAudioBusMessageReceived, &pstAudioCtrl->stSdkPlayAudio, NULL);
			if (gst_element_set_state(pstAudioCtrl->stSdkPlayAudio.pPipeline, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE)
			{
				printf("%s() call gst_element_set_state(SDK audio, PLAYING) failed.\r\n", __FUNCTION__);
				ret = ERR_INIT_FAIL;
			}
		}
	}

	if (pSink != NULL)
		gst_object_unref(pSink);

	if (pPay != NULL)
		gst_object_unref(pPay);

	if (pBus != NULL)
		gst_object_unref(pBus);

	if (ret == RET_SUCCESS)
		ret = Audio_GetSdkPlayAudioError(pstAudioCtrl);

	if (ret != RET_SUCCESS)
	{
		__atomic_store_n(&pstAudioCtrl->stSdkPlayAudio.lastError, ret, __ATOMIC_SEQ_CST);
		if ( (cleanupRet = Audio_StopSdkPlayAudio(pstAudioCtrl)) != RET_SUCCESS )
			return cleanupRet;
	}

	return ret;
}

int Audio_StopSdkPlayAudio(AHEADFORM_AUDIO_CTRL *pstAudioCtrl)
{
	GstBus *pBus = NULL;
	int ret;

	if (pstAudioCtrl == NULL)
		return ERR_ARGV_1;

	if ( (ret = pthread_mutex_lock(&pstAudioCtrl->stSdkPlayAudio.writeMutex)) != RET_SUCCESS )
	{
		printf("%s() call pthread_mutex_lock(SDK audio write) failed: %s\r\n", __FUNCTION__, strerror(ret));
		return ERR_MUTEX_OP_FAILED;
	}

	if (pstAudioCtrl->stSdkPlayAudio.pPipeline != NULL)
	{
		if (gst_element_set_state(pstAudioCtrl->stSdkPlayAudio.pPipeline, GST_STATE_NULL) == GST_STATE_CHANGE_FAILURE)
		{
			printf("%s() call gst_element_set_state(SDK audio, NULL) failed.\r\n", __FUNCTION__);
			ret = ERR_STATE_TRANSITION_INVALID;
			__atomic_store_n(&pstAudioCtrl->stSdkPlayAudio.lastError, ret, __ATOMIC_SEQ_CST);
		}
		else
		{
			pBus = gst_element_get_bus(pstAudioCtrl->stSdkPlayAudio.pPipeline);
			if (pBus != NULL)
			{
				gst_bus_set_sync_handler(pBus, NULL, NULL, NULL);
				gst_object_unref(pBus);
			}

			if (pstAudioCtrl->stSdkPlayAudio.pSource != NULL)
				gst_object_unref(pstAudioCtrl->stSdkPlayAudio.pSource);

			gst_object_unref(pstAudioCtrl->stSdkPlayAudio.pPipeline);
			pstAudioCtrl->stSdkPlayAudio.pSource = NULL;
			pstAudioCtrl->stSdkPlayAudio.pPipeline = NULL;
			pstAudioCtrl->stSdkPlayAudio.nextPts_ns = GST_CLOCK_TIME_NONE;
		}
	}
	pthread_mutex_unlock(&pstAudioCtrl->stSdkPlayAudio.writeMutex);
	return ret;
}

int Audio_GetSdkPlayAudioError(const AHEADFORM_AUDIO_CTRL *pstAudioCtrl)
{
	if (pstAudioCtrl == NULL)
		return ERR_ARGV_1;

	return __atomic_load_n(&pstAudioCtrl->stSdkPlayAudio.lastError, __ATOMIC_SEQ_CST);
}

int Audio_WriteSdkPlayPcm(AHEADFORM_AUDIO_CTRL *pstAudioCtrl, const unsigned char *pPcmData, size_t dataLen)
{
	GstClock *pClock = NULL;
	GstBuffer *pBuffer = NULL;
	GstClockTime runningTime_ns, pts_ns;
	GstFlowReturn flowRet;
	guint64 queuedFrameCnt;
	int ret;

	if (pstAudioCtrl == NULL)
		return ERR_ARGV_1;

	if (pPcmData == NULL)
		return ERR_ARGV_2;

	if (dataLen != AHEADFORM_AUDIO_PCM_FRAME_SIZE)
		return ERR_ARGV_3;

	if ( (ret = pthread_mutex_lock(&pstAudioCtrl->stSdkPlayAudio.writeMutex)) != RET_SUCCESS )
	{
		printf("%s() call pthread_mutex_lock(SDK audio write) failed: %s\r\n", __FUNCTION__, strerror(ret));
		return ERR_MUTEX_OP_FAILED;
	}

	if (pstAudioCtrl->stSdkPlayAudio.pSource == NULL)
	{
		pthread_mutex_unlock(&pstAudioCtrl->stSdkPlayAudio.writeMutex);
		return ERR_MEDIA_CHANNEL_NOT_READY;
	}

	if ( (ret = Audio_GetSdkPlayAudioError(pstAudioCtrl)) != RET_SUCCESS )
	{
		pthread_mutex_unlock(&pstAudioCtrl->stSdkPlayAudio.writeMutex);
		return ret;
	}
	g_object_get(pstAudioCtrl->stSdkPlayAudio.pSource, "current-level-buffers", &queuedFrameCnt, NULL);
	if (queuedFrameCnt >= AHEADFORM_SDK_PLAY_AUDIO_QUEUE_FRAME_CNT_MAX)
	{
		pthread_mutex_unlock(&pstAudioCtrl->stSdkPlayAudio.writeMutex);
		return ERRCODE_RESOURCE_BUSY;
	}
	pClock = gst_element_get_clock(pstAudioCtrl->stSdkPlayAudio.pPipeline);
	if (pClock == NULL)
	{
		pthread_mutex_unlock(&pstAudioCtrl->stSdkPlayAudio.writeMutex);
		return ERR_MEDIA_CHANNEL_NOT_READY;
	}
	runningTime_ns = gst_clock_get_time(pClock) - gst_element_get_base_time(pstAudioCtrl->stSdkPlayAudio.pPipeline);
	gst_object_unref(pClock);
	pts_ns = pstAudioCtrl->stSdkPlayAudio.nextPts_ns;
	if (!GST_CLOCK_TIME_IS_VALID(pts_ns) || pts_ns < runningTime_ns)
		pts_ns = runningTime_ns;

	if ( (pBuffer = gst_buffer_new_allocate(NULL, dataLen, NULL)) == NULL )
	{
		pthread_mutex_unlock(&pstAudioCtrl->stSdkPlayAudio.writeMutex);
		return ERR_MEMORY_ALLOC;
	}
	gst_buffer_fill(pBuffer, 0, pPcmData, dataLen);
	GST_BUFFER_PTS(pBuffer) = pts_ns;
	GST_BUFFER_DURATION(pBuffer) = AHEADFORM_AUDIO_FRAME_DURATION_NS;
	if (pts_ns != pstAudioCtrl->stSdkPlayAudio.nextPts_ns)
		GST_BUFFER_FLAG_SET(pBuffer, GST_BUFFER_FLAG_DISCONT);

	flowRet = gst_app_src_push_buffer(GST_APP_SRC(pstAudioCtrl->stSdkPlayAudio.pSource), pBuffer);
	if (flowRet == GST_FLOW_OK)
	{
		pstAudioCtrl->stSdkPlayAudio.nextPts_ns = pts_ns + AHEADFORM_AUDIO_FRAME_DURATION_NS;
		ret = RET_SUCCESS;
	}
	else
	{
		printf("%s() call gst_app_src_push_buffer(SDK audio) failed: %s\r\n", __FUNCTION__, gst_flow_get_name(flowRet));
		ret = ERR_SOCKET_IO;
		__atomic_store_n(&pstAudioCtrl->stSdkPlayAudio.lastError, ret, __ATOMIC_SEQ_CST);
	}
	pthread_mutex_unlock(&pstAudioCtrl->stSdkPlayAudio.writeMutex);
	return ret;
}
