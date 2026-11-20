#include "AheadFormTopics.h"
#include "AheadFormDdsMsg.h"
#include "Time.h"

#include <poll.h>
#include <sys/eventfd.h>
#include <unistd.h>

#define AHEADFORM_DDS_TAKE_CNT_ONCE 16 //Maximum samples per dds_take(); fewer available samples are returned without waiting to fill the batch.

static void CallBack_TopicDataAvailable(dds_entity_t ddsReader, void *pUserArg);
static int CopyAudioDeviceReplyFromDdsSample(const arbitration_msgs_msg_MediaAudioDeviceReply *pstDdsSample, AHEADFORM_AUDIO_DEVICE_REPLY *pstReply);
static int InitDdsTopic(AHEADFORM_TOPICS *pstTopics, ENUM_AHEADFORM_TOPIC_ID topicId);
static int InitDdsReader(AHEADFORM_TOPICS *pstTopics, ENUM_AHEADFORM_TOPIC_ID topicId);
static int InitDdsWriter(AHEADFORM_TOPICS *pstTopics, ENUM_AHEADFORM_TOPIC_ID topicId);
static dds_qos_t* CreateTopicQos(dds_reliability_kind_t reliabilityKind, int historyDepth);
static int WaitRobotDdsTopicDiscovery(AHEADFORM_TOPICS *pstTopics);
static void CopySensorStateFromDdsSample(const arbitration_msgs_msg_SensorState *pstDdsSample, AHEADFORM_SENSOR_STATE *pstState);
static int CopyNeckStateFromDdsSample(const arbitration_msgs_msg_NeckState *pstDdsSample, AHEADFORM_NECK_STATE *pstState);
static int CopyFaultPoolFromDdsSample(const arbitration_msgs_msg_FaultPool *pstDdsSample, AHEADFORM_FAULT_POOL *pstPool);
static int CopyEmoteReplyFromDdsSample(const emote_msgs_EmoteControlReply *pstDdsSample, AHEADFORM_EMOTE_REPLY *pstReply);
static int CopyEmoteEventFromDdsSample(const emote_msgs_EmoteEvent *pstDdsSample, AHEADFORM_EMOTE_EVENT *pstEvent);


const AHEADFORM_TOPIC_DESC gTopicDescAry[AHEADFORM_TOPIC_ID_CNT] =
{
	[AHEADFORM_TOPIC_ID_VOLUME_ASK] = {"/hw/media/volume/ask", &arbitration_msgs_msg_MediaVolumeAsk_desc, AHEADFORM_TOPIC_DIRECTION_PUBLISH, 0, DDS_RELIABILITY_RELIABLE, 10},
	[AHEADFORM_TOPIC_ID_VOLUME_REPLY] = {"/hw/media/volume/reply", &arbitration_msgs_msg_MediaVolumeReply_desc, AHEADFORM_TOPIC_DIRECTION_SUBSCRIBE, sizeof(AHEADFORM_VOLUME_REPLY), DDS_RELIABILITY_RELIABLE, 10},
	[AHEADFORM_TOPIC_ID_MOTION_POSE] = {"/aheadform/control/aheadmotion", &arbitration_msgs_msg_AheadMotion_desc, AHEADFORM_TOPIC_DIRECTION_PUBLISH, 0, DDS_RELIABILITY_BEST_EFFORT, 8},
	[AHEADFORM_TOPIC_ID_NECK_POSE] = {"/aheadform/control/neck", &arbitration_msgs_msg_NeckControl_desc, AHEADFORM_TOPIC_DIRECTION_PUBLISH, 0, DDS_RELIABILITY_BEST_EFFORT, 1},
	[AHEADFORM_TOPIC_ID_FACE] = {"/aheadform/control/face", &arbitration_msgs_msg_FaceControl_desc, AHEADFORM_TOPIC_DIRECTION_PUBLISH, 0, DDS_RELIABILITY_BEST_EFFORT, 1},
	[AHEADFORM_TOPIC_ID_SERVO_TARGETS] = {"/hw/face/servo_direct_target", &arbitration_msgs_msg_FaceServoDirectTarget_desc, AHEADFORM_TOPIC_DIRECTION_PUBLISH, 0, DDS_RELIABILITY_BEST_EFFORT, 1},
	[AHEADFORM_TOPIC_ID_SENSOR_STATE] = {"/hw/state/sensor", &arbitration_msgs_msg_SensorState_desc, AHEADFORM_TOPIC_DIRECTION_SUBSCRIBE, sizeof(AHEADFORM_SENSOR_STATE), DDS_RELIABILITY_BEST_EFFORT, 1},
	[AHEADFORM_TOPIC_ID_NECK_STATE] = {"/hw/state/neck", &arbitration_msgs_msg_NeckState_desc, AHEADFORM_TOPIC_DIRECTION_SUBSCRIBE, sizeof(AHEADFORM_NECK_STATE), DDS_RELIABILITY_BEST_EFFORT, 1},
	[AHEADFORM_TOPIC_ID_FAULT_POOL] = {"/aheadform/status/faults", &arbitration_msgs_msg_FaultPool_desc, AHEADFORM_TOPIC_DIRECTION_SUBSCRIBE, sizeof(AHEADFORM_FAULT_POOL), DDS_RELIABILITY_RELIABLE, 1},
	[AHEADFORM_TOPIC_ID_AUDIO_DEVICE_ASK] = {"/hw/media/audio_device/ask", &arbitration_msgs_msg_MediaAudioDeviceAsk_desc, AHEADFORM_TOPIC_DIRECTION_PUBLISH, 0, DDS_RELIABILITY_RELIABLE, 10},
	[AHEADFORM_TOPIC_ID_AUDIO_DEVICE_REPLY] = {"/hw/media/audio_device/reply", &arbitration_msgs_msg_MediaAudioDeviceReply_desc, AHEADFORM_TOPIC_DIRECTION_SUBSCRIBE, sizeof(AHEADFORM_AUDIO_DEVICE_REPLY), DDS_RELIABILITY_RELIABLE, 10},
	[AHEADFORM_TOPIC_ID_OPERATING_MODE_CTRL] = {"/system/operating_mode/control", &cloud_agent_msgs_msg_OperatingModeControl_desc, AHEADFORM_TOPIC_DIRECTION_PUBLISH, 0, DDS_RELIABILITY_RELIABLE, 8},
	[AHEADFORM_TOPIC_ID_OPERATING_MODE_REPLY] = {"/system/operating_mode/control/reply", &cloud_agent_msgs_msg_OperatingModeControlReply_desc, AHEADFORM_TOPIC_DIRECTION_SUBSCRIBE, sizeof(AHEADFORM_OPERATING_MODE_REPLY), DDS_RELIABILITY_RELIABLE, 8},
	[AHEADFORM_TOPIC_ID_OPERATING_MODE_STATE] = {"/llm/mode", &llm_msgs_LlmMode_desc, AHEADFORM_TOPIC_DIRECTION_SUBSCRIBE, sizeof(AHEADFORM_OPERATING_MODE_STATE), DDS_RELIABILITY_RELIABLE, 1},
	[AHEADFORM_TOPIC_ID_EMOTE_CTRL] = {"/emote/play", &emote_msgs_EmoteControl_desc, AHEADFORM_TOPIC_DIRECTION_PUBLISH, 0, DDS_RELIABILITY_RELIABLE, 16},
	[AHEADFORM_TOPIC_ID_EMOTE_REPLY] = {"/emote/play/reply", &emote_msgs_EmoteControlReply_desc, AHEADFORM_TOPIC_DIRECTION_SUBSCRIBE, sizeof(AHEADFORM_EMOTE_REPLY), DDS_RELIABILITY_RELIABLE, 16},
	[AHEADFORM_TOPIC_ID_EMOTE_EVENT] = {"/emote/play/status", &emote_msgs_EmoteEvent_desc, AHEADFORM_TOPIC_DIRECTION_SUBSCRIBE, sizeof(AHEADFORM_EMOTE_EVENT), DDS_RELIABILITY_RELIABLE, 16}
};


int AheadFormTopics_Init(AHEADFORM_TOPICS *pstTopics, const AHEADFORM_DDS_CFG *pstDdsCfg)
{
	int i, ret;

	if (pstTopics == NULL)
		return ERR_ARGV_1;

	if (pstDdsCfg == NULL)
		return ERR_ARGV_2;

	if (pstDdsCfg->strNetworkInterface[0] == 0 || strnlen(pstDdsCfg->strNetworkInterface, sizeof(pstDdsCfg->strNetworkInterface)) >= sizeof(pstDdsCfg->strNetworkInterface))
		return ERR_NETWORK_INTERFACE_INVALID;

	memset(pstTopics, 0, sizeof(*pstTopics));
	for (i = 0; i < AHEADFORM_TOPIC_ID_CNT; i++)
	{
		pstTopics->szstTopic[i].topicId = (ENUM_AHEADFORM_TOPIC_ID)i;
		pstTopics->szstTopic[i].stReaderList.pTail = &pstTopics->szstTopic[i].stReaderList.stHead;
		pstTopics->szstTopic[i].stWriterList.pTail = &pstTopics->szstTopic[i].stWriterList.stHead;
	}

	for (i = 0; i < AHEADFORM_TOPIC_ID_CNT; i++)
	{
		if (!(gTopicDescAry[i].direction & AHEADFORM_TOPIC_DIRECTION_SUBSCRIBE))
			continue;

		switch (i)
		{
			case AHEADFORM_TOPIC_ID_VOLUME_REPLY:
				ret = SPMCRingFifo_Init(&pstTopics->szstTopic[i].stFifo, pstTopics->szstVolumeReplyFifo, sizeof(AHEADFORM_VOLUME_REPLY), AHEADFORM_VOLUME_REPLY_FIFO_ELEM_CNT);
				break;

			case AHEADFORM_TOPIC_ID_SENSOR_STATE:
				ret = SPMCRingFifo_Init(&pstTopics->szstTopic[i].stFifo, pstTopics->szstSensorStateFifo, sizeof(AHEADFORM_SENSOR_STATE), AHEADFORM_SENSOR_STATE_FIFO_ELEM_CNT);
				break;

			case AHEADFORM_TOPIC_ID_NECK_STATE:
				ret = SPMCRingFifo_Init(&pstTopics->szstTopic[i].stFifo, pstTopics->szstNeckStateFifo, sizeof(AHEADFORM_NECK_STATE), AHEADFORM_NECK_STATE_FIFO_ELEM_CNT);
				break;

			case AHEADFORM_TOPIC_ID_FAULT_POOL:
				ret = SPMCRingFifo_Init(&pstTopics->szstTopic[i].stFifo, pstTopics->szstFaultPoolFifo, sizeof(AHEADFORM_FAULT_POOL), AHEADFORM_FAULT_POOL_FIFO_ELEM_CNT);
				break;

			case AHEADFORM_TOPIC_ID_AUDIO_DEVICE_REPLY:
				ret = SPMCRingFifo_Init(&pstTopics->szstTopic[i].stFifo, pstTopics->szstAudioDeviceReplyFifo, sizeof(AHEADFORM_AUDIO_DEVICE_REPLY), AHEADFORM_AUDIO_DEVICE_REPLY_FIFO_ELEM_CNT);
				break;

			case AHEADFORM_TOPIC_ID_OPERATING_MODE_REPLY:
				ret = SPMCRingFifo_Init(&pstTopics->szstTopic[i].stFifo, pstTopics->szstOperatingModeReplyFifo, sizeof(AHEADFORM_OPERATING_MODE_REPLY), AHEADFORM_OPERATING_MODE_REPLY_FIFO_ELEM_CNT);
				break;

			case AHEADFORM_TOPIC_ID_OPERATING_MODE_STATE:
				ret = SPMCRingFifo_Init(&pstTopics->szstTopic[i].stFifo, pstTopics->szstOperatingModeStateFifo, sizeof(AHEADFORM_OPERATING_MODE_STATE), AHEADFORM_OPERATING_MODE_STATE_FIFO_ELEM_CNT);
				break;

			case AHEADFORM_TOPIC_ID_EMOTE_REPLY:
				ret = SPMCRingFifo_Init(&pstTopics->szstTopic[i].stFifo, pstTopics->szstEmoteReplyFifo, sizeof(AHEADFORM_EMOTE_REPLY), AHEADFORM_EMOTE_REPLY_FIFO_ELEM_CNT);
				break;

			case AHEADFORM_TOPIC_ID_EMOTE_EVENT:
				ret = SPMCRingFifo_Init(&pstTopics->szstTopic[i].stFifo, pstTopics->szstEmoteEventFifo, sizeof(AHEADFORM_EMOTE_EVENT), AHEADFORM_EMOTE_EVENT_FIFO_ELEM_CNT);
				break;

			default:
				return ERR_TOPIC_NOT_FOUND;
		}

		if (ret != RET_SUCCESS)
			return ret;

		if ( (ret = pthread_mutex_init(&pstTopics->szstTopic[i].topicMutex, NULL)) != 0 )
		{
			SPMCRingFifo_Deinit(&pstTopics->szstTopic[i].stFifo);
			printf("%s() call pthread_mutex_init(topic = %s) failed: %s\r\n", __FUNCTION__, gTopicDescAry[i].strTopicName, strerror(ret));
			return ERR_MUTEX_INIT;
		}
	}

	if ( (ret = DdsCtrl_Init(&pstTopics->stDdsCtrl, pstDdsCfg->strNetworkInterface, pstDdsCfg->domain_id)) != RET_SUCCESS )
		return ret;

	for (i = 0; i < AHEADFORM_TOPIC_ID_CNT; i++)
	{
		if (gTopicDescAry[i].direction == AHEADFORM_TOPIC_DIRECTION_INVALID || (gTopicDescAry[i].direction & ~AHEADFORM_TOPIC_DIRECTION_PUBLISH_SUBSCRIBE) != 0)
			return ERR_TOPIC_DIRECTION_INVALID;

		if ( (ret = InitDdsTopic(pstTopics, (ENUM_AHEADFORM_TOPIC_ID)i)) != RET_SUCCESS )
			return ret;

		if (gTopicDescAry[i].direction & AHEADFORM_TOPIC_DIRECTION_PUBLISH)
		{
			if ( (ret = InitDdsWriter(pstTopics, (ENUM_AHEADFORM_TOPIC_ID)i)) != RET_SUCCESS )
				return ret;
		}

		if (gTopicDescAry[i].direction & AHEADFORM_TOPIC_DIRECTION_SUBSCRIBE)
		{
			if ( (ret = InitDdsReader(pstTopics, (ENUM_AHEADFORM_TOPIC_ID)i)) != RET_SUCCESS )
				return ret;
		}
	}

	if ( (ret = WaitRobotDdsTopicDiscovery(pstTopics)) != RET_SUCCESS )
		return ret;

	printf("%s succeed.\r\n", __FUNCTION__);
	return RET_SUCCESS;
}

int AheadFormTopics_Deinit(AHEADFORM_TOPICS *pstTopics)
{
	int i, ret;

	if (pstTopics == NULL)
		return ERR_ARGV_1;

	for (i = 0; i < AHEADFORM_TOPIC_ID_CNT; i++)
	{
		if (pstTopics->szstTopic[i].stReaderList.readerCnt != 0 || pstTopics->szstTopic[i].stWriterList.writerCnt != 0)
			return ERR_RESOURCE_BUSY;
	}

	for (i = 0; i < AHEADFORM_TOPIC_ID_CNT; i++)
	{
		if (pstTopics->szstTopic[i].ddsReader > 0)
		{
			if (dds_delete(pstTopics->szstTopic[i].ddsReader) < 0)
				return ERR_DDS_ENTITY_DELETE_FAILED;

			pstTopics->szstTopic[i].ddsReader = 0;
		}

		if (pstTopics->szstTopic[i].ddsWriter > 0)
		{
			if (dds_delete(pstTopics->szstTopic[i].ddsWriter) < 0)
				return ERR_DDS_ENTITY_DELETE_FAILED;

			pstTopics->szstTopic[i].ddsWriter = 0;
		}

		if (pstTopics->szstTopic[i].ddsTopic > 0)
		{
			if (dds_delete(pstTopics->szstTopic[i].ddsTopic) < 0)
				return ERR_DDS_ENTITY_DELETE_FAILED;

			pstTopics->szstTopic[i].ddsTopic = 0;
		}
	}

	if ( (ret = DdsCtrl_Deinit(&pstTopics->stDdsCtrl)) != RET_SUCCESS )
		return ret;

	for (i = 0; i < AHEADFORM_TOPIC_ID_CNT; i++)
	{
		if (!(gTopicDescAry[i].direction & AHEADFORM_TOPIC_DIRECTION_SUBSCRIBE) || pstTopics->szstTopic[i].stFifo.pElemAryBuf == NULL)
			continue;

		if ( (ret = pthread_mutex_destroy(&pstTopics->szstTopic[i].topicMutex)) != 0 )
		{
			printf("%s() call pthread_mutex_destroy(topic = %s) failed: %s\r\n", __FUNCTION__, gTopicDescAry[i].strTopicName, strerror(ret));
			return ERR_MUTEX_OP_FAILED;
		}

		if ( (ret = SPMCRingFifo_Deinit(&pstTopics->szstTopic[i].stFifo)) != RET_SUCCESS )
			return ret;
	}
	memset(pstTopics, 0, sizeof(*pstTopics));
	return RET_SUCCESS;
}

AHEADFORM_TOPIC_READER* AheadFormTopics_RegisterReader(AHEADFORM_TOPICS *pstTopics, ENUM_AHEADFORM_TOPIC_ID topicId, const AHEADFORM_TOPIC_READER_PARA *pstReaderPara)
{
	AHEADFORM_TOPIC_READER *pstReader = NULL;
	int ret;

	if (pstTopics == NULL)
	{
		PRINT_FUNCTION_ERRINFO(ERR_ARGV_1, __FUNCTION__);
		return NULL;
	}

	if (topicId < 0 || topicId >= AHEADFORM_TOPIC_ID_CNT)
	{
		PRINT_FUNCTION_ERRINFO(ERR_ARGV_2, __FUNCTION__);
		return NULL;
	}

	if (pstReaderPara == NULL)
	{
		PRINT_FUNCTION_ERRINFO(ERR_ARGV_3, __FUNCTION__);
		return NULL;
	}

	if (pstTopics->stDdsCtrl.ddsParticipant <= 0)
	{
		PRINT_FUNCTION_ERRINFO(ERR_ARGV_1, __FUNCTION__, "pstTopics is invalid; call AheadFormTopics_Init() successfully before use");
		return NULL;
	}

	if (!(gTopicDescAry[topicId].direction & AHEADFORM_TOPIC_DIRECTION_SUBSCRIBE))
	{
		PRINT_FUNCTION_ERRINFO(ERR_TOPIC_DIRECTION_INVALID, __FUNCTION__);
		return NULL;
	}

	if (pstTopics->szstTopic[topicId].stFifo.pElemAryBuf == NULL || gTopicDescAry[topicId].sizeOf1Elem == 0)
	{
		PRINT_FUNCTION_ERRINFO(ERR_TOPIC_NOT_FOUND, __FUNCTION__);
		return NULL;
	}

	if (pstTopics->szstTopic[topicId].ddsTopic <= 0 || pstTopics->szstTopic[topicId].ddsReader <= 0)
	{
		PRINT_FUNCTION_ERRINFO(ERR_SDK_NOT_READY, __FUNCTION__, "call AheadFormTopics_Init() successfully before registering a Reader");
		return NULL;
	}

	if ( (pstReader = (AHEADFORM_TOPIC_READER*)malloc(sizeof(*pstReader))) == NULL )
	{
		PRINT_FUNCTION_ERRINFO(ERR_MEMORY_ALLOC, __FUNCTION__);
		return NULL;
	}
	memset(pstReader, 0, sizeof(*pstReader));
	pstReader->topicId = topicId;
	pstReader->pTopicMutex = &pstTopics->szstTopic[topicId].topicMutex;
	if ( (pstReader->rxDataUpdateEventfd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC)) < 0 )
	{
		free(pstReader);
		PRINT_FUNCTION_ERRINFO(ERR_EVENTFD_CREATE_FAILED, __FUNCTION__);
		return NULL;
	}
	pstReader->ReaderCallback = pstReaderPara->ReaderCallback;
	pstReader->pUserArg = pstReaderPara->pUserArg;
	if (pthread_mutex_lock(&pstTopics->szstTopic[topicId].topicMutex) != 0)
	{
		close(pstReader->rxDataUpdateEventfd);
		free(pstReader);
		PRINT_FUNCTION_ERRINFO(ERR_MUTEX_OP_FAILED, __FUNCTION__);
		return NULL;
	}

	if ( (ret = SPMCRingFifo_Reader_Init(&pstReader->stFifoReader, &pstTopics->szstTopic[topicId].stFifo)) == RET_SUCCESS )
	{
		pstTopics->szstTopic[topicId].stReaderList.pTail->pNext = pstReader;
		pstTopics->szstTopic[topicId].stReaderList.pTail = pstReader;
		pstTopics->szstTopic[topicId].stReaderList.readerCnt++;
	}
	pthread_mutex_unlock(&pstTopics->szstTopic[topicId].topicMutex);
	if (ret != RET_SUCCESS)
	{
		close(pstReader->rxDataUpdateEventfd);
		free(pstReader);
		PRINT_FUNCTION_ERRINFO(ret, __FUNCTION__);
		return NULL;
	}

	return pstReader;
}

int AheadFormTopics_UnregisterReader(AHEADFORM_TOPICS *pstTopics, AHEADFORM_TOPIC_READER *pstReader)
{
	AHEADFORM_TOPIC_READER *pstPrevReader = NULL, *pstCurrentReader = NULL;
	unsigned long long now_us;
	unsigned int i;
	int ret = RET_SUCCESS;

	if (pstTopics == NULL)
		return ERR_ARGV_1;

	if (pstReader == NULL)
		return ERR_ARGV_2;

	if (pstReader->topicId < 0 || pstReader->topicId >= AHEADFORM_TOPIC_ID_CNT)
		return ERR_ARGV_2;

	if (pthread_mutex_lock(&pstTopics->szstTopic[pstReader->topicId].topicMutex) != 0)
		return ERR_MUTEX_OP_FAILED;

	pstPrevReader = &pstTopics->szstTopic[pstReader->topicId].stReaderList.stHead;
	for (pstCurrentReader = pstPrevReader->pNext; pstCurrentReader != NULL && pstCurrentReader != pstReader; pstCurrentReader = pstCurrentReader->pNext)
		pstPrevReader = pstCurrentReader;

	if (pstCurrentReader == NULL)
	{
		ret = ERR_SUBSCRIPTION_NOT_FOUND;
	}
	else
	{
		now_us = GetTimeUsCnt_UpToNow();
		for (i = 0; i < AHEADFORM_TOPIC_ASYNC_REQUEST_CNT_MAX; i++)
		{
			if (pstReader->szstAsyncRequest[i].notifyEventfd <= 0)
				continue;

			if (now_us >= pstReader->szstAsyncRequest[i].matchReplyDeadline_us)
			{
				pstReader->szstAsyncRequest[i].notifyEventfd = 0;
				continue;
			}
			ret = ERR_RESOURCE_BUSY;
			break;
		}

		if (ret == RET_SUCCESS && pstReader->rxDataUpdateEventfd > 0)
		{
			if (close(pstReader->rxDataUpdateEventfd) < 0)
				ret = ERR_EVENTFD_IO_FAILED;

			pstReader->rxDataUpdateEventfd = 0;
		}

		if (ret == RET_SUCCESS)
		{
			pstPrevReader->pNext = pstCurrentReader->pNext;
			if (pstTopics->szstTopic[pstReader->topicId].stReaderList.pTail == pstCurrentReader)
				pstTopics->szstTopic[pstReader->topicId].stReaderList.pTail = pstPrevReader;

			pstTopics->szstTopic[pstReader->topicId].stReaderList.readerCnt--;
		}
	}
	pthread_mutex_unlock(&pstTopics->szstTopic[pstReader->topicId].topicMutex);
	if (ret == RET_SUCCESS)
		free(pstCurrentReader);

	return ret;
}

AHEADFORM_TOPIC_WRITER* AheadFormTopics_RegisterWriter(AHEADFORM_TOPICS *pstTopics, ENUM_AHEADFORM_TOPIC_ID topicId)
{
	AHEADFORM_TOPIC_WRITER *pstWriter = NULL;

	if (pstTopics == NULL)
	{
		PRINT_FUNCTION_ERRINFO(ERR_ARGV_1, __FUNCTION__);
		return NULL;
	}

	if (topicId < 0 || topicId >= AHEADFORM_TOPIC_ID_CNT)
	{
		PRINT_FUNCTION_ERRINFO(ERR_ARGV_2, __FUNCTION__);
		return NULL;
	}

	if (pstTopics->stDdsCtrl.ddsParticipant <= 0)
	{
		PRINT_FUNCTION_ERRINFO(ERR_ARGV_1, __FUNCTION__, "pstTopics is invalid; call AheadFormTopics_Init() successfully before use");
		return NULL;
	}

	if (!(gTopicDescAry[topicId].direction & AHEADFORM_TOPIC_DIRECTION_PUBLISH))
	{
		PRINT_FUNCTION_ERRINFO(ERR_TOPIC_DIRECTION_INVALID, __FUNCTION__);
		return NULL;
	}

	if (pstTopics->szstTopic[topicId].ddsTopic <= 0 || pstTopics->szstTopic[topicId].ddsWriter <= 0)
	{
		PRINT_FUNCTION_ERRINFO(ERR_SDK_NOT_READY, __FUNCTION__, "call AheadFormTopics_Init() successfully before registering a Writer");
		return NULL;
	}

	if ( (pstWriter = (AHEADFORM_TOPIC_WRITER*)malloc(sizeof(*pstWriter))) == NULL )
	{
		PRINT_FUNCTION_ERRINFO(ERR_MEMORY_ALLOC, __FUNCTION__);
		return NULL;
	}
	pstWriter->topicId = topicId;
	pstWriter->pDdsWriter = &pstTopics->szstTopic[topicId].ddsWriter;
	pstWriter->pNext = NULL;
	pstTopics->szstTopic[topicId].stWriterList.pTail->pNext = pstWriter;
	pstTopics->szstTopic[topicId].stWriterList.pTail = pstWriter;
	pstTopics->szstTopic[topicId].stWriterList.writerCnt++;
	return pstWriter;
}

int AheadFormTopics_UnregisterWriter(AHEADFORM_TOPICS *pstTopics, AHEADFORM_TOPIC_WRITER *pstWriter)
{
	AHEADFORM_TOPIC_WRITER *pstPrevWriter = NULL, *pstCurrentWriter = NULL;

	if (pstTopics == NULL)
		return ERR_ARGV_1;

	if (pstWriter == NULL)
		return ERR_ARGV_2;

	if (pstWriter->topicId < 0 || pstWriter->topicId >= AHEADFORM_TOPIC_ID_CNT)
		return ERR_ARGV_2;

	pstPrevWriter = &pstTopics->szstTopic[pstWriter->topicId].stWriterList.stHead;
	for (pstCurrentWriter = pstPrevWriter->pNext; pstCurrentWriter != NULL && pstCurrentWriter != pstWriter; pstCurrentWriter = pstCurrentWriter->pNext)
		pstPrevWriter = pstCurrentWriter;

	if (pstCurrentWriter == NULL)
		return ERR_SUBSCRIPTION_NOT_FOUND;

	pstPrevWriter->pNext = pstCurrentWriter->pNext;
	if (pstTopics->szstTopic[pstWriter->topicId].stWriterList.pTail == pstCurrentWriter)
		pstTopics->szstTopic[pstWriter->topicId].stWriterList.pTail = pstPrevWriter;

	pstTopics->szstTopic[pstWriter->topicId].stWriterList.writerCnt--;
	free(pstCurrentWriter);
	return RET_SUCCESS;
}

int TopicReader_ReadLatest1Elem(AHEADFORM_TOPIC_READER *pstReader, void *pElem)
{
	if (pstReader == NULL)
		return ERR_ARGV_1;

	return SPMCRingFifo_ReadLatest1Elem(pstReader->stFifoReader.pstFifo, pElem);
}

int TopicReader_ReadRecentElems(AHEADFORM_TOPIC_READER *pstReader, void *pElemAryBuf, unsigned int rdElemCnt)
{
	int mutexRet, ret;

	if (pstReader == NULL || pstReader->pTopicMutex == NULL || pstReader->stFifoReader.pstFifo == NULL || pstReader->stFifoReader.pstFifo->pElemAryBuf == NULL)
		return ERR_ARGV_1;

	if (pElemAryBuf == NULL)
		return ERR_ARGV_2;

	if (rdElemCnt == 0)
		return 0;

	if ( (mutexRet = pthread_mutex_lock(pstReader->pTopicMutex)) != 0 )
	{
		printf("%s() call pthread_mutex_lock(topicId = %d) failed: %s\r\n", __FUNCTION__, (int)pstReader->topicId, strerror(mutexRet));
		return ERR_MUTEX_OP_FAILED;
	}
	ret = SPMCRingFifo_ReadRecentElems(pstReader->stFifoReader.pstFifo, pElemAryBuf, rdElemCnt);
	if ( (mutexRet = pthread_mutex_unlock(pstReader->pTopicMutex)) != 0 )
	{
		printf("%s() call pthread_mutex_unlock(topicId = %d) failed: %s\r\n", __FUNCTION__, (int)pstReader->topicId, strerror(mutexRet));
		return ERR_MUTEX_OP_FAILED;
	}

	return ret;
}

int TopicReader_PopElem(AHEADFORM_TOPIC_READER *pstReader, void *pElem)
{
	if (pstReader == NULL)
		return ERR_ARGV_1;

	return SPMCRingFifo_Reader_PopElem(&pstReader->stFifoReader, pElem);
}

int TopicReader_PopElems(AHEADFORM_TOPIC_READER *pstReader, void *pElemAryBuf, unsigned int rdElemCnt)
{
	if (pstReader == NULL)
		return ERR_ARGV_1;

	return SPMCRingFifo_Reader_PopElems(&pstReader->stFifoReader, pElemAryBuf, rdElemCnt);
}

int TopicReader_DropOldElems(AHEADFORM_TOPIC_READER *pstReader, unsigned int retainedFreshElemCnt)
{
	if (pstReader == NULL)
		return ERR_ARGV_1;

	return SPMCRingFifo_Reader_DropOldElems(&pstReader->stFifoReader, retainedFreshElemCnt);
}

unsigned int TopicReader_GetReadableElemCnt(AHEADFORM_TOPIC_READER *pstReader)
{
	if (pstReader == NULL)
	{
		PRINT_FUNCTION_ERRINFO(ERR_ARGV_1, __FUNCTION__);
		return 0;
	}

	return SPMCRingFifo_Reader_GetReadableElemCnt(&pstReader->stFifoReader);
}

unsigned long long TopicReader_GetMissedElemCnt(AHEADFORM_TOPIC_READER *pstReader)
{
	if (pstReader == NULL)
	{
		PRINT_FUNCTION_ERRINFO(ERR_ARGV_1, __FUNCTION__);
		return 0;
	}

	return SPMCRingFifo_Reader_GetMissedElemCnt(&pstReader->stFifoReader);
}

int TopicReader_RegisterAsyncRequest(AHEADFORM_TOPIC_READER *pstReader, AHEADFORM_TOPIC_MATCH_REPLY_WITH_REQ MatchReplyWithReq, const void *pMatchFuncArg, unsigned int matchFuncArgSize,
	unsigned long long MatchReplyTimeout_us, int notifyEventfd, void *pReplyElemBuf, unsigned int replyElemBufSize)
{
	unsigned long long matchReplyDeadline_us = GetTimeUsCnt_UpToNow() + MatchReplyTimeout_us;
	unsigned long long now_us;
	unsigned int i, freeRequestIndex = AHEADFORM_TOPIC_ASYNC_REQUEST_CNT_MAX;

	if (pstReader == NULL || pstReader->pTopicMutex == NULL || pstReader->topicId < 0 || pstReader->topicId >= AHEADFORM_TOPIC_ID_CNT)
		return ERR_ARGV_1;

	if (MatchReplyWithReq == NULL)
		return ERR_ARGV_2;

	if (pMatchFuncArg == NULL)
		return ERR_ARGV_3;

	if (matchFuncArgSize == 0 || matchFuncArgSize > sizeof(pstReader->szstAsyncRequest[0].matchFuncArgBuf))
		return ERR_ARGV_4;

	if (MatchReplyTimeout_us == 0)
		return ERR_ARGV_5;

	if (notifyEventfd <= 0 || notifyEventfd == pstReader->rxDataUpdateEventfd)
		return ERR_ARGV;

	if (pReplyElemBuf == NULL || replyElemBufSize < gTopicDescAry[pstReader->topicId].sizeOf1Elem)
		return ERR_ARGV;

	if (pthread_mutex_lock(pstReader->pTopicMutex) != 0)
		return ERR_MUTEX_OP_FAILED;

	now_us = GetTimeUsCnt_UpToNow();
	if (now_us >= matchReplyDeadline_us)
	{
		pthread_mutex_unlock(pstReader->pTopicMutex);
		return ERR_REQUEST_TIMEOUT;
	}

	for (i = 0; i < AHEADFORM_TOPIC_ASYNC_REQUEST_CNT_MAX; i++)
	{
		if (pstReader->szstAsyncRequest[i].notifyEventfd > 0 && now_us >= pstReader->szstAsyncRequest[i].matchReplyDeadline_us)
			pstReader->szstAsyncRequest[i].notifyEventfd = 0;

		if (pstReader->szstAsyncRequest[i].notifyEventfd == notifyEventfd)
		{
			pthread_mutex_unlock(pstReader->pTopicMutex);
			return ERRCODE_RESOURCE_BUSY;
		}

		if (pstReader->szstAsyncRequest[i].notifyEventfd == 0 && freeRequestIndex == AHEADFORM_TOPIC_ASYNC_REQUEST_CNT_MAX)
			freeRequestIndex = i;
	}

	if (freeRequestIndex == AHEADFORM_TOPIC_ASYNC_REQUEST_CNT_MAX)
	{
		pthread_mutex_unlock(pstReader->pTopicMutex);
		return ERRCODE_RESOURCE_BUSY;
	}
	pstReader->szstAsyncRequest[freeRequestIndex].MatchReplyWithReq = MatchReplyWithReq;
	memcpy(pstReader->szstAsyncRequest[freeRequestIndex].matchFuncArgBuf, pMatchFuncArg, matchFuncArgSize);
	pstReader->szstAsyncRequest[freeRequestIndex].pReplyElemBuf = pReplyElemBuf;
	pstReader->szstAsyncRequest[freeRequestIndex].matchReplyDeadline_us = matchReplyDeadline_us;
	pstReader->szstAsyncRequest[freeRequestIndex].notifyEventfd = notifyEventfd;
	pthread_mutex_unlock(pstReader->pTopicMutex);
	return RET_SUCCESS;
}

int TopicReader_CheckIsAsyncRequestReplied(AHEADFORM_TOPIC_READER *pstReader, int notifyEventfd)
{
	struct pollfd stPollFd;
	unsigned int i;

	if (pstReader == NULL || pstReader->pTopicMutex == NULL)
		return ERR_ARGV_1;

	if (notifyEventfd <= 0)
		return ERR_ARGV_2;

	if (pthread_mutex_lock(pstReader->pTopicMutex) != 0)
		return ERR_MUTEX_OP_FAILED;

	stPollFd.fd = notifyEventfd;
	stPollFd.events = POLLIN;
	if (poll(&stPollFd, 1, 0) < 0 || (stPollFd.revents & (POLLERR | POLLHUP | POLLNVAL)))
	{
		pthread_mutex_unlock(pstReader->pTopicMutex);
		return ERR_EVENTFD_IO_FAILED;
	}

	if (stPollFd.revents & POLLIN)
	{
		pthread_mutex_unlock(pstReader->pTopicMutex);
		return 1;
	}

	for (i = 0; i < AHEADFORM_TOPIC_ASYNC_REQUEST_CNT_MAX; i++)
	{
		if (pstReader->szstAsyncRequest[i].notifyEventfd != notifyEventfd)
			continue;

		if (GetTimeUsCnt_UpToNow() >= pstReader->szstAsyncRequest[i].matchReplyDeadline_us)
		{
			pstReader->szstAsyncRequest[i].notifyEventfd = 0;
			break;
		}
		pthread_mutex_unlock(pstReader->pTopicMutex);
		return 0;
	}
	pthread_mutex_unlock(pstReader->pTopicMutex);
	return ERRCODE_REQUEST_TIMEOUT;
}

int TopicReader_CancelAsyncRequest(AHEADFORM_TOPIC_READER *pstReader, int notifyEventfd)
{
	unsigned int i;

	if (pstReader == NULL || pstReader->pTopicMutex == NULL)
		return ERR_ARGV_1;

	if (notifyEventfd <= 0)
		return ERR_ARGV_2;

	if (pthread_mutex_lock(pstReader->pTopicMutex) != 0)
		return ERR_MUTEX_OP_FAILED;

	for (i = 0; i < AHEADFORM_TOPIC_ASYNC_REQUEST_CNT_MAX; i++)
	{
		if (pstReader->szstAsyncRequest[i].notifyEventfd != notifyEventfd)
			continue;

		pstReader->szstAsyncRequest[i].notifyEventfd = 0;
		break;
	}
	pthread_mutex_unlock(pstReader->pTopicMutex);
	return RET_SUCCESS;
}

int TopicWriter_Write(AHEADFORM_TOPIC_WRITER *pstWriter, const void *pDdsSample)
{
	dds_return_t ddsRet;

	if (pstWriter == NULL)
		return ERR_ARGV_1;

	if (pDdsSample == NULL)
		return ERR_ARGV_2;

	if (pstWriter->topicId < 0 || pstWriter->topicId >= AHEADFORM_TOPIC_ID_CNT)
		return ERR_TOPIC_NOT_FOUND;

	if (!(gTopicDescAry[pstWriter->topicId].direction & AHEADFORM_TOPIC_DIRECTION_PUBLISH))
		return ERR_TOPIC_DIRECTION_INVALID;

	if (pstWriter->pDdsWriter == NULL || *pstWriter->pDdsWriter <= 0)
		return ERR_SDK_NOT_READY;

	if ( (ddsRet = dds_write(*pstWriter->pDdsWriter, pDdsSample)) < 0 )
	{
		printf("%s() call dds_write(topic = %s) failed: %s\r\n", __FUNCTION__, gTopicDescAry[pstWriter->topicId].strTopicName, dds_strretcode(-ddsRet));
		return ERR_DDS_PUBLISH_FAILED;
	}

	return RET_SUCCESS;
}

int AheadFormTopic_PushElemAndCallReaderCallbacks(AHEADFORM_TOPIC *pstTopic, const void *pElem)
{
	AHEADFORM_TOPIC_READER *pstReader = NULL;
	const void *pElemAddr = NULL;
	unsigned int i;
	int notifyEventfd, ret = RET_SUCCESS;

	if (pstTopic == NULL)
		return ERR_ARGV_1;

	if (pElem == NULL)
		return ERR_ARGV_2;

	if ( (pElemAddr = SPMCRingFifo_Writer_PushElemAndGetAddr(&pstTopic->stFifo, pElem)) == NULL )
		return ERRCODE_COMM_RING_OP_FAIL;

	for (pstReader = pstTopic->stReaderList.stHead.pNext; pstReader != NULL; pstReader = pstReader->pNext)
	{
		for (i = 0; i < AHEADFORM_TOPIC_ASYNC_REQUEST_CNT_MAX; i++)
		{
			if (pstReader->szstAsyncRequest[i].notifyEventfd <= 0)
				continue;

			if (GetTimeUsCnt_UpToNow() >= pstReader->szstAsyncRequest[i].matchReplyDeadline_us)
			{
				pstReader->szstAsyncRequest[i].notifyEventfd = 0;
				continue;
			}

			if (pstReader->szstAsyncRequest[i].MatchReplyWithReq(pElemAddr, pstReader->szstAsyncRequest[i].matchFuncArgBuf) != RET_SUCCESS)
				continue;

			memcpy(pstReader->szstAsyncRequest[i].pReplyElemBuf, pElemAddr, pstTopic->stFifo.sizeOf1Elem);
			notifyEventfd = pstReader->szstAsyncRequest[i].notifyEventfd;
			pstReader->szstAsyncRequest[i].notifyEventfd = 0;
			if (eventfd_write(notifyEventfd, 1) < 0 && errno != EAGAIN && ret == RET_SUCCESS)
				ret = ERR_EVENTFD_IO_FAILED;
		}
	}

	for (pstReader = pstTopic->stReaderList.stHead.pNext; pstReader != NULL; pstReader = pstReader->pNext)
	{
		if (pstReader->rxDataUpdateEventfd > 0 && eventfd_write(pstReader->rxDataUpdateEventfd, 1) < 0 && errno != EAGAIN && ret == RET_SUCCESS)
			ret = ERR_EVENTFD_IO_FAILED;
	}

	for (pstReader = pstTopic->stReaderList.stHead.pNext; pstReader != NULL; pstReader = pstReader->pNext)
	{
		if (pstReader->ReaderCallback != NULL)
			pstReader->ReaderCallback(pElemAddr, pstReader->pUserArg);
	}

	return ret;
}

static int InitDdsTopic(AHEADFORM_TOPICS *pstTopics, ENUM_AHEADFORM_TOPIC_ID topicId)
{
	dds_entity_t ddsTopic;

	if (pstTopics->szstTopic[topicId].ddsTopic > 0)
		return RET_SUCCESS;

	if (gTopicDescAry[topicId].pstDdsTopicDesc == NULL || gTopicDescAry[topicId].strTopicName == NULL)
		return ERR_TOPIC_NOT_FOUND;

	if ( (ddsTopic = dds_create_topic(pstTopics->stDdsCtrl.ddsParticipant, gTopicDescAry[topicId].pstDdsTopicDesc, gTopicDescAry[topicId].strTopicName, NULL, NULL)) < 0 )
		return ERR_DDS_TOPIC_CREATE_FAILED;

	pstTopics->szstTopic[topicId].ddsTopic = ddsTopic;
	return RET_SUCCESS;
}

static int InitDdsReader(AHEADFORM_TOPICS *pstTopics, ENUM_AHEADFORM_TOPIC_ID topicId)
{
	dds_qos_t *pstQos = NULL;
	dds_listener_t *pstListener = NULL;
	dds_entity_t ddsReader;
	dds_return_t ddsRet;

	if (pstTopics->szstTopic[topicId].ddsReader > 0)
		return RET_SUCCESS;

	if ( (pstQos = CreateTopicQos(gTopicDescAry[topicId].reliabilityKind, gTopicDescAry[topicId].historyDepth)) == NULL )
		return ERRCODE_MEMORY_ALLOC;

	if ( (pstListener = dds_create_listener(&pstTopics->szstTopic[topicId])) == NULL )
	{
		dds_delete_qos(pstQos);
		return ERR_MEMORY_ALLOC;
	}

	if ( (ddsRet = dds_lset_data_available_arg(pstListener, CallBack_TopicDataAvailable, &pstTopics->szstTopic[topicId], true)) < 0 )
	{
		dds_delete_listener(pstListener);
		dds_delete_qos(pstQos);
		return ERR_DDS_ENDPOINT_CREATE_FAILED;
	}
	ddsReader = dds_create_reader(pstTopics->stDdsCtrl.ddsParticipant, pstTopics->szstTopic[topicId].ddsTopic, pstQos, pstListener);
	dds_delete_listener(pstListener);
	dds_delete_qos(pstQos);
	if (ddsReader < 0)
		return ERR_DDS_ENDPOINT_CREATE_FAILED;

	pstTopics->szstTopic[topicId].ddsReader = ddsReader;
	return RET_SUCCESS;
}

static int InitDdsWriter(AHEADFORM_TOPICS *pstTopics, ENUM_AHEADFORM_TOPIC_ID topicId)
{
	dds_qos_t *pstQos = NULL;
	dds_entity_t ddsWriter;

	if (pstTopics->szstTopic[topicId].ddsWriter > 0)
		return RET_SUCCESS;

	if ( (pstQos = CreateTopicQos(gTopicDescAry[topicId].reliabilityKind, gTopicDescAry[topicId].historyDepth)) == NULL )
		return ERRCODE_MEMORY_ALLOC;

	ddsWriter = dds_create_writer(pstTopics->stDdsCtrl.ddsParticipant, pstTopics->szstTopic[topicId].ddsTopic, pstQos, NULL);
	dds_delete_qos(pstQos);
	if (ddsWriter < 0)
		return ERR_DDS_ENDPOINT_CREATE_FAILED;

	pstTopics->szstTopic[topicId].ddsWriter = ddsWriter;
	return RET_SUCCESS;
}

static dds_qos_t* CreateTopicQos(dds_reliability_kind_t reliabilityKind, int historyDepth)
{
	dds_qos_t *pstQos = dds_create_qos();

	if (pstQos == NULL)
	{
		PRINT_FUNCTION_ERRINFO(ERR_MEMORY_ALLOC, __FUNCTION__);
		return NULL;
	}
	dds_qset_reliability(pstQos, reliabilityKind, DDS_MSECS(1000));
	dds_qset_durability(pstQos, DDS_DURABILITY_VOLATILE);
	dds_qset_history(pstQos, DDS_HISTORY_KEEP_LAST, historyDepth);
	return pstQos;
}

static int WaitRobotDdsTopicDiscovery(AHEADFORM_TOPICS *pstTopics)
{
	dds_publication_matched_status_t stPublicationStatus;
	dds_subscription_matched_status_t stSubscriptionStatus;
	dds_attach_t szTriggered[2 * AHEADFORM_TOPIC_ID_CNT];
	dds_instance_handle_t matchedEndpointHandle, ownEndpointHandle;
	dds_entity_t ddsWaitSet;
	dds_return_t ddsRet, matchedEndpointCnt;
	unsigned long long deadline_us, now_us;
	unsigned int requiredEndpointCnt = 0, discoveredEndpointCnt;
	int i;

	deadline_us = GetTimeUsCnt_UpToNow() + (unsigned long long)AHEADFORM_ROBOT_DDS_DISCOVERY_TIMEOUT_MS * 1000;
	if ( (ddsWaitSet = dds_create_waitset(pstTopics->stDdsCtrl.ddsParticipant)) < 0 )
	{
		printf("%s() call dds_create_waitset(participant = %d) failed: ddsRet=%d\r\n", __FUNCTION__, pstTopics->stDdsCtrl.ddsParticipant, ddsWaitSet);
		return ERR_EVENT;
	}

	for (i = 0; i < AHEADFORM_TOPIC_ID_CNT; i++)
	{
		if (gTopicDescAry[i].direction & AHEADFORM_TOPIC_DIRECTION_PUBLISH)
		{
			if ( (ddsRet = dds_set_status_mask(pstTopics->szstTopic[i].ddsWriter, DDS_PUBLICATION_MATCHED_STATUS)) < 0 )
			{
				printf("%s() call dds_set_status_mask(writer, topic = %s) failed: ddsRet=%d\r\n", __FUNCTION__, gTopicDescAry[i].strTopicName, ddsRet);
				return ERR_EVENT;
			}

			if ( (ddsRet = dds_waitset_attach(ddsWaitSet, pstTopics->szstTopic[i].ddsWriter, i * 2)) < 0 )
			{
				printf("%s() call dds_waitset_attach(writer, topic = %s) failed: ddsRet=%d\r\n", __FUNCTION__, gTopicDescAry[i].strTopicName, ddsRet);
				return ERR_EVENT;
			}
			requiredEndpointCnt++;
		}

		if (gTopicDescAry[i].direction & AHEADFORM_TOPIC_DIRECTION_SUBSCRIBE)
		{
			if ( (ddsRet = dds_set_status_mask(pstTopics->szstTopic[i].ddsReader, DDS_SUBSCRIPTION_MATCHED_STATUS)) < 0 )
			{
				printf("%s() call dds_set_status_mask(reader, topic = %s) failed: ddsRet=%d\r\n", __FUNCTION__, gTopicDescAry[i].strTopicName, ddsRet);
				return ERR_EVENT;
			}

			if ( (ddsRet = dds_waitset_attach(ddsWaitSet, pstTopics->szstTopic[i].ddsReader, i * 2 + 1)) < 0 )
			{
				printf("%s() call dds_waitset_attach(reader, topic = %s) failed: ddsRet=%d\r\n", __FUNCTION__, gTopicDescAry[i].strTopicName, ddsRet);
				return ERR_EVENT;
			}
			requiredEndpointCnt++;
		}
	}

	while (1)
	{
		discoveredEndpointCnt = 0;
		for (i = 0; i < AHEADFORM_TOPIC_ID_CNT; i++)
		{
			if (gTopicDescAry[i].direction & AHEADFORM_TOPIC_DIRECTION_PUBLISH)
			{
				//Clear the matched status trigger before inspecting the current matching endpoints.
				if ( (ddsRet = dds_get_publication_matched_status(pstTopics->szstTopic[i].ddsWriter, &stPublicationStatus)) < 0 )
				{
					printf("%s() call dds_get_publication_matched_status(topic = %s) failed: ddsRet=%d\r\n", __FUNCTION__, gTopicDescAry[i].strTopicName, ddsRet);
					return ERR_EVENT;
				}

				if ( (matchedEndpointCnt = dds_get_matched_subscriptions(pstTopics->szstTopic[i].ddsWriter, &matchedEndpointHandle, 1)) < 0 )
				{
					printf("%s() call dds_get_matched_subscriptions(topic = %s) failed: ddsRet=%d\r\n", __FUNCTION__, gTopicDescAry[i].strTopicName, matchedEndpointCnt);
					return ERR_EVENT;
				}
				ownEndpointHandle = DDS_HANDLE_NIL;
				if (matchedEndpointCnt == 1 && (gTopicDescAry[i].direction & AHEADFORM_TOPIC_DIRECTION_SUBSCRIBE))
				{
					if ( (ddsRet = dds_get_instance_handle(pstTopics->szstTopic[i].ddsReader, &ownEndpointHandle)) < 0 )
					{
						printf("%s() call dds_get_instance_handle(reader, topic = %s) failed: ddsRet=%d\r\n", __FUNCTION__, gTopicDescAry[i].strTopicName, ddsRet);
						return ERR_EVENT;
					}
				}

				if (matchedEndpointCnt > 1 || (matchedEndpointCnt == 1 && matchedEndpointHandle != ownEndpointHandle))
					discoveredEndpointCnt++;
			}

			if (gTopicDescAry[i].direction & AHEADFORM_TOPIC_DIRECTION_SUBSCRIBE)
			{
				//Clear the matched status trigger before inspecting the current matching endpoints.
				if ( (ddsRet = dds_get_subscription_matched_status(pstTopics->szstTopic[i].ddsReader, &stSubscriptionStatus)) < 0 )
				{
					printf("%s() call dds_get_subscription_matched_status(topic = %s) failed: ddsRet=%d\r\n", __FUNCTION__, gTopicDescAry[i].strTopicName, ddsRet);
					return ERR_EVENT;
				}

				if ( (matchedEndpointCnt = dds_get_matched_publications(pstTopics->szstTopic[i].ddsReader, &matchedEndpointHandle, 1)) < 0 )
				{
					printf("%s() call dds_get_matched_publications(topic = %s) failed: ddsRet=%d\r\n", __FUNCTION__, gTopicDescAry[i].strTopicName, matchedEndpointCnt);
					return ERR_EVENT;
				}
				ownEndpointHandle = DDS_HANDLE_NIL;
				if (matchedEndpointCnt == 1 && (gTopicDescAry[i].direction & AHEADFORM_TOPIC_DIRECTION_PUBLISH))
				{
					if ( (ddsRet = dds_get_instance_handle(pstTopics->szstTopic[i].ddsWriter, &ownEndpointHandle)) < 0 )
					{
						printf("%s() call dds_get_instance_handle(writer, topic = %s) failed: ddsRet=%d\r\n", __FUNCTION__, gTopicDescAry[i].strTopicName, ddsRet);
						return ERR_EVENT;
					}
				}

				if (matchedEndpointCnt > 1 || (matchedEndpointCnt == 1 && matchedEndpointHandle != ownEndpointHandle))
					discoveredEndpointCnt++;
			}
		}

		if (discoveredEndpointCnt == requiredEndpointCnt)
		{
			if ( (ddsRet = dds_delete(ddsWaitSet)) < 0 )
			{
				printf("%s() call dds_delete(waitset = %d) failed: ddsRet=%d\r\n", __FUNCTION__, ddsWaitSet, ddsRet);
				return ERR_DDS_ENTITY_DELETE_FAILED;
			}

			return RET_SUCCESS;
		}
		now_us = GetTimeUsCnt_UpToNow();
		if (now_us >= deadline_us)
		{
			printf("%s() DDS discovery timed out: matched %u of %u required robot endpoints.\r\n", __FUNCTION__, discoveredEndpointCnt, requiredEndpointCnt);
			return ERR_DDS_DISCOVERY_TIMEOUT;
		}

		if ( (ddsRet = dds_waitset_wait(ddsWaitSet, szTriggered, 2 * AHEADFORM_TOPIC_ID_CNT, (dds_duration_t)(deadline_us - now_us) * 1000)) < 0 )
		{
			printf("%s() call dds_waitset_wait(waitset = %d) failed: ddsRet=%d\r\n", __FUNCTION__, ddsWaitSet, ddsRet);
			return ERR_EVENT;
		}
	}
}

static void CopySensorStateFromDdsSample(const arbitration_msgs_msg_SensorState *pstDdsSample, AHEADFORM_SENSOR_STATE *pstState)
{
	pstState->statusSeq = pstDdsSample->status_seq;
	pstState->statusBuildTime_us = pstDdsSample->status_build_time_us;
	pstState->state = (ENUM_AHEADFORM_SENSOR_STATE)pstDdsSample->state;
	pstState->errCode = pstDdsSample->err_code;
	pstState->imuRpyFeedbackAge_ms = pstDdsSample->imu_rpy_feedback_age_ms;
	pstState->roll_rad = pstDdsSample->roll_rad;
	pstState->pitch_rad = pstDdsSample->pitch_rad;
	pstState->yaw_rad = pstDdsSample->yaw_rad;
	pstState->isReady = pstDdsSample->ready;
}

static int CopyNeckStateFromDdsSample(const arbitration_msgs_msg_NeckState *pstDdsSample, AHEADFORM_NECK_STATE *pstState)
{
	const arbitration_msgs_msg_NeckMotorSample *pstMotor = NULL;
	AHEADFORM_NECK_MOTOR_SAMPLE *pstRetMotor = NULL;
	unsigned int i;

	if (pstDdsSample->motors._length > AHEADFORM_NECK_MOTOR_CNT_MAX || pstDdsSample->motors._length > pstDdsSample->motors._maximum)
	{
		printf("%s() invalid neck motor count: length=%u, maximum=%u.\r\n", __FUNCTION__, pstDdsSample->motors._length, pstDdsSample->motors._maximum);
		return ERR_DATA_PACKET_FORMAT;
	}

	if (pstDdsSample->motors._length > 0 && pstDdsSample->motors._buffer == NULL)
	{
		printf("%s() neck motor buffer is NULL, length=%u.\r\n", __FUNCTION__, pstDdsSample->motors._length);
		return ERR_DATA_PACKET_FORMAT;
	}
	memset(pstState, 0, sizeof(*pstState));
	pstState->state = (ENUM_AHEADFORM_NECK_STATE)pstDdsSample->state;
	pstState->errCode = pstDdsSample->err_code;
	pstState->isReady = pstDdsSample->ready;
	pstState->isFaultActive = pstDdsSample->fault_active;
	pstState->lastAcceptedTargetId = pstDdsSample->last_accepted_target_id;
	pstState->isTargetReached = pstDdsSample->reached;
	if (pstDdsSample->pose != NULL)
	{
		pstState->isPoseValid = 1;
		pstState->stPose.roll_rad = pstDdsSample->pose->roll_rad;
		pstState->stPose.pitch_rad = pstDdsSample->pose->pitch_rad;
		pstState->stPose.yaw_rad = pstDdsSample->pose->yaw_rad;
	}

	for (i = 0; i < pstDdsSample->motors._length; i++)
	{
		pstMotor = &pstDdsSample->motors._buffer[i];
		pstRetMotor = &pstState->szstMotor[i];
		pstRetMotor->motorId = pstMotor->motor_id;
		if (pstMotor->temperature_c != NULL)
		{
			pstRetMotor->isTemperatureValid = 1;
			pstRetMotor->Temperature_C = *pstMotor->temperature_c;
		}

		if (pstMotor->current_a != NULL)
		{
			pstRetMotor->isCurrentValid = 1;
			pstRetMotor->Current_A = *pstMotor->current_a;
		}

		if (pstMotor->voltage_v != NULL)
		{
			pstRetMotor->isVoltageValid = 1;
			pstRetMotor->Voltage_V = *pstMotor->voltage_v;
		}

		if (pstMotor->motor_fault_code != NULL)
		{
			pstRetMotor->isMotorFaultCodeValid = 1;
			pstRetMotor->motorFaultCode = *pstMotor->motor_fault_code;
		}

		if (pstMotor->force_n != NULL)
		{
			pstRetMotor->isForceValid = 1;
			pstRetMotor->Force_N = *pstMotor->force_n;
		}
	}
	pstState->motorCnt = pstDdsSample->motors._length;
	return RET_SUCCESS;
}

static int CopyFaultPoolFromDdsSample(const arbitration_msgs_msg_FaultPool *pstDdsSample, AHEADFORM_FAULT_POOL *pstPool)
{
	const arbitration_msgs_msg_FaultRecord *pstFault = NULL;
	AHEADFORM_FAULT_RECORD *pstRetFault = NULL;
	unsigned int i;

	if (pstDdsSample->faults._length > AHEADFORM_FAULT_CNT_MAX || pstDdsSample->faults._length > pstDdsSample->faults._maximum)
	{
		printf("%s() invalid fault count: length=%u, maximum=%u, capacity=%u.\r\n", __FUNCTION__, pstDdsSample->faults._length, pstDdsSample->faults._maximum, AHEADFORM_FAULT_CNT_MAX);
		return ERR_DATA_PACKET_FORMAT;
	}

	if (pstDdsSample->faults._length > 0 && pstDdsSample->faults._buffer == NULL)
	{
		printf("%s() fault buffer is NULL, length=%u.\r\n", __FUNCTION__, pstDdsSample->faults._length);
		return ERR_DATA_PACKET_FORMAT;
	}
	memset(pstPool, 0, sizeof(*pstPool));
	for (i = 0; i < pstDdsSample->faults._length; i++)
	{
		pstFault = &pstDdsSample->faults._buffer[i];
		pstRetFault = &pstPool->szstFault[i];
		if (pstFault->source == NULL || strnlen(pstFault->source, sizeof(pstRetFault->source)) >= sizeof(pstRetFault->source))
		{
			printf("%s() invalid or overlong fault[%u].source.\r\n", __FUNCTION__, i);
			return ERR_DATA_PACKET_FORMAT;
		}

		if (pstFault->reason == NULL || strnlen(pstFault->reason, sizeof(pstRetFault->reason)) >= sizeof(pstRetFault->reason))
		{
			printf("%s() invalid or overlong fault[%u].reason.\r\n", __FUNCTION__, i);
			return ERR_DATA_PACKET_FORMAT;
		}

		if (pstFault->timestamp == NULL || strnlen(pstFault->timestamp, sizeof(pstRetFault->timestamp)) >= sizeof(pstRetFault->timestamp))
		{
			printf("%s() invalid or overlong fault[%u].timestamp.\r\n", __FUNCTION__, i);
			return ERR_DATA_PACKET_FORMAT;
		}
		strcpy(pstRetFault->source, pstFault->source);
		pstRetFault->processFaultCode = pstFault->process_fault_code;
		pstRetFault->level = pstFault->level;
		strcpy(pstRetFault->reason, pstFault->reason);
		pstRetFault->motorFaultCode = pstFault->motor_fault_code;
		strcpy(pstRetFault->timestamp, pstFault->timestamp);
	}
	pstPool->faultCnt = pstDdsSample->faults._length;
	return RET_SUCCESS;
}

static int CopyEmoteReplyFromDdsSample(const emote_msgs_EmoteControlReply *pstDdsSample, AHEADFORM_EMOTE_REPLY *pstReply)
{
	if (strnlen(pstDdsSample->reason, sizeof(pstReply->strReason)) >= sizeof(pstReply->strReason))
	{
		printf("%s() invalid or overlong Emote reply reason.\r\n", __FUNCTION__);
		return ERR_DATA_PACKET_FORMAT;
	}
	pstReply->requestId = pstDdsSample->request_id;
	pstReply->result = (ENUM_AHEADFORM_EMOTE_RESULT)pstDdsSample->result;
	strcpy(pstReply->strReason, pstDdsSample->reason);
	pstReply->ReceiveTime_us = GetTimeUsCnt_UpToNow();
	return RET_SUCCESS;
}

static int CopyEmoteEventFromDdsSample(const emote_msgs_EmoteEvent *pstDdsSample, AHEADFORM_EMOTE_EVENT *pstEvent)
{
	if (pstDdsSample->state == NULL || strnlen(pstDdsSample->state, sizeof(pstEvent->strState)) >= sizeof(pstEvent->strState))
	{
		printf("%s() invalid or overlong Emote event state.\r\n", __FUNCTION__);
		return ERR_DATA_PACKET_FORMAT;
	}

	if (pstDdsSample->reason == NULL || strnlen(pstDdsSample->reason, sizeof(pstEvent->strReason)) >= sizeof(pstEvent->strReason))
	{
		printf("%s() invalid or overlong Emote event reason.\r\n", __FUNCTION__);
		return ERR_DATA_PACKET_FORMAT;
	}

	if (pstDdsSample->source == NULL || strnlen(pstDdsSample->source, sizeof(pstEvent->strSource)) >= sizeof(pstEvent->strSource))
	{
		printf("%s() invalid or overlong Emote event source.\r\n", __FUNCTION__);
		return ERR_DATA_PACKET_FORMAT;
	}
	pstEvent->requestId = pstDdsSample->request_id;
	strcpy(pstEvent->strState, pstDdsSample->state);
	strcpy(pstEvent->strReason, pstDdsSample->reason);
	strcpy(pstEvent->strSource, pstDdsSample->source);
	pstEvent->isLoop = pstDdsSample->loop;
	pstEvent->isReset = pstDdsSample->reset;
	pstEvent->ReceiveTime_us = GetTimeUsCnt_UpToNow();
	return RET_SUCCESS;
}

static void CallBack_TopicDataAvailable(dds_entity_t ddsReader, void *pUserArg)
{
	AHEADFORM_TOPIC *pstTopic = (AHEADFORM_TOPIC*)pUserArg;
	const arbitration_msgs_msg_MediaVolumeReply *pstReply = NULL;
	const cloud_agent_msgs_msg_OperatingModeControlReply *pstOperatingModeReply = NULL;
	const llm_msgs_LlmMode *pstOperatingModeState = NULL;
	const void *pElem = NULL;
	void *pSamples[AHEADFORM_DDS_TAKE_CNT_ONCE] = {0};
	dds_sample_info_t szstSampleInfo[AHEADFORM_DDS_TAKE_CNT_ONCE];
	AHEADFORM_VOLUME_REPLY stReply;
	AHEADFORM_SENSOR_STATE stSensorState;
	AHEADFORM_NECK_STATE stNeckState;
	AHEADFORM_FAULT_POOL stFaultPool;
	AHEADFORM_AUDIO_DEVICE_REPLY stAudioDeviceReply;
	AHEADFORM_OPERATING_MODE_REPLY stOperatingModeReply;
	AHEADFORM_OPERATING_MODE_STATE stOperatingModeState;
	AHEADFORM_EMOTE_REPLY stEmoteReply;
	AHEADFORM_EMOTE_EVENT stEmoteEvent;
	dds_return_t sampleCnt, ddsRet;
	unsigned int i;
	int mutexRet, ret = RET_SUCCESS;

	if (pstTopic == NULL || (pstTopic->topicId != AHEADFORM_TOPIC_ID_VOLUME_REPLY && pstTopic->topicId != AHEADFORM_TOPIC_ID_SENSOR_STATE &&
		pstTopic->topicId != AHEADFORM_TOPIC_ID_NECK_STATE && pstTopic->topicId != AHEADFORM_TOPIC_ID_FAULT_POOL && pstTopic->topicId != AHEADFORM_TOPIC_ID_AUDIO_DEVICE_REPLY &&
		pstTopic->topicId != AHEADFORM_TOPIC_ID_OPERATING_MODE_REPLY && pstTopic->topicId != AHEADFORM_TOPIC_ID_OPERATING_MODE_STATE &&
		pstTopic->topicId != AHEADFORM_TOPIC_ID_EMOTE_REPLY && pstTopic->topicId != AHEADFORM_TOPIC_ID_EMOTE_EVENT))
		return;

	if ( (mutexRet = pthread_mutex_lock(&pstTopic->topicMutex)) != 0 )
	{
		PRINT_FUNCTION_ERRINFO(ERR_MUTEX_OP_FAILED, __FUNCTION__, "call pthread_mutex_lock(topic = %s) failed: %s", gTopicDescAry[pstTopic->topicId].strTopicName, strerror(mutexRet));
		return;
	}

	do
	{
		if ( (sampleCnt = dds_take(ddsReader, pSamples, szstSampleInfo, AHEADFORM_DDS_TAKE_CNT_ONCE, AHEADFORM_DDS_TAKE_CNT_ONCE)) < 0 )
		{
			PRINT_FUNCTION_ERRINFO(ERR_DDS_TAKE_FAILED, __FUNCTION__, "call dds_take(reader = %d) failed: %s", ddsReader, dds_strretcode(-sampleCnt));
			break;
		}

		if (sampleCnt == 0)
			break;

		for (i = 0; i < (unsigned int)sampleCnt; i++)
		{
			if (!szstSampleInfo[i].valid_data || pSamples[i] == NULL)
				continue;

			switch (pstTopic->topicId)
			{
				case AHEADFORM_TOPIC_ID_VOLUME_REPLY:
					pstReply = (const arbitration_msgs_msg_MediaVolumeReply*)pSamples[i];
					if (pstReply->source_name == NULL || pstReply->source_name[0] == 0 || strnlen(pstReply->source_name, sizeof(stReply.sourceName)) == sizeof(stReply.sourceName))
					{
						PRINT_FUNCTION_ERRINFO(ERR_DATA_PACKET_FORMAT, __FUNCTION__, "invalid or overlong volume reply source_name");
						continue;
					}
					stReply.askId = pstReply->ask_id;
					strcpy(stReply.sourceName, pstReply->source_name);
					stReply.operation = pstReply->operation;
					stReply.retCode = pstReply->ret_code;
					stReply.queryDataValid = pstReply->query_data_valid;
					stReply.stVolume.micVolumePercent = pstReply->mic_volume_percent;
					stReply.stVolume.spkVolumePercent = pstReply->spk_volume_percent;
					stReply.ReceiveTime_us = GetTimeUsCnt_UpToNow();
					pElem = &stReply;
					break;

				case AHEADFORM_TOPIC_ID_SENSOR_STATE:
					CopySensorStateFromDdsSample((const arbitration_msgs_msg_SensorState*)pSamples[i], &stSensorState);
					stSensorState.ReceiveTime_us = GetTimeUsCnt_UpToNow();
					pElem = &stSensorState;
					break;

				case AHEADFORM_TOPIC_ID_NECK_STATE:
					if (CopyNeckStateFromDdsSample((const arbitration_msgs_msg_NeckState*)pSamples[i], &stNeckState) != RET_SUCCESS)
						continue;

					stNeckState.ReceiveTime_us = GetTimeUsCnt_UpToNow();
					pElem = &stNeckState;
					break;

				case AHEADFORM_TOPIC_ID_FAULT_POOL:
					if (CopyFaultPoolFromDdsSample((const arbitration_msgs_msg_FaultPool*)pSamples[i], &stFaultPool) != RET_SUCCESS)
						continue;

					stFaultPool.ReceiveTime_us = GetTimeUsCnt_UpToNow();
					pElem = &stFaultPool;
					break;

				case AHEADFORM_TOPIC_ID_AUDIO_DEVICE_REPLY:
					if (CopyAudioDeviceReplyFromDdsSample((const arbitration_msgs_msg_MediaAudioDeviceReply*)pSamples[i], &stAudioDeviceReply) != RET_SUCCESS)
						continue;

					pElem = &stAudioDeviceReply;
					break;

				case AHEADFORM_TOPIC_ID_OPERATING_MODE_REPLY:
					pstOperatingModeReply = (const cloud_agent_msgs_msg_OperatingModeControlReply*)pSamples[i];
					stOperatingModeReply.requestId = pstOperatingModeReply->request_id;
					stOperatingModeReply.result = (ENUM_AHEADFORM_OPERATING_MODE_RESULT)pstOperatingModeReply->result;
					stOperatingModeReply.ReceiveTime_us = GetTimeUsCnt_UpToNow();
					pElem = &stOperatingModeReply;
					break;

				case AHEADFORM_TOPIC_ID_OPERATING_MODE_STATE:
					pstOperatingModeState = (const llm_msgs_LlmMode*)pSamples[i];
					stOperatingModeState.mode = AHEADFORM_OPERATING_MODE_UNKNOWN;
					if (pstOperatingModeState->mode != NULL)
					{
						if (strcmp(pstOperatingModeState->mode, "dev") == 0)
							stOperatingModeState.mode = AHEADFORM_OPERATING_MODE_DEV;
						else if (strcmp(pstOperatingModeState->mode, "interactive") == 0)
							stOperatingModeState.mode = AHEADFORM_OPERATING_MODE_INTERACTIVE;
					}
					stOperatingModeState.ReceiveTime_us = GetTimeUsCnt_UpToNow();
					pElem = &stOperatingModeState;
					break;

				case AHEADFORM_TOPIC_ID_EMOTE_REPLY:
					if (CopyEmoteReplyFromDdsSample((const emote_msgs_EmoteControlReply*)pSamples[i], &stEmoteReply) != RET_SUCCESS)
						continue;

					pElem = &stEmoteReply;
					break;

				case AHEADFORM_TOPIC_ID_EMOTE_EVENT:
					if (CopyEmoteEventFromDdsSample((const emote_msgs_EmoteEvent*)pSamples[i], &stEmoteEvent) != RET_SUCCESS)
						continue;

					pElem = &stEmoteEvent;
					break;

				default:
					continue;
			}

			if ( (ret = AheadFormTopic_PushElemAndCallReaderCallbacks(pstTopic, pElem)) != RET_SUCCESS && ret != ERRCODE_EVENTFD_IO_FAILED )
				break;
		}

		if ( (ddsRet = dds_return_loan(ddsReader, pSamples, sampleCnt)) < 0 )
		{
			PRINT_FUNCTION_ERRINFO(ERR_DDS_TAKE_FAILED, __FUNCTION__, "call dds_return_loan(reader = %d) failed: %s", ddsReader, dds_strretcode(-ddsRet));
			break;
		}

		if (ret != RET_SUCCESS && ret != ERRCODE_EVENTFD_IO_FAILED)
			break;

	} while (sampleCnt == AHEADFORM_DDS_TAKE_CNT_ONCE);

	if ( (mutexRet = pthread_mutex_unlock(&pstTopic->topicMutex)) != 0 )
		PRINT_FUNCTION_ERRINFO(ERR_MUTEX_OP_FAILED, __FUNCTION__, "call pthread_mutex_unlock(topic = %s) failed: %s", gTopicDescAry[pstTopic->topicId].strTopicName, strerror(mutexRet));
}

static int CopyAudioDeviceReplyFromDdsSample(const arbitration_msgs_msg_MediaAudioDeviceReply *pstDdsSample, AHEADFORM_AUDIO_DEVICE_REPLY *pstReply)
{
	unsigned int i;

	if (pstDdsSample == NULL)
		return ERR_ARGV_1;

	if (pstReply == NULL)
		return ERR_ARGV_2;

	if (pstDdsSample->source_name == NULL || pstDdsSample->source_name[0] == 0 || strnlen(pstDdsSample->source_name, sizeof(pstReply->sourceName)) == sizeof(pstReply->sourceName))
		return ERR_DATA_PACKET_FORMAT;

	if (pstDdsSample->devices._length > AHEADFORM_AUDIO_DEVICE_CNT_MAX || pstDdsSample->devices._length > pstDdsSample->devices._maximum)
		return ERR_DATA_PACKET_FORMAT;

	if (pstDdsSample->devices._length > 0 && pstDdsSample->devices._buffer == NULL)
		return ERR_DATA_PACKET_FORMAT;

	for (i = 0; i < pstDdsSample->devices._length; i++)
	{
		if (pstDdsSample->devices._buffer[i].device_id == NULL || strnlen(pstDdsSample->devices._buffer[i].device_id, sizeof(pstReply->szstDeviceInfo[i].strDeviceId)) == sizeof(pstReply->szstDeviceInfo[i].strDeviceId))
			return ERR_DATA_PACKET_FORMAT;

		if (pstDdsSample->devices._buffer[i].device_name == NULL || strnlen(pstDdsSample->devices._buffer[i].device_name, sizeof(pstReply->szstDeviceInfo[i].deviceName)) == sizeof(pstReply->szstDeviceInfo[i].deviceName))
			return ERR_DATA_PACKET_FORMAT;

		strcpy(pstReply->szstDeviceInfo[i].strDeviceId, pstDdsSample->devices._buffer[i].device_id);
		strcpy(pstReply->szstDeviceInfo[i].deviceName, pstDdsSample->devices._buffer[i].device_name);
	}
	pstReply->stTime.sec = pstDdsSample->stamp.sec;
	pstReply->stTime.ns = pstDdsSample->stamp.nanosec;
	pstReply->askId = pstDdsSample->ask_id;
	strcpy(pstReply->sourceName, pstDdsSample->source_name);
	pstReply->retCode = pstDdsSample->ret_code;
	pstReply->operation = (ENUM_AHEADFORM_AUDIO_DEVICE_OPERATION)pstDdsSample->operation;
	pstReply->deviceType = (ENUM_AHEADFORM_AUDIO_DEVICE_TYPE)pstDdsSample->device_type;
	pstReply->queryDataValid = pstDdsSample->query_data_valid;
	pstReply->deviceCnt = pstDdsSample->devices._length;
	pstReply->ReceiveTime_us = GetTimeUsCnt_UpToNow();
	return RET_SUCCESS;
}
