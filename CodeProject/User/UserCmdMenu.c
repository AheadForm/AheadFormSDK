#include "UserCmdMenu.h"
#include "Time.h"

#include <ctype.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <strings.h>
#include <time.h>
#include <sys/eventfd.h>
#include <sys/select.h>
#include <sys/timerfd.h>
#include <unistd.h>

#define VOLUME_QUERY_TIMEOUT_MS 200
#define CTRL_REQUEST_TIMEOUT_MS 3000
#define USER_CMD_REPLY_METHOD_RX_EVENTFD 1
#define USER_CMD_REPLY_METHOD_ASYNC_REQUEST 2
#define USER_CMD_REPLY_METHOD_READER_CALLBACK 3
#ifndef USER_CMD_REPLY_METHOD
#define USER_CMD_REPLY_METHOD USER_CMD_REPLY_METHOD_RX_EVENTFD
#endif

#if USER_CMD_REPLY_METHOD != USER_CMD_REPLY_METHOD_RX_EVENTFD && USER_CMD_REPLY_METHOD != USER_CMD_REPLY_METHOD_ASYNC_REQUEST && USER_CMD_REPLY_METHOD != USER_CMD_REPLY_METHOD_READER_CALLBACK
#error Invalid USER_CMD_REPLY_METHOD
#endif


static int PlaybackScriptOfUser(AHEADFORM_HEAD_CTRL *pstHeadCtrl, AHEADFORM_AUDIO_CTRL *pstAudioCtrl, const char *csvFilePath, const char *audioFilePath);

static int PrintCmdMenu(AHEADFORM_ALL_CTRL *pstAheadForm, int argc, char *argv[]);
static int ShellCmd(AHEADFORM_ALL_CTRL *pstAheadForm, int argc, char *argv[]);
static int ErrLogPrint(AHEADFORM_ALL_CTRL *pstAheadForm, int argc, char *argv[]);
static int WatchVolume(AHEADFORM_ALL_CTRL *pstAheadForm, int argc, char *argv[]);
static int QueryVolume(AHEADFORM_ALL_CTRL *pstAheadForm, int argc, char *argv[]);
static int AudioDevice(AHEADFORM_ALL_CTRL *pstAheadForm, int argc, char *argv[]);
static int OperatingMode(AHEADFORM_ALL_CTRL *pstAheadForm, int argc, char *argv[]);
static int EmotePlayback(AHEADFORM_ALL_CTRL *pstAheadForm, int argc, char *argv[]);
static int EmotePlayCtrl(AHEADFORM_ALL_CTRL *pstAheadForm, int argc, char *argv[]);
static int PlayScript(AHEADFORM_ALL_CTRL *pstAheadForm, int argc, char *argv[]);
static int Tx1MotionPose(AHEADFORM_ALL_CTRL *pstAheadForm, int argc, char *argv[]);
static int Tx1Face(AHEADFORM_ALL_CTRL *pstAheadForm, int argc, char *argv[]);
static int Tx1NeckPose(AHEADFORM_ALL_CTRL *pstAheadForm, int argc, char *argv[]);
static int TxServosTargetLogicDeg(AHEADFORM_ALL_CTRL *pstAheadForm, int argc, char *argv[]);
static void PrintSensorState(const void *pElem, void *pUserArg);
static void PrintNeckState(const void *pElem, void *pUserArg);
static void PrintFaultPool(const void *pElem, void *pUserArg);
static int ReadRecentState(AHEADFORM_ALL_CTRL *pstAheadForm, int argc, char *argv[]);
static int WatchState(AHEADFORM_ALL_CTRL *pstAheadForm, int argc, char *argv[]);
static int Exit(AHEADFORM_ALL_CTRL *pstAheadForm, int argc, char *argv[]);

const USERIN_TASK gUserInTaskAry[USERIN_CMD_CNT_MAX] =
{
	{"help", PrintCmdMenu, "print cmd menu"},
	{"shell", ShellCmd, "shell cmd   -- eg: shell ls -l"},
	{"errlog", ErrLogPrint, "errlog [storeFilePath]   -- print or store errorlog from power on"},
	{"WatchVolume", WatchVolume, "WatchVolume seconds   -- watch replies for the specified duration in seconds; query immediately and once per second"},
	{"QueryVolume", QueryVolume, "query current volume once"},
	{"AudioDevice", AudioDevice, "AudioDevice mic/spk [devId]\r\n"
								 "\t\t\t\t    <AudioDevice mic>       -- list microphones (* current)\r\n"
								 "\t\t\t\t    <AudioDevice mic mic0>  -- select microphone mic0\r\n"
								 "\t\t\t\t    <AudioDevice spk>       -- list speakers (* current)\r\n"
								 "\t\t\t\t    <AudioDevice spk spk0>  -- select speaker spk0"},
	{"OperatingMode", OperatingMode, "OperatingMode [dev/interactive]\r\n"
								 "\t\t\t\t    <OperatingMode>         -- Get current operating mode\r\n"
								 "\t\t\t\t    <OperatingMode dev>     -- leave interactive mode"},
	{"EmotePlayback", EmotePlayback, "EmotePlayback [csvFileName] [audioFileName] [loop]\r\n"
									 "\t\t\t\t    <EmotePlayback intro.csv>\r\n"
									 "\t\t\t\t    <EmotePlayback voice.wav>\r\n"
									 "\t\t\t\t    <EmotePlayback voice.mp3 intro.csv loop>"},
	{"EmotePlayCtrl", EmotePlayCtrl, "EmotePlayCtrl pause/resume/stop\r\n"
									 "\t\t\t\t    <EmotePlayCtrl pause>\r\n"
									 "\t\t\t\t    <EmotePlayCtrl resume>\r\n"
									 "\t\t\t\t    <EmotePlayCtrl stop>"},
	{"PlayScript", PlayScript, "PlayScript csvFilePath [audioFilePath]\r\n"
							   "\t\t\t\t    <PlayScript intro.csv>\r\n"
							   "\t\t\t\t    <PlayScript intro.csv voice.mp3>\r\n"
							   "\t\t\t\t    <PlayScript intro.csv voice.wav>"},
	{"Tx1MotionPose", Tx1MotionPose, "Tx1MotionPose t_us pitch_rad yaw_rad roll_rad headX headY headZ leftPitch_rad leftYaw_rad rightPitch_rad rightYaw_rad bs0 bs1 ... bs51\r\n"
									 "\t\t\t\t    bs: [0,1] (eg: 0.5), rad: finite radians (eg: -0.1)"},
	{"Tx1Face", Tx1Face, "Tx1Face leftPitch_rad leftYaw_rad rightPitch_rad rightYaw_rad bs0 bs1 ... bs51\r\n"
						 "\t\t\t\t    bs: [0,1] (eg: 0.5), rad: finite radians (eg: -0.1)"},
	{"Tx1NeckPose", Tx1NeckPose, "Tx1NeckPose pitch_rad yaw_rad roll_rad   -- eg: Tx1NeckPose 0.1 0 0"},
	{"TxServosTargetLogicDeg", TxServosTargetLogicDeg, "TxServosTargetLogicDeg deg0 deg1 ... deg19"},
	{"ReadRecentState", ReadRecentState, "ReadRecentState sensor/neck/faults cnt(1~499)\r\n"
										 "\t\t\t\t    <ReadRecentState sensor 10>\r\n"
										 "\t\t\t\t    <ReadRecentState neck 1>\r\n"
										 "\t\t\t\t    <ReadRecentState faults 499>"},
	{"WatchState", WatchState, "WatchState sensor/neck/faults seconds   -- eg: <WatchState sensor 10>"},
	{"exit", Exit, "exit"},
	{"\0", NULL}
};

int PrintCmdResult(char *strCmd, int cmdRet)
{
	int ret = 0;

	if ( (cmdRet == RET_SUCCESS) || (cmdRet == RET_EXIT) )
	{
		STD_INFO_PRINT("cmd[%s] succeed.\r\n", strCmd);
	}
	else if (cmdRet != RET_SKIPPED)
	{
		STD_INFO_PRINT("cmd[%s] error!\r\n", strCmd);
		PrintAllErrLog();
	}

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

#define PLAY_SCRIPT_MOTION_FRAME_CNT_MAX 32768
#define PLAY_SCRIPT_PCM_BUF_SIZE (20 * 1024 * 1024)
#define PLAY_SCRIPT_PERIOD_US (AHEADFORM_AUDIO_FRAME_DURATION_NS / 1000)
#define PLAY_SCRIPT_MOTION_DELAY_US 150000

static int PlaybackScriptOfUser(AHEADFORM_HEAD_CTRL *pstHeadCtrl, AHEADFORM_AUDIO_CTRL *pstAudioCtrl, const char *csvFilePath, const char *audioFilePath)
{
	//Call serially; static buffers are shared by all invocations.
	static AHEADFORM_MOTION_POSE szstPoseAry[PLAY_SCRIPT_MOTION_FRAME_CNT_MAX];
	static unsigned char pcmBuf[PLAY_SCRIPT_PCM_BUF_SIZE];
	AHEADFORM_MOTION_FILE_INFO stMotionInfo;
	AHEADFORM_AUDIO_FILE_INFO stAudioInfo;
	AHEADFORM_MOTION_POSE stPose;
	unsigned char pcmFrame[AHEADFORM_AUDIO_PCM_FRAME_SIZE];
	unsigned char isMotionCompleted = 0, isAudioStarted = 0;
	const unsigned char *pPcmData = NULL;
	const char *strSuffix = NULL;
	struct timespec stStartTime;
	struct itimerspec stTimerSpec;
	fd_set rdFdSet;
	size_t pcmDataLen = 0, pcmOffset = 0, frameDataLen;
	unsigned long long firstProcessTime_us, timerOverCnt, elapsedPeriodCnt = 0, motionPeriodCnt = 0, sentMotionFrameCnt = 0, targetTime_us = 0, audioEndTime_us = 0;
	int timer_fd, motionTimerFd = 0, max_fd, cleanupRet, ret;

	if (pstHeadCtrl == NULL)
		return ERR_ARGV_1;

	if (pstAudioCtrl == NULL && audioFilePath != NULL)
		return ERR_ARGV_2;

	if (csvFilePath == NULL || csvFilePath[0] == 0)
		return ERR_ARGV_3;

	if (audioFilePath != NULL && audioFilePath[0] == 0)
		return ERR_ARGV_4;

	if ( (strSuffix = strrchr(csvFilePath, '.')) == NULL || strcasecmp(strSuffix, ".csv") != 0 )
		return ERR_ARGV;

	if (audioFilePath != NULL)
	{
		if ( (strSuffix = strrchr(audioFilePath, '.')) == NULL || (strcasecmp(strSuffix, ".mp3") != 0 && strcasecmp(strSuffix, ".wav") != 0) )
			return ERR_ARGV;
	}
	printf("PlayScript loading motion: %s\r\n", csvFilePath);
	if ( (ret = Head_LoadMotionCsv(csvFilePath, szstPoseAry, sizeof(szstPoseAry), &stMotionInfo)) != RET_SUCCESS )
		return ret;

	printf("PlayScript motion: %u source frames, %.3f s.\r\n", stMotionInfo.frameCnt, stMotionInfo.duration_us / 1000000.0);
	if (audioFilePath != NULL)
	{
		printf("PlayScript decoding audio: %s\r\n", audioFilePath);
		if ( (ret = Audio_DecodeFileToPcm(audioFilePath, pcmBuf, sizeof(pcmBuf), &stAudioInfo)) != RET_SUCCESS )
			return ret;

		pcmDataLen = stAudioInfo.sampleFrameCnt * 2;
		audioEndTime_us = (pcmDataLen + AHEADFORM_AUDIO_PCM_FRAME_SIZE - 1) / AHEADFORM_AUDIO_PCM_FRAME_SIZE * PLAY_SCRIPT_PERIOD_US;
		printf("PlayScript audio: %zu PCM bytes, %.3f s.\r\n", pcmDataLen, stAudioInfo.duration_us / 1000000.0);
	}

	if ( (timer_fd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC)) <= 0 )
	{
		printf("%s() call timerfd_create(PlayScript timer) failed: %s\r\n", __FUNCTION__, strerror(errno));
		return ERR_CLOCK;
	}

	if (audioFilePath != NULL)
	{
		if ( (motionTimerFd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC)) <= 0 )
		{
			printf("%s() call timerfd_create(PlayScript motion timer) failed: %s\r\n", __FUNCTION__, strerror(errno));
			ret = ERR_CLOCK;
		}
		else
		{
			ret = Audio_StartSdkPlayAudio(pstAudioCtrl);
			isAudioStarted = ret == RET_SUCCESS;
		}
	}

	if (ret == RET_SUCCESS)
	{
		printf("PlayScript period = %d us, motion delay after first PCM = %d us.\r\n", PLAY_SCRIPT_PERIOD_US, audioFilePath != NULL ? PLAY_SCRIPT_MOTION_DELAY_US : 0);
		if (clock_gettime(CLOCK_MONOTONIC, &stStartTime) != RET_SUCCESS)
		{
			printf("%s() call clock_gettime(CLOCK_MONOTONIC) failed: %s\r\n", __FUNCTION__, strerror(errno));
			ret = ERR_CLOCK;
		}
		else
		{
			firstProcessTime_us = (unsigned long long)stStartTime.tv_sec * 1000000 + stStartTime.tv_nsec / 1000 + PLAY_SCRIPT_PERIOD_US;
			stTimerSpec.it_interval.tv_sec = audioFilePath != NULL ? 0 : PLAY_SCRIPT_PERIOD_US / 1000000;
			stTimerSpec.it_interval.tv_nsec = audioFilePath != NULL ? 0 : (PLAY_SCRIPT_PERIOD_US % 1000000) * 1000;
			stTimerSpec.it_value.tv_sec = firstProcessTime_us / 1000000;
			stTimerSpec.it_value.tv_nsec = (firstProcessTime_us % 1000000) * 1000;
			if (timerfd_settime(timer_fd, TFD_TIMER_ABSTIME, &stTimerSpec, NULL) != RET_SUCCESS)
			{
				printf("%s() call timerfd_settime(timer_fd = %d) failed: %s\r\n", __FUNCTION__, timer_fd, strerror(errno));
				ret = ERR_CLOCK;
			}
		}
	}
	max_fd = motionTimerFd > timer_fd ? motionTimerFd : timer_fd;
	while (ret == RET_SUCCESS)
	{
		FD_ZERO(&rdFdSet);
		FD_SET(timer_fd, &rdFdSet);
		if (audioFilePath != NULL && !isMotionCompleted)
			FD_SET(motionTimerFd, &rdFdSet);

		if (select(max_fd + 1, &rdFdSet, NULL, NULL, NULL) < 0)
		{
			printf("%s() call select(timer_fd = %d, motionTimerFd = %d) failed: %s\r\n", __FUNCTION__, timer_fd, motionTimerFd, strerror(errno));
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
			if (audioFilePath != NULL)
			{
				if ( (ret = Audio_GetSdkPlayAudioError(pstAudioCtrl)) != RET_SUCCESS )
					break;

				if (pcmOffset < pcmDataLen && timerOverCnt > 1)
				{
					printf("%s() PlayScript audio deadline missed: timerOverCnt = %llu.\r\n", __FUNCTION__, timerOverCnt);
					ret = ERR_CLOCK;
					break;
				}

				if (pcmOffset < pcmDataLen)
				{
					frameDataLen = pcmDataLen - pcmOffset;
					pPcmData = pcmBuf + pcmOffset;
					if (frameDataLen >= sizeof(pcmFrame))
					{
						frameDataLen = sizeof(pcmFrame);
					}
					else
					{
						memcpy(pcmFrame, pPcmData, frameDataLen);
						memset(pcmFrame + frameDataLen, 0, sizeof(pcmFrame) - frameDataLen);
						pPcmData = pcmFrame;
					}

					if ( (ret = Audio_WriteSdkPlayPcm(pstAudioCtrl, pPcmData, sizeof(pcmFrame))) != RET_SUCCESS )
					{
						printf("%s() Audio_WriteSdkPlayPcm(pcmOffset = %zu) failed, ret = 0x%08X.\r\n", __FUNCTION__, pcmOffset, (unsigned int)ret);
						break;
					}

					if (pcmOffset == 0)
					{
						//Use the first successful PCM submission as the phase origin.
						if (clock_gettime(CLOCK_MONOTONIC, &stStartTime) != RET_SUCCESS)
						{
							printf("%s() call clock_gettime(CLOCK_MONOTONIC) failed: %s\r\n", __FUNCTION__, strerror(errno));
							ret = ERR_CLOCK;
							break;
						}
						firstProcessTime_us = (unsigned long long)stStartTime.tv_sec * 1000000 + stStartTime.tv_nsec / 1000;
						stTimerSpec.it_interval.tv_sec = PLAY_SCRIPT_PERIOD_US / 1000000;
						stTimerSpec.it_interval.tv_nsec = (PLAY_SCRIPT_PERIOD_US % 1000000) * 1000;
						stTimerSpec.it_value.tv_sec = (firstProcessTime_us + PLAY_SCRIPT_PERIOD_US) / 1000000;
						stTimerSpec.it_value.tv_nsec = ((firstProcessTime_us + PLAY_SCRIPT_PERIOD_US) % 1000000) * 1000;
						if (timerfd_settime(timer_fd, TFD_TIMER_ABSTIME, &stTimerSpec, NULL) != RET_SUCCESS)
						{
							printf("%s() call timerfd_settime(timer_fd = %d) failed: %s\r\n", __FUNCTION__, timer_fd, strerror(errno));
							ret = ERR_CLOCK;
							break;
						}
						stTimerSpec.it_value.tv_sec = (firstProcessTime_us + PLAY_SCRIPT_MOTION_DELAY_US) / 1000000;
						stTimerSpec.it_value.tv_nsec = ((firstProcessTime_us + PLAY_SCRIPT_MOTION_DELAY_US) % 1000000) * 1000;
						if (timerfd_settime(motionTimerFd, TFD_TIMER_ABSTIME, &stTimerSpec, NULL) != RET_SUCCESS)
						{
							printf("%s() call timerfd_settime(motionTimerFd = %d) failed: %s\r\n", __FUNCTION__, motionTimerFd, strerror(errno));
							ret = ERR_CLOCK;
							break;
						}
					}
					pcmOffset += frameDataLen;
				}
			}
			else
			{
				targetTime_us = (elapsedPeriodCnt - 1) * PLAY_SCRIPT_PERIOD_US;
			}
		}

		if (audioFilePath != NULL && FD_ISSET(motionTimerFd, &rdFdSet))
		{
			if (read(motionTimerFd, &timerOverCnt, sizeof(timerOverCnt)) < 0)
			{
				printf("%s() call read(motionTimerFd = %d) failed: %s\r\n", __FUNCTION__, motionTimerFd, strerror(errno));
				ret = ERR_CLOCK;
				break;
			}
			motionPeriodCnt += timerOverCnt;
			targetTime_us = (motionPeriodCnt - 1) * PLAY_SCRIPT_PERIOD_US;
		}

		if (!isMotionCompleted && ((audioFilePath == NULL && FD_ISSET(timer_fd, &rdFdSet)) || (audioFilePath != NULL && FD_ISSET(motionTimerFd, &rdFdSet))))
		{
			if (targetTime_us > stMotionInfo.duration_us)
				targetTime_us = stMotionInfo.duration_us;

			if ( (ret = Head_CalcInterpolateMotionPose(szstPoseAry, stMotionInfo.frameCnt, targetTime_us, &stPose)) != RET_SUCCESS )
				break;

			if ( (ret = Head_Send1MotionPose(pstHeadCtrl, &stPose)) != RET_SUCCESS )
				break;

			sentMotionFrameCnt++;
			isMotionCompleted = targetTime_us == stMotionInfo.duration_us;
		}

		if (FD_ISSET(timer_fd, &rdFdSet) && elapsedPeriodCnt % 100 == 1)
			printf("PlayScript client %.3f s, motion %.3f / %.3f s, PCM %zu / %zu bytes.\r\n", (elapsedPeriodCnt - 1) * PLAY_SCRIPT_PERIOD_US / 1000000.0, targetTime_us / 1000000.0, stMotionInfo.duration_us / 1000000.0, pcmOffset, pcmDataLen);

		if (isMotionCompleted && pcmOffset == pcmDataLen && (elapsedPeriodCnt - 1) * PLAY_SCRIPT_PERIOD_US >= audioEndTime_us)
			break;
	}

	if (motionTimerFd > 0 && close(motionTimerFd) != RET_SUCCESS)
	{
		printf("%s() call close(motionTimerFd = %d) failed: %s\r\n", __FUNCTION__, motionTimerFd, strerror(errno));
		ret = ERR_CLOCK;
	}

	if (close(timer_fd) != RET_SUCCESS)
	{
		printf("%s() call close(timer_fd = %d) failed: %s\r\n", __FUNCTION__, timer_fd, strerror(errno));
		ret = ERR_CLOCK;
	}

	if (isAudioStarted && ret != RET_SUCCESS)
	{
		if ( (cleanupRet = Audio_StopSdkPlayAudio(pstAudioCtrl)) != RET_SUCCESS )
			printf("%s() Audio_StopSdkPlayAudio() failed, ret = 0x%08X.\r\n", __FUNCTION__, (unsigned int)cleanupRet);
	}
	printf("PlayScript motion %.3f / %.3f s, sent %llu frames, PCM %zu / %zu bytes.\r\n", targetTime_us / 1000000.0, stMotionInfo.duration_us / 1000000.0, sentMotionFrameCnt, pcmOffset, pcmDataLen);
	return ret;
}

//Cmd Task function...................................................................................................................
//
static int PrintCmdMenu(AHEADFORM_ALL_CTRL *pstAheadForm, int argc, char *argv[])
{
	int i;

	if (argc != 1)
		return ERR_ARGC;

	for (i = 0; gUserInTaskAry[i].pFunction != NULL; i++)
		printf("\t%-24s:%s\r\n", gUserInTaskAry[i].strCmd, gUserInTaskAry[i].strDescription);

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

#if USER_CMD_REPLY_METHOD == USER_CMD_REPLY_METHOD_RX_EVENTFD //timerfd + rxDataUpdateEventfd
static int WatchVolume(AHEADFORM_ALL_CTRL *pstAheadForm, int argc, char *argv[])
{
	AHEADFORM_TOPIC_READER_PARA stReaderPara = {0};
	AHEADFORM_TOPIC_READER *pstReader = NULL;
	AHEADFORM_VOLUME_ASK stAsk;
	AHEADFORM_VOLUME_REPLY stReply;
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

	if ( (ret = Audio_BuildQueryVolumeRequest(&pstAheadForm->stRobotCtrl.stMediaCtrl.stAudioCtrl, &stAsk)) != RET_SUCCESS )
		return ret;

	if ( (pstReader = Audio_RegisterVolumeReader(&pstAheadForm->stRobotCtrl.stMediaCtrl.stAudioCtrl, &stReaderPara)) == NULL )
		return ERRCODE_INIT_FAIL;

	if ( (timer_fd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC)) <= 0 )
	{
		printf("%s() call timerfd_create(volume query timer) failed: %s\r\n", __FUNCTION__, strerror(errno));
		if ( (cleanupRet = Audio_UnregisterVolumeReader(&pstAheadForm->stRobotCtrl.stMediaCtrl.stAudioCtrl, pstReader)) != RET_SUCCESS )
		{
			printf("%s() Audio_UnregisterVolumeReader(Reader = %p) failed, ret = 0x%08X; 退出 AheadFormSDK。\r\n", __FUNCTION__, (void*)pstReader, cleanupRet);
			PrintAllErrLog();
			exit(EXIT_FAILURE);
		}

		return ERR_CLOCK;
	}

	if ( (ret = SetTimerPeriod_ms(timer_fd, 1000)) != RET_SUCCESS )
	{
		if ( (cleanupRet = Audio_UnregisterVolumeReader(&pstAheadForm->stRobotCtrl.stMediaCtrl.stAudioCtrl, pstReader)) != RET_SUCCESS )
		{
			printf("%s() Audio_UnregisterVolumeReader(Reader = %p) failed, ret = 0x%08X; 退出 AheadFormSDK。\r\n", __FUNCTION__, (void*)pstReader, cleanupRet);
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

	if ( (ret = Audio_SendVolumeRequest(&pstAheadForm->stRobotCtrl.stMediaCtrl.stAudioCtrl, &stAsk)) != RET_SUCCESS )
	{
		if ( (cleanupRet = Audio_UnregisterVolumeReader(&pstAheadForm->stRobotCtrl.stMediaCtrl.stAudioCtrl, pstReader)) != RET_SUCCESS )
		{
			printf("%s() Audio_UnregisterVolumeReader(Reader = %p) failed, ret = 0x%08X; 退出 AheadFormSDK。\r\n", __FUNCTION__, (void*)pstReader, cleanupRet);
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

		if (FD_ISSET(pstReader->rxDataUpdateEventfd, &rdFdSet))
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

			if ( (ret = Audio_BuildQueryVolumeRequest(&pstAheadForm->stRobotCtrl.stMediaCtrl.stAudioCtrl, &stAsk)) != RET_SUCCESS )
				break;

			if ( (ret = Audio_SendVolumeRequest(&pstAheadForm->stRobotCtrl.stMediaCtrl.stAudioCtrl, &stAsk)) != RET_SUCCESS )
				break;

		}
	}

	if ( (cleanupRet = Audio_UnregisterVolumeReader(&pstAheadForm->stRobotCtrl.stMediaCtrl.stAudioCtrl, pstReader)) != RET_SUCCESS )
	{
		printf("%s() Audio_UnregisterVolumeReader(Reader = %p) failed, ret = 0x%08X; 退出 AheadFormSDK。\r\n", __FUNCTION__, (void*)pstReader, cleanupRet);
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

#elif USER_CMD_REPLY_METHOD == USER_CMD_REPLY_METHOD_ASYNC_REQUEST //timerfd + async request notifyEventfd
static int WatchVolume(AHEADFORM_ALL_CTRL *pstAheadForm, int argc, char *argv[])
{
	AHEADFORM_VOLUME_REPLY stReply;
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

	if ( (ret = Audio_QueryVolumeAsync(&pstAheadForm->stRobotCtrl.stMediaCtrl.stAudioCtrl, VOLUME_QUERY_TIMEOUT_MS * 1000, notifyEventfd, &stReply)) != RET_SUCCESS )
	{
		if ( (cleanupRet = Audio_CancelVolumeAsyncRequest(&pstAheadForm->stRobotCtrl.stMediaCtrl.stAudioCtrl, notifyEventfd)) != RET_SUCCESS )
		{
			printf("%s() Audio_CancelVolumeAsyncRequest(notifyEventfd = %d) failed, ret = 0x%08X; 退出 AheadFormSDK。\r\n", __FUNCTION__, notifyEventfd, cleanupRet);
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

		if (FD_ISSET(notifyEventfd, &rdFdSet))
		{
			if (eventfd_read(notifyEventfd, &eventCnt) == RET_SUCCESS)
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
		}

		if (FD_ISSET(timer_fd, &rdFdSet))
		{
			if (read(timer_fd, &timerOverCnt, sizeof(timerOverCnt)) < 0)
			{
				printf("%s() call read(timer_fd = %d) failed: %s\r\n", __FUNCTION__, timer_fd, strerror(errno));
				ret = ERR_CLOCK;
				break;
			}

			if ( (cleanupRet = Audio_CancelVolumeAsyncRequest(&pstAheadForm->stRobotCtrl.stMediaCtrl.stAudioCtrl, notifyEventfd)) != RET_SUCCESS )
			{
				printf("%s() Audio_CancelVolumeAsyncRequest(notifyEventfd = %d) failed, ret = 0x%08X; 退出 AheadFormSDK。\r\n", __FUNCTION__, notifyEventfd, cleanupRet);
				PrintAllErrLog();
				exit(EXIT_FAILURE);
			}

			//Cancel ends SDK access; consume any completion that won the race before cancellation.
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
			elapsedPeriodCnt += timerOverCnt;
			if (elapsedPeriodCnt >= (unsigned long long)seconds)
				break;

			if ( (ret = Audio_QueryVolumeAsync(&pstAheadForm->stRobotCtrl.stMediaCtrl.stAudioCtrl, VOLUME_QUERY_TIMEOUT_MS * 1000, notifyEventfd, &stReply)) != RET_SUCCESS )
				break;

		}
	}

	if ( (cleanupRet = Audio_CancelVolumeAsyncRequest(&pstAheadForm->stRobotCtrl.stMediaCtrl.stAudioCtrl, notifyEventfd)) != RET_SUCCESS )
	{
		printf("%s() Audio_CancelVolumeAsyncRequest(notifyEventfd = %d) failed, ret = 0x%08X; 退出 AheadFormSDK。\r\n", __FUNCTION__, notifyEventfd, cleanupRet);
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

#else //timerfd + Reader callback
static void PrintVolumeSample(const void *pElem, void *pUserArg)
{
	const AHEADFORM_VOLUME_REPLY *pstReply = (const AHEADFORM_VOLUME_REPLY*)pElem;

	printf("source=%s, askId=%llu, operation=%u, ret=%d, queryValid=%u.\r\n", pstReply->sourceName, pstReply->askId, pstReply->operation, pstReply->retCode, pstReply->queryDataValid);
	printf("volume receive=%llu us, mic=%.1f%%, speaker=%.1f%%.\r\n", pstReply->ReceiveTime_us, pstReply->stVolume.micVolumePercent, pstReply->stVolume.spkVolumePercent);
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

	if ( (ret = Audio_BuildQueryVolumeRequest(&pstAheadForm->stRobotCtrl.stMediaCtrl.stAudioCtrl, &stAsk)) != RET_SUCCESS )
		return ret;

	if ( (pstReader = Audio_RegisterVolumeReader(&pstAheadForm->stRobotCtrl.stMediaCtrl.stAudioCtrl, &stReaderPara)) == NULL )
		return ERRCODE_INIT_FAIL;

	if ( (timer_fd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC)) <= 0 )
	{
		printf("%s() call timerfd_create(volume query timer) failed: %s\r\n", __FUNCTION__, strerror(errno));
		if ( (cleanupRet = Audio_UnregisterVolumeReader(&pstAheadForm->stRobotCtrl.stMediaCtrl.stAudioCtrl, pstReader)) != RET_SUCCESS )
		{
			printf("%s() Audio_UnregisterVolumeReader(Reader = %p) failed, ret = 0x%08X; 退出 AheadFormSDK。\r\n", __FUNCTION__, (void*)pstReader, cleanupRet);
			PrintAllErrLog();
			exit(EXIT_FAILURE);
		}

		return ERR_CLOCK;
	}

	if ( (ret = SetTimerPeriod_ms(timer_fd, 1000)) != RET_SUCCESS )
	{
		if ( (cleanupRet = Audio_UnregisterVolumeReader(&pstAheadForm->stRobotCtrl.stMediaCtrl.stAudioCtrl, pstReader)) != RET_SUCCESS )
		{
			printf("%s() Audio_UnregisterVolumeReader(Reader = %p) failed, ret = 0x%08X; 退出 AheadFormSDK。\r\n", __FUNCTION__, (void*)pstReader, cleanupRet);
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

	if ( (ret = Audio_SendVolumeRequest(&pstAheadForm->stRobotCtrl.stMediaCtrl.stAudioCtrl, &stAsk)) != RET_SUCCESS )
	{
		if ( (cleanupRet = Audio_UnregisterVolumeReader(&pstAheadForm->stRobotCtrl.stMediaCtrl.stAudioCtrl, pstReader)) != RET_SUCCESS )
		{
			printf("%s() Audio_UnregisterVolumeReader(Reader = %p) failed, ret = 0x%08X; 退出 AheadFormSDK。\r\n", __FUNCTION__, (void*)pstReader, cleanupRet);
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

			if ( (ret = Audio_BuildQueryVolumeRequest(&pstAheadForm->stRobotCtrl.stMediaCtrl.stAudioCtrl, &stAsk)) != RET_SUCCESS )
				break;

			if ( (ret = Audio_SendVolumeRequest(&pstAheadForm->stRobotCtrl.stMediaCtrl.stAudioCtrl, &stAsk)) != RET_SUCCESS )
				break;
		}
	}

	if ( (cleanupRet = Audio_UnregisterVolumeReader(&pstAheadForm->stRobotCtrl.stMediaCtrl.stAudioCtrl, pstReader)) != RET_SUCCESS )
	{
		printf("%s() Audio_UnregisterVolumeReader(Reader = %p) failed, ret = 0x%08X; 退出 AheadFormSDK。\r\n", __FUNCTION__, (void*)pstReader, cleanupRet);
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

#if USER_CMD_REPLY_METHOD == USER_CMD_REPLY_METHOD_RX_EVENTFD //query volume by rxDataUpdateEventfd
static int QueryVolume(AHEADFORM_ALL_CTRL *pstAheadForm, int argc, char *argv[])
{
	AHEADFORM_TOPIC_READER_PARA stReaderPara = {0};
	AHEADFORM_TOPIC_READER *pstReader = NULL;
	AHEADFORM_VOLUME_ASK stAsk;
	AHEADFORM_VOLUME_REPLY stReply;
	AHEADFORM_VOLUME stVolume;
	struct timeval stTimeout = {.tv_sec = VOLUME_QUERY_TIMEOUT_MS / 1000, .tv_usec = VOLUME_QUERY_TIMEOUT_MS % 1000 * 1000};
	fd_set rdFdSet;
	eventfd_t eventCnt;
	unsigned int i, readableElemCnt;
	int max_fd, cleanupRet, ret;

	if ( (ret = IsUserInError(argc, argv, "1", NULL)) != RET_SUCCESS )
		return ret;

	if ( (ret = Audio_BuildQueryVolumeRequest(&pstAheadForm->stRobotCtrl.stMediaCtrl.stAudioCtrl, &stAsk)) != RET_SUCCESS )
		return ret;

	if ( (pstReader = Audio_RegisterVolumeReader(&pstAheadForm->stRobotCtrl.stMediaCtrl.stAudioCtrl, &stReaderPara)) == NULL )
		return ERRCODE_INIT_FAIL;

	if ( (ret = Audio_SendVolumeRequest(&pstAheadForm->stRobotCtrl.stMediaCtrl.stAudioCtrl, &stAsk)) != RET_SUCCESS )
	{
		if ( (cleanupRet = Audio_UnregisterVolumeReader(&pstAheadForm->stRobotCtrl.stMediaCtrl.stAudioCtrl, pstReader)) != RET_SUCCESS )
		{
			printf("%s() Audio_UnregisterVolumeReader(Reader = %p) failed, ret = 0x%08X; 退出 AheadFormSDK。\r\n", __FUNCTION__, (void*)pstReader, cleanupRet);
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
		if ( (ret = select(max_fd + 1, &rdFdSet, NULL, NULL, &stTimeout)) < 0 )
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

				if (Audio_MatchVolumeReplyWithReq(&stReply, &stAsk) == RET_SUCCESS)
					break;

				ret = 0;
			}
		}

		if (ret != 0)
			break;

		if (stTimeout.tv_sec == 0 && stTimeout.tv_usec == 0)
			break;

	}

	if ( (cleanupRet = Audio_UnregisterVolumeReader(&pstAheadForm->stRobotCtrl.stMediaCtrl.stAudioCtrl, pstReader)) != RET_SUCCESS )
	{
		printf("%s() Audio_UnregisterVolumeReader(Reader = %p) failed, ret = 0x%08X; 退出 AheadFormSDK。\r\n", __FUNCTION__, (void*)pstReader, cleanupRet);
		PrintAllErrLog();
		exit(EXIT_FAILURE);
	}

	if (ret == 0)
		return ERR_REQUEST_TIMEOUT;

	if (ret < 0)
		return ret;

	if ( (ret = Audio_GetVolumeFromReply(&stReply, &stVolume)) != RET_SUCCESS )
		return ret;

	printf("mic volume = %.1f%%, speaker volume = %.1f%%.\r\n", stVolume.micVolumePercent, stVolume.spkVolumePercent);
	return RET_SUCCESS;
}

#elif USER_CMD_REPLY_METHOD == USER_CMD_REPLY_METHOD_ASYNC_REQUEST //query volume by async request
static int QueryVolume(AHEADFORM_ALL_CTRL *pstAheadForm, int argc, char *argv[])
{
	AHEADFORM_VOLUME_REPLY stReply;
	AHEADFORM_VOLUME stVolume;
	struct timeval stTimeout = {.tv_sec = VOLUME_QUERY_TIMEOUT_MS / 1000, .tv_usec = VOLUME_QUERY_TIMEOUT_MS % 1000 * 1000};
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

	if ( (ret = Audio_QueryVolumeAsync(&pstAheadForm->stRobotCtrl.stMediaCtrl.stAudioCtrl, VOLUME_QUERY_TIMEOUT_MS * 1000, notifyEventfd, &stReply)) != RET_SUCCESS )
	{
		if ( (cleanupRet = Audio_CancelVolumeAsyncRequest(&pstAheadForm->stRobotCtrl.stMediaCtrl.stAudioCtrl, notifyEventfd)) != RET_SUCCESS )
		{
			printf("%s() Audio_CancelVolumeAsyncRequest(notifyEventfd = %d) failed, ret = 0x%08X; 退出 AheadFormSDK。\r\n", __FUNCTION__, notifyEventfd, cleanupRet);
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
		if ( (ret = select(max_fd + 1, &rdFdSet, NULL, NULL, &stTimeout)) < 0 )
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
			ret = 1;
			break;
		}

		if (stTimeout.tv_sec == 0 && stTimeout.tv_usec == 0)
			break;

	}

	if ( (cleanupRet = Audio_CancelVolumeAsyncRequest(&pstAheadForm->stRobotCtrl.stMediaCtrl.stAudioCtrl, notifyEventfd)) != RET_SUCCESS )
	{
		printf("%s() Audio_CancelVolumeAsyncRequest(notifyEventfd = %d) failed, ret = 0x%08X; 退出 AheadFormSDK。\r\n", __FUNCTION__, notifyEventfd, cleanupRet);
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
			ret = ERR_REQUEST_TIMEOUT;
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

	if (ret != 1)
		return ret;

	if ( (ret = Audio_GetVolumeFromReply(&stReply, &stVolume)) != RET_SUCCESS )
		return ret;

	printf("mic volume = %.1f%%, speaker volume = %.1f%%.\r\n", stVolume.micVolumePercent, stVolume.spkVolumePercent);
	return RET_SUCCESS;
}

#else //query volume by Reader callback
typedef struct
{
	AHEADFORM_VOLUME_ASK stAsk;
	AHEADFORM_VOLUME_REPLY stReply;
	int completionEventfd;
	int isReplied;
} VOLUME_QUERY_CALLBACK_CTX;

static void CopyVolumeReplyIfMatchesReq(const void *pElem, void *pUserArg)
{
	const AHEADFORM_VOLUME_REPLY *pstReply = (const AHEADFORM_VOLUME_REPLY*)pElem;
	VOLUME_QUERY_CALLBACK_CTX *pstCtx = (VOLUME_QUERY_CALLBACK_CTX*)pUserArg;

	if (pstCtx->isReplied || Audio_MatchVolumeReplyWithReq(pstReply, &pstCtx->stAsk) != RET_SUCCESS)
		return;

	pstCtx->stReply = *pstReply;
	pstCtx->isReplied = 1;
	if (eventfd_write(pstCtx->completionEventfd, 1) < 0 && errno != EAGAIN)
		PRINT_FUNCTION_ERRINFO(ERR_EVENTFD_IO_FAILED, __FUNCTION__, "call eventfd_write(completionEventfd = %d) failed: %s", pstCtx->completionEventfd, strerror(errno));
}

static int QueryVolume(AHEADFORM_ALL_CTRL *pstAheadForm, int argc, char *argv[])
{
	VOLUME_QUERY_CALLBACK_CTX stCtx = {.isReplied = 0};
	AHEADFORM_TOPIC_READER_PARA stReaderPara = {CopyVolumeReplyIfMatchesReq, &stCtx};
	AHEADFORM_TOPIC_READER *pstReader = NULL;
	AHEADFORM_VOLUME stVolume;
	struct timeval stTimeout = {.tv_sec = VOLUME_QUERY_TIMEOUT_MS / 1000, .tv_usec = VOLUME_QUERY_TIMEOUT_MS % 1000 * 1000};
	fd_set rdFdSet;
	int max_fd, cleanupRet, ret;

	if ( (ret = IsUserInError(argc, argv, "1", NULL)) != RET_SUCCESS )
		return ret;

	if ( (ret = Audio_BuildQueryVolumeRequest(&pstAheadForm->stRobotCtrl.stMediaCtrl.stAudioCtrl, &stCtx.stAsk)) != RET_SUCCESS )
		return ret;

	if ( (stCtx.completionEventfd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC)) <= 0 )
	{
		printf("%s() call eventfd(volume completion) failed: %s\r\n", __FUNCTION__, strerror(errno));
		return ERR_EVENTFD_CREATE_FAILED;
	}

	if ( (pstReader = Audio_RegisterVolumeReader(&pstAheadForm->stRobotCtrl.stMediaCtrl.stAudioCtrl, &stReaderPara)) == NULL )
	{
		if (close(stCtx.completionEventfd) < 0)
		{
			printf("%s() call close(stCtx.completionEventfd = %d) failed: %s\r\n", __FUNCTION__, stCtx.completionEventfd, strerror(errno));
			return ERR_EVENTFD_IO_FAILED;
		}

		return ERRCODE_INIT_FAIL;
	}

	if ( (ret = Audio_SendVolumeRequest(&pstAheadForm->stRobotCtrl.stMediaCtrl.stAudioCtrl, &stCtx.stAsk)) != RET_SUCCESS )
	{
		if ( (cleanupRet = Audio_UnregisterVolumeReader(&pstAheadForm->stRobotCtrl.stMediaCtrl.stAudioCtrl, pstReader)) != RET_SUCCESS )
		{
			printf("%s() Audio_UnregisterVolumeReader(Reader = %p) failed, ret = 0x%08X; 退出 AheadFormSDK。\r\n", __FUNCTION__, (void*)pstReader, cleanupRet);
			PrintAllErrLog();
			exit(EXIT_FAILURE);
		}

		if (close(stCtx.completionEventfd) < 0)
		{
			printf("%s() call close(stCtx.completionEventfd = %d) failed: %s\r\n", __FUNCTION__, stCtx.completionEventfd, strerror(errno));
			return ERR_EVENTFD_IO_FAILED;
		}

		return ret;
	}
	max_fd = stCtx.completionEventfd;
	while (1)
	{
		FD_ZERO(&rdFdSet);
		FD_SET(stCtx.completionEventfd, &rdFdSet);
		if ( (ret = select(max_fd + 1, &rdFdSet, NULL, NULL, &stTimeout)) < 0 )
		{
			printf("%s() call select(completionEventfd = %d) failed: %s\r\n", __FUNCTION__, stCtx.completionEventfd, strerror(errno));
			ret = ERR_SELECT;
			break;
		}
		else if (ret == 0)
		{
			break;
		}
		ret = RET_SUCCESS;
		if (FD_ISSET(stCtx.completionEventfd, &rdFdSet))
			break;

		if (stTimeout.tv_sec == 0 && stTimeout.tv_usec == 0)
			break;
	}

	//Unregister synchronizes with callbacks before this thread reads their output.
	if ( (cleanupRet = Audio_UnregisterVolumeReader(&pstAheadForm->stRobotCtrl.stMediaCtrl.stAudioCtrl, pstReader)) != RET_SUCCESS )
	{
		printf("%s() Audio_UnregisterVolumeReader(Reader = %p) failed, ret = 0x%08X; 退出 AheadFormSDK。\r\n", __FUNCTION__, (void*)pstReader, cleanupRet);
		PrintAllErrLog();
		exit(EXIT_FAILURE);
	}

	if (close(stCtx.completionEventfd) < 0)
	{
		printf("%s() call close(stCtx.completionEventfd = %d) failed: %s\r\n", __FUNCTION__, stCtx.completionEventfd, strerror(errno));
		return ERR_EVENTFD_IO_FAILED;
	}

	if (ret != RET_SUCCESS)
		return ret;

	if (!stCtx.isReplied)
		return ERR_REQUEST_TIMEOUT;

	if ( (ret = Audio_GetVolumeFromReply(&stCtx.stReply, &stVolume)) != RET_SUCCESS )
		return ret;

	printf("mic volume = %.1f%%, speaker volume = %.1f%%.\r\n", stVolume.micVolumePercent, stVolume.spkVolumePercent);
	return RET_SUCCESS;
}
#endif

static int PlayScript(AHEADFORM_ALL_CTRL *pstAheadForm, int argc, char *argv[])
{
	const char *csvFilePath = NULL, *audioFilePath = NULL;
	int ret;

	if ( (ret = IsUserInError(argc, argv, "2,3", NULL)) != RET_SUCCESS )
		return ret;

	csvFilePath = argv[1];
	if (argc == 3)
		audioFilePath = argv[2];

	return PlaybackScriptOfUser(&pstAheadForm->stRobotCtrl.stHeadCtrl, &pstAheadForm->stRobotCtrl.stMediaCtrl.stAudioCtrl, csvFilePath, audioFilePath);
}

static int Tx1MotionPose(AHEADFORM_ALL_CTRL *pstAheadForm, int argc, char *argv[])
{
	AHEADFORM_MOTION_POSE stPose;
	float szValues[10 + AHEADFORM_BLENDSHAPE_CNT];
	char *pEnd = NULL;
	unsigned int i;
	int ret;

	if (argc != 64)
		return ERR_ARGC;

	if ( (ret = IsUserInError(argc, argv, NULL, "1")) != RET_SUCCESS )
		return ret;

	stPose.t_us = strtoull(argv[1], NULL, 0);
	for (i = 0; i < 10 + AHEADFORM_BLENDSHAPE_CNT; i++)
	{
		szValues[i] = strtof(argv[i + 2], &pEnd);
		if (pEnd == argv[i + 2] || *pEnd != 0 || !isfinite(szValues[i]))
			return ERR_ARGV;
	}
	stPose.stNeck.pitch_rad = szValues[0];
	stPose.stNeck.yaw_rad = szValues[1];
	stPose.stNeck.roll_rad = szValues[2];
	stPose.stNeck.headX = szValues[3];
	stPose.stNeck.headY = szValues[4];
	stPose.stNeck.headZ = szValues[5];
	stPose.stEyes.leftPitch_rad = szValues[6];
	stPose.stEyes.leftYaw_rad = szValues[7];
	stPose.stEyes.rightPitch_rad = szValues[8];
	stPose.stEyes.rightYaw_rad = szValues[9];
	memcpy(stPose.blendshapes, &szValues[10], sizeof(stPose.blendshapes));
	return Head_Send1MotionPose(&pstAheadForm->stRobotCtrl.stHeadCtrl, &stPose);
}

static int Tx1Face(AHEADFORM_ALL_CTRL *pstAheadForm, int argc, char *argv[])
{
	AHEADFORM_EYE_POSE stEyes;
	float szValues[4 + AHEADFORM_BLENDSHAPE_CNT];
	char *pEnd = NULL;
	unsigned int i;

	if (argc != 57)
		return ERR_ARGC;

	for (i = 0; i < 4 + AHEADFORM_BLENDSHAPE_CNT; i++)
	{
		szValues[i] = strtof(argv[i + 1], &pEnd);
		if (pEnd == argv[i + 1] || *pEnd != 0 || !isfinite(szValues[i]))
			return ERR_ARGV;
	}
	stEyes.leftPitch_rad = szValues[0];
	stEyes.leftYaw_rad = szValues[1];
	stEyes.rightPitch_rad = szValues[2];
	stEyes.rightYaw_rad = szValues[3];
	return Head_Send1Face(&pstAheadForm->stRobotCtrl.stHeadCtrl, &stEyes, &szValues[4]);
}

static int Tx1NeckPose(AHEADFORM_ALL_CTRL *pstAheadForm, int argc, char *argv[])
{
	float szValues[3];
	char *pEnd = NULL;
	unsigned int i;
	int ret;

	if ( (ret = IsUserInError(argc, argv, "4", NULL)) != RET_SUCCESS )
		return ret;

	for (i = 0; i < 3; i++)
	{
		szValues[i] = strtof(argv[i + 1], &pEnd);
		if (pEnd == argv[i + 1] || *pEnd != 0 || !isfinite(szValues[i]))
			return ERR_ARGV;
	}

	return Head_Send1NeckPose(&pstAheadForm->stRobotCtrl.stHeadCtrl, szValues[0], szValues[1], szValues[2]);
}

static int TxServosTargetLogicDeg(AHEADFORM_ALL_CTRL *pstAheadForm, int argc, char *argv[])
{
	double szTargetLogicalDeg[AHEADFORM_FACE_SERVO_CNT];
	char *pEnd = NULL;
	unsigned int i;

	if (argc != 21)
		return ERR_ARGC;

	for (i = 0; i < AHEADFORM_FACE_SERVO_CNT; i++)
	{
		szTargetLogicalDeg[i] = strtod(argv[i + 1], &pEnd);
		if (pEnd == argv[i + 1] || *pEnd != 0 || !isfinite(szTargetLogicalDeg[i]))
			return ERR_ARGV;
	}

	return Head_SendServosTargetDeg(&pstAheadForm->stRobotCtrl.stHeadCtrl, szTargetLogicalDeg);
}

#define MEDIA_REQUEST_TIMEOUT_MS 3000

#if USER_CMD_REPLY_METHOD == USER_CMD_REPLY_METHOD_RX_EVENTFD
static int AudioDevice(AHEADFORM_ALL_CTRL *pstAheadForm, int argc, char *argv[])
{
	AHEADFORM_AUDIO_CTRL *pstAudioCtrl = &pstAheadForm->stRobotCtrl.stMediaCtrl.stAudioCtrl;
	AHEADFORM_TOPIC_READER_PARA stReaderPara = {0};
	AHEADFORM_TOPIC_READER *pstReader = NULL;
	AHEADFORM_AUDIO_DEVICE_ASK stAsk;
	AHEADFORM_AUDIO_DEVICE_REPLY stReply;
	AHEADFORM_AUDIO_DEVICE_REPLY stDeviceList;
	AHEADFORM_AUDIO_DEVICE_ROUTE stRoute = {0};
	ENUM_AHEADFORM_AUDIO_DEVICE_OPERATION operation;
	ENUM_AHEADFORM_AUDIO_DEVICE_TYPE deviceType;
	const char *currentDeviceId = NULL;
	struct timeval stTimeout;
	fd_set rdFdSet;
	eventfd_t eventCnt;
	unsigned int i, requestIndex, readableElemCnt;
	int max_fd, cleanupRet, isCurrentListed = 0, ret;

	if ( (ret = IsUserInError(argc, argv, "2;3", NULL)) != RET_SUCCESS )
		return ret;

	if (strcasecmp(argv[1], "mic") == 0)
		deviceType = AHEADFORM_AUDIO_DEVICE_TYPE_MIC;
	else if (strcasecmp(argv[1], "spk") == 0)
		deviceType = AHEADFORM_AUDIO_DEVICE_TYPE_SPK;
	else
		return ERR_ARGV;

	if (argc == 3)
	{
		if (strlen(argv[2]) >= AHEADFORM_AUDIO_DEVICE_ID_LEN_MAX)
			return ERR_ARGV;

		if (deviceType == AHEADFORM_AUDIO_DEVICE_TYPE_MIC)
			strcpy(stRoute.micDeviceId, argv[2]);
		else
			strcpy(stRoute.spkDeviceId, argv[2]);
	}

	for (requestIndex = 0; requestIndex < (argc == 2 ? 2 : 1); requestIndex++)
	{
		if (argc == 3)
		{
			ret = Audio_BuildRouteAudioDeviceRequest(pstAudioCtrl, &stRoute, &stAsk);
		}
		else
		{
			operation = requestIndex == 0 ? AHEADFORM_AUDIO_DEVICE_OPERATION_GET_LIST : AHEADFORM_AUDIO_DEVICE_OPERATION_GET_CURRENT;
			ret = Audio_BuildQueryAudioDeviceRequest(pstAudioCtrl, operation, deviceType, &stAsk);
		}

		if (ret != RET_SUCCESS)
			break;

		if ( (pstReader = Audio_RegisterAudioDeviceReader(pstAudioCtrl, &stReaderPara)) == NULL )
		{
			ret = ERRCODE_INIT_FAIL;
			break;
		}

		if ( (ret = Audio_SendAudioDeviceRequest(pstAudioCtrl, &stAsk)) == RET_SUCCESS )
		{
			stTimeout.tv_sec = MEDIA_REQUEST_TIMEOUT_MS / 1000;
			stTimeout.tv_usec = MEDIA_REQUEST_TIMEOUT_MS % 1000 * 1000;
			max_fd = pstReader->rxDataUpdateEventfd;
			while (1)
			{
				FD_ZERO(&rdFdSet);
				FD_SET(pstReader->rxDataUpdateEventfd, &rdFdSet);
				if ( (ret = select(max_fd + 1, &rdFdSet, NULL, NULL, &stTimeout)) < 0 )
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
					readableElemCnt = TopicReader_GetReadableElemCnt(pstReader);
					for (i = 0; i < readableElemCnt; i++)
					{
						if ( (ret = TopicReader_PopElem(pstReader, &stReply)) <= 0 )
							break;

						if (Audio_MatchAudioDeviceReplyWithReq(&stReply, &stAsk) == RET_SUCCESS)
							break;

						ret = 0;
					}
				}

				if (ret != 0)
					break;

				if (stTimeout.tv_sec == 0 && stTimeout.tv_usec == 0)
					break;
			}
		}

		if ( (cleanupRet = Audio_UnregisterAudioDeviceReader(pstAudioCtrl, pstReader)) != RET_SUCCESS )
		{
			printf("%s() Audio_UnregisterAudioDeviceReader(Reader = %p) failed, ret = 0x%08X; 退出 AheadFormSDK。\r\n", __FUNCTION__, (void*)pstReader, cleanupRet);
			PrintAllErrLog();
			exit(EXIT_FAILURE);
		}
		pstReader = NULL;
		if (ret == 0)
		{
			ret = ERR_REQUEST_TIMEOUT;
		}
		else if (ret > 0)
		{
			ret = stReply.retCode;
		}

		if (ret != RET_SUCCESS)
			break;

		if (argc == 2)
		{
			if (!stReply.queryDataValid || (requestIndex == 1 && stReply.deviceCnt > 1))
			{
				ret = ERR_DATA_PACKET_FORMAT;
				break;
			}

			if (requestIndex == 0)
				stDeviceList = stReply;
		}
	}

	if (argc == 3)
	{
		if (ret == RET_SUCCESS)
			printf("%s device: %s.\r\n", argv[1], argv[2]);

		return ret;
	}

	if (requestIndex == 0)
		return ret;

	if (ret == RET_SUCCESS && stReply.deviceCnt == 1 && stReply.szstDeviceInfo[0].strDeviceId[0] != 0)
		currentDeviceId = stReply.szstDeviceInfo[0].strDeviceId;

	printf("%s devices (* current):\r\n", argv[1]);
	for (i = 0; i < stDeviceList.deviceCnt; i++)
	{
		if (currentDeviceId != NULL && strcmp(stDeviceList.szstDeviceInfo[i].strDeviceId, currentDeviceId) == 0)
		{
			printf("* %s  %s\r\n", stDeviceList.szstDeviceInfo[i].strDeviceId, stDeviceList.szstDeviceInfo[i].deviceName);
			isCurrentListed = 1;
		}
		else
		{
			printf("  %s  %s\r\n", stDeviceList.szstDeviceInfo[i].strDeviceId, stDeviceList.szstDeviceInfo[i].deviceName);
		}
	}

	if (stDeviceList.deviceCnt == 0)
		printf("  No available devices.\r\n");

	if (currentDeviceId == NULL)
	{
		printf("current: %s.\r\n", ret == RET_SUCCESS ? "none" : "unknown");
	}
	else if (!isCurrentListed)
	{
		printf("* %s  %s (not in available list)\r\n", currentDeviceId, stReply.szstDeviceInfo[0].deviceName);
	}

	return ret;
}

#elif USER_CMD_REPLY_METHOD == USER_CMD_REPLY_METHOD_ASYNC_REQUEST
static int AudioDevice(AHEADFORM_ALL_CTRL *pstAheadForm, int argc, char *argv[])
{
	AHEADFORM_AUDIO_CTRL *pstAudioCtrl = &pstAheadForm->stRobotCtrl.stMediaCtrl.stAudioCtrl;
	AHEADFORM_AUDIO_DEVICE_ASK stAsk;
	AHEADFORM_AUDIO_DEVICE_REPLY stReply;
	AHEADFORM_AUDIO_DEVICE_REPLY stDeviceList;
	AHEADFORM_AUDIO_DEVICE_ROUTE stRoute = {0};
	ENUM_AHEADFORM_AUDIO_DEVICE_OPERATION operation;
	ENUM_AHEADFORM_AUDIO_DEVICE_TYPE deviceType;
	const char *currentDeviceId = NULL;
	struct timeval stTimeout;
	fd_set rdFdSet;
	eventfd_t eventCnt;
	unsigned int i, requestIndex;
	int notifyEventfd, max_fd, cleanupRet, isCurrentListed = 0, ret;

	if ( (ret = IsUserInError(argc, argv, "2;3", NULL)) != RET_SUCCESS )
		return ret;

	if (strcasecmp(argv[1], "mic") == 0)
		deviceType = AHEADFORM_AUDIO_DEVICE_TYPE_MIC;
	else if (strcasecmp(argv[1], "spk") == 0)
		deviceType = AHEADFORM_AUDIO_DEVICE_TYPE_SPK;
	else
		return ERR_ARGV;

	if (argc == 3)
	{
		if (strlen(argv[2]) >= AHEADFORM_AUDIO_DEVICE_ID_LEN_MAX)
			return ERR_ARGV;

		if (deviceType == AHEADFORM_AUDIO_DEVICE_TYPE_MIC)
			strcpy(stRoute.micDeviceId, argv[2]);
		else
			strcpy(stRoute.spkDeviceId, argv[2]);
	}

	for (requestIndex = 0; requestIndex < (argc == 2 ? 2 : 1); requestIndex++)
	{
		if (argc == 3)
		{
			ret = Audio_BuildRouteAudioDeviceRequest(pstAudioCtrl, &stRoute, &stAsk);
		}
		else
		{
			operation = requestIndex == 0 ? AHEADFORM_AUDIO_DEVICE_OPERATION_GET_LIST : AHEADFORM_AUDIO_DEVICE_OPERATION_GET_CURRENT;
			ret = Audio_BuildQueryAudioDeviceRequest(pstAudioCtrl, operation, deviceType, &stAsk);
		}

		if (ret != RET_SUCCESS)
			break;

		if ( (notifyEventfd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC)) <= 0 )
		{
			printf("%s() call eventfd(audio device completion) failed: %s\r\n", __FUNCTION__, strerror(errno));
			ret = ERR_EVENTFD_CREATE_FAILED;
			break;
		}

		if ( (ret = Audio_SubmitAudioDeviceAsyncRequest(pstAudioCtrl, &stAsk, (unsigned long long)MEDIA_REQUEST_TIMEOUT_MS * 1000, notifyEventfd, &stReply)) == RET_SUCCESS )
		{
			stTimeout.tv_sec = MEDIA_REQUEST_TIMEOUT_MS / 1000;
			stTimeout.tv_usec = MEDIA_REQUEST_TIMEOUT_MS % 1000 * 1000;
			max_fd = notifyEventfd;
			while (1)
			{
				FD_ZERO(&rdFdSet);
				FD_SET(notifyEventfd, &rdFdSet);
				if ( (ret = select(max_fd + 1, &rdFdSet, NULL, NULL, &stTimeout)) < 0 )
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
					ret = 1;
					break;
				}

				if (stTimeout.tv_sec == 0 && stTimeout.tv_usec == 0)
					break;
			}
		}

		if ( (cleanupRet = Audio_CancelAudioDeviceAsyncRequest(pstAudioCtrl, notifyEventfd)) != RET_SUCCESS )
		{
			printf("%s() Audio_CancelAudioDeviceAsyncRequest(notifyEventfd = %d) failed, ret = 0x%08X; 退出 AheadFormSDK。\r\n", __FUNCTION__, notifyEventfd, cleanupRet);
			PrintAllErrLog();
			exit(EXIT_FAILURE);
		}

		if (ret == 1 || ret == 0 || ret == ERRCODE_REQUEST_TIMEOUT)
		{
			if (eventfd_read(notifyEventfd, &eventCnt) == 0)
			{
				ret = 1;
			}
			else if (errno == EAGAIN)
			{
				ret = ERR_REQUEST_TIMEOUT;
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
			ret = ERR_EVENTFD_IO_FAILED;
			break;
		}

		if (ret == 1)
			ret = stReply.retCode;

		if (ret != RET_SUCCESS)
			break;

		if (argc == 2)
		{
			if (!stReply.queryDataValid || (requestIndex == 1 && stReply.deviceCnt > 1))
			{
				ret = ERR_DATA_PACKET_FORMAT;
				break;
			}

			if (requestIndex == 0)
				stDeviceList = stReply;
		}
	}

	if (argc == 3)
	{
		if (ret == RET_SUCCESS)
			printf("%s device: %s.\r\n", argv[1], argv[2]);

		return ret;
	}

	if (requestIndex == 0)
		return ret;

	if (ret == RET_SUCCESS && stReply.deviceCnt == 1 && stReply.szstDeviceInfo[0].strDeviceId[0] != 0)
		currentDeviceId = stReply.szstDeviceInfo[0].strDeviceId;

	printf("%s devices (* current):\r\n", argv[1]);
	for (i = 0; i < stDeviceList.deviceCnt; i++)
	{
		if (currentDeviceId != NULL && strcmp(stDeviceList.szstDeviceInfo[i].strDeviceId, currentDeviceId) == 0)
		{
			printf("* %s  %s\r\n", stDeviceList.szstDeviceInfo[i].strDeviceId, stDeviceList.szstDeviceInfo[i].deviceName);
			isCurrentListed = 1;
		}
		else
		{
			printf("  %s  %s\r\n", stDeviceList.szstDeviceInfo[i].strDeviceId, stDeviceList.szstDeviceInfo[i].deviceName);
		}
	}

	if (stDeviceList.deviceCnt == 0)
		printf("  No available devices.\r\n");

	if (currentDeviceId == NULL)
	{
		printf("current: %s.\r\n", ret == RET_SUCCESS ? "none" : "unknown");
	}
	else if (!isCurrentListed)
	{
		printf("* %s  %s (not in available list)\r\n", currentDeviceId, stReply.szstDeviceInfo[0].deviceName);
	}

	return ret;
}

#elif USER_CMD_REPLY_METHOD == USER_CMD_REPLY_METHOD_READER_CALLBACK
typedef struct
{
	AHEADFORM_AUDIO_DEVICE_ASK stAsk;
	AHEADFORM_AUDIO_DEVICE_REPLY stReply;
	int completionEventfd;
	int isReplied;
} AUDIO_DEVICE_CALLBACK_CTX;

static void CopyAudioDeviceReplyIfMatchesReq(const void *pElem, void *pUserArg)
{
	const AHEADFORM_AUDIO_DEVICE_REPLY *pstReply = (const AHEADFORM_AUDIO_DEVICE_REPLY*)pElem;
	AUDIO_DEVICE_CALLBACK_CTX *pstCtx = (AUDIO_DEVICE_CALLBACK_CTX*)pUserArg;

	if (pstCtx->isReplied || Audio_MatchAudioDeviceReplyWithReq(pstReply, &pstCtx->stAsk) != RET_SUCCESS)
		return;

	pstCtx->stReply = *pstReply;
	pstCtx->isReplied = 1;
	if (eventfd_write(pstCtx->completionEventfd, 1) < 0 && errno != EAGAIN)
		PRINT_FUNCTION_ERRINFO(ERR_EVENTFD_IO_FAILED, __FUNCTION__, "call eventfd_write(completionEventfd = %d) failed: %s", pstCtx->completionEventfd, strerror(errno));
}

static int AudioDevice(AHEADFORM_ALL_CTRL *pstAheadForm, int argc, char *argv[])
{
	AHEADFORM_AUDIO_CTRL *pstAudioCtrl = &pstAheadForm->stRobotCtrl.stMediaCtrl.stAudioCtrl;
	AUDIO_DEVICE_CALLBACK_CTX stCtx = {.isReplied = 0};
	AHEADFORM_TOPIC_READER_PARA stReaderPara = {CopyAudioDeviceReplyIfMatchesReq, &stCtx};
	AHEADFORM_TOPIC_READER *pstReader = NULL;
	AHEADFORM_AUDIO_DEVICE_ASK stAsk;
	AHEADFORM_AUDIO_DEVICE_REPLY stReply;
	AHEADFORM_AUDIO_DEVICE_REPLY stDeviceList;
	AHEADFORM_AUDIO_DEVICE_ROUTE stRoute = {0};
	ENUM_AHEADFORM_AUDIO_DEVICE_OPERATION operation;
	ENUM_AHEADFORM_AUDIO_DEVICE_TYPE deviceType;
	const char *currentDeviceId = NULL;
	struct timeval stTimeout;
	fd_set rdFdSet;
	unsigned int i, requestIndex;
	int max_fd, cleanupRet, isCurrentListed = 0, ret;

	if ( (ret = IsUserInError(argc, argv, "2;3", NULL)) != RET_SUCCESS )
		return ret;

	if (strcasecmp(argv[1], "mic") == 0)
		deviceType = AHEADFORM_AUDIO_DEVICE_TYPE_MIC;
	else if (strcasecmp(argv[1], "spk") == 0)
		deviceType = AHEADFORM_AUDIO_DEVICE_TYPE_SPK;
	else
		return ERR_ARGV;

	if (argc == 3)
	{
		if (strlen(argv[2]) >= AHEADFORM_AUDIO_DEVICE_ID_LEN_MAX)
			return ERR_ARGV;

		if (deviceType == AHEADFORM_AUDIO_DEVICE_TYPE_MIC)
			strcpy(stRoute.micDeviceId, argv[2]);
		else
			strcpy(stRoute.spkDeviceId, argv[2]);
	}

	for (requestIndex = 0; requestIndex < (argc == 2 ? 2 : 1); requestIndex++)
	{
		if (argc == 3)
		{
			ret = Audio_BuildRouteAudioDeviceRequest(pstAudioCtrl, &stRoute, &stAsk);
		}
		else
		{
			operation = requestIndex == 0 ? AHEADFORM_AUDIO_DEVICE_OPERATION_GET_LIST : AHEADFORM_AUDIO_DEVICE_OPERATION_GET_CURRENT;
			ret = Audio_BuildQueryAudioDeviceRequest(pstAudioCtrl, operation, deviceType, &stAsk);
		}

		if (ret != RET_SUCCESS)
			break;

		stCtx.stAsk = stAsk;
		stCtx.isReplied = 0;
		if ( (stCtx.completionEventfd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC)) <= 0 )
		{
			printf("%s() call eventfd(audio device completion) failed: %s\r\n", __FUNCTION__, strerror(errno));
			ret = ERR_EVENTFD_CREATE_FAILED;
			break;
		}

		if ( (pstReader = Audio_RegisterAudioDeviceReader(pstAudioCtrl, &stReaderPara)) == NULL )
		{
			ret = ERRCODE_INIT_FAIL;
			if (close(stCtx.completionEventfd) < 0)
			{
				printf("%s() call close(completionEventfd = %d) failed: %s\r\n", __FUNCTION__, stCtx.completionEventfd, strerror(errno));
				ret = ERR_EVENTFD_IO_FAILED;
			}
			break;
		}

		if ( (ret = Audio_SendAudioDeviceRequest(pstAudioCtrl, &stCtx.stAsk)) == RET_SUCCESS )
		{
			stTimeout.tv_sec = MEDIA_REQUEST_TIMEOUT_MS / 1000;
			stTimeout.tv_usec = MEDIA_REQUEST_TIMEOUT_MS % 1000 * 1000;
			max_fd = stCtx.completionEventfd;
			while (1)
			{
				FD_ZERO(&rdFdSet);
				FD_SET(stCtx.completionEventfd, &rdFdSet);
				if ( (ret = select(max_fd + 1, &rdFdSet, NULL, NULL, &stTimeout)) < 0 )
				{
					printf("%s() call select(completionEventfd = %d) failed: %s\r\n", __FUNCTION__, stCtx.completionEventfd, strerror(errno));
					ret = ERR_SELECT;
					break;
				}
				else if (ret == 0)
				{
					break;
				}
				ret = RET_SUCCESS;
				if (FD_ISSET(stCtx.completionEventfd, &rdFdSet))
					break;

				if (stTimeout.tv_sec == 0 && stTimeout.tv_usec == 0)
					break;
			}
		}

		if ( (cleanupRet = Audio_UnregisterAudioDeviceReader(pstAudioCtrl, pstReader)) != RET_SUCCESS )
		{
			printf("%s() Audio_UnregisterAudioDeviceReader(Reader = %p) failed, ret = 0x%08X; 退出 AheadFormSDK。\r\n", __FUNCTION__, (void*)pstReader, cleanupRet);
			PrintAllErrLog();
			exit(EXIT_FAILURE);
		}
		pstReader = NULL;
		if (close(stCtx.completionEventfd) < 0)
		{
			printf("%s() call close(completionEventfd = %d) failed: %s\r\n", __FUNCTION__, stCtx.completionEventfd, strerror(errno));
			ret = ERR_EVENTFD_IO_FAILED;
			break;
		}

		if (ret != RET_SUCCESS)
			break;

		if (!stCtx.isReplied)
		{
			ret = ERR_REQUEST_TIMEOUT;
			break;
		}
		stReply = stCtx.stReply;
		ret = stReply.retCode;
		if (ret != RET_SUCCESS)
			break;

		if (argc == 2)
		{
			if (!stReply.queryDataValid || (requestIndex == 1 && stReply.deviceCnt > 1))
			{
				ret = ERR_DATA_PACKET_FORMAT;
				break;
			}

			if (requestIndex == 0)
				stDeviceList = stReply;
		}
	}

	if (argc == 3)
	{
		if (ret == RET_SUCCESS)
			printf("%s device: %s.\r\n", argv[1], argv[2]);

		return ret;
	}

	if (requestIndex == 0)
		return ret;

	if (ret == RET_SUCCESS && stReply.deviceCnt == 1 && stReply.szstDeviceInfo[0].strDeviceId[0] != 0)
		currentDeviceId = stReply.szstDeviceInfo[0].strDeviceId;

	printf("%s devices (* current):\r\n", argv[1]);
	for (i = 0; i < stDeviceList.deviceCnt; i++)
	{
		if (currentDeviceId != NULL && strcmp(stDeviceList.szstDeviceInfo[i].strDeviceId, currentDeviceId) == 0)
		{
			printf("* %s  %s\r\n", stDeviceList.szstDeviceInfo[i].strDeviceId, stDeviceList.szstDeviceInfo[i].deviceName);
			isCurrentListed = 1;
		}
		else
		{
			printf("  %s  %s\r\n", stDeviceList.szstDeviceInfo[i].strDeviceId, stDeviceList.szstDeviceInfo[i].deviceName);
		}
	}

	if (stDeviceList.deviceCnt == 0)
		printf("  No available devices.\r\n");

	if (currentDeviceId == NULL)
	{
		printf("current: %s.\r\n", ret == RET_SUCCESS ? "none" : "unknown");
	}
	else if (!isCurrentListed)
	{
		printf("* %s  %s (not in available list)\r\n", currentDeviceId, stReply.szstDeviceInfo[0].deviceName);
	}

	return ret;
}

#endif

//Allow the robot's 5-second LLM reply timeout to reach the menu.
#define OPERATING_MODE_SET_TIMEOUT_MS 6000

#if USER_CMD_REPLY_METHOD == USER_CMD_REPLY_METHOD_RX_EVENTFD
static int OperatingMode(AHEADFORM_ALL_CTRL *pstAheadForm, int argc, char *argv[])
{
	AHEADFORM_TOPIC_READER_PARA stReaderPara = {0};
	AHEADFORM_TOPIC_READER *pstReader = NULL;
	AHEADFORM_OPERATING_MODE_ASK stAsk;
	AHEADFORM_OPERATING_MODE_REPLY stReply;
	AHEADFORM_OPERATING_MODE_STATE stState;
	ENUM_AHEADFORM_OPERATING_MODE mode;
	struct timeval stTimeout;
	fd_set rdFdSet;
	eventfd_t eventCnt;
	unsigned int i, readableElemCnt;
	int max_fd, cleanupRet, ret;

	if ( (ret = IsUserInError(argc, argv, "1,2", NULL)) != RET_SUCCESS )
		return ret;

	if (argc == 1)
	{
		if ( (ret = OperatingMode_ReadStateSync(&pstAheadForm->stRobotCtrl.stOperatingModeCtrl, &stState, CTRL_REQUEST_TIMEOUT_MS)) != RET_SUCCESS )
			return ret;

		printf("operating mode receive=%llu us, mode=%s.\r\n", stState.ReceiveTime_us, stState.mode == AHEADFORM_OPERATING_MODE_DEV ? "dev" : "interactive");
		return RET_SUCCESS;
	}

	if (strcasecmp(argv[1], "dev") == 0)
		mode = AHEADFORM_OPERATING_MODE_DEV;
	else if (strcasecmp(argv[1], "interactive") == 0)
		mode = AHEADFORM_OPERATING_MODE_INTERACTIVE;
	else
		return ERR_ARGV;

	if ( (ret = OperatingMode_BuildRequest(&pstAheadForm->stRobotCtrl.stOperatingModeCtrl, mode, &stAsk)) != RET_SUCCESS )
		return ret;

	if ( (pstReader = OperatingMode_RegisterReader(&pstAheadForm->stRobotCtrl.stOperatingModeCtrl, &stReaderPara)) == NULL )
		return ERRCODE_INIT_FAIL;

	if ( (ret = OperatingMode_SendRequest(&pstAheadForm->stRobotCtrl.stOperatingModeCtrl, &stAsk)) == RET_SUCCESS )
	{
		stTimeout.tv_sec = OPERATING_MODE_SET_TIMEOUT_MS / 1000;
		stTimeout.tv_usec = OPERATING_MODE_SET_TIMEOUT_MS % 1000 * 1000;
		max_fd = pstReader->rxDataUpdateEventfd;
		while (1)
		{
			FD_ZERO(&rdFdSet);
			FD_SET(pstReader->rxDataUpdateEventfd, &rdFdSet);
			if ( (ret = select(max_fd + 1, &rdFdSet, NULL, NULL, &stTimeout)) < 0 )
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
				readableElemCnt = TopicReader_GetReadableElemCnt(pstReader);
				for (i = 0; i < readableElemCnt; i++)
				{
					if ( (ret = TopicReader_PopElem(pstReader, &stReply)) <= 0 )
						break;

					if (OperatingMode_MatchReplyWithReq(&stReply, &stAsk) == RET_SUCCESS)
						break;

					ret = 0;
				}
			}

			if (ret != 0)
				break;

			if (stTimeout.tv_sec == 0 && stTimeout.tv_usec == 0)
				break;
		}
	}

	if ( (cleanupRet = OperatingMode_UnregisterReader(&pstAheadForm->stRobotCtrl.stOperatingModeCtrl, pstReader)) != RET_SUCCESS )
	{
		printf("%s() OperatingMode_UnregisterReader(Reader = %p) failed, ret = 0x%08X; exiting AheadFormSDK.\r\n", __FUNCTION__, (void*)pstReader, (unsigned int)cleanupRet);
		PrintAllErrLog();
		exit(EXIT_FAILURE);
	}

	if (ret == 0)
		return ERR_REQUEST_TIMEOUT;

	if (ret < 0)
		return ret;

	printf("operating mode receive=%llu us, requestId=%llu, result=%u.\r\n", stReply.ReceiveTime_us, stReply.requestId, (unsigned int)stReply.result);
	if (stReply.result == AHEADFORM_OPERATING_MODE_RESULT_SUCCEEDED)
		return RET_SUCCESS;

	if (stReply.result == AHEADFORM_OPERATING_MODE_RESULT_FAILED)
		return ERR_REQUEST_REJECTED;

	return ERR_DATA_PACKET_FORMAT;
}

#elif USER_CMD_REPLY_METHOD == USER_CMD_REPLY_METHOD_ASYNC_REQUEST
static int OperatingMode(AHEADFORM_ALL_CTRL *pstAheadForm, int argc, char *argv[])
{
	AHEADFORM_OPERATING_MODE_ASK stAsk;
	AHEADFORM_OPERATING_MODE_REPLY stReply;
	AHEADFORM_OPERATING_MODE_STATE stState;
	ENUM_AHEADFORM_OPERATING_MODE mode;
	struct timeval stTimeout;
	fd_set rdFdSet;
	eventfd_t eventCnt;
	int notifyEventfd, max_fd, cleanupRet, ret;

	if ( (ret = IsUserInError(argc, argv, "1,2", NULL)) != RET_SUCCESS )
		return ret;

	if (argc == 1)
	{
		if ( (ret = OperatingMode_ReadStateSync(&pstAheadForm->stRobotCtrl.stOperatingModeCtrl, &stState, CTRL_REQUEST_TIMEOUT_MS)) != RET_SUCCESS )
			return ret;

		printf("operating mode receive=%llu us, mode=%s.\r\n", stState.ReceiveTime_us, stState.mode == AHEADFORM_OPERATING_MODE_DEV ? "dev" : "interactive");
		return RET_SUCCESS;
	}

	if (strcasecmp(argv[1], "dev") == 0)
		mode = AHEADFORM_OPERATING_MODE_DEV;
	else if (strcasecmp(argv[1], "interactive") == 0)
		mode = AHEADFORM_OPERATING_MODE_INTERACTIVE;
	else
		return ERR_ARGV;

	if ( (ret = OperatingMode_BuildRequest(&pstAheadForm->stRobotCtrl.stOperatingModeCtrl, mode, &stAsk)) != RET_SUCCESS )
		return ret;

	if ( (notifyEventfd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC)) <= 0 )
	{
		printf("%s() call eventfd(operating mode completion) failed: %s\r\n", __FUNCTION__, strerror(errno));
		return ERR_EVENTFD_CREATE_FAILED;
	}

	if ( (ret = OperatingMode_SubmitAsyncRequest(&pstAheadForm->stRobotCtrl.stOperatingModeCtrl, &stAsk, (unsigned long long)OPERATING_MODE_SET_TIMEOUT_MS * 1000, notifyEventfd, &stReply)) == RET_SUCCESS )
	{
		stTimeout.tv_sec = OPERATING_MODE_SET_TIMEOUT_MS / 1000;
		stTimeout.tv_usec = OPERATING_MODE_SET_TIMEOUT_MS % 1000 * 1000;
		max_fd = notifyEventfd;
		while (1)
		{
			FD_ZERO(&rdFdSet);
			FD_SET(notifyEventfd, &rdFdSet);
			if ( (ret = select(max_fd + 1, &rdFdSet, NULL, NULL, &stTimeout)) < 0 )
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
				ret = 1;
				break;
			}

			if (stTimeout.tv_sec == 0 && stTimeout.tv_usec == 0)
				break;
		}
	}

	if ( (cleanupRet = OperatingMode_CancelAsyncRequest(&pstAheadForm->stRobotCtrl.stOperatingModeCtrl, notifyEventfd)) != RET_SUCCESS )
	{
		printf("%s() OperatingMode_CancelAsyncRequest(notifyEventfd = %d) failed, ret = 0x%08X; exiting AheadFormSDK.\r\n", __FUNCTION__, notifyEventfd, (unsigned int)cleanupRet);
		PrintAllErrLog();
		exit(EXIT_FAILURE);
	}

	if (ret == 1 || ret == 0 || ret == ERRCODE_REQUEST_TIMEOUT)
	{
		if (eventfd_read(notifyEventfd, &eventCnt) == 0)
		{
			ret = 1;
		}
		else if (errno == EAGAIN)
		{
			ret = ERR_REQUEST_TIMEOUT;
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

	if (ret != 1)
		return ret;

	printf("operating mode receive=%llu us, requestId=%llu, result=%u.\r\n", stReply.ReceiveTime_us, stReply.requestId, (unsigned int)stReply.result);
	if (stReply.result == AHEADFORM_OPERATING_MODE_RESULT_SUCCEEDED)
		return RET_SUCCESS;

	if (stReply.result == AHEADFORM_OPERATING_MODE_RESULT_FAILED)
		return ERR_REQUEST_REJECTED;

	return ERR_DATA_PACKET_FORMAT;
}

#elif USER_CMD_REPLY_METHOD == USER_CMD_REPLY_METHOD_READER_CALLBACK
typedef struct
{
	AHEADFORM_OPERATING_MODE_ASK stAsk;
	AHEADFORM_OPERATING_MODE_REPLY stReply;
	int completionEventfd;
	int isReplied;
} OPERATING_MODE_CALLBACK_CTX;

static void CopyOperatingModeReplyIfMatchesReq(const void *pElem, void *pUserArg)
{
	const AHEADFORM_OPERATING_MODE_REPLY *pstReply = (const AHEADFORM_OPERATING_MODE_REPLY*)pElem;
	OPERATING_MODE_CALLBACK_CTX *pstCtx = (OPERATING_MODE_CALLBACK_CTX*)pUserArg;

	if (pstCtx->isReplied || OperatingMode_MatchReplyWithReq(pstReply, &pstCtx->stAsk) != RET_SUCCESS)
		return;

	pstCtx->stReply = *pstReply;
	pstCtx->isReplied = 1;
	if (eventfd_write(pstCtx->completionEventfd, 1) < 0 && errno != EAGAIN)
		PRINT_FUNCTION_ERRINFO(ERR_EVENTFD_IO_FAILED, __FUNCTION__, "call eventfd_write(completionEventfd = %d) failed: %s", pstCtx->completionEventfd, strerror(errno));
}

static int OperatingMode(AHEADFORM_ALL_CTRL *pstAheadForm, int argc, char *argv[])
{
	OPERATING_MODE_CALLBACK_CTX stCtx = {.isReplied = 0};
	AHEADFORM_TOPIC_READER_PARA stReaderPara = {CopyOperatingModeReplyIfMatchesReq, &stCtx};
	AHEADFORM_TOPIC_READER *pstReader = NULL;
	AHEADFORM_OPERATING_MODE_STATE stState;
	ENUM_AHEADFORM_OPERATING_MODE mode;
	struct timeval stTimeout;
	fd_set rdFdSet;
	int max_fd, cleanupRet, ret;

	if ( (ret = IsUserInError(argc, argv, "1,2", NULL)) != RET_SUCCESS )
		return ret;

	if (argc == 1)
	{
		if ( (ret = OperatingMode_ReadStateSync(&pstAheadForm->stRobotCtrl.stOperatingModeCtrl, &stState, CTRL_REQUEST_TIMEOUT_MS)) != RET_SUCCESS )
			return ret;

		printf("operating mode receive=%llu us, mode=%s.\r\n", stState.ReceiveTime_us, stState.mode == AHEADFORM_OPERATING_MODE_DEV ? "dev" : "interactive");
		return RET_SUCCESS;
	}

	if (strcasecmp(argv[1], "dev") == 0)
		mode = AHEADFORM_OPERATING_MODE_DEV;
	else if (strcasecmp(argv[1], "interactive") == 0)
		mode = AHEADFORM_OPERATING_MODE_INTERACTIVE;
	else
		return ERR_ARGV;

	if ( (ret = OperatingMode_BuildRequest(&pstAheadForm->stRobotCtrl.stOperatingModeCtrl, mode, &stCtx.stAsk)) != RET_SUCCESS )
		return ret;

	if ( (stCtx.completionEventfd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC)) <= 0 )
	{
		printf("%s() call eventfd(operating mode completion) failed: %s\r\n", __FUNCTION__, strerror(errno));
		return ERR_EVENTFD_CREATE_FAILED;
	}

	if ( (pstReader = OperatingMode_RegisterReader(&pstAheadForm->stRobotCtrl.stOperatingModeCtrl, &stReaderPara)) == NULL )
	{
		if (close(stCtx.completionEventfd) < 0)
		{
			printf("%s() call close(completionEventfd = %d) failed: %s\r\n", __FUNCTION__, stCtx.completionEventfd, strerror(errno));
			return ERR_EVENTFD_IO_FAILED;
		}

		return ERRCODE_INIT_FAIL;
	}

	if ( (ret = OperatingMode_SendRequest(&pstAheadForm->stRobotCtrl.stOperatingModeCtrl, &stCtx.stAsk)) == RET_SUCCESS )
	{
		stTimeout.tv_sec = OPERATING_MODE_SET_TIMEOUT_MS / 1000;
		stTimeout.tv_usec = OPERATING_MODE_SET_TIMEOUT_MS % 1000 * 1000;
		max_fd = stCtx.completionEventfd;
		while (1)
		{
			FD_ZERO(&rdFdSet);
			FD_SET(stCtx.completionEventfd, &rdFdSet);
			if ( (ret = select(max_fd + 1, &rdFdSet, NULL, NULL, &stTimeout)) < 0 )
			{
				printf("%s() call select(completionEventfd = %d) failed: %s\r\n", __FUNCTION__, stCtx.completionEventfd, strerror(errno));
				ret = ERR_SELECT;
				break;
			}
			else if (ret == 0)
			{
				break;
			}
			ret = RET_SUCCESS;
			if (FD_ISSET(stCtx.completionEventfd, &rdFdSet))
				break;

			if (stTimeout.tv_sec == 0 && stTimeout.tv_usec == 0)
				break;
		}
	}

	if ( (cleanupRet = OperatingMode_UnregisterReader(&pstAheadForm->stRobotCtrl.stOperatingModeCtrl, pstReader)) != RET_SUCCESS )
	{
		printf("%s() OperatingMode_UnregisterReader(Reader = %p) failed, ret = 0x%08X; exiting AheadFormSDK.\r\n", __FUNCTION__, (void*)pstReader, (unsigned int)cleanupRet);
		PrintAllErrLog();
		exit(EXIT_FAILURE);
	}

	if (close(stCtx.completionEventfd) < 0)
	{
		printf("%s() call close(completionEventfd = %d) failed: %s\r\n", __FUNCTION__, stCtx.completionEventfd, strerror(errno));
		return ERR_EVENTFD_IO_FAILED;
	}

	if (ret != RET_SUCCESS)
		return ret;

	if (!stCtx.isReplied)
		return ERR_REQUEST_TIMEOUT;

	printf("operating mode receive=%llu us, requestId=%llu, result=%u.\r\n", stCtx.stReply.ReceiveTime_us, stCtx.stReply.requestId, (unsigned int)stCtx.stReply.result);
	if (stCtx.stReply.result == AHEADFORM_OPERATING_MODE_RESULT_SUCCEEDED)
		return RET_SUCCESS;

	if (stCtx.stReply.result == AHEADFORM_OPERATING_MODE_RESULT_FAILED)
		return ERR_REQUEST_REJECTED;

	return ERR_DATA_PACKET_FORMAT;
}

#endif

static int EmotePlayback(AHEADFORM_ALL_CTRL *pstAheadForm, int argc, char *argv[])
{
	AHEADFORM_EMOTE_CLIP stMotionClip, stAudioClip;
	AHEADFORM_EMOTE_CLIP *pstClip = NULL;
	AHEADFORM_EMOTE_ASK stAsk;
	AHEADFORM_EMOTE_REPLY stReply;
	AHEADFORM_EMOTE_EVENT stEvent;
	const char *strSuffix = NULL;
	size_t sourceLen;
	unsigned int motionClipCnt = 0, audioClipCnt = 0;
	unsigned char isLoop = 0;
	struct timeval stTimeout;
	fd_set rdFdSet;
	eventfd_t eventCnt;
	unsigned long long stateWaitStart_us, elapsed_us, remaining_us;
	int replyEventfd, stateEventfd, max_fd, cleanupRet, stateRet = 0;
	int i, fileCnt, ret;

	if ( (ret = IsUserInError(argc, argv, "2,3,4", NULL)) != RET_SUCCESS )
		return ret;

	fileCnt = argc - 1;
	if (strcasecmp(argv[argc - 1], "loop") == 0)
	{
		isLoop = 1;
		fileCnt--;
	}

	if (fileCnt < 1 || fileCnt > 2)
		return ERR_ARGC;

	for (i = 1; i < 1 + fileCnt; i++)
	{
		if ( (sourceLen = strnlen(argv[i], AHEADFORM_EMOTE_SOURCE_LEN_MAX)) == 0 || sourceLen >= AHEADFORM_EMOTE_SOURCE_LEN_MAX )
			return ERR_ARGV;

		if ( (strSuffix = strrchr(argv[i], '.')) == NULL || strSuffix == argv[i] )
			return ERR_ARGV;

		if (strcasecmp(strSuffix, ".csv") == 0)
		{
			if (motionClipCnt != 0)
				return ERR_ARGV;

			pstClip = &stMotionClip;
			motionClipCnt = 1;
		}
		else if (strcasecmp(strSuffix, ".mp3") == 0 || strcasecmp(strSuffix, ".wav") == 0)
		{
			if (audioClipCnt != 0)
				return ERR_ARGV;

			pstClip = &stAudioClip;
			audioClipCnt = 1;
		}
		else
		{
			return ERR_ARGV;
		}
		memcpy(pstClip->strSource, argv[i], sourceLen + 1);
		pstClip->startAt_ms = 0;
		pstClip->clipBegin_ms = 0;
		pstClip->clipEnd_ms = 0;
		pstClip->gapBehavior = AHEADFORM_EMOTE_GAP_DEFAULT;
	}

	if ( (ret = EmotePlay_BuildPlayRequest(&pstAheadForm->stRobotCtrl.stEmotePlayCtrl, AHEADFORM_EMOTE_SOURCE_NAME, motionClipCnt > 0 ? &stMotionClip : NULL, motionClipCnt, audioClipCnt > 0 ? &stAudioClip : NULL, audioClipCnt, isLoop, &stAsk)) != RET_SUCCESS )
		return ret;

	if ( (replyEventfd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC)) <= 0 )
	{
		printf("%s() call eventfd(emote reply completion) failed: %s\r\n", __FUNCTION__, strerror(errno));
		return ERR_EVENTFD_CREATE_FAILED;
	}

	if ( (stateEventfd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC)) <= 0 )
	{
		printf("%s() call eventfd(emote state completion) failed: %s\r\n", __FUNCTION__, strerror(errno));
		if (close(replyEventfd) < 0)
		{
			printf("%s() call close(replyEventfd = %d) failed: %s\r\n", __FUNCTION__, replyEventfd, strerror(errno));
			return ERR_EVENTFD_IO_FAILED;
		}

		return ERR_EVENTFD_CREATE_FAILED;
	}
	stateWaitStart_us = GetTimeUsCnt_UpToNow();
	if ( (ret = EmotePlay_RegisterGetStateAsyncRequest(&pstAheadForm->stRobotCtrl.stEmotePlayCtrl, stAsk.requestId, (unsigned long long)CTRL_REQUEST_TIMEOUT_MS * 1000, stateEventfd, &stEvent)) != RET_SUCCESS )
	{
		if (close(replyEventfd) < 0)
		{
			printf("%s() call close(replyEventfd = %d) failed: %s\r\n", __FUNCTION__, replyEventfd, strerror(errno));
			ret = ERR_EVENTFD_IO_FAILED;
		}

		if (close(stateEventfd) < 0)
		{
			printf("%s() call close(stateEventfd = %d) failed: %s\r\n", __FUNCTION__, stateEventfd, strerror(errno));
			ret = ERR_EVENTFD_IO_FAILED;
		}

		return ret;
	}

	if ( (ret = EmotePlay_SubmitAsyncRequest(&pstAheadForm->stRobotCtrl.stEmotePlayCtrl, &stAsk, (unsigned long long)CTRL_REQUEST_TIMEOUT_MS * 1000, replyEventfd, &stReply)) == RET_SUCCESS )
	{
		stTimeout.tv_sec = CTRL_REQUEST_TIMEOUT_MS / 1000;
		stTimeout.tv_usec = CTRL_REQUEST_TIMEOUT_MS % 1000 * 1000;
		max_fd = replyEventfd;
		FD_ZERO(&rdFdSet);
		FD_SET(replyEventfd, &rdFdSet);
		if ( (ret = select(max_fd + 1, &rdFdSet, NULL, NULL, &stTimeout)) < 0 )
		{
			printf("%s() call select(replyEventfd = %d) failed: %s\r\n", __FUNCTION__, replyEventfd, strerror(errno));
			ret = ERR_SELECT;
		}
		else if (ret == 0)
		{
			ret = ERR_REQUEST_TIMEOUT;
		}
		else if (FD_ISSET(replyEventfd, &rdFdSet))
		{
			ret = 1;
		}
	}

	if (ret != 1)
	{
		if ( (cleanupRet = EmotePlay_CancelAsyncRequest(&pstAheadForm->stRobotCtrl.stEmotePlayCtrl, replyEventfd)) != RET_SUCCESS )
		{
			printf("%s() EmotePlay_CancelAsyncRequest(replyEventfd = %d) failed, ret = 0x%08X; exiting AheadFormSDK.\r\n", __FUNCTION__, replyEventfd, (unsigned int)cleanupRet);
			PrintAllErrLog();
			exit(EXIT_FAILURE);
		}

		if (ret == ERRCODE_REQUEST_TIMEOUT)
		{
			if (eventfd_read(replyEventfd, &eventCnt) == RET_SUCCESS)
			{
				ret = 1;
			}
			else if (errno != EAGAIN)
			{
				printf("%s() call eventfd_read(replyEventfd = %d) failed: %s\r\n", __FUNCTION__, replyEventfd, strerror(errno));
				ret = ERR_EVENTFD_IO_FAILED;
			}
		}
	}

	if (close(replyEventfd) < 0)
	{
		printf("%s() call close(replyEventfd = %d) failed: %s\r\n", __FUNCTION__, replyEventfd, strerror(errno));
		ret = ERR_EVENTFD_IO_FAILED;
	}

	if (ret == 1)
	{
		printf("emote receive=%llu us, requestId=%llu, result=%u, reason=%s.\r\n", stReply.ReceiveTime_us, stReply.requestId, (unsigned int)stReply.result, stReply.strReason);
		ret = RET_SUCCESS;
		if (stReply.result == AHEADFORM_EMOTE_RESULT_SUCCEEDED)
		{
			elapsed_us = GetTimeUsCnt_UpToNow() - stateWaitStart_us;
			remaining_us = elapsed_us < (unsigned long long)CTRL_REQUEST_TIMEOUT_MS * 1000 ? (unsigned long long)CTRL_REQUEST_TIMEOUT_MS * 1000 - elapsed_us : 0;
			stTimeout.tv_sec = remaining_us / 1000000;
			stTimeout.tv_usec = remaining_us % 1000000;
			max_fd = stateEventfd;
			FD_ZERO(&rdFdSet);
			FD_SET(stateEventfd, &rdFdSet);
			if ( (stateRet = select(max_fd + 1, &rdFdSet, NULL, NULL, &stTimeout)) < 0 )
			{
				printf("%s() call select(stateEventfd = %d) failed: %s\r\n", __FUNCTION__, stateEventfd, strerror(errno));
				ret = ERR_SELECT;
			}
			else if (stateRet > 0)
			{
				stateRet = FD_ISSET(stateEventfd, &rdFdSet) ? 1 : 0;
			}
		}
	}

	if (stateRet != 1)
	{
		if ( (cleanupRet = EmotePlay_CancelGetStateAsyncRequest(&pstAheadForm->stRobotCtrl.stEmotePlayCtrl, stateEventfd)) != RET_SUCCESS )
		{
			printf("%s() EmotePlay_CancelGetStateAsyncRequest(stateEventfd = %d) failed, ret = 0x%08X; exiting AheadFormSDK.\r\n", __FUNCTION__, stateEventfd, (unsigned int)cleanupRet);
			PrintAllErrLog();
			exit(EXIT_FAILURE);
		}

		if (ret == RET_SUCCESS && stReply.result == AHEADFORM_EMOTE_RESULT_SUCCEEDED)
		{
			if (eventfd_read(stateEventfd, &eventCnt) == RET_SUCCESS)
			{
				stateRet = 1;
			}
			else if (errno != EAGAIN)
			{
				printf("%s() call eventfd_read(stateEventfd = %d) failed: %s\r\n", __FUNCTION__, stateEventfd, strerror(errno));
				ret = ERR_EVENTFD_IO_FAILED;
			}
		}
	}

	if (close(stateEventfd) < 0)
	{
		printf("%s() call close(stateEventfd = %d) failed: %s\r\n", __FUNCTION__, stateEventfd, strerror(errno));
		ret = ERR_EVENTFD_IO_FAILED;
	}

	if (ret != RET_SUCCESS)
		return ret;

	if (stReply.result == AHEADFORM_EMOTE_RESULT_FAILED)
		return ERR_REQUEST_REJECTED;

	if (stReply.result != AHEADFORM_EMOTE_RESULT_SUCCEEDED)
		return ERR_DATA_PACKET_FORMAT;

	if (stateRet == 1)
	{
		printf("emote state receive=%llu us, requestId=%llu, state=%s.\r\n", stEvent.ReceiveTime_us, stEvent.requestId, stEvent.strState);
		printf("emote state reason=%s, source=%s, loop=%u, reset=%u.\r\n", stEvent.strReason, stEvent.strSource, stEvent.isLoop, stEvent.isReset);
	}
	else
	{
		printf("emote requestId=%llu: no playback state received within %d ms.\r\n", stAsk.requestId, CTRL_REQUEST_TIMEOUT_MS);
	}

	return RET_SUCCESS;
}

#if USER_CMD_REPLY_METHOD == USER_CMD_REPLY_METHOD_RX_EVENTFD
static int EmotePlayCtrl(AHEADFORM_ALL_CTRL *pstAheadForm, int argc, char *argv[])
{
	AHEADFORM_TOPIC_READER_PARA stReaderPara = {0};
	AHEADFORM_TOPIC_READER *pstReader = NULL;
	AHEADFORM_EMOTE_ASK stAsk;
	ENUM_AHEADFORM_EMOTE_CMD cmd;
	AHEADFORM_EMOTE_REPLY stReply;
	struct timeval stTimeout;
	fd_set rdFdSet;
	eventfd_t eventCnt;
	unsigned int i, readableElemCnt;
	int max_fd, cleanupRet, ret;

	if ( (ret = IsUserInError(argc, argv, "2", NULL)) != RET_SUCCESS )
		return ret;

	if (strcasecmp(argv[1], "pause") == 0)
		cmd = AHEADFORM_EMOTE_CMD_PAUSE;
	else if (strcasecmp(argv[1], "resume") == 0)
		cmd = AHEADFORM_EMOTE_CMD_RESUME;
	else if (strcasecmp(argv[1], "stop") == 0)
		cmd = AHEADFORM_EMOTE_CMD_STOP;
	else
		return ERR_ARGV;

	if ( (ret = EmotePlay_BuildCtrlRequest(&pstAheadForm->stRobotCtrl.stEmotePlayCtrl, cmd, &stAsk)) != RET_SUCCESS )
		return ret;

	if ( (pstReader = EmotePlay_RegisterReplyReader(&pstAheadForm->stRobotCtrl.stEmotePlayCtrl, &stReaderPara)) == NULL )
		return ERRCODE_INIT_FAIL;

	if ( (ret = EmotePlay_SendRequest(&pstAheadForm->stRobotCtrl.stEmotePlayCtrl, &stAsk)) == RET_SUCCESS )
	{
		stTimeout.tv_sec = CTRL_REQUEST_TIMEOUT_MS / 1000;
		stTimeout.tv_usec = CTRL_REQUEST_TIMEOUT_MS % 1000 * 1000;
		max_fd = pstReader->rxDataUpdateEventfd;
		while (1)
		{
			FD_ZERO(&rdFdSet);
			FD_SET(pstReader->rxDataUpdateEventfd, &rdFdSet);
			if ( (ret = select(max_fd + 1, &rdFdSet, NULL, NULL, &stTimeout)) < 0 )
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
				readableElemCnt = TopicReader_GetReadableElemCnt(pstReader);
				for (i = 0; i < readableElemCnt; i++)
				{
					if ( (ret = TopicReader_PopElem(pstReader, &stReply)) <= 0 )
						break;

					if (stReply.requestId == stAsk.requestId)
						break;

					ret = 0;
				}
			}

			if (ret != 0)
				break;

			if (stTimeout.tv_sec == 0 && stTimeout.tv_usec == 0)
				break;
		}
	}

	if ( (cleanupRet = EmotePlay_UnregisterReplyReader(&pstAheadForm->stRobotCtrl.stEmotePlayCtrl, pstReader)) != RET_SUCCESS )
	{
		printf("%s() EmotePlay_UnregisterReplyReader(Reader = %p) failed, ret = 0x%08X; exiting AheadFormSDK.\r\n", __FUNCTION__, (void*)pstReader, (unsigned int)cleanupRet);
		PrintAllErrLog();
		exit(EXIT_FAILURE);
	}

	if (ret == 0)
		return ERR_REQUEST_TIMEOUT;

	if (ret < 0)
		return ret;

	printf("emote receive=%llu us, requestId=%llu, result=%u, reason=%s.\r\n", stReply.ReceiveTime_us, stReply.requestId, (unsigned int)stReply.result, stReply.strReason);
	if (stReply.result == AHEADFORM_EMOTE_RESULT_SUCCEEDED)
		return RET_SUCCESS;

	if (stReply.result == AHEADFORM_EMOTE_RESULT_FAILED)
		return ERR_REQUEST_REJECTED;

	return ERR_DATA_PACKET_FORMAT;
}

#elif USER_CMD_REPLY_METHOD == USER_CMD_REPLY_METHOD_ASYNC_REQUEST
static int EmotePlayCtrl(AHEADFORM_ALL_CTRL *pstAheadForm, int argc, char *argv[])
{
	AHEADFORM_EMOTE_ASK stAsk;
	ENUM_AHEADFORM_EMOTE_CMD cmd;
	AHEADFORM_EMOTE_REPLY stReply;
	struct timeval stTimeout;
	fd_set rdFdSet;
	eventfd_t eventCnt;
	int notifyEventfd, max_fd, cleanupRet, ret;

	if ( (ret = IsUserInError(argc, argv, "2", NULL)) != RET_SUCCESS )
		return ret;

	if (strcasecmp(argv[1], "pause") == 0)
		cmd = AHEADFORM_EMOTE_CMD_PAUSE;
	else if (strcasecmp(argv[1], "resume") == 0)
		cmd = AHEADFORM_EMOTE_CMD_RESUME;
	else if (strcasecmp(argv[1], "stop") == 0)
		cmd = AHEADFORM_EMOTE_CMD_STOP;
	else
		return ERR_ARGV;

	if ( (ret = EmotePlay_BuildCtrlRequest(&pstAheadForm->stRobotCtrl.stEmotePlayCtrl, cmd, &stAsk)) != RET_SUCCESS )
		return ret;

	if ( (notifyEventfd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC)) <= 0 )
	{
		printf("%s() call eventfd(emote completion) failed: %s\r\n", __FUNCTION__, strerror(errno));
		return ERR_EVENTFD_CREATE_FAILED;
	}

	if ( (ret = EmotePlay_SubmitAsyncRequest(&pstAheadForm->stRobotCtrl.stEmotePlayCtrl, &stAsk, (unsigned long long)CTRL_REQUEST_TIMEOUT_MS * 1000, notifyEventfd, &stReply)) == RET_SUCCESS )
	{
		stTimeout.tv_sec = CTRL_REQUEST_TIMEOUT_MS / 1000;
		stTimeout.tv_usec = CTRL_REQUEST_TIMEOUT_MS % 1000 * 1000;
		max_fd = notifyEventfd;
		while (1)
		{
			FD_ZERO(&rdFdSet);
			FD_SET(notifyEventfd, &rdFdSet);
			if ( (ret = select(max_fd + 1, &rdFdSet, NULL, NULL, &stTimeout)) < 0 )
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
				ret = 1;
				break;
			}

			if (stTimeout.tv_sec == 0 && stTimeout.tv_usec == 0)
				break;
		}
	}

	if (ret != 1)
	{
		if ( (cleanupRet = EmotePlay_CancelAsyncRequest(&pstAheadForm->stRobotCtrl.stEmotePlayCtrl, notifyEventfd)) != RET_SUCCESS )
		{
			printf("%s() EmotePlay_CancelAsyncRequest(notifyEventfd = %d) failed, ret = 0x%08X; exiting AheadFormSDK.\r\n", __FUNCTION__, notifyEventfd, (unsigned int)cleanupRet);
			PrintAllErrLog();
			exit(EXIT_FAILURE);
		}

		if (ret == 0 || ret == ERRCODE_REQUEST_TIMEOUT)
		{
			if (eventfd_read(notifyEventfd, &eventCnt) == RET_SUCCESS)
			{
				ret = 1;
			}
			else if (errno == EAGAIN)
			{
				ret = ERR_REQUEST_TIMEOUT;
			}
			else
			{
				printf("%s() call eventfd_read(notifyEventfd = %d) failed: %s\r\n", __FUNCTION__, notifyEventfd, strerror(errno));
				ret = ERR_EVENTFD_IO_FAILED;
			}
		}
	}

	if (close(notifyEventfd) < 0)
	{
		printf("%s() call close(notifyEventfd = %d) failed: %s\r\n", __FUNCTION__, notifyEventfd, strerror(errno));
		return ERR_EVENTFD_IO_FAILED;
	}

	if (ret != 1)
		return ret;

	printf("emote receive=%llu us, requestId=%llu, result=%u, reason=%s.\r\n", stReply.ReceiveTime_us, stReply.requestId, (unsigned int)stReply.result, stReply.strReason);
	if (stReply.result == AHEADFORM_EMOTE_RESULT_SUCCEEDED)
		return RET_SUCCESS;

	if (stReply.result == AHEADFORM_EMOTE_RESULT_FAILED)
		return ERR_REQUEST_REJECTED;

	return ERR_DATA_PACKET_FORMAT;
}

#elif USER_CMD_REPLY_METHOD == USER_CMD_REPLY_METHOD_READER_CALLBACK
typedef struct
{
	unsigned long long requestId;
	AHEADFORM_EMOTE_REPLY stReply;
	int completionEventfd;
	int isReplied;
} EMOTE_REPLY_CALLBACK_CTX;

static void CopyEmoteReplyIfMatchesReq(const void *pElem, void *pUserArg)
{
	const AHEADFORM_EMOTE_REPLY *pstReply = (const AHEADFORM_EMOTE_REPLY*)pElem;
	EMOTE_REPLY_CALLBACK_CTX *pstCtx = (EMOTE_REPLY_CALLBACK_CTX*)pUserArg;

	if (pstCtx->isReplied || pstReply->requestId != pstCtx->requestId)
		return;

	pstCtx->stReply = *pstReply;
	pstCtx->isReplied = 1;
	if (eventfd_write(pstCtx->completionEventfd, 1) < 0 && errno != EAGAIN)
		PRINT_FUNCTION_ERRINFO(ERR_EVENTFD_IO_FAILED, __FUNCTION__, "call eventfd_write(completionEventfd = %d) failed: %s", pstCtx->completionEventfd, strerror(errno));
}

static int EmotePlayCtrl(AHEADFORM_ALL_CTRL *pstAheadForm, int argc, char *argv[])
{
	EMOTE_REPLY_CALLBACK_CTX stCtx;
	AHEADFORM_TOPIC_READER_PARA stReaderPara = {CopyEmoteReplyIfMatchesReq, &stCtx};
	AHEADFORM_TOPIC_READER *pstReader = NULL;
	AHEADFORM_EMOTE_ASK stAsk;
	ENUM_AHEADFORM_EMOTE_CMD cmd;
	struct timeval stTimeout;
	fd_set rdFdSet;
	int max_fd, cleanupRet, ret;

	if ( (ret = IsUserInError(argc, argv, "2", NULL)) != RET_SUCCESS )
		return ret;

	if (strcasecmp(argv[1], "pause") == 0)
		cmd = AHEADFORM_EMOTE_CMD_PAUSE;
	else if (strcasecmp(argv[1], "resume") == 0)
		cmd = AHEADFORM_EMOTE_CMD_RESUME;
	else if (strcasecmp(argv[1], "stop") == 0)
		cmd = AHEADFORM_EMOTE_CMD_STOP;
	else
		return ERR_ARGV;

	if ( (ret = EmotePlay_BuildCtrlRequest(&pstAheadForm->stRobotCtrl.stEmotePlayCtrl, cmd, &stAsk)) != RET_SUCCESS )
		return ret;

	stCtx.requestId = stAsk.requestId;
	stCtx.isReplied = 0;
	if ( (stCtx.completionEventfd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC)) <= 0 )
	{
		printf("%s() call eventfd(emote completion) failed: %s\r\n", __FUNCTION__, strerror(errno));
		return ERR_EVENTFD_CREATE_FAILED;
	}

	if ( (pstReader = EmotePlay_RegisterReplyReader(&pstAheadForm->stRobotCtrl.stEmotePlayCtrl, &stReaderPara)) == NULL )
	{
		if (close(stCtx.completionEventfd) < 0)
		{
			printf("%s() call close(completionEventfd = %d) failed: %s\r\n", __FUNCTION__, stCtx.completionEventfd, strerror(errno));
			return ERR_EVENTFD_IO_FAILED;
		}

		return ERRCODE_INIT_FAIL;
	}

	if ( (ret = EmotePlay_SendRequest(&pstAheadForm->stRobotCtrl.stEmotePlayCtrl, &stAsk)) == RET_SUCCESS )
	{
		stTimeout.tv_sec = CTRL_REQUEST_TIMEOUT_MS / 1000;
		stTimeout.tv_usec = CTRL_REQUEST_TIMEOUT_MS % 1000 * 1000;
		max_fd = stCtx.completionEventfd;
		while (1)
		{
			FD_ZERO(&rdFdSet);
			FD_SET(stCtx.completionEventfd, &rdFdSet);
			if ( (ret = select(max_fd + 1, &rdFdSet, NULL, NULL, &stTimeout)) < 0 )
			{
				printf("%s() call select(completionEventfd = %d) failed: %s\r\n", __FUNCTION__, stCtx.completionEventfd, strerror(errno));
				ret = ERR_SELECT;
				break;
			}
			else if (ret == 0)
			{
				break;
			}
			ret = RET_SUCCESS;
			if (FD_ISSET(stCtx.completionEventfd, &rdFdSet))
				break;

			if (stTimeout.tv_sec == 0 && stTimeout.tv_usec == 0)
				break;
		}
	}

	if ( (cleanupRet = EmotePlay_UnregisterReplyReader(&pstAheadForm->stRobotCtrl.stEmotePlayCtrl, pstReader)) != RET_SUCCESS )
	{
		printf("%s() EmotePlay_UnregisterReplyReader(Reader = %p) failed, ret = 0x%08X; exiting AheadFormSDK.\r\n", __FUNCTION__, (void*)pstReader, (unsigned int)cleanupRet);
		PrintAllErrLog();
		exit(EXIT_FAILURE);
	}

	if (close(stCtx.completionEventfd) < 0)
	{
		printf("%s() call close(completionEventfd = %d) failed: %s\r\n", __FUNCTION__, stCtx.completionEventfd, strerror(errno));
		return ERR_EVENTFD_IO_FAILED;
	}

	if (ret != RET_SUCCESS)
		return ret;

	if (!stCtx.isReplied)
		return ERR_REQUEST_TIMEOUT;

	printf("emote receive=%llu us, requestId=%llu, result=%u, reason=%s.\r\n", stCtx.stReply.ReceiveTime_us, stCtx.stReply.requestId, (unsigned int)stCtx.stReply.result, stCtx.stReply.strReason);
	if (stCtx.stReply.result == AHEADFORM_EMOTE_RESULT_SUCCEEDED)
		return RET_SUCCESS;

	if (stCtx.stReply.result == AHEADFORM_EMOTE_RESULT_FAILED)
		return ERR_REQUEST_REJECTED;

	return ERR_DATA_PACKET_FORMAT;
}

#endif

static void PrintSensorState(const void *pElem, void *pUserArg)
{
	const AHEADFORM_SENSOR_STATE *pstState = (const AHEADFORM_SENSOR_STATE*)pElem;

	printf("sensor receive=%llu us, state=%u, errCode=0x%08X, ready=%u.\r\n", pstState->ReceiveTime_us, (unsigned int)pstState->state, (unsigned int)pstState->errCode, pstState->isReady);
	printf("sensor statusSeq=%llu, statusBuildTime_us=%llu.\r\n", pstState->statusSeq, pstState->statusBuildTime_us);
	printf("sensor imuRpyFeedbackAge_ms=%u.\r\n", pstState->imuRpyFeedbackAge_ms);
	printf("sensor roll_rad=%.6f, pitch_rad=%.6f, yaw_rad=%.6f.\r\n", pstState->roll_rad, pstState->pitch_rad, pstState->yaw_rad);
}

static void PrintNeckState(const void *pElem, void *pUserArg)
{
	const AHEADFORM_NECK_STATE *pstState = (const AHEADFORM_NECK_STATE*)pElem;
	const AHEADFORM_NECK_MOTOR_SAMPLE *pstMotor = NULL;
	unsigned int i;

	printf("neck receive=%llu us, state=%u, errCode=%d, ready=%u, faultActive=%u.\r\n", pstState->ReceiveTime_us, (unsigned int)pstState->state, pstState->errCode, pstState->isReady, pstState->isFaultActive);
	printf("neck lastAcceptedTargetId=%llu, targetReached=%u, poseValid=%u, motorCnt=%u.\r\n", pstState->lastAcceptedTargetId, pstState->isTargetReached, pstState->isPoseValid, pstState->motorCnt);
	if (pstState->isPoseValid)
		printf("neck roll_rad=%.6f, pitch_rad=%.6f, yaw_rad=%.6f.\r\n", pstState->stPose.roll_rad, pstState->stPose.pitch_rad, pstState->stPose.yaw_rad);

	for (i = 0; i < pstState->motorCnt; i++)
	{
		pstMotor = &pstState->szstMotor[i];
		printf("motor[%u] id=%u, temperatureValid=%u, currentValid=%u, voltageValid=%u.\r\n", i, pstMotor->motorId, pstMotor->isTemperatureValid, pstMotor->isCurrentValid, pstMotor->isVoltageValid);
		printf("motor[%u] motorFaultCodeValid=%u, forceValid=%u.\r\n", i, pstMotor->isMotorFaultCodeValid, pstMotor->isForceValid);
		if (pstMotor->isTemperatureValid)
			printf("motor[%u] Temperature_C=%g.\r\n", i, pstMotor->Temperature_C);

		if (pstMotor->isCurrentValid)
			printf("motor[%u] Current_A=%g.\r\n", i, pstMotor->Current_A);

		if (pstMotor->isVoltageValid)
			printf("motor[%u] Voltage_V=%g.\r\n", i, pstMotor->Voltage_V);

		if (pstMotor->isMotorFaultCodeValid)
			printf("motor[%u] motorFaultCode=%d.\r\n", i, pstMotor->motorFaultCode);

		if (pstMotor->isForceValid)
			printf("motor[%u] Force_N=%g.\r\n", i, pstMotor->Force_N);
	}
}

static void PrintFaultPool(const void *pElem, void *pUserArg)
{
	const AHEADFORM_FAULT_POOL *pstPool = (const AHEADFORM_FAULT_POOL*)pElem;
	const AHEADFORM_FAULT_RECORD *pstFault = NULL;
	unsigned long long age_us;
	unsigned int i;

	age_us = GetTimeUsCnt_UpToNow() - pstPool->ReceiveTime_us;
	printf("fault pool receive=%llu us, faultCnt=%u, age_us=%llu, stale=%u.\r\n", pstPool->ReceiveTime_us, pstPool->faultCnt, age_us, (unsigned int)(age_us >= 3000000));
	for (i = 0; i < pstPool->faultCnt; i++)
	{
		pstFault = &pstPool->szstFault[i];
		printf("fault[%u] source=%s, processFaultCode=0x%08X, level=%u, motorFaultCode=0x%08X.\r\n", i, pstFault->source, pstFault->processFaultCode, pstFault->level, pstFault->motorFaultCode);
		printf("fault[%u] timestamp=%s, reason=%s.\r\n", i, pstFault->timestamp, pstFault->reason);
	}
}

typedef union
{
	AHEADFORM_SENSOR_STATE szstSensorState[AHEADFORM_SENSOR_STATE_FIFO_ELEM_CNT / 2 - 1];
	AHEADFORM_NECK_STATE szstNeckState[AHEADFORM_NECK_STATE_FIFO_ELEM_CNT / 2 - 1];
	AHEADFORM_FAULT_POOL szstFaultPool[AHEADFORM_FAULT_POOL_FIFO_ELEM_CNT / 2 - 1];
} UNION_USER_CMD_RECENT_STATE;

static int ReadRecentState(AHEADFORM_ALL_CTRL *pstAheadForm, int argc, char *argv[])
{
	static UNION_USER_CMD_RECENT_STATE uState;
	AHEADFORM_TOPIC_READER_CALLBACK PrintState = NULL;
	unsigned char *pElemAryBuf = NULL;
	char *pEnd = NULL;
	size_t sizeOf1Elem;
	unsigned long long stateCnt;
	unsigned int i;
	int ret;

	if ( (ret = IsUserInError(argc, argv, "3", "2")) != RET_SUCCESS )
		return ret;

	if (argv[2][0] < '0' || argv[2][0] > '9')
		return ERR_ARGV;

	stateCnt = strtoull(argv[2], &pEnd, 10);
	if (pEnd == argv[2] || *pEnd != '\0' || stateCnt == 0 || stateCnt > 499)
		return ERR_ARGV;

	if (strcasecmp(argv[1], "sensor") == 0)
	{
		pElemAryBuf = (unsigned char*)uState.szstSensorState;
		sizeOf1Elem = sizeof(AHEADFORM_SENSOR_STATE);
		PrintState = PrintSensorState;
		ret = Status_ReadSensorRecentState(&pstAheadForm->stRobotCtrl.stStatus, uState.szstSensorState, (unsigned int)stateCnt);
	}
	else if (strcasecmp(argv[1], "neck") == 0)
	{
		pElemAryBuf = (unsigned char*)uState.szstNeckState;
		sizeOf1Elem = sizeof(AHEADFORM_NECK_STATE);
		PrintState = PrintNeckState;
		ret = Status_ReadNeckRecentState(&pstAheadForm->stRobotCtrl.stStatus, uState.szstNeckState, (unsigned int)stateCnt);
	}
	else if (strcasecmp(argv[1], "faults") == 0)
	{
		pElemAryBuf = (unsigned char*)uState.szstFaultPool;
		sizeOf1Elem = sizeof(AHEADFORM_FAULT_POOL);
		PrintState = PrintFaultPool;
		ret = Status_ReadRecentFaultPools(&pstAheadForm->stRobotCtrl.stStatus, uState.szstFaultPool, (unsigned int)stateCnt);
	}
	else
	{
		return ERR_ARGV;
	}

	if (ret == 0)
	{
		printf("No cached %s snapshot.\r\n", argv[1]);
	}
	else if (ret > 0)
	{
		printf("%s recent snapshot count=%d (oldest first within selected range).\r\n", argv[1], ret);
		for (i = 0; i < (unsigned int)ret; i++)
		{
			printf("snapshot[%u]:\r\n", i);
			PrintState(pElemAryBuf + i * sizeOf1Elem, NULL);
		}
	}

	return ret < 0 ? ret : RET_SUCCESS;
}

#define USER_CMD_STATUS_RX_EVENTFD 1
#define USER_CMD_STATUS_READER_CALLBACK 2
#ifndef USER_CMD_STATUS_RX_METHOD
#define USER_CMD_STATUS_RX_METHOD USER_CMD_STATUS_RX_EVENTFD
#endif

#if USER_CMD_STATUS_RX_METHOD != USER_CMD_STATUS_RX_EVENTFD && USER_CMD_STATUS_RX_METHOD != USER_CMD_STATUS_READER_CALLBACK
#error Invalid USER_CMD_STATUS_RX_METHOD
#endif

#if USER_CMD_STATUS_RX_METHOD == USER_CMD_STATUS_RX_EVENTFD
typedef union
{
	AHEADFORM_SENSOR_STATE stSensorState;
	AHEADFORM_NECK_STATE stNeckState;
	AHEADFORM_FAULT_POOL stFaultPool;
} UNION_USER_CMD_WATCH_STATE;

static int WatchState(AHEADFORM_ALL_CTRL *pstAheadForm, int argc, char *argv[])
{
	AHEADFORM_TOPIC_READER_PARA stReaderPara = {0};
	AHEADFORM_TOPIC_READER *pstReader = NULL;
	UNION_USER_CMD_WATCH_STATE uState;
	ENUM_AHEADFORM_TOPIC_ID topicId;
	AHEADFORM_TOPIC_READER_CALLBACK PrintState = NULL;
	fd_set rdFdSet;
	eventfd_t eventCnt;
	unsigned long long seconds, timerOverCnt, elapsedPeriodCnt = 0, missedElemCnt;
	unsigned int i, readableElemCnt;
	int timer_fd, max_fd, cleanupRet, ret;

	if ( (ret = IsUserInError(argc, argv, "3", "2")) != RET_SUCCESS )
		return ret;

	if (strcasecmp(argv[1], "sensor") == 0)
	{
		topicId = AHEADFORM_TOPIC_ID_SENSOR_STATE;
		PrintState = PrintSensorState;
	}
	else if (strcasecmp(argv[1], "neck") == 0)
	{
		topicId = AHEADFORM_TOPIC_ID_NECK_STATE;
		PrintState = PrintNeckState;
	}
	else if (strcasecmp(argv[1], "faults") == 0)
	{
		topicId = AHEADFORM_TOPIC_ID_FAULT_POOL;
		PrintState = PrintFaultPool;
	}
	else
	{
		return ERR_ARGV;
	}

	seconds = strtoull(argv[2], NULL, 0);
	if (seconds == 0 || seconds > INT_MAX)
		return ERR_ARGV;

	if ( (timer_fd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC)) <= 0 )
	{
		printf("%s() call timerfd_create(state watch timer) failed: %s\r\n", __FUNCTION__, strerror(errno));
		return ERR_CLOCK;
	}

	if ( (pstReader = Status_RegisterReader(&pstAheadForm->stRobotCtrl.stStatus, topicId, &stReaderPara)) == NULL )
	{
		if (close(timer_fd) < 0)
		{
			printf("%s() call close(timer_fd = %d) failed: %s\r\n", __FUNCTION__, timer_fd, strerror(errno));
			return ERR_CLOCK;
		}

		return ERRCODE_INIT_FAIL;
	}
	max_fd = timer_fd > pstReader->rxDataUpdateEventfd ? timer_fd : pstReader->rxDataUpdateEventfd;
	ret = SetTimerPeriod_ms(timer_fd, 1000);
	while (ret == RET_SUCCESS)
	{
		FD_ZERO(&rdFdSet);
		FD_SET(timer_fd, &rdFdSet);
		FD_SET(pstReader->rxDataUpdateEventfd, &rdFdSet);
		if (select(max_fd + 1, &rdFdSet, NULL, NULL, NULL) < 0)
		{
			printf("%s() call select(timer_fd = %d, rxDataUpdateEventfd = %d) failed: %s\r\n", __FUNCTION__, timer_fd, pstReader->rxDataUpdateEventfd, strerror(errno));
			ret = ERR_SELECT;
			break;
		}

		if (FD_ISSET(pstReader->rxDataUpdateEventfd, &rdFdSet))
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
				ret = TopicReader_PopElem(pstReader, &uState);
				if (ret == 1)
					PrintState(&uState, NULL);

				if (ret <= 0)
					break;
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
			if (elapsedPeriodCnt >= seconds)
				break;
		}
	}
	missedElemCnt = TopicReader_GetMissedElemCnt(pstReader);
	if ( (cleanupRet = Status_UnregisterReader(&pstAheadForm->stRobotCtrl.stStatus, pstReader)) != RET_SUCCESS )
	{
		printf("%s() Status_UnregisterReader(reader = %p) failed, ret = 0x%08X; exiting AheadFormSDK.\r\n", __FUNCTION__, (void*)pstReader, cleanupRet);
		PrintAllErrLog();
		exit(EXIT_FAILURE);
	}

	if (close(timer_fd) < 0)
	{
		printf("%s() call close(timer_fd = %d) failed: %s\r\n", __FUNCTION__, timer_fd, strerror(errno));
		ret = ERR_CLOCK;
	}
	printf("%s state FIFO missed=%llu.\r\n", argv[1], missedElemCnt);
	return ret;
}

#elif USER_CMD_STATUS_RX_METHOD == USER_CMD_STATUS_READER_CALLBACK
static int WatchState(AHEADFORM_ALL_CTRL *pstAheadForm, int argc, char *argv[])
{
	AHEADFORM_TOPIC_READER_PARA stReaderPara = {0};
	AHEADFORM_TOPIC_READER *pstReader = NULL;
	ENUM_AHEADFORM_TOPIC_ID topicId;
	fd_set rdFdSet;
	unsigned long long seconds, timerOverCnt, elapsedPeriodCnt = 0;
	int timer_fd, cleanupRet, ret;

	if ( (ret = IsUserInError(argc, argv, "3", "2")) != RET_SUCCESS )
		return ret;

	if (strcasecmp(argv[1], "sensor") == 0)
	{
		topicId = AHEADFORM_TOPIC_ID_SENSOR_STATE;
		stReaderPara.ReaderCallback = PrintSensorState;
	}
	else if (strcasecmp(argv[1], "neck") == 0)
	{
		topicId = AHEADFORM_TOPIC_ID_NECK_STATE;
		stReaderPara.ReaderCallback = PrintNeckState;
	}
	else if (strcasecmp(argv[1], "faults") == 0)
	{
		topicId = AHEADFORM_TOPIC_ID_FAULT_POOL;
		stReaderPara.ReaderCallback = PrintFaultPool;
	}
	else
	{
		return ERR_ARGV;
	}

	seconds = strtoull(argv[2], NULL, 0);
	if (seconds == 0 || seconds > INT_MAX)
		return ERR_ARGV;

	if ( (timer_fd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC)) <= 0 )
	{
		printf("%s() call timerfd_create(state watch timer) failed: %s\r\n", __FUNCTION__, strerror(errno));
		return ERR_CLOCK;
	}

	if ( (pstReader = Status_RegisterReader(&pstAheadForm->stRobotCtrl.stStatus, topicId, &stReaderPara)) == NULL )
	{
		if (close(timer_fd) < 0)
		{
			printf("%s() call close(timer_fd = %d) failed: %s\r\n", __FUNCTION__, timer_fd, strerror(errno));
			return ERR_CLOCK;
		}

		return ERRCODE_INIT_FAIL;
	}
	ret = SetTimerPeriod_ms(timer_fd, 1000);
	while (ret == RET_SUCCESS)
	{
		FD_ZERO(&rdFdSet);
		FD_SET(timer_fd, &rdFdSet);
		if (select(timer_fd + 1, &rdFdSet, NULL, NULL, NULL) < 0)
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
			if (elapsedPeriodCnt >= seconds)
				break;
		}
	}

	if ( (cleanupRet = Status_UnregisterReader(&pstAheadForm->stRobotCtrl.stStatus, pstReader)) != RET_SUCCESS )
	{
		printf("%s() Status_UnregisterReader(reader = %p) failed, ret = 0x%08X; exiting AheadFormSDK.\r\n", __FUNCTION__, (void*)pstReader, cleanupRet);
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

static int Exit(AHEADFORM_ALL_CTRL *pstAheadForm, int argc, char *argv[])
{
	if (argc != 1)
		return ERR_ARGC;

	return RET_EXIT;
}
