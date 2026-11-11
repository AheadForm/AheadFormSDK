#include "AheadFormTopics.h"
#include "AheadFormDdsMsg.h"
#include "OperatingModeMessages.h"
#include "Time.h"

#include <math.h>
#include <poll.h>
#include <sys/eventfd.h>
#include <unistd.h>

#define AHEADFORM_DDS_TAKE_CNT_ONCE 16 //Maximum samples per dds_take(); fewer available samples are returned without waiting to fill the batch.

static void CallBack_TopicDataAvailable(dds_entity_t ddsReader, void *pUserArg);
static int InitDdsTopic(AHEADFORM_TOPICS *pstTopics, ENUM_AHEADFORM_TOPIC_ID topicId);
static int InitDdsReader(AHEADFORM_TOPICS *pstTopics, ENUM_AHEADFORM_TOPIC_ID topicId);
static int InitDdsWriter(AHEADFORM_TOPICS *pstTopics, ENUM_AHEADFORM_TOPIC_ID topicId);
static dds_qos_t* CreateTopicQos(ENUM_AHEADFORM_TOPIC_ID topicId);
static int ConvertDdsDomainSample(AHEADFORM_TOPIC *pstTopic, const void *pDdsSample);
static int WaitRobotDdsTopicDiscovery(AHEADFORM_TOPICS *pstTopics);


const AHEADFORM_TOPIC_DESC gTopicDescAry[AHEADFORM_TOPIC_ID_CNT] =
{
	{NULL, NULL, AHEADFORM_TOPIC_DIRECTION_INVALID, 0, DDS_RELIABILITY_BEST_EFFORT, DDS_DURABILITY_VOLATILE, 1},
	{"/hw/media/volume/ask", &arbitration_msgs_msg_MediaVolumeAsk_desc, AHEADFORM_TOPIC_DIRECTION_PUBLISH, sizeof(AHEADFORM_VOLUME_ASK), DDS_RELIABILITY_RELIABLE, DDS_DURABILITY_VOLATILE, 10},
	{"/hw/media/volume/reply", &arbitration_msgs_msg_MediaVolumeReply_desc, AHEADFORM_TOPIC_DIRECTION_SUBSCRIBE, sizeof(AHEADFORM_VOLUME_REPLY_SAMPLE), DDS_RELIABILITY_RELIABLE, DDS_DURABILITY_VOLATILE, 10},
	{"/aheadform/control/aheadmotion", &arbitration_msgs_msg_AheadMotion_desc, AHEADFORM_TOPIC_DIRECTION_PUBLISH, sizeof(AHEADFORM_MOTION_COMMAND), DDS_RELIABILITY_BEST_EFFORT, DDS_DURABILITY_VOLATILE, 8},
	{"/aheadform/control/neck", &arbitration_msgs_msg_NeckControl_desc, AHEADFORM_TOPIC_DIRECTION_PUBLISH, sizeof(AHEADFORM_NECK_COMMAND), DDS_RELIABILITY_BEST_EFFORT, DDS_DURABILITY_VOLATILE, 1},
	{"/aheadform/control/face", &arbitration_msgs_msg_FaceControl_desc, AHEADFORM_TOPIC_DIRECTION_PUBLISH, sizeof(AHEADFORM_FACE_COMMAND), DDS_RELIABILITY_RELIABLE, DDS_DURABILITY_VOLATILE, 1},
	{"/hw/face/servo_direct_target", &arbitration_msgs_msg_FaceServoDirectTarget_desc, AHEADFORM_TOPIC_DIRECTION_PUBLISH, sizeof(AHEADFORM_SERVO_COMMAND), DDS_RELIABILITY_BEST_EFFORT, DDS_DURABILITY_VOLATILE, 1},
	{"/motion/preset_motion/ask", &arbitration_msgs_msg_PresetMotionAsk_desc, AHEADFORM_TOPIC_DIRECTION_PUBLISH, sizeof(AHEADFORM_PRESET_MOTION_ASK), DDS_RELIABILITY_RELIABLE, DDS_DURABILITY_VOLATILE, 10},
	{"/motion/preset_motion/reply", &arbitration_msgs_msg_PresetMotionReply_desc, AHEADFORM_TOPIC_DIRECTION_SUBSCRIBE, sizeof(AHEADFORM_PRESET_MOTION_REPLY_SAMPLE), DDS_RELIABILITY_RELIABLE, DDS_DURABILITY_VOLATILE, 10},
	{"/hw/media/audio_device/ask", &arbitration_msgs_msg_MediaAudioDeviceAsk_desc, AHEADFORM_TOPIC_DIRECTION_PUBLISH, sizeof(AHEADFORM_AUDIO_DEVICE_ASK), DDS_RELIABILITY_RELIABLE, DDS_DURABILITY_VOLATILE, 10},
	{"/hw/media/audio_device/reply", &arbitration_msgs_msg_MediaAudioDeviceReply_desc, AHEADFORM_TOPIC_DIRECTION_SUBSCRIBE, sizeof(AHEADFORM_AUDIO_DEVICE_REPLY_SAMPLE), DDS_RELIABILITY_RELIABLE, DDS_DURABILITY_VOLATILE, 10},
	{"/hw/media/channel/ask", &arbitration_msgs_msg_MediaChannelControlAsk_desc, AHEADFORM_TOPIC_DIRECTION_PUBLISH, sizeof(AHEADFORM_MEDIA_CHANNEL_ASK), DDS_RELIABILITY_RELIABLE, DDS_DURABILITY_VOLATILE, 10},
	{"/hw/media/channel/reply", &arbitration_msgs_msg_MediaChannelControlReply_desc, AHEADFORM_TOPIC_DIRECTION_SUBSCRIBE, sizeof(AHEADFORM_MEDIA_CHANNEL_REPLY_SAMPLE), DDS_RELIABILITY_RELIABLE, DDS_DURABILITY_VOLATILE, 10},
	{"/hw/state", &arbitration_msgs_msg_HardwareState_desc, AHEADFORM_TOPIC_DIRECTION_SUBSCRIBE, sizeof(AHEADFORM_HARDWARE_STATE_SAMPLE), DDS_RELIABILITY_RELIABLE, DDS_DURABILITY_VOLATILE, 1},
	{"/hw/state/face", &arbitration_msgs_msg_FaceState_desc, AHEADFORM_TOPIC_DIRECTION_SUBSCRIBE, sizeof(AHEADFORM_FACE_STATE_SAMPLE), DDS_RELIABILITY_BEST_EFFORT, DDS_DURABILITY_VOLATILE, 1},
	{"/hw/state/neck", &arbitration_msgs_msg_NeckState_desc, AHEADFORM_TOPIC_DIRECTION_SUBSCRIBE, sizeof(AHEADFORM_NECK_STATE_SAMPLE), DDS_RELIABILITY_BEST_EFFORT, DDS_DURABILITY_VOLATILE, 1},
	{"/hw/state/media", &arbitration_msgs_msg_MediaState_desc, AHEADFORM_TOPIC_DIRECTION_SUBSCRIBE, sizeof(AHEADFORM_MEDIA_STATE_SAMPLE), DDS_RELIABILITY_BEST_EFFORT, DDS_DURABILITY_VOLATILE, 1},
	{"/hw/state/power", &arbitration_msgs_msg_PowerState_desc, AHEADFORM_TOPIC_DIRECTION_SUBSCRIBE, sizeof(AHEADFORM_POWER_STATE_SAMPLE), DDS_RELIABILITY_BEST_EFFORT, DDS_DURABILITY_VOLATILE, 1},
	{"/hw/state/sensor", &arbitration_msgs_msg_SensorState_desc, AHEADFORM_TOPIC_DIRECTION_SUBSCRIBE, sizeof(AHEADFORM_SENSOR_STATE_SAMPLE), DDS_RELIABILITY_BEST_EFFORT, DDS_DURABILITY_VOLATILE, 1},
	{"/hw/fault/active", &arbitration_msgs_msg_HardwareFaultActiveSet_desc, AHEADFORM_TOPIC_DIRECTION_SUBSCRIBE, sizeof(AHEADFORM_FAULT_SET_SAMPLE), DDS_RELIABILITY_RELIABLE, DDS_DURABILITY_TRANSIENT_LOCAL, 1},
	{"/hw/fault/events", &arbitration_msgs_msg_HardwareFaultEvent_desc, AHEADFORM_TOPIC_DIRECTION_SUBSCRIBE, sizeof(AHEADFORM_FAULT_EVENT_SAMPLE), DDS_RELIABILITY_RELIABLE, DDS_DURABILITY_VOLATILE, 50},
	{"/hw/control/ask", &arbitration_msgs_msg_HardwareControlAsk_desc, AHEADFORM_TOPIC_DIRECTION_PUBLISH, sizeof(AHEADFORM_HARDWARE_CONTROL_ASK), DDS_RELIABILITY_RELIABLE, DDS_DURABILITY_VOLATILE, 10},
	{"/hw/control/reply", &arbitration_msgs_msg_HardwareControlReply_desc, AHEADFORM_TOPIC_DIRECTION_SUBSCRIBE, sizeof(AHEADFORM_HARDWARE_CONTROL_REPLY_SAMPLE), DDS_RELIABILITY_RELIABLE, DDS_DURABILITY_VOLATILE, 10},
	{"/safety/control/ask", &arbitration_msgs_msg_SafetyControlAsk_desc, AHEADFORM_TOPIC_DIRECTION_PUBLISH, sizeof(AHEADFORM_SAFETY_CONTROL_ASK), DDS_RELIABILITY_RELIABLE, DDS_DURABILITY_VOLATILE, 10},
	{"/safety/control/reply", &arbitration_msgs_msg_SafetyControlReply_desc, AHEADFORM_TOPIC_DIRECTION_SUBSCRIBE, sizeof(AHEADFORM_SAFETY_CONTROL_REPLY_SAMPLE), DDS_RELIABILITY_RELIABLE, DDS_DURABILITY_VOLATILE, 10},
	{"/system/operating_mode/control", &cloud_agent_msgs_msg_OperatingModeControl_desc, AHEADFORM_TOPIC_DIRECTION_PUBLISH, sizeof(AHEADFORM_OPERATING_MODE_ASK), DDS_RELIABILITY_RELIABLE, DDS_DURABILITY_VOLATILE, 8},
	{"/system/operating_mode/control/reply", &cloud_agent_msgs_msg_OperatingModeControlReply_desc, AHEADFORM_TOPIC_DIRECTION_SUBSCRIBE, sizeof(AHEADFORM_OPERATING_MODE_REPLY_SAMPLE), DDS_RELIABILITY_RELIABLE, DDS_DURABILITY_VOLATILE, 8}
};


int AheadFormTopics_Init(AHEADFORM_TOPICS *pstTopics, const AHEADFORM_DDS_CFG *pstDdsCfg, const AHEADFORM_STATUS_CFG *pstStatusCfg)
{
	int i, ret;
	unsigned int elemSize, elemCnt;
	void *pStorage = NULL;

	if (pstTopics == NULL)
		return ERR_ARGV_1;

	if (pstDdsCfg == NULL)
		return ERR_ARGV_2;

	if (pstStatusCfg == NULL || pstStatusCfg->faultRecordCntMax == 0)
		return ERR_ARGV_3;

	if (pstDdsCfg->strNetworkInterface[0] == 0 || strnlen(pstDdsCfg->strNetworkInterface, sizeof(pstDdsCfg->strNetworkInterface)) == sizeof(pstDdsCfg->strNetworkInterface))
		return ERR_NETWORK_INTERFACE_INVALID;

	if (pstStatusCfg->faultRecordCntMax > (UINT_MAX - sizeof(AHEADFORM_FAULT_SET_SAMPLE)) / sizeof(AHEADFORM_FAULT_RECORD))
		return ERR_OVER_BUF;

	memset(pstTopics, 0, sizeof(*pstTopics));
	for (i = 0; i < AHEADFORM_TOPIC_ID_CNT; i++)
	{
		pstTopics->szstTopic[i].topicId = (ENUM_AHEADFORM_TOPIC_ID)i;
		pstTopics->szstTopic[i].stReaderList.pTail = &pstTopics->szstTopic[i].stReaderList.stHead;
		pstTopics->szstTopic[i].stWriterList.pTail = &pstTopics->szstTopic[i].stWriterList.stHead;
	}

	for (i = AHEADFORM_TOPIC_ID_INVALID + 1; i < AHEADFORM_TOPIC_ID_CNT; i++)
	{
		if (!(gTopicDescAry[i].direction & AHEADFORM_TOPIC_DIRECTION_SUBSCRIBE))
			continue;

		elemSize = gTopicDescAry[i].sizeOf1Elem;
		elemCnt = i == AHEADFORM_TOPIC_ID_VOLUME_REPLY ? AHEADFORM_VOLUME_REPLY_FIFO_ELEM_CNT : AHEADFORM_TOPIC_FIFO_ELEM_CNT;
		if (i == AHEADFORM_TOPIC_ID_FAULT_SET)
			elemSize += pstStatusCfg->faultRecordCntMax * sizeof(AHEADFORM_FAULT_RECORD);

		if ((size_t)elemSize > SIZE_MAX / elemCnt)
			return ERR_OVER_BUF;

		pStorage = i == AHEADFORM_TOPIC_ID_VOLUME_REPLY ? (void*)pstTopics->szstVolumeReplyFifo : calloc(elemCnt, elemSize);
		if (pStorage == NULL)
			return ERR_MEMORY_ALLOC;

		if ( (ret = SPMCRingFifo_Init(&pstTopics->szstTopic[i].stFifo, pStorage, elemSize, elemCnt)) != RET_SUCCESS )
			return ret;

		if ( (pstTopics->szstTopic[i].pReceiveElem = calloc(1, elemSize)) == NULL )
			return ERR_MEMORY_ALLOC;

		if ( (ret = pthread_mutex_init(&pstTopics->szstTopic[i].topicMutex, NULL)) != 0 )
		{
			printf("%s() call pthread_mutex_init(topic = %s) failed: %s\r\n", __FUNCTION__, gTopicDescAry[i].strTopicName, strerror(ret));
			return ERR_MUTEX_INIT;
		}
		pstTopics->szstTopic[i].isMutexInitialized = 1;
		pstTopics->szstTopic[i].lastReceiveResult = ERRCODE_NO_FRESH_FEEDBACK;
	}

	if ( (ret = DdsCtrl_Init(&pstTopics->stDdsCtrl, pstDdsCfg->strNetworkInterface, pstDdsCfg->domain_id)) != RET_SUCCESS )
		return ret;

	for (i = AHEADFORM_TOPIC_ID_INVALID + 1; i < AHEADFORM_TOPIC_ID_CNT; i++)
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

	for (i = AHEADFORM_TOPIC_ID_INVALID + 1; i < AHEADFORM_TOPIC_ID_CNT; i++)
	{
		if (pstTopics->szstTopic[i].isMutexInitialized)
		{
			if ( (ret = pthread_mutex_destroy(&pstTopics->szstTopic[i].topicMutex)) != 0 )
			{
				printf("%s() call pthread_mutex_destroy(topic = %s) failed: %s\r\n", __FUNCTION__, gTopicDescAry[i].strTopicName, strerror(ret));
				return ERR_MUTEX_OP_FAILED;
			}
			pstTopics->szstTopic[i].isMutexInitialized = 0;
		}
		free(pstTopics->szstTopic[i].pReceiveElem);
		pstTopics->szstTopic[i].pReceiveElem = NULL;
		if (i != AHEADFORM_TOPIC_ID_VOLUME_REPLY)
			free(pstTopics->szstTopic[i].stFifo.pElemAryBuf);

		SPMCRingFifo_Deinit(&pstTopics->szstTopic[i].stFifo);
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

	if (topicId <= AHEADFORM_TOPIC_ID_INVALID || topicId >= AHEADFORM_TOPIC_ID_CNT)
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
	pstReader->pLastReceiveResult = &pstTopics->szstTopic[topicId].lastReceiveResult;
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

	if (pstReader->topicId <= AHEADFORM_TOPIC_ID_INVALID || pstReader->topicId >= AHEADFORM_TOPIC_ID_CNT)
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
				memset(&pstReader->szstAsyncRequest[i], 0, sizeof(pstReader->szstAsyncRequest[i]));
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

	if (topicId <= AHEADFORM_TOPIC_ID_INVALID || topicId >= AHEADFORM_TOPIC_ID_CNT)
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

	if (pstWriter->topicId <= AHEADFORM_TOPIC_ID_INVALID || pstWriter->topicId >= AHEADFORM_TOPIC_ID_CNT)
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

	if (pstReader == NULL || pstReader->pTopicMutex == NULL || pstReader->topicId <= AHEADFORM_TOPIC_ID_INVALID || pstReader->topicId >= AHEADFORM_TOPIC_ID_CNT)
		return ERR_ARGV_1;

	if (MatchReplyWithReq == NULL)
		return ERR_ARGV_2;

	if (pMatchFuncArg == NULL)
		return ERR_ARGV_3;

	if (matchFuncArgSize == 0 || matchFuncArgSize > sizeof(pstReader->szstAsyncRequest[0].matchFuncArgBuf))
		return ERR_ARGV_4;

	if (MatchReplyTimeout_us == 0)
		return ERR_REQUEST_TIMEOUT;

	if (notifyEventfd <= 0 || notifyEventfd == pstReader->rxDataUpdateEventfd)
		return ERR_ARGV;

	if (pReplyElemBuf == NULL || replyElemBufSize < pstReader->stFifoReader.pstFifo->sizeOf1Elem)
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
			memset(&pstReader->szstAsyncRequest[i], 0, sizeof(pstReader->szstAsyncRequest[i]));

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
			memset(&pstReader->szstAsyncRequest[i], 0, sizeof(pstReader->szstAsyncRequest[i]));
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

		memset(&pstReader->szstAsyncRequest[i], 0, sizeof(pstReader->szstAsyncRequest[i]));
		break;
	}
	pthread_mutex_unlock(pstReader->pTopicMutex);
	return RET_SUCCESS;
}

static int WriteVolumeRequest(AHEADFORM_TOPIC_WRITER *pstWriter, const void *pElem)
{
	const AHEADFORM_VOLUME_ASK *pstAsk = (const AHEADFORM_VOLUME_ASK*)pElem;
	arbitration_msgs_msg_MediaVolumeAsk stDdsAsk = {0};
	struct timespec stTime;
	dds_return_t ddsRet;

	if (pstWriter == NULL)
		return ERR_ARGV_1;

	if (pElem == NULL)
		return ERR_ARGV_2;

	if (pstWriter->topicId != AHEADFORM_TOPIC_ID_VOLUME_ASK || pstWriter->pDdsWriter == NULL || *pstWriter->pDdsWriter <= 0)
		return ERR_TOPIC_DIRECTION_INVALID;

	if (pstAsk->askId == 0 || pstAsk->sourceName[0] == 0 || strnlen(pstAsk->sourceName, sizeof(pstAsk->sourceName)) == sizeof(pstAsk->sourceName))
		return ERR_ARGV_2;

	if (pstAsk->operation != AHEADFORM_VOLUME_OPERATION_GET && pstAsk->operation != AHEADFORM_VOLUME_OPERATION_SET)
		return ERR_ARGV_2;

	if (pstAsk->operation == AHEADFORM_VOLUME_OPERATION_SET && (!pstAsk->isSetMicVolume && !pstAsk->isSetSpkVolume))
		return ERR_ARGV_2;

	if (pstAsk->isSetMicVolume && (!isfinite(pstAsk->stVolume.micVolumePercent) || pstAsk->stVolume.micVolumePercent < 0.0f || pstAsk->stVolume.micVolumePercent > 100.0f))
		return ERR_ARGV_2;

	if (pstAsk->isSetSpkVolume && (!isfinite(pstAsk->stVolume.spkVolumePercent) || pstAsk->stVolume.spkVolumePercent < 0.0f || pstAsk->stVolume.spkVolumePercent > 100.0f))
		return ERR_ARGV_2;

	if (clock_gettime(CLOCK_REALTIME, &stTime) < 0)
		return ERR_CLOCK;

	stDdsAsk.stamp.sec = stTime.tv_sec;
	stDdsAsk.stamp.nanosec = stTime.tv_nsec;
	stDdsAsk.ask_id = pstAsk->askId;
	stDdsAsk.source_name = (char*)pstAsk->sourceName;
	stDdsAsk.operation = pstAsk->operation;
	stDdsAsk.has_mic_volume = pstAsk->isSetMicVolume != 0;
	stDdsAsk.mic_volume_percent = pstAsk->stVolume.micVolumePercent;
	stDdsAsk.has_spk_volume = pstAsk->isSetSpkVolume != 0;
	stDdsAsk.spk_volume_percent = pstAsk->stVolume.spkVolumePercent;
	if ( (ddsRet = dds_write(*pstWriter->pDdsWriter, &stDdsAsk)) < 0 )
		return ERR_DDS_PUBLISH_FAILED;

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

	__atomic_store_n(&pstTopic->lastReceiveResult, RET_SUCCESS, __ATOMIC_RELEASE);
	for (pstReader = pstTopic->stReaderList.stHead.pNext; pstReader != NULL; pstReader = pstReader->pNext)
	{
		for (i = 0; i < AHEADFORM_TOPIC_ASYNC_REQUEST_CNT_MAX; i++)
		{
			if (pstReader->szstAsyncRequest[i].notifyEventfd <= 0)
				continue;

			if (GetTimeUsCnt_UpToNow() >= pstReader->szstAsyncRequest[i].matchReplyDeadline_us)
			{
				memset(&pstReader->szstAsyncRequest[i], 0, sizeof(pstReader->szstAsyncRequest[i]));
				continue;
			}

			if (!pstReader->szstAsyncRequest[i].MatchReplyWithReq(pElemAddr, pstReader->szstAsyncRequest[i].matchFuncArgBuf))
				continue;

			memcpy(pstReader->szstAsyncRequest[i].pReplyElemBuf, pElemAddr, pstTopic->stFifo.sizeOf1Elem);
			notifyEventfd = pstReader->szstAsyncRequest[i].notifyEventfd;
			memset(&pstReader->szstAsyncRequest[i], 0, sizeof(pstReader->szstAsyncRequest[i]));
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

	if ( (pstQos = CreateTopicQos(topicId)) == NULL )
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

	if ( (pstQos = CreateTopicQos(topicId)) == NULL )
		return ERRCODE_MEMORY_ALLOC;

	ddsWriter = dds_create_writer(pstTopics->stDdsCtrl.ddsParticipant, pstTopics->szstTopic[topicId].ddsTopic, pstQos, NULL);
	dds_delete_qos(pstQos);
	if (ddsWriter < 0)
		return ERR_DDS_ENDPOINT_CREATE_FAILED;

	pstTopics->szstTopic[topicId].ddsWriter = ddsWriter;
	return RET_SUCCESS;
}

static dds_qos_t* CreateTopicQos(ENUM_AHEADFORM_TOPIC_ID topicId)
{
	dds_qos_t *pstQos = dds_create_qos();

	if (pstQos == NULL)
	{
		PRINT_FUNCTION_ERRINFO(ERR_MEMORY_ALLOC, __FUNCTION__);
		return NULL;
	}
	dds_qset_reliability(pstQos, gTopicDescAry[topicId].reliabilityKind, DDS_MSECS(1000));
	dds_qset_durability(pstQos, gTopicDescAry[topicId].durabilityKind);
	dds_qset_history(pstQos, DDS_HISTORY_KEEP_LAST, gTopicDescAry[topicId].historyDepth);
	return pstQos;
}

static int WaitRobotDdsTopicDiscovery(AHEADFORM_TOPICS *pstTopics)
{
	dds_publication_matched_status_t stPublicationStatus;
	dds_subscription_matched_status_t stSubscriptionStatus;
	dds_attach_t szTriggered[2 * (AHEADFORM_TOPIC_ID_CNT - 1)];
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

	for (i = AHEADFORM_TOPIC_ID_INVALID + 1; i < AHEADFORM_TOPIC_ID_CNT; i++)
	{
		//Deployed external Bridge guarantees volume request/reply and streamed full motion; other domains are optional.
		if (i != AHEADFORM_TOPIC_ID_VOLUME_ASK && i != AHEADFORM_TOPIC_ID_VOLUME_REPLY && i != AHEADFORM_TOPIC_ID_MOTION)
			continue;

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
		for (i = AHEADFORM_TOPIC_ID_INVALID + 1; i < AHEADFORM_TOPIC_ID_CNT; i++)
		{
			if (i != AHEADFORM_TOPIC_ID_VOLUME_ASK && i != AHEADFORM_TOPIC_ID_VOLUME_REPLY && i != AHEADFORM_TOPIC_ID_MOTION)
				continue;

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
			return ERR_DDS_DISCOVERY_TIMEOUT;

		if ( (ddsRet = dds_waitset_wait(ddsWaitSet, szTriggered, 2 * (AHEADFORM_TOPIC_ID_CNT - 1), (dds_duration_t)(deadline_us - now_us) * 1000)) < 0 )
		{
			printf("%s() call dds_waitset_wait(waitset = %d) failed: ddsRet=%d\r\n", __FUNCTION__, ddsWaitSet, ddsRet);
			return ERR_EVENT;
		}
	}
}

static void CallBack_TopicDataAvailable(dds_entity_t ddsReader, void *pUserArg)
{
	AHEADFORM_TOPIC *pstTopic = (AHEADFORM_TOPIC*)pUserArg;
	const arbitration_msgs_msg_MediaVolumeReply *pstDdsSample = NULL;
	void *pSamples[AHEADFORM_DDS_TAKE_CNT_ONCE] = {0};
	dds_sample_info_t szstSampleInfo[AHEADFORM_DDS_TAKE_CNT_ONCE];
	AHEADFORM_VOLUME_REPLY_SAMPLE stSample;
	dds_return_t sampleCnt;
	unsigned int i;
	int ret = RET_SUCCESS;

	if (pstTopic == NULL)
		return;

	if (pthread_mutex_lock(&pstTopic->topicMutex) != 0)
	{
		PRINT_FUNCTION_ERRINFO(ERR_MUTEX_OP_FAILED, __FUNCTION__);
		return;
	}

	do
	{
		if ( (sampleCnt = dds_take(ddsReader, pSamples, szstSampleInfo, AHEADFORM_DDS_TAKE_CNT_ONCE, AHEADFORM_DDS_TAKE_CNT_ONCE)) < 0 )
		{
			PRINT_FUNCTION_ERRINFO(ERR_DDS_TAKE_FAILED, __FUNCTION__);
			break;
		}

		if (sampleCnt == 0)
			break;

		for (i = 0; i < (unsigned int)sampleCnt; i++)
		{
			if (!szstSampleInfo[i].valid_data || pSamples[i] == NULL)
				continue;

			if (pstTopic->topicId != AHEADFORM_TOPIC_ID_VOLUME_REPLY)
			{
				ret = ConvertDdsDomainSample(pstTopic, pSamples[i]);
				if (ret == RET_SUCCESS)
				{
					ret = AheadFormTopic_PushElemAndCallReaderCallbacks(pstTopic, pstTopic->pReceiveElem);
					if (ret != RET_SUCCESS && ret != ERRCODE_EVENTFD_IO_FAILED)
						break;
				}
				else
				{
					AHEADFORM_TOPIC_READER *pstReader = pstTopic->stReaderList.stHead.pNext;

					__atomic_store_n(&pstTopic->lastReceiveResult, ret, __ATOMIC_RELEASE);
					printf("%s() rejected topic = %s: ret=0x%x\r\n", __FUNCTION__, gTopicDescAry[pstTopic->topicId].strTopicName, ret);
					for (; pstReader != NULL; pstReader = pstReader->pNext)
					{
						if (eventfd_write(pstReader->rxDataUpdateEventfd, 1) < 0 && errno != EAGAIN)
							PRINT_FUNCTION_ERRINFO(ERR_EVENTFD_IO_FAILED, __FUNCTION__, "call eventfd_write(fd = %d) failed: %s", pstReader->rxDataUpdateEventfd, strerror(errno));
					}
				}
				ret = RET_SUCCESS;
				continue;
			}
			pstDdsSample = (const arbitration_msgs_msg_MediaVolumeReply*)pSamples[i];
			if (!szstSampleInfo[i].valid_data || pstDdsSample == NULL)
				continue;

			if (pstDdsSample->source_name == NULL || pstDdsSample->source_name[0] == 0 || strnlen(pstDdsSample->source_name, sizeof(stSample.sourceName)) == sizeof(stSample.sourceName))
			{
				PRINT_FUNCTION_ERRINFO(ERR_DATA_PACKET_FORMAT, __FUNCTION__, "invalid or overlong volume reply source_name");
				continue;
			}
			__atomic_store_n(&pstTopic->lastReceiveResult, RET_SUCCESS, __ATOMIC_RELEASE);
			stSample.askId = pstDdsSample->ask_id;
			strcpy(stSample.sourceName, pstDdsSample->source_name);
			stSample.operation = pstDdsSample->operation;
			stSample.retCode = pstDdsSample->ret_code;
			stSample.queryDataValid = pstDdsSample->query_data_valid;
			stSample.stVolume.micVolumePercent = pstDdsSample->mic_volume_percent;
			stSample.stVolume.spkVolumePercent = pstDdsSample->spk_volume_percent;
			stSample.ReceiveTime_us = GetTimeUsCnt_UpToNow();
			if ( (ret = AheadFormTopic_PushElemAndCallReaderCallbacks(pstTopic, &stSample)) != RET_SUCCESS && ret != ERRCODE_EVENTFD_IO_FAILED )
				break;
		}

		if (dds_return_loan(ddsReader, pSamples, sampleCnt) < 0)
		{
			PRINT_FUNCTION_ERRINFO(ERR_DDS_TAKE_FAILED, __FUNCTION__);
			break;
		}

		if (ret != RET_SUCCESS && ret != ERRCODE_EVENTFD_IO_FAILED)
			break;
	} while (sampleCnt == AHEADFORM_DDS_TAKE_CNT_ONCE);

	pthread_mutex_unlock(&pstTopic->topicMutex);
}


unsigned int TopicReader_GetElemSize(AHEADFORM_TOPIC_READER *pstReader)
{
	if (pstReader == NULL || pstReader->stFifoReader.pstFifo == NULL)
		return 0;

	return pstReader->stFifoReader.pstFifo->sizeOf1Elem;
}

int TopicReader_GetLastReceiveResult(AHEADFORM_TOPIC_READER *pstReader)
{
	if (pstReader == NULL || pstReader->pLastReceiveResult == NULL)
		return ERR_ARGV_1;

	return __atomic_load_n(pstReader->pLastReceiveResult, __ATOMIC_ACQUIRE);
}

int TopicWriter_Write(AHEADFORM_TOPIC_WRITER *pstWriter, const void *pElem)
{
	if (pstWriter == NULL)
		return ERR_ARGV_1;

	if (pElem == NULL)
		return ERR_ARGV_2;

	if (pstWriter->topicId == AHEADFORM_TOPIC_ID_VOLUME_ASK)
		return WriteVolumeRequest(pstWriter, pElem);

	return TopicWriter_WriteDomainSample(pstWriter, pElem);
}

static int ConvertDdsDomainSample(AHEADFORM_TOPIC *pstTopic, const void *pDdsSample)
{
	unsigned int i;

	switch (pstTopic->topicId)
	{
		case AHEADFORM_TOPIC_ID_PRESET_MOTION_REPLY:
		{
			const arbitration_msgs_msg_PresetMotionReply *pstDds = (const arbitration_msgs_msg_PresetMotionReply*)pDdsSample;
			AHEADFORM_PRESET_MOTION_REPLY_SAMPLE *pstSample = (AHEADFORM_PRESET_MOTION_REPLY_SAMPLE*)pstTopic->pReceiveElem;

			memset(pstSample, 0, pstTopic->stFifo.sizeOf1Elem);
			pstSample->ReceiveTime_us = GetTimeUsCnt_UpToNow();
			pstSample->stSourceTime.sec = pstDds->stamp.sec;
			pstSample->stSourceTime.nanosec = pstDds->stamp.nanosec;
			pstSample->askId = pstDds->ask_id;
			if (strnlen(pstDds->source_name, sizeof(pstSample->sourceName)) == sizeof(pstSample->sourceName))
				return ERR_DATA_PACKET_FORMAT;

			strcpy(pstSample->sourceName, pstDds->source_name);
			pstSample->retCode = pstDds->ret_code;
			return RET_SUCCESS;
		}

		case AHEADFORM_TOPIC_ID_AUDIO_DEVICE_REPLY:
		{
			const arbitration_msgs_msg_MediaAudioDeviceReply *pstDds = (const arbitration_msgs_msg_MediaAudioDeviceReply*)pDdsSample;
			AHEADFORM_AUDIO_DEVICE_REPLY_SAMPLE *pstSample = (AHEADFORM_AUDIO_DEVICE_REPLY_SAMPLE*)pstTopic->pReceiveElem;

			memset(pstSample, 0, pstTopic->stFifo.sizeOf1Elem);
			pstSample->ReceiveTime_us = GetTimeUsCnt_UpToNow();
			pstSample->stSourceTime.sec = pstDds->stamp.sec;
			pstSample->stSourceTime.nanosec = pstDds->stamp.nanosec;
			pstSample->askId = pstDds->ask_id;
			if (pstDds->source_name == NULL || strnlen(pstDds->source_name, sizeof(pstSample->sourceName)) == sizeof(pstSample->sourceName))
				return ERR_DATA_PACKET_FORMAT;

			strcpy(pstSample->sourceName, pstDds->source_name);
			pstSample->operation = pstDds->operation;
			pstSample->retCode = pstDds->ret_code;
			pstSample->deviceType = pstDds->device_type;
			pstSample->queryDataValid = pstDds->query_data_valid;
			if (pstDds->devices._length > AHEADFORM_AUDIO_DEVICE_CNT_MAX || (pstDds->devices._length != 0 && pstDds->devices._buffer == NULL))
				return ERR_DATA_PACKET_FORMAT;

			pstSample->deviceCnt = pstDds->devices._length;
			for (i = 0; i < pstDds->devices._length; i++)
			{
				if (pstDds->devices._buffer[i].device_id == NULL || strnlen(pstDds->devices._buffer[i].device_id, sizeof(pstSample->szstDevice[i].deviceId)) == sizeof(pstSample->szstDevice[i].deviceId))
					return ERR_DATA_PACKET_FORMAT;

				strcpy(pstSample->szstDevice[i].deviceId, pstDds->devices._buffer[i].device_id);
				if (pstDds->devices._buffer[i].device_name == NULL || strnlen(pstDds->devices._buffer[i].device_name, sizeof(pstSample->szstDevice[i].deviceName)) == sizeof(pstSample->szstDevice[i].deviceName))
					return ERR_DATA_PACKET_FORMAT;

				strcpy(pstSample->szstDevice[i].deviceName, pstDds->devices._buffer[i].device_name);
			}
			return RET_SUCCESS;
		}

		case AHEADFORM_TOPIC_ID_MEDIA_CHANNEL_REPLY:
		{
			const arbitration_msgs_msg_MediaChannelControlReply *pstDds = (const arbitration_msgs_msg_MediaChannelControlReply*)pDdsSample;
			AHEADFORM_MEDIA_CHANNEL_REPLY_SAMPLE *pstSample = (AHEADFORM_MEDIA_CHANNEL_REPLY_SAMPLE*)pstTopic->pReceiveElem;

			memset(pstSample, 0, pstTopic->stFifo.sizeOf1Elem);
			pstSample->ReceiveTime_us = GetTimeUsCnt_UpToNow();
			pstSample->stSourceTime.sec = pstDds->stamp.sec;
			pstSample->stSourceTime.nanosec = pstDds->stamp.nanosec;
			pstSample->askId = pstDds->ask_id;
			if (pstDds->source_name == NULL || strnlen(pstDds->source_name, sizeof(pstSample->sourceName)) == sizeof(pstSample->sourceName))
				return ERR_DATA_PACKET_FORMAT;

			strcpy(pstSample->sourceName, pstDds->source_name);
			pstSample->retCode = pstDds->ret_code;
			return RET_SUCCESS;
		}

		case AHEADFORM_TOPIC_ID_HARDWARE_STATE:
		{
			const arbitration_msgs_msg_HardwareState *pstDds = (const arbitration_msgs_msg_HardwareState*)pDdsSample;
			AHEADFORM_HARDWARE_STATE_SAMPLE *pstSample = (AHEADFORM_HARDWARE_STATE_SAMPLE*)pstTopic->pReceiveElem;

			memset(pstSample, 0, pstTopic->stFifo.sizeOf1Elem);
			pstSample->ReceiveTime_us = GetTimeUsCnt_UpToNow();
			pstSample->stSourceTime.sec = pstDds->stamp.sec;
			pstSample->stSourceTime.nanosec = pstDds->stamp.nanosec;
			pstSample->arbiterAlive = pstDds->arbiter_alive;
			pstSample->arbiterErrorCode = pstDds->arbiter_error_code;
			if (strnlen(pstDds->arbiter_error_reason, sizeof(pstSample->arbiterErrorReason)) == sizeof(pstSample->arbiterErrorReason))
				return ERR_DATA_PACKET_FORMAT;

			strcpy(pstSample->arbiterErrorReason, pstDds->arbiter_error_reason);
			pstSample->allOnline = pstDds->all_online;
			pstSample->allReady = pstDds->all_ready;
			pstSample->anyFaultActive = pstDds->any_fault_active;
			pstSample->anyEstopLatched = pstDds->any_estop_latched;
			if (strnlen(pstDds->active_motion_source_name, sizeof(pstSample->activeMotionSourceName)) == sizeof(pstSample->activeMotionSourceName))
				return ERR_DATA_PACKET_FORMAT;

			strcpy(pstSample->activeMotionSourceName, pstDds->active_motion_source_name);
			pstSample->faceMotionPermitted = pstDds->face_motion_permitted;
			pstSample->neckMotionPermitted = pstDds->neck_motion_permitted;
			pstSample->safeStopMotionBlockActive = pstDds->safe_stop_motion_block_active;
			return RET_SUCCESS;
		}

		case AHEADFORM_TOPIC_ID_FACE_STATE:
		{
			const arbitration_msgs_msg_FaceState *pstDds = (const arbitration_msgs_msg_FaceState*)pDdsSample;
			AHEADFORM_FACE_STATE_SAMPLE *pstSample = (AHEADFORM_FACE_STATE_SAMPLE*)pstTopic->pReceiveElem;

			memset(pstSample, 0, pstTopic->stFifo.sizeOf1Elem);
			pstSample->ReceiveTime_us = GetTimeUsCnt_UpToNow();
			pstSample->stSourceTime.sec = pstDds->stamp.sec;
			pstSample->stSourceTime.nanosec = pstDds->stamp.nanosec;
			pstSample->stDeviceState.stConnection.handshaked = pstDds->device_state.connection.handshaked;
			pstSample->stDeviceState.stConnection.hasControl = pstDds->device_state.connection.has_control;
			pstSample->stDeviceState.stConnection.statusPushGranted = pstDds->device_state.connection.status_push_granted;
			pstSample->stDeviceState.stConnection.estopPermitGranted = pstDds->device_state.connection.estop_permit_granted;
			pstSample->stDeviceState.stConnection.daemonEndpoint = pstDds->device_state.connection.daemon_endpoint;
			pstSample->stDeviceState.stConnection.daemonState = pstDds->device_state.connection.daemon_state;
			pstSample->stDeviceState.stConnection.helloErrCode = pstDds->device_state.connection.hello_err_code;
			pstSample->stDeviceState.stConnection.faultActive = pstDds->device_state.connection.fault_active;
			if (pstDds->device_state.connection.daemon_name == NULL || strnlen(pstDds->device_state.connection.daemon_name, sizeof(pstSample->stDeviceState.stConnection.daemonName)) == sizeof(pstSample->stDeviceState.stConnection.daemonName))
				return ERR_DATA_PACKET_FORMAT;

			strcpy(pstSample->stDeviceState.stConnection.daemonName, pstDds->device_state.connection.daemon_name);
			pstSample->stDeviceState.stOperationalStatus.online = pstDds->device_state.operational_status.online;
			pstSample->stDeviceState.stOperationalStatus.ready = pstDds->device_state.operational_status.ready;
			pstSample->stDeviceState.stOperationalStatus.state = pstDds->device_state.operational_status.state;
			pstSample->stDeviceState.stOperationalStatus.errCode = pstDds->device_state.operational_status.err_code;
			pstSample->stDeviceState.stOperationalStatus.faultActive = pstDds->device_state.operational_status.fault_active;
			pstSample->stDeviceState.stOperationalStatus.ownerOnline = pstDds->device_state.operational_status.owner_online;
			pstSample->stDeviceState.stOperationalStatus.canOk = pstDds->device_state.operational_status.can_ok;
			pstSample->stDeviceState.stOperationalStatus.reached = pstDds->device_state.operational_status.reached;
			pstSample->stDeviceState.stOperationalStatus.lastAcceptedTargetId = pstDds->device_state.operational_status.last_accepted_target_id;
			pstSample->stDeviceState.stOperationalStatus.targetHasExec_ms = pstDds->device_state.operational_status.target_has_exec_ms;
			pstSample->stDeviceState.stOperationalStatus.trajectoryPlanRun_ms = pstDds->device_state.operational_status.trajectory_plan_run_ms;
			pstSample->hasLatestSample = pstDds->has_latest_sample;
			pstSample->stLatestSample.stSourceTime.sec = pstDds->latest_sample.stamp.sec;
			pstSample->stLatestSample.stSourceTime.nanosec = pstDds->latest_sample.stamp.nanosec;
			pstSample->stLatestSample.sampleSeq = pstDds->latest_sample.sample_seq;
			pstSample->stLatestSample.msgId = pstDds->latest_sample.msg_id;
			pstSample->stLatestSample.refMsgId = pstDds->latest_sample.ref_msg_id;
			pstSample->stLatestSample.daemonId = pstDds->latest_sample.daemon_id;
			pstSample->stLatestSample.state = pstDds->latest_sample.state;
			pstSample->stLatestSample.errCode = pstDds->latest_sample.err_code;
			pstSample->stLatestSample.ready = pstDds->latest_sample.ready;
			pstSample->stLatestSample.online = pstDds->latest_sample.online;
			pstSample->stLatestSample.faultActive = pstDds->latest_sample.fault_active;
			pstSample->stLatestSample.ownerOnline = pstDds->latest_sample.owner_online;
			pstSample->stLatestSample.lastAcceptedTargetId = pstDds->latest_sample.last_accepted_target_id;
			pstSample->stLatestSample.statusSeq = pstDds->latest_sample.status_seq;
			pstSample->stLatestSample.statusBuildTime_us = pstDds->latest_sample.status_build_time_us;
			pstSample->stLatestSample.faceCtrlFaultCode = pstDds->latest_sample.face_ctrl_fault_code;
			pstSample->stLatestSample.allServoReady = pstDds->latest_sample.all_servo_ready;
			pstSample->stLatestSample.servoCount = pstDds->latest_sample.servo_count;
			for (i = 0; i < 20; i++)
			{
				pstSample->stLatestSample.szstServo[i].servoId = pstDds->latest_sample.servos[i].servo_id;
				pstSample->stLatestSample.szstServo[i].targetLogic_deg = pstDds->latest_sample.servos[i].target_logic_deg;
				pstSample->stLatestSample.szstServo[i].motionRefLogic_deg = pstDds->latest_sample.servos[i].motion_ref_logic_deg;
				pstSample->stLatestSample.szstServo[i].feedbackTargetLogic_deg = pstDds->latest_sample.servos[i].feedback_target_logic_deg;
				pstSample->stLatestSample.szstServo[i].currentLogic_deg = pstDds->latest_sample.servos[i].current_logic_deg;
				pstSample->stLatestSample.szstServo[i].logicSpeed_deg_s = pstDds->latest_sample.servos[i].logic_speed_deg_s;
				pstSample->stLatestSample.szstServo[i].temperatureC = pstDds->latest_sample.servos[i].temperature_c;
				pstSample->stLatestSample.szstServo[i].voltageV = pstDds->latest_sample.servos[i].voltage_v;
				pstSample->stLatestSample.szstServo[i].currentA = pstDds->latest_sample.servos[i].current_a;
				pstSample->stLatestSample.szstServo[i].feedbackAge_ms = pstDds->latest_sample.servos[i].feedback_age_ms;
				pstSample->stLatestSample.szstServo[i].hlsPresentStatus = pstDds->latest_sample.servos[i].hls_present_status;
				pstSample->stLatestSample.szstServo[i].faultCode = pstDds->latest_sample.servos[i].fault_code;
			}
			return RET_SUCCESS;
		}

		case AHEADFORM_TOPIC_ID_NECK_STATE:
		{
			const arbitration_msgs_msg_NeckState *pstDds = (const arbitration_msgs_msg_NeckState*)pDdsSample;
			AHEADFORM_NECK_STATE_SAMPLE *pstSample = (AHEADFORM_NECK_STATE_SAMPLE*)pstTopic->pReceiveElem;

			memset(pstSample, 0, pstTopic->stFifo.sizeOf1Elem);
			pstSample->ReceiveTime_us = GetTimeUsCnt_UpToNow();
			pstSample->stSourceTime.sec = pstDds->stamp.sec;
			pstSample->stSourceTime.nanosec = pstDds->stamp.nanosec;
			pstSample->stDeviceState.stConnection.handshaked = pstDds->device_state.connection.handshaked;
			pstSample->stDeviceState.stConnection.hasControl = pstDds->device_state.connection.has_control;
			pstSample->stDeviceState.stConnection.statusPushGranted = pstDds->device_state.connection.status_push_granted;
			pstSample->stDeviceState.stConnection.estopPermitGranted = pstDds->device_state.connection.estop_permit_granted;
			pstSample->stDeviceState.stConnection.daemonEndpoint = pstDds->device_state.connection.daemon_endpoint;
			pstSample->stDeviceState.stConnection.daemonState = pstDds->device_state.connection.daemon_state;
			pstSample->stDeviceState.stConnection.helloErrCode = pstDds->device_state.connection.hello_err_code;
			pstSample->stDeviceState.stConnection.faultActive = pstDds->device_state.connection.fault_active;
			if (pstDds->device_state.connection.daemon_name == NULL || strnlen(pstDds->device_state.connection.daemon_name, sizeof(pstSample->stDeviceState.stConnection.daemonName)) == sizeof(pstSample->stDeviceState.stConnection.daemonName))
				return ERR_DATA_PACKET_FORMAT;

			strcpy(pstSample->stDeviceState.stConnection.daemonName, pstDds->device_state.connection.daemon_name);
			pstSample->stDeviceState.stOperationalStatus.online = pstDds->device_state.operational_status.online;
			pstSample->stDeviceState.stOperationalStatus.ready = pstDds->device_state.operational_status.ready;
			pstSample->stDeviceState.stOperationalStatus.state = pstDds->device_state.operational_status.state;
			pstSample->stDeviceState.stOperationalStatus.errCode = pstDds->device_state.operational_status.err_code;
			pstSample->stDeviceState.stOperationalStatus.faultActive = pstDds->device_state.operational_status.fault_active;
			pstSample->stDeviceState.stOperationalStatus.ownerOnline = pstDds->device_state.operational_status.owner_online;
			pstSample->stDeviceState.stOperationalStatus.canOk = pstDds->device_state.operational_status.can_ok;
			pstSample->stDeviceState.stOperationalStatus.reached = pstDds->device_state.operational_status.reached;
			pstSample->stDeviceState.stOperationalStatus.lastAcceptedTargetId = pstDds->device_state.operational_status.last_accepted_target_id;
			pstSample->stDeviceState.stOperationalStatus.targetHasExec_ms = pstDds->device_state.operational_status.target_has_exec_ms;
			pstSample->stDeviceState.stOperationalStatus.trajectoryPlanRun_ms = pstDds->device_state.operational_status.trajectory_plan_run_ms;
			pstSample->hasLatestSample = pstDds->has_latest_sample;
			pstSample->stLatestSample.stSourceTime.sec = pstDds->latest_sample.stamp.sec;
			pstSample->stLatestSample.stSourceTime.nanosec = pstDds->latest_sample.stamp.nanosec;
			pstSample->stLatestSample.sampleSeq = pstDds->latest_sample.sample_seq;
			pstSample->stLatestSample.msgId = pstDds->latest_sample.msg_id;
			pstSample->stLatestSample.refMsgId = pstDds->latest_sample.ref_msg_id;
			pstSample->stLatestSample.daemonId = pstDds->latest_sample.daemon_id;
			pstSample->stLatestSample.state = pstDds->latest_sample.state;
			pstSample->stLatestSample.errCode = pstDds->latest_sample.err_code;
			pstSample->stLatestSample.ready = pstDds->latest_sample.ready;
			pstSample->stLatestSample.online = pstDds->latest_sample.online;
			pstSample->stLatestSample.faultActive = pstDds->latest_sample.fault_active;
			pstSample->stLatestSample.ownerOnline = pstDds->latest_sample.owner_online;
			pstSample->stLatestSample.canOk = pstDds->latest_sample.can_ok;
			pstSample->stLatestSample.reached = pstDds->latest_sample.reached;
			pstSample->stLatestSample.lastAcceptedTargetId = pstDds->latest_sample.last_accepted_target_id;
			pstSample->stLatestSample.statusSeq = pstDds->latest_sample.status_seq;
			pstSample->stLatestSample.statusBuildTime_us = pstDds->latest_sample.status_build_time_us;
			pstSample->stLatestSample.targetHasExec_ms = pstDds->latest_sample.target_has_exec_ms;
			pstSample->stLatestSample.trajectoryPlanRun_ms = pstDds->latest_sample.trajectory_plan_run_ms;
			pstSample->stLatestSample.stPose.pitch_rad = pstDds->latest_sample.pose.pitch;
			pstSample->stLatestSample.stPose.yaw_rad = pstDds->latest_sample.pose.yaw;
			pstSample->stLatestSample.stPose.roll_rad = pstDds->latest_sample.pose.roll;
			pstSample->stLatestSample.stPose.headX = pstDds->latest_sample.pose.head_x;
			pstSample->stLatestSample.stPose.headY = pstDds->latest_sample.pose.head_y;
			pstSample->stLatestSample.stPose.headZ = pstDds->latest_sample.pose.head_z;
			for (i = 0; i < 3; i++)
			{
				pstSample->stLatestSample.szstMotor[i].motorId = pstDds->latest_sample.motors[i].motor_id;
				pstSample->stLatestSample.szstMotor[i].feedbackTimeout = pstDds->latest_sample.motors[i].feedback_timeout;
				pstSample->stLatestSample.szstMotor[i].lkMotorState = pstDds->latest_sample.motors[i].lk_motor_state;
				pstSample->stLatestSample.szstMotor[i].errorState = pstDds->latest_sample.motors[i].error_state;
				pstSample->stLatestSample.szstMotor[i].sampleAge_ms = pstDds->latest_sample.motors[i].sample_age_ms;
				pstSample->stLatestSample.szstMotor[i].target_deg = pstDds->latest_sample.motors[i].target_deg;
				pstSample->stLatestSample.szstMotor[i].lastMotionRef_deg = pstDds->latest_sample.motors[i].last_motion_ref_deg;
				pstSample->stLatestSample.szstMotor[i].currentRaw_deg = pstDds->latest_sample.motors[i].current_deg_raw;
				pstSample->stLatestSample.szstMotor[i].currentFiltered_deg = pstDds->latest_sample.motors[i].current_deg_filtered;
				pstSample->stLatestSample.szstMotor[i].speed_deg_s = pstDds->latest_sample.motors[i].speed_deg_s;
				pstSample->stLatestSample.szstMotor[i].temperatureC = pstDds->latest_sample.motors[i].temperature_c;
				pstSample->stLatestSample.szstMotor[i].voltageV = pstDds->latest_sample.motors[i].voltage_v;
				pstSample->stLatestSample.szstMotor[i].currentA = pstDds->latest_sample.motors[i].current_a;
			}
			return RET_SUCCESS;
		}

		case AHEADFORM_TOPIC_ID_MEDIA_STATE:
		{
			const arbitration_msgs_msg_MediaState *pstDds = (const arbitration_msgs_msg_MediaState*)pDdsSample;
			AHEADFORM_MEDIA_STATE_SAMPLE *pstSample = (AHEADFORM_MEDIA_STATE_SAMPLE*)pstTopic->pReceiveElem;

			memset(pstSample, 0, pstTopic->stFifo.sizeOf1Elem);
			pstSample->ReceiveTime_us = GetTimeUsCnt_UpToNow();
			pstSample->stSourceTime.sec = pstDds->stamp.sec;
			pstSample->stSourceTime.nanosec = pstDds->stamp.nanosec;
			pstSample->stDeviceState.stConnection.handshaked = pstDds->device_state.connection.handshaked;
			pstSample->stDeviceState.stConnection.hasControl = pstDds->device_state.connection.has_control;
			pstSample->stDeviceState.stConnection.statusPushGranted = pstDds->device_state.connection.status_push_granted;
			pstSample->stDeviceState.stConnection.estopPermitGranted = pstDds->device_state.connection.estop_permit_granted;
			pstSample->stDeviceState.stConnection.daemonEndpoint = pstDds->device_state.connection.daemon_endpoint;
			pstSample->stDeviceState.stConnection.daemonState = pstDds->device_state.connection.daemon_state;
			pstSample->stDeviceState.stConnection.helloErrCode = pstDds->device_state.connection.hello_err_code;
			pstSample->stDeviceState.stConnection.faultActive = pstDds->device_state.connection.fault_active;
			if (pstDds->device_state.connection.daemon_name == NULL || strnlen(pstDds->device_state.connection.daemon_name, sizeof(pstSample->stDeviceState.stConnection.daemonName)) == sizeof(pstSample->stDeviceState.stConnection.daemonName))
				return ERR_DATA_PACKET_FORMAT;

			strcpy(pstSample->stDeviceState.stConnection.daemonName, pstDds->device_state.connection.daemon_name);
			pstSample->stDeviceState.stOperationalStatus.online = pstDds->device_state.operational_status.online;
			pstSample->stDeviceState.stOperationalStatus.ready = pstDds->device_state.operational_status.ready;
			pstSample->stDeviceState.stOperationalStatus.state = pstDds->device_state.operational_status.state;
			pstSample->stDeviceState.stOperationalStatus.errCode = pstDds->device_state.operational_status.err_code;
			pstSample->stDeviceState.stOperationalStatus.faultActive = pstDds->device_state.operational_status.fault_active;
			pstSample->stDeviceState.stOperationalStatus.ownerOnline = pstDds->device_state.operational_status.owner_online;
			pstSample->stDeviceState.stOperationalStatus.canOk = pstDds->device_state.operational_status.can_ok;
			pstSample->stDeviceState.stOperationalStatus.reached = pstDds->device_state.operational_status.reached;
			pstSample->stDeviceState.stOperationalStatus.lastAcceptedTargetId = pstDds->device_state.operational_status.last_accepted_target_id;
			pstSample->stDeviceState.stOperationalStatus.targetHasExec_ms = pstDds->device_state.operational_status.target_has_exec_ms;
			pstSample->stDeviceState.stOperationalStatus.trajectoryPlanRun_ms = pstDds->device_state.operational_status.trajectory_plan_run_ms;
			pstSample->hasLatestSample = pstDds->has_latest_sample;
			pstSample->stLatestSample.stSourceTime.sec = pstDds->latest_sample.stamp.sec;
			pstSample->stLatestSample.stSourceTime.nanosec = pstDds->latest_sample.stamp.nanosec;
			pstSample->stLatestSample.sampleSeq = pstDds->latest_sample.sample_seq;
			pstSample->stLatestSample.msgId = pstDds->latest_sample.msg_id;
			pstSample->stLatestSample.refMsgId = pstDds->latest_sample.ref_msg_id;
			pstSample->stLatestSample.daemonId = pstDds->latest_sample.daemon_id;
			pstSample->stLatestSample.state = pstDds->latest_sample.state;
			pstSample->stLatestSample.errCode = pstDds->latest_sample.err_code;
			pstSample->stLatestSample.ready = pstDds->latest_sample.ready;
			pstSample->stLatestSample.online = pstDds->latest_sample.online;
			pstSample->stLatestSample.faultActive = pstDds->latest_sample.fault_active;
			pstSample->stLatestSample.ownerOnline = pstDds->latest_sample.owner_online;
			pstSample->stLatestSample.mediaTransClientConnected = pstDds->latest_sample.media_trans_client_connected;
			pstSample->stLatestSample.pulseAudioAecReady = pstDds->latest_sample.pulse_audio_aec_ready;
			pstSample->stLatestSample.video.channelState = pstDds->latest_sample.video.channel_state;
			pstSample->stLatestSample.video.deviceAvailable = pstDds->latest_sample.video.device_available;
			pstSample->stLatestSample.video.activeReaderMask = pstDds->latest_sample.video.active_reader_mask;
			pstSample->stLatestSample.video.readableFrameCount = pstDds->latest_sample.video.readable_frame_count;
			pstSample->stLatestSample.video.lastSequence = pstDds->latest_sample.video.last_sequence;
			pstSample->stLatestSample.video.droppedFrameCount = pstDds->latest_sample.video.dropped_frame_count;
			pstSample->stLatestSample.mic.channelState = pstDds->latest_sample.mic.channel_state;
			pstSample->stLatestSample.mic.deviceAvailable = pstDds->latest_sample.mic.device_available;
			pstSample->stLatestSample.mic.activeReaderMask = pstDds->latest_sample.mic.active_reader_mask;
			pstSample->stLatestSample.mic.readableFrameCount = pstDds->latest_sample.mic.readable_frame_count;
			pstSample->stLatestSample.mic.lastSequence = pstDds->latest_sample.mic.last_sequence;
			pstSample->stLatestSample.mic.droppedFrameCount = pstDds->latest_sample.mic.dropped_frame_count;
			pstSample->stLatestSample.playback.channelState = pstDds->latest_sample.playback.channel_state;
			pstSample->stLatestSample.playback.deviceAvailable = pstDds->latest_sample.playback.device_available;
			pstSample->stLatestSample.playback.activeReaderMask = pstDds->latest_sample.playback.active_reader_mask;
			pstSample->stLatestSample.playback.readableFrameCount = pstDds->latest_sample.playback.readable_frame_count;
			pstSample->stLatestSample.playback.lastSequence = pstDds->latest_sample.playback.last_sequence;
			pstSample->stLatestSample.playback.droppedFrameCount = pstDds->latest_sample.playback.dropped_frame_count;
			pstSample->stLatestSample.vad.channelState = pstDds->latest_sample.vad.channel_state;
			pstSample->stLatestSample.vad.deviceAvailable = pstDds->latest_sample.vad.device_available;
			pstSample->stLatestSample.vad.activeReaderMask = pstDds->latest_sample.vad.active_reader_mask;
			pstSample->stLatestSample.vad.readableFrameCount = pstDds->latest_sample.vad.readable_frame_count;
			pstSample->stLatestSample.vad.lastSequence = pstDds->latest_sample.vad.last_sequence;
			pstSample->stLatestSample.vad.droppedFrameCount = pstDds->latest_sample.vad.dropped_frame_count;
			return RET_SUCCESS;
		}

		case AHEADFORM_TOPIC_ID_POWER_STATE:
		{
			const arbitration_msgs_msg_PowerState *pstDds = (const arbitration_msgs_msg_PowerState*)pDdsSample;
			AHEADFORM_POWER_STATE_SAMPLE *pstSample = (AHEADFORM_POWER_STATE_SAMPLE*)pstTopic->pReceiveElem;

			memset(pstSample, 0, pstTopic->stFifo.sizeOf1Elem);
			pstSample->ReceiveTime_us = GetTimeUsCnt_UpToNow();
			pstSample->stSourceTime.sec = pstDds->stamp.sec;
			pstSample->stSourceTime.nanosec = pstDds->stamp.nanosec;
			pstSample->stDeviceState.stConnection.handshaked = pstDds->device_state.connection.handshaked;
			pstSample->stDeviceState.stConnection.hasControl = pstDds->device_state.connection.has_control;
			pstSample->stDeviceState.stConnection.statusPushGranted = pstDds->device_state.connection.status_push_granted;
			pstSample->stDeviceState.stConnection.estopPermitGranted = pstDds->device_state.connection.estop_permit_granted;
			pstSample->stDeviceState.stConnection.daemonEndpoint = pstDds->device_state.connection.daemon_endpoint;
			pstSample->stDeviceState.stConnection.daemonState = pstDds->device_state.connection.daemon_state;
			pstSample->stDeviceState.stConnection.helloErrCode = pstDds->device_state.connection.hello_err_code;
			pstSample->stDeviceState.stConnection.faultActive = pstDds->device_state.connection.fault_active;
			if (pstDds->device_state.connection.daemon_name == NULL || strnlen(pstDds->device_state.connection.daemon_name, sizeof(pstSample->stDeviceState.stConnection.daemonName)) == sizeof(pstSample->stDeviceState.stConnection.daemonName))
				return ERR_DATA_PACKET_FORMAT;

			strcpy(pstSample->stDeviceState.stConnection.daemonName, pstDds->device_state.connection.daemon_name);
			pstSample->stDeviceState.stOperationalStatus.online = pstDds->device_state.operational_status.online;
			pstSample->stDeviceState.stOperationalStatus.ready = pstDds->device_state.operational_status.ready;
			pstSample->stDeviceState.stOperationalStatus.state = pstDds->device_state.operational_status.state;
			pstSample->stDeviceState.stOperationalStatus.errCode = pstDds->device_state.operational_status.err_code;
			pstSample->stDeviceState.stOperationalStatus.faultActive = pstDds->device_state.operational_status.fault_active;
			pstSample->stDeviceState.stOperationalStatus.ownerOnline = pstDds->device_state.operational_status.owner_online;
			pstSample->stDeviceState.stOperationalStatus.canOk = pstDds->device_state.operational_status.can_ok;
			pstSample->stDeviceState.stOperationalStatus.reached = pstDds->device_state.operational_status.reached;
			pstSample->stDeviceState.stOperationalStatus.lastAcceptedTargetId = pstDds->device_state.operational_status.last_accepted_target_id;
			pstSample->stDeviceState.stOperationalStatus.targetHasExec_ms = pstDds->device_state.operational_status.target_has_exec_ms;
			pstSample->stDeviceState.stOperationalStatus.trajectoryPlanRun_ms = pstDds->device_state.operational_status.trajectory_plan_run_ms;
			pstSample->hasLatestSample = pstDds->has_latest_sample;
			pstSample->stLatestSample.stSourceTime.sec = pstDds->latest_sample.stamp.sec;
			pstSample->stLatestSample.stSourceTime.nanosec = pstDds->latest_sample.stamp.nanosec;
			pstSample->stLatestSample.sampleSeq = pstDds->latest_sample.sample_seq;
			pstSample->stLatestSample.msgId = pstDds->latest_sample.msg_id;
			pstSample->stLatestSample.refMsgId = pstDds->latest_sample.ref_msg_id;
			pstSample->stLatestSample.statusBuildTime_us = pstDds->latest_sample.status_build_time_us;
			pstSample->stLatestSample.daemonId = pstDds->latest_sample.daemon_id;
			pstSample->stLatestSample.state = pstDds->latest_sample.state;
			pstSample->stLatestSample.errCode = pstDds->latest_sample.err_code;
			pstSample->stLatestSample.ready = pstDds->latest_sample.ready;
			pstSample->stLatestSample.online = pstDds->latest_sample.online;
			pstSample->stLatestSample.faultActive = pstDds->latest_sample.fault_active;
			pstSample->stLatestSample.ownerOnline = pstDds->latest_sample.owner_online;
			pstSample->stLatestSample.requestUpperPowerOn = pstDds->latest_sample.request_upper_power_on;
			pstSample->stLatestSample.requestLowerPowerOn = pstDds->latest_sample.request_lower_power_on;
			pstSample->stLatestSample.upperPowerOn = pstDds->latest_sample.upper_power_on;
			pstSample->stLatestSample.lowerPowerOn = pstDds->latest_sample.lower_power_on;
			pstSample->stLatestSample.requestMainPowerOff = pstDds->latest_sample.request_main_power_off;
			pstSample->stLatestSample.camWorkStateLedOn = pstDds->latest_sample.cam_work_state_led_on;
			pstSample->stLatestSample.powerSwitchReleased = pstDds->latest_sample.power_switch_released;
			pstSample->stLatestSample.fanRotationSpeedPercent = pstDds->latest_sample.fan_rotation_speed_percent;
			pstSample->stLatestSample.tempLimitC = pstDds->latest_sample.temp_limit_c;
			memcpy(pstSample->stLatestSample.upperTemp_C, pstDds->latest_sample.upper_temp_c, sizeof(pstSample->stLatestSample.upperTemp_C));
			pstSample->stLatestSample.upperBoardFeedbackAge_ms = pstDds->latest_sample.upper_board_feedback_age_ms;
			pstSample->stLatestSample.lowerBoardFeedbackAge_ms = pstDds->latest_sample.lower_board_feedback_age_ms;
			return RET_SUCCESS;
		}

		case AHEADFORM_TOPIC_ID_SENSOR_STATE:
		{
			const arbitration_msgs_msg_SensorState *pstDds = (const arbitration_msgs_msg_SensorState*)pDdsSample;
			AHEADFORM_SENSOR_STATE_SAMPLE *pstSample = (AHEADFORM_SENSOR_STATE_SAMPLE*)pstTopic->pReceiveElem;

			memset(pstSample, 0, pstTopic->stFifo.sizeOf1Elem);
			pstSample->ReceiveTime_us = GetTimeUsCnt_UpToNow();
			pstSample->stSourceTime.sec = pstDds->stamp.sec;
			pstSample->stSourceTime.nanosec = pstDds->stamp.nanosec;
			pstSample->stDeviceState.stConnection.handshaked = pstDds->device_state.connection.handshaked;
			pstSample->stDeviceState.stConnection.hasControl = pstDds->device_state.connection.has_control;
			pstSample->stDeviceState.stConnection.statusPushGranted = pstDds->device_state.connection.status_push_granted;
			pstSample->stDeviceState.stConnection.estopPermitGranted = pstDds->device_state.connection.estop_permit_granted;
			pstSample->stDeviceState.stConnection.daemonEndpoint = pstDds->device_state.connection.daemon_endpoint;
			pstSample->stDeviceState.stConnection.daemonState = pstDds->device_state.connection.daemon_state;
			pstSample->stDeviceState.stConnection.helloErrCode = pstDds->device_state.connection.hello_err_code;
			pstSample->stDeviceState.stConnection.faultActive = pstDds->device_state.connection.fault_active;
			if (pstDds->device_state.connection.daemon_name == NULL || strnlen(pstDds->device_state.connection.daemon_name, sizeof(pstSample->stDeviceState.stConnection.daemonName)) == sizeof(pstSample->stDeviceState.stConnection.daemonName))
				return ERR_DATA_PACKET_FORMAT;

			strcpy(pstSample->stDeviceState.stConnection.daemonName, pstDds->device_state.connection.daemon_name);
			pstSample->stDeviceState.stOperationalStatus.online = pstDds->device_state.operational_status.online;
			pstSample->stDeviceState.stOperationalStatus.ready = pstDds->device_state.operational_status.ready;
			pstSample->stDeviceState.stOperationalStatus.state = pstDds->device_state.operational_status.state;
			pstSample->stDeviceState.stOperationalStatus.errCode = pstDds->device_state.operational_status.err_code;
			pstSample->stDeviceState.stOperationalStatus.faultActive = pstDds->device_state.operational_status.fault_active;
			pstSample->stDeviceState.stOperationalStatus.ownerOnline = pstDds->device_state.operational_status.owner_online;
			pstSample->stDeviceState.stOperationalStatus.canOk = pstDds->device_state.operational_status.can_ok;
			pstSample->stDeviceState.stOperationalStatus.reached = pstDds->device_state.operational_status.reached;
			pstSample->stDeviceState.stOperationalStatus.lastAcceptedTargetId = pstDds->device_state.operational_status.last_accepted_target_id;
			pstSample->stDeviceState.stOperationalStatus.targetHasExec_ms = pstDds->device_state.operational_status.target_has_exec_ms;
			pstSample->stDeviceState.stOperationalStatus.trajectoryPlanRun_ms = pstDds->device_state.operational_status.trajectory_plan_run_ms;
			pstSample->hasLatestSample = pstDds->has_latest_sample;
			pstSample->stLatestSample.stSourceTime.sec = pstDds->latest_sample.stamp.sec;
			pstSample->stLatestSample.stSourceTime.nanosec = pstDds->latest_sample.stamp.nanosec;
			pstSample->stLatestSample.sampleSeq = pstDds->latest_sample.sample_seq;
			pstSample->stLatestSample.msgId = pstDds->latest_sample.msg_id;
			pstSample->stLatestSample.refMsgId = pstDds->latest_sample.ref_msg_id;
			pstSample->stLatestSample.daemonId = pstDds->latest_sample.daemon_id;
			pstSample->stLatestSample.state = pstDds->latest_sample.state;
			pstSample->stLatestSample.errCode = pstDds->latest_sample.err_code;
			pstSample->stLatestSample.ready = pstDds->latest_sample.ready;
			pstSample->stLatestSample.online = pstDds->latest_sample.online;
			pstSample->stLatestSample.faultActive = pstDds->latest_sample.fault_active;
			pstSample->stLatestSample.ownerOnline = pstDds->latest_sample.owner_online;
			pstSample->stLatestSample.statusSeq = pstDds->latest_sample.status_seq;
			pstSample->stLatestSample.statusBuildTime_us = pstDds->latest_sample.status_build_time_us;
			pstSample->stLatestSample.imuRpyFeedbackAge_ms = pstDds->latest_sample.imu_rpy_feedback_age_ms;
			pstSample->stLatestSample.roll_rad = pstDds->latest_sample.roll;
			pstSample->stLatestSample.pitch_rad = pstDds->latest_sample.pitch;
			pstSample->stLatestSample.yaw_rad = pstDds->latest_sample.yaw;
			return RET_SUCCESS;
		}

		case AHEADFORM_TOPIC_ID_FAULT_SET:
		{
			const arbitration_msgs_msg_HardwareFaultActiveSet *pstDds = (const arbitration_msgs_msg_HardwareFaultActiveSet*)pDdsSample;
			AHEADFORM_FAULT_SET_SAMPLE *pstSample = (AHEADFORM_FAULT_SET_SAMPLE*)pstTopic->pReceiveElem;

			memset(pstSample, 0, pstTopic->stFifo.sizeOf1Elem);
			pstSample->ReceiveTime_us = GetTimeUsCnt_UpToNow();
			if (pstDds->faults._length > (pstTopic->stFifo.sizeOf1Elem - sizeof(*pstSample)) / sizeof(AHEADFORM_FAULT_RECORD))
				return ERR_OVER_BUF;

			if (pstDds->faults._length != 0 && pstDds->faults._buffer == NULL)
				return ERR_DATA_PACKET_FORMAT;

			pstSample->sourceTime_sec = pstDds->stamp.sec;
			pstSample->sourceTime_nanosec = pstDds->stamp.nanosec;
			pstSample->faultRecordCnt = pstDds->faults._length;
			for (i = 0; i < pstDds->faults._length; i++)
			{
				pstSample->szstFaultRecord[i].device = pstDds->faults._buffer[i].device.value;
				pstSample->szstFaultRecord[i].source = pstDds->faults._buffer[i].source.value;
				pstSample->szstFaultRecord[i].objectId = pstDds->faults._buffer[i].object_id;
				pstSample->szstFaultRecord[i].errCode = pstDds->faults._buffer[i].err_code;
			}
			return RET_SUCCESS;
		}

		case AHEADFORM_TOPIC_ID_FAULT_EVENT:
		{
			const arbitration_msgs_msg_HardwareFaultEvent *pstDds = (const arbitration_msgs_msg_HardwareFaultEvent*)pDdsSample;
			AHEADFORM_FAULT_EVENT_SAMPLE *pstSample = (AHEADFORM_FAULT_EVENT_SAMPLE*)pstTopic->pReceiveElem;

			memset(pstSample, 0, pstTopic->stFifo.sizeOf1Elem);
			pstSample->ReceiveTime_us = GetTimeUsCnt_UpToNow();
			pstSample->stSourceTime.sec = pstDds->stamp.sec;
			pstSample->stSourceTime.nanosec = pstDds->stamp.nanosec;
			pstSample->sourceSeq = pstDds->sequence;
			pstSample->eventType = pstDds->event_type;
			pstSample->stFault.device = pstDds->fault.device.value;
			pstSample->stFault.source = pstDds->fault.source.value;
			pstSample->stFault.objectId = pstDds->fault.object_id;
			pstSample->stFault.errCode = pstDds->fault.err_code;
			return RET_SUCCESS;
		}

		case AHEADFORM_TOPIC_ID_HARDWARE_CONTROL_REPLY:
		{
			const arbitration_msgs_msg_HardwareControlReply *pstDds = (const arbitration_msgs_msg_HardwareControlReply*)pDdsSample;
			AHEADFORM_HARDWARE_CONTROL_REPLY_SAMPLE *pstSample = (AHEADFORM_HARDWARE_CONTROL_REPLY_SAMPLE*)pstTopic->pReceiveElem;

			memset(pstSample, 0, pstTopic->stFifo.sizeOf1Elem);
			pstSample->ReceiveTime_us = GetTimeUsCnt_UpToNow();
			pstSample->stSourceTime.sec = pstDds->stamp.sec;
			pstSample->stSourceTime.nanosec = pstDds->stamp.nanosec;
			pstSample->askId = pstDds->ask_id;
			if (pstDds->source_name == NULL || strnlen(pstDds->source_name, sizeof(pstSample->sourceName)) == sizeof(pstSample->sourceName))
				return ERR_DATA_PACKET_FORMAT;

			strcpy(pstSample->sourceName, pstDds->source_name);
			pstSample->action = pstDds->action;
			pstSample->device = pstDds->device.value;
			pstSample->retCode = pstDds->ret_code;
			return RET_SUCCESS;
		}

		case AHEADFORM_TOPIC_ID_SAFETY_CONTROL_REPLY:
		{
			const arbitration_msgs_msg_SafetyControlReply *pstDds = (const arbitration_msgs_msg_SafetyControlReply*)pDdsSample;
			AHEADFORM_SAFETY_CONTROL_REPLY_SAMPLE *pstSample = (AHEADFORM_SAFETY_CONTROL_REPLY_SAMPLE*)pstTopic->pReceiveElem;

			memset(pstSample, 0, pstTopic->stFifo.sizeOf1Elem);
			pstSample->ReceiveTime_us = GetTimeUsCnt_UpToNow();
			pstSample->stSourceTime.sec = pstDds->stamp.sec;
			pstSample->stSourceTime.nanosec = pstDds->stamp.nanosec;
			pstSample->askId = pstDds->ask_id;
			if (pstDds->source_name == NULL || strnlen(pstDds->source_name, sizeof(pstSample->sourceName)) == sizeof(pstSample->sourceName))
				return ERR_DATA_PACKET_FORMAT;

			strcpy(pstSample->sourceName, pstDds->source_name);
			pstSample->action = pstDds->action;
			pstSample->retCode = pstDds->ret_code;
			return RET_SUCCESS;
		}

		case AHEADFORM_TOPIC_ID_OPERATING_MODE_REPLY:
		{
			const cloud_agent_msgs_msg_OperatingModeControlReply *pstDds = (const cloud_agent_msgs_msg_OperatingModeControlReply*)pDdsSample;
			AHEADFORM_OPERATING_MODE_REPLY_SAMPLE *pstSample = (AHEADFORM_OPERATING_MODE_REPLY_SAMPLE*)pstTopic->pReceiveElem;

			memset(pstSample, 0, pstTopic->stFifo.sizeOf1Elem);
			pstSample->ReceiveTime_us = GetTimeUsCnt_UpToNow();
			pstSample->requestId = pstDds->request_id;
			pstSample->result = pstDds->result;
			return RET_SUCCESS;
		}

		default:
			return ERR_TOPIC_NOT_FOUND;
	}
}
