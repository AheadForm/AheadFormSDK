#include "UserCmdMenu.h"
#include "Time.h"

#include <ctype.h>
#include <limits.h>
#include <stdlib.h>
#include <strings.h>
#include <time.h>
#include <sys/eventfd.h>
#include <sys/select.h>
#include <sys/timerfd.h>
#include <unistd.h>

#define VOLUME_QUERY_TIMEOUT_MS 200
#define USER_CMD_REPLY_METHOD_ASYNC_REQUEST 1
#define USER_CMD_REPLY_METHOD_RX_EVENTFD 2
#define USER_CMD_REPLY_METHOD_READER_CALLBACK 3
#ifndef USER_CMD_REPLY_METHOD
#define USER_CMD_REPLY_METHOD USER_CMD_REPLY_METHOD_RX_EVENTFD
#endif

#if USER_CMD_REPLY_METHOD != USER_CMD_REPLY_METHOD_ASYNC_REQUEST && USER_CMD_REPLY_METHOD != USER_CMD_REPLY_METHOD_RX_EVENTFD && USER_CMD_REPLY_METHOD != USER_CMD_REPLY_METHOD_READER_CALLBACK
#error Invalid USER_CMD_REPLY_METHOD
#endif


static int PrintCmdMenu(AHEADFORM_ALL_CTRL *pstAheadForm, int argc, char *argv[]);
static int ShellCmd(AHEADFORM_ALL_CTRL *pstAheadForm, int argc, char *argv[]);
static int ErrLogPrint(AHEADFORM_ALL_CTRL *pstAheadForm, int argc, char *argv[]);
static int WatchVolume(AHEADFORM_ALL_CTRL *pstAheadForm, int argc, char *argv[]);
static int QueryVolume(AHEADFORM_ALL_CTRL *pstAheadForm, int argc, char *argv[]);
static int Exit(AHEADFORM_ALL_CTRL *pstAheadForm, int argc, char *argv[]);


const USERIN_TASK gUserInTaskAry[USERIN_CMD_CNT_MAX] =
{
	{"help", PrintCmdMenu, "print cmd menu"},
	{"shell", ShellCmd, "shell cmd   -- eg: shell ls -l"},
	{"errlog", ErrLogPrint, "errlog (storeFilePath)   -- printf or store errorlog from power on"},
	{"WatchVolume", WatchVolume, "WatchVolume seconds  \t-    eg: WatchVolume 10 means that query volume every second and print replies for 10s"},
	{"QueryVolume", QueryVolume, "query current volume once"},
	{"PresetMotion", UserCmd_PresetMotion, "PresetMotion action (1=SLEEP, 2=CENTER)"},
	{"AudioDevice", UserCmd_AudioDevice, "AudioDevice operation type [deviceId] (op: 1=route, 2=list, 3=current; type: 0=mic, 1=speaker)"},
	{"MediaChannel", UserCmd_MediaChannel, "MediaChannel video mic playback vad (0=keep, 1=disable, 2=enable)"},
	{"HardwareControl", UserCmd_HardwareControl, "HardwareControl action device (2=error log, 3=exit daemon; device: 1..5)"},
	{"SafetyControl", UserCmd_SafetyControl, "SafetyControl action [facePermit neckPermit] (1=permission, 2=safe stop, 3=clear fault, 4=estop)"},
	{"OperatingMode", UserCmd_OperatingMode, "OperatingMode mode (1=DEV, 2=INTERACTIVE)"},
	{"SendNeck", UserCmd_SendNeck, "SendNeck pitch_rad yaw_rad roll_rad"},
	{"SendServo", UserCmd_SendServo, "SendServo angle_deg_1 ... angle_deg_20"},
	{"MotionSample", UserCmd_MotionSample, "MotionSample file.csv t_us motion|face|inspect"},
	{"ReadStatus", UserCmd_ReadStatus, "ReadStatus topic latest|oldest|all; ReadStatus topic export path"},
	{"WatchStatus", UserCmd_WatchStatus, "WatchStatus hardware|events seconds"},
	{"WatchMedia", UserCmd_WatchMedia, "WatchMedia video|mic seconds"},
	{"SendWav", UserCmd_SendWav, "SendWav mono_16k_s16le.wav"},
	{"PlayScript", UserCmd_PlayScript, "PlayScript show.csv (same-basename MP3)"},
	{"StopScript", UserCmd_StopScript, "StopScript"},
	{"ScriptStatus", UserCmd_ScriptStatus, "ScriptStatus"},
	{"exit", Exit, "exit"},
	{"\0", NULL}
};

int PrintCmdResult(char *strCmd, int cmdRet)
{
	int ret = 0;

	STD_INFO_PRINT("\r\n");
	if ( (cmdRet == RET_SUCCESS) || (cmdRet == RET_EXIT) )
	{
		STD_INFO_PRINT("cmd[%s] succeed.\r\n", strCmd);
	}
	else if (cmdRet != RET_SKIPPED)
	{
		STD_INFO_PRINT("cmd[%s] error!\r\n", strCmd);
		PrintAllErrLog();
	}
	STD_INFO_PRINT("\r\n");
	return ret;
}

int IsDigitStr(char *strCheck)
{
	int i;

	if (strCheck == NULL || strCheck[0] == '\0')
		return 0;

	if (strncasecmp(strCheck, "0x", 2) == 0)
	{
		if (strCheck[2] == '\0')
			return 0;

		for (i = 2; strCheck[i] != '\0'; i++)
		{
			if (!isxdigit((unsigned char)strCheck[i]))
				return 0;
		}
	}
	else
	{
		if (strCheck[0] == '0' && strCheck[1] != '\0')
			return 0;

		for (i = 0; strCheck[i] != '\0'; i++)
		{
			if (!isdigit((unsigned char)strCheck[i]))
				return 0;
		}
	}

	return 1;
}

/*
	arg:
		needArgc == NULL    means not exist or don't check;
		needArgc == "3;4"   means that argc ought to be 3 or 4;

		digArgvPos == NULL  means not need digitial arg or don't check;
		digArgvPos == "1,3" means that  argv[1] and argv[3] ought to be digitial arg (digitial arg support all radix);
		digArgvPos == "1,3;1,2,3" means that "argv[1] and argv[3]" or "argv[1] and argv[2] and argv[3]" ought to be digtial arg;
		digArgvPos == " ;1,2,3" means that "no digtial arg" or "argv[1] and argv[2] and argv[3]" ought to be digtial arg;
		digArgvPos == "1,2,3; " means that "argv[1] and argv[2] and argv[3]" ought to be digtial arg or "no digtial arg";
		digArgvPos == "all" means that "argv[1] ~ argv[argc - 1]" all ought to be digitial arg.

	return: 
		0 means right; else return ERR_ARGC or ERR_ARGV
*/
int IsUserInError(int userInArgc, char *argv[], char *needArgc, char *digArgvPos)
{
	char strDigArgvPos[256] = {0}, *strVaildArgvPos = NULL;
	int i, n, usrInArgvId, argc, isArgcRight = 0, LeftDivSignCnt = 0;

	if (needArgc != NULL)
	{
		for (i = 0; needArgc[i] != 0; i++)
		{
			if ((needArgc[i] >= '0') && (needArgc[i] <= '9'))
			{	
				argc = needArgc[i] - '0';
				if (userInArgc == argc)
				{
					isArgcRight = 1;
					break;
				}
			}
		}
		if (!isArgcRight)
		{
			printf("IsUserInError return ERR_ARGC\r\n");
			return ERR_ARGC;
		}

		for (n = 0; n < i; n++)
		{
			if (needArgc[n] == ';')
				LeftDivSignCnt++;
		}

		if (digArgvPos)
		{
			strcpy(strDigArgvPos, digArgvPos);
			strVaildArgvPos = strtok(strDigArgvPos, ";");
			for (i = 0; i < LeftDivSignCnt; i++)
				strVaildArgvPos = strtok(NULL, ";");

			for (i = 0; strVaildArgvPos[i] != 0; i++)
			{
				if ((strVaildArgvPos[i] >= '0') && (strVaildArgvPos[i] <= '9'))
				{
					usrInArgvId = strVaildArgvPos[i] - '0';
					if (!(IsDigitStr(argv[usrInArgvId])))
						return ERR_ARGV;
				}
			}	
		}
	}
	else if (digArgvPos != NULL)//needArgc == NULL
	{
		if (strcasecmp(digArgvPos, "all") == 0)// all argv ought to be digitial arg
		{
			for (i = 1; i < userInArgc; i++)
			{
				if (!(IsDigitStr(argv[i])))
					return ERR_ARGV;
			}
		}
		else
		{
			strVaildArgvPos = digArgvPos;
			for (i = 0; strVaildArgvPos[i] != 0; i++)
			{
				if ((strVaildArgvPos[i] >= '0') && (strVaildArgvPos[i] <= '9'))
				{
					usrInArgvId = strVaildArgvPos[i] - '0';
					if (!(IsDigitStr(argv[usrInArgvId])))
						return ERR_ARGV;
				}
			}	
		}
	}

	return RET_SUCCESS;//userIn is right
}

//Cmd Task function...................................................................................................................
//
static int PrintCmdMenu(AHEADFORM_ALL_CTRL *pstAheadForm, int argc, char *argv[])
{
	int i;

	if (argc != 1)
		return ERR_ARGC;

	for (i = 0; gUserInTaskAry[i].pFunction != NULL; i++)
		STD_INFO_PRINT("\t%-16s:%s\r\n", gUserInTaskAry[i].strCmd, gUserInTaskAry[i].strDescription);

	return RET_SUCCESS;
}

static int ShellCmd(AHEADFORM_ALL_CTRL *pstAheadForm, int argc, char *argv[])
{
	int i, cmdLen = 0, argLen = 0, remain = 0;
	char shCmd[1024] = {0};

	if ((argc < 1) || (argc > 12))
		return ERR_ARGC;

	for (i = 1; i < argc; i++)
	{
		remain = sizeof(shCmd) - cmdLen;
		argLen = snprintf(shCmd + cmdLen, remain, "%s ", argv[i]);
		if ((argLen < 0) || (argLen >= remain))
			return ERR_OVER_BUF;
		cmdLen += argLen;
	}
	system(shCmd);
    return RET_SUCCESS;
}


static int ErrLogPrint(AHEADFORM_ALL_CTRL *pstAheadForm, int argc, char *argv[])
{
	int ret = RET_SUCCESS;

	if ( (ret = IsUserInError(argc, argv, "1; 2", NULL)) != RET_SUCCESS )
		return ret;

	if (argc == 1)
		return PrintAllErrLog();
	else
		return StoreAllErrLogToFile(argv[1]);
}

#if USER_CMD_REPLY_METHOD == USER_CMD_REPLY_METHOD_ASYNC_REQUEST //timerfd + async request notifyEventfd
static int WatchVolume(AHEADFORM_ALL_CTRL *pstAheadForm, int argc, char *argv[])
{
	AHEADFORM_VOLUME_REPLY_SAMPLE stReply;
	fd_set rdFdSet;
	eventfd_t eventCnt;
	unsigned long long timerOverCnt, elapsedPeriodCnt = 0;
	long seconds;
	int notifyEventfd, timer_fd, max_fd, cleanupRet, ret;

	if ( (ret = IsUserInError(argc, argv, "2", "1")) != RET_SUCCESS )
		return ret;

	seconds = strtol(argv[1], NULL, 0);
	if (seconds <= 0 || seconds > INT_MAX)
		return ERR_ARGV;

	if ( (notifyEventfd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC)) <= 0 )
	{
		printf("%s() call eventfd(volume completion) failed: %s\r\n", __FUNCTION__, strerror(errno));
		return ERR_EVENTFD_CREATE_FAILED;
	}

	if ( (timer_fd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC)) <= 0 )
	{
		printf("%s() call timerfd_create(volume query timer) failed: %s\r\n", __FUNCTION__, strerror(errno));
		if (close(notifyEventfd) < 0)
		{
			printf("%s() call close(notifyEventfd = %d) failed: %s\r\n", __FUNCTION__, notifyEventfd, strerror(errno));
			return ERR_EVENTFD_IO_FAILED;
		}

		return ERR_CLOCK;
	}

	if ( (ret = SetTimerPeriod_ms(timer_fd, 1000)) != RET_SUCCESS )
	{
		if (close(timer_fd) < 0)
		{
			printf("%s() call close(timer_fd = %d) failed: %s\r\n", __FUNCTION__, timer_fd, strerror(errno));
			ret = ERR_CLOCK;
		}

		if (close(notifyEventfd) < 0)
		{
			printf("%s() call close(notifyEventfd = %d) failed: %s\r\n", __FUNCTION__, notifyEventfd, strerror(errno));
			ret = ERR_EVENTFD_IO_FAILED;
		}

		return ret;
	}

	if ( (ret = Media_QueryVolumeAsync(&pstAheadForm->stRobotCtrl.stMediaCtrl, VOLUME_QUERY_TIMEOUT_MS * 1000, notifyEventfd, &stReply)) != RET_SUCCESS )
	{
		if ( (cleanupRet = Media_CancelVolumeAsyncRequest(&pstAheadForm->stRobotCtrl.stMediaCtrl, notifyEventfd)) != RET_SUCCESS )
		{
			printf("%s() Media_CancelVolumeAsyncRequest(notifyEventfd = %d) failed, ret = 0x%08X; 退出 AheadFormSDK。\r\n", __FUNCTION__, notifyEventfd, cleanupRet);
			PrintAllErrLog();
			exit(EXIT_FAILURE);
		}

		if (close(timer_fd) < 0)
		{
			printf("%s() call close(timer_fd = %d) failed: %s\r\n", __FUNCTION__, timer_fd, strerror(errno));
			ret = ERR_CLOCK;
		}

		if (close(notifyEventfd) < 0)
		{
			printf("%s() call close(notifyEventfd = %d) failed: %s\r\n", __FUNCTION__, notifyEventfd, strerror(errno));
			ret = ERR_EVENTFD_IO_FAILED;
		}

		return ret;
	}
	max_fd = timer_fd;
	while (1)
	{
		FD_ZERO(&rdFdSet);
		FD_SET(timer_fd, &rdFdSet);
		FD_SET(notifyEventfd, &rdFdSet);
		//Add other application fds here; only timer readiness schedules another request.
		if (select(max_fd + 1, &rdFdSet, NULL, NULL, NULL) < 0)
		{
			printf("%s() call select(timer_fd = %d, notifyEventfd = %d) failed: %s\r\n", __FUNCTION__, timer_fd, notifyEventfd, strerror(errno));
			ret = ERR_SELECT;
			break;
		}

		if (FD_ISSET(timer_fd, &rdFdSet))
		{
			if (read(timer_fd, &timerOverCnt, sizeof(timerOverCnt)) < 0)
			{
				printf("%s() call read(timer_fd = %d) failed: %s\r\n", __FUNCTION__, timer_fd, strerror(errno));
				ret = ERR_CLOCK;
				break;
			}
		}

		if (FD_ISSET(notifyEventfd, &rdFdSet) || FD_ISSET(timer_fd, &rdFdSet))
		{
			ret = Media_CheckIsVolumeAsyncRequestReplied(&pstAheadForm->stRobotCtrl.stMediaCtrl, notifyEventfd);
			if (ret == 0)
			{
				if ( (cleanupRet = Media_CancelVolumeAsyncRequest(&pstAheadForm->stRobotCtrl.stMediaCtrl, notifyEventfd)) != RET_SUCCESS )
				{
					printf("%s() Media_CancelVolumeAsyncRequest(notifyEventfd = %d) failed, ret = 0x%08X; 退出 AheadFormSDK。\r\n", __FUNCTION__, notifyEventfd, cleanupRet);
					PrintAllErrLog();
					exit(EXIT_FAILURE);
				}
			}
			else if (ret != 1 && ret != ERRCODE_REQUEST_TIMEOUT)
			{
				break;
			}

			//A successful Cancel permits checking completion that won the race before cancellation.
			if (eventfd_read(notifyEventfd, &eventCnt) == 0)
			{
				printf("source=%s, askId=%llu, operation=%u, ret=%d, queryValid=%u.\r\n", stReply.sourceName, stReply.askId, stReply.operation, stReply.retCode, stReply.queryDataValid);
				printf("volume receive=%llu us, mic=%.1f%%, speaker=%.1f%%.\r\n", stReply.ReceiveTime_us, stReply.stVolume.micVolumePercent, stReply.stVolume.spkVolumePercent);
			}
			else if (errno != EAGAIN)
			{
				printf("%s() call eventfd_read(notifyEventfd = %d) failed: %s\r\n", __FUNCTION__, notifyEventfd, strerror(errno));
				ret = ERR_EVENTFD_IO_FAILED;
				break;
			}
			ret = RET_SUCCESS;
		}

		if (FD_ISSET(timer_fd, &rdFdSet))
		{
			elapsedPeriodCnt += timerOverCnt;
			if (elapsedPeriodCnt >= (unsigned long long)seconds)
				break;

			if ( (ret = Media_QueryVolumeAsync(&pstAheadForm->stRobotCtrl.stMediaCtrl, VOLUME_QUERY_TIMEOUT_MS * 1000, notifyEventfd, &stReply)) != RET_SUCCESS )
				break;
		}
	}

	if ( (cleanupRet = Media_CancelVolumeAsyncRequest(&pstAheadForm->stRobotCtrl.stMediaCtrl, notifyEventfd)) != RET_SUCCESS )
	{
		printf("%s() Media_CancelVolumeAsyncRequest(notifyEventfd = %d) failed, ret = 0x%08X; 退出 AheadFormSDK。\r\n", __FUNCTION__, notifyEventfd, cleanupRet);
		PrintAllErrLog();
		exit(EXIT_FAILURE);
	}

	if (close(timer_fd) < 0)
	{
		printf("%s() call close(timer_fd = %d) failed: %s\r\n", __FUNCTION__, timer_fd, strerror(errno));
		ret = ERR_CLOCK;
	}

	if (close(notifyEventfd) < 0)
	{
		printf("%s() call close(notifyEventfd = %d) failed: %s\r\n", __FUNCTION__, notifyEventfd, strerror(errno));
		ret = ERR_EVENTFD_IO_FAILED;
	}

	return ret;
}

#elif USER_CMD_REPLY_METHOD == USER_CMD_REPLY_METHOD_RX_EVENTFD //timerfd + rxDataUpdateEventfd
static int WatchVolume(AHEADFORM_ALL_CTRL *pstAheadForm, int argc, char *argv[])
{
	AHEADFORM_TOPIC_READER_PARA stReaderPara = {0};
	AHEADFORM_TOPIC_READER *pstReader = NULL;
	AHEADFORM_VOLUME_ASK stAsk;
	AHEADFORM_VOLUME_REPLY_SAMPLE stReply;
	fd_set rdFdSet;
	eventfd_t eventCnt;
	unsigned long long timerOverCnt, elapsedPeriodCnt = 0;
	unsigned int i, readableElemCnt;
	long seconds;
	int timer_fd, max_fd, cleanupRet, ret;

	if ( (ret = IsUserInError(argc, argv, "2", "1")) != RET_SUCCESS )
		return ret;

	seconds = strtol(argv[1], NULL, 0);
	if (seconds <= 0 || seconds > INT_MAX)
		return ERR_ARGV;

	if ( (ret = Media_BuildQueryVolumeRequest(&pstAheadForm->stRobotCtrl.stMediaCtrl, &stAsk)) != RET_SUCCESS )
		return ret;

	if ( (pstReader = Media_RegisterVolumeReader(&pstAheadForm->stRobotCtrl.stMediaCtrl, &stReaderPara)) == NULL )
		return ERRCODE_INIT_FAIL;

	if ( (timer_fd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC)) <= 0 )
	{
		printf("%s() call timerfd_create(volume query timer) failed: %s\r\n", __FUNCTION__, strerror(errno));
		if ( (cleanupRet = Media_UnregisterVolumeReader(&pstAheadForm->stRobotCtrl.stMediaCtrl, pstReader)) != RET_SUCCESS )
		{
			printf("%s() Media_UnregisterVolumeReader(Reader = %p) failed, ret = 0x%08X; 退出 AheadFormSDK。\r\n", __FUNCTION__, (void*)pstReader, cleanupRet);
			PrintAllErrLog();
			exit(EXIT_FAILURE);
		}

		return ERR_CLOCK;
	}

	if ( (ret = SetTimerPeriod_ms(timer_fd, 1000)) != RET_SUCCESS )
	{
		if ( (cleanupRet = Media_UnregisterVolumeReader(&pstAheadForm->stRobotCtrl.stMediaCtrl, pstReader)) != RET_SUCCESS )
		{
			printf("%s() Media_UnregisterVolumeReader(Reader = %p) failed, ret = 0x%08X; 退出 AheadFormSDK。\r\n", __FUNCTION__, (void*)pstReader, cleanupRet);
			PrintAllErrLog();
			exit(EXIT_FAILURE);
		}

		if (close(timer_fd) < 0)
		{
			printf("%s() call close(timer_fd = %d) failed: %s\r\n", __FUNCTION__, timer_fd, strerror(errno));
			ret = ERR_CLOCK;
		}

		return ret;
	}

	if ( (ret = Media_SendVolumeRequest(&pstAheadForm->stRobotCtrl.stMediaCtrl, &stAsk)) != RET_SUCCESS )
	{
		if ( (cleanupRet = Media_UnregisterVolumeReader(&pstAheadForm->stRobotCtrl.stMediaCtrl, pstReader)) != RET_SUCCESS )
		{
			printf("%s() Media_UnregisterVolumeReader(Reader = %p) failed, ret = 0x%08X; 退出 AheadFormSDK。\r\n", __FUNCTION__, (void*)pstReader, cleanupRet);
			PrintAllErrLog();
			exit(EXIT_FAILURE);
		}

		if (close(timer_fd) < 0)
		{
			printf("%s() call close(timer_fd = %d) failed: %s\r\n", __FUNCTION__, timer_fd, strerror(errno));
			ret = ERR_CLOCK;
		}

		return ret;
	}
	max_fd = timer_fd;
	while (1)
	{
		FD_ZERO(&rdFdSet);
		FD_SET(timer_fd, &rdFdSet);
		FD_SET(pstReader->rxDataUpdateEventfd, &rdFdSet);
		//Add other application fds here; only timer readiness schedules another request.
		if (select(max_fd + 1, &rdFdSet, NULL, NULL, NULL) < 0)
		{
			printf("%s() call select(timer_fd = %d, rxDataUpdateEventfd = %d) failed: %s\r\n", __FUNCTION__, timer_fd, pstReader->rxDataUpdateEventfd, strerror(errno));
			ret = ERR_SELECT;
			break;
		}

		if (FD_ISSET(pstReader->rxDataUpdateEventfd, &rdFdSet) || FD_ISSET(timer_fd, &rdFdSet))
		{
			if (eventfd_read(pstReader->rxDataUpdateEventfd, &eventCnt) < 0 && errno != EAGAIN)
			{
				printf("%s() call eventfd_read(rxDataUpdateEventfd = %d) failed: %s\r\n", __FUNCTION__, pstReader->rxDataUpdateEventfd, strerror(errno));
				ret = ERR_EVENTFD_IO_FAILED;
				break;
			}
			readableElemCnt = TopicReader_GetReadableElemCnt(pstReader);
			for (i = 0; i < readableElemCnt; i++)
			{
				if ( (ret = TopicReader_PopElem(pstReader, &stReply)) <= 0 )
					break;

				printf("source=%s, askId=%llu, operation=%u, ret=%d, queryValid=%u.\r\n", stReply.sourceName, stReply.askId, stReply.operation, stReply.retCode, stReply.queryDataValid);
				printf("volume receive=%llu us, mic=%.1f%%, speaker=%.1f%%.\r\n", stReply.ReceiveTime_us, stReply.stVolume.micVolumePercent, stReply.stVolume.spkVolumePercent);
			}

			if (ret < 0)
				break;

			ret = RET_SUCCESS;
		}

		if (FD_ISSET(timer_fd, &rdFdSet))
		{
			if (read(timer_fd, &timerOverCnt, sizeof(timerOverCnt)) < 0)
			{
				printf("%s() call read(timer_fd = %d) failed: %s\r\n", __FUNCTION__, timer_fd, strerror(errno));
				ret = ERR_CLOCK;
				break;
			}
			elapsedPeriodCnt += timerOverCnt;
			if (elapsedPeriodCnt >= (unsigned long long)seconds)
				break;

			if ( (ret = Media_BuildQueryVolumeRequest(&pstAheadForm->stRobotCtrl.stMediaCtrl, &stAsk)) != RET_SUCCESS )
				break;

			if ( (ret = Media_SendVolumeRequest(&pstAheadForm->stRobotCtrl.stMediaCtrl, &stAsk)) != RET_SUCCESS )
				break;
		}
	}

	if ( (cleanupRet = Media_UnregisterVolumeReader(&pstAheadForm->stRobotCtrl.stMediaCtrl, pstReader)) != RET_SUCCESS )
	{
		printf("%s() Media_UnregisterVolumeReader(Reader = %p) failed, ret = 0x%08X; 退出 AheadFormSDK。\r\n", __FUNCTION__, (void*)pstReader, cleanupRet);
		PrintAllErrLog();
		exit(EXIT_FAILURE);
	}

	if (close(timer_fd) < 0)
	{
		printf("%s() call close(timer_fd = %d) failed: %s\r\n", __FUNCTION__, timer_fd, strerror(errno));
		ret = ERR_CLOCK;
	}

	return ret;
}

#else //timerfd + Reader callback
static void PrintVolumeSample(const void *pElem, void *pUserArg)
{
	const AHEADFORM_VOLUME_REPLY_SAMPLE *pstSample = (const AHEADFORM_VOLUME_REPLY_SAMPLE*)pElem;

	printf("source=%s, askId=%llu, operation=%u, ret=%d, queryValid=%u.\r\n", pstSample->sourceName, pstSample->askId, pstSample->operation, pstSample->retCode, pstSample->queryDataValid);
	printf("volume receive=%llu us, mic=%.1f%%, speaker=%.1f%%.\r\n", pstSample->ReceiveTime_us, pstSample->stVolume.micVolumePercent, pstSample->stVolume.spkVolumePercent);
}

static int WatchVolume(AHEADFORM_ALL_CTRL *pstAheadForm, int argc, char *argv[])
{
	AHEADFORM_TOPIC_READER_PARA stReaderPara = {PrintVolumeSample, NULL};
	AHEADFORM_TOPIC_READER *pstReader = NULL;
	AHEADFORM_VOLUME_ASK stAsk;
	fd_set rdFdSet;
	unsigned long long timerOverCnt, elapsedPeriodCnt = 0;
	long seconds;
	int timer_fd, max_fd, cleanupRet, ret;

	if ( (ret = IsUserInError(argc, argv, "2", "1")) != RET_SUCCESS )
		return ret;

	seconds = strtol(argv[1], NULL, 0);
	if (seconds <= 0 || seconds > INT_MAX)
		return ERR_ARGV;

	if ( (ret = Media_BuildQueryVolumeRequest(&pstAheadForm->stRobotCtrl.stMediaCtrl, &stAsk)) != RET_SUCCESS )
		return ret;

	if ( (pstReader = Media_RegisterVolumeReader(&pstAheadForm->stRobotCtrl.stMediaCtrl, &stReaderPara)) == NULL )
		return ERRCODE_INIT_FAIL;

	if ( (timer_fd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC)) <= 0 )
	{
		printf("%s() call timerfd_create(volume query timer) failed: %s\r\n", __FUNCTION__, strerror(errno));
		if ( (cleanupRet = Media_UnregisterVolumeReader(&pstAheadForm->stRobotCtrl.stMediaCtrl, pstReader)) != RET_SUCCESS )
		{
			printf("%s() Media_UnregisterVolumeReader(Reader = %p) failed, ret = 0x%08X; 退出 AheadFormSDK。\r\n", __FUNCTION__, (void*)pstReader, cleanupRet);
			PrintAllErrLog();
			exit(EXIT_FAILURE);
		}

		return ERR_CLOCK;
	}

	if ( (ret = SetTimerPeriod_ms(timer_fd, 1000)) != RET_SUCCESS )
	{
		if ( (cleanupRet = Media_UnregisterVolumeReader(&pstAheadForm->stRobotCtrl.stMediaCtrl, pstReader)) != RET_SUCCESS )
		{
			printf("%s() Media_UnregisterVolumeReader(Reader = %p) failed, ret = 0x%08X; 退出 AheadFormSDK。\r\n", __FUNCTION__, (void*)pstReader, cleanupRet);
			PrintAllErrLog();
			exit(EXIT_FAILURE);
		}

		if (close(timer_fd) < 0)
		{
			printf("%s() call close(timer_fd = %d) failed: %s\r\n", __FUNCTION__, timer_fd, strerror(errno));
			ret = ERR_CLOCK;
		}

		return ret;
	}

	if ( (ret = Media_SendVolumeRequest(&pstAheadForm->stRobotCtrl.stMediaCtrl, &stAsk)) != RET_SUCCESS )
	{
		if ( (cleanupRet = Media_UnregisterVolumeReader(&pstAheadForm->stRobotCtrl.stMediaCtrl, pstReader)) != RET_SUCCESS )
		{
			printf("%s() Media_UnregisterVolumeReader(Reader = %p) failed, ret = 0x%08X; 退出 AheadFormSDK。\r\n", __FUNCTION__, (void*)pstReader, cleanupRet);
			PrintAllErrLog();
			exit(EXIT_FAILURE);
		}

		if (close(timer_fd) < 0)
		{
			printf("%s() call close(timer_fd = %d) failed: %s\r\n", __FUNCTION__, timer_fd, strerror(errno));
			ret = ERR_CLOCK;
		}

		return ret;
	}
	max_fd = timer_fd;
	while (1)
	{
		FD_ZERO(&rdFdSet);
		FD_SET(timer_fd, &rdFdSet);
		//Add other application fds here; only timer readiness schedules another request.
		if (select(max_fd + 1, &rdFdSet, NULL, NULL, NULL) < 0)
		{
			printf("%s() call select(timer_fd = %d) failed: %s\r\n", __FUNCTION__, timer_fd, strerror(errno));
			ret = ERR_SELECT;
			break;
		}

		if (FD_ISSET(timer_fd, &rdFdSet))
		{
			if (read(timer_fd, &timerOverCnt, sizeof(timerOverCnt)) < 0)
			{
				printf("%s() call read(timer_fd = %d) failed: %s\r\n", __FUNCTION__, timer_fd, strerror(errno));
				ret = ERR_CLOCK;
				break;
			}
			elapsedPeriodCnt += timerOverCnt;
			if (elapsedPeriodCnt >= (unsigned long long)seconds)
				break;

			if ( (ret = Media_BuildQueryVolumeRequest(&pstAheadForm->stRobotCtrl.stMediaCtrl, &stAsk)) != RET_SUCCESS )
				break;

			if ( (ret = Media_SendVolumeRequest(&pstAheadForm->stRobotCtrl.stMediaCtrl, &stAsk)) != RET_SUCCESS )
				break;
		}
	}

	if ( (cleanupRet = Media_UnregisterVolumeReader(&pstAheadForm->stRobotCtrl.stMediaCtrl, pstReader)) != RET_SUCCESS )
	{
		printf("%s() Media_UnregisterVolumeReader(Reader = %p) failed, ret = 0x%08X; 退出 AheadFormSDK。\r\n", __FUNCTION__, (void*)pstReader, cleanupRet);
		PrintAllErrLog();
		exit(EXIT_FAILURE);
	}

	if (close(timer_fd) < 0)
	{
		printf("%s() call close(timer_fd = %d) failed: %s\r\n", __FUNCTION__, timer_fd, strerror(errno));
		ret = ERR_CLOCK;
	}

	return ret;
}
#endif

#if USER_CMD_REPLY_METHOD == USER_CMD_REPLY_METHOD_ASYNC_REQUEST //query volume by async request
static int QueryVolume(AHEADFORM_ALL_CTRL *pstAheadForm, int argc, char *argv[])
{
	AHEADFORM_VOLUME_REPLY_SAMPLE stReply;
	AHEADFORM_VOLUME stVolume;
	struct timeval timeout = {.tv_sec = VOLUME_QUERY_TIMEOUT_MS / 1000, .tv_usec = VOLUME_QUERY_TIMEOUT_MS % 1000 * 1000};
	fd_set rdFdSet;
	eventfd_t eventCnt;
	int notifyEventfd, max_fd, cleanupRet, ret;

	if ( (ret = IsUserInError(argc, argv, "1", NULL)) != RET_SUCCESS )
		return ret;

	if ( (notifyEventfd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC)) <= 0 )
	{
		printf("%s() call eventfd(volume completion) failed: %s\r\n", __FUNCTION__, strerror(errno));
		return ERR_EVENTFD_CREATE_FAILED;
	}

	if ( (ret = Media_QueryVolumeAsync(&pstAheadForm->stRobotCtrl.stMediaCtrl, VOLUME_QUERY_TIMEOUT_MS * 1000, notifyEventfd, &stReply)) != RET_SUCCESS )
	{
		if ( (cleanupRet = Media_CancelVolumeAsyncRequest(&pstAheadForm->stRobotCtrl.stMediaCtrl, notifyEventfd)) != RET_SUCCESS )
		{
			printf("%s() Media_CancelVolumeAsyncRequest(notifyEventfd = %d) failed, ret = 0x%08X; 退出 AheadFormSDK。\r\n", __FUNCTION__, notifyEventfd, cleanupRet);
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
			ret = Media_CheckIsVolumeAsyncRequestReplied(&pstAheadForm->stRobotCtrl.stMediaCtrl, notifyEventfd);
			break;
		}

		if (timeout.tv_sec == 0 && timeout.tv_usec == 0)
			break;
	}

	if ( (cleanupRet = Media_CancelVolumeAsyncRequest(&pstAheadForm->stRobotCtrl.stMediaCtrl, notifyEventfd)) != RET_SUCCESS )
	{
		printf("%s() Media_CancelVolumeAsyncRequest(notifyEventfd = %d) failed, ret = 0x%08X; 退出 AheadFormSDK。\r\n", __FUNCTION__, notifyEventfd, cleanupRet);
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

	if ( (ret = Media_GetVolumeFromReply(&stReply, &stVolume)) != RET_SUCCESS )
		return ret;

	printf("mic volume = %.1f%%, speaker volume = %.1f%%.\r\n", stVolume.micVolumePercent, stVolume.spkVolumePercent);
	return RET_SUCCESS;
}

#elif USER_CMD_REPLY_METHOD == USER_CMD_REPLY_METHOD_RX_EVENTFD //query volume by rxDataUpdateEventfd
static int QueryVolume(AHEADFORM_ALL_CTRL *pstAheadForm, int argc, char *argv[])
{
	AHEADFORM_TOPIC_READER_PARA stReaderPara = {0};
	AHEADFORM_TOPIC_READER *pstReader = NULL;
	AHEADFORM_VOLUME_ASK stAsk;
	AHEADFORM_VOLUME_REPLY_SAMPLE stReply;
	AHEADFORM_VOLUME stVolume;
	struct timeval timeout = {.tv_sec = VOLUME_QUERY_TIMEOUT_MS / 1000, .tv_usec = VOLUME_QUERY_TIMEOUT_MS % 1000 * 1000};
	fd_set rdFdSet;
	eventfd_t eventCnt;
	unsigned int i, readableElemCnt;
	int max_fd, cleanupRet, ret;

	if ( (ret = IsUserInError(argc, argv, "1", NULL)) != RET_SUCCESS )
		return ret;

	if ( (ret = Media_BuildQueryVolumeRequest(&pstAheadForm->stRobotCtrl.stMediaCtrl, &stAsk)) != RET_SUCCESS )
		return ret;

	if ( (pstReader = Media_RegisterVolumeReader(&pstAheadForm->stRobotCtrl.stMediaCtrl, &stReaderPara)) == NULL )
		return ERRCODE_INIT_FAIL;

	if ( (ret = Media_SendVolumeRequest(&pstAheadForm->stRobotCtrl.stMediaCtrl, &stAsk)) != RET_SUCCESS )
	{
		if ( (cleanupRet = Media_UnregisterVolumeReader(&pstAheadForm->stRobotCtrl.stMediaCtrl, pstReader)) != RET_SUCCESS )
		{
			printf("%s() Media_UnregisterVolumeReader(Reader = %p) failed, ret = 0x%08X; 退出 AheadFormSDK。\r\n", __FUNCTION__, (void*)pstReader, cleanupRet);
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

				if (Media_MatchVolumeReplyWithReq(&stReply, &stAsk))
					break;

				ret = 0;
			}
		}

		if (ret != 0)
			break;

		if (timeout.tv_sec == 0 && timeout.tv_usec == 0)
			break;
	}

	if ( (cleanupRet = Media_UnregisterVolumeReader(&pstAheadForm->stRobotCtrl.stMediaCtrl, pstReader)) != RET_SUCCESS )
	{
		printf("%s() Media_UnregisterVolumeReader(Reader = %p) failed, ret = 0x%08X; 退出 AheadFormSDK。\r\n", __FUNCTION__, (void*)pstReader, cleanupRet);
		PrintAllErrLog();
		exit(EXIT_FAILURE);
	}

	if (ret == 0)
		return ERR_REQUEST_TIMEOUT;

	if (ret < 0)
		return ret;

	if ( (ret = Media_GetVolumeFromReply(&stReply, &stVolume)) != RET_SUCCESS )
		return ret;

	printf("mic volume = %.1f%%, speaker volume = %.1f%%.\r\n", stVolume.micVolumePercent, stVolume.spkVolumePercent);
	return RET_SUCCESS;
}

#else //query volume by Reader callback
typedef struct
{
	AHEADFORM_VOLUME_ASK stAsk;
	AHEADFORM_VOLUME_REPLY_SAMPLE stReply;
	int completionEventfd;
	int isReplied;
} VOLUME_QUERY_CALLBACK_CONTEXT;

static void CopyVolumeReplyIfMatchesReq(const void *pElem, void *pUserArg)
{
	const AHEADFORM_VOLUME_REPLY_SAMPLE *pstReply = (const AHEADFORM_VOLUME_REPLY_SAMPLE*)pElem;
	VOLUME_QUERY_CALLBACK_CONTEXT *pstContext = (VOLUME_QUERY_CALLBACK_CONTEXT*)pUserArg;

	if (pstContext->isReplied || !Media_MatchVolumeReplyWithReq(pstReply, &pstContext->stAsk))
		return;

	pstContext->stReply = *pstReply;
	pstContext->isReplied = 1;
	if (eventfd_write(pstContext->completionEventfd, 1) < 0 && errno != EAGAIN)
		PRINT_FUNCTION_ERRINFO(ERR_EVENTFD_IO_FAILED, __FUNCTION__, "call eventfd_write(completionEventfd = %d) failed: %s", pstContext->completionEventfd, strerror(errno));
}

static int QueryVolume(AHEADFORM_ALL_CTRL *pstAheadForm, int argc, char *argv[])
{
	VOLUME_QUERY_CALLBACK_CONTEXT stContext = {.isReplied = 0};
	AHEADFORM_TOPIC_READER_PARA stReaderPara = {CopyVolumeReplyIfMatchesReq, &stContext};
	AHEADFORM_TOPIC_READER *pstReader = NULL;
	AHEADFORM_VOLUME stVolume;
	struct timeval timeout = {.tv_sec = VOLUME_QUERY_TIMEOUT_MS / 1000, .tv_usec = VOLUME_QUERY_TIMEOUT_MS % 1000 * 1000};
	fd_set rdFdSet;
	int max_fd, cleanupRet, ret;

	if ( (ret = IsUserInError(argc, argv, "1", NULL)) != RET_SUCCESS )
		return ret;

	if ( (ret = Media_BuildQueryVolumeRequest(&pstAheadForm->stRobotCtrl.stMediaCtrl, &stContext.stAsk)) != RET_SUCCESS )
		return ret;

	if ( (stContext.completionEventfd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC)) <= 0 )
	{
		printf("%s() call eventfd(volume completion) failed: %s\r\n", __FUNCTION__, strerror(errno));
		return ERR_EVENTFD_CREATE_FAILED;
	}

	if ( (pstReader = Media_RegisterVolumeReader(&pstAheadForm->stRobotCtrl.stMediaCtrl, &stReaderPara)) == NULL )
	{
		if (close(stContext.completionEventfd) < 0)
		{
			printf("%s() call close(stContext.completionEventfd = %d) failed: %s\r\n", __FUNCTION__, stContext.completionEventfd, strerror(errno));
			return ERR_EVENTFD_IO_FAILED;
		}

		return ERRCODE_INIT_FAIL;
	}

	if ( (ret = Media_SendVolumeRequest(&pstAheadForm->stRobotCtrl.stMediaCtrl, &stContext.stAsk)) != RET_SUCCESS )
	{
		if ( (cleanupRet = Media_UnregisterVolumeReader(&pstAheadForm->stRobotCtrl.stMediaCtrl, pstReader)) != RET_SUCCESS )
		{
			printf("%s() Media_UnregisterVolumeReader(Reader = %p) failed, ret = 0x%08X; 退出 AheadFormSDK。\r\n", __FUNCTION__, (void*)pstReader, cleanupRet);
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
	if ( (cleanupRet = Media_UnregisterVolumeReader(&pstAheadForm->stRobotCtrl.stMediaCtrl, pstReader)) != RET_SUCCESS )
	{
		printf("%s() Media_UnregisterVolumeReader(Reader = %p) failed, ret = 0x%08X; 退出 AheadFormSDK。\r\n", __FUNCTION__, (void*)pstReader, cleanupRet);
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

	if ( (ret = Media_GetVolumeFromReply(&stContext.stReply, &stVolume)) != RET_SUCCESS )
		return ret;

	printf("mic volume = %.1f%%, speaker volume = %.1f%%.\r\n", stVolume.micVolumePercent, stVolume.spkVolumePercent);
	return RET_SUCCESS;
}
#endif

static int Exit(AHEADFORM_ALL_CTRL *pstAheadForm, int argc, char *argv[])
{
	if (argc != 1)
		return ERR_ARGC;

	return RET_EXIT;
}
