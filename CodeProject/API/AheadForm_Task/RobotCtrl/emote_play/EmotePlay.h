#ifndef __AHEADFORM_EMOTE_PLAY_H__
#define __AHEADFORM_EMOTE_PLAY_H__

#include "AheadFormTopics.h"

typedef struct
{
	AHEADFORM_TOPICS *pstTopics;
	unsigned long long nextEmotePlayRequestId;
	AHEADFORM_TOPIC_READER *pstEmotePlayReplyReader;
	AHEADFORM_TOPIC_WRITER *pstEmotePlayCtrlWriter;
	AHEADFORM_TOPIC_READER *pstEmotePlayEventReader;
} AHEADFORM_EMOTE_PLAY_CTRL;

/*
	pstTopics must be successfully initialized first.
	Serialize lifecycle operations; stop customer Reader use and cancel unfinished async requests and state waits before deinitialization.
	Do not copy active handles or unregister internal references through generic Topic APIs.
*/
int EmotePlay_Init(AHEADFORM_EMOTE_PLAY_CTRL *pstEmotePlayCtrl, AHEADFORM_TOPICS *pstTopics);
int EmotePlay_Deinit(AHEADFORM_EMOTE_PLAY_CTRL *pstEmotePlayCtrl);

/*
	Readers receive all samples without request filtering; register before publishing when early replies or events are needed.
	Unregister waits for in-flight callbacks. Callback samples are borrowed only during the callback.
	The internal reply and event Readers belong to this module and cannot be unregistered through these customer interfaces.
*/
AHEADFORM_TOPIC_READER* EmotePlay_RegisterReplyReader(AHEADFORM_EMOTE_PLAY_CTRL *pstEmotePlayCtrl, const AHEADFORM_TOPIC_READER_PARA *pstReaderPara);
int EmotePlay_UnregisterReplyReader(AHEADFORM_EMOTE_PLAY_CTRL *pstEmotePlayCtrl, AHEADFORM_TOPIC_READER *pstReader);
AHEADFORM_TOPIC_READER* EmotePlay_RegisterEventReader(AHEADFORM_EMOTE_PLAY_CTRL *pstEmotePlayCtrl, const AHEADFORM_TOPIC_READER_PARA *pstReaderPara);
int EmotePlay_UnregisterEventReader(AHEADFORM_EMOTE_PLAY_CTRL *pstEmotePlayCtrl, AHEADFORM_TOPIC_READER *pstReader);

/*
	Build a new nonzero requestId from this module instance's independent atomic counter.
	Request IDs are not unique across instances, processes or restarts.
	PLAY copies the used clips into pstAsk; input arrays must not overlap pstAsk and are needed only during this call.
	Each track accepts at most 64 clips; at least one track is required. A zero count requires a NULL input pointer.
	sourceType applies to both tracks. NAME uses the complete robot-side filename including its suffix, copied verbatim.
	Time fields are forwarded in milliseconds; resource and timeline acceptance are determined by the robot.
	Build validates request shape; Send also validates used source strings and gap values.
	Unused array elements are not initialized. Only use the output after RET_SUCCESS.
*/
int EmotePlay_BuildPlayRequest(AHEADFORM_EMOTE_PLAY_CTRL *pstEmotePlayCtrl, ENUM_AHEADFORM_EMOTE_SOURCE_TYPE sourceType, const AHEADFORM_EMOTE_CLIP *pstMotionClips, unsigned int motionClipCnt,
	const AHEADFORM_EMOTE_CLIP *pstAudioClips, unsigned int audioClipCnt, unsigned char isLoop, AHEADFORM_EMOTE_ASK *pstAsk);

/*
	Only PAUSE, RESUME and STOP are accepted; each command has its own new requestId.
	Control requests contain no clips and do not initialize unused clip storage.
*/
int EmotePlay_BuildCtrlRequest(AHEADFORM_EMOTE_PLAY_CTRL *pstEmotePlayCtrl, ENUM_AHEADFORM_EMOTE_CMD cmd, AHEADFORM_EMOTE_ASK *pstAsk);

/*
	Success only means local DDS publication. Prepare reply/event handling before sending.
	Request storage is borrowed during this call; sources are copied verbatim without stripping suffixes.
	PLAY uses reset=true; other commands use reset=false. Start and finish actions are always NONE.
	This module does not load customer files, advance a playback timeline or claim output ownership.
*/
int EmotePlay_SendRequest(AHEADFORM_EMOTE_PLAY_CTRL *pstEmotePlayCtrl, const AHEADFORM_EMOTE_ASK *pstAsk);

/*
	Register before publishing; success only means registration and local publication.
	Matching copies requestId only. Request storage is needed only during submission.
	The caller owns an empty dedicated EFD_NONBLOCK | EFD_CLOEXEC eventfd and independent reply storage for each request.
	Keep both valid until completion notification is observed, Check returns 1 or ERRCODE_REQUEST_TIMEOUT, or Cancel succeeds.
	Read pstReply after completion notification or Check returns 1; Check is optional and must precede clearing the notification.
	Check returns 1 for any matched result, 0 while pending, or an error; it does not consume notification.
	Timeout does not write notifyEventfd; a wait timeout alone does not end SDK access.
	A matched reply is not proof that playback completed. Use independent reply/event Readers for subsequent samples.
*/
int EmotePlay_SubmitAsyncRequest(AHEADFORM_EMOTE_PLAY_CTRL *pstEmotePlayCtrl, const AHEADFORM_EMOTE_ASK *pstAsk, unsigned long long MatchReplyTimeout_us, int notifyEventfd, AHEADFORM_EMOTE_REPLY *pstReply);
int EmotePlay_CheckIsAsyncRequestReplied(AHEADFORM_EMOTE_PLAY_CTRL *pstEmotePlayCtrl, int notifyEventfd);

/*
	Idempotent; success ends SDK access to the reply buffer and notification eventfd.
	Does not consume or close notifyEventfd and does not undo the remote request.
*/
int EmotePlay_CancelAsyncRequest(AHEADFORM_EMOTE_PLAY_CTRL *pstEmotePlayCtrl, int notifyEventfd);

/*
	Register a get-state async request for one future state event on the internal Event Reader; this call does not wait or publish a request.
	Register before sending PLAY when early events must be captured. playRequestId identifies the original PLAY.
	Match requestId only, excluding RESOURCE_FILES_RELEASED; all other state strings are retained verbatim.
	Use independent event storage and an empty dedicated EFD_NONBLOCK | EFD_CLOEXEC eventfd for each request.
	Keep both valid until completion notification is observed, Check returns 1 or ERRCODE_REQUEST_TIMEOUT, or Cancel succeeds.
	MatchStateTimeout_us starts at registration; timeout does not write notifyEventfd.
	Read event storage after completion notification or Check returns 1.
	Check returns 1 for a matched state, 0 while pending, or an error; it does not consume notification.
	If used, Check must precede clearing notification.
	Cancel is idempotent, ends SDK access, and does not consume or close the eventfd.
	No internal Reader FIFO cursor or rxDataUpdateEventfd is consumed.
	A completed get-state request does not guarantee current state or playback completion; use a customer Reader for continuous observation.
*/
int EmotePlay_RegisterGetStateAsyncRequest(AHEADFORM_EMOTE_PLAY_CTRL *pstEmotePlayCtrl, unsigned long long playRequestId, unsigned long long MatchStateTimeout_us, int notifyEventfd, AHEADFORM_EMOTE_EVENT *pstEvent);
int EmotePlay_CheckIsGetStateAsyncRequestCompleted(AHEADFORM_EMOTE_PLAY_CTRL *pstEmotePlayCtrl, int notifyEventfd);
int EmotePlay_CancelGetStateAsyncRequest(AHEADFORM_EMOTE_PLAY_CTRL *pstEmotePlayCtrl, int notifyEventfd);

/*
	Wait in the calling thread using select and FD_ISSET on a dedicated completion eventfd.
	RET_SUCCESS means a matching reply was received; inspect pstReply->result for the remote result.
	The SDK writes pstReply directly; only RET_SUCCESS makes it usable. Other returns leave its contents unspecified.
	Does not consume the internal Reader FIFO or its rxDataUpdateEventfd.
	timeout_ms is a relative reply wait, not a strict limit on total call duration.
	Timeout or cancellation does not undo the published request.
*/
int EmotePlay_SubmitAsyncRequestAndWaitReply(AHEADFORM_EMOTE_PLAY_CTRL *pstEmotePlayCtrl, const AHEADFORM_EMOTE_ASK *pstAsk, AHEADFORM_EMOTE_REPLY *pstReply, int timeout_ms);

#endif
