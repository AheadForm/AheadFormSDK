#include "UserCmdMenu.h"

#include <sys/eventfd.h>
#include <sys/select.h>
#include <unistd.h>

#define REQUEST_REPLY_TIMEOUT_MS 1000
#define USER_CMD_REPLY_METHOD_ASYNC_REQUEST 1
#define USER_CMD_REPLY_METHOD_RX_EVENTFD 2
#define USER_CMD_REPLY_METHOD_READER_CALLBACK 3
#ifndef USER_CMD_REPLY_METHOD
#define USER_CMD_REPLY_METHOD USER_CMD_REPLY_METHOD_RX_EVENTFD
#endif

#if USER_CMD_REPLY_METHOD != USER_CMD_REPLY_METHOD_ASYNC_REQUEST && USER_CMD_REPLY_METHOD != USER_CMD_REPLY_METHOD_RX_EVENTFD && USER_CMD_REPLY_METHOD != USER_CMD_REPLY_METHOD_READER_CALLBACK
#error Invalid USER_CMD_REPLY_METHOD
#endif


#if USER_CMD_REPLY_METHOD == USER_CMD_REPLY_METHOD_ASYNC_REQUEST //PresetMotion by async request
int UserCmd_PresetMotion(AHEADFORM_ALL_CTRL *pstAheadForm, int argc, char *argv[])
{
	AHEADFORM_PRESET_MOTION_REPLY_SAMPLE stReply;
	unsigned char action;
	long parsedAction;
	struct timeval timeout = {.tv_sec = REQUEST_REPLY_TIMEOUT_MS / 1000, .tv_usec = REQUEST_REPLY_TIMEOUT_MS % 1000 * 1000};
	fd_set rdFdSet;
	eventfd_t eventCnt;
	int notifyEventfd, max_fd, cleanupRet, ret;

	if ( (ret = IsUserInError(argc, argv, "2", "1")) != RET_SUCCESS )
		return ret;

	parsedAction = strtol(argv[1], NULL, 0);
	if (parsedAction != AHEADFORM_PRESET_ACTION_SLEEP && parsedAction != AHEADFORM_PRESET_ACTION_CENTER)
		return ERR_ARGV;

	action = (unsigned char)parsedAction;

	if ( (notifyEventfd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC)) <= 0 )
	{
		printf("%s() call eventfd(PresetMotion completion) failed: %s\r\n", __FUNCTION__, strerror(errno));
		return ERR_EVENTFD_CREATE_FAILED;
	}

	if ( (ret = Head_PresetMotionAsync(&pstAheadForm->stRobotCtrl.stHeadCtrl, action, REQUEST_REPLY_TIMEOUT_MS * 1000, notifyEventfd, &stReply)) != RET_SUCCESS )
	{
		if ( (cleanupRet = Head_CancelPresetMotionAsyncRequest(&pstAheadForm->stRobotCtrl.stHeadCtrl, notifyEventfd)) != RET_SUCCESS )
		{
			printf("%s() Head_CancelPresetMotionAsyncRequest(notifyEventfd = %d) failed, ret = 0x%08X; 退出 AheadFormSDK。\r\n", __FUNCTION__, notifyEventfd, cleanupRet);
			PrintAllErrLog();
			exit(EXIT_FAILURE);
		}

		if (close(notifyEventfd) < 0)
		{
			printf("%s() call close(notifyEventfd = %d) failed: %s\r\n", __FUNCTION__, notifyEventfd, strerror(errno));
			return ERR_EVENTFD_IO_FAILED;
		}

		return ret;
	}
	max_fd = notifyEventfd;
	while (1)
	{
		FD_ZERO(&rdFdSet);
		FD_SET(notifyEventfd, &rdFdSet);
		//Other application fds can share this select set and use their own FD_ISSET handlers.
		if ( (ret = select(max_fd + 1, &rdFdSet, NULL, NULL, &timeout)) < 0 )
		{
			printf("%s() call select(notifyEventfd = %d) failed: %s\r\n", __FUNCTION__, notifyEventfd, strerror(errno));
			ret = ERR_SELECT;
			break;
		}
		else if (ret == 0)
		{
			break;
		}
		ret = 0;
		if (FD_ISSET(notifyEventfd, &rdFdSet))
		{
			ret = Head_CheckIsPresetMotionAsyncRequestReplied(&pstAheadForm->stRobotCtrl.stHeadCtrl, notifyEventfd);
			break;
		}

		if (timeout.tv_sec == 0 && timeout.tv_usec == 0)
			break;
	}

	if ( (cleanupRet = Head_CancelPresetMotionAsyncRequest(&pstAheadForm->stRobotCtrl.stHeadCtrl, notifyEventfd)) != RET_SUCCESS )
	{
		printf("%s() Head_CancelPresetMotionAsyncRequest(notifyEventfd = %d) failed, ret = 0x%08X; 退出 AheadFormSDK。\r\n", __FUNCTION__, notifyEventfd, cleanupRet);
		PrintAllErrLog();
		exit(EXIT_FAILURE);
	}

	if (ret == 1 || ret == 0 || ret == ERRCODE_REQUEST_TIMEOUT)
	{
		//Cancel ends SDK access; a final notification resolves completion racing with cancellation.
		if (eventfd_read(notifyEventfd, &eventCnt) == 0)
		{
			ret = 1;
		}
		else if (errno == EAGAIN)
		{
			ret = ERRCODE_REQUEST_TIMEOUT;
		}
		else
		{
			printf("%s() call eventfd_read(notifyEventfd = %d) failed: %s\r\n", __FUNCTION__, notifyEventfd, strerror(errno));
			ret = ERR_EVENTFD_IO_FAILED;
		}
	}

	if (close(notifyEventfd) < 0)
	{
		printf("%s() call close(notifyEventfd = %d) failed: %s\r\n", __FUNCTION__, notifyEventfd, strerror(errno));
		return ERR_EVENTFD_IO_FAILED;
	}

	if (ret == ERRCODE_REQUEST_TIMEOUT)
		return ERR_REQUEST_TIMEOUT;

	if (ret != 1)
		return ret;

	printf("preset askId=%llu, source=%s, remote ret=%d (acceptance result).\r\n", stReply.askId, stReply.sourceName, stReply.retCode);
	return stReply.retCode == 0 ? RET_SUCCESS : ERR_RX_FAILED;
}

#elif USER_CMD_REPLY_METHOD == USER_CMD_REPLY_METHOD_RX_EVENTFD //PresetMotion by rxDataUpdateEventfd
int UserCmd_PresetMotion(AHEADFORM_ALL_CTRL *pstAheadForm, int argc, char *argv[])
{
	AHEADFORM_TOPIC_READER_PARA stReaderPara = {0};
	AHEADFORM_TOPIC_READER *pstReader = NULL;
	AHEADFORM_PRESET_MOTION_ASK stAsk;
	AHEADFORM_PRESET_MOTION_REPLY_SAMPLE stReply;
	unsigned char action;
	long parsedAction;
	struct timeval timeout = {.tv_sec = REQUEST_REPLY_TIMEOUT_MS / 1000, .tv_usec = REQUEST_REPLY_TIMEOUT_MS % 1000 * 1000};
	fd_set rdFdSet;
	eventfd_t eventCnt;
	unsigned int i, readableElemCnt;
	int max_fd, cleanupRet, ret;

	if ( (ret = IsUserInError(argc, argv, "2", "1")) != RET_SUCCESS )
		return ret;

	parsedAction = strtol(argv[1], NULL, 0);
	if (parsedAction != AHEADFORM_PRESET_ACTION_SLEEP && parsedAction != AHEADFORM_PRESET_ACTION_CENTER)
		return ERR_ARGV;

	action = (unsigned char)parsedAction;

	if ( (ret = Head_BuildPresetMotionRequest(&pstAheadForm->stRobotCtrl.stHeadCtrl, action, &stAsk)) != RET_SUCCESS )
		return ret;

	if ( (pstReader = Head_RegisterPresetMotionReader(&pstAheadForm->stRobotCtrl.stHeadCtrl, &stReaderPara)) == NULL )
		return ERRCODE_INIT_FAIL;

	if ( (ret = Head_SendPresetMotionRequest(&pstAheadForm->stRobotCtrl.stHeadCtrl, &stAsk)) != RET_SUCCESS )
	{
		if ( (cleanupRet = Head_UnregisterPresetMotionReader(&pstAheadForm->stRobotCtrl.stHeadCtrl, pstReader)) != RET_SUCCESS )
		{
			printf("%s() Head_UnregisterPresetMotionReader(Reader = %p) failed, ret = 0x%08X; 退出 AheadFormSDK。\r\n", __FUNCTION__, (void*)pstReader, cleanupRet);
			PrintAllErrLog();
			exit(EXIT_FAILURE);
		}

		return ret;
	}
	max_fd = pstReader->rxDataUpdateEventfd;
	while (1)
	{
		FD_ZERO(&rdFdSet);
		FD_SET(pstReader->rxDataUpdateEventfd, &rdFdSet);
		if ( (ret = select(max_fd + 1, &rdFdSet, NULL, NULL, &timeout)) < 0 )
		{
			printf("%s() call select(rxDataUpdateEventfd = %d) failed: %s\r\n", __FUNCTION__, pstReader->rxDataUpdateEventfd, strerror(errno));
			ret = ERR_SELECT;
			break;
		}
		else if (ret == 0)
		{
			break;
		}
		ret = 0;
		if (FD_ISSET(pstReader->rxDataUpdateEventfd, &rdFdSet))
		{
			if (eventfd_read(pstReader->rxDataUpdateEventfd, &eventCnt) < 0 && errno != EAGAIN)
			{
				printf("%s() call eventfd_read(rxDataUpdateEventfd = %d) failed: %s\r\n", __FUNCTION__, pstReader->rxDataUpdateEventfd, strerror(errno));
				ret = ERR_EVENTFD_IO_FAILED;
				break;
			}
			//Consume a finite FIFO snapshot; eventCnt is only an update hint.
			readableElemCnt = TopicReader_GetReadableElemCnt(pstReader);
			for (i = 0; i < readableElemCnt; i++)
			{
				if ( (ret = TopicReader_PopElem(pstReader, &stReply)) <= 0 )
					break;

				if (Head_MatchPresetMotionReplyWithReq(&stReply, &stAsk))
					break;

				ret = 0;
			}
		}

		if (ret != 0)
			break;

		if (timeout.tv_sec == 0 && timeout.tv_usec == 0)
			break;
	}

	if ( (cleanupRet = Head_UnregisterPresetMotionReader(&pstAheadForm->stRobotCtrl.stHeadCtrl, pstReader)) != RET_SUCCESS )
	{
		printf("%s() Head_UnregisterPresetMotionReader(Reader = %p) failed, ret = 0x%08X; 退出 AheadFormSDK。\r\n", __FUNCTION__, (void*)pstReader, cleanupRet);
		PrintAllErrLog();
		exit(EXIT_FAILURE);
	}

	if (ret == 0)
		return ERR_REQUEST_TIMEOUT;

	if (ret < 0)
		return ret;

	printf("preset askId=%llu, source=%s, remote ret=%d (acceptance result).\r\n", stReply.askId, stReply.sourceName, stReply.retCode);
	return stReply.retCode == 0 ? RET_SUCCESS : ERR_RX_FAILED;
}

#else //PresetMotion by Reader callback
typedef struct
{
	AHEADFORM_PRESET_MOTION_ASK stAsk;
	AHEADFORM_PRESET_MOTION_REPLY_SAMPLE stReply;
	int completionEventfd;
	int isReplied;
} PRESETMOTION_CALLBACK_CONTEXT;

static void CopyPresetMotionReplyIfMatchesReq(const void *pElem, void *pUserArg)
{
	const AHEADFORM_PRESET_MOTION_REPLY_SAMPLE *pstReply = (const AHEADFORM_PRESET_MOTION_REPLY_SAMPLE*)pElem;
	PRESETMOTION_CALLBACK_CONTEXT *pstContext = (PRESETMOTION_CALLBACK_CONTEXT*)pUserArg;

	if (pstContext->isReplied || !Head_MatchPresetMotionReplyWithReq(pstReply, &pstContext->stAsk))
		return;

	pstContext->stReply = *pstReply;
	pstContext->isReplied = 1;
	if (eventfd_write(pstContext->completionEventfd, 1) < 0 && errno != EAGAIN)
		PRINT_FUNCTION_ERRINFO(ERR_EVENTFD_IO_FAILED, __FUNCTION__, "call eventfd_write(completionEventfd = %d) failed: %s", pstContext->completionEventfd, strerror(errno));
}

int UserCmd_PresetMotion(AHEADFORM_ALL_CTRL *pstAheadForm, int argc, char *argv[])
{
	PRESETMOTION_CALLBACK_CONTEXT stContext = {.isReplied = 0};
	AHEADFORM_TOPIC_READER_PARA stReaderPara = {CopyPresetMotionReplyIfMatchesReq, &stContext};
	AHEADFORM_TOPIC_READER *pstReader = NULL;
	unsigned char action;
	long parsedAction;
	struct timeval timeout = {.tv_sec = REQUEST_REPLY_TIMEOUT_MS / 1000, .tv_usec = REQUEST_REPLY_TIMEOUT_MS % 1000 * 1000};
	fd_set rdFdSet;
	int max_fd, cleanupRet, ret;

	if ( (ret = IsUserInError(argc, argv, "2", "1")) != RET_SUCCESS )
		return ret;

	parsedAction = strtol(argv[1], NULL, 0);
	if (parsedAction != AHEADFORM_PRESET_ACTION_SLEEP && parsedAction != AHEADFORM_PRESET_ACTION_CENTER)
		return ERR_ARGV;

	action = (unsigned char)parsedAction;

	if ( (ret = Head_BuildPresetMotionRequest(&pstAheadForm->stRobotCtrl.stHeadCtrl, action, &stContext.stAsk)) != RET_SUCCESS )
		return ret;

	if ( (stContext.completionEventfd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC)) <= 0 )
	{
		printf("%s() call eventfd(PresetMotion completion) failed: %s\r\n", __FUNCTION__, strerror(errno));
		return ERR_EVENTFD_CREATE_FAILED;
	}

	if ( (pstReader = Head_RegisterPresetMotionReader(&pstAheadForm->stRobotCtrl.stHeadCtrl, &stReaderPara)) == NULL )
	{
		if (close(stContext.completionEventfd) < 0)
		{
			printf("%s() call close(stContext.completionEventfd = %d) failed: %s\r\n", __FUNCTION__, stContext.completionEventfd, strerror(errno));
			return ERR_EVENTFD_IO_FAILED;
		}

		return ERRCODE_INIT_FAIL;
	}

	if ( (ret = Head_SendPresetMotionRequest(&pstAheadForm->stRobotCtrl.stHeadCtrl, &stContext.stAsk)) != RET_SUCCESS )
	{
		if ( (cleanupRet = Head_UnregisterPresetMotionReader(&pstAheadForm->stRobotCtrl.stHeadCtrl, pstReader)) != RET_SUCCESS )
		{
			printf("%s() Head_UnregisterPresetMotionReader(Reader = %p) failed, ret = 0x%08X; 退出 AheadFormSDK。\r\n", __FUNCTION__, (void*)pstReader, cleanupRet);
			PrintAllErrLog();
			exit(EXIT_FAILURE);
		}

		if (close(stContext.completionEventfd) < 0)
		{
			printf("%s() call close(stContext.completionEventfd = %d) failed: %s\r\n", __FUNCTION__, stContext.completionEventfd, strerror(errno));
			return ERR_EVENTFD_IO_FAILED;
		}

		return ret;
	}
	max_fd = stContext.completionEventfd;
	while (1)
	{
		FD_ZERO(&rdFdSet);
		FD_SET(stContext.completionEventfd, &rdFdSet);
		if ( (ret = select(max_fd + 1, &rdFdSet, NULL, NULL, &timeout)) < 0 )
		{
			printf("%s() call select(completionEventfd = %d) failed: %s\r\n", __FUNCTION__, stContext.completionEventfd, strerror(errno));
			ret = ERR_SELECT;
			break;
		}
		else if (ret == 0)
		{
			break;
		}
		ret = RET_SUCCESS;
		if (FD_ISSET(stContext.completionEventfd, &rdFdSet))
			break;

		if (timeout.tv_sec == 0 && timeout.tv_usec == 0)
			break;
	}

	//Unregister synchronizes with callbacks before this thread reads their output.
	if ( (cleanupRet = Head_UnregisterPresetMotionReader(&pstAheadForm->stRobotCtrl.stHeadCtrl, pstReader)) != RET_SUCCESS )
	{
		printf("%s() Head_UnregisterPresetMotionReader(Reader = %p) failed, ret = 0x%08X; 退出 AheadFormSDK。\r\n", __FUNCTION__, (void*)pstReader, cleanupRet);
		PrintAllErrLog();
		exit(EXIT_FAILURE);
	}

	if (close(stContext.completionEventfd) < 0)
	{
		printf("%s() call close(stContext.completionEventfd = %d) failed: %s\r\n", __FUNCTION__, stContext.completionEventfd, strerror(errno));
		return ERR_EVENTFD_IO_FAILED;
	}

	if (ret != RET_SUCCESS)
		return ret;

	if (!stContext.isReplied)
		return ERR_REQUEST_TIMEOUT;

	printf("preset askId=%llu, source=%s, remote ret=%d (acceptance result).\r\n", stContext.stReply.askId, stContext.stReply.sourceName, stContext.stReply.retCode);
	return stContext.stReply.retCode == 0 ? RET_SUCCESS : ERR_RX_FAILED;
}
#endif

#if USER_CMD_REPLY_METHOD == USER_CMD_REPLY_METHOD_ASYNC_REQUEST //AudioDevice by async request
int UserCmd_AudioDevice(AHEADFORM_ALL_CTRL *pstAheadForm, int argc, char *argv[])
{
	AHEADFORM_AUDIO_DEVICE_REPLY_SAMPLE stReply;
	AHEADFORM_AUDIO_DEVICE_PARA stPara = {0};
	long operation, deviceType;
	unsigned int deviceIndex;
	struct timeval timeout = {.tv_sec = REQUEST_REPLY_TIMEOUT_MS / 1000, .tv_usec = REQUEST_REPLY_TIMEOUT_MS % 1000 * 1000};
	fd_set rdFdSet;
	eventfd_t eventCnt;
	int notifyEventfd, max_fd, cleanupRet, ret;

	if ( (ret = IsUserInError(argc, argv, "3;4", "1,2;1,2")) != RET_SUCCESS )
		return ret;

	operation = strtol(argv[1], NULL, 0);
	deviceType = strtol(argv[2], NULL, 0);
	if (operation < AHEADFORM_AUDIO_DEVICE_OPERATION_ROUTE || operation > AHEADFORM_AUDIO_DEVICE_OPERATION_GET_CURRENT || deviceType > AHEADFORM_AUDIO_DEVICE_SPK)
		return ERR_ARGV;

	stPara.operation = (unsigned char)operation;
	stPara.deviceType = (unsigned char)deviceType;
	if (operation == AHEADFORM_AUDIO_DEVICE_OPERATION_ROUTE)
	{
		if (argc != 4 || argv[3][0] == 0 || strlen(argv[3]) >= AHEADFORM_AUDIO_DEVICE_ID_LEN_MAX)
			return ERR_ARGV;

		if (deviceType == AHEADFORM_AUDIO_DEVICE_MIC)
		{
			stPara.hasMicDevice = 1;
			strcpy(stPara.micDeviceId, argv[3]);
		}
		else
		{
			stPara.hasSpkDevice = 1;
			strcpy(stPara.spkDeviceId, argv[3]);
		}
	}
	else if (argc != 3)
	{
		return ERR_ARGC;
	}

	if ( (notifyEventfd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC)) <= 0 )
	{
		printf("%s() call eventfd(AudioDevice completion) failed: %s\r\n", __FUNCTION__, strerror(errno));
		return ERR_EVENTFD_CREATE_FAILED;
	}

	if ( (ret = Media_AudioDeviceAsync(&pstAheadForm->stRobotCtrl.stMediaCtrl, &stPara, REQUEST_REPLY_TIMEOUT_MS * 1000, notifyEventfd, &stReply)) != RET_SUCCESS )
	{
		if ( (cleanupRet = Media_CancelAudioDeviceAsyncRequest(&pstAheadForm->stRobotCtrl.stMediaCtrl, notifyEventfd)) != RET_SUCCESS )
		{
			printf("%s() Media_CancelAudioDeviceAsyncRequest(notifyEventfd = %d) failed, ret = 0x%08X; 退出 AheadFormSDK。\r\n", __FUNCTION__, notifyEventfd, cleanupRet);
			PrintAllErrLog();
			exit(EXIT_FAILURE);
		}

		if (close(notifyEventfd) < 0)
		{
			printf("%s() call close(notifyEventfd = %d) failed: %s\r\n", __FUNCTION__, notifyEventfd, strerror(errno));
			return ERR_EVENTFD_IO_FAILED;
		}

		return ret;
	}
	max_fd = notifyEventfd;
	while (1)
	{
		FD_ZERO(&rdFdSet);
		FD_SET(notifyEventfd, &rdFdSet);
		//Other application fds can share this select set and use their own FD_ISSET handlers.
		if ( (ret = select(max_fd + 1, &rdFdSet, NULL, NULL, &timeout)) < 0 )
		{
			printf("%s() call select(notifyEventfd = %d) failed: %s\r\n", __FUNCTION__, notifyEventfd, strerror(errno));
			ret = ERR_SELECT;
			break;
		}
		else if (ret == 0)
		{
			break;
		}
		ret = 0;
		if (FD_ISSET(notifyEventfd, &rdFdSet))
		{
			ret = Media_CheckIsAudioDeviceAsyncRequestReplied(&pstAheadForm->stRobotCtrl.stMediaCtrl, notifyEventfd);
			break;
		}

		if (timeout.tv_sec == 0 && timeout.tv_usec == 0)
			break;
	}

	if ( (cleanupRet = Media_CancelAudioDeviceAsyncRequest(&pstAheadForm->stRobotCtrl.stMediaCtrl, notifyEventfd)) != RET_SUCCESS )
	{
		printf("%s() Media_CancelAudioDeviceAsyncRequest(notifyEventfd = %d) failed, ret = 0x%08X; 退出 AheadFormSDK。\r\n", __FUNCTION__, notifyEventfd, cleanupRet);
		PrintAllErrLog();
		exit(EXIT_FAILURE);
	}

	if (ret == 1 || ret == 0 || ret == ERRCODE_REQUEST_TIMEOUT)
	{
		//Cancel ends SDK access; a final notification resolves completion racing with cancellation.
		if (eventfd_read(notifyEventfd, &eventCnt) == 0)
		{
			ret = 1;
		}
		else if (errno == EAGAIN)
		{
			ret = ERRCODE_REQUEST_TIMEOUT;
		}
		else
		{
			printf("%s() call eventfd_read(notifyEventfd = %d) failed: %s\r\n", __FUNCTION__, notifyEventfd, strerror(errno));
			ret = ERR_EVENTFD_IO_FAILED;
		}
	}

	if (close(notifyEventfd) < 0)
	{
		printf("%s() call close(notifyEventfd = %d) failed: %s\r\n", __FUNCTION__, notifyEventfd, strerror(errno));
		return ERR_EVENTFD_IO_FAILED;
	}

	if (ret == ERRCODE_REQUEST_TIMEOUT)
		return ERR_REQUEST_TIMEOUT;

	if (ret != 1)
		return ret;

	printf("audio device askId=%llu, operation=%u, type=%u, remote ret=%d, queryDataValid=%u.\r\n", stReply.askId, stReply.operation, stReply.deviceType, stReply.retCode, stReply.queryDataValid);
	if (stReply.queryDataValid)
	{
		for (deviceIndex = 0; deviceIndex < stReply.deviceCnt; deviceIndex++)
			printf("device[%u] id=%s, name=%s.\r\n", deviceIndex, stReply.szstDevice[deviceIndex].deviceId, stReply.szstDevice[deviceIndex].deviceName);
	}

	return stReply.retCode == 0 ? RET_SUCCESS : ERR_RX_FAILED;
}

#elif USER_CMD_REPLY_METHOD == USER_CMD_REPLY_METHOD_RX_EVENTFD //AudioDevice by rxDataUpdateEventfd
int UserCmd_AudioDevice(AHEADFORM_ALL_CTRL *pstAheadForm, int argc, char *argv[])
{
	AHEADFORM_TOPIC_READER_PARA stReaderPara = {0};
	AHEADFORM_TOPIC_READER *pstReader = NULL;
	AHEADFORM_AUDIO_DEVICE_ASK stAsk;
	AHEADFORM_AUDIO_DEVICE_REPLY_SAMPLE stReply;
	AHEADFORM_AUDIO_DEVICE_PARA stPara = {0};
	long operation, deviceType;
	unsigned int deviceIndex;
	struct timeval timeout = {.tv_sec = REQUEST_REPLY_TIMEOUT_MS / 1000, .tv_usec = REQUEST_REPLY_TIMEOUT_MS % 1000 * 1000};
	fd_set rdFdSet;
	eventfd_t eventCnt;
	unsigned int i, readableElemCnt;
	int max_fd, cleanupRet, ret;

	if ( (ret = IsUserInError(argc, argv, "3;4", "1,2;1,2")) != RET_SUCCESS )
		return ret;

	operation = strtol(argv[1], NULL, 0);
	deviceType = strtol(argv[2], NULL, 0);
	if (operation < AHEADFORM_AUDIO_DEVICE_OPERATION_ROUTE || operation > AHEADFORM_AUDIO_DEVICE_OPERATION_GET_CURRENT || deviceType > AHEADFORM_AUDIO_DEVICE_SPK)
		return ERR_ARGV;

	stPara.operation = (unsigned char)operation;
	stPara.deviceType = (unsigned char)deviceType;
	if (operation == AHEADFORM_AUDIO_DEVICE_OPERATION_ROUTE)
	{
		if (argc != 4 || argv[3][0] == 0 || strlen(argv[3]) >= AHEADFORM_AUDIO_DEVICE_ID_LEN_MAX)
			return ERR_ARGV;

		if (deviceType == AHEADFORM_AUDIO_DEVICE_MIC)
		{
			stPara.hasMicDevice = 1;
			strcpy(stPara.micDeviceId, argv[3]);
		}
		else
		{
			stPara.hasSpkDevice = 1;
			strcpy(stPara.spkDeviceId, argv[3]);
		}
	}
	else if (argc != 3)
	{
		return ERR_ARGC;
	}

	if ( (ret = Media_BuildAudioDeviceRequest(&pstAheadForm->stRobotCtrl.stMediaCtrl, &stPara, &stAsk)) != RET_SUCCESS )
		return ret;

	if ( (pstReader = Media_RegisterAudioDeviceReader(&pstAheadForm->stRobotCtrl.stMediaCtrl, &stReaderPara)) == NULL )
		return ERRCODE_INIT_FAIL;

	if ( (ret = Media_SendAudioDeviceRequest(&pstAheadForm->stRobotCtrl.stMediaCtrl, &stAsk)) != RET_SUCCESS )
	{
		if ( (cleanupRet = Media_UnregisterAudioDeviceReader(&pstAheadForm->stRobotCtrl.stMediaCtrl, pstReader)) != RET_SUCCESS )
		{
			printf("%s() Media_UnregisterAudioDeviceReader(Reader = %p) failed, ret = 0x%08X; 退出 AheadFormSDK。\r\n", __FUNCTION__, (void*)pstReader, cleanupRet);
			PrintAllErrLog();
			exit(EXIT_FAILURE);
		}

		return ret;
	}
	max_fd = pstReader->rxDataUpdateEventfd;
	while (1)
	{
		FD_ZERO(&rdFdSet);
		FD_SET(pstReader->rxDataUpdateEventfd, &rdFdSet);
		if ( (ret = select(max_fd + 1, &rdFdSet, NULL, NULL, &timeout)) < 0 )
		{
			printf("%s() call select(rxDataUpdateEventfd = %d) failed: %s\r\n", __FUNCTION__, pstReader->rxDataUpdateEventfd, strerror(errno));
			ret = ERR_SELECT;
			break;
		}
		else if (ret == 0)
		{
			break;
		}
		ret = 0;
		if (FD_ISSET(pstReader->rxDataUpdateEventfd, &rdFdSet))
		{
			if (eventfd_read(pstReader->rxDataUpdateEventfd, &eventCnt) < 0 && errno != EAGAIN)
			{
				printf("%s() call eventfd_read(rxDataUpdateEventfd = %d) failed: %s\r\n", __FUNCTION__, pstReader->rxDataUpdateEventfd, strerror(errno));
				ret = ERR_EVENTFD_IO_FAILED;
				break;
			}
			//Consume a finite FIFO snapshot; eventCnt is only an update hint.
			readableElemCnt = TopicReader_GetReadableElemCnt(pstReader);
			for (i = 0; i < readableElemCnt; i++)
			{
				if ( (ret = TopicReader_PopElem(pstReader, &stReply)) <= 0 )
					break;

				if (Media_MatchAudioDeviceReplyWithReq(&stReply, &stAsk))
					break;

				ret = 0;
			}
		}

		if (ret != 0)
			break;

		if (timeout.tv_sec == 0 && timeout.tv_usec == 0)
			break;
	}

	if ( (cleanupRet = Media_UnregisterAudioDeviceReader(&pstAheadForm->stRobotCtrl.stMediaCtrl, pstReader)) != RET_SUCCESS )
	{
		printf("%s() Media_UnregisterAudioDeviceReader(Reader = %p) failed, ret = 0x%08X; 退出 AheadFormSDK。\r\n", __FUNCTION__, (void*)pstReader, cleanupRet);
		PrintAllErrLog();
		exit(EXIT_FAILURE);
	}

	if (ret == 0)
		return ERR_REQUEST_TIMEOUT;

	if (ret < 0)
		return ret;

	printf("audio device askId=%llu, operation=%u, type=%u, remote ret=%d, queryDataValid=%u.\r\n", stReply.askId, stReply.operation, stReply.deviceType, stReply.retCode, stReply.queryDataValid);
	if (stReply.queryDataValid)
	{
		for (deviceIndex = 0; deviceIndex < stReply.deviceCnt; deviceIndex++)
			printf("device[%u] id=%s, name=%s.\r\n", deviceIndex, stReply.szstDevice[deviceIndex].deviceId, stReply.szstDevice[deviceIndex].deviceName);
	}

	return stReply.retCode == 0 ? RET_SUCCESS : ERR_RX_FAILED;
}

#else //AudioDevice by Reader callback
typedef struct
{
	AHEADFORM_AUDIO_DEVICE_ASK stAsk;
	AHEADFORM_AUDIO_DEVICE_REPLY_SAMPLE stReply;
	int completionEventfd;
	int isReplied;
} AUDIODEVICE_CALLBACK_CONTEXT;

static void CopyAudioDeviceReplyIfMatchesReq(const void *pElem, void *pUserArg)
{
	const AHEADFORM_AUDIO_DEVICE_REPLY_SAMPLE *pstReply = (const AHEADFORM_AUDIO_DEVICE_REPLY_SAMPLE*)pElem;
	AUDIODEVICE_CALLBACK_CONTEXT *pstContext = (AUDIODEVICE_CALLBACK_CONTEXT*)pUserArg;

	if (pstContext->isReplied || !Media_MatchAudioDeviceReplyWithReq(pstReply, &pstContext->stAsk))
		return;

	pstContext->stReply = *pstReply;
	pstContext->isReplied = 1;
	if (eventfd_write(pstContext->completionEventfd, 1) < 0 && errno != EAGAIN)
		PRINT_FUNCTION_ERRINFO(ERR_EVENTFD_IO_FAILED, __FUNCTION__, "call eventfd_write(completionEventfd = %d) failed: %s", pstContext->completionEventfd, strerror(errno));
}

int UserCmd_AudioDevice(AHEADFORM_ALL_CTRL *pstAheadForm, int argc, char *argv[])
{
	AUDIODEVICE_CALLBACK_CONTEXT stContext = {.isReplied = 0};
	AHEADFORM_TOPIC_READER_PARA stReaderPara = {CopyAudioDeviceReplyIfMatchesReq, &stContext};
	AHEADFORM_TOPIC_READER *pstReader = NULL;
	AHEADFORM_AUDIO_DEVICE_PARA stPara = {0};
	long operation, deviceType;
	unsigned int deviceIndex;
	struct timeval timeout = {.tv_sec = REQUEST_REPLY_TIMEOUT_MS / 1000, .tv_usec = REQUEST_REPLY_TIMEOUT_MS % 1000 * 1000};
	fd_set rdFdSet;
	int max_fd, cleanupRet, ret;

	if ( (ret = IsUserInError(argc, argv, "3;4", "1,2;1,2")) != RET_SUCCESS )
		return ret;

	operation = strtol(argv[1], NULL, 0);
	deviceType = strtol(argv[2], NULL, 0);
	if (operation < AHEADFORM_AUDIO_DEVICE_OPERATION_ROUTE || operation > AHEADFORM_AUDIO_DEVICE_OPERATION_GET_CURRENT || deviceType > AHEADFORM_AUDIO_DEVICE_SPK)
		return ERR_ARGV;

	stPara.operation = (unsigned char)operation;
	stPara.deviceType = (unsigned char)deviceType;
	if (operation == AHEADFORM_AUDIO_DEVICE_OPERATION_ROUTE)
	{
		if (argc != 4 || argv[3][0] == 0 || strlen(argv[3]) >= AHEADFORM_AUDIO_DEVICE_ID_LEN_MAX)
			return ERR_ARGV;

		if (deviceType == AHEADFORM_AUDIO_DEVICE_MIC)
		{
			stPara.hasMicDevice = 1;
			strcpy(stPara.micDeviceId, argv[3]);
		}
		else
		{
			stPara.hasSpkDevice = 1;
			strcpy(stPara.spkDeviceId, argv[3]);
		}
	}
	else if (argc != 3)
	{
		return ERR_ARGC;
	}

	if ( (ret = Media_BuildAudioDeviceRequest(&pstAheadForm->stRobotCtrl.stMediaCtrl, &stPara, &stContext.stAsk)) != RET_SUCCESS )
		return ret;

	if ( (stContext.completionEventfd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC)) <= 0 )
	{
		printf("%s() call eventfd(AudioDevice completion) failed: %s\r\n", __FUNCTION__, strerror(errno));
		return ERR_EVENTFD_CREATE_FAILED;
	}

	if ( (pstReader = Media_RegisterAudioDeviceReader(&pstAheadForm->stRobotCtrl.stMediaCtrl, &stReaderPara)) == NULL )
	{
		if (close(stContext.completionEventfd) < 0)
		{
			printf("%s() call close(stContext.completionEventfd = %d) failed: %s\r\n", __FUNCTION__, stContext.completionEventfd, strerror(errno));
			return ERR_EVENTFD_IO_FAILED;
		}

		return ERRCODE_INIT_FAIL;
	}

	if ( (ret = Media_SendAudioDeviceRequest(&pstAheadForm->stRobotCtrl.stMediaCtrl, &stContext.stAsk)) != RET_SUCCESS )
	{
		if ( (cleanupRet = Media_UnregisterAudioDeviceReader(&pstAheadForm->stRobotCtrl.stMediaCtrl, pstReader)) != RET_SUCCESS )
		{
			printf("%s() Media_UnregisterAudioDeviceReader(Reader = %p) failed, ret = 0x%08X; 退出 AheadFormSDK。\r\n", __FUNCTION__, (void*)pstReader, cleanupRet);
			PrintAllErrLog();
			exit(EXIT_FAILURE);
		}

		if (close(stContext.completionEventfd) < 0)
		{
			printf("%s() call close(stContext.completionEventfd = %d) failed: %s\r\n", __FUNCTION__, stContext.completionEventfd, strerror(errno));
			return ERR_EVENTFD_IO_FAILED;
		}

		return ret;
	}
	max_fd = stContext.completionEventfd;
	while (1)
	{
		FD_ZERO(&rdFdSet);
		FD_SET(stContext.completionEventfd, &rdFdSet);
		if ( (ret = select(max_fd + 1, &rdFdSet, NULL, NULL, &timeout)) < 0 )
		{
			printf("%s() call select(completionEventfd = %d) failed: %s\r\n", __FUNCTION__, stContext.completionEventfd, strerror(errno));
			ret = ERR_SELECT;
			break;
		}
		else if (ret == 0)
		{
			break;
		}
		ret = RET_SUCCESS;
		if (FD_ISSET(stContext.completionEventfd, &rdFdSet))
			break;

		if (timeout.tv_sec == 0 && timeout.tv_usec == 0)
			break;
	}

	//Unregister synchronizes with callbacks before this thread reads their output.
	if ( (cleanupRet = Media_UnregisterAudioDeviceReader(&pstAheadForm->stRobotCtrl.stMediaCtrl, pstReader)) != RET_SUCCESS )
	{
		printf("%s() Media_UnregisterAudioDeviceReader(Reader = %p) failed, ret = 0x%08X; 退出 AheadFormSDK。\r\n", __FUNCTION__, (void*)pstReader, cleanupRet);
		PrintAllErrLog();
		exit(EXIT_FAILURE);
	}

	if (close(stContext.completionEventfd) < 0)
	{
		printf("%s() call close(stContext.completionEventfd = %d) failed: %s\r\n", __FUNCTION__, stContext.completionEventfd, strerror(errno));
		return ERR_EVENTFD_IO_FAILED;
	}

	if (ret != RET_SUCCESS)
		return ret;

	if (!stContext.isReplied)
		return ERR_REQUEST_TIMEOUT;

	printf("audio device askId=%llu, operation=%u, type=%u, remote ret=%d, queryDataValid=%u.\r\n", stContext.stReply.askId, stContext.stReply.operation, stContext.stReply.deviceType, stContext.stReply.retCode, stContext.stReply.queryDataValid);
	if (stContext.stReply.queryDataValid)
	{
		for (deviceIndex = 0; deviceIndex < stContext.stReply.deviceCnt; deviceIndex++)
			printf("device[%u] id=%s, name=%s.\r\n", deviceIndex, stContext.stReply.szstDevice[deviceIndex].deviceId, stContext.stReply.szstDevice[deviceIndex].deviceName);
	}

	return stContext.stReply.retCode == 0 ? RET_SUCCESS : ERR_RX_FAILED;
}
#endif

#if USER_CMD_REPLY_METHOD == USER_CMD_REPLY_METHOD_ASYNC_REQUEST //MediaChannel by async request
int UserCmd_MediaChannel(AHEADFORM_ALL_CTRL *pstAheadForm, int argc, char *argv[])
{
	AHEADFORM_MEDIA_CHANNEL_REPLY_SAMPLE stReply;
	AHEADFORM_MEDIA_CHANNEL_PARA stPara;
	long values[4];
	unsigned int parameterIndex;
	struct timeval timeout = {.tv_sec = REQUEST_REPLY_TIMEOUT_MS / 1000, .tv_usec = REQUEST_REPLY_TIMEOUT_MS % 1000 * 1000};
	fd_set rdFdSet;
	eventfd_t eventCnt;
	int notifyEventfd, max_fd, cleanupRet, ret;

	if ( (ret = IsUserInError(argc, argv, "5", "1,2,3,4")) != RET_SUCCESS )
		return ret;

	for (parameterIndex = 0; parameterIndex < 4; parameterIndex++)
	{
		values[parameterIndex] = strtol(argv[parameterIndex + 1], NULL, 0);
		if (values[parameterIndex] > AHEADFORM_CHANNEL_ENABLE)
			return ERR_ARGV;
	}
	stPara.videoCtrl = (unsigned char)values[0];
	stPara.micCtrl = (unsigned char)values[1];
	stPara.playbackCtrl = (unsigned char)values[2];
	stPara.vadCtrl = (unsigned char)values[3];

	if ( (notifyEventfd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC)) <= 0 )
	{
		printf("%s() call eventfd(MediaChannel completion) failed: %s\r\n", __FUNCTION__, strerror(errno));
		return ERR_EVENTFD_CREATE_FAILED;
	}

	if ( (ret = Media_ChannelAsync(&pstAheadForm->stRobotCtrl.stMediaCtrl, &stPara, REQUEST_REPLY_TIMEOUT_MS * 1000, notifyEventfd, &stReply)) != RET_SUCCESS )
	{
		if ( (cleanupRet = Media_CancelChannelAsyncRequest(&pstAheadForm->stRobotCtrl.stMediaCtrl, notifyEventfd)) != RET_SUCCESS )
		{
			printf("%s() Media_CancelChannelAsyncRequest(notifyEventfd = %d) failed, ret = 0x%08X; 退出 AheadFormSDK。\r\n", __FUNCTION__, notifyEventfd, cleanupRet);
			PrintAllErrLog();
			exit(EXIT_FAILURE);
		}

		if (close(notifyEventfd) < 0)
		{
			printf("%s() call close(notifyEventfd = %d) failed: %s\r\n", __FUNCTION__, notifyEventfd, strerror(errno));
			return ERR_EVENTFD_IO_FAILED;
		}

		return ret;
	}
	max_fd = notifyEventfd;
	while (1)
	{
		FD_ZERO(&rdFdSet);
		FD_SET(notifyEventfd, &rdFdSet);
		//Other application fds can share this select set and use their own FD_ISSET handlers.
		if ( (ret = select(max_fd + 1, &rdFdSet, NULL, NULL, &timeout)) < 0 )
		{
			printf("%s() call select(notifyEventfd = %d) failed: %s\r\n", __FUNCTION__, notifyEventfd, strerror(errno));
			ret = ERR_SELECT;
			break;
		}
		else if (ret == 0)
		{
			break;
		}
		ret = 0;
		if (FD_ISSET(notifyEventfd, &rdFdSet))
		{
			ret = Media_CheckIsChannelAsyncRequestReplied(&pstAheadForm->stRobotCtrl.stMediaCtrl, notifyEventfd);
			break;
		}

		if (timeout.tv_sec == 0 && timeout.tv_usec == 0)
			break;
	}

	if ( (cleanupRet = Media_CancelChannelAsyncRequest(&pstAheadForm->stRobotCtrl.stMediaCtrl, notifyEventfd)) != RET_SUCCESS )
	{
		printf("%s() Media_CancelChannelAsyncRequest(notifyEventfd = %d) failed, ret = 0x%08X; 退出 AheadFormSDK。\r\n", __FUNCTION__, notifyEventfd, cleanupRet);
		PrintAllErrLog();
		exit(EXIT_FAILURE);
	}

	if (ret == 1 || ret == 0 || ret == ERRCODE_REQUEST_TIMEOUT)
	{
		//Cancel ends SDK access; a final notification resolves completion racing with cancellation.
		if (eventfd_read(notifyEventfd, &eventCnt) == 0)
		{
			ret = 1;
		}
		else if (errno == EAGAIN)
		{
			ret = ERRCODE_REQUEST_TIMEOUT;
		}
		else
		{
			printf("%s() call eventfd_read(notifyEventfd = %d) failed: %s\r\n", __FUNCTION__, notifyEventfd, strerror(errno));
			ret = ERR_EVENTFD_IO_FAILED;
		}
	}

	if (close(notifyEventfd) < 0)
	{
		printf("%s() call close(notifyEventfd = %d) failed: %s\r\n", __FUNCTION__, notifyEventfd, strerror(errno));
		return ERR_EVENTFD_IO_FAILED;
	}

	if (ret == ERRCODE_REQUEST_TIMEOUT)
		return ERR_REQUEST_TIMEOUT;

	if (ret != 1)
		return ret;

	printf("media channel askId=%llu, remote ret=%d (acceptance result).\r\n", stReply.askId, stReply.retCode);
	return stReply.retCode == 0 ? RET_SUCCESS : ERR_RX_FAILED;
}

#elif USER_CMD_REPLY_METHOD == USER_CMD_REPLY_METHOD_RX_EVENTFD //MediaChannel by rxDataUpdateEventfd
int UserCmd_MediaChannel(AHEADFORM_ALL_CTRL *pstAheadForm, int argc, char *argv[])
{
	AHEADFORM_TOPIC_READER_PARA stReaderPara = {0};
	AHEADFORM_TOPIC_READER *pstReader = NULL;
	AHEADFORM_MEDIA_CHANNEL_ASK stAsk;
	AHEADFORM_MEDIA_CHANNEL_REPLY_SAMPLE stReply;
	AHEADFORM_MEDIA_CHANNEL_PARA stPara;
	long values[4];
	unsigned int parameterIndex;
	struct timeval timeout = {.tv_sec = REQUEST_REPLY_TIMEOUT_MS / 1000, .tv_usec = REQUEST_REPLY_TIMEOUT_MS % 1000 * 1000};
	fd_set rdFdSet;
	eventfd_t eventCnt;
	unsigned int i, readableElemCnt;
	int max_fd, cleanupRet, ret;

	if ( (ret = IsUserInError(argc, argv, "5", "1,2,3,4")) != RET_SUCCESS )
		return ret;

	for (parameterIndex = 0; parameterIndex < 4; parameterIndex++)
	{
		values[parameterIndex] = strtol(argv[parameterIndex + 1], NULL, 0);
		if (values[parameterIndex] > AHEADFORM_CHANNEL_ENABLE)
			return ERR_ARGV;
	}
	stPara.videoCtrl = (unsigned char)values[0];
	stPara.micCtrl = (unsigned char)values[1];
	stPara.playbackCtrl = (unsigned char)values[2];
	stPara.vadCtrl = (unsigned char)values[3];

	if ( (ret = Media_BuildChannelRequest(&pstAheadForm->stRobotCtrl.stMediaCtrl, &stPara, &stAsk)) != RET_SUCCESS )
		return ret;

	if ( (pstReader = Media_RegisterChannelReader(&pstAheadForm->stRobotCtrl.stMediaCtrl, &stReaderPara)) == NULL )
		return ERRCODE_INIT_FAIL;

	if ( (ret = Media_SendChannelRequest(&pstAheadForm->stRobotCtrl.stMediaCtrl, &stAsk)) != RET_SUCCESS )
	{
		if ( (cleanupRet = Media_UnregisterChannelReader(&pstAheadForm->stRobotCtrl.stMediaCtrl, pstReader)) != RET_SUCCESS )
		{
			printf("%s() Media_UnregisterChannelReader(Reader = %p) failed, ret = 0x%08X; 退出 AheadFormSDK。\r\n", __FUNCTION__, (void*)pstReader, cleanupRet);
			PrintAllErrLog();
			exit(EXIT_FAILURE);
		}

		return ret;
	}
	max_fd = pstReader->rxDataUpdateEventfd;
	while (1)
	{
		FD_ZERO(&rdFdSet);
		FD_SET(pstReader->rxDataUpdateEventfd, &rdFdSet);
		if ( (ret = select(max_fd + 1, &rdFdSet, NULL, NULL, &timeout)) < 0 )
		{
			printf("%s() call select(rxDataUpdateEventfd = %d) failed: %s\r\n", __FUNCTION__, pstReader->rxDataUpdateEventfd, strerror(errno));
			ret = ERR_SELECT;
			break;
		}
		else if (ret == 0)
		{
			break;
		}
		ret = 0;
		if (FD_ISSET(pstReader->rxDataUpdateEventfd, &rdFdSet))
		{
			if (eventfd_read(pstReader->rxDataUpdateEventfd, &eventCnt) < 0 && errno != EAGAIN)
			{
				printf("%s() call eventfd_read(rxDataUpdateEventfd = %d) failed: %s\r\n", __FUNCTION__, pstReader->rxDataUpdateEventfd, strerror(errno));
				ret = ERR_EVENTFD_IO_FAILED;
				break;
			}
			//Consume a finite FIFO snapshot; eventCnt is only an update hint.
			readableElemCnt = TopicReader_GetReadableElemCnt(pstReader);
			for (i = 0; i < readableElemCnt; i++)
			{
				if ( (ret = TopicReader_PopElem(pstReader, &stReply)) <= 0 )
					break;

				if (Media_MatchChannelReplyWithReq(&stReply, &stAsk))
					break;

				ret = 0;
			}
		}

		if (ret != 0)
			break;

		if (timeout.tv_sec == 0 && timeout.tv_usec == 0)
			break;
	}

	if ( (cleanupRet = Media_UnregisterChannelReader(&pstAheadForm->stRobotCtrl.stMediaCtrl, pstReader)) != RET_SUCCESS )
	{
		printf("%s() Media_UnregisterChannelReader(Reader = %p) failed, ret = 0x%08X; 退出 AheadFormSDK。\r\n", __FUNCTION__, (void*)pstReader, cleanupRet);
		PrintAllErrLog();
		exit(EXIT_FAILURE);
	}

	if (ret == 0)
		return ERR_REQUEST_TIMEOUT;

	if (ret < 0)
		return ret;

	printf("media channel askId=%llu, remote ret=%d (acceptance result).\r\n", stReply.askId, stReply.retCode);
	return stReply.retCode == 0 ? RET_SUCCESS : ERR_RX_FAILED;
}

#else //MediaChannel by Reader callback
typedef struct
{
	AHEADFORM_MEDIA_CHANNEL_ASK stAsk;
	AHEADFORM_MEDIA_CHANNEL_REPLY_SAMPLE stReply;
	int completionEventfd;
	int isReplied;
} MEDIACHANNEL_CALLBACK_CONTEXT;

static void CopyMediaChannelReplyIfMatchesReq(const void *pElem, void *pUserArg)
{
	const AHEADFORM_MEDIA_CHANNEL_REPLY_SAMPLE *pstReply = (const AHEADFORM_MEDIA_CHANNEL_REPLY_SAMPLE*)pElem;
	MEDIACHANNEL_CALLBACK_CONTEXT *pstContext = (MEDIACHANNEL_CALLBACK_CONTEXT*)pUserArg;

	if (pstContext->isReplied || !Media_MatchChannelReplyWithReq(pstReply, &pstContext->stAsk))
		return;

	pstContext->stReply = *pstReply;
	pstContext->isReplied = 1;
	if (eventfd_write(pstContext->completionEventfd, 1) < 0 && errno != EAGAIN)
		PRINT_FUNCTION_ERRINFO(ERR_EVENTFD_IO_FAILED, __FUNCTION__, "call eventfd_write(completionEventfd = %d) failed: %s", pstContext->completionEventfd, strerror(errno));
}

int UserCmd_MediaChannel(AHEADFORM_ALL_CTRL *pstAheadForm, int argc, char *argv[])
{
	MEDIACHANNEL_CALLBACK_CONTEXT stContext = {.isReplied = 0};
	AHEADFORM_TOPIC_READER_PARA stReaderPara = {CopyMediaChannelReplyIfMatchesReq, &stContext};
	AHEADFORM_TOPIC_READER *pstReader = NULL;
	AHEADFORM_MEDIA_CHANNEL_PARA stPara;
	long values[4];
	unsigned int parameterIndex;
	struct timeval timeout = {.tv_sec = REQUEST_REPLY_TIMEOUT_MS / 1000, .tv_usec = REQUEST_REPLY_TIMEOUT_MS % 1000 * 1000};
	fd_set rdFdSet;
	int max_fd, cleanupRet, ret;

	if ( (ret = IsUserInError(argc, argv, "5", "1,2,3,4")) != RET_SUCCESS )
		return ret;

	for (parameterIndex = 0; parameterIndex < 4; parameterIndex++)
	{
		values[parameterIndex] = strtol(argv[parameterIndex + 1], NULL, 0);
		if (values[parameterIndex] > AHEADFORM_CHANNEL_ENABLE)
			return ERR_ARGV;
	}
	stPara.videoCtrl = (unsigned char)values[0];
	stPara.micCtrl = (unsigned char)values[1];
	stPara.playbackCtrl = (unsigned char)values[2];
	stPara.vadCtrl = (unsigned char)values[3];

	if ( (ret = Media_BuildChannelRequest(&pstAheadForm->stRobotCtrl.stMediaCtrl, &stPara, &stContext.stAsk)) != RET_SUCCESS )
		return ret;

	if ( (stContext.completionEventfd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC)) <= 0 )
	{
		printf("%s() call eventfd(MediaChannel completion) failed: %s\r\n", __FUNCTION__, strerror(errno));
		return ERR_EVENTFD_CREATE_FAILED;
	}

	if ( (pstReader = Media_RegisterChannelReader(&pstAheadForm->stRobotCtrl.stMediaCtrl, &stReaderPara)) == NULL )
	{
		if (close(stContext.completionEventfd) < 0)
		{
			printf("%s() call close(stContext.completionEventfd = %d) failed: %s\r\n", __FUNCTION__, stContext.completionEventfd, strerror(errno));
			return ERR_EVENTFD_IO_FAILED;
		}

		return ERRCODE_INIT_FAIL;
	}

	if ( (ret = Media_SendChannelRequest(&pstAheadForm->stRobotCtrl.stMediaCtrl, &stContext.stAsk)) != RET_SUCCESS )
	{
		if ( (cleanupRet = Media_UnregisterChannelReader(&pstAheadForm->stRobotCtrl.stMediaCtrl, pstReader)) != RET_SUCCESS )
		{
			printf("%s() Media_UnregisterChannelReader(Reader = %p) failed, ret = 0x%08X; 退出 AheadFormSDK。\r\n", __FUNCTION__, (void*)pstReader, cleanupRet);
			PrintAllErrLog();
			exit(EXIT_FAILURE);
		}

		if (close(stContext.completionEventfd) < 0)
		{
			printf("%s() call close(stContext.completionEventfd = %d) failed: %s\r\n", __FUNCTION__, stContext.completionEventfd, strerror(errno));
			return ERR_EVENTFD_IO_FAILED;
		}

		return ret;
	}
	max_fd = stContext.completionEventfd;
	while (1)
	{
		FD_ZERO(&rdFdSet);
		FD_SET(stContext.completionEventfd, &rdFdSet);
		if ( (ret = select(max_fd + 1, &rdFdSet, NULL, NULL, &timeout)) < 0 )
		{
			printf("%s() call select(completionEventfd = %d) failed: %s\r\n", __FUNCTION__, stContext.completionEventfd, strerror(errno));
			ret = ERR_SELECT;
			break;
		}
		else if (ret == 0)
		{
			break;
		}
		ret = RET_SUCCESS;
		if (FD_ISSET(stContext.completionEventfd, &rdFdSet))
			break;

		if (timeout.tv_sec == 0 && timeout.tv_usec == 0)
			break;
	}

	//Unregister synchronizes with callbacks before this thread reads their output.
	if ( (cleanupRet = Media_UnregisterChannelReader(&pstAheadForm->stRobotCtrl.stMediaCtrl, pstReader)) != RET_SUCCESS )
	{
		printf("%s() Media_UnregisterChannelReader(Reader = %p) failed, ret = 0x%08X; 退出 AheadFormSDK。\r\n", __FUNCTION__, (void*)pstReader, cleanupRet);
		PrintAllErrLog();
		exit(EXIT_FAILURE);
	}

	if (close(stContext.completionEventfd) < 0)
	{
		printf("%s() call close(stContext.completionEventfd = %d) failed: %s\r\n", __FUNCTION__, stContext.completionEventfd, strerror(errno));
		return ERR_EVENTFD_IO_FAILED;
	}

	if (ret != RET_SUCCESS)
		return ret;

	if (!stContext.isReplied)
		return ERR_REQUEST_TIMEOUT;

	printf("media channel askId=%llu, remote ret=%d (acceptance result).\r\n", stContext.stReply.askId, stContext.stReply.retCode);
	return stContext.stReply.retCode == 0 ? RET_SUCCESS : ERR_RX_FAILED;
}
#endif

#if USER_CMD_REPLY_METHOD == USER_CMD_REPLY_METHOD_ASYNC_REQUEST //HardwareControl by async request
int UserCmd_HardwareControl(AHEADFORM_ALL_CTRL *pstAheadForm, int argc, char *argv[])
{
	AHEADFORM_HARDWARE_CONTROL_REPLY_SAMPLE stReply;
	AHEADFORM_HARDWARE_CONTROL_PARA stPara;
	long action, device;
	struct timeval timeout = {.tv_sec = REQUEST_REPLY_TIMEOUT_MS / 1000, .tv_usec = REQUEST_REPLY_TIMEOUT_MS % 1000 * 1000};
	fd_set rdFdSet;
	eventfd_t eventCnt;
	int notifyEventfd, max_fd, cleanupRet, ret;

	if ( (ret = IsUserInError(argc, argv, "3", "1,2")) != RET_SUCCESS )
		return ret;

	action = strtol(argv[1], NULL, 0);
	device = strtol(argv[2], NULL, 0);
	if ((action != AHEADFORM_HARDWARE_ACTION_PRINT_ERROR_LOG && action != AHEADFORM_HARDWARE_ACTION_EXIT_DAEMON) || device < AHEADFORM_HARDWARE_DEVICE_FACE || device > AHEADFORM_HARDWARE_DEVICE_SENSOR)
		return ERR_ARGV;

	stPara.action = (unsigned char)action;
	stPara.device = (unsigned char)device;

	if ( (notifyEventfd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC)) <= 0 )
	{
		printf("%s() call eventfd(HardwareControl completion) failed: %s\r\n", __FUNCTION__, strerror(errno));
		return ERR_EVENTFD_CREATE_FAILED;
	}

	if ( (ret = AheadForm_HardwareControlAsync(pstAheadForm, &stPara, REQUEST_REPLY_TIMEOUT_MS * 1000, notifyEventfd, &stReply)) != RET_SUCCESS )
	{
		if ( (cleanupRet = AheadForm_CancelHardwareControlAsyncRequest(pstAheadForm, notifyEventfd)) != RET_SUCCESS )
		{
			printf("%s() AheadForm_CancelHardwareControlAsyncRequest(notifyEventfd = %d) failed, ret = 0x%08X; 退出 AheadFormSDK。\r\n", __FUNCTION__, notifyEventfd, cleanupRet);
			PrintAllErrLog();
			exit(EXIT_FAILURE);
		}

		if (close(notifyEventfd) < 0)
		{
			printf("%s() call close(notifyEventfd = %d) failed: %s\r\n", __FUNCTION__, notifyEventfd, strerror(errno));
			return ERR_EVENTFD_IO_FAILED;
		}

		return ret;
	}
	max_fd = notifyEventfd;
	while (1)
	{
		FD_ZERO(&rdFdSet);
		FD_SET(notifyEventfd, &rdFdSet);
		//Other application fds can share this select set and use their own FD_ISSET handlers.
		if ( (ret = select(max_fd + 1, &rdFdSet, NULL, NULL, &timeout)) < 0 )
		{
			printf("%s() call select(notifyEventfd = %d) failed: %s\r\n", __FUNCTION__, notifyEventfd, strerror(errno));
			ret = ERR_SELECT;
			break;
		}
		else if (ret == 0)
		{
			break;
		}
		ret = 0;
		if (FD_ISSET(notifyEventfd, &rdFdSet))
		{
			ret = AheadForm_CheckIsHardwareControlAsyncRequestReplied(pstAheadForm, notifyEventfd);
			break;
		}

		if (timeout.tv_sec == 0 && timeout.tv_usec == 0)
			break;
	}

	if ( (cleanupRet = AheadForm_CancelHardwareControlAsyncRequest(pstAheadForm, notifyEventfd)) != RET_SUCCESS )
	{
		printf("%s() AheadForm_CancelHardwareControlAsyncRequest(notifyEventfd = %d) failed, ret = 0x%08X; 退出 AheadFormSDK。\r\n", __FUNCTION__, notifyEventfd, cleanupRet);
		PrintAllErrLog();
		exit(EXIT_FAILURE);
	}

	if (ret == 1 || ret == 0 || ret == ERRCODE_REQUEST_TIMEOUT)
	{
		//Cancel ends SDK access; a final notification resolves completion racing with cancellation.
		if (eventfd_read(notifyEventfd, &eventCnt) == 0)
		{
			ret = 1;
		}
		else if (errno == EAGAIN)
		{
			ret = ERRCODE_REQUEST_TIMEOUT;
		}
		else
		{
			printf("%s() call eventfd_read(notifyEventfd = %d) failed: %s\r\n", __FUNCTION__, notifyEventfd, strerror(errno));
			ret = ERR_EVENTFD_IO_FAILED;
		}
	}

	if (close(notifyEventfd) < 0)
	{
		printf("%s() call close(notifyEventfd = %d) failed: %s\r\n", __FUNCTION__, notifyEventfd, strerror(errno));
		return ERR_EVENTFD_IO_FAILED;
	}

	if (ret == ERRCODE_REQUEST_TIMEOUT)
		return ERR_REQUEST_TIMEOUT;

	if (ret != 1)
		return ret;

	printf("hardware askId=%llu, action=%u, device=%u, remote ret=%d (acceptance result).\r\n", stReply.askId, stReply.action, stReply.device, stReply.retCode);
	return stReply.retCode == 0 ? RET_SUCCESS : ERR_RX_FAILED;
}

#elif USER_CMD_REPLY_METHOD == USER_CMD_REPLY_METHOD_RX_EVENTFD //HardwareControl by rxDataUpdateEventfd
int UserCmd_HardwareControl(AHEADFORM_ALL_CTRL *pstAheadForm, int argc, char *argv[])
{
	AHEADFORM_TOPIC_READER_PARA stReaderPara = {0};
	AHEADFORM_TOPIC_READER *pstReader = NULL;
	AHEADFORM_HARDWARE_CONTROL_ASK stAsk;
	AHEADFORM_HARDWARE_CONTROL_REPLY_SAMPLE stReply;
	AHEADFORM_HARDWARE_CONTROL_PARA stPara;
	long action, device;
	struct timeval timeout = {.tv_sec = REQUEST_REPLY_TIMEOUT_MS / 1000, .tv_usec = REQUEST_REPLY_TIMEOUT_MS % 1000 * 1000};
	fd_set rdFdSet;
	eventfd_t eventCnt;
	unsigned int i, readableElemCnt;
	int max_fd, cleanupRet, ret;

	if ( (ret = IsUserInError(argc, argv, "3", "1,2")) != RET_SUCCESS )
		return ret;

	action = strtol(argv[1], NULL, 0);
	device = strtol(argv[2], NULL, 0);
	if ((action != AHEADFORM_HARDWARE_ACTION_PRINT_ERROR_LOG && action != AHEADFORM_HARDWARE_ACTION_EXIT_DAEMON) || device < AHEADFORM_HARDWARE_DEVICE_FACE || device > AHEADFORM_HARDWARE_DEVICE_SENSOR)
		return ERR_ARGV;

	stPara.action = (unsigned char)action;
	stPara.device = (unsigned char)device;

	if ( (ret = AheadForm_BuildHardwareControlRequest(pstAheadForm, &stPara, &stAsk)) != RET_SUCCESS )
		return ret;

	if ( (pstReader = AheadForm_RegisterHardwareControlReader(pstAheadForm, &stReaderPara)) == NULL )
		return ERRCODE_INIT_FAIL;

	if ( (ret = AheadForm_SendHardwareControlRequest(pstAheadForm, &stAsk)) != RET_SUCCESS )
	{
		if ( (cleanupRet = AheadForm_UnregisterHardwareControlReader(pstAheadForm, pstReader)) != RET_SUCCESS )
		{
			printf("%s() AheadForm_UnregisterHardwareControlReader(Reader = %p) failed, ret = 0x%08X; 退出 AheadFormSDK。\r\n", __FUNCTION__, (void*)pstReader, cleanupRet);
			PrintAllErrLog();
			exit(EXIT_FAILURE);
		}

		return ret;
	}
	max_fd = pstReader->rxDataUpdateEventfd;
	while (1)
	{
		FD_ZERO(&rdFdSet);
		FD_SET(pstReader->rxDataUpdateEventfd, &rdFdSet);
		if ( (ret = select(max_fd + 1, &rdFdSet, NULL, NULL, &timeout)) < 0 )
		{
			printf("%s() call select(rxDataUpdateEventfd = %d) failed: %s\r\n", __FUNCTION__, pstReader->rxDataUpdateEventfd, strerror(errno));
			ret = ERR_SELECT;
			break;
		}
		else if (ret == 0)
		{
			break;
		}
		ret = 0;
		if (FD_ISSET(pstReader->rxDataUpdateEventfd, &rdFdSet))
		{
			if (eventfd_read(pstReader->rxDataUpdateEventfd, &eventCnt) < 0 && errno != EAGAIN)
			{
				printf("%s() call eventfd_read(rxDataUpdateEventfd = %d) failed: %s\r\n", __FUNCTION__, pstReader->rxDataUpdateEventfd, strerror(errno));
				ret = ERR_EVENTFD_IO_FAILED;
				break;
			}
			//Consume a finite FIFO snapshot; eventCnt is only an update hint.
			readableElemCnt = TopicReader_GetReadableElemCnt(pstReader);
			for (i = 0; i < readableElemCnt; i++)
			{
				if ( (ret = TopicReader_PopElem(pstReader, &stReply)) <= 0 )
					break;

				if (AheadForm_MatchHardwareControlReplyWithReq(&stReply, &stAsk))
					break;

				ret = 0;
			}
		}

		if (ret != 0)
			break;

		if (timeout.tv_sec == 0 && timeout.tv_usec == 0)
			break;
	}

	if ( (cleanupRet = AheadForm_UnregisterHardwareControlReader(pstAheadForm, pstReader)) != RET_SUCCESS )
	{
		printf("%s() AheadForm_UnregisterHardwareControlReader(Reader = %p) failed, ret = 0x%08X; 退出 AheadFormSDK。\r\n", __FUNCTION__, (void*)pstReader, cleanupRet);
		PrintAllErrLog();
		exit(EXIT_FAILURE);
	}

	if (ret == 0)
		return ERR_REQUEST_TIMEOUT;

	if (ret < 0)
		return ret;

	printf("hardware askId=%llu, action=%u, device=%u, remote ret=%d (acceptance result).\r\n", stReply.askId, stReply.action, stReply.device, stReply.retCode);
	return stReply.retCode == 0 ? RET_SUCCESS : ERR_RX_FAILED;
}

#else //HardwareControl by Reader callback
typedef struct
{
	AHEADFORM_HARDWARE_CONTROL_ASK stAsk;
	AHEADFORM_HARDWARE_CONTROL_REPLY_SAMPLE stReply;
	int completionEventfd;
	int isReplied;
} HARDWARECONTROL_CALLBACK_CONTEXT;

static void CopyHardwareControlReplyIfMatchesReq(const void *pElem, void *pUserArg)
{
	const AHEADFORM_HARDWARE_CONTROL_REPLY_SAMPLE *pstReply = (const AHEADFORM_HARDWARE_CONTROL_REPLY_SAMPLE*)pElem;
	HARDWARECONTROL_CALLBACK_CONTEXT *pstContext = (HARDWARECONTROL_CALLBACK_CONTEXT*)pUserArg;

	if (pstContext->isReplied || !AheadForm_MatchHardwareControlReplyWithReq(pstReply, &pstContext->stAsk))
		return;

	pstContext->stReply = *pstReply;
	pstContext->isReplied = 1;
	if (eventfd_write(pstContext->completionEventfd, 1) < 0 && errno != EAGAIN)
		PRINT_FUNCTION_ERRINFO(ERR_EVENTFD_IO_FAILED, __FUNCTION__, "call eventfd_write(completionEventfd = %d) failed: %s", pstContext->completionEventfd, strerror(errno));
}

int UserCmd_HardwareControl(AHEADFORM_ALL_CTRL *pstAheadForm, int argc, char *argv[])
{
	HARDWARECONTROL_CALLBACK_CONTEXT stContext = {.isReplied = 0};
	AHEADFORM_TOPIC_READER_PARA stReaderPara = {CopyHardwareControlReplyIfMatchesReq, &stContext};
	AHEADFORM_TOPIC_READER *pstReader = NULL;
	AHEADFORM_HARDWARE_CONTROL_PARA stPara;
	long action, device;
	struct timeval timeout = {.tv_sec = REQUEST_REPLY_TIMEOUT_MS / 1000, .tv_usec = REQUEST_REPLY_TIMEOUT_MS % 1000 * 1000};
	fd_set rdFdSet;
	int max_fd, cleanupRet, ret;

	if ( (ret = IsUserInError(argc, argv, "3", "1,2")) != RET_SUCCESS )
		return ret;

	action = strtol(argv[1], NULL, 0);
	device = strtol(argv[2], NULL, 0);
	if ((action != AHEADFORM_HARDWARE_ACTION_PRINT_ERROR_LOG && action != AHEADFORM_HARDWARE_ACTION_EXIT_DAEMON) || device < AHEADFORM_HARDWARE_DEVICE_FACE || device > AHEADFORM_HARDWARE_DEVICE_SENSOR)
		return ERR_ARGV;

	stPara.action = (unsigned char)action;
	stPara.device = (unsigned char)device;

	if ( (ret = AheadForm_BuildHardwareControlRequest(pstAheadForm, &stPara, &stContext.stAsk)) != RET_SUCCESS )
		return ret;

	if ( (stContext.completionEventfd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC)) <= 0 )
	{
		printf("%s() call eventfd(HardwareControl completion) failed: %s\r\n", __FUNCTION__, strerror(errno));
		return ERR_EVENTFD_CREATE_FAILED;
	}

	if ( (pstReader = AheadForm_RegisterHardwareControlReader(pstAheadForm, &stReaderPara)) == NULL )
	{
		if (close(stContext.completionEventfd) < 0)
		{
			printf("%s() call close(stContext.completionEventfd = %d) failed: %s\r\n", __FUNCTION__, stContext.completionEventfd, strerror(errno));
			return ERR_EVENTFD_IO_FAILED;
		}

		return ERRCODE_INIT_FAIL;
	}

	if ( (ret = AheadForm_SendHardwareControlRequest(pstAheadForm, &stContext.stAsk)) != RET_SUCCESS )
	{
		if ( (cleanupRet = AheadForm_UnregisterHardwareControlReader(pstAheadForm, pstReader)) != RET_SUCCESS )
		{
			printf("%s() AheadForm_UnregisterHardwareControlReader(Reader = %p) failed, ret = 0x%08X; 退出 AheadFormSDK。\r\n", __FUNCTION__, (void*)pstReader, cleanupRet);
			PrintAllErrLog();
			exit(EXIT_FAILURE);
		}

		if (close(stContext.completionEventfd) < 0)
		{
			printf("%s() call close(stContext.completionEventfd = %d) failed: %s\r\n", __FUNCTION__, stContext.completionEventfd, strerror(errno));
			return ERR_EVENTFD_IO_FAILED;
		}

		return ret;
	}
	max_fd = stContext.completionEventfd;
	while (1)
	{
		FD_ZERO(&rdFdSet);
		FD_SET(stContext.completionEventfd, &rdFdSet);
		if ( (ret = select(max_fd + 1, &rdFdSet, NULL, NULL, &timeout)) < 0 )
		{
			printf("%s() call select(completionEventfd = %d) failed: %s\r\n", __FUNCTION__, stContext.completionEventfd, strerror(errno));
			ret = ERR_SELECT;
			break;
		}
		else if (ret == 0)
		{
			break;
		}
		ret = RET_SUCCESS;
		if (FD_ISSET(stContext.completionEventfd, &rdFdSet))
			break;

		if (timeout.tv_sec == 0 && timeout.tv_usec == 0)
			break;
	}

	//Unregister synchronizes with callbacks before this thread reads their output.
	if ( (cleanupRet = AheadForm_UnregisterHardwareControlReader(pstAheadForm, pstReader)) != RET_SUCCESS )
	{
		printf("%s() AheadForm_UnregisterHardwareControlReader(Reader = %p) failed, ret = 0x%08X; 退出 AheadFormSDK。\r\n", __FUNCTION__, (void*)pstReader, cleanupRet);
		PrintAllErrLog();
		exit(EXIT_FAILURE);
	}

	if (close(stContext.completionEventfd) < 0)
	{
		printf("%s() call close(stContext.completionEventfd = %d) failed: %s\r\n", __FUNCTION__, stContext.completionEventfd, strerror(errno));
		return ERR_EVENTFD_IO_FAILED;
	}

	if (ret != RET_SUCCESS)
		return ret;

	if (!stContext.isReplied)
		return ERR_REQUEST_TIMEOUT;

	printf("hardware askId=%llu, action=%u, device=%u, remote ret=%d (acceptance result).\r\n", stContext.stReply.askId, stContext.stReply.action, stContext.stReply.device, stContext.stReply.retCode);
	return stContext.stReply.retCode == 0 ? RET_SUCCESS : ERR_RX_FAILED;
}
#endif

#if USER_CMD_REPLY_METHOD == USER_CMD_REPLY_METHOD_ASYNC_REQUEST //SafetyControl by async request
int UserCmd_SafetyControl(AHEADFORM_ALL_CTRL *pstAheadForm, int argc, char *argv[])
{
	AHEADFORM_SAFETY_CONTROL_REPLY_SAMPLE stReply;
	AHEADFORM_SAFETY_CONTROL_PARA stPara = {0};
	long action, facePermit, neckPermit;
	struct timeval timeout = {.tv_sec = REQUEST_REPLY_TIMEOUT_MS / 1000, .tv_usec = REQUEST_REPLY_TIMEOUT_MS % 1000 * 1000};
	fd_set rdFdSet;
	eventfd_t eventCnt;
	int notifyEventfd, max_fd, cleanupRet, ret;

	if ( (ret = IsUserInError(argc, argv, "2;4", "1;1,2,3")) != RET_SUCCESS )
		return ret;

	action = strtol(argv[1], NULL, 0);
	if (action < AHEADFORM_SAFETY_ACTION_MOTION_PERMISSION || action > AHEADFORM_SAFETY_ACTION_ESTOP)
		return ERR_ARGV;

	stPara.action = (unsigned char)action;
	if (action == AHEADFORM_SAFETY_ACTION_MOTION_PERMISSION)
	{
		if (argc != 4)
			return ERR_ARGC;

		facePermit = strtol(argv[2], NULL, 0);
		neckPermit = strtol(argv[3], NULL, 0);
		if (facePermit > 1 || neckPermit > 1)
			return ERR_ARGV;

		stPara.faceMotionPermitted = (unsigned char)facePermit;
		stPara.neckMotionPermitted = (unsigned char)neckPermit;
	}
	else if (argc != 2)
	{
		return ERR_ARGC;
	}

	if ( (notifyEventfd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC)) <= 0 )
	{
		printf("%s() call eventfd(SafetyControl completion) failed: %s\r\n", __FUNCTION__, strerror(errno));
		return ERR_EVENTFD_CREATE_FAILED;
	}

	if ( (ret = AheadForm_SafetyControlAsync(pstAheadForm, &stPara, REQUEST_REPLY_TIMEOUT_MS * 1000, notifyEventfd, &stReply)) != RET_SUCCESS )
	{
		if ( (cleanupRet = AheadForm_CancelSafetyControlAsyncRequest(pstAheadForm, notifyEventfd)) != RET_SUCCESS )
		{
			printf("%s() AheadForm_CancelSafetyControlAsyncRequest(notifyEventfd = %d) failed, ret = 0x%08X; 退出 AheadFormSDK。\r\n", __FUNCTION__, notifyEventfd, cleanupRet);
			PrintAllErrLog();
			exit(EXIT_FAILURE);
		}

		if (close(notifyEventfd) < 0)
		{
			printf("%s() call close(notifyEventfd = %d) failed: %s\r\n", __FUNCTION__, notifyEventfd, strerror(errno));
			return ERR_EVENTFD_IO_FAILED;
		}

		return ret;
	}
	max_fd = notifyEventfd;
	while (1)
	{
		FD_ZERO(&rdFdSet);
		FD_SET(notifyEventfd, &rdFdSet);
		//Other application fds can share this select set and use their own FD_ISSET handlers.
		if ( (ret = select(max_fd + 1, &rdFdSet, NULL, NULL, &timeout)) < 0 )
		{
			printf("%s() call select(notifyEventfd = %d) failed: %s\r\n", __FUNCTION__, notifyEventfd, strerror(errno));
			ret = ERR_SELECT;
			break;
		}
		else if (ret == 0)
		{
			break;
		}
		ret = 0;
		if (FD_ISSET(notifyEventfd, &rdFdSet))
		{
			ret = AheadForm_CheckIsSafetyControlAsyncRequestReplied(pstAheadForm, notifyEventfd);
			break;
		}

		if (timeout.tv_sec == 0 && timeout.tv_usec == 0)
			break;
	}

	if ( (cleanupRet = AheadForm_CancelSafetyControlAsyncRequest(pstAheadForm, notifyEventfd)) != RET_SUCCESS )
	{
		printf("%s() AheadForm_CancelSafetyControlAsyncRequest(notifyEventfd = %d) failed, ret = 0x%08X; 退出 AheadFormSDK。\r\n", __FUNCTION__, notifyEventfd, cleanupRet);
		PrintAllErrLog();
		exit(EXIT_FAILURE);
	}

	if (ret == 1 || ret == 0 || ret == ERRCODE_REQUEST_TIMEOUT)
	{
		//Cancel ends SDK access; a final notification resolves completion racing with cancellation.
		if (eventfd_read(notifyEventfd, &eventCnt) == 0)
		{
			ret = 1;
		}
		else if (errno == EAGAIN)
		{
			ret = ERRCODE_REQUEST_TIMEOUT;
		}
		else
		{
			printf("%s() call eventfd_read(notifyEventfd = %d) failed: %s\r\n", __FUNCTION__, notifyEventfd, strerror(errno));
			ret = ERR_EVENTFD_IO_FAILED;
		}
	}

	if (close(notifyEventfd) < 0)
	{
		printf("%s() call close(notifyEventfd = %d) failed: %s\r\n", __FUNCTION__, notifyEventfd, strerror(errno));
		return ERR_EVENTFD_IO_FAILED;
	}

	if (ret == ERRCODE_REQUEST_TIMEOUT)
		return ERR_REQUEST_TIMEOUT;

	if (ret != 1)
		return ret;

	printf("safety askId=%llu, action=%u, remote ret=%d (acceptance result; inspect status for completion).\r\n", stReply.askId, stReply.action, stReply.retCode);
	return stReply.retCode == 0 ? RET_SUCCESS : ERR_RX_FAILED;
}

#elif USER_CMD_REPLY_METHOD == USER_CMD_REPLY_METHOD_RX_EVENTFD //SafetyControl by rxDataUpdateEventfd
int UserCmd_SafetyControl(AHEADFORM_ALL_CTRL *pstAheadForm, int argc, char *argv[])
{
	AHEADFORM_TOPIC_READER_PARA stReaderPara = {0};
	AHEADFORM_TOPIC_READER *pstReader = NULL;
	AHEADFORM_SAFETY_CONTROL_ASK stAsk;
	AHEADFORM_SAFETY_CONTROL_REPLY_SAMPLE stReply;
	AHEADFORM_SAFETY_CONTROL_PARA stPara = {0};
	long action, facePermit, neckPermit;
	struct timeval timeout = {.tv_sec = REQUEST_REPLY_TIMEOUT_MS / 1000, .tv_usec = REQUEST_REPLY_TIMEOUT_MS % 1000 * 1000};
	fd_set rdFdSet;
	eventfd_t eventCnt;
	unsigned int i, readableElemCnt;
	int max_fd, cleanupRet, ret;

	if ( (ret = IsUserInError(argc, argv, "2;4", "1;1,2,3")) != RET_SUCCESS )
		return ret;

	action = strtol(argv[1], NULL, 0);
	if (action < AHEADFORM_SAFETY_ACTION_MOTION_PERMISSION || action > AHEADFORM_SAFETY_ACTION_ESTOP)
		return ERR_ARGV;

	stPara.action = (unsigned char)action;
	if (action == AHEADFORM_SAFETY_ACTION_MOTION_PERMISSION)
	{
		if (argc != 4)
			return ERR_ARGC;

		facePermit = strtol(argv[2], NULL, 0);
		neckPermit = strtol(argv[3], NULL, 0);
		if (facePermit > 1 || neckPermit > 1)
			return ERR_ARGV;

		stPara.faceMotionPermitted = (unsigned char)facePermit;
		stPara.neckMotionPermitted = (unsigned char)neckPermit;
	}
	else if (argc != 2)
	{
		return ERR_ARGC;
	}

	if ( (ret = AheadForm_BuildSafetyControlRequest(pstAheadForm, &stPara, &stAsk)) != RET_SUCCESS )
		return ret;

	if ( (pstReader = AheadForm_RegisterSafetyControlReader(pstAheadForm, &stReaderPara)) == NULL )
		return ERRCODE_INIT_FAIL;

	if ( (ret = AheadForm_SendSafetyControlRequest(pstAheadForm, &stAsk)) != RET_SUCCESS )
	{
		if ( (cleanupRet = AheadForm_UnregisterSafetyControlReader(pstAheadForm, pstReader)) != RET_SUCCESS )
		{
			printf("%s() AheadForm_UnregisterSafetyControlReader(Reader = %p) failed, ret = 0x%08X; 退出 AheadFormSDK。\r\n", __FUNCTION__, (void*)pstReader, cleanupRet);
			PrintAllErrLog();
			exit(EXIT_FAILURE);
		}

		return ret;
	}
	max_fd = pstReader->rxDataUpdateEventfd;
	while (1)
	{
		FD_ZERO(&rdFdSet);
		FD_SET(pstReader->rxDataUpdateEventfd, &rdFdSet);
		if ( (ret = select(max_fd + 1, &rdFdSet, NULL, NULL, &timeout)) < 0 )
		{
			printf("%s() call select(rxDataUpdateEventfd = %d) failed: %s\r\n", __FUNCTION__, pstReader->rxDataUpdateEventfd, strerror(errno));
			ret = ERR_SELECT;
			break;
		}
		else if (ret == 0)
		{
			break;
		}
		ret = 0;
		if (FD_ISSET(pstReader->rxDataUpdateEventfd, &rdFdSet))
		{
			if (eventfd_read(pstReader->rxDataUpdateEventfd, &eventCnt) < 0 && errno != EAGAIN)
			{
				printf("%s() call eventfd_read(rxDataUpdateEventfd = %d) failed: %s\r\n", __FUNCTION__, pstReader->rxDataUpdateEventfd, strerror(errno));
				ret = ERR_EVENTFD_IO_FAILED;
				break;
			}
			//Consume a finite FIFO snapshot; eventCnt is only an update hint.
			readableElemCnt = TopicReader_GetReadableElemCnt(pstReader);
			for (i = 0; i < readableElemCnt; i++)
			{
				if ( (ret = TopicReader_PopElem(pstReader, &stReply)) <= 0 )
					break;

				if (AheadForm_MatchSafetyControlReplyWithReq(&stReply, &stAsk))
					break;

				ret = 0;
			}
		}

		if (ret != 0)
			break;

		if (timeout.tv_sec == 0 && timeout.tv_usec == 0)
			break;
	}

	if ( (cleanupRet = AheadForm_UnregisterSafetyControlReader(pstAheadForm, pstReader)) != RET_SUCCESS )
	{
		printf("%s() AheadForm_UnregisterSafetyControlReader(Reader = %p) failed, ret = 0x%08X; 退出 AheadFormSDK。\r\n", __FUNCTION__, (void*)pstReader, cleanupRet);
		PrintAllErrLog();
		exit(EXIT_FAILURE);
	}

	if (ret == 0)
		return ERR_REQUEST_TIMEOUT;

	if (ret < 0)
		return ret;

	printf("safety askId=%llu, action=%u, remote ret=%d (acceptance result; inspect status for completion).\r\n", stReply.askId, stReply.action, stReply.retCode);
	return stReply.retCode == 0 ? RET_SUCCESS : ERR_RX_FAILED;
}

#else //SafetyControl by Reader callback
typedef struct
{
	AHEADFORM_SAFETY_CONTROL_ASK stAsk;
	AHEADFORM_SAFETY_CONTROL_REPLY_SAMPLE stReply;
	int completionEventfd;
	int isReplied;
} SAFETYCONTROL_CALLBACK_CONTEXT;

static void CopySafetyControlReplyIfMatchesReq(const void *pElem, void *pUserArg)
{
	const AHEADFORM_SAFETY_CONTROL_REPLY_SAMPLE *pstReply = (const AHEADFORM_SAFETY_CONTROL_REPLY_SAMPLE*)pElem;
	SAFETYCONTROL_CALLBACK_CONTEXT *pstContext = (SAFETYCONTROL_CALLBACK_CONTEXT*)pUserArg;

	if (pstContext->isReplied || !AheadForm_MatchSafetyControlReplyWithReq(pstReply, &pstContext->stAsk))
		return;

	pstContext->stReply = *pstReply;
	pstContext->isReplied = 1;
	if (eventfd_write(pstContext->completionEventfd, 1) < 0 && errno != EAGAIN)
		PRINT_FUNCTION_ERRINFO(ERR_EVENTFD_IO_FAILED, __FUNCTION__, "call eventfd_write(completionEventfd = %d) failed: %s", pstContext->completionEventfd, strerror(errno));
}

int UserCmd_SafetyControl(AHEADFORM_ALL_CTRL *pstAheadForm, int argc, char *argv[])
{
	SAFETYCONTROL_CALLBACK_CONTEXT stContext = {.isReplied = 0};
	AHEADFORM_TOPIC_READER_PARA stReaderPara = {CopySafetyControlReplyIfMatchesReq, &stContext};
	AHEADFORM_TOPIC_READER *pstReader = NULL;
	AHEADFORM_SAFETY_CONTROL_PARA stPara = {0};
	long action, facePermit, neckPermit;
	struct timeval timeout = {.tv_sec = REQUEST_REPLY_TIMEOUT_MS / 1000, .tv_usec = REQUEST_REPLY_TIMEOUT_MS % 1000 * 1000};
	fd_set rdFdSet;
	int max_fd, cleanupRet, ret;

	if ( (ret = IsUserInError(argc, argv, "2;4", "1;1,2,3")) != RET_SUCCESS )
		return ret;

	action = strtol(argv[1], NULL, 0);
	if (action < AHEADFORM_SAFETY_ACTION_MOTION_PERMISSION || action > AHEADFORM_SAFETY_ACTION_ESTOP)
		return ERR_ARGV;

	stPara.action = (unsigned char)action;
	if (action == AHEADFORM_SAFETY_ACTION_MOTION_PERMISSION)
	{
		if (argc != 4)
			return ERR_ARGC;

		facePermit = strtol(argv[2], NULL, 0);
		neckPermit = strtol(argv[3], NULL, 0);
		if (facePermit > 1 || neckPermit > 1)
			return ERR_ARGV;

		stPara.faceMotionPermitted = (unsigned char)facePermit;
		stPara.neckMotionPermitted = (unsigned char)neckPermit;
	}
	else if (argc != 2)
	{
		return ERR_ARGC;
	}

	if ( (ret = AheadForm_BuildSafetyControlRequest(pstAheadForm, &stPara, &stContext.stAsk)) != RET_SUCCESS )
		return ret;

	if ( (stContext.completionEventfd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC)) <= 0 )
	{
		printf("%s() call eventfd(SafetyControl completion) failed: %s\r\n", __FUNCTION__, strerror(errno));
		return ERR_EVENTFD_CREATE_FAILED;
	}

	if ( (pstReader = AheadForm_RegisterSafetyControlReader(pstAheadForm, &stReaderPara)) == NULL )
	{
		if (close(stContext.completionEventfd) < 0)
		{
			printf("%s() call close(stContext.completionEventfd = %d) failed: %s\r\n", __FUNCTION__, stContext.completionEventfd, strerror(errno));
			return ERR_EVENTFD_IO_FAILED;
		}

		return ERRCODE_INIT_FAIL;
	}

	if ( (ret = AheadForm_SendSafetyControlRequest(pstAheadForm, &stContext.stAsk)) != RET_SUCCESS )
	{
		if ( (cleanupRet = AheadForm_UnregisterSafetyControlReader(pstAheadForm, pstReader)) != RET_SUCCESS )
		{
			printf("%s() AheadForm_UnregisterSafetyControlReader(Reader = %p) failed, ret = 0x%08X; 退出 AheadFormSDK。\r\n", __FUNCTION__, (void*)pstReader, cleanupRet);
			PrintAllErrLog();
			exit(EXIT_FAILURE);
		}

		if (close(stContext.completionEventfd) < 0)
		{
			printf("%s() call close(stContext.completionEventfd = %d) failed: %s\r\n", __FUNCTION__, stContext.completionEventfd, strerror(errno));
			return ERR_EVENTFD_IO_FAILED;
		}

		return ret;
	}
	max_fd = stContext.completionEventfd;
	while (1)
	{
		FD_ZERO(&rdFdSet);
		FD_SET(stContext.completionEventfd, &rdFdSet);
		if ( (ret = select(max_fd + 1, &rdFdSet, NULL, NULL, &timeout)) < 0 )
		{
			printf("%s() call select(completionEventfd = %d) failed: %s\r\n", __FUNCTION__, stContext.completionEventfd, strerror(errno));
			ret = ERR_SELECT;
			break;
		}
		else if (ret == 0)
		{
			break;
		}
		ret = RET_SUCCESS;
		if (FD_ISSET(stContext.completionEventfd, &rdFdSet))
			break;

		if (timeout.tv_sec == 0 && timeout.tv_usec == 0)
			break;
	}

	//Unregister synchronizes with callbacks before this thread reads their output.
	if ( (cleanupRet = AheadForm_UnregisterSafetyControlReader(pstAheadForm, pstReader)) != RET_SUCCESS )
	{
		printf("%s() AheadForm_UnregisterSafetyControlReader(Reader = %p) failed, ret = 0x%08X; 退出 AheadFormSDK。\r\n", __FUNCTION__, (void*)pstReader, cleanupRet);
		PrintAllErrLog();
		exit(EXIT_FAILURE);
	}

	if (close(stContext.completionEventfd) < 0)
	{
		printf("%s() call close(stContext.completionEventfd = %d) failed: %s\r\n", __FUNCTION__, stContext.completionEventfd, strerror(errno));
		return ERR_EVENTFD_IO_FAILED;
	}

	if (ret != RET_SUCCESS)
		return ret;

	if (!stContext.isReplied)
		return ERR_REQUEST_TIMEOUT;

	printf("safety askId=%llu, action=%u, remote ret=%d (acceptance result; inspect status for completion).\r\n", stContext.stReply.askId, stContext.stReply.action, stContext.stReply.retCode);
	return stContext.stReply.retCode == 0 ? RET_SUCCESS : ERR_RX_FAILED;
}
#endif

#if USER_CMD_REPLY_METHOD == USER_CMD_REPLY_METHOD_ASYNC_REQUEST //OperatingMode by async request
int UserCmd_OperatingMode(AHEADFORM_ALL_CTRL *pstAheadForm, int argc, char *argv[])
{
	AHEADFORM_OPERATING_MODE_REPLY_SAMPLE stReply;
	unsigned int mode;
	long parsedMode;
	struct timeval timeout = {.tv_sec = REQUEST_REPLY_TIMEOUT_MS / 1000, .tv_usec = REQUEST_REPLY_TIMEOUT_MS % 1000 * 1000};
	fd_set rdFdSet;
	eventfd_t eventCnt;
	int notifyEventfd, max_fd, cleanupRet, ret;

	if ( (ret = IsUserInError(argc, argv, "2", "1")) != RET_SUCCESS )
		return ret;

	parsedMode = strtol(argv[1], NULL, 0);
	if (parsedMode != AHEADFORM_OPERATING_MODE_DEV && parsedMode != AHEADFORM_OPERATING_MODE_INTERACTIVE)
		return ERR_ARGV;

	mode = (unsigned int)parsedMode;

	if ( (notifyEventfd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC)) <= 0 )
	{
		printf("%s() call eventfd(OperatingMode completion) failed: %s\r\n", __FUNCTION__, strerror(errno));
		return ERR_EVENTFD_CREATE_FAILED;
	}

	if ( (ret = AheadForm_OperatingModeAsync(pstAheadForm, mode, REQUEST_REPLY_TIMEOUT_MS * 1000, notifyEventfd, &stReply)) != RET_SUCCESS )
	{
		if ( (cleanupRet = AheadForm_CancelOperatingModeAsyncRequest(pstAheadForm, notifyEventfd)) != RET_SUCCESS )
		{
			printf("%s() AheadForm_CancelOperatingModeAsyncRequest(notifyEventfd = %d) failed, ret = 0x%08X; 退出 AheadFormSDK。\r\n", __FUNCTION__, notifyEventfd, cleanupRet);
			PrintAllErrLog();
			exit(EXIT_FAILURE);
		}

		if (close(notifyEventfd) < 0)
		{
			printf("%s() call close(notifyEventfd = %d) failed: %s\r\n", __FUNCTION__, notifyEventfd, strerror(errno));
			return ERR_EVENTFD_IO_FAILED;
		}

		return ret;
	}
	max_fd = notifyEventfd;
	while (1)
	{
		FD_ZERO(&rdFdSet);
		FD_SET(notifyEventfd, &rdFdSet);
		//Other application fds can share this select set and use their own FD_ISSET handlers.
		if ( (ret = select(max_fd + 1, &rdFdSet, NULL, NULL, &timeout)) < 0 )
		{
			printf("%s() call select(notifyEventfd = %d) failed: %s\r\n", __FUNCTION__, notifyEventfd, strerror(errno));
			ret = ERR_SELECT;
			break;
		}
		else if (ret == 0)
		{
			break;
		}
		ret = 0;
		if (FD_ISSET(notifyEventfd, &rdFdSet))
		{
			ret = AheadForm_CheckIsOperatingModeAsyncRequestReplied(pstAheadForm, notifyEventfd);
			break;
		}

		if (timeout.tv_sec == 0 && timeout.tv_usec == 0)
			break;
	}

	if ( (cleanupRet = AheadForm_CancelOperatingModeAsyncRequest(pstAheadForm, notifyEventfd)) != RET_SUCCESS )
	{
		printf("%s() AheadForm_CancelOperatingModeAsyncRequest(notifyEventfd = %d) failed, ret = 0x%08X; 退出 AheadFormSDK。\r\n", __FUNCTION__, notifyEventfd, cleanupRet);
		PrintAllErrLog();
		exit(EXIT_FAILURE);
	}

	if (ret == 1 || ret == 0 || ret == ERRCODE_REQUEST_TIMEOUT)
	{
		//Cancel ends SDK access; a final notification resolves completion racing with cancellation.
		if (eventfd_read(notifyEventfd, &eventCnt) == 0)
		{
			ret = 1;
		}
		else if (errno == EAGAIN)
		{
			ret = ERRCODE_REQUEST_TIMEOUT;
		}
		else
		{
			printf("%s() call eventfd_read(notifyEventfd = %d) failed: %s\r\n", __FUNCTION__, notifyEventfd, strerror(errno));
			ret = ERR_EVENTFD_IO_FAILED;
		}
	}

	if (close(notifyEventfd) < 0)
	{
		printf("%s() call close(notifyEventfd = %d) failed: %s\r\n", __FUNCTION__, notifyEventfd, strerror(errno));
		return ERR_EVENTFD_IO_FAILED;
	}

	if (ret == ERRCODE_REQUEST_TIMEOUT)
		return ERR_REQUEST_TIMEOUT;

	if (ret != 1)
		return ret;

	printf("operating mode requestId=%llu, result=%u.\r\n", stReply.requestId, stReply.result);
	return stReply.result == AHEADFORM_OPERATING_MODE_RESULT_SUCCEEDED ? RET_SUCCESS : ERR_RX_FAILED;
}

#elif USER_CMD_REPLY_METHOD == USER_CMD_REPLY_METHOD_RX_EVENTFD //OperatingMode by rxDataUpdateEventfd
int UserCmd_OperatingMode(AHEADFORM_ALL_CTRL *pstAheadForm, int argc, char *argv[])
{
	AHEADFORM_TOPIC_READER_PARA stReaderPara = {0};
	AHEADFORM_TOPIC_READER *pstReader = NULL;
	AHEADFORM_OPERATING_MODE_ASK stAsk;
	AHEADFORM_OPERATING_MODE_REPLY_SAMPLE stReply;
	unsigned int mode;
	long parsedMode;
	struct timeval timeout = {.tv_sec = REQUEST_REPLY_TIMEOUT_MS / 1000, .tv_usec = REQUEST_REPLY_TIMEOUT_MS % 1000 * 1000};
	fd_set rdFdSet;
	eventfd_t eventCnt;
	unsigned int i, readableElemCnt;
	int max_fd, cleanupRet, ret;

	if ( (ret = IsUserInError(argc, argv, "2", "1")) != RET_SUCCESS )
		return ret;

	parsedMode = strtol(argv[1], NULL, 0);
	if (parsedMode != AHEADFORM_OPERATING_MODE_DEV && parsedMode != AHEADFORM_OPERATING_MODE_INTERACTIVE)
		return ERR_ARGV;

	mode = (unsigned int)parsedMode;

	if ( (ret = AheadForm_BuildOperatingModeRequest(pstAheadForm, mode, &stAsk)) != RET_SUCCESS )
		return ret;

	if ( (pstReader = AheadForm_RegisterOperatingModeReader(pstAheadForm, &stReaderPara)) == NULL )
		return ERRCODE_INIT_FAIL;

	if ( (ret = AheadForm_SendOperatingModeRequest(pstAheadForm, &stAsk)) != RET_SUCCESS )
	{
		if ( (cleanupRet = AheadForm_UnregisterOperatingModeReader(pstAheadForm, pstReader)) != RET_SUCCESS )
		{
			printf("%s() AheadForm_UnregisterOperatingModeReader(Reader = %p) failed, ret = 0x%08X; 退出 AheadFormSDK。\r\n", __FUNCTION__, (void*)pstReader, cleanupRet);
			PrintAllErrLog();
			exit(EXIT_FAILURE);
		}

		return ret;
	}
	max_fd = pstReader->rxDataUpdateEventfd;
	while (1)
	{
		FD_ZERO(&rdFdSet);
		FD_SET(pstReader->rxDataUpdateEventfd, &rdFdSet);
		if ( (ret = select(max_fd + 1, &rdFdSet, NULL, NULL, &timeout)) < 0 )
		{
			printf("%s() call select(rxDataUpdateEventfd = %d) failed: %s\r\n", __FUNCTION__, pstReader->rxDataUpdateEventfd, strerror(errno));
			ret = ERR_SELECT;
			break;
		}
		else if (ret == 0)
		{
			break;
		}
		ret = 0;
		if (FD_ISSET(pstReader->rxDataUpdateEventfd, &rdFdSet))
		{
			if (eventfd_read(pstReader->rxDataUpdateEventfd, &eventCnt) < 0 && errno != EAGAIN)
			{
				printf("%s() call eventfd_read(rxDataUpdateEventfd = %d) failed: %s\r\n", __FUNCTION__, pstReader->rxDataUpdateEventfd, strerror(errno));
				ret = ERR_EVENTFD_IO_FAILED;
				break;
			}
			//Consume a finite FIFO snapshot; eventCnt is only an update hint.
			readableElemCnt = TopicReader_GetReadableElemCnt(pstReader);
			for (i = 0; i < readableElemCnt; i++)
			{
				if ( (ret = TopicReader_PopElem(pstReader, &stReply)) <= 0 )
					break;

				if (AheadForm_MatchOperatingModeReplyWithReq(&stReply, &stAsk))
					break;

				ret = 0;
			}
		}

		if (ret != 0)
			break;

		if (timeout.tv_sec == 0 && timeout.tv_usec == 0)
			break;
	}

	if ( (cleanupRet = AheadForm_UnregisterOperatingModeReader(pstAheadForm, pstReader)) != RET_SUCCESS )
	{
		printf("%s() AheadForm_UnregisterOperatingModeReader(Reader = %p) failed, ret = 0x%08X; 退出 AheadFormSDK。\r\n", __FUNCTION__, (void*)pstReader, cleanupRet);
		PrintAllErrLog();
		exit(EXIT_FAILURE);
	}

	if (ret == 0)
		return ERR_REQUEST_TIMEOUT;

	if (ret < 0)
		return ret;

	printf("operating mode requestId=%llu, result=%u.\r\n", stReply.requestId, stReply.result);
	return stReply.result == AHEADFORM_OPERATING_MODE_RESULT_SUCCEEDED ? RET_SUCCESS : ERR_RX_FAILED;
}

#else //OperatingMode by Reader callback
typedef struct
{
	AHEADFORM_OPERATING_MODE_ASK stAsk;
	AHEADFORM_OPERATING_MODE_REPLY_SAMPLE stReply;
	int completionEventfd;
	int isReplied;
} OPERATINGMODE_CALLBACK_CONTEXT;

static void CopyOperatingModeReplyIfMatchesReq(const void *pElem, void *pUserArg)
{
	const AHEADFORM_OPERATING_MODE_REPLY_SAMPLE *pstReply = (const AHEADFORM_OPERATING_MODE_REPLY_SAMPLE*)pElem;
	OPERATINGMODE_CALLBACK_CONTEXT *pstContext = (OPERATINGMODE_CALLBACK_CONTEXT*)pUserArg;

	if (pstContext->isReplied || !AheadForm_MatchOperatingModeReplyWithReq(pstReply, &pstContext->stAsk))
		return;

	pstContext->stReply = *pstReply;
	pstContext->isReplied = 1;
	if (eventfd_write(pstContext->completionEventfd, 1) < 0 && errno != EAGAIN)
		PRINT_FUNCTION_ERRINFO(ERR_EVENTFD_IO_FAILED, __FUNCTION__, "call eventfd_write(completionEventfd = %d) failed: %s", pstContext->completionEventfd, strerror(errno));
}

int UserCmd_OperatingMode(AHEADFORM_ALL_CTRL *pstAheadForm, int argc, char *argv[])
{
	OPERATINGMODE_CALLBACK_CONTEXT stContext = {.isReplied = 0};
	AHEADFORM_TOPIC_READER_PARA stReaderPara = {CopyOperatingModeReplyIfMatchesReq, &stContext};
	AHEADFORM_TOPIC_READER *pstReader = NULL;
	unsigned int mode;
	long parsedMode;
	struct timeval timeout = {.tv_sec = REQUEST_REPLY_TIMEOUT_MS / 1000, .tv_usec = REQUEST_REPLY_TIMEOUT_MS % 1000 * 1000};
	fd_set rdFdSet;
	int max_fd, cleanupRet, ret;

	if ( (ret = IsUserInError(argc, argv, "2", "1")) != RET_SUCCESS )
		return ret;

	parsedMode = strtol(argv[1], NULL, 0);
	if (parsedMode != AHEADFORM_OPERATING_MODE_DEV && parsedMode != AHEADFORM_OPERATING_MODE_INTERACTIVE)
		return ERR_ARGV;

	mode = (unsigned int)parsedMode;

	if ( (ret = AheadForm_BuildOperatingModeRequest(pstAheadForm, mode, &stContext.stAsk)) != RET_SUCCESS )
		return ret;

	if ( (stContext.completionEventfd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC)) <= 0 )
	{
		printf("%s() call eventfd(OperatingMode completion) failed: %s\r\n", __FUNCTION__, strerror(errno));
		return ERR_EVENTFD_CREATE_FAILED;
	}

	if ( (pstReader = AheadForm_RegisterOperatingModeReader(pstAheadForm, &stReaderPara)) == NULL )
	{
		if (close(stContext.completionEventfd) < 0)
		{
			printf("%s() call close(stContext.completionEventfd = %d) failed: %s\r\n", __FUNCTION__, stContext.completionEventfd, strerror(errno));
			return ERR_EVENTFD_IO_FAILED;
		}

		return ERRCODE_INIT_FAIL;
	}

	if ( (ret = AheadForm_SendOperatingModeRequest(pstAheadForm, &stContext.stAsk)) != RET_SUCCESS )
	{
		if ( (cleanupRet = AheadForm_UnregisterOperatingModeReader(pstAheadForm, pstReader)) != RET_SUCCESS )
		{
			printf("%s() AheadForm_UnregisterOperatingModeReader(Reader = %p) failed, ret = 0x%08X; 退出 AheadFormSDK。\r\n", __FUNCTION__, (void*)pstReader, cleanupRet);
			PrintAllErrLog();
			exit(EXIT_FAILURE);
		}

		if (close(stContext.completionEventfd) < 0)
		{
			printf("%s() call close(stContext.completionEventfd = %d) failed: %s\r\n", __FUNCTION__, stContext.completionEventfd, strerror(errno));
			return ERR_EVENTFD_IO_FAILED;
		}

		return ret;
	}
	max_fd = stContext.completionEventfd;
	while (1)
	{
		FD_ZERO(&rdFdSet);
		FD_SET(stContext.completionEventfd, &rdFdSet);
		if ( (ret = select(max_fd + 1, &rdFdSet, NULL, NULL, &timeout)) < 0 )
		{
			printf("%s() call select(completionEventfd = %d) failed: %s\r\n", __FUNCTION__, stContext.completionEventfd, strerror(errno));
			ret = ERR_SELECT;
			break;
		}
		else if (ret == 0)
		{
			break;
		}
		ret = RET_SUCCESS;
		if (FD_ISSET(stContext.completionEventfd, &rdFdSet))
			break;

		if (timeout.tv_sec == 0 && timeout.tv_usec == 0)
			break;
	}

	//Unregister synchronizes with callbacks before this thread reads their output.
	if ( (cleanupRet = AheadForm_UnregisterOperatingModeReader(pstAheadForm, pstReader)) != RET_SUCCESS )
	{
		printf("%s() AheadForm_UnregisterOperatingModeReader(Reader = %p) failed, ret = 0x%08X; 退出 AheadFormSDK。\r\n", __FUNCTION__, (void*)pstReader, cleanupRet);
		PrintAllErrLog();
		exit(EXIT_FAILURE);
	}

	if (close(stContext.completionEventfd) < 0)
	{
		printf("%s() call close(stContext.completionEventfd = %d) failed: %s\r\n", __FUNCTION__, stContext.completionEventfd, strerror(errno));
		return ERR_EVENTFD_IO_FAILED;
	}

	if (ret != RET_SUCCESS)
		return ret;

	if (!stContext.isReplied)
		return ERR_REQUEST_TIMEOUT;

	printf("operating mode requestId=%llu, result=%u.\r\n", stContext.stReply.requestId, stContext.stReply.result);
	return stContext.stReply.result == AHEADFORM_OPERATING_MODE_RESULT_SUCCEEDED ? RET_SUCCESS : ERR_RX_FAILED;
}
#endif
