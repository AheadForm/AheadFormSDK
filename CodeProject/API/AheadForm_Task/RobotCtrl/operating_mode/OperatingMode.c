#include "OperatingMode.h"
#include "AheadFormDdsMsg.h"

#include <sys/eventfd.h>
#include <sys/select.h>
#include <unistd.h>

int OperatingMode_Init(AHEADFORM_OPERATING_MODE_CTRL *pstOperatingModeCtrl, AHEADFORM_TOPICS *pstTopics)
{
	AHEADFORM_TOPIC_READER_PARA stReaderPara = {0};

	if (pstOperatingModeCtrl == NULL)
		return ERR_ARGV_1;

	if (pstTopics == NULL)
		return ERR_ARGV_2;

	if (pstTopics->stDdsCtrl.ddsParticipant <= 0)
		return ERR_ARGV_2;

	memset(pstOperatingModeCtrl, 0, sizeof(*pstOperatingModeCtrl));
	pstOperatingModeCtrl->pstTopics = pstTopics;
	if ( (pstOperatingModeCtrl->pstOperatingModeReplyReader = OperatingMode_RegisterReader(pstOperatingModeCtrl, &stReaderPara)) == NULL )
		return ERRCODE_INIT_FAIL;

	if ( (pstOperatingModeCtrl->pstOperatingModeCtrlWriter = AheadFormTopics_RegisterWriter(pstTopics, AHEADFORM_TOPIC_ID_OPERATING_MODE_CTRL)) == NULL )
		return ERRCODE_INIT_FAIL;

	if ( (pstOperatingModeCtrl->pstOperatingModeStateReader = AheadFormTopics_RegisterReader(pstTopics, AHEADFORM_TOPIC_ID_OPERATING_MODE_STATE, &stReaderPara)) == NULL )
		return ERRCODE_INIT_FAIL;

	printf("%s succeed.\r\n", __FUNCTION__);
	return RET_SUCCESS;
}

int OperatingMode_Deinit(AHEADFORM_OPERATING_MODE_CTRL *pstOperatingModeCtrl)
{
	int ret;

	if (pstOperatingModeCtrl == NULL)
		return ERR_ARGV_1;

	if (pstOperatingModeCtrl->pstOperatingModeStateReader != NULL)
	{
		if ( (ret = AheadFormTopics_UnregisterReader(pstOperatingModeCtrl->pstTopics, pstOperatingModeCtrl->pstOperatingModeStateReader)) != RET_SUCCESS )
			return ret;

		pstOperatingModeCtrl->pstOperatingModeStateReader = NULL;
	}

	if (pstOperatingModeCtrl->pstOperatingModeReplyReader != NULL)
	{
		if ( (ret = AheadFormTopics_UnregisterReader(pstOperatingModeCtrl->pstTopics, pstOperatingModeCtrl->pstOperatingModeReplyReader)) != RET_SUCCESS )
			return ret;

		pstOperatingModeCtrl->pstOperatingModeReplyReader = NULL;
	}

	if (pstOperatingModeCtrl->pstOperatingModeCtrlWriter != NULL)
	{
		if ( (ret = AheadFormTopics_UnregisterWriter(pstOperatingModeCtrl->pstTopics, pstOperatingModeCtrl->pstOperatingModeCtrlWriter)) != RET_SUCCESS )
			return ret;

		pstOperatingModeCtrl->pstOperatingModeCtrlWriter = NULL;
	}
	memset(pstOperatingModeCtrl, 0, sizeof(*pstOperatingModeCtrl));
	return RET_SUCCESS;
}

AHEADFORM_TOPIC_READER* OperatingMode_RegisterReader(AHEADFORM_OPERATING_MODE_CTRL *pstOperatingModeCtrl, const AHEADFORM_TOPIC_READER_PARA *pstReaderPara)
{
	if (pstOperatingModeCtrl == NULL)
	{
		PRINT_FUNCTION_ERRINFO(ERR_ARGV_1, __FUNCTION__);
		return NULL;
	}

	if (pstReaderPara == NULL)
	{
		PRINT_FUNCTION_ERRINFO(ERR_ARGV_2, __FUNCTION__);
		return NULL;
	}

	if (pstOperatingModeCtrl->pstTopics == NULL || pstOperatingModeCtrl->pstTopics->stDdsCtrl.ddsParticipant <= 0)
	{
		PRINT_FUNCTION_ERRINFO(ERR_SDK_NOT_READY, __FUNCTION__);
		return NULL;
	}

	return AheadFormTopics_RegisterReader(pstOperatingModeCtrl->pstTopics, AHEADFORM_TOPIC_ID_OPERATING_MODE_REPLY, pstReaderPara);
}

int OperatingMode_UnregisterReader(AHEADFORM_OPERATING_MODE_CTRL *pstOperatingModeCtrl, AHEADFORM_TOPIC_READER *pstReader)
{
	if (pstOperatingModeCtrl == NULL)
		return ERR_ARGV_1;

	if (pstReader == NULL)
		return ERR_ARGV_2;

	if (pstOperatingModeCtrl->pstTopics == NULL)
		return ERR_SDK_NOT_READY;

	if (pstReader == pstOperatingModeCtrl->pstOperatingModeReplyReader)
		return ERR_RESOURCE_BUSY;

	if (pstReader->topicId != AHEADFORM_TOPIC_ID_OPERATING_MODE_REPLY)
		return ERR_ARGV_2;

	return AheadFormTopics_UnregisterReader(pstOperatingModeCtrl->pstTopics, pstReader);
}

int OperatingMode_BuildRequest(AHEADFORM_OPERATING_MODE_CTRL *pstOperatingModeCtrl, ENUM_AHEADFORM_OPERATING_MODE mode, AHEADFORM_OPERATING_MODE_ASK *pstRequest)
{
	if (pstOperatingModeCtrl == NULL)
		return ERR_ARGV_1;

	if (mode != AHEADFORM_OPERATING_MODE_DEV && mode != AHEADFORM_OPERATING_MODE_INTERACTIVE)
		return ERR_ARGV_2;

	if (pstRequest == NULL)
		return ERR_ARGV_3;

	if (pstOperatingModeCtrl->pstOperatingModeReplyReader == NULL || pstOperatingModeCtrl->pstOperatingModeCtrlWriter == NULL)
		return ERR_SDK_NOT_READY;

	do
	{
		pstRequest->requestId = __atomic_add_fetch(&pstOperatingModeCtrl->nextOperatingModeRequestId, 1, __ATOMIC_RELAXED);
	} while (pstRequest->requestId == 0);

	pstRequest->mode = mode;
	return RET_SUCCESS;
}

int OperatingMode_SendRequest(AHEADFORM_OPERATING_MODE_CTRL *pstOperatingModeCtrl, const AHEADFORM_OPERATING_MODE_ASK *pstAsk)
{
	cloud_agent_msgs_msg_OperatingModeControl stDdsAsk;

	if (pstOperatingModeCtrl == NULL)
		return ERR_ARGV_1;

	if (pstAsk == NULL)
		return ERR_ARGV_2;

	if (pstOperatingModeCtrl->pstOperatingModeCtrlWriter == NULL)
		return ERR_SDK_NOT_READY;

	if (pstAsk->requestId == 0 || (pstAsk->mode != AHEADFORM_OPERATING_MODE_DEV && pstAsk->mode != AHEADFORM_OPERATING_MODE_INTERACTIVE))
		return ERR_ARGV_2;

	stDdsAsk.request_id = pstAsk->requestId;
	stDdsAsk.mode = (cloud_agent_msgs_msg_OperatingMode)pstAsk->mode;
	return TopicWriter_Write(pstOperatingModeCtrl->pstOperatingModeCtrlWriter, &stDdsAsk);
}

int OperatingMode_SubmitAsyncRequest(AHEADFORM_OPERATING_MODE_CTRL *pstOperatingModeCtrl, const AHEADFORM_OPERATING_MODE_ASK *pstAsk, unsigned long long MatchReplyTimeout_us, int notifyEventfd, AHEADFORM_OPERATING_MODE_REPLY *pstReply)
{
	int cancelRet, ret;

	if (pstOperatingModeCtrl == NULL)
		return ERR_ARGV_1;

	if (pstAsk == NULL)
		return ERR_ARGV_2;

	if (MatchReplyTimeout_us == 0)
		return ERR_ARGV_3;

	if (notifyEventfd <= 0)
		return ERR_ARGV_4;

	if (pstReply == NULL)
		return ERR_ARGV_5;

	if (pstOperatingModeCtrl->pstOperatingModeCtrlWriter == NULL || pstOperatingModeCtrl->pstOperatingModeReplyReader == NULL)
		return ERR_SDK_NOT_READY;

	if ( (ret = TopicReader_RegisterAsyncRequest(pstOperatingModeCtrl->pstOperatingModeReplyReader, OperatingMode_MatchReplyWithReq, pstAsk, sizeof(*pstAsk), MatchReplyTimeout_us, notifyEventfd, pstReply, sizeof(*pstReply))) != RET_SUCCESS )
		return ret;

	if ( (ret = OperatingMode_SendRequest(pstOperatingModeCtrl, pstAsk)) != RET_SUCCESS )
	{
		if ( (cancelRet = TopicReader_CancelAsyncRequest(pstOperatingModeCtrl->pstOperatingModeReplyReader, notifyEventfd)) != RET_SUCCESS )
			return cancelRet;

		return ret;
	}

	return RET_SUCCESS;
}

int OperatingMode_CheckIsAsyncRequestReplied(AHEADFORM_OPERATING_MODE_CTRL *pstOperatingModeCtrl, int notifyEventfd)
{
	if (pstOperatingModeCtrl == NULL)
		return ERR_ARGV_1;

	if (pstOperatingModeCtrl->pstOperatingModeReplyReader == NULL)
		return ERR_SDK_NOT_READY;

	return TopicReader_CheckIsAsyncRequestReplied(pstOperatingModeCtrl->pstOperatingModeReplyReader, notifyEventfd);
}

int OperatingMode_CancelAsyncRequest(AHEADFORM_OPERATING_MODE_CTRL *pstOperatingModeCtrl, int notifyEventfd)
{
	if (pstOperatingModeCtrl == NULL)
		return ERR_ARGV_1;

	if (pstOperatingModeCtrl->pstOperatingModeReplyReader == NULL)
		return ERR_SDK_NOT_READY;

	return TopicReader_CancelAsyncRequest(pstOperatingModeCtrl->pstOperatingModeReplyReader, notifyEventfd);
}

int OperatingMode_MatchReplyWithReq(const void *pReplyElem, const void *pRequest)
{
	const AHEADFORM_OPERATING_MODE_REPLY *pstReply = (const AHEADFORM_OPERATING_MODE_REPLY*)pReplyElem;
	AHEADFORM_OPERATING_MODE_ASK stRequest;

	if (pstReply == NULL)
		return ERR_ARGV_1;

	if (pRequest == NULL)
		return ERR_ARGV_2;

	memcpy(&stRequest, pRequest, sizeof(stRequest));
	if (pstReply->requestId != stRequest.requestId)
		return ERRCODE_MATCH_REPLY_WITH_REQ_FAIL;

	return RET_SUCCESS;
}

int OperatingMode_SubmitAsyncRequestAndWaitReply(AHEADFORM_OPERATING_MODE_CTRL *pstOperatingModeCtrl, const AHEADFORM_OPERATING_MODE_ASK *pstAsk, AHEADFORM_OPERATING_MODE_REPLY *pstReply, int timeout_ms)
{
	eventfd_t eventCnt;
	struct timeval stTimeout;
	fd_set rdFdSet;
	int notifyEventfd, max_fd, cleanupRet, ret;

	if (pstOperatingModeCtrl == NULL)
		return ERR_ARGV_1;

	if (pstAsk == NULL)
		return ERR_ARGV_2;

	if (pstReply == NULL)
		return ERR_ARGV_3;

	if (timeout_ms <= 0)
		return ERR_ARGV_4;

	if (pstOperatingModeCtrl->pstOperatingModeReplyReader == NULL || pstOperatingModeCtrl->pstOperatingModeCtrlWriter == NULL)
		return ERR_SDK_NOT_READY;

	if ( (notifyEventfd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC)) <= 0 )
	{
		printf("%s() call eventfd(operating mode completion) failed: %s\r\n", __FUNCTION__, strerror(errno));
		return ERR_EVENTFD_CREATE_FAILED;
	}

	if ( (ret = OperatingMode_SubmitAsyncRequest(pstOperatingModeCtrl, pstAsk, (unsigned long long)timeout_ms * 1000, notifyEventfd, pstReply)) != RET_SUCCESS )
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
		if ( (cleanupRet = OperatingMode_CancelAsyncRequest(pstOperatingModeCtrl, notifyEventfd)) != RET_SUCCESS )
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

int OperatingMode_ReadStateSync(AHEADFORM_OPERATING_MODE_CTRL *pstOperatingModeCtrl, AHEADFORM_OPERATING_MODE_STATE *pstState, int timeout_ms)
{
	AHEADFORM_OPERATING_MODE_STATE stState;
	AHEADFORM_TOPIC_READER *pstReader = NULL;
	eventfd_t eventCnt;
	struct timeval stTimeout;
	fd_set rdFdSet;
	int max_fd, ret;

	if (pstOperatingModeCtrl == NULL)
		return ERR_ARGV_1;

	if (pstState == NULL)
		return ERR_ARGV_2;

	if (timeout_ms <= 0)
		return ERR_ARGV_3;

	if ( (pstReader = pstOperatingModeCtrl->pstOperatingModeStateReader) == NULL )
		return ERR_SDK_NOT_READY;

	if (eventfd_read(pstReader->rxDataUpdateEventfd, &eventCnt) < 0 && errno != EAGAIN)
	{
		printf("%s() call eventfd_read(rxDataUpdateEventfd = %d) failed: %s\r\n", __FUNCTION__, pstReader->rxDataUpdateEventfd, strerror(errno));
		return ERR_EVENTFD_IO_FAILED;
	}

	if ( (ret = TopicReader_DropOldElems(pstReader, 0)) < 0 )
		return ret;

	stTimeout.tv_sec = timeout_ms / 1000;
	stTimeout.tv_usec = timeout_ms % 1000 * 1000;
	max_fd = pstReader->rxDataUpdateEventfd;
	while (1)
	{
		FD_ZERO(&rdFdSet);
		FD_SET(pstReader->rxDataUpdateEventfd, &rdFdSet);
		if ( (ret = select(max_fd + 1, &rdFdSet, NULL, NULL, &stTimeout)) < 0 )
		{
			printf("%s() call select(rxDataUpdateEventfd = %d) failed: %s\r\n", __FUNCTION__, pstReader->rxDataUpdateEventfd, strerror(errno));
			return ERR_SELECT;
		}
		else if (ret == 0)
		{
			return ERR_REQUEST_TIMEOUT;
		}

		if (eventfd_read(pstReader->rxDataUpdateEventfd, &eventCnt) < 0 && errno != EAGAIN)
		{
			printf("%s() call eventfd_read(rxDataUpdateEventfd = %d) failed: %s\r\n", __FUNCTION__, pstReader->rxDataUpdateEventfd, strerror(errno));
			return ERR_EVENTFD_IO_FAILED;
		}

		if ( (ret = TopicReader_PopElem(pstReader, &stState)) < 0 )
			return ret;

		if (ret > 0)
		{
			if (stState.mode != AHEADFORM_OPERATING_MODE_DEV && stState.mode != AHEADFORM_OPERATING_MODE_INTERACTIVE)
				return ERR_DATA_PACKET_FORMAT;

			*pstState = stState;
			return RET_SUCCESS;
		}

		if (stTimeout.tv_sec == 0 && stTimeout.tv_usec == 0)
			return ERR_REQUEST_TIMEOUT;
	}
}
