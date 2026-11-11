#ifndef __AHEADFORM_MEDIA_H__
#define __AHEADFORM_MEDIA_H__

#include "AheadFormTopics.h"

#define AHEADFORM_MEDIA_FIFO_ELEM_CNT 1000
#define AHEADFORM_PCM_SAMPLE_RATE 16000
#define AHEADFORM_PCM_CHANNEL_CNT 1
#define AHEADFORM_PCM_FRAME_SAMPLE_CNT 320
#define AHEADFORM_PCM_FRAME_SIZE (AHEADFORM_PCM_FRAME_SAMPLE_CNT * 2)
#define AHEADFORM_MIC_FRAME_SIZE_MAX 65536
#define AHEADFORM_VIDEO_FRAME_SIZE_MAX (128 * 1024)
#define AHEADFORM_SPEAKER_QUEUED_FRAME_CNT_MAX 10

typedef enum
{
	AHEADFORM_MEDIA_STREAM_VIDEO = 0,
	AHEADFORM_MEDIA_STREAM_MIC,
	AHEADFORM_MEDIA_STREAM_CNT
} ENUM_AHEADFORM_MEDIA_STREAM;

typedef enum
{
	AHEADFORM_MEDIA_FORMAT_H264_ANNEX_B = 1,
	AHEADFORM_MEDIA_FORMAT_PCM_S16LE
} ENUM_AHEADFORM_MEDIA_FORMAT;

typedef struct
{
	unsigned long long ReceiveTime_us;
	unsigned long long sequence;
	unsigned long long pts_ns;
	unsigned long long ptsEpoch_us;
	ENUM_AHEADFORM_MEDIA_FORMAT format;
	unsigned int dataSize;
	unsigned int width;
	unsigned int height;
	unsigned int sampleRate;
	unsigned int channelCnt;
	unsigned int sampleCnt;
	unsigned char isPtsValid;
	unsigned char isKeyframe;
	unsigned char hasParameterSets;
	unsigned char isDiscontinuous;
	unsigned char data[];
} AHEADFORM_MEDIA_FRAME;

typedef void (*AHEADFORM_MEDIA_FRAME_CALLBACK)(const AHEADFORM_MEDIA_FRAME *pstFrame, unsigned char isReaderDiscontinuous, void *pUserArg);

typedef struct
{
	AHEADFORM_MEDIA_FRAME_CALLBACK ReaderCallback;
	void *pUserArg;
} AHEADFORM_MEDIA_READER_PARA;

typedef struct AHEADFORM_MEDIA_READER AHEADFORM_MEDIA_READER;

typedef struct
{
	FILE *pFile;
	unsigned int dataSize;
	unsigned int remainingSize;
	unsigned char isWriting;
} AHEADFORM_WAV_FILE;

typedef struct
{
	FILE *pDataFile;
	FILE *pTimeFile;
	unsigned long long dataOffset;
	unsigned char isWaitingForIdr;
} AHEADFORM_H264_FILE;

typedef struct
{
	unsigned char operation;
	unsigned char deviceType;
	unsigned char hasMicDevice;
	char micDeviceId[AHEADFORM_AUDIO_DEVICE_ID_LEN_MAX];
	unsigned char hasSpkDevice;
	char spkDeviceId[AHEADFORM_AUDIO_DEVICE_ID_LEN_MAX];
} AHEADFORM_AUDIO_DEVICE_PARA;

typedef struct
{
	unsigned char videoCtrl;
	unsigned char micCtrl;
	unsigned char playbackCtrl;
	unsigned char vadCtrl;
} AHEADFORM_MEDIA_CHANNEL_PARA;

typedef struct
{
	AHEADFORM_TOPICS *pstTopics;
	AHEADFORM_TOPIC_READER *pstVolumeReplyReader;
	AHEADFORM_TOPIC_WRITER *pstVolumeAskWriter;
	AHEADFORM_TOPIC_WRITER *pstAudioDeviceAskWriter;
	AHEADFORM_TOPIC_READER *pstAudioDeviceReplyReader;
	AHEADFORM_TOPIC_WRITER *pstChannelAskWriter;
	AHEADFORM_TOPIC_READER *pstChannelReplyReader;
	AHEADFORM_MEDIA_CFG stCfg;
	struct AHEADFORM_MEDIA_STREAMS *pstStreams;
} AHEADFORM_MEDIA_CTRL;

typedef struct
{
	AHEADFORM_VOLUME stVolume;
	//Nonzero selects the corresponding volume for this SET; zero leaves it unchanged.
	unsigned char isSetMicVolume;
	unsigned char isSetSpkVolume;
} AHEADFORM_VOLUME_SET_PARA;

//pstTopics must be successfully initialized first.
int Media_Init(AHEADFORM_MEDIA_CTRL *pstMediaCtrl, AHEADFORM_TOPICS *pstTopics, const AHEADFORM_MEDIA_CFG *pstMediaCfg);
int Media_Deinit(AHEADFORM_MEDIA_CTRL *pstMediaCtrl);
//Receives all volume replies without request filtering; register before publishing.
AHEADFORM_TOPIC_READER* Media_RegisterVolumeReader(AHEADFORM_MEDIA_CTRL *pstMediaCtrl, const AHEADFORM_TOPIC_READER_PARA *pstReaderPara);
int Media_UnregisterVolumeReader(AHEADFORM_MEDIA_CTRL *pstMediaCtrl, AHEADFORM_TOPIC_READER *pstReader);
//Each call assigns a new nonzero askId.
int Media_BuildQueryVolumeRequest(AHEADFORM_MEDIA_CTRL *pstMediaCtrl, AHEADFORM_VOLUME_ASK *pstRequest);
int Media_BuildSetVolumeRequest(AHEADFORM_MEDIA_CTRL *pstMediaCtrl, const AHEADFORM_VOLUME_SET_PARA *pstSetPara, AHEADFORM_VOLUME_ASK *pstRequest);

/*
	Prepare reply handling before publishing; success only means local DDS publication succeeded.
	Does not register reply matching, wait for a reply or retain pstAsk.
*/
int Media_SendVolumeRequest(AHEADFORM_MEDIA_CTRL *pstMediaCtrl, const AHEADFORM_VOLUME_ASK *pstAsk);

/*
	MatchReplyTimeout_us starts at Topic registration after request construction and includes topicMutex wait time.
	The SDK saves the request matching data; pstSetPara is only needed during the SET call.
	The caller owns an empty dedicated EFD_NONBLOCK | EFD_CLOEXEC eventfd and independent reply storage.
	Keep pstReply and the eventfd valid until Check returns 1 or ERRCODE_REQUEST_TIMEOUT, or Cancel succeeds.
	Read pstReply only after Check returns 1.
*/
int Media_QueryVolumeAsync(AHEADFORM_MEDIA_CTRL *pstMediaCtrl, unsigned long long MatchReplyTimeout_us, int notifyEventfd, AHEADFORM_VOLUME_REPLY_SAMPLE *pstReply);
int Media_SetVolumeAsync(AHEADFORM_MEDIA_CTRL *pstMediaCtrl, const AHEADFORM_VOLUME_SET_PARA *pstSetPara, unsigned long long MatchReplyTimeout_us, int notifyEventfd, AHEADFORM_VOLUME_REPLY_SAMPLE *pstReply);
/*
	Return 1 for a matched reply (including rejection), 0 while pending, or an error.
	Check before clearing notifyEventfd; this function does not consume the notification.
	Inspect reply retCode for robot acceptance.
*/
int Media_CheckIsVolumeAsyncRequestReplied(AHEADFORM_MEDIA_CTRL *pstMediaCtrl, int notifyEventfd);
//Idempotent; does not read or close notifyEventfd.
int Media_CancelVolumeAsyncRequest(AHEADFORM_MEDIA_CTRL *pstMediaCtrl, int notifyEventfd);

//Match sourceName, askId and operation; pRequest provides sizeof(AHEADFORM_VOLUME_ASK) readable request bytes.
int Media_MatchVolumeReplyWithReq(const void *pReplyElem, const void *pRequest);
/*
	Requires a successful GET with valid finite volumes in [0,100]; failure leaves pstVolume unchanged.
	Match manually received replies before decoding; request identity is not checked here.
*/
int Media_GetVolumeFromReply(const AHEADFORM_VOLUME_REPLY_SAMPLE *pstReply, AHEADFORM_VOLUME *pstVolume);
//timeout_ms is a relative reply timeout, not a strict limit on total call duration.
int Media_QueryVolumeSync(AHEADFORM_MEDIA_CTRL *pstMediaCtrl, AHEADFORM_VOLUME *pstVolume, int timeout_ms);

/*
	Build assigns identity; Send reads only during the call. Async builds, copies matching values and publishes.
	Completion storage/eventfd follow TopicReader_RegisterAsyncRequest; inspect the remote reply after Check returns 1.
	MatchReplyWithReq reads a complete request object. Register a customer Reader before publishing for manual matching.
*/
int Media_BuildAudioDeviceRequest(AHEADFORM_MEDIA_CTRL *pstMediaCtrl, const AHEADFORM_AUDIO_DEVICE_PARA *pstPara, AHEADFORM_AUDIO_DEVICE_ASK *pstRequest);
int Media_SendAudioDeviceRequest(AHEADFORM_MEDIA_CTRL *pstMediaCtrl, const AHEADFORM_AUDIO_DEVICE_ASK *pstRequest);
int Media_AudioDeviceAsync(AHEADFORM_MEDIA_CTRL *pstMediaCtrl, const AHEADFORM_AUDIO_DEVICE_PARA *pstPara, unsigned long long MatchReplyTimeout_us, int notifyEventfd, AHEADFORM_AUDIO_DEVICE_REPLY_SAMPLE *pstReply);
int Media_CheckIsAudioDeviceAsyncRequestReplied(AHEADFORM_MEDIA_CTRL *pstMediaCtrl, int notifyEventfd);
int Media_CancelAudioDeviceAsyncRequest(AHEADFORM_MEDIA_CTRL *pstMediaCtrl, int notifyEventfd);
AHEADFORM_TOPIC_READER* Media_RegisterAudioDeviceReader(AHEADFORM_MEDIA_CTRL *pstMediaCtrl, const AHEADFORM_TOPIC_READER_PARA *pstReaderPara);
int Media_UnregisterAudioDeviceReader(AHEADFORM_MEDIA_CTRL *pstMediaCtrl, AHEADFORM_TOPIC_READER *pstReader);
int Media_MatchAudioDeviceReplyWithReq(const void *pReplyElem, const void *pRequest);

int Media_BuildChannelRequest(AHEADFORM_MEDIA_CTRL *pstMediaCtrl, const AHEADFORM_MEDIA_CHANNEL_PARA *pstPara, AHEADFORM_MEDIA_CHANNEL_ASK *pstRequest);
int Media_SendChannelRequest(AHEADFORM_MEDIA_CTRL *pstMediaCtrl, const AHEADFORM_MEDIA_CHANNEL_ASK *pstRequest);
int Media_ChannelAsync(AHEADFORM_MEDIA_CTRL *pstMediaCtrl, const AHEADFORM_MEDIA_CHANNEL_PARA *pstPara, unsigned long long MatchReplyTimeout_us, int notifyEventfd, AHEADFORM_MEDIA_CHANNEL_REPLY_SAMPLE *pstReply);
int Media_CheckIsChannelAsyncRequestReplied(AHEADFORM_MEDIA_CTRL *pstMediaCtrl, int notifyEventfd);
int Media_CancelChannelAsyncRequest(AHEADFORM_MEDIA_CTRL *pstMediaCtrl, int notifyEventfd);
AHEADFORM_TOPIC_READER* Media_RegisterChannelReader(AHEADFORM_MEDIA_CTRL *pstMediaCtrl, const AHEADFORM_TOPIC_READER_PARA *pstReaderPara);
int Media_UnregisterChannelReader(AHEADFORM_MEDIA_CTRL *pstMediaCtrl, AHEADFORM_TOPIC_READER *pstReader);
int Media_MatchChannelReplyWithReq(const void *pReplyElem, const void *pRequest);

/*
	Init prepares bounded storage but does not start transport. Serialize Init/Deinit/Start/Stop/Register/Unregister.
	Stop keeps Readers and their FIFO alive, waits for streaming callbacks, and releases the native pipeline.
	If Start fails and its Stop cleanup also fails, retry Stop/Deinit before starting again.
	Callbacks run synchronously on GStreamer threads and borrow frame bytes only during the call.
	Keep pUserArg valid until successful unregister; do not unregister, stop or destroy a path inside its callback.
	A separate sequential consumer may drain each Reader without locking. The SPMC stable-copy contract applies.
*/
int Media_InitStreams(AHEADFORM_MEDIA_CTRL *pstMediaCtrl);
int Media_DeinitStreams(AHEADFORM_MEDIA_CTRL *pstMediaCtrl);
int Media_StartVideoReceive(AHEADFORM_MEDIA_CTRL *pstMediaCtrl);
int Media_StopVideoReceive(AHEADFORM_MEDIA_CTRL *pstMediaCtrl);
int Media_StartMicReceive(AHEADFORM_MEDIA_CTRL *pstMediaCtrl);
int Media_StopMicReceive(AHEADFORM_MEDIA_CTRL *pstMediaCtrl);
int Media_StartSpeakerSend(AHEADFORM_MEDIA_CTRL *pstMediaCtrl);
int Media_StopSpeakerSend(AHEADFORM_MEDIA_CTRL *pstMediaCtrl);
//Copy exactly one 20 ms, 16 kHz mono S16LE frame; full 10-frame appsrc queue returns ERRCODE_RESOURCE_BUSY.
int Media_WriteSpeakerPcmFrame(AHEADFORM_MEDIA_CTRL *pstMediaCtrl, const void *pPcm, unsigned int pcmSize);
//EOS waits for queued PCM to be sent locally; it cannot prove that the robot speaker rendered it.
int Media_FinishSpeakerSend(AHEADFORM_MEDIA_CTRL *pstMediaCtrl, unsigned int timeout_ms);
int Media_GetSpeakerSendResult(AHEADFORM_MEDIA_CTRL *pstMediaCtrl);

AHEADFORM_MEDIA_READER* Media_RegisterFrameReader(AHEADFORM_MEDIA_CTRL *pstMediaCtrl, ENUM_AHEADFORM_MEDIA_STREAM stream, const AHEADFORM_MEDIA_READER_PARA *pstReaderPara);
int Media_UnregisterFrameReader(AHEADFORM_MEDIA_CTRL *pstMediaCtrl, AHEADFORM_MEDIA_READER *pstReader);
//SDK owns this nonblocking eventfd; clear its frame/conversion hint before draining. Query transport errors with GetLastReceiveResult.
int MediaReader_GetEventfd(AHEADFORM_MEDIA_READER *pstReader);
unsigned int MediaReader_GetElemSize(AHEADFORM_MEDIA_READER *pstReader);
int MediaReader_GetLastReceiveResult(AHEADFORM_MEDIA_READER *pstReader);
unsigned int MediaReader_GetReadableElemCnt(AHEADFORM_MEDIA_READER *pstReader);
unsigned long long MediaReader_GetMissedElemCnt(AHEADFORM_MEDIA_READER *pstReader);
/*
	Return 1 for a copied frame, 0 when none, or an error. Caller capacity must cover GetElemSize().
	Video Pop resumes at an IDR with SPS/PPS after a network gap or this Reader losing frames.
	Latest video is for inspection: it may be a predictive AU and is marked discontinuous; use Pop for decoding.
	PTS is segment running time in ns, valid only within ptsEpoch_us (a pipeline epoch identifier).
*/
int MediaReader_PopFrame(AHEADFORM_MEDIA_READER *pstReader, AHEADFORM_MEDIA_FRAME *pstFrame, unsigned int frameBufSize);
int MediaReader_ReadLatestFrame(AHEADFORM_MEDIA_READER *pstReader, AHEADFORM_MEDIA_FRAME *pstFrame, unsigned int frameBufSize);

//Zero-initialize file handles before their first Open. Single-track WAV uses 16 kHz mono S16LE; Read returns bytes, zero at EOF, or an error.
int Media_OpenWavReader(const char *filePath, AHEADFORM_WAV_FILE *pstWav);
int Media_ReadWavPcm(AHEADFORM_WAV_FILE *pstWav, void *pPcm, unsigned int pcmBufSize);
int Media_OpenWavWriter(const char *filePath, AHEADFORM_WAV_FILE *pstWav);
int Media_WriteWavPcm(AHEADFORM_WAV_FILE *pstWav, const void *pPcm, unsigned int pcmSize);
int Media_CloseWav(AHEADFORM_WAV_FILE *pstWav);
//Decode an MP3 to owned 16 kHz mono S16LE bytes; caller frees *ppPcm after use. No robot access.
int Media_LoadMp3Pcm(const char *filePath, unsigned char **ppPcm, unsigned int *pPcmSize);
//H.264 AU bytes and a CSV time index are separate explicit files. Write returns 1 when saved, 0 while waiting for IDR; close after an IO error.
int Media_OpenH264Writer(const char *dataFilePath, const char *timeFilePath, AHEADFORM_H264_FILE *pstFile);
int Media_WriteH264Frame(AHEADFORM_H264_FILE *pstFile, const AHEADFORM_MEDIA_FRAME *pstFrame);
int Media_CloseH264Writer(AHEADFORM_H264_FILE *pstFile);

#endif
