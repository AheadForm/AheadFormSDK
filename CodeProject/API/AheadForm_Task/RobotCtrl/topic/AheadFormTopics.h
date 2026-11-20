#ifndef __AHEADFORM_TOPICS_H__
#define __AHEADFORM_TOPICS_H__

#include "DdsCtrl.h"
#include "ParseInitConfig.h"
#include "SPMCRingFifo.h"
#include "AheadFormTopicData.h"

#include <pthread.h>
#include <dds/dds.h>

#define AHEADFORM_VOLUME_REPLY_FIFO_ELEM_CNT 1129
#define AHEADFORM_SENSOR_STATE_FIFO_ELEM_CNT 1000
#define AHEADFORM_NECK_STATE_FIFO_ELEM_CNT 1000
#define AHEADFORM_FAULT_POOL_FIFO_ELEM_CNT 1000
#define AHEADFORM_AUDIO_DEVICE_REPLY_FIFO_ELEM_CNT 1000
#define AHEADFORM_OPERATING_MODE_REPLY_FIFO_ELEM_CNT 1000
#define AHEADFORM_OPERATING_MODE_STATE_FIFO_ELEM_CNT 1000
#define AHEADFORM_EMOTE_REPLY_FIFO_ELEM_CNT 1000
#define AHEADFORM_EMOTE_EVENT_FIFO_ELEM_CNT 1000
#define AHEADFORM_TOPIC_ASYNC_REQUEST_CNT_MAX 6
#define AHEADFORM_TOPIC_MATCH_FUNC_ARG_SIZE_MAX 128
#define AHEADFORM_ROBOT_DDS_DISCOVERY_TIMEOUT_MS 1000

typedef enum
{
	AHEADFORM_TOPIC_ID_INVALID = -1,
	AHEADFORM_TOPIC_ID_VOLUME_ASK,
	AHEADFORM_TOPIC_ID_VOLUME_REPLY,
	AHEADFORM_TOPIC_ID_MOTION_POSE,
	AHEADFORM_TOPIC_ID_NECK_POSE,
	AHEADFORM_TOPIC_ID_FACE,
	AHEADFORM_TOPIC_ID_SERVO_TARGETS,
	AHEADFORM_TOPIC_ID_SENSOR_STATE,
	AHEADFORM_TOPIC_ID_NECK_STATE,
	AHEADFORM_TOPIC_ID_FAULT_POOL,
	AHEADFORM_TOPIC_ID_AUDIO_DEVICE_ASK,
	AHEADFORM_TOPIC_ID_AUDIO_DEVICE_REPLY,
	AHEADFORM_TOPIC_ID_OPERATING_MODE_CTRL,
	AHEADFORM_TOPIC_ID_OPERATING_MODE_REPLY,
	AHEADFORM_TOPIC_ID_OPERATING_MODE_STATE,
	AHEADFORM_TOPIC_ID_EMOTE_CTRL,
	AHEADFORM_TOPIC_ID_EMOTE_REPLY,
	AHEADFORM_TOPIC_ID_EMOTE_EVENT,
	AHEADFORM_TOPIC_ID_CNT
} ENUM_AHEADFORM_TOPIC_ID;

typedef enum
{
	AHEADFORM_TOPIC_DIRECTION_INVALID = 0,
	AHEADFORM_TOPIC_DIRECTION_PUBLISH = 1 << 0,
	AHEADFORM_TOPIC_DIRECTION_SUBSCRIBE = 1 << 1,
	AHEADFORM_TOPIC_DIRECTION_PUBLISH_SUBSCRIBE = AHEADFORM_TOPIC_DIRECTION_PUBLISH | AHEADFORM_TOPIC_DIRECTION_SUBSCRIBE
} ENUM_AHEADFORM_TOPIC_DIRECTION;

typedef void (*AHEADFORM_TOPIC_READER_CALLBACK)(const void *pElem, void *pUserArg);
/*
	Return RET_SUCCESS for a match, ERRCODE_MATCH_REPLY_WITH_REQ_FAIL for a mismatch, or another error code for a failure.
	Record local faults with ERR_*; all non-success results leave asynchronous requests pending.
	Keep the matching function short and preserve SDK lifecycle.
*/
typedef int (*AHEADFORM_TOPIC_MATCH_REPLY_WITH_REQ)(const void *pReplyElem, const void *pMatchFuncArg);

/*
	SDK-maintained registration state; callers do not create, pass or modify these records.
	Matching argument bytes are stored here; the source address is not retained.
	Reply storage and the notification eventfd remain caller-owned.
*/
typedef struct
{
	AHEADFORM_TOPIC_MATCH_REPLY_WITH_REQ MatchReplyWithReq;
	unsigned char matchFuncArgBuf[AHEADFORM_TOPIC_MATCH_FUNC_ARG_SIZE_MAX];
	void *pReplyElemBuf;
	unsigned long long matchReplyDeadline_us;
	int notifyEventfd;
} AHEADFORM_TOPIC_ASYNC_REQUEST;

typedef struct
{
	const char *strTopicName;
	const dds_topic_descriptor_t *pstDdsTopicDesc;
	ENUM_AHEADFORM_TOPIC_DIRECTION direction;
	unsigned int sizeOf1Elem;
	dds_reliability_kind_t reliabilityKind;
	int historyDepth;
} AHEADFORM_TOPIC_DESC;

/*
	Registration reads this configuration during the call and retains pUserArg without copying its target.
	Prepare the target before registration and keep it valid until successful unregister waits for in-flight callbacks.
	Register and unregister outside Reader callbacks.
*/
typedef struct
{
	AHEADFORM_TOPIC_READER_CALLBACK ReaderCallback;
	void *pUserArg;
} AHEADFORM_TOPIC_READER_PARA;

typedef struct TOPIC_READER_NODE
{
	ENUM_AHEADFORM_TOPIC_ID topicId;
	SPMC_RING_FIFO_READER stFifoReader;
	pthread_mutex_t *pTopicMutex;
	int rxDataUpdateEventfd;
	AHEADFORM_TOPIC_ASYNC_REQUEST szstAsyncRequest[AHEADFORM_TOPIC_ASYNC_REQUEST_CNT_MAX];
	AHEADFORM_TOPIC_READER_CALLBACK ReaderCallback;
	void *pUserArg;
	struct TOPIC_READER_NODE *pNext;
} AHEADFORM_TOPIC_READER;

typedef struct TOPIC_WRITER_NODE
{
	ENUM_AHEADFORM_TOPIC_ID topicId;
	dds_entity_t *pDdsWriter;
	struct TOPIC_WRITER_NODE *pNext;
} AHEADFORM_TOPIC_WRITER;

typedef struct
{
	AHEADFORM_TOPIC_READER stHead;
	AHEADFORM_TOPIC_READER *pTail;
	unsigned int readerCnt;
} AHEADFORM_TOPIC_READER_LIST;

typedef struct
{
	AHEADFORM_TOPIC_WRITER stHead;
	AHEADFORM_TOPIC_WRITER *pTail;
	unsigned int writerCnt;
} AHEADFORM_TOPIC_WRITER_LIST;

typedef struct
{
	ENUM_AHEADFORM_TOPIC_ID topicId;
	SPMC_RING_FIFO stFifo;
	AHEADFORM_TOPIC_READER_LIST stReaderList;
	AHEADFORM_TOPIC_WRITER_LIST stWriterList;
	dds_entity_t ddsTopic;
	dds_entity_t ddsReader;
	dds_entity_t ddsWriter;
	pthread_mutex_t topicMutex;
} AHEADFORM_TOPIC;

typedef struct
{
	AHEADFORM_DDS_CTRL stDdsCtrl;
	AHEADFORM_TOPIC szstTopic[AHEADFORM_TOPIC_ID_CNT];
	unsigned long long nextAskId;
	AHEADFORM_VOLUME_REPLY szstVolumeReplyFifo[AHEADFORM_VOLUME_REPLY_FIFO_ELEM_CNT];
	AHEADFORM_SENSOR_STATE szstSensorStateFifo[AHEADFORM_SENSOR_STATE_FIFO_ELEM_CNT];
	AHEADFORM_NECK_STATE szstNeckStateFifo[AHEADFORM_NECK_STATE_FIFO_ELEM_CNT];
	AHEADFORM_FAULT_POOL szstFaultPoolFifo[AHEADFORM_FAULT_POOL_FIFO_ELEM_CNT];
	AHEADFORM_AUDIO_DEVICE_REPLY szstAudioDeviceReplyFifo[AHEADFORM_AUDIO_DEVICE_REPLY_FIFO_ELEM_CNT];
	AHEADFORM_OPERATING_MODE_REPLY szstOperatingModeReplyFifo[AHEADFORM_OPERATING_MODE_REPLY_FIFO_ELEM_CNT];
	AHEADFORM_OPERATING_MODE_STATE szstOperatingModeStateFifo[AHEADFORM_OPERATING_MODE_STATE_FIFO_ELEM_CNT];
	AHEADFORM_EMOTE_REPLY szstEmoteReplyFifo[AHEADFORM_EMOTE_REPLY_FIFO_ELEM_CNT];
	AHEADFORM_EMOTE_EVENT szstEmoteEventFifo[AHEADFORM_EMOTE_EVENT_FIFO_ELEM_CNT];
} AHEADFORM_TOPICS;

extern const AHEADFORM_TOPIC_DESC gTopicDescAry[AHEADFORM_TOPIC_ID_CNT];

/*
	The robot must already be running; initialization waits for an external peer for every enabled DDS endpoint.
	Matching the SDK's own Reader and Writer does not satisfy discovery.
	Release existing resources before reinitialization and serialize lifecycle operations.
*/
int AheadFormTopics_Init(AHEADFORM_TOPICS *pstTopics, const AHEADFORM_DDS_CFG *pstDdsCfg);

//Release all business modules and registered Readers/Writers before deinitializing Topics.
int AheadFormTopics_Deinit(AHEADFORM_TOPICS *pstTopics);

AHEADFORM_TOPIC_READER* AheadFormTopics_RegisterReader(AHEADFORM_TOPICS *pstTopics, ENUM_AHEADFORM_TOPIC_ID topicId, const AHEADFORM_TOPIC_READER_PARA *pstReaderPara);
int AheadFormTopics_UnregisterReader(AHEADFORM_TOPICS *pstTopics, AHEADFORM_TOPIC_READER *pstReader);
AHEADFORM_TOPIC_WRITER* AheadFormTopics_RegisterWriter(AHEADFORM_TOPICS *pstTopics, ENUM_AHEADFORM_TOPIC_ID topicId);
int AheadFormTopics_UnregisterWriter(AHEADFORM_TOPICS *pstTopics, AHEADFORM_TOPIC_WRITER *pstWriter);

/*
	SDK owns rxDataUpdateEventfd: one sequential consumer clears it before draining FIFO; its counter is a hint, not a sample count.
	Pop, Drop and statistics neither lock topicMutex nor access eventfd.
*/
int TopicReader_ReadLatest1Elem(AHEADFORM_TOPIC_READER *pstReader, void *pElem);
/*
	Output needs rdElemCnt elements; selected elements are oldest first.
	Nonzero rdElemCnt must be < FIFO capacity / 2; otherwise ERR_ARGV_3 before copying.
	Return < 0: ERR_*; >= 0: copied count. Only positive returns make output usable; 0 leaves it unchanged.
	Client threads only, outside Reader callbacks/matching functions; may wait for topicMutex.
	No cursor, notification or missed-count changes.
*/
int TopicReader_ReadRecentElems(AHEADFORM_TOPIC_READER *pstReader, void *pElemAryBuf, unsigned int rdElemCnt);
int TopicReader_PopElem(AHEADFORM_TOPIC_READER *pstReader, void *pElem);
int TopicReader_PopElems(AHEADFORM_TOPIC_READER *pstReader, void *pElemAryBuf, unsigned int rdElemCnt);
int TopicReader_DropOldElems(AHEADFORM_TOPIC_READER *pstReader, unsigned int retainedFreshElemCnt);
unsigned int TopicReader_GetReadableElemCnt(AHEADFORM_TOPIC_READER *pstReader);
unsigned long long TopicReader_GetMissedElemCnt(AHEADFORM_TOPIC_READER *pstReader);
/*
	Register before publishing; MatchReplyTimeout_us starts at entry and includes topicMutex wait time.
	The caller owns an empty dedicated EFD_NONBLOCK | EFD_CLOEXEC eventfd; registration does not access it.
	pMatchFuncArg must be non-NULL; matchFuncArgSize is its byte length, from 1 to AHEADFORM_TOPIC_MATCH_FUNC_ARG_SIZE_MAX.
	Registration copies these bytes; the source only needs to remain valid and unchanged during this call.
	Provide self-contained matching values, without external pointers, synchronization objects or owned resources.
	The matching function must read only the supplied bytes and must not retain their address.
	Matching functions must memcpy structure arguments into typed locals before reading their fields.
	Keep reply storage/eventfd valid until completion notification is observed, Check returns 1 or ERRCODE_REQUEST_TIMEOUT, or Cancel succeeds.
	Read the reply after completion notification or Check returns 1; Check is optional.
	Timeout does not write notifyEventfd; a wait timeout alone does not end SDK access.
*/
int TopicReader_RegisterAsyncRequest(AHEADFORM_TOPIC_READER *pstReader, AHEADFORM_TOPIC_MATCH_REPLY_WITH_REQ MatchReplyWithReq, const void *pMatchFuncArg, unsigned int matchFuncArgSize,
	unsigned long long MatchReplyTimeout_us, int notifyEventfd, void *pReplyElemBuf, unsigned int replyElemBufSize);
/*
	Return 1 for a matched reply, 0 while pending, or an error (including ERRCODE_REQUEST_TIMEOUT).
	Optional state query; completion notification already makes the reply readable.
	Does not consume notification; if used, check the current request before clearing notifyEventfd.
*/
int TopicReader_CheckIsAsyncRequestReplied(AHEADFORM_TOPIC_READER *pstReader, int notifyEventfd);
/*
	Idempotent; success ends SDK access to the reply buffer and eventfd.
	Stop Reader consumers and cancel unfinished requests before unregistering; the caller closes its notification eventfds.
*/
int TopicReader_CancelAsyncRequest(AHEADFORM_TOPIC_READER *pstReader, int notifyEventfd);
/*
	pDdsSample must match the generated DDS type registered for pstWriter->topicId.
	Domain modules validate business parameters and build the DDS sample before calling this function.
	The sample and referenced strings remain caller-owned and valid until this call returns.
	Success only means local DDS publication succeeded.
*/
int TopicWriter_Write(AHEADFORM_TOPIC_WRITER *pstWriter, const void *pDdsSample);

/*
	The caller must serialize producers and keep the Reader list stable.
	Matching requests complete before Reader notifications and callbacks; callbacks run synchronously in registration order.
	Callbacks borrow the same const FIFO slot only for the duration of each call.
*/
int AheadFormTopic_PushElemAndCallReaderCallbacks(AHEADFORM_TOPIC *pstTopic, const void *pElem);

#endif
