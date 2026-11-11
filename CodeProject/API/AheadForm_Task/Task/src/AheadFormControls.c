#include "AheadForm_Task.h"

typedef struct
{
	unsigned long long askId;
	char sourceName[AHEADFORM_SOURCE_NAME_LEN_MAX];
	unsigned char action;
	unsigned char device;
} MATCH_HARDWARE_CONTROL;

static int MatchSavedHardwareControl(const void *pReplyElem, const void *pMatchArg)
{
	const AHEADFORM_HARDWARE_CONTROL_REPLY_SAMPLE *pstReply = (const AHEADFORM_HARDWARE_CONTROL_REPLY_SAMPLE*)pReplyElem;
	MATCH_HARDWARE_CONTROL stMatch;

	memcpy(&stMatch, pMatchArg, sizeof(stMatch));
	return pstReply->askId == stMatch.askId && strcmp(pstReply->sourceName, stMatch.sourceName) == 0 && pstReply->action == stMatch.action && pstReply->device == stMatch.device;
}

int AheadForm_BuildHardwareControlRequest(AHEADFORM_ALL_CTRL *pstAheadForm, const AHEADFORM_HARDWARE_CONTROL_PARA *pstPara, AHEADFORM_HARDWARE_CONTROL_ASK *pstRequest)
{
	dds_guid_t stGuid;
	dds_return_t ddsRet;
	unsigned int i;

	if (pstAheadForm == NULL)
		return ERR_ARGV_1;

	if (pstPara == NULL)
		return ERR_ARGV_2;

	if (pstRequest == NULL)
		return ERR_ARGV_3;

	if (pstAheadForm->pstHardwareControlAskWriter == NULL || pstAheadForm->pstHardwareControlReplyReader == NULL)
		return ERR_SDK_NOT_READY;

	if ((pstPara->action != AHEADFORM_HARDWARE_ACTION_PRINT_ERROR_LOG && pstPara->action != AHEADFORM_HARDWARE_ACTION_EXIT_DAEMON) || pstPara->device < AHEADFORM_HARDWARE_DEVICE_FACE || pstPara->device > AHEADFORM_HARDWARE_DEVICE_SENSOR)
		return ERR_ARGV_2;

	memset(pstRequest, 0, sizeof(*pstRequest));
	do
	{
		pstRequest->askId = __atomic_add_fetch(&pstAheadForm->stTopics.nextAskId, 1, __ATOMIC_RELAXED);
	} while (pstRequest->askId == 0);

	if ( (ddsRet = dds_get_guid(*pstAheadForm->pstHardwareControlAskWriter->pDdsWriter, &stGuid)) < 0 )
	{
		printf("%s() call dds_get_guid(request writer) failed: ddsRet=%d\r\n", __FUNCTION__, ddsRet);
		return ERR_EVENT;
	}
	strcpy(pstRequest->sourceName, "aheadform_sdk_");
	for (i = 0; i < sizeof(stGuid.v); i++)
		snprintf(pstRequest->sourceName + sizeof("aheadform_sdk_") - 1 + i * 2, 3, "%02x", (unsigned int)stGuid.v[i]);

	pstRequest->action = pstPara->action;
	pstRequest->device = pstPara->device;
	return RET_SUCCESS;
}

int AheadForm_SendHardwareControlRequest(AHEADFORM_ALL_CTRL *pstAheadForm, const AHEADFORM_HARDWARE_CONTROL_ASK *pstRequest)
{
	if (pstAheadForm == NULL)
		return ERR_ARGV_1;

	if (pstRequest == NULL)
		return ERR_ARGV_2;

	if (pstAheadForm->pstHardwareControlAskWriter == NULL)
		return ERR_SDK_NOT_READY;

	return TopicWriter_Write(pstAheadForm->pstHardwareControlAskWriter, pstRequest);
}

int AheadForm_HardwareControlAsync(AHEADFORM_ALL_CTRL *pstAheadForm, const AHEADFORM_HARDWARE_CONTROL_PARA *pstPara, unsigned long long MatchReplyTimeout_us, int notifyEventfd, AHEADFORM_HARDWARE_CONTROL_REPLY_SAMPLE *pstReply)
{
	AHEADFORM_HARDWARE_CONTROL_ASK stRequest;
	MATCH_HARDWARE_CONTROL stMatch;
	int ret, cancelRet;

	if (pstAheadForm == NULL)
		return ERR_ARGV_1;

	if (pstReply == NULL)
		return ERR_ARGV_5;

	if ( (ret = AheadForm_BuildHardwareControlRequest(pstAheadForm, pstPara, &stRequest)) != RET_SUCCESS )
		return ret;

	stMatch.askId = stRequest.askId;
	strcpy(stMatch.sourceName, stRequest.sourceName);
	stMatch.action = stRequest.action;
	stMatch.device = stRequest.device;

	if ( (ret = TopicReader_RegisterAsyncRequest(pstAheadForm->pstHardwareControlReplyReader, MatchSavedHardwareControl, &stMatch, sizeof(stMatch), MatchReplyTimeout_us, notifyEventfd, pstReply, sizeof(*pstReply))) != RET_SUCCESS )
		return ret;

	if ( (ret = AheadForm_SendHardwareControlRequest(pstAheadForm, &stRequest)) != RET_SUCCESS )
	{
		if ( (cancelRet = TopicReader_CancelAsyncRequest(pstAheadForm->pstHardwareControlReplyReader, notifyEventfd)) != RET_SUCCESS )
			return cancelRet;

		return ret;
	}

	return RET_SUCCESS;
}

int AheadForm_CheckIsHardwareControlAsyncRequestReplied(AHEADFORM_ALL_CTRL *pstAheadForm, int notifyEventfd)
{
	if (pstAheadForm == NULL)
		return ERR_ARGV_1;

	if (pstAheadForm->pstHardwareControlReplyReader == NULL)
		return ERR_SDK_NOT_READY;

	return TopicReader_CheckIsAsyncRequestReplied(pstAheadForm->pstHardwareControlReplyReader, notifyEventfd);
}

int AheadForm_CancelHardwareControlAsyncRequest(AHEADFORM_ALL_CTRL *pstAheadForm, int notifyEventfd)
{
	if (pstAheadForm == NULL)
		return ERR_ARGV_1;

	if (pstAheadForm->pstHardwareControlReplyReader == NULL)
		return ERR_SDK_NOT_READY;

	return TopicReader_CancelAsyncRequest(pstAheadForm->pstHardwareControlReplyReader, notifyEventfd);
}

AHEADFORM_TOPIC_READER* AheadForm_RegisterHardwareControlReader(AHEADFORM_ALL_CTRL *pstAheadForm, const AHEADFORM_TOPIC_READER_PARA *pstReaderPara)
{
	if (pstAheadForm == NULL)
	{
		PRINT_FUNCTION_ERRINFO(ERR_ARGV_1, __FUNCTION__);
		return NULL;
	}

	return AheadFormTopics_RegisterReader(&pstAheadForm->stTopics, AHEADFORM_TOPIC_ID_HARDWARE_CONTROL_REPLY, pstReaderPara);
}

int AheadForm_UnregisterHardwareControlReader(AHEADFORM_ALL_CTRL *pstAheadForm, AHEADFORM_TOPIC_READER *pstReader)
{
	if (pstAheadForm == NULL)
		return ERR_ARGV_1;

	if (pstReader == NULL || pstReader == pstAheadForm->pstHardwareControlReplyReader || pstReader->topicId != AHEADFORM_TOPIC_ID_HARDWARE_CONTROL_REPLY)
		return ERR_ARGV_2;

	return AheadFormTopics_UnregisterReader(&pstAheadForm->stTopics, pstReader);
}

int AheadForm_MatchHardwareControlReplyWithReq(const void *pReplyElem, const void *pRequest)
{
	const AHEADFORM_HARDWARE_CONTROL_REPLY_SAMPLE *pstReply = (const AHEADFORM_HARDWARE_CONTROL_REPLY_SAMPLE*)pReplyElem;
	AHEADFORM_HARDWARE_CONTROL_ASK stRequest;

	if (pstReply == NULL || pRequest == NULL)
		return 0;

	memcpy(&stRequest, pRequest, sizeof(stRequest));
	return pstReply->askId == stRequest.askId && strcmp(pstReply->sourceName, stRequest.sourceName) == 0 && pstReply->action == stRequest.action && pstReply->device == stRequest.device;
}

typedef struct
{
	unsigned long long askId;
	char sourceName[AHEADFORM_SOURCE_NAME_LEN_MAX];
	unsigned char action;
} MATCH_SAFETY_CONTROL;

static int MatchSavedSafetyControl(const void *pReplyElem, const void *pMatchArg)
{
	const AHEADFORM_SAFETY_CONTROL_REPLY_SAMPLE *pstReply = (const AHEADFORM_SAFETY_CONTROL_REPLY_SAMPLE*)pReplyElem;
	MATCH_SAFETY_CONTROL stMatch;

	memcpy(&stMatch, pMatchArg, sizeof(stMatch));
	return pstReply->askId == stMatch.askId && strcmp(pstReply->sourceName, stMatch.sourceName) == 0 && pstReply->action == stMatch.action;
}

int AheadForm_BuildSafetyControlRequest(AHEADFORM_ALL_CTRL *pstAheadForm, const AHEADFORM_SAFETY_CONTROL_PARA *pstPara, AHEADFORM_SAFETY_CONTROL_ASK *pstRequest)
{
	dds_guid_t stGuid;
	dds_return_t ddsRet;
	unsigned int i;

	if (pstAheadForm == NULL)
		return ERR_ARGV_1;

	if (pstPara == NULL)
		return ERR_ARGV_2;

	if (pstRequest == NULL)
		return ERR_ARGV_3;

	if (pstAheadForm->pstSafetyControlAskWriter == NULL || pstAheadForm->pstSafetyControlReplyReader == NULL)
		return ERR_SDK_NOT_READY;

	if (pstPara->action < AHEADFORM_SAFETY_ACTION_MOTION_PERMISSION || pstPara->action > AHEADFORM_SAFETY_ACTION_ESTOP)
		return ERR_ARGV_2;

	memset(pstRequest, 0, sizeof(*pstRequest));
	do
	{
		pstRequest->askId = __atomic_add_fetch(&pstAheadForm->stTopics.nextAskId, 1, __ATOMIC_RELAXED);
	} while (pstRequest->askId == 0);

	if ( (ddsRet = dds_get_guid(*pstAheadForm->pstSafetyControlAskWriter->pDdsWriter, &stGuid)) < 0 )
	{
		printf("%s() call dds_get_guid(request writer) failed: ddsRet=%d\r\n", __FUNCTION__, ddsRet);
		return ERR_EVENT;
	}
	strcpy(pstRequest->sourceName, "aheadform_sdk_");
	for (i = 0; i < sizeof(stGuid.v); i++)
		snprintf(pstRequest->sourceName + sizeof("aheadform_sdk_") - 1 + i * 2, 3, "%02x", (unsigned int)stGuid.v[i]);

	pstRequest->action = pstPara->action;
	pstRequest->faceMotionPermitted = pstPara->faceMotionPermitted;
	pstRequest->neckMotionPermitted = pstPara->neckMotionPermitted;
	return RET_SUCCESS;
}

int AheadForm_SendSafetyControlRequest(AHEADFORM_ALL_CTRL *pstAheadForm, const AHEADFORM_SAFETY_CONTROL_ASK *pstRequest)
{
	if (pstAheadForm == NULL)
		return ERR_ARGV_1;

	if (pstRequest == NULL)
		return ERR_ARGV_2;

	if (pstAheadForm->pstSafetyControlAskWriter == NULL)
		return ERR_SDK_NOT_READY;

	return TopicWriter_Write(pstAheadForm->pstSafetyControlAskWriter, pstRequest);
}

int AheadForm_SafetyControlAsync(AHEADFORM_ALL_CTRL *pstAheadForm, const AHEADFORM_SAFETY_CONTROL_PARA *pstPara, unsigned long long MatchReplyTimeout_us, int notifyEventfd, AHEADFORM_SAFETY_CONTROL_REPLY_SAMPLE *pstReply)
{
	AHEADFORM_SAFETY_CONTROL_ASK stRequest;
	MATCH_SAFETY_CONTROL stMatch;
	int ret, cancelRet;

	if (pstAheadForm == NULL)
		return ERR_ARGV_1;

	if (pstReply == NULL)
		return ERR_ARGV_5;

	if ( (ret = AheadForm_BuildSafetyControlRequest(pstAheadForm, pstPara, &stRequest)) != RET_SUCCESS )
		return ret;

	stMatch.askId = stRequest.askId;
	strcpy(stMatch.sourceName, stRequest.sourceName);
	stMatch.action = stRequest.action;

	if ( (ret = TopicReader_RegisterAsyncRequest(pstAheadForm->pstSafetyControlReplyReader, MatchSavedSafetyControl, &stMatch, sizeof(stMatch), MatchReplyTimeout_us, notifyEventfd, pstReply, sizeof(*pstReply))) != RET_SUCCESS )
		return ret;

	if ( (ret = AheadForm_SendSafetyControlRequest(pstAheadForm, &stRequest)) != RET_SUCCESS )
	{
		if ( (cancelRet = TopicReader_CancelAsyncRequest(pstAheadForm->pstSafetyControlReplyReader, notifyEventfd)) != RET_SUCCESS )
			return cancelRet;

		return ret;
	}

	return RET_SUCCESS;
}

int AheadForm_CheckIsSafetyControlAsyncRequestReplied(AHEADFORM_ALL_CTRL *pstAheadForm, int notifyEventfd)
{
	if (pstAheadForm == NULL)
		return ERR_ARGV_1;

	if (pstAheadForm->pstSafetyControlReplyReader == NULL)
		return ERR_SDK_NOT_READY;

	return TopicReader_CheckIsAsyncRequestReplied(pstAheadForm->pstSafetyControlReplyReader, notifyEventfd);
}

int AheadForm_CancelSafetyControlAsyncRequest(AHEADFORM_ALL_CTRL *pstAheadForm, int notifyEventfd)
{
	if (pstAheadForm == NULL)
		return ERR_ARGV_1;

	if (pstAheadForm->pstSafetyControlReplyReader == NULL)
		return ERR_SDK_NOT_READY;

	return TopicReader_CancelAsyncRequest(pstAheadForm->pstSafetyControlReplyReader, notifyEventfd);
}

AHEADFORM_TOPIC_READER* AheadForm_RegisterSafetyControlReader(AHEADFORM_ALL_CTRL *pstAheadForm, const AHEADFORM_TOPIC_READER_PARA *pstReaderPara)
{
	if (pstAheadForm == NULL)
	{
		PRINT_FUNCTION_ERRINFO(ERR_ARGV_1, __FUNCTION__);
		return NULL;
	}

	return AheadFormTopics_RegisterReader(&pstAheadForm->stTopics, AHEADFORM_TOPIC_ID_SAFETY_CONTROL_REPLY, pstReaderPara);
}

int AheadForm_UnregisterSafetyControlReader(AHEADFORM_ALL_CTRL *pstAheadForm, AHEADFORM_TOPIC_READER *pstReader)
{
	if (pstAheadForm == NULL)
		return ERR_ARGV_1;

	if (pstReader == NULL || pstReader == pstAheadForm->pstSafetyControlReplyReader || pstReader->topicId != AHEADFORM_TOPIC_ID_SAFETY_CONTROL_REPLY)
		return ERR_ARGV_2;

	return AheadFormTopics_UnregisterReader(&pstAheadForm->stTopics, pstReader);
}

int AheadForm_MatchSafetyControlReplyWithReq(const void *pReplyElem, const void *pRequest)
{
	const AHEADFORM_SAFETY_CONTROL_REPLY_SAMPLE *pstReply = (const AHEADFORM_SAFETY_CONTROL_REPLY_SAMPLE*)pReplyElem;
	AHEADFORM_SAFETY_CONTROL_ASK stRequest;

	if (pstReply == NULL || pRequest == NULL)
		return 0;

	memcpy(&stRequest, pRequest, sizeof(stRequest));
	return pstReply->askId == stRequest.askId && strcmp(pstReply->sourceName, stRequest.sourceName) == 0 && pstReply->action == stRequest.action;
}

typedef struct
{
	unsigned long long requestId;
} MATCH_OPERATING_MODE;

static int MatchSavedOperatingMode(const void *pReplyElem, const void *pMatchArg)
{
	const AHEADFORM_OPERATING_MODE_REPLY_SAMPLE *pstReply = (const AHEADFORM_OPERATING_MODE_REPLY_SAMPLE*)pReplyElem;
	MATCH_OPERATING_MODE stMatch;

	memcpy(&stMatch, pMatchArg, sizeof(stMatch));
	return pstReply->requestId == stMatch.requestId;
}

int AheadForm_BuildOperatingModeRequest(AHEADFORM_ALL_CTRL *pstAheadForm, unsigned int mode, AHEADFORM_OPERATING_MODE_ASK *pstRequest)
{
	if (pstAheadForm == NULL)
		return ERR_ARGV_1;

	if (pstRequest == NULL)
		return ERR_ARGV_3;

	if (pstAheadForm->pstOperatingModeAskWriter == NULL || pstAheadForm->pstOperatingModeReplyReader == NULL)
		return ERR_SDK_NOT_READY;

	if (mode != AHEADFORM_OPERATING_MODE_DEV && mode != AHEADFORM_OPERATING_MODE_INTERACTIVE)
		return ERR_ARGV_2;

	memset(pstRequest, 0, sizeof(*pstRequest));
	do
	{
		pstRequest->requestId = __atomic_add_fetch(&pstAheadForm->stTopics.nextAskId, 1, __ATOMIC_RELAXED);
	} while (pstRequest->requestId == 0);
	pstRequest->mode = mode;
	return RET_SUCCESS;
}

int AheadForm_SendOperatingModeRequest(AHEADFORM_ALL_CTRL *pstAheadForm, const AHEADFORM_OPERATING_MODE_ASK *pstRequest)
{
	if (pstAheadForm == NULL)
		return ERR_ARGV_1;

	if (pstRequest == NULL)
		return ERR_ARGV_2;

	if (pstAheadForm->pstOperatingModeAskWriter == NULL)
		return ERR_SDK_NOT_READY;

	return TopicWriter_Write(pstAheadForm->pstOperatingModeAskWriter, pstRequest);
}

int AheadForm_OperatingModeAsync(AHEADFORM_ALL_CTRL *pstAheadForm, unsigned int mode, unsigned long long MatchReplyTimeout_us, int notifyEventfd, AHEADFORM_OPERATING_MODE_REPLY_SAMPLE *pstReply)
{
	AHEADFORM_OPERATING_MODE_ASK stRequest;
	MATCH_OPERATING_MODE stMatch;
	int ret, cancelRet;

	if (pstAheadForm == NULL)
		return ERR_ARGV_1;

	if (pstReply == NULL)
		return ERR_ARGV_5;

	if ( (ret = AheadForm_BuildOperatingModeRequest(pstAheadForm, mode, &stRequest)) != RET_SUCCESS )
		return ret;

	stMatch.requestId = stRequest.requestId;

	if ( (ret = TopicReader_RegisterAsyncRequest(pstAheadForm->pstOperatingModeReplyReader, MatchSavedOperatingMode, &stMatch, sizeof(stMatch), MatchReplyTimeout_us, notifyEventfd, pstReply, sizeof(*pstReply))) != RET_SUCCESS )
		return ret;

	if ( (ret = AheadForm_SendOperatingModeRequest(pstAheadForm, &stRequest)) != RET_SUCCESS )
	{
		if ( (cancelRet = TopicReader_CancelAsyncRequest(pstAheadForm->pstOperatingModeReplyReader, notifyEventfd)) != RET_SUCCESS )
			return cancelRet;

		return ret;
	}

	return RET_SUCCESS;
}

int AheadForm_CheckIsOperatingModeAsyncRequestReplied(AHEADFORM_ALL_CTRL *pstAheadForm, int notifyEventfd)
{
	if (pstAheadForm == NULL)
		return ERR_ARGV_1;

	if (pstAheadForm->pstOperatingModeReplyReader == NULL)
		return ERR_SDK_NOT_READY;

	return TopicReader_CheckIsAsyncRequestReplied(pstAheadForm->pstOperatingModeReplyReader, notifyEventfd);
}

int AheadForm_CancelOperatingModeAsyncRequest(AHEADFORM_ALL_CTRL *pstAheadForm, int notifyEventfd)
{
	if (pstAheadForm == NULL)
		return ERR_ARGV_1;

	if (pstAheadForm->pstOperatingModeReplyReader == NULL)
		return ERR_SDK_NOT_READY;

	return TopicReader_CancelAsyncRequest(pstAheadForm->pstOperatingModeReplyReader, notifyEventfd);
}

AHEADFORM_TOPIC_READER* AheadForm_RegisterOperatingModeReader(AHEADFORM_ALL_CTRL *pstAheadForm, const AHEADFORM_TOPIC_READER_PARA *pstReaderPara)
{
	if (pstAheadForm == NULL)
	{
		PRINT_FUNCTION_ERRINFO(ERR_ARGV_1, __FUNCTION__);
		return NULL;
	}

	return AheadFormTopics_RegisterReader(&pstAheadForm->stTopics, AHEADFORM_TOPIC_ID_OPERATING_MODE_REPLY, pstReaderPara);
}

int AheadForm_UnregisterOperatingModeReader(AHEADFORM_ALL_CTRL *pstAheadForm, AHEADFORM_TOPIC_READER *pstReader)
{
	if (pstAheadForm == NULL)
		return ERR_ARGV_1;

	if (pstReader == NULL || pstReader == pstAheadForm->pstOperatingModeReplyReader || pstReader->topicId != AHEADFORM_TOPIC_ID_OPERATING_MODE_REPLY)
		return ERR_ARGV_2;

	return AheadFormTopics_UnregisterReader(&pstAheadForm->stTopics, pstReader);
}

int AheadForm_MatchOperatingModeReplyWithReq(const void *pReplyElem, const void *pRequest)
{
	const AHEADFORM_OPERATING_MODE_REPLY_SAMPLE *pstReply = (const AHEADFORM_OPERATING_MODE_REPLY_SAMPLE*)pReplyElem;
	AHEADFORM_OPERATING_MODE_ASK stRequest;

	if (pstReply == NULL || pRequest == NULL)
		return 0;

	memcpy(&stRequest, pRequest, sizeof(stRequest));
	return pstReply->requestId == stRequest.requestId;
}

