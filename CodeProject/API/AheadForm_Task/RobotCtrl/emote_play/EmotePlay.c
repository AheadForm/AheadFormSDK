#include "EmotePlay.h"
#include "AheadFormDdsMsg.h"

#include <sys/eventfd.h>
#include <sys/select.h>
#include <unistd.h>

static int ConvertEmoteClipToDds(const AHEADFORM_EMOTE_CLIP *pstClip, emote_msgs_EmoteClip *pstDdsClip);
static int EmotePlay_MatchReplyWithRequestId(const void *pReplyElem, const void *pRequestId);
static int EmotePlay_MatchStateWithRequestId(const void *pEventElem, const void *pRequestId);

int EmotePlay_Init(AHEADFORM_EMOTE_PLAY_CTRL *pstEmotePlayCtrl, AHEADFORM_TOPICS *pstTopics)
{
	AHEADFORM_TOPIC_READER_PARA stReaderPara = {0};

	if (pstEmotePlayCtrl == NULL)
		return ERR_ARGV_1;

	if (pstTopics == NULL)
		return ERR_ARGV_2;

	if (pstTopics->stDdsCtrl.ddsParticipant <= 0)
		return ERR_ARGV_2;

	memset(pstEmotePlayCtrl, 0, sizeof(*pstEmotePlayCtrl));
	pstEmotePlayCtrl->pstTopics = pstTopics;
	if ( (pstEmotePlayCtrl->pstEmotePlayReplyReader = EmotePlay_RegisterReplyReader(pstEmotePlayCtrl, &stReaderPara)) == NULL )
		return ERRCODE_INIT_FAIL;

	if ( (pstEmotePlayCtrl->pstEmotePlayCtrlWriter = AheadFormTopics_RegisterWriter(pstTopics, AHEADFORM_TOPIC_ID_EMOTE_CTRL)) == NULL )
		return ERRCODE_INIT_FAIL;

	if ( (pstEmotePlayCtrl->pstEmotePlayEventReader = EmotePlay_RegisterEventReader(pstEmotePlayCtrl, &stReaderPara)) == NULL )
		return ERRCODE_INIT_FAIL;

	printf("%s succeed.\r\n", __FUNCTION__);
	return RET_SUCCESS;
}

int EmotePlay_Deinit(AHEADFORM_EMOTE_PLAY_CTRL *pstEmotePlayCtrl)
{
	int ret;

	if (pstEmotePlayCtrl == NULL)
		return ERR_ARGV_1;

	if (pstEmotePlayCtrl->pstEmotePlayEventReader != NULL)
	{
		if ( (ret = AheadFormTopics_UnregisterReader(pstEmotePlayCtrl->pstTopics, pstEmotePlayCtrl->pstEmotePlayEventReader)) != RET_SUCCESS )
			return ret;

		pstEmotePlayCtrl->pstEmotePlayEventReader = NULL;
	}

	if (pstEmotePlayCtrl->pstEmotePlayCtrlWriter != NULL)
	{
		if ( (ret = AheadFormTopics_UnregisterWriter(pstEmotePlayCtrl->pstTopics, pstEmotePlayCtrl->pstEmotePlayCtrlWriter)) != RET_SUCCESS )
			return ret;

		pstEmotePlayCtrl->pstEmotePlayCtrlWriter = NULL;
	}

	if (pstEmotePlayCtrl->pstEmotePlayReplyReader != NULL)
	{
		if ( (ret = AheadFormTopics_UnregisterReader(pstEmotePlayCtrl->pstTopics, pstEmotePlayCtrl->pstEmotePlayReplyReader)) != RET_SUCCESS )
			return ret;

		pstEmotePlayCtrl->pstEmotePlayReplyReader = NULL;
	}
	pstEmotePlayCtrl->pstTopics = NULL;
	pstEmotePlayCtrl->nextEmotePlayRequestId = 0;
	return RET_SUCCESS;
}

AHEADFORM_TOPIC_READER* EmotePlay_RegisterReplyReader(AHEADFORM_EMOTE_PLAY_CTRL *pstEmotePlayCtrl, const AHEADFORM_TOPIC_READER_PARA *pstReaderPara)
{
	if (pstEmotePlayCtrl == NULL)
	{
		PRINT_FUNCTION_ERRINFO(ERR_ARGV_1, __FUNCTION__);
		return NULL;
	}

	if (pstReaderPara == NULL)
	{
		PRINT_FUNCTION_ERRINFO(ERR_ARGV_2, __FUNCTION__);
		return NULL;
	}

	if (pstEmotePlayCtrl->pstTopics == NULL || pstEmotePlayCtrl->pstTopics->stDdsCtrl.ddsParticipant <= 0)
	{
		PRINT_FUNCTION_ERRINFO(ERR_SDK_NOT_READY, __FUNCTION__);
		return NULL;
	}

	return AheadFormTopics_RegisterReader(pstEmotePlayCtrl->pstTopics, AHEADFORM_TOPIC_ID_EMOTE_REPLY, pstReaderPara);
}

int EmotePlay_UnregisterReplyReader(AHEADFORM_EMOTE_PLAY_CTRL *pstEmotePlayCtrl, AHEADFORM_TOPIC_READER *pstReader)
{
	if (pstEmotePlayCtrl == NULL)
		return ERR_ARGV_1;

	if (pstReader == NULL)
		return ERR_ARGV_2;

	if (pstEmotePlayCtrl->pstTopics == NULL)
		return ERR_SDK_NOT_READY;

	if (pstReader == pstEmotePlayCtrl->pstEmotePlayReplyReader)
		return ERR_RESOURCE_BUSY;

	if (pstReader->topicId != AHEADFORM_TOPIC_ID_EMOTE_REPLY)
		return ERR_ARGV_2;

	return AheadFormTopics_UnregisterReader(pstEmotePlayCtrl->pstTopics, pstReader);
}

AHEADFORM_TOPIC_READER* EmotePlay_RegisterEventReader(AHEADFORM_EMOTE_PLAY_CTRL *pstEmotePlayCtrl, const AHEADFORM_TOPIC_READER_PARA *pstReaderPara)
{
	if (pstEmotePlayCtrl == NULL)
	{
		PRINT_FUNCTION_ERRINFO(ERR_ARGV_1, __FUNCTION__);
		return NULL;
	}

	if (pstReaderPara == NULL)
	{
		PRINT_FUNCTION_ERRINFO(ERR_ARGV_2, __FUNCTION__);
		return NULL;
	}

	if (pstEmotePlayCtrl->pstTopics == NULL || pstEmotePlayCtrl->pstTopics->stDdsCtrl.ddsParticipant <= 0)
	{
		PRINT_FUNCTION_ERRINFO(ERR_SDK_NOT_READY, __FUNCTION__);
		return NULL;
	}

	return AheadFormTopics_RegisterReader(pstEmotePlayCtrl->pstTopics, AHEADFORM_TOPIC_ID_EMOTE_EVENT, pstReaderPara);
}

int EmotePlay_UnregisterEventReader(AHEADFORM_EMOTE_PLAY_CTRL *pstEmotePlayCtrl, AHEADFORM_TOPIC_READER *pstReader)
{
	if (pstEmotePlayCtrl == NULL)
		return ERR_ARGV_1;

	if (pstReader == NULL)
		return ERR_ARGV_2;

	if (pstEmotePlayCtrl->pstTopics == NULL)
		return ERR_SDK_NOT_READY;

	if (pstReader == pstEmotePlayCtrl->pstEmotePlayEventReader)
		return ERR_RESOURCE_BUSY;

	if (pstReader->topicId != AHEADFORM_TOPIC_ID_EMOTE_EVENT)
		return ERR_ARGV_2;

	return AheadFormTopics_UnregisterReader(pstEmotePlayCtrl->pstTopics, pstReader);
}

int EmotePlay_BuildPlayRequest(AHEADFORM_EMOTE_PLAY_CTRL *pstEmotePlayCtrl, ENUM_AHEADFORM_EMOTE_SOURCE_TYPE sourceType, const AHEADFORM_EMOTE_CLIP *pstMotionClips, unsigned int motionClipCnt,
	const AHEADFORM_EMOTE_CLIP *pstAudioClips, unsigned int audioClipCnt, unsigned char isLoop, AHEADFORM_EMOTE_ASK *pstAsk)
{
	if (pstEmotePlayCtrl == NULL)
		return ERR_ARGV_1;

	if (sourceType != AHEADFORM_EMOTE_SOURCE_ID && sourceType != AHEADFORM_EMOTE_SOURCE_NAME)
		return ERR_ARGV_2;

	if ((motionClipCnt == 0 && pstMotionClips != NULL) || (motionClipCnt > 0 && pstMotionClips == NULL))
		return ERR_ARGV_3;

	if (motionClipCnt > AHEADFORM_EMOTE_CLIP_CNT_MAX)
		return ERR_ARGV_4;

	if ((audioClipCnt == 0 && pstAudioClips != NULL) || (audioClipCnt > 0 && pstAudioClips == NULL))
		return ERR_ARGV_5;

	if (audioClipCnt > AHEADFORM_EMOTE_CLIP_CNT_MAX)
		return ERR_ARGV;

	if (motionClipCnt == 0 && audioClipCnt == 0)
		return ERR_ARGV;

	if (isLoop > 1)
		return ERR_ARGV;

	if (pstAsk == NULL)
		return ERR_ARGV;

	if (pstEmotePlayCtrl->pstEmotePlayReplyReader == NULL || pstEmotePlayCtrl->pstEmotePlayCtrlWriter == NULL)
		return ERR_SDK_NOT_READY;

	do
	{
		pstAsk->requestId = __atomic_add_fetch(&pstEmotePlayCtrl->nextEmotePlayRequestId, 1, __ATOMIC_RELAXED);
	} while (pstAsk->requestId == 0);

	pstAsk->cmd = AHEADFORM_EMOTE_CMD_PLAY;
	pstAsk->sourceType = sourceType;
	pstAsk->motionClipCnt = motionClipCnt;
	pstAsk->audioClipCnt = audioClipCnt;
	pstAsk->isLoop = isLoop;
	if (motionClipCnt > 0)
		memcpy(pstAsk->szstMotionClips, pstMotionClips, motionClipCnt * sizeof(*pstMotionClips));

	if (audioClipCnt > 0)
		memcpy(pstAsk->szstAudioClips, pstAudioClips, audioClipCnt * sizeof(*pstAudioClips));

	return RET_SUCCESS;
}

int EmotePlay_BuildCtrlRequest(AHEADFORM_EMOTE_PLAY_CTRL *pstEmotePlayCtrl, ENUM_AHEADFORM_EMOTE_CMD cmd, AHEADFORM_EMOTE_ASK *pstAsk)
{
	if (pstEmotePlayCtrl == NULL)
		return ERR_ARGV_1;

	if (cmd != AHEADFORM_EMOTE_CMD_PAUSE && cmd != AHEADFORM_EMOTE_CMD_RESUME && cmd != AHEADFORM_EMOTE_CMD_STOP)
		return ERR_ARGV_2;

	if (pstAsk == NULL)
		return ERR_ARGV_3;

	if (pstEmotePlayCtrl->pstEmotePlayReplyReader == NULL || pstEmotePlayCtrl->pstEmotePlayCtrlWriter == NULL)
		return ERR_SDK_NOT_READY;

	do
	{
		pstAsk->requestId = __atomic_add_fetch(&pstEmotePlayCtrl->nextEmotePlayRequestId, 1, __ATOMIC_RELAXED);
	} while (pstAsk->requestId == 0);

	pstAsk->cmd = cmd;
	pstAsk->sourceType = AHEADFORM_EMOTE_SOURCE_NAME;
	pstAsk->motionClipCnt = 0;
	pstAsk->audioClipCnt = 0;
	pstAsk->isLoop = 0;
	return RET_SUCCESS;
}

static int ConvertEmoteClipToDds(const AHEADFORM_EMOTE_CLIP *pstClip, emote_msgs_EmoteClip *pstDdsClip)
{
	size_t sourceLen;

	if ( (sourceLen = strnlen(pstClip->strSource, sizeof(pstClip->strSource))) == 0 || sourceLen >= sizeof(pstClip->strSource) )
		return ERRCODE_DATA_PACKET_FORMAT;

	if (pstClip->gapBehavior != AHEADFORM_EMOTE_GAP_DEFAULT && pstClip->gapBehavior != AHEADFORM_EMOTE_GAP_HOLD && pstClip->gapBehavior != AHEADFORM_EMOTE_GAP_INTERPOLATE)
		return ERRCODE_DATA_PACKET_FORMAT;

	memcpy(pstDdsClip->source, pstClip->strSource, sourceLen + 1);
	pstDdsClip->start_at_ms = pstClip->startAt_ms;
	pstDdsClip->clip_in_ms = pstClip->clipBegin_ms;
	pstDdsClip->clip_out_ms = pstClip->clipEnd_ms;
	pstDdsClip->gap_behavior = (emote_msgs_EmoteGapBehavior)pstClip->gapBehavior;
	return RET_SUCCESS;
}

int EmotePlay_SendRequest(AHEADFORM_EMOTE_PLAY_CTRL *pstEmotePlayCtrl, const AHEADFORM_EMOTE_ASK *pstAsk)
{
	emote_msgs_EmoteControl stDdsAsk;
	emote_msgs_EmoteClip szstDdsMotionClips[AHEADFORM_EMOTE_CLIP_CNT_MAX];
	emote_msgs_EmoteClip szstDdsAudioClips[AHEADFORM_EMOTE_CLIP_CNT_MAX];
	unsigned int i;

	if (pstEmotePlayCtrl == NULL)
		return ERR_ARGV_1;

	if (pstAsk == NULL)
		return ERR_ARGV_2;

	if (pstEmotePlayCtrl->pstEmotePlayCtrlWriter == NULL)
		return ERR_SDK_NOT_READY;

	if (pstAsk->requestId == 0)
		return ERR_ARGV_2;

	if (pstAsk->cmd != AHEADFORM_EMOTE_CMD_PLAY && pstAsk->cmd != AHEADFORM_EMOTE_CMD_PAUSE && pstAsk->cmd != AHEADFORM_EMOTE_CMD_RESUME && pstAsk->cmd != AHEADFORM_EMOTE_CMD_STOP)
		return ERR_ARGV_2;

	if (pstAsk->sourceType != AHEADFORM_EMOTE_SOURCE_ID && pstAsk->sourceType != AHEADFORM_EMOTE_SOURCE_NAME)
		return ERR_ARGV_2;

	if (pstAsk->motionClipCnt > AHEADFORM_EMOTE_CLIP_CNT_MAX || pstAsk->audioClipCnt > AHEADFORM_EMOTE_CLIP_CNT_MAX || pstAsk->isLoop > 1)
		return ERR_ARGV_2;

	if (pstAsk->cmd == AHEADFORM_EMOTE_CMD_PLAY)
	{
		if (pstAsk->motionClipCnt == 0 && pstAsk->audioClipCnt == 0)
			return ERR_ARGV_2;
	}
	else if (pstAsk->motionClipCnt != 0 || pstAsk->audioClipCnt != 0 || pstAsk->isLoop != 0)
	{
		return ERR_ARGV_2;
	}

	for (i = 0; i < pstAsk->motionClipCnt; i++)
	{
		if (ConvertEmoteClipToDds(&pstAsk->szstMotionClips[i], &szstDdsMotionClips[i]) != RET_SUCCESS)
			return ERR_ARGV_2;
	}

	for (i = 0; i < pstAsk->audioClipCnt; i++)
	{
		if (ConvertEmoteClipToDds(&pstAsk->szstAudioClips[i], &szstDdsAudioClips[i]) != RET_SUCCESS)
			return ERR_ARGV_2;
	}
	stDdsAsk.request_id = pstAsk->requestId;
	stDdsAsk.command = (emote_msgs_EmoteCommand)pstAsk->cmd;
	stDdsAsk.source_type = (emote_msgs_EmoteSourceType)pstAsk->sourceType;
	stDdsAsk.motion_clips._maximum = pstAsk->motionClipCnt;
	stDdsAsk.motion_clips._length = pstAsk->motionClipCnt;
	stDdsAsk.motion_clips._buffer = pstAsk->motionClipCnt > 0 ? szstDdsMotionClips : NULL;
	stDdsAsk.motion_clips._release = false;
	stDdsAsk.audio_clips._maximum = pstAsk->audioClipCnt;
	stDdsAsk.audio_clips._length = pstAsk->audioClipCnt;
	stDdsAsk.audio_clips._buffer = pstAsk->audioClipCnt > 0 ? szstDdsAudioClips : NULL;
	stDdsAsk.audio_clips._release = false;
	stDdsAsk.reset = pstAsk->cmd == AHEADFORM_EMOTE_CMD_PLAY;
	stDdsAsk.loop = pstAsk->isLoop != 0;
	stDdsAsk.start_action = emote_msgs_EMOTE_ACTION_NONE;
	stDdsAsk.finish_action = emote_msgs_EMOTE_ACTION_NONE;
	return TopicWriter_Write(pstEmotePlayCtrl->pstEmotePlayCtrlWriter, &stDdsAsk);
}

static int EmotePlay_MatchReplyWithRequestId(const void *pReplyElem, const void *pRequestId)
{
	const AHEADFORM_EMOTE_REPLY *pstReply = (const AHEADFORM_EMOTE_REPLY*)pReplyElem;
	unsigned long long requestId;

	memcpy(&requestId, pRequestId, sizeof(requestId));
	if (pstReply->requestId != requestId)
		return ERRCODE_MATCH_REPLY_WITH_REQ_FAIL;

	return RET_SUCCESS;
}

static int EmotePlay_MatchStateWithRequestId(const void *pEventElem, const void *pRequestId)
{
	const AHEADFORM_EMOTE_EVENT *pstEvent = (const AHEADFORM_EMOTE_EVENT*)pEventElem;
	unsigned long long requestId;

	memcpy(&requestId, pRequestId, sizeof(requestId));
	if (pstEvent->requestId != requestId || strcmp(pstEvent->strState, "RESOURCE_FILES_RELEASED") == 0)
		return ERRCODE_MATCH_REPLY_WITH_REQ_FAIL;

	return RET_SUCCESS;
}

int EmotePlay_SubmitAsyncRequest(AHEADFORM_EMOTE_PLAY_CTRL *pstEmotePlayCtrl, const AHEADFORM_EMOTE_ASK *pstAsk, unsigned long long MatchReplyTimeout_us, int notifyEventfd, AHEADFORM_EMOTE_REPLY *pstReply)
{
	int cancelRet, ret;

	if (pstEmotePlayCtrl == NULL)
		return ERR_ARGV_1;

	if (pstAsk == NULL)
		return ERR_ARGV_2;

	if (MatchReplyTimeout_us == 0)
		return ERR_ARGV_3;

	if (notifyEventfd <= 0)
		return ERR_ARGV_4;

	if (pstReply == NULL)
		return ERR_ARGV_5;

	if (pstEmotePlayCtrl->pstEmotePlayCtrlWriter == NULL || pstEmotePlayCtrl->pstEmotePlayReplyReader == NULL)
		return ERR_SDK_NOT_READY;

	if ( (ret = TopicReader_RegisterAsyncRequest(pstEmotePlayCtrl->pstEmotePlayReplyReader, EmotePlay_MatchReplyWithRequestId, &pstAsk->requestId, sizeof(pstAsk->requestId), MatchReplyTimeout_us, notifyEventfd, pstReply, sizeof(*pstReply))) != RET_SUCCESS )
		return ret;

	if ( (ret = EmotePlay_SendRequest(pstEmotePlayCtrl, pstAsk)) != RET_SUCCESS )
	{
		if ( (cancelRet = TopicReader_CancelAsyncRequest(pstEmotePlayCtrl->pstEmotePlayReplyReader, notifyEventfd)) != RET_SUCCESS )
			return cancelRet;

		return ret;
	}

	return RET_SUCCESS;
}

int EmotePlay_CheckIsAsyncRequestReplied(AHEADFORM_EMOTE_PLAY_CTRL *pstEmotePlayCtrl, int notifyEventfd)
{
	if (pstEmotePlayCtrl == NULL)
		return ERR_ARGV_1;

	if (pstEmotePlayCtrl->pstEmotePlayReplyReader == NULL)
		return ERR_SDK_NOT_READY;

	return TopicReader_CheckIsAsyncRequestReplied(pstEmotePlayCtrl->pstEmotePlayReplyReader, notifyEventfd);
}

int EmotePlay_CancelAsyncRequest(AHEADFORM_EMOTE_PLAY_CTRL *pstEmotePlayCtrl, int notifyEventfd)
{
	if (pstEmotePlayCtrl == NULL)
		return ERR_ARGV_1;

	if (pstEmotePlayCtrl->pstEmotePlayReplyReader == NULL)
		return ERR_SDK_NOT_READY;

	return TopicReader_CancelAsyncRequest(pstEmotePlayCtrl->pstEmotePlayReplyReader, notifyEventfd);
}

int EmotePlay_RegisterGetStateAsyncRequest(AHEADFORM_EMOTE_PLAY_CTRL *pstEmotePlayCtrl, unsigned long long playRequestId, unsigned long long MatchStateTimeout_us, int notifyEventfd, AHEADFORM_EMOTE_EVENT *pstEvent)
{
	if (pstEmotePlayCtrl == NULL)
		return ERR_ARGV_1;

	if (playRequestId == 0)
		return ERR_ARGV_2;

	if (MatchStateTimeout_us == 0)
		return ERR_ARGV_3;

	if (notifyEventfd <= 0)
		return ERR_ARGV_4;

	if (pstEvent == NULL)
		return ERR_ARGV_5;

	if (pstEmotePlayCtrl->pstEmotePlayEventReader == NULL)
		return ERR_SDK_NOT_READY;

	return TopicReader_RegisterAsyncRequest(pstEmotePlayCtrl->pstEmotePlayEventReader, EmotePlay_MatchStateWithRequestId, &playRequestId, sizeof(playRequestId), MatchStateTimeout_us, notifyEventfd, pstEvent, sizeof(*pstEvent));
}

int EmotePlay_CheckIsGetStateAsyncRequestCompleted(AHEADFORM_EMOTE_PLAY_CTRL *pstEmotePlayCtrl, int notifyEventfd)
{
	if (pstEmotePlayCtrl == NULL)
		return ERR_ARGV_1;

	if (pstEmotePlayCtrl->pstEmotePlayEventReader == NULL)
		return ERR_SDK_NOT_READY;

	return TopicReader_CheckIsAsyncRequestReplied(pstEmotePlayCtrl->pstEmotePlayEventReader, notifyEventfd);
}

int EmotePlay_CancelGetStateAsyncRequest(AHEADFORM_EMOTE_PLAY_CTRL *pstEmotePlayCtrl, int notifyEventfd)
{
	if (pstEmotePlayCtrl == NULL)
		return ERR_ARGV_1;

	if (pstEmotePlayCtrl->pstEmotePlayEventReader == NULL)
		return ERR_SDK_NOT_READY;

	return TopicReader_CancelAsyncRequest(pstEmotePlayCtrl->pstEmotePlayEventReader, notifyEventfd);
}

int EmotePlay_SubmitAsyncRequestAndWaitReply(AHEADFORM_EMOTE_PLAY_CTRL *pstEmotePlayCtrl, const AHEADFORM_EMOTE_ASK *pstAsk, AHEADFORM_EMOTE_REPLY *pstReply, int timeout_ms)
{
	eventfd_t eventCnt;
	struct timeval stTimeout;
	fd_set rdFdSet;
	int notifyEventfd, max_fd, cleanupRet, ret;

	if (pstEmotePlayCtrl == NULL)
		return ERR_ARGV_1;

	if (pstAsk == NULL)
		return ERR_ARGV_2;

	if (pstReply == NULL)
		return ERR_ARGV_3;

	if (timeout_ms <= 0)
		return ERR_ARGV_4;

	if (pstEmotePlayCtrl->pstEmotePlayReplyReader == NULL || pstEmotePlayCtrl->pstEmotePlayCtrlWriter == NULL)
		return ERR_SDK_NOT_READY;

	if ( (notifyEventfd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC)) <= 0 )
	{
		printf("%s() call eventfd(emote completion) failed: %s\r\n", __FUNCTION__, strerror(errno));
		return ERR_EVENTFD_CREATE_FAILED;
	}

	if ( (ret = EmotePlay_SubmitAsyncRequest(pstEmotePlayCtrl, pstAsk, (unsigned long long)timeout_ms * 1000, notifyEventfd, pstReply)) != RET_SUCCESS )
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
		if ( (cleanupRet = EmotePlay_CancelAsyncRequest(pstEmotePlayCtrl, notifyEventfd)) != RET_SUCCESS )
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
