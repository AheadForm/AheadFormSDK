#ifndef __AHEADFORM_AUDIO_H__
#define __AHEADFORM_AUDIO_H__

#include "AheadFormTopics.h"
#include "AudioFile.h"

#include <gst/gst.h>
#include <pthread.h>

#define AHEADFORM_AUDIO_SAMPLE_RATE_HZ 16000
#define AHEADFORM_AUDIO_CHANNEL_CNT 1
#define AHEADFORM_AUDIO_PCM_FRAME_SIZE 640
#define AHEADFORM_AUDIO_FRAME_DURATION_NS 20000000
#define AHEADFORM_SDK_PLAY_AUDIO_QUEUE_FRAME_CNT_MAX 10

typedef struct
{
	unsigned long long frameSeq;
	unsigned long long pts_ns;
	unsigned long long ReceiveTime_us;
	unsigned char isDiscontinuous;
	unsigned char pcmData[AHEADFORM_AUDIO_PCM_FRAME_SIZE];
} AHEADFORM_MIC_FRAME;

typedef struct MIC_READER_NODE
{
	SPMC_RING_FIFO_READER stFifoReader;
	int rxDataUpdateEventfd;
	unsigned long long lastMissedFrameCnt;
	struct MIC_READER_NODE *pNext;
} AHEADFORM_MIC_READER;

typedef struct
{
	GstElement *pPipeline;
	SPMC_RING_FIFO stFifo;
	AHEADFORM_MIC_FRAME *pstElemAry;
	AHEADFORM_MIC_READER *pstReaders;
	pthread_mutex_t readerListMutex;
	AHEADFORM_MIC_FRAME stFrame;
	unsigned int pcmByteCnt;
	GstClockTime expectedPts_ns;
	unsigned char isDiscontinuous;
	int lastError;
} AHEADFORM_MIC_CAPTURE;

typedef struct
{
	GstElement *pPipeline;
	GstElement *pSource;
	pthread_mutex_t writeMutex;
	GstClockTime nextPts_ns;
	int lastError;
} AHEADFORM_SDK_PLAY_AUDIO;

typedef struct
{
	AHEADFORM_TOPICS *pstTopics;
	AHEADFORM_TOPIC_READER *pstVolumeReplyReader;
	AHEADFORM_TOPIC_WRITER *pstVolumeAskWriter;
	AHEADFORM_TOPIC_READER *pstAudioDeviceReplyReader;
	AHEADFORM_TOPIC_WRITER *pstAudioDeviceAskWriter;
	AHEADFORM_MEDIA_CFG stCfg;
	char strNetworkInterface[AHEADFORM_NETWORK_INTERFACE_LEN_MAX];
	AHEADFORM_MIC_CAPTURE stMicCapture;
	AHEADFORM_SDK_PLAY_AUDIO stSdkPlayAudio;
} AHEADFORM_AUDIO_CTRL;

typedef struct
{
	AHEADFORM_VOLUME stVolume;
	//Nonzero selects the corresponding volume for this SET; zero leaves it unchanged.
	unsigned char isSetMicVolume;
	unsigned char isSetSpkVolume;
} AHEADFORM_VOLUME_SET_PARA;

/*
	pstTopics and pstCfg must be successfully initialized first; configuration is copied.
	Runtime APIs require successful Init; on Init failure, the application reports the error and exits without Deinit.
	After teardown begins, Deinit errors are terminal; do not retry Deinit or use the partially released handle.
*/
int Audio_Init(AHEADFORM_AUDIO_CTRL *pstAudioCtrl, AHEADFORM_TOPICS *pstTopics, const AHEADFORM_INIT_CONFIG *pstCfg);
int Audio_Deinit(AHEADFORM_AUDIO_CTRL *pstAudioCtrl);
//Receives all volume replies without request filtering; register before publishing.
AHEADFORM_TOPIC_READER* Audio_RegisterVolumeReader(AHEADFORM_AUDIO_CTRL *pstAudioCtrl, const AHEADFORM_TOPIC_READER_PARA *pstReaderPara);
int Audio_UnregisterVolumeReader(AHEADFORM_AUDIO_CTRL *pstAudioCtrl, AHEADFORM_TOPIC_READER *pstReader);
//Each call assigns a new nonzero askId.
int Audio_BuildQueryVolumeRequest(AHEADFORM_AUDIO_CTRL *pstAudioCtrl, AHEADFORM_VOLUME_ASK *pstRequest);
int Audio_BuildSetVolumeRequest(AHEADFORM_AUDIO_CTRL *pstAudioCtrl, const AHEADFORM_VOLUME_SET_PARA *pstSetPara, AHEADFORM_VOLUME_ASK *pstRequest);

/*
	Prepare reply handling before publishing; success only means local DDS publication succeeded.
	Does not register reply matching, wait for a reply or retain pstAsk.
*/
int Audio_SendVolumeRequest(AHEADFORM_AUDIO_CTRL *pstAudioCtrl, const AHEADFORM_VOLUME_ASK *pstAsk);

/*
	MatchReplyTimeout_us starts at Topic registration after request construction and includes topicMutex wait time.
	The SDK saves the request matching data; pstSetPara is only needed during the SET call.
	The caller owns an empty dedicated EFD_NONBLOCK | EFD_CLOEXEC eventfd and independent reply storage.
	Keep both valid until completion notification is observed, Check returns 1 or ERRCODE_REQUEST_TIMEOUT, or Cancel succeeds.
	Read pstReply after completion notification or Check returns 1; Check is optional.
	Timeout does not write notifyEventfd; a wait timeout alone does not end SDK access.
*/
int Audio_QueryVolumeAsync(AHEADFORM_AUDIO_CTRL *pstAudioCtrl, unsigned long long MatchReplyTimeout_us, int notifyEventfd, AHEADFORM_VOLUME_REPLY *pstReply);
int Audio_SetVolumeAsync(AHEADFORM_AUDIO_CTRL *pstAudioCtrl, const AHEADFORM_VOLUME_SET_PARA *pstSetPara, unsigned long long MatchReplyTimeout_us, int notifyEventfd, AHEADFORM_VOLUME_REPLY *pstReply);
/*
	Return 1 for a matched reply (including rejection), 0 while pending, or an error.
	Optional state query; completion notification already makes the reply readable.
	If used, check before clearing notifyEventfd; this function does not consume the notification.
	Inspect reply retCode for robot acceptance.
*/
int Audio_CheckIsVolumeAsyncRequestReplied(AHEADFORM_AUDIO_CTRL *pstAudioCtrl, int notifyEventfd);
//Idempotent; does not read or close notifyEventfd.
int Audio_CancelVolumeAsyncRequest(AHEADFORM_AUDIO_CTRL *pstAudioCtrl, int notifyEventfd);

/*
	Match sourceName, askId and operation; pRequest provides sizeof(AHEADFORM_VOLUME_ASK) readable request bytes.
	Return RET_SUCCESS for a match or ERRCODE_MATCH_REPLY_WITH_REQ_FAIL for a mismatch.
	NULL arguments return ERR_ARGV_1 or ERR_ARGV_2; reply retCode is not checked.
*/
int Audio_MatchVolumeReplyWithReq(const void *pReplyElem, const void *pRequest);
/*
	Requires a successful GET with valid finite volumes in [0,100]; failure leaves pstVolume unchanged.
	Match manually received replies before decoding; request identity is not checked here.
*/
int Audio_GetVolumeFromReply(const AHEADFORM_VOLUME_REPLY *pstReply, AHEADFORM_VOLUME *pstVolume);
/*
	Submit an asynchronous GET and wait in the calling thread using select and FD_ISSET on a dedicated completion eventfd.
	RET_SUCCESS means a matching reply was received; this function does not decode or validate the remote result.
	The SDK writes pstReply directly; only RET_SUCCESS makes it usable. Other returns leave its contents unspecified.
	Call Audio_GetVolumeFromReply separately when decoded volume values are required.
	Does not consume the internal Reader FIFO or its rxDataUpdateEventfd.
	timeout_ms is a relative reply wait, not a strict limit on total call duration.
	Timeout or cancellation does not undo the published request.
*/
int Audio_QueryVolumeAndWaitReply(AHEADFORM_AUDIO_CTRL *pstAudioCtrl, AHEADFORM_VOLUME_REPLY *pstReply, int timeout_ms);

//Receives all replies without request filtering; register before publishing.
AHEADFORM_TOPIC_READER* Audio_RegisterAudioDeviceReader(AHEADFORM_AUDIO_CTRL *pstAudioCtrl, const AHEADFORM_TOPIC_READER_PARA *pstReaderPara);
int Audio_UnregisterAudioDeviceReader(AHEADFORM_AUDIO_CTRL *pstAudioCtrl, AHEADFORM_TOPIC_READER *pstReader);
int Audio_BuildQueryAudioDeviceRequest(AHEADFORM_AUDIO_CTRL *pstAudioCtrl, ENUM_AHEADFORM_AUDIO_DEVICE_OPERATION operation, ENUM_AHEADFORM_AUDIO_DEVICE_TYPE deviceType, AHEADFORM_AUDIO_DEVICE_ASK *pstRequest);
int Audio_BuildRouteAudioDeviceRequest(AHEADFORM_AUDIO_CTRL *pstAudioCtrl, const AHEADFORM_AUDIO_DEVICE_ROUTE *pstRoute, AHEADFORM_AUDIO_DEVICE_ASK *pstRequest);
//Success only means local DDS publication; prepare reply handling before sending.
int Audio_SendAudioDeviceRequest(AHEADFORM_AUDIO_CTRL *pstAudioCtrl, const AHEADFORM_AUDIO_DEVICE_ASK *pstAsk);
/*
	Register before publishing and copy the request matching bytes; success only means registration and local publication.
	The caller owns an empty dedicated EFD_NONBLOCK | EFD_CLOEXEC eventfd and independent reply storage.
	Keep both valid until completion notification is observed, Check returns 1 or ERRCODE_REQUEST_TIMEOUT, or Cancel succeeds.
	Read pstReply after completion notification or Check returns 1; Check is optional.
	Check returns 1 for a matched reply including rejection, 0 while pending, or an error; it does not consume notification.
	If using Check, call it before clearing notifyEventfd.
	Timeout does not write notifyEventfd; a wait timeout alone does not end SDK access.
*/
int Audio_SubmitAudioDeviceAsyncRequest(AHEADFORM_AUDIO_CTRL *pstAudioCtrl, const AHEADFORM_AUDIO_DEVICE_ASK *pstAsk, unsigned long long MatchReplyTimeout_us, int notifyEventfd, AHEADFORM_AUDIO_DEVICE_REPLY *pstReply);
int Audio_CheckIsAudioDeviceAsyncRequestReplied(AHEADFORM_AUDIO_CTRL *pstAudioCtrl, int notifyEventfd);
//Idempotent; does not consume or close notifyEventfd and does not undo robot control.
int Audio_CancelAudioDeviceAsyncRequest(AHEADFORM_AUDIO_CTRL *pstAudioCtrl, int notifyEventfd);
/*
	Match sourceName, askId and operation; queries additionally match deviceType, ROUTE ignores deviceType.
	Return RET_SUCCESS, ERRCODE_MATCH_REPLY_WITH_REQ_FAIL or ERR_ARGV_1/2; reply retCode is not checked.
	pRequest provides sizeof(AHEADFORM_AUDIO_DEVICE_ASK) readable request bytes.
*/
int Audio_MatchAudioDeviceReplyWithReq(const void *pReplyElem, const void *pRequest);
/*
	Submit an asynchronous request and wait in the calling thread using select and FD_ISSET on a dedicated completion eventfd.
	RET_SUCCESS means a matching reply was received; inspect pstReply->retCode for the remote result.
	The SDK writes pstReply directly; only RET_SUCCESS makes it usable. Other returns leave its contents unspecified.
	Does not consume the internal Reader FIFO or its rxDataUpdateEventfd.
	timeout_ms is a relative reply wait, not a strict limit on total call duration.
	Timeout or cancellation does not undo the published request.
*/
int Audio_SubmitAudioDeviceAsyncRequestAndWaitReply(AHEADFORM_AUDIO_CTRL *pstAudioCtrl, const AHEADFORM_AUDIO_DEVICE_ASK *pstAsk, AHEADFORM_AUDIO_DEVICE_REPLY *pstReply, int timeout_ms);

/*
	Capture obtains a copy of the robot MIC stream; it does not control robot devices or AI.
	Lifecycle and registration calls are serialized by the caller and run outside callbacks.
	Do not copy active handles or modify SDK runtime members.
	Stop retains the FIFO and Readers; Deinit returns ERRCODE_RESOURCE_BUSY before teardown while customer MIC Readers remain.
*/
int Audio_StartMicCapture(AHEADFORM_AUDIO_CTRL *pstAudioCtrl);
int Audio_StopMicCapture(AHEADFORM_AUDIO_CTRL *pstAudioCtrl);
int Audio_GetMicCaptureError(const AHEADFORM_AUDIO_CTRL *pstAudioCtrl);

/*
	Register after the first successful capture Start; starts at the current writer position.
	SDK owns rxDataUpdateEventfd; clear it before draining. One sequential consumer per Reader.
	Stop consuming and waiting before unregister. Notification count is only an update hint.
*/
AHEADFORM_MIC_READER* Audio_RegisterMicReader(AHEADFORM_AUDIO_CTRL *pstAudioCtrl);
int Audio_UnregisterMicReader(AHEADFORM_AUDIO_CTRL *pstAudioCtrl, AHEADFORM_MIC_READER *pstReader);

/*
	One frame contains 640 bytes of 16000 Hz mono S16LE PCM, lasting 20 ms.
	Return 1, 0 when empty, or an error. Only 1 makes output valid.
	pts_ns is stream time or ULLONG_MAX; ReceiveTime_us is local monotonic arrival time.
	Known gaps discard partial frames without inserting silence; missed counts are Reader-local.
*/
int Audio_MicReader_PopFrame(AHEADFORM_MIC_READER *pstReader, AHEADFORM_MIC_FRAME *pstRetFrame);
int Audio_MicReader_DropOldFrames(AHEADFORM_MIC_READER *pstReader, unsigned int retainedFreshFrameCnt);
unsigned int Audio_MicReader_GetReadableFrameCnt(AHEADFORM_MIC_READER *pstReader);
unsigned long long Audio_MicReader_GetMissedFrameCnt(AHEADFORM_MIC_READER *pstReader);

/*
	SDK playback sends customer audio; it does not implicitly control robot devices or routing.
	Each Write copies exactly 640 bytes of S16LE PCM before returning. Success means local enqueue only.
	appsrc holds at most 10 waiting frames plus downstream in-flight data; a full queue returns BUSY.
	Writes on the same handle are serialized. Stop discards pending data and coordinates with Write.
*/
int Audio_StartSdkPlayAudio(AHEADFORM_AUDIO_CTRL *pstAudioCtrl);
int Audio_StopSdkPlayAudio(AHEADFORM_AUDIO_CTRL *pstAudioCtrl);
int Audio_GetSdkPlayAudioError(const AHEADFORM_AUDIO_CTRL *pstAudioCtrl);
int Audio_WriteSdkPlayPcm(AHEADFORM_AUDIO_CTRL *pstAudioCtrl, const unsigned char *pPcmData, size_t dataLen);

#endif
