#include "Head.h"

typedef struct
{
	unsigned long long askId;
	char sourceName[AHEADFORM_MOTION_SOURCE_NAME_LEN_MAX];
} MATCH_PRESET_MOTION;

static int MatchSavedPresetMotion(const void *pReplyElem, const void *pMatchArg)
{
	const AHEADFORM_PRESET_MOTION_REPLY_SAMPLE *pstReply = (const AHEADFORM_PRESET_MOTION_REPLY_SAMPLE*)pReplyElem;
	MATCH_PRESET_MOTION stMatch;

	memcpy(&stMatch, pMatchArg, sizeof(stMatch));
	return pstReply->askId == stMatch.askId && strcmp(pstReply->sourceName, stMatch.sourceName) == 0;
}

int Head_BuildPresetMotionRequest(AHEADFORM_HEAD_CTRL *pstHeadCtrl, unsigned char action, AHEADFORM_PRESET_MOTION_ASK *pstRequest)
{
	if (pstHeadCtrl == NULL)
		return ERR_ARGV_1;

	if (pstRequest == NULL)
		return ERR_ARGV_3;

	if (pstHeadCtrl->pstPresetAskWriter == NULL || pstHeadCtrl->pstPresetReplyReader == NULL)
		return ERR_SDK_NOT_READY;

	if (action != AHEADFORM_PRESET_ACTION_SLEEP && action != AHEADFORM_PRESET_ACTION_CENTER)
		return ERR_ARGV_2;

	memset(pstRequest, 0, sizeof(*pstRequest));
	do
	{
		pstRequest->askId = __atomic_add_fetch(&pstHeadCtrl->pstTopics->nextAskId, 1, __ATOMIC_RELAXED);
	} while (pstRequest->askId == 0);
	strcpy(pstRequest->sourceName, pstHeadCtrl->stCfg.sourceName);
	pstRequest->action = action;
	return RET_SUCCESS;
}

int Head_SendPresetMotionRequest(AHEADFORM_HEAD_CTRL *pstHeadCtrl, const AHEADFORM_PRESET_MOTION_ASK *pstRequest)
{
	if (pstHeadCtrl == NULL)
		return ERR_ARGV_1;

	if (pstRequest == NULL)
		return ERR_ARGV_2;

	if (pstHeadCtrl->pstPresetAskWriter == NULL)
		return ERR_SDK_NOT_READY;

	return TopicWriter_Write(pstHeadCtrl->pstPresetAskWriter, pstRequest);
}

int Head_PresetMotionAsync(AHEADFORM_HEAD_CTRL *pstHeadCtrl, unsigned char action, unsigned long long MatchReplyTimeout_us, int notifyEventfd, AHEADFORM_PRESET_MOTION_REPLY_SAMPLE *pstReply)
{
	AHEADFORM_PRESET_MOTION_ASK stRequest;
	MATCH_PRESET_MOTION stMatch;
	int ret, cancelRet;

	if (pstHeadCtrl == NULL)
		return ERR_ARGV_1;

	if (pstReply == NULL)
		return ERR_ARGV_5;

	if ( (ret = Head_BuildPresetMotionRequest(pstHeadCtrl, action, &stRequest)) != RET_SUCCESS )
		return ret;

	stMatch.askId = stRequest.askId;
	strcpy(stMatch.sourceName, stRequest.sourceName);

	if ( (ret = TopicReader_RegisterAsyncRequest(pstHeadCtrl->pstPresetReplyReader, MatchSavedPresetMotion, &stMatch, sizeof(stMatch), MatchReplyTimeout_us, notifyEventfd, pstReply, sizeof(*pstReply))) != RET_SUCCESS )
		return ret;

	if ( (ret = Head_SendPresetMotionRequest(pstHeadCtrl, &stRequest)) != RET_SUCCESS )
	{
		if ( (cancelRet = TopicReader_CancelAsyncRequest(pstHeadCtrl->pstPresetReplyReader, notifyEventfd)) != RET_SUCCESS )
			return cancelRet;

		return ret;
	}

	return RET_SUCCESS;
}

int Head_CheckIsPresetMotionAsyncRequestReplied(AHEADFORM_HEAD_CTRL *pstHeadCtrl, int notifyEventfd)
{
	if (pstHeadCtrl == NULL)
		return ERR_ARGV_1;

	if (pstHeadCtrl->pstPresetReplyReader == NULL)
		return ERR_SDK_NOT_READY;

	return TopicReader_CheckIsAsyncRequestReplied(pstHeadCtrl->pstPresetReplyReader, notifyEventfd);
}

int Head_CancelPresetMotionAsyncRequest(AHEADFORM_HEAD_CTRL *pstHeadCtrl, int notifyEventfd)
{
	if (pstHeadCtrl == NULL)
		return ERR_ARGV_1;

	if (pstHeadCtrl->pstPresetReplyReader == NULL)
		return ERR_SDK_NOT_READY;

	return TopicReader_CancelAsyncRequest(pstHeadCtrl->pstPresetReplyReader, notifyEventfd);
}

AHEADFORM_TOPIC_READER* Head_RegisterPresetMotionReader(AHEADFORM_HEAD_CTRL *pstHeadCtrl, const AHEADFORM_TOPIC_READER_PARA *pstReaderPara)
{
	if (pstHeadCtrl == NULL)
	{
		PRINT_FUNCTION_ERRINFO(ERR_ARGV_1, __FUNCTION__);
		return NULL;
	}

	return AheadFormTopics_RegisterReader(pstHeadCtrl->pstTopics, AHEADFORM_TOPIC_ID_PRESET_MOTION_REPLY, pstReaderPara);
}

int Head_UnregisterPresetMotionReader(AHEADFORM_HEAD_CTRL *pstHeadCtrl, AHEADFORM_TOPIC_READER *pstReader)
{
	if (pstHeadCtrl == NULL)
		return ERR_ARGV_1;

	if (pstReader == NULL || pstReader == pstHeadCtrl->pstPresetReplyReader || pstReader->topicId != AHEADFORM_TOPIC_ID_PRESET_MOTION_REPLY)
		return ERR_ARGV_2;

	return AheadFormTopics_UnregisterReader(pstHeadCtrl->pstTopics, pstReader);
}

int Head_MatchPresetMotionReplyWithReq(const void *pReplyElem, const void *pRequest)
{
	const AHEADFORM_PRESET_MOTION_REPLY_SAMPLE *pstReply = (const AHEADFORM_PRESET_MOTION_REPLY_SAMPLE*)pReplyElem;
	AHEADFORM_PRESET_MOTION_ASK stRequest;

	if (pstReply == NULL || pRequest == NULL)
		return 0;

	memcpy(&stRequest, pRequest, sizeof(stRequest));
	return pstReply->askId == stRequest.askId && strcmp(pstReply->sourceName, stRequest.sourceName) == 0;
}

