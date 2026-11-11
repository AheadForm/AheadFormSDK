#include "Media.h"

typedef struct
{
	unsigned long long askId;
	char sourceName[AHEADFORM_SOURCE_NAME_LEN_MAX];
	unsigned char operation;
	unsigned char deviceType;
} MATCH_AUDIO_DEVICE;

static int MatchSavedAudioDevice(const void *pReplyElem, const void *pMatchArg)
{
	const AHEADFORM_AUDIO_DEVICE_REPLY_SAMPLE *pstReply = (const AHEADFORM_AUDIO_DEVICE_REPLY_SAMPLE*)pReplyElem;
	MATCH_AUDIO_DEVICE stMatch;

	memcpy(&stMatch, pMatchArg, sizeof(stMatch));
	return pstReply->askId == stMatch.askId && strcmp(pstReply->sourceName, stMatch.sourceName) == 0 && pstReply->operation == stMatch.operation && pstReply->deviceType == stMatch.deviceType;
}

int Media_BuildAudioDeviceRequest(AHEADFORM_MEDIA_CTRL *pstMediaCtrl, const AHEADFORM_AUDIO_DEVICE_PARA *pstPara, AHEADFORM_AUDIO_DEVICE_ASK *pstRequest)
{
	dds_guid_t stGuid;
	dds_return_t ddsRet;
	unsigned int i;

	if (pstMediaCtrl == NULL)
		return ERR_ARGV_1;

	if (pstPara == NULL)
		return ERR_ARGV_2;

	if (pstRequest == NULL)
		return ERR_ARGV_3;

	if (pstMediaCtrl->pstAudioDeviceAskWriter == NULL || pstMediaCtrl->pstAudioDeviceReplyReader == NULL)
		return ERR_SDK_NOT_READY;

	if (pstPara->operation < AHEADFORM_AUDIO_DEVICE_OPERATION_ROUTE || pstPara->operation > AHEADFORM_AUDIO_DEVICE_OPERATION_GET_CURRENT || pstPara->deviceType > AHEADFORM_AUDIO_DEVICE_SPK)
		return ERR_ARGV_2;

	if (strnlen(pstPara->micDeviceId, sizeof(pstPara->micDeviceId)) == sizeof(pstPara->micDeviceId) || strnlen(pstPara->spkDeviceId, sizeof(pstPara->spkDeviceId)) == sizeof(pstPara->spkDeviceId))
		return ERR_ARGV_2;

	if (pstPara->operation == AHEADFORM_AUDIO_DEVICE_OPERATION_ROUTE && ((!pstPara->hasMicDevice && !pstPara->hasSpkDevice) || (pstPara->hasMicDevice && !pstPara->micDeviceId[0]) || (pstPara->hasSpkDevice && !pstPara->spkDeviceId[0])))
		return ERR_ARGV_2;

	memset(pstRequest, 0, sizeof(*pstRequest));
	do
	{
		pstRequest->askId = __atomic_add_fetch(&pstMediaCtrl->pstTopics->nextAskId, 1, __ATOMIC_RELAXED);
	} while (pstRequest->askId == 0);

	if ( (ddsRet = dds_get_guid(*pstMediaCtrl->pstAudioDeviceAskWriter->pDdsWriter, &stGuid)) < 0 )
	{
		printf("%s() call dds_get_guid(request writer) failed: ddsRet=%d\r\n", __FUNCTION__, ddsRet);
		return ERR_EVENT;
	}
	strcpy(pstRequest->sourceName, "aheadform_sdk_");
	for (i = 0; i < sizeof(stGuid.v); i++)
		snprintf(pstRequest->sourceName + sizeof("aheadform_sdk_") - 1 + i * 2, 3, "%02x", (unsigned int)stGuid.v[i]);

	pstRequest->operation = pstPara->operation;
	pstRequest->deviceType = pstPara->deviceType;
	pstRequest->hasMicDevice = pstPara->hasMicDevice != 0;
	strcpy(pstRequest->micDeviceId, pstPara->micDeviceId);
	pstRequest->hasSpkDevice = pstPara->hasSpkDevice != 0;
	strcpy(pstRequest->spkDeviceId, pstPara->spkDeviceId);
	return RET_SUCCESS;
}

int Media_SendAudioDeviceRequest(AHEADFORM_MEDIA_CTRL *pstMediaCtrl, const AHEADFORM_AUDIO_DEVICE_ASK *pstRequest)
{
	if (pstMediaCtrl == NULL)
		return ERR_ARGV_1;

	if (pstRequest == NULL)
		return ERR_ARGV_2;

	if (pstMediaCtrl->pstAudioDeviceAskWriter == NULL)
		return ERR_SDK_NOT_READY;

	return TopicWriter_Write(pstMediaCtrl->pstAudioDeviceAskWriter, pstRequest);
}

int Media_AudioDeviceAsync(AHEADFORM_MEDIA_CTRL *pstMediaCtrl, const AHEADFORM_AUDIO_DEVICE_PARA *pstPara, unsigned long long MatchReplyTimeout_us, int notifyEventfd, AHEADFORM_AUDIO_DEVICE_REPLY_SAMPLE *pstReply)
{
	AHEADFORM_AUDIO_DEVICE_ASK stRequest;
	MATCH_AUDIO_DEVICE stMatch;
	int ret, cancelRet;

	if (pstMediaCtrl == NULL)
		return ERR_ARGV_1;

	if (pstReply == NULL)
		return ERR_ARGV_5;

	if ( (ret = Media_BuildAudioDeviceRequest(pstMediaCtrl, pstPara, &stRequest)) != RET_SUCCESS )
		return ret;

	stMatch.askId = stRequest.askId;
	strcpy(stMatch.sourceName, stRequest.sourceName);
	stMatch.operation = stRequest.operation;
	stMatch.deviceType = stRequest.deviceType;

	if ( (ret = TopicReader_RegisterAsyncRequest(pstMediaCtrl->pstAudioDeviceReplyReader, MatchSavedAudioDevice, &stMatch, sizeof(stMatch), MatchReplyTimeout_us, notifyEventfd, pstReply, sizeof(*pstReply))) != RET_SUCCESS )
		return ret;

	if ( (ret = Media_SendAudioDeviceRequest(pstMediaCtrl, &stRequest)) != RET_SUCCESS )
	{
		if ( (cancelRet = TopicReader_CancelAsyncRequest(pstMediaCtrl->pstAudioDeviceReplyReader, notifyEventfd)) != RET_SUCCESS )
			return cancelRet;

		return ret;
	}

	return RET_SUCCESS;
}

int Media_CheckIsAudioDeviceAsyncRequestReplied(AHEADFORM_MEDIA_CTRL *pstMediaCtrl, int notifyEventfd)
{
	if (pstMediaCtrl == NULL)
		return ERR_ARGV_1;

	if (pstMediaCtrl->pstAudioDeviceReplyReader == NULL)
		return ERR_SDK_NOT_READY;

	return TopicReader_CheckIsAsyncRequestReplied(pstMediaCtrl->pstAudioDeviceReplyReader, notifyEventfd);
}

int Media_CancelAudioDeviceAsyncRequest(AHEADFORM_MEDIA_CTRL *pstMediaCtrl, int notifyEventfd)
{
	if (pstMediaCtrl == NULL)
		return ERR_ARGV_1;

	if (pstMediaCtrl->pstAudioDeviceReplyReader == NULL)
		return ERR_SDK_NOT_READY;

	return TopicReader_CancelAsyncRequest(pstMediaCtrl->pstAudioDeviceReplyReader, notifyEventfd);
}

AHEADFORM_TOPIC_READER* Media_RegisterAudioDeviceReader(AHEADFORM_MEDIA_CTRL *pstMediaCtrl, const AHEADFORM_TOPIC_READER_PARA *pstReaderPara)
{
	if (pstMediaCtrl == NULL)
	{
		PRINT_FUNCTION_ERRINFO(ERR_ARGV_1, __FUNCTION__);
		return NULL;
	}

	return AheadFormTopics_RegisterReader(pstMediaCtrl->pstTopics, AHEADFORM_TOPIC_ID_AUDIO_DEVICE_REPLY, pstReaderPara);
}

int Media_UnregisterAudioDeviceReader(AHEADFORM_MEDIA_CTRL *pstMediaCtrl, AHEADFORM_TOPIC_READER *pstReader)
{
	if (pstMediaCtrl == NULL)
		return ERR_ARGV_1;

	if (pstReader == NULL || pstReader == pstMediaCtrl->pstAudioDeviceReplyReader || pstReader->topicId != AHEADFORM_TOPIC_ID_AUDIO_DEVICE_REPLY)
		return ERR_ARGV_2;

	return AheadFormTopics_UnregisterReader(pstMediaCtrl->pstTopics, pstReader);
}

int Media_MatchAudioDeviceReplyWithReq(const void *pReplyElem, const void *pRequest)
{
	const AHEADFORM_AUDIO_DEVICE_REPLY_SAMPLE *pstReply = (const AHEADFORM_AUDIO_DEVICE_REPLY_SAMPLE*)pReplyElem;
	AHEADFORM_AUDIO_DEVICE_ASK stRequest;

	if (pstReply == NULL || pRequest == NULL)
		return 0;

	memcpy(&stRequest, pRequest, sizeof(stRequest));
	return pstReply->askId == stRequest.askId && strcmp(pstReply->sourceName, stRequest.sourceName) == 0 && pstReply->operation == stRequest.operation && pstReply->deviceType == stRequest.deviceType;
}

typedef struct
{
	unsigned long long askId;
	char sourceName[AHEADFORM_SOURCE_NAME_LEN_MAX];
} MATCH_MEDIA_CHANNEL;

static int MatchSavedChannel(const void *pReplyElem, const void *pMatchArg)
{
	const AHEADFORM_MEDIA_CHANNEL_REPLY_SAMPLE *pstReply = (const AHEADFORM_MEDIA_CHANNEL_REPLY_SAMPLE*)pReplyElem;
	MATCH_MEDIA_CHANNEL stMatch;

	memcpy(&stMatch, pMatchArg, sizeof(stMatch));
	return pstReply->askId == stMatch.askId && strcmp(pstReply->sourceName, stMatch.sourceName) == 0;
}

int Media_BuildChannelRequest(AHEADFORM_MEDIA_CTRL *pstMediaCtrl, const AHEADFORM_MEDIA_CHANNEL_PARA *pstPara, AHEADFORM_MEDIA_CHANNEL_ASK *pstRequest)
{
	dds_guid_t stGuid;
	dds_return_t ddsRet;
	unsigned int i;

	if (pstMediaCtrl == NULL)
		return ERR_ARGV_1;

	if (pstPara == NULL)
		return ERR_ARGV_2;

	if (pstRequest == NULL)
		return ERR_ARGV_3;

	if (pstMediaCtrl->pstChannelAskWriter == NULL || pstMediaCtrl->pstChannelReplyReader == NULL)
		return ERR_SDK_NOT_READY;

	if (pstPara->videoCtrl > AHEADFORM_CHANNEL_ENABLE || pstPara->micCtrl > AHEADFORM_CHANNEL_ENABLE || pstPara->playbackCtrl > AHEADFORM_CHANNEL_ENABLE || pstPara->vadCtrl > AHEADFORM_CHANNEL_ENABLE)
		return ERR_ARGV_2;

	memset(pstRequest, 0, sizeof(*pstRequest));
	do
	{
		pstRequest->askId = __atomic_add_fetch(&pstMediaCtrl->pstTopics->nextAskId, 1, __ATOMIC_RELAXED);
	} while (pstRequest->askId == 0);

	if ( (ddsRet = dds_get_guid(*pstMediaCtrl->pstChannelAskWriter->pDdsWriter, &stGuid)) < 0 )
	{
		printf("%s() call dds_get_guid(request writer) failed: ddsRet=%d\r\n", __FUNCTION__, ddsRet);
		return ERR_EVENT;
	}
	strcpy(pstRequest->sourceName, "aheadform_sdk_");
	for (i = 0; i < sizeof(stGuid.v); i++)
		snprintf(pstRequest->sourceName + sizeof("aheadform_sdk_") - 1 + i * 2, 3, "%02x", (unsigned int)stGuid.v[i]);

	pstRequest->videoCtrl = pstPara->videoCtrl;
	pstRequest->micCtrl = pstPara->micCtrl;
	pstRequest->playbackCtrl = pstPara->playbackCtrl;
	pstRequest->vadCtrl = pstPara->vadCtrl;
	return RET_SUCCESS;
}

int Media_SendChannelRequest(AHEADFORM_MEDIA_CTRL *pstMediaCtrl, const AHEADFORM_MEDIA_CHANNEL_ASK *pstRequest)
{
	if (pstMediaCtrl == NULL)
		return ERR_ARGV_1;

	if (pstRequest == NULL)
		return ERR_ARGV_2;

	if (pstMediaCtrl->pstChannelAskWriter == NULL)
		return ERR_SDK_NOT_READY;

	return TopicWriter_Write(pstMediaCtrl->pstChannelAskWriter, pstRequest);
}

int Media_ChannelAsync(AHEADFORM_MEDIA_CTRL *pstMediaCtrl, const AHEADFORM_MEDIA_CHANNEL_PARA *pstPara, unsigned long long MatchReplyTimeout_us, int notifyEventfd, AHEADFORM_MEDIA_CHANNEL_REPLY_SAMPLE *pstReply)
{
	AHEADFORM_MEDIA_CHANNEL_ASK stRequest;
	MATCH_MEDIA_CHANNEL stMatch;
	int ret, cancelRet;

	if (pstMediaCtrl == NULL)
		return ERR_ARGV_1;

	if (pstReply == NULL)
		return ERR_ARGV_5;

	if ( (ret = Media_BuildChannelRequest(pstMediaCtrl, pstPara, &stRequest)) != RET_SUCCESS )
		return ret;

	stMatch.askId = stRequest.askId;
	strcpy(stMatch.sourceName, stRequest.sourceName);

	if ( (ret = TopicReader_RegisterAsyncRequest(pstMediaCtrl->pstChannelReplyReader, MatchSavedChannel, &stMatch, sizeof(stMatch), MatchReplyTimeout_us, notifyEventfd, pstReply, sizeof(*pstReply))) != RET_SUCCESS )
		return ret;

	if ( (ret = Media_SendChannelRequest(pstMediaCtrl, &stRequest)) != RET_SUCCESS )
	{
		if ( (cancelRet = TopicReader_CancelAsyncRequest(pstMediaCtrl->pstChannelReplyReader, notifyEventfd)) != RET_SUCCESS )
			return cancelRet;

		return ret;
	}

	return RET_SUCCESS;
}

int Media_CheckIsChannelAsyncRequestReplied(AHEADFORM_MEDIA_CTRL *pstMediaCtrl, int notifyEventfd)
{
	if (pstMediaCtrl == NULL)
		return ERR_ARGV_1;

	if (pstMediaCtrl->pstChannelReplyReader == NULL)
		return ERR_SDK_NOT_READY;

	return TopicReader_CheckIsAsyncRequestReplied(pstMediaCtrl->pstChannelReplyReader, notifyEventfd);
}

int Media_CancelChannelAsyncRequest(AHEADFORM_MEDIA_CTRL *pstMediaCtrl, int notifyEventfd)
{
	if (pstMediaCtrl == NULL)
		return ERR_ARGV_1;

	if (pstMediaCtrl->pstChannelReplyReader == NULL)
		return ERR_SDK_NOT_READY;

	return TopicReader_CancelAsyncRequest(pstMediaCtrl->pstChannelReplyReader, notifyEventfd);
}

AHEADFORM_TOPIC_READER* Media_RegisterChannelReader(AHEADFORM_MEDIA_CTRL *pstMediaCtrl, const AHEADFORM_TOPIC_READER_PARA *pstReaderPara)
{
	if (pstMediaCtrl == NULL)
	{
		PRINT_FUNCTION_ERRINFO(ERR_ARGV_1, __FUNCTION__);
		return NULL;
	}

	return AheadFormTopics_RegisterReader(pstMediaCtrl->pstTopics, AHEADFORM_TOPIC_ID_MEDIA_CHANNEL_REPLY, pstReaderPara);
}

int Media_UnregisterChannelReader(AHEADFORM_MEDIA_CTRL *pstMediaCtrl, AHEADFORM_TOPIC_READER *pstReader)
{
	if (pstMediaCtrl == NULL)
		return ERR_ARGV_1;

	if (pstReader == NULL || pstReader == pstMediaCtrl->pstChannelReplyReader || pstReader->topicId != AHEADFORM_TOPIC_ID_MEDIA_CHANNEL_REPLY)
		return ERR_ARGV_2;

	return AheadFormTopics_UnregisterReader(pstMediaCtrl->pstTopics, pstReader);
}

int Media_MatchChannelReplyWithReq(const void *pReplyElem, const void *pRequest)
{
	const AHEADFORM_MEDIA_CHANNEL_REPLY_SAMPLE *pstReply = (const AHEADFORM_MEDIA_CHANNEL_REPLY_SAMPLE*)pReplyElem;
	AHEADFORM_MEDIA_CHANNEL_ASK stRequest;

	if (pstReply == NULL || pRequest == NULL)
		return 0;

	memcpy(&stRequest, pRequest, sizeof(stRequest));
	return pstReply->askId == stRequest.askId && strcmp(pstReply->sourceName, stRequest.sourceName) == 0;
}

