#include "Media.h"

#include <math.h>
#include <sys/eventfd.h>
#include <sys/select.h>
#include <unistd.h>

static int BuildVolumeRequestIdentity(AHEADFORM_MEDIA_CTRL *pstMediaCtrl, AHEADFORM_VOLUME_ASK *pstRequest);
static int SubmitVolumeRequestAsync(AHEADFORM_MEDIA_CTRL *pstMediaCtrl, unsigned long long MatchReplyTimeout_us, int notifyEventfd, const AHEADFORM_VOLUME_ASK *pstRequest, AHEADFORM_VOLUME_REPLY_SAMPLE *pstReply);


int Media_Init(AHEADFORM_MEDIA_CTRL *pstMediaCtrl, AHEADFORM_TOPICS *pstTopics, const AHEADFORM_MEDIA_CFG *pstMediaCfg)
{
	AHEADFORM_TOPIC_READER_PARA stReaderPara = {0};

	if (pstMediaCtrl == NULL)
		return ERR_ARGV_1;

	if (pstTopics == NULL)
		return ERR_ARGV_2;

	if (pstTopics->stDdsCtrl.ddsParticipant <= 0)
		return ERR_ARGV_2;

	if (pstMediaCfg == NULL)
		return ERR_ARGV_3;

	memset(pstMediaCtrl, 0, sizeof(*pstMediaCtrl));
	pstMediaCtrl->stCfg = *pstMediaCfg;
	pstMediaCtrl->pstTopics = pstTopics;
	if ( (pstMediaCtrl->pstVolumeReplyReader = Media_RegisterVolumeReader(pstMediaCtrl, &stReaderPara)) == NULL )
		return ERRCODE_INIT_FAIL;

	if ( (pstMediaCtrl->pstVolumeAskWriter = AheadFormTopics_RegisterWriter(pstTopics, AHEADFORM_TOPIC_ID_VOLUME_ASK)) == NULL )
		return ERRCODE_INIT_FAIL;

	if ( (pstMediaCtrl->pstAudioDeviceAskWriter = AheadFormTopics_RegisterWriter(pstTopics, AHEADFORM_TOPIC_ID_AUDIO_DEVICE_ASK)) == NULL )
		return ERRCODE_INIT_FAIL;

	if ( (pstMediaCtrl->pstAudioDeviceReplyReader = AheadFormTopics_RegisterReader(pstTopics, AHEADFORM_TOPIC_ID_AUDIO_DEVICE_REPLY, &stReaderPara)) == NULL )
		return ERRCODE_INIT_FAIL;

	if ( (pstMediaCtrl->pstChannelAskWriter = AheadFormTopics_RegisterWriter(pstTopics, AHEADFORM_TOPIC_ID_MEDIA_CHANNEL_ASK)) == NULL )
		return ERRCODE_INIT_FAIL;

	if ( (pstMediaCtrl->pstChannelReplyReader = AheadFormTopics_RegisterReader(pstTopics, AHEADFORM_TOPIC_ID_MEDIA_CHANNEL_REPLY, &stReaderPara)) == NULL )
		return ERRCODE_INIT_FAIL;

	return Media_InitStreams(pstMediaCtrl);
}

int Media_Deinit(AHEADFORM_MEDIA_CTRL *pstMediaCtrl)
{
	int ret;

	if (pstMediaCtrl == NULL)
		return ERR_ARGV_1;

	if ( (ret = Media_DeinitStreams(pstMediaCtrl)) != RET_SUCCESS )
		return ret;

	if (pstMediaCtrl->pstChannelReplyReader != NULL)
	{
		if ( (ret = AheadFormTopics_UnregisterReader(pstMediaCtrl->pstTopics, pstMediaCtrl->pstChannelReplyReader)) != RET_SUCCESS )
			return ret;

		pstMediaCtrl->pstChannelReplyReader = NULL;
	}

	if (pstMediaCtrl->pstChannelAskWriter != NULL)
	{
		if ( (ret = AheadFormTopics_UnregisterWriter(pstMediaCtrl->pstTopics, pstMediaCtrl->pstChannelAskWriter)) != RET_SUCCESS )
			return ret;

		pstMediaCtrl->pstChannelAskWriter = NULL;
	}

	if (pstMediaCtrl->pstAudioDeviceReplyReader != NULL)
	{
		if ( (ret = AheadFormTopics_UnregisterReader(pstMediaCtrl->pstTopics, pstMediaCtrl->pstAudioDeviceReplyReader)) != RET_SUCCESS )
			return ret;

		pstMediaCtrl->pstAudioDeviceReplyReader = NULL;
	}

	if (pstMediaCtrl->pstAudioDeviceAskWriter != NULL)
	{
		if ( (ret = AheadFormTopics_UnregisterWriter(pstMediaCtrl->pstTopics, pstMediaCtrl->pstAudioDeviceAskWriter)) != RET_SUCCESS )
			return ret;

		pstMediaCtrl->pstAudioDeviceAskWriter = NULL;
	}

	if (pstMediaCtrl->pstVolumeReplyReader != NULL)
	{
		if ( (ret = AheadFormTopics_UnregisterReader(pstMediaCtrl->pstTopics, pstMediaCtrl->pstVolumeReplyReader)) != RET_SUCCESS )
			return ret;

		pstMediaCtrl->pstVolumeReplyReader = NULL;
	}

	if (pstMediaCtrl->pstVolumeAskWriter != NULL)
	{
		if ( (ret = AheadFormTopics_UnregisterWriter(pstMediaCtrl->pstTopics, pstMediaCtrl->pstVolumeAskWriter)) != RET_SUCCESS )
			return ret;

		pstMediaCtrl->pstVolumeAskWriter = NULL;
	}
	memset(pstMediaCtrl, 0, sizeof(*pstMediaCtrl));
	return RET_SUCCESS;
}

AHEADFORM_TOPIC_READER* Media_RegisterVolumeReader(AHEADFORM_MEDIA_CTRL *pstMediaCtrl, const AHEADFORM_TOPIC_READER_PARA *pstReaderPara)
{
	if (pstMediaCtrl == NULL)
	{
		PRINT_FUNCTION_ERRINFO(ERR_ARGV_1, __FUNCTION__);
		return NULL;
	}

	if (pstReaderPara == NULL)
	{
		PRINT_FUNCTION_ERRINFO(ERR_ARGV_2, __FUNCTION__);
		return NULL;
	}

	if (pstMediaCtrl->pstTopics == NULL)
	{
		PRINT_FUNCTION_ERRINFO(ERR_SDK_NOT_READY, __FUNCTION__);
		return NULL;
	}

	return AheadFormTopics_RegisterReader(pstMediaCtrl->pstTopics, AHEADFORM_TOPIC_ID_VOLUME_REPLY, pstReaderPara);
}

int Media_UnregisterVolumeReader(AHEADFORM_MEDIA_CTRL *pstMediaCtrl, AHEADFORM_TOPIC_READER *pstReader)
{
	if (pstMediaCtrl == NULL)
		return ERR_ARGV_1;

	if (pstReader == NULL)
		return ERR_ARGV_2;

	if (pstMediaCtrl->pstTopics == NULL)
		return ERR_SDK_NOT_READY;

	if (pstReader->topicId != AHEADFORM_TOPIC_ID_VOLUME_REPLY || pstReader == pstMediaCtrl->pstVolumeReplyReader)
		return ERR_ARGV_2;

	return AheadFormTopics_UnregisterReader(pstMediaCtrl->pstTopics, pstReader);
}

int Media_BuildQueryVolumeRequest(AHEADFORM_MEDIA_CTRL *pstMediaCtrl, AHEADFORM_VOLUME_ASK *pstRequest)
{
	int ret;

	if ( (ret = BuildVolumeRequestIdentity(pstMediaCtrl, pstRequest)) != RET_SUCCESS )
		return ret;

	pstRequest->operation = AHEADFORM_VOLUME_OPERATION_GET;
	return RET_SUCCESS;
}

int Media_BuildSetVolumeRequest(AHEADFORM_MEDIA_CTRL *pstMediaCtrl, const AHEADFORM_VOLUME_SET_PARA *pstSetPara, AHEADFORM_VOLUME_ASK *pstRequest)
{
	int ret;

	if (pstMediaCtrl == NULL)
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

	if ( (ret = BuildVolumeRequestIdentity(pstMediaCtrl, pstRequest)) != RET_SUCCESS )
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

int Media_SendVolumeRequest(AHEADFORM_MEDIA_CTRL *pstMediaCtrl, const AHEADFORM_VOLUME_ASK *pstAsk)
{
	if (pstMediaCtrl == NULL)
		return ERR_ARGV_1;

	if (pstAsk == NULL)
		return ERR_ARGV_2;

	if (pstMediaCtrl->pstTopics == NULL || pstMediaCtrl->pstVolumeAskWriter == NULL)
		return ERR_SDK_NOT_READY;

	return TopicWriter_Write(pstMediaCtrl->pstVolumeAskWriter, pstAsk);
}

int Media_QueryVolumeAsync(AHEADFORM_MEDIA_CTRL *pstMediaCtrl, unsigned long long MatchReplyTimeout_us, int notifyEventfd, AHEADFORM_VOLUME_REPLY_SAMPLE *pstReply)
{
	AHEADFORM_VOLUME_ASK stRequest;
	int ret;

	if (pstMediaCtrl == NULL)
		return ERR_ARGV_1;

	if (MatchReplyTimeout_us == 0)
		return ERR_REQUEST_TIMEOUT;

	if (notifyEventfd <= 0)
		return ERR_ARGV_3;

	if (pstReply == NULL)
		return ERR_ARGV_4;

	if ( (ret = Media_BuildQueryVolumeRequest(pstMediaCtrl, &stRequest)) != RET_SUCCESS )
		return ret;

	return SubmitVolumeRequestAsync(pstMediaCtrl, MatchReplyTimeout_us, notifyEventfd, &stRequest, pstReply);
}

int Media_SetVolumeAsync(AHEADFORM_MEDIA_CTRL *pstMediaCtrl, const AHEADFORM_VOLUME_SET_PARA *pstSetPara, unsigned long long MatchReplyTimeout_us, int notifyEventfd, AHEADFORM_VOLUME_REPLY_SAMPLE *pstReply)
{
	AHEADFORM_VOLUME_ASK stRequest;
	int ret;

	if (pstMediaCtrl == NULL)
		return ERR_ARGV_1;

	if (pstSetPara == NULL)
		return ERR_ARGV_2;

	if (MatchReplyTimeout_us == 0)
		return ERR_REQUEST_TIMEOUT;

	if (notifyEventfd <= 0)
		return ERR_ARGV_4;

	if (pstReply == NULL)
		return ERR_ARGV_5;

	if ( (ret = Media_BuildSetVolumeRequest(pstMediaCtrl, pstSetPara, &stRequest)) != RET_SUCCESS )
		return ret;

	return SubmitVolumeRequestAsync(pstMediaCtrl, MatchReplyTimeout_us, notifyEventfd, &stRequest, pstReply);
}

int Media_CheckIsVolumeAsyncRequestReplied(AHEADFORM_MEDIA_CTRL *pstMediaCtrl, int notifyEventfd)
{
	if (pstMediaCtrl == NULL)
		return ERR_ARGV_1;

	if (pstMediaCtrl->pstTopics == NULL || pstMediaCtrl->pstVolumeReplyReader == NULL)
		return ERR_SDK_NOT_READY;

	return TopicReader_CheckIsAsyncRequestReplied(pstMediaCtrl->pstVolumeReplyReader, notifyEventfd);
}

int Media_CancelVolumeAsyncRequest(AHEADFORM_MEDIA_CTRL *pstMediaCtrl, int notifyEventfd)
{
	if (pstMediaCtrl == NULL)
		return ERR_ARGV_1;

	if (pstMediaCtrl->pstTopics == NULL || pstMediaCtrl->pstVolumeReplyReader == NULL)
		return ERR_SDK_NOT_READY;

	return TopicReader_CancelAsyncRequest(pstMediaCtrl->pstVolumeReplyReader, notifyEventfd);
}

int Media_MatchVolumeReplyWithReq(const void *pReplyElem, const void *pRequest)
{
	const AHEADFORM_VOLUME_REPLY_SAMPLE *pstReply = (const AHEADFORM_VOLUME_REPLY_SAMPLE*)pReplyElem;
	AHEADFORM_VOLUME_ASK stRequest;

	if (pstReply == NULL || pRequest == NULL)
		return 0;

	memcpy(&stRequest, pRequest, sizeof(stRequest));
	return pstReply->askId == stRequest.askId && pstReply->operation == stRequest.operation && strcmp(pstReply->sourceName, stRequest.sourceName) == 0;
}

int Media_GetVolumeFromReply(const AHEADFORM_VOLUME_REPLY_SAMPLE *pstReply, AHEADFORM_VOLUME *pstVolume)
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

int Media_QueryVolumeSync(AHEADFORM_MEDIA_CTRL *pstMediaCtrl, AHEADFORM_VOLUME *pstVolume, int timeout_ms)
{
	AHEADFORM_VOLUME_REPLY_SAMPLE stReply;
	eventfd_t eventCnt;
	struct timeval stTimeout;
	fd_set rdFdSet;
	int notifyEventfd, cleanupRet, ret;

	if (pstMediaCtrl == NULL)
		return ERR_ARGV_1;

	if (pstVolume == NULL)
		return ERR_ARGV_2;

	if (timeout_ms < 0)
		return ERR_ARGV_3;

	if (timeout_ms == 0)
		return ERR_REQUEST_TIMEOUT;

	if ( (notifyEventfd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC)) <= 0 )
	{
		printf("%s() call eventfd(volume completion) failed: %s\r\n", __FUNCTION__, strerror(errno));
		return ERR_EVENTFD_CREATE_FAILED;
	}

	if ( (ret = Media_QueryVolumeAsync(pstMediaCtrl, (unsigned long long)timeout_ms * 1000, notifyEventfd, &stReply)) != RET_SUCCESS )
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
	while ( (ret = Media_CheckIsVolumeAsyncRequestReplied(pstMediaCtrl, notifyEventfd)) == 0 )
	{
		FD_ZERO(&rdFdSet);
		FD_SET(notifyEventfd, &rdFdSet);
		if ( (ret = select(notifyEventfd + 1, &rdFdSet, NULL, NULL, &stTimeout)) < 0 )
		{
			printf("%s() call select(notifyEventfd = %d) failed: %s\r\n", __FUNCTION__, notifyEventfd, strerror(errno));
			ret = ERR_SELECT;
			break;
		}
		else if (ret == 0)
		{
			ret = ERRCODE_REQUEST_TIMEOUT;
			break;
		}
	}

	if (ret != 1)
	{
		cleanupRet = Media_CancelVolumeAsyncRequest(pstMediaCtrl, notifyEventfd);
		if (cleanupRet != RET_SUCCESS)
		{
			ret = cleanupRet;
		}
		else if (ret == ERRCODE_REQUEST_TIMEOUT)
		{
			if (eventfd_read(notifyEventfd, &eventCnt) == 0)
			{
				ret = 1;
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

	if (ret == ERRCODE_REQUEST_TIMEOUT)
		return ERR_REQUEST_TIMEOUT;

	if (ret != 1)
		return ret;

	return Media_GetVolumeFromReply(&stReply, pstVolume);
}

static int BuildVolumeRequestIdentity(AHEADFORM_MEDIA_CTRL *pstMediaCtrl, AHEADFORM_VOLUME_ASK *pstRequest)
{
	dds_guid_t stGuid;
	dds_return_t ddsRet;
	unsigned int i;

	if (pstMediaCtrl == NULL)
		return ERR_ARGV_1;

	if (pstRequest == NULL)
		return ERR_ARGV_2;

	if (pstMediaCtrl->pstTopics == NULL || pstMediaCtrl->pstVolumeAskWriter == NULL || pstMediaCtrl->pstVolumeReplyReader == NULL)
		return ERR_SDK_NOT_READY;

	if ( (ddsRet = dds_get_guid(*pstMediaCtrl->pstVolumeAskWriter->pDdsWriter, &stGuid)) < 0 )
	{
		printf("%s() call dds_get_guid(volume ask writer = %d) failed: ddsRet=%d\r\n", __FUNCTION__, *pstMediaCtrl->pstVolumeAskWriter->pDdsWriter, ddsRet);
		return ERR_EVENT;
	}
	memset(pstRequest, 0, sizeof(*pstRequest));
	do
	{
		pstRequest->askId = __atomic_add_fetch(&pstMediaCtrl->pstTopics->nextAskId, 1, __ATOMIC_RELAXED);
	} while (pstRequest->askId == 0);

	strcpy(pstRequest->sourceName, "aheadform_sdk_");
	for (i = 0; i < sizeof(stGuid.v); i++)
		snprintf(pstRequest->sourceName + sizeof("aheadform_sdk_") - 1 + i * 2, 3, "%02x", (unsigned int)stGuid.v[i]);

	return RET_SUCCESS;
}

static int SubmitVolumeRequestAsync(AHEADFORM_MEDIA_CTRL *pstMediaCtrl, unsigned long long MatchReplyTimeout_us, int notifyEventfd, const AHEADFORM_VOLUME_ASK *pstRequest, AHEADFORM_VOLUME_REPLY_SAMPLE *pstReply)
{
	int cancelRet, ret;

	if ( (ret = TopicReader_RegisterAsyncRequest(pstMediaCtrl->pstVolumeReplyReader, Media_MatchVolumeReplyWithReq, pstRequest, sizeof(*pstRequest), MatchReplyTimeout_us, notifyEventfd, pstReply, sizeof(*pstReply))) != RET_SUCCESS )
		return ret;

	if ( (ret = Media_SendVolumeRequest(pstMediaCtrl, pstRequest)) != RET_SUCCESS )
	{
		if ( (cancelRet = TopicReader_CancelAsyncRequest(pstMediaCtrl->pstVolumeReplyReader, notifyEventfd)) != RET_SUCCESS )
			return cancelRet;

		return ret;
	}

	return RET_SUCCESS;
}
