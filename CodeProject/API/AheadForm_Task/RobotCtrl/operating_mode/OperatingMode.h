#ifndef __AHEADFORM_OPERATING_MODE_H__
#define __AHEADFORM_OPERATING_MODE_H__

#include "AheadFormTopics.h"

typedef struct
{
	AHEADFORM_TOPICS *pstTopics;
	unsigned long long nextOperatingModeRequestId;
	AHEADFORM_TOPIC_READER *pstOperatingModeReplyReader;
	AHEADFORM_TOPIC_WRITER *pstOperatingModeCtrlWriter;
	AHEADFORM_TOPIC_READER *pstOperatingModeStateReader;
} AHEADFORM_OPERATING_MODE_CTRL;

/*
	pstTopics must be successfully initialized first.
	Serialize lifecycle operations; stop customer Reader use and cancel unfinished async requests before deinitialization.
	Do not copy active handles or unregister internal references through generic Topic APIs.
*/
int OperatingMode_Init(AHEADFORM_OPERATING_MODE_CTRL *pstOperatingModeCtrl, AHEADFORM_TOPICS *pstTopics);
int OperatingMode_Deinit(AHEADFORM_OPERATING_MODE_CTRL *pstOperatingModeCtrl);

/*
	Receives all replies without request filtering; register before publishing.
	Unregister waits for in-flight callbacks; the internal Reader is owned by this module.
*/
AHEADFORM_TOPIC_READER* OperatingMode_RegisterReader(AHEADFORM_OPERATING_MODE_CTRL *pstOperatingModeCtrl, const AHEADFORM_TOPIC_READER_PARA *pstReaderPara);
int OperatingMode_UnregisterReader(AHEADFORM_OPERATING_MODE_CTRL *pstOperatingModeCtrl, AHEADFORM_TOPIC_READER *pstReader);

/*
	Build a new nonzero requestId from this module instance's independent atomic counter.
	Only DEV and INTERACTIVE are accepted.
	Request IDs do not provide uniqueness across instances, processes or restarts.
*/
int OperatingMode_BuildRequest(AHEADFORM_OPERATING_MODE_CTRL *pstOperatingModeCtrl, ENUM_AHEADFORM_OPERATING_MODE mode, AHEADFORM_OPERATING_MODE_ASK *pstRequest);

/*
	Success only means local DDS publication.
	Prepare reply handling before sending; request storage is borrowed during this call.
*/
int OperatingMode_SendRequest(AHEADFORM_OPERATING_MODE_CTRL *pstOperatingModeCtrl, const AHEADFORM_OPERATING_MODE_ASK *pstAsk);

/*
	Register before publishing and copy the request matching bytes; success only means registration and local publication.
	The caller owns an empty dedicated EFD_NONBLOCK | EFD_CLOEXEC eventfd and independent reply storage.
	Keep both valid until completion notification is observed, Check returns 1 or ERRCODE_REQUEST_TIMEOUT, or Cancel succeeds.
	Read pstReply after completion notification or Check returns 1; Check is optional.
	Check returns 1 for any matched result, including FAILED or an unknown value, 0 while pending, or an error.
	Check does not consume notification; if used, call it before clearing notifyEventfd.
	Timeout does not write notifyEventfd; a wait timeout alone does not end SDK access.
*/
int OperatingMode_SubmitAsyncRequest(AHEADFORM_OPERATING_MODE_CTRL *pstOperatingModeCtrl, const AHEADFORM_OPERATING_MODE_ASK *pstAsk, unsigned long long MatchReplyTimeout_us, int notifyEventfd, AHEADFORM_OPERATING_MODE_REPLY *pstReply);
int OperatingMode_CheckIsAsyncRequestReplied(AHEADFORM_OPERATING_MODE_CTRL *pstOperatingModeCtrl, int notifyEventfd);

/*
	Idempotent; success ends SDK access to the reply buffer and notification eventfd.
	Does not consume or close notifyEventfd and does not undo the remote request.
*/
int OperatingMode_CancelAsyncRequest(AHEADFORM_OPERATING_MODE_CTRL *pstOperatingModeCtrl, int notifyEventfd);

/*
	Match requestId only; result is not checked.
	Return RET_SUCCESS, ERRCODE_MATCH_REPLY_WITH_REQ_FAIL or ERR_ARGV_1/2.
	pRequest provides sizeof(AHEADFORM_OPERATING_MODE_ASK) readable request bytes.
*/
int OperatingMode_MatchReplyWithReq(const void *pReplyElem, const void *pRequest);

/*
	Submit an asynchronous request and wait in the calling thread using select and FD_ISSET on a dedicated completion eventfd.
	RET_SUCCESS means a matching reply was received; inspect pstReply->result for the remote result.
	The SDK writes pstReply directly; only RET_SUCCESS makes it usable. Other returns leave its contents unspecified.
	Does not consume the internal Reader FIFO or its rxDataUpdateEventfd.
	timeout_ms is a relative reply wait, not a strict limit on total call duration.
	Timeout or cancellation does not undo the published request.
*/
int OperatingMode_SubmitAsyncRequestAndWaitReply(AHEADFORM_OPERATING_MODE_CTRL *pstOperatingModeCtrl, const AHEADFORM_OPERATING_MODE_ASK *pstAsk, AHEADFORM_OPERATING_MODE_REPLY *pstReply, int timeout_ms);

/*
	Serialize consumers of this module's internal state Reader.
	Clear notification and discard unread history, then wait for a newly received state.
	This does not publish a request or wait for the mode to change.
	DEV/INTERACTIVE returns RET_SUCCESS; UNKNOWN returns ERRCODE_DATA_PACKET_FORMAT.
	All failures leave pstState unchanged.
	ReceiveTime_us is local receipt time, not remote generation time.
*/
int OperatingMode_ReadStateSync(AHEADFORM_OPERATING_MODE_CTRL *pstOperatingModeCtrl, AHEADFORM_OPERATING_MODE_STATE *pstState, int timeout_ms);

#endif
