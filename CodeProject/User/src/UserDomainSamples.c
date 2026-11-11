#include "UserCmdMenu.h"
#include "Time.h"

#include <strings.h>
#include <sys/eventfd.h>
#include <sys/select.h>
#include <unistd.h>

#define USER_CMD_STREAM_METHOD_RX_EVENTFD 1
#define USER_CMD_STREAM_METHOD_READER_CALLBACK 2
#ifndef USER_CMD_STREAM_METHOD
#define USER_CMD_STREAM_METHOD USER_CMD_STREAM_METHOD_RX_EVENTFD
#endif

int UserCmd_SendNeck(AHEADFORM_ALL_CTRL *pstAheadForm, int argc, char *argv[])
{
	AHEADFORM_NECK_POSE stNeck = {0};
	char *pEnd = NULL;
	float values[3];
	unsigned int i;

	if (argc != 4)
		return ERR_ARGC;

	for (i = 0; i < 3; i++)
	{
		values[i] = strtof(argv[i + 1], &pEnd);
		if (pEnd == argv[i + 1] || *pEnd != 0 || !isfinite(values[i]))
			return ERR_ARGV;
	}
	stNeck.pitch_rad = values[0];
	stNeck.yaw_rad = values[1];
	stNeck.roll_rad = values[2];
	return Head_SendNeck(&pstAheadForm->stRobotCtrl.stHeadCtrl, &stNeck);
}

int UserCmd_SendServo(AHEADFORM_ALL_CTRL *pstAheadForm, int argc, char *argv[])
{
	double targetLogic_deg[AHEADFORM_FACE_SERVO_CNT];
	char *pEnd = NULL;
	unsigned int i;

	if (argc != AHEADFORM_FACE_SERVO_CNT + 1)
		return ERR_ARGC;

	for (i = 0; i < AHEADFORM_FACE_SERVO_CNT; i++)
	{
		targetLogic_deg[i] = strtod(argv[i + 1], &pEnd);
		if (pEnd == argv[i + 1] || *pEnd != 0 || !isfinite(targetLogic_deg[i]))
			return ERR_ARGV;
	}

	return Head_SendServoTargets(&pstAheadForm->stRobotCtrl.stHeadCtrl, targetLogic_deg);
}

int UserCmd_MotionSample(AHEADFORM_ALL_CTRL *pstAheadForm, int argc, char *argv[])
{
	AHEADFORM_MOTION_CLIP *pstClip = NULL;
	AHEADFORM_MOTION_FRAME stFrame;
	unsigned long long t_us = 0;
	char *p;
	int ret;

	if (argc != 4 || (strcasecmp(argv[3], "motion") != 0 && strcasecmp(argv[3], "face") != 0 && strcasecmp(argv[3], "inspect") != 0))
		return ERR_ARGV;

	if (!IsDigitStr(argv[2]) || strncasecmp(argv[2], "0x", 2) == 0)
		return ERR_ARGV;

	for (p = argv[2]; *p != 0; p++)
	{
		if (t_us > (ULLONG_MAX - (*p - '0')) / 10)
			return ERR_ARGV;

		t_us = t_us * 10 + (*p - '0');
	}

	if ( (ret = Head_LoadMotionCsv(argv[1], &pstClip)) != RET_SUCCESS )
		return ret;

	ret = Head_SampleMotionClip(pstClip, t_us, &stFrame);
	if (ret == RET_SUCCESS)
	{
		printf("CSV frames=%u, sample t_us=%llu, neck(rad)=%g,%g,%g.\r\n", pstClip->frameCnt, stFrame.t_us, stFrame.stNeck.pitch_rad, stFrame.stNeck.yaw_rad, stFrame.stNeck.roll_rad);
		if (strcasecmp(argv[3], "motion") == 0)
			ret = Head_SendMotion(&pstAheadForm->stRobotCtrl.stHeadCtrl, &stFrame);
		else if (strcasecmp(argv[3], "face") == 0)
			ret = Head_SendFace(&pstAheadForm->stRobotCtrl.stHeadCtrl, &stFrame.stEyes, stFrame.blendshapes);
	}
	Head_ReleaseMotionClip(pstClip);
	return ret;
}

static void PrintStatusSample(ENUM_AHEADFORM_TOPIC_ID topicId, const void *pSample)
{
	const AHEADFORM_HARDWARE_STATE_SAMPLE *pstHardware = NULL;
	const AHEADFORM_FAULT_SET_SAMPLE *pstFaultSet = NULL;
	const AHEADFORM_FAULT_EVENT_SAMPLE *pstFaultEvent = NULL;
	unsigned int i;

	switch (topicId)
	{
		case AHEADFORM_TOPIC_ID_HARDWARE_STATE:
			pstHardware = (const AHEADFORM_HARDWARE_STATE_SAMPLE*)pSample;
			printf("hardware receive=%llu, online=%u, ready=%u, fault=%u, estop=%u, source=%s.\r\n",
				pstHardware->ReceiveTime_us, pstHardware->allOnline, pstHardware->allReady,
				pstHardware->anyFaultActive, pstHardware->anyEstopLatched, pstHardware->activeMotionSourceName);
			break;

		case AHEADFORM_TOPIC_ID_FACE_STATE:
			printf("face receive=%llu, online=%u, ready=%u, sampleSeq=%llu.\r\n",
				((const AHEADFORM_FACE_STATE_SAMPLE*)pSample)->ReceiveTime_us,
				((const AHEADFORM_FACE_STATE_SAMPLE*)pSample)->stDeviceState.stOperationalStatus.online,
				((const AHEADFORM_FACE_STATE_SAMPLE*)pSample)->stDeviceState.stOperationalStatus.ready,
				((const AHEADFORM_FACE_STATE_SAMPLE*)pSample)->stLatestSample.sampleSeq);
			break;

		case AHEADFORM_TOPIC_ID_NECK_STATE:
			printf("neck receive=%llu, online=%u, ready=%u, sampleSeq=%llu.\r\n",
				((const AHEADFORM_NECK_STATE_SAMPLE*)pSample)->ReceiveTime_us,
				((const AHEADFORM_NECK_STATE_SAMPLE*)pSample)->stDeviceState.stOperationalStatus.online,
				((const AHEADFORM_NECK_STATE_SAMPLE*)pSample)->stDeviceState.stOperationalStatus.ready,
				((const AHEADFORM_NECK_STATE_SAMPLE*)pSample)->stLatestSample.sampleSeq);
			break;

		case AHEADFORM_TOPIC_ID_MEDIA_STATE:
			printf("media receive=%llu, online=%u, ready=%u, sampleSeq=%llu.\r\n",
				((const AHEADFORM_MEDIA_STATE_SAMPLE*)pSample)->ReceiveTime_us,
				((const AHEADFORM_MEDIA_STATE_SAMPLE*)pSample)->stDeviceState.stOperationalStatus.online,
				((const AHEADFORM_MEDIA_STATE_SAMPLE*)pSample)->stDeviceState.stOperationalStatus.ready,
				((const AHEADFORM_MEDIA_STATE_SAMPLE*)pSample)->stLatestSample.sampleSeq);
			break;

		case AHEADFORM_TOPIC_ID_POWER_STATE:
			printf("power receive=%llu, online=%u, ready=%u, sampleSeq=%llu.\r\n",
				((const AHEADFORM_POWER_STATE_SAMPLE*)pSample)->ReceiveTime_us,
				((const AHEADFORM_POWER_STATE_SAMPLE*)pSample)->stDeviceState.stOperationalStatus.online,
				((const AHEADFORM_POWER_STATE_SAMPLE*)pSample)->stDeviceState.stOperationalStatus.ready,
				((const AHEADFORM_POWER_STATE_SAMPLE*)pSample)->stLatestSample.sampleSeq);
			break;

		case AHEADFORM_TOPIC_ID_SENSOR_STATE:
			printf("sensor receive=%llu, online=%u, ready=%u, sampleSeq=%llu.\r\n",
				((const AHEADFORM_SENSOR_STATE_SAMPLE*)pSample)->ReceiveTime_us,
				((const AHEADFORM_SENSOR_STATE_SAMPLE*)pSample)->stDeviceState.stOperationalStatus.online,
				((const AHEADFORM_SENSOR_STATE_SAMPLE*)pSample)->stDeviceState.stOperationalStatus.ready,
				((const AHEADFORM_SENSOR_STATE_SAMPLE*)pSample)->stLatestSample.sampleSeq);
			break;

		case AHEADFORM_TOPIC_ID_FAULT_SET:
			pstFaultSet = (const AHEADFORM_FAULT_SET_SAMPLE*)pSample;
			printf("current fault receive=%llu, recordCnt=%u.\r\n", pstFaultSet->ReceiveTime_us, pstFaultSet->faultRecordCnt);
			for (i = 0; i < pstFaultSet->faultRecordCnt; i++)
				printf("fault[%u] device=%u, source=%u, object=%u, code=%d.\r\n", i, pstFaultSet->szstFaultRecord[i].device, pstFaultSet->szstFaultRecord[i].source, pstFaultSet->szstFaultRecord[i].objectId, pstFaultSet->szstFaultRecord[i].errCode);
			break;

		case AHEADFORM_TOPIC_ID_FAULT_EVENT:
			pstFaultEvent = (const AHEADFORM_FAULT_EVENT_SAMPLE*)pSample;
			printf("fault event seq=%llu, type=%u, device=%u, source=%u, object=%u, code=%d.\r\n",
				pstFaultEvent->sourceSeq, pstFaultEvent->eventType, pstFaultEvent->stFault.device,
				pstFaultEvent->stFault.source, pstFaultEvent->stFault.objectId, pstFaultEvent->stFault.errCode);
			break;

		default:
			break;
	}
}

static void PrintMediaFrame(const AHEADFORM_MEDIA_FRAME *pstFrame, unsigned char isReaderDiscontinuous, void *pUserArg)
{
	printf("media stream=%u, sequence=%llu, bytes=%u, PTS valid=%u, PTS=%llu ns, discontinuous=%u.\r\n", *(const ENUM_AHEADFORM_MEDIA_STREAM*)pUserArg, pstFrame->sequence, pstFrame->dataSize, pstFrame->isPtsValid, pstFrame->pts_ns, isReaderDiscontinuous);
}

int UserCmd_ReadStatus(AHEADFORM_ALL_CTRL *pstAheadForm, int argc, char *argv[])
{
	AHEADFORM_TOPIC_READER *pstReader = NULL;
	void *pSample = NULL;
	AHEADFORM_STATUS_EXPORT_RESULT stExport = {0};
	const char *szNames[AHEADFORM_STATUS_TOPIC_CNT] = {"hardware", "face", "neck", "media", "power", "sensor", "faults", "events"};
	unsigned int i, elemSize, sampleCnt;
	int ret;

	if (argc != 3 && argc != 4)
		return ERR_ARGC;

	for (i = 0; i < AHEADFORM_STATUS_TOPIC_CNT; i++)
	{
		if (strcasecmp(argv[1], szNames[i]) == 0)
			break;
	}

	if (i == AHEADFORM_STATUS_TOPIC_CNT)
		return ERR_ARGV;

	pstReader = pstAheadForm->stRobotCtrl.stStatus.szpstReader[i];
	if (argc == 4 && strcasecmp(argv[2], "export") == 0)
	{
		ret = Status_ExportUnread(pstReader, argv[3], UINT_MAX, &stExport);
		printf("exported=%u, FIFO missed=%llu, source rollback=%u, latest receive result=0x%08X.\r\n", stExport.exportedSampleCnt, stExport.missedElemCnt, stExport.sourceSeqRollbackCnt, stExport.lastReceiveResult);
		return ret;
	}

	if (argc != 3 || (strcasecmp(argv[2], "latest") != 0 && strcasecmp(argv[2], "oldest") != 0 && strcasecmp(argv[2], "all") != 0))
		return ERR_ARGV;

	elemSize = TopicReader_GetElemSize(pstReader);
	if (elemSize == 0)
		return ERR_SDK_NOT_READY;

	if ( (pSample = malloc(elemSize)) == NULL )
		return ERR_MEMORY_ALLOC;

	if (strcasecmp(argv[2], "latest") == 0)
	{
		ret = TopicReader_GetLastReceiveResult(pstReader);
		if (ret == RET_SUCCESS)
			ret = TopicReader_ReadLatest1Elem(pstReader, pSample);

		if (ret == 1)
		{
			ret = TopicReader_GetLastReceiveResult(pstReader);
			if (ret == RET_SUCCESS)
				ret = 1;
		}

		if (ret == 1)
			PrintStatusSample(pstReader->topicId, pSample);
	}
	else
	{
		sampleCnt = strcasecmp(argv[2], "all") == 0 ? TopicReader_GetReadableElemCnt(pstReader) : 1;
		ret = RET_SUCCESS;
		for (i = 0; i < sampleCnt; i++)
		{
			if ( (ret = TopicReader_PopElem(pstReader, pSample)) != 1 )
				break;

			PrintStatusSample(pstReader->topicId, pSample);
		}
	}
	printf("FIFO missed=%llu, latest receive result=0x%08X.\r\n", TopicReader_GetMissedElemCnt(pstReader), TopicReader_GetLastReceiveResult(pstReader));
	free(pSample);
	return ret < 0 ? ret : RET_SUCCESS;
}

#if USER_CMD_STREAM_METHOD == USER_CMD_STREAM_METHOD_READER_CALLBACK
static void PrintPushedStatus(const void *pSample, void *pUserArg)
{
	const ENUM_AHEADFORM_TOPIC_ID *pTopicId = (const ENUM_AHEADFORM_TOPIC_ID*)pUserArg;

	PrintStatusSample(*pTopicId, pSample);
}

int UserCmd_WatchStatus(AHEADFORM_ALL_CTRL *pstAheadForm, int argc, char *argv[])
{
	ENUM_AHEADFORM_TOPIC_ID topicId;
	AHEADFORM_TOPIC_READER_PARA stPara = {PrintPushedStatus, &topicId};
	AHEADFORM_TOPIC_READER *pstReader = NULL;
	struct timeval timeout;
	long seconds;
	int ret, cleanupRet;

	if ( (ret = IsUserInError(argc, argv, "3", "2")) != RET_SUCCESS )
		return ret;

	seconds = strtol(argv[2], NULL, 0);
	if (seconds <= 0 || seconds > INT_MAX)
		return ERR_ARGV;

	if (strcasecmp(argv[1], "hardware") == 0)
		topicId = AHEADFORM_TOPIC_ID_HARDWARE_STATE;
	else if (strcasecmp(argv[1], "events") == 0)
		topicId = AHEADFORM_TOPIC_ID_FAULT_EVENT;
	else
		return ERR_ARGV;

	if ( (pstReader = AheadForm_RegisterTopicReader(pstAheadForm, topicId, &stPara)) == NULL )
		return ERRCODE_INIT_FAIL;

	timeout.tv_sec = seconds;
	timeout.tv_usec = 0;
	ret = select(0, NULL, NULL, NULL, &timeout);
	if (ret < 0)
	{
		printf("%s() call select(status callback window) failed: %s\r\n", __FUNCTION__, strerror(errno));
		ret = ERR_SELECT;
	}

	if ( (cleanupRet = AheadForm_UnregisterTopicReader(pstAheadForm, pstReader)) != RET_SUCCESS )
	{
		printf("%s() unregister status Reader failed: 0x%08X; exiting before context expires.\r\n", __FUNCTION__, cleanupRet);
		exit(EXIT_FAILURE);
	}

	return ret;
}

#elif USER_CMD_STREAM_METHOD == USER_CMD_STREAM_METHOD_RX_EVENTFD
int UserCmd_WatchStatus(AHEADFORM_ALL_CTRL *pstAheadForm, int argc, char *argv[])
{
	AHEADFORM_TOPIC_READER_PARA stPara = {0};
	AHEADFORM_TOPIC_READER *pstReader = NULL;
	ENUM_AHEADFORM_TOPIC_ID topicId;
	AHEADFORM_HARDWARE_STATE_SAMPLE stHardware;
	AHEADFORM_FAULT_EVENT_SAMPLE stEvent;
	void *pSample = NULL;
	struct timeval timeout;
	fd_set rdFdSet;
	eventfd_t eventCnt;
	long seconds;
	unsigned int budget;
	int ret, cleanupRet;

	if ( (ret = IsUserInError(argc, argv, "3", "2")) != RET_SUCCESS )
		return ret;

	seconds = strtol(argv[2], NULL, 0);
	if (seconds <= 0 || seconds > INT_MAX)
		return ERR_ARGV;

	if (strcasecmp(argv[1], "hardware") == 0)
	{
		topicId = AHEADFORM_TOPIC_ID_HARDWARE_STATE;
		pSample = &stHardware;
	}
	else if (strcasecmp(argv[1], "events") == 0)
	{
		topicId = AHEADFORM_TOPIC_ID_FAULT_EVENT;
		pSample = &stEvent;
	}
	else
	{
		return ERR_ARGV;
	}

	if ( (pstReader = AheadForm_RegisterTopicReader(pstAheadForm, topicId, &stPara)) == NULL )
		return ERRCODE_INIT_FAIL;

	timeout.tv_sec = seconds;
	timeout.tv_usec = 0;
	while (1)
	{
		FD_ZERO(&rdFdSet);
		FD_SET(pstReader->rxDataUpdateEventfd, &rdFdSet);
		ret = select(pstReader->rxDataUpdateEventfd + 1, &rdFdSet, NULL, NULL, &timeout);
		if (ret < 0)
		{
			printf("%s() call select(status rx fd = %d) failed: %s\r\n", __FUNCTION__, pstReader->rxDataUpdateEventfd, strerror(errno));
			ret = ERR_SELECT;
			break;
		}
		else if (ret == 0)
		{
			break;
		}

		if (FD_ISSET(pstReader->rxDataUpdateEventfd, &rdFdSet))
		{
			if (eventfd_read(pstReader->rxDataUpdateEventfd, &eventCnt) < 0 && errno != EAGAIN)
			{
				printf("%s() call eventfd_read(status fd = %d) failed: %s\r\n", __FUNCTION__, pstReader->rxDataUpdateEventfd, strerror(errno));
				ret = ERR_EVENTFD_IO_FAILED;
				break;
			}
			budget = TopicReader_GetReadableElemCnt(pstReader);
			while (budget-- != 0 && (ret = TopicReader_PopElem(pstReader, pSample)) == 1)
				PrintStatusSample(topicId, pSample);

			printf("FIFO missed=%llu, latest receive result=0x%08X.\r\n", TopicReader_GetMissedElemCnt(pstReader), TopicReader_GetLastReceiveResult(pstReader));
		}

		if (ret < 0 || (timeout.tv_sec == 0 && timeout.tv_usec == 0))
			break;
	}

	if ( (cleanupRet = AheadForm_UnregisterTopicReader(pstAheadForm, pstReader)) != RET_SUCCESS )
	{
		printf("%s() unregister status Reader failed: 0x%08X; exiting.\r\n", __FUNCTION__, cleanupRet);
		exit(EXIT_FAILURE);
	}

	return ret < 0 ? ret : RET_SUCCESS;
}

#else
#error Invalid USER_CMD_STREAM_METHOD
#endif

#if USER_CMD_STREAM_METHOD == USER_CMD_STREAM_METHOD_READER_CALLBACK
int UserCmd_WatchMedia(AHEADFORM_ALL_CTRL *pstAheadForm, int argc, char *argv[])
{
	AHEADFORM_MEDIA_CTRL *pstMedia = &pstAheadForm->stRobotCtrl.stMediaCtrl;
	AHEADFORM_MEDIA_READER_PARA stPara = {PrintMediaFrame, NULL};
	AHEADFORM_MEDIA_READER *pstReader = NULL;
	ENUM_AHEADFORM_MEDIA_STREAM stream;
	struct timeval timeout;
	long seconds;
	int ret, cleanupRet, stopRet;
	unsigned char isStarted = 0;

	if ( (ret = IsUserInError(argc, argv, "3", "2")) != RET_SUCCESS )
		return ret;

	seconds = strtol(argv[2], NULL, 0);
	if (seconds <= 0 || seconds > INT_MAX)
		return ERR_ARGV;

	if (strcasecmp(argv[1], "video") == 0)
		stream = AHEADFORM_MEDIA_STREAM_VIDEO;
	else if (strcasecmp(argv[1], "mic") == 0)
		stream = AHEADFORM_MEDIA_STREAM_MIC;
	else
		return ERR_ARGV;

	stPara.pUserArg = &stream;
	if ( (pstReader = Media_RegisterFrameReader(pstMedia, stream, &stPara)) == NULL )
		return ERRCODE_INIT_FAIL;

	ret = stream == AHEADFORM_MEDIA_STREAM_VIDEO ? Media_StartVideoReceive(pstMedia) : Media_StartMicReceive(pstMedia);
	if (ret == RET_SUCCESS)
	{
		isStarted = 1;
		timeout.tv_sec = seconds;
		timeout.tv_usec = 0;
		ret = select(0, NULL, NULL, NULL, &timeout);
		if (ret < 0)
		{
			printf("%s() call select(media callback window) failed: %s\r\n", __FUNCTION__, strerror(errno));
			ret = ERR_SELECT;
		}

		printf("media final receive result=0x%08X.\r\n", MediaReader_GetLastReceiveResult(pstReader));
		if (ret >= 0)
			ret = MediaReader_GetLastReceiveResult(pstReader);
	}

	if ( (cleanupRet = Media_UnregisterFrameReader(pstMedia, pstReader)) != RET_SUCCESS )
	{
		printf("%s() Media_UnregisterFrameReader(Reader = %p) failed: 0x%08X; exiting.\r\n", __FUNCTION__, (void*)pstReader, cleanupRet);
		exit(EXIT_FAILURE);
	}

	if (isStarted)
	{
		stopRet = stream == AHEADFORM_MEDIA_STREAM_VIDEO ? Media_StopVideoReceive(pstMedia) : Media_StopMicReceive(pstMedia);
		if (stopRet != RET_SUCCESS)
			ret = stopRet;
	}

	return ret < 0 ? ret : RET_SUCCESS;
}

#elif USER_CMD_STREAM_METHOD == USER_CMD_STREAM_METHOD_RX_EVENTFD
int UserCmd_WatchMedia(AHEADFORM_ALL_CTRL *pstAheadForm, int argc, char *argv[])
{
	AHEADFORM_MEDIA_CTRL *pstMedia = &pstAheadForm->stRobotCtrl.stMediaCtrl;
	AHEADFORM_MEDIA_READER_PARA stPara = {0};
	AHEADFORM_MEDIA_READER *pstReader = NULL;
	ENUM_AHEADFORM_MEDIA_STREAM stream;
	AHEADFORM_MEDIA_FRAME *pstFrame = NULL;
	struct timeval timeout;
	fd_set rdFdSet;
	eventfd_t eventCnt;
	long seconds;
	unsigned int frameBufSize, frameCnt;
	int eventFd, ret, cleanupRet, stopRet;
	unsigned char isStarted = 0;

	if ( (ret = IsUserInError(argc, argv, "3", "2")) != RET_SUCCESS )
		return ret;

	seconds = strtol(argv[2], NULL, 0);
	if (seconds <= 0 || seconds > INT_MAX)
		return ERR_ARGV;

	if (strcasecmp(argv[1], "video") == 0)
		stream = AHEADFORM_MEDIA_STREAM_VIDEO;
	else if (strcasecmp(argv[1], "mic") == 0)
		stream = AHEADFORM_MEDIA_STREAM_MIC;
	else
		return ERR_ARGV;

	if ( (pstReader = Media_RegisterFrameReader(pstMedia, stream, &stPara)) == NULL )
		return ERRCODE_INIT_FAIL;

	ret = stream == AHEADFORM_MEDIA_STREAM_VIDEO ? Media_StartVideoReceive(pstMedia) : Media_StartMicReceive(pstMedia);
	if (ret == RET_SUCCESS)
	{
		isStarted = 1;
		frameBufSize = MediaReader_GetElemSize(pstReader);
		pstFrame = (AHEADFORM_MEDIA_FRAME*)malloc(frameBufSize);
		if (pstFrame == NULL)
			ret = ERR_MEMORY_ALLOC;
		else
		{
			eventFd = MediaReader_GetEventfd(pstReader);
			timeout.tv_sec = seconds;
			timeout.tv_usec = 0;
			while (1)
			{
				FD_ZERO(&rdFdSet);
				FD_SET(eventFd, &rdFdSet);
				ret = select(eventFd + 1, &rdFdSet, NULL, NULL, &timeout);
				if (ret < 0)
				{
					printf("%s() call select(media eventfd = %d) failed: %s\r\n", __FUNCTION__, eventFd, strerror(errno));
					ret = ERR_SELECT;
					break;
				}
				else if (ret == 0)
				{
					break;
				}

				if (FD_ISSET(eventFd, &rdFdSet))
				{
					if (eventfd_read(eventFd, &eventCnt) < 0 && errno != EAGAIN)
					{
						printf("%s() call eventfd_read(media fd = %d) failed: %s\r\n", __FUNCTION__, eventFd, strerror(errno));
						ret = ERR_EVENTFD_IO_FAILED;
						break;
					}

					frameCnt = MediaReader_GetReadableElemCnt(pstReader);
					while (frameCnt-- != 0 && (ret = MediaReader_PopFrame(pstReader, pstFrame, frameBufSize)) == 1)
						PrintMediaFrame(pstFrame, pstFrame->isDiscontinuous, &stream);

					printf("media missed=%llu, receive result=0x%08X.\r\n", MediaReader_GetMissedElemCnt(pstReader), MediaReader_GetLastReceiveResult(pstReader));
				}

				if (ret < 0 || (timeout.tv_sec == 0 && timeout.tv_usec == 0))
					break;
			}
		}

		printf("media final receive result=0x%08X.\r\n", MediaReader_GetLastReceiveResult(pstReader));
		if (ret >= 0)
			ret = MediaReader_GetLastReceiveResult(pstReader);
	}

	if ( (cleanupRet = Media_UnregisterFrameReader(pstMedia, pstReader)) != RET_SUCCESS )
	{
		printf("%s() Media_UnregisterFrameReader(Reader = %p) failed: 0x%08X; exiting.\r\n", __FUNCTION__, (void*)pstReader, cleanupRet);
		exit(EXIT_FAILURE);
	}

	if (isStarted)
	{
		stopRet = stream == AHEADFORM_MEDIA_STREAM_VIDEO ? Media_StopVideoReceive(pstMedia) : Media_StopMicReceive(pstMedia);
		if (stopRet != RET_SUCCESS)
			ret = stopRet;
	}
	free(pstFrame);
	return ret < 0 ? ret : RET_SUCCESS;
}

#else
#error Invalid USER_CMD_STREAM_METHOD
#endif

int UserCmd_SendWav(AHEADFORM_ALL_CTRL *pstAheadForm, int argc, char *argv[])
{
	AHEADFORM_MEDIA_CTRL *pstMedia = &pstAheadForm->stRobotCtrl.stMediaCtrl;
	AHEADFORM_WAV_FILE stWav = {0};
	unsigned char pcm[AHEADFORM_PCM_FRAME_SIZE];
	unsigned long long wakeTime_us, now_us;
	struct timeval sleepTime;
	int readSize, ret, cleanupRet;

	if (argc != 2)
		return ERR_ARGC;

	if ( (ret = Media_OpenWavReader(argv[1], &stWav)) != RET_SUCCESS )
		return ret;

	ret = Media_StartSpeakerSend(pstMedia);
	if (ret == RET_SUCCESS)
	{
		wakeTime_us = GetTimeUsCnt_UpToNow();
		while ((readSize = Media_ReadWavPcm(&stWav, pcm, sizeof(pcm))) > 0)
		{
			if (readSize < sizeof(pcm))
				memset(pcm + readSize, 0, sizeof(pcm) - readSize);

			if ( (ret = Media_WriteSpeakerPcmFrame(pstMedia, pcm, sizeof(pcm))) != RET_SUCCESS )
				break;

			wakeTime_us += 20000;
			now_us = GetTimeUsCnt_UpToNow();
			if (now_us < wakeTime_us)
			{
				sleepTime.tv_sec = (wakeTime_us - now_us) / 1000000;
				sleepTime.tv_usec = (wakeTime_us - now_us) % 1000000;
				if (select(0, NULL, NULL, NULL, &sleepTime) < 0)
				{
					printf("%s() call select(speaker pacing) failed: %s\r\n", __FUNCTION__, strerror(errno));
					ret = ERR_CLOCK;
					break;
				}
			}
		}

		if (readSize < 0)
			ret = readSize;

		if (ret == RET_SUCCESS)
			ret = Media_FinishSpeakerSend(pstMedia, 3000);

		cleanupRet = Media_StopSpeakerSend(pstMedia);
		if (cleanupRet != RET_SUCCESS)
			ret = cleanupRet;
	}
	cleanupRet = Media_CloseWav(&stWav);
	if (cleanupRet != RET_SUCCESS)
		ret = cleanupRet;

	return ret;
}
