#include "Playback.h"
#include "Time.h"

#include <pthread.h>
#include <time.h>
#include <strings.h>

struct AHEADFORM_SCRIPT_PLAYER
{
	AHEADFORM_HEAD_CTRL *pstHeadCtrl;
	AHEADFORM_MEDIA_CTRL *pstMediaCtrl;
	AHEADFORM_STATUS *pstStatus;
	pthread_mutex_t mutex;
	pthread_t thread;
	unsigned char hasThread;
	unsigned char stopRequested;
	char *csvFilePath;
	AHEADFORM_PLAYBACK_INFO stInfo;
};

static unsigned long long ScriptTimeUs(void)
{
	struct timespec stTime;

	if (clock_gettime(CLOCK_MONOTONIC, &stTime) != 0)
	{
		printf("%s() call clock_gettime(CLOCK_MONOTONIC) failed: %s\r\n", __FUNCTION__, strerror(errno));
		return 0;
	}

	return (unsigned long long)stTime.tv_sec * 1000000 + stTime.tv_nsec / 1000;
}

static void* SendScript(void *pArg)
{
	struct AHEADFORM_SCRIPT_PLAYER *pstPlayer = pArg;
	AHEADFORM_MOTION_CLIP *pstClip = NULL;
	AHEADFORM_HARDWARE_STATE_SAMPLE stHardware;
	unsigned char *pPcm = NULL;
	unsigned char pcmFrame[AHEADFORM_PCM_FRAME_SIZE];
	char *mp3FilePath = NULL;
	struct timespec stDeadline;
	unsigned long long startTime_us = 0, now_us, nextTime_us, pcmTime_us, motionTime_us, busyStart_us = 0;
	unsigned int pcmSize = 0, pcmOffset = 0, motionIndex = 0, pcmIndex = 0, copySize;
	unsigned char isSpeakerStarted = 0;
	int ret = RET_SUCCESS, cleanupRet;

	if ( (mp3FilePath = strdup(pstPlayer->csvFilePath)) == NULL )
		ret = ERR_MEMORY_ALLOC;
	else
		memcpy(mp3FilePath + strlen(mp3FilePath) - 4, ".mp3", 5);

	if (ret == RET_SUCCESS)
		ret = Head_LoadMotionCsv(pstPlayer->csvFilePath, &pstClip);

	if (ret == RET_SUCCESS && !__atomic_load_n(&pstPlayer->stopRequested, __ATOMIC_ACQUIRE))
		ret = Media_LoadMp3Pcm(mp3FilePath, &pPcm, &pcmSize);

	if (ret == RET_SUCCESS && !__atomic_load_n(&pstPlayer->stopRequested, __ATOMIC_ACQUIRE))
	{
		ret = Status_ReadLatestHardwareState(pstPlayer->pstStatus, &stHardware);
		if (ret == 1 && GetTimeUsCnt_UpToNow() - stHardware.ReceiveTime_us < 1000000)
		{
			ret = RET_SUCCESS;
			if (stHardware.activeMotionSourceName[0] != 0 && strcmp(stHardware.activeMotionSourceName, pstPlayer->pstHeadCtrl->stCfg.sourceName) != 0)
			{
				printf("%s() robot motion owned by %s; wait for source release before PlayScript\r\n", __FUNCTION__, stHardware.activeMotionSourceName);
				ret = ERR_RESOURCE_BUSY;
			}
			else if (!stHardware.faceMotionPermitted || !stHardware.neckMotionPermitted || stHardware.anyEstopLatched || stHardware.safeStopMotionBlockActive)
			{
				printf("%s() robot motion blocked: facePermit=%u neckPermit=%u estop=%u safeStop=%u\r\n", __FUNCTION__,
					stHardware.faceMotionPermitted, stHardware.neckMotionPermitted, stHardware.anyEstopLatched, stHardware.safeStopMotionBlockActive);
				ret = ERR_TARGET_REJECTED_BY_STATE;
			}
		}
		else
		{
			printf("%s() fresh /hw/state required before playback, read result=0x%08X\r\n", __FUNCTION__, ret);
			ret = ERR_SDK_NOT_READY;
		}
	}

	if (ret == RET_SUCCESS && !__atomic_load_n(&pstPlayer->stopRequested, __ATOMIC_ACQUIRE))
	{
		ret = Media_StartSpeakerSend(pstPlayer->pstMediaCtrl);
		isSpeakerStarted = ret == RET_SUCCESS;
	}

	if (ret == RET_SUCCESS && isSpeakerStarted)
	{
		pthread_mutex_lock(&pstPlayer->mutex);
		pstPlayer->stInfo.state = AHEADFORM_PLAYBACK_PLAYING;
		pstPlayer->stInfo.motionFrameCnt = pstClip->frameCnt;
		pstPlayer->stInfo.pcmFrameCnt = (pcmSize - 1) / AHEADFORM_PCM_FRAME_SIZE + 1;
		pstPlayer->stInfo.duration_us = (unsigned long long)pstPlayer->stInfo.pcmFrameCnt * 20000;
		if (pstClip->pstFrames[pstClip->frameCnt - 1].t_us + AHEADFORM_SCRIPT_MOTION_DELAY_US > pstPlayer->stInfo.duration_us)
			pstPlayer->stInfo.duration_us = pstClip->pstFrames[pstClip->frameCnt - 1].t_us + AHEADFORM_SCRIPT_MOTION_DELAY_US;

		pthread_mutex_unlock(&pstPlayer->mutex);
		while (!__atomic_load_n(&pstPlayer->stopRequested, __ATOMIC_ACQUIRE) && (pcmOffset < pcmSize || motionIndex < pstClip->frameCnt))
		{
			now_us = ScriptTimeUs();
			pcmTime_us = startTime_us + (unsigned long long)pcmIndex * 20000;
			motionTime_us = motionIndex < pstClip->frameCnt ? startTime_us + AHEADFORM_SCRIPT_MOTION_DELAY_US + pstClip->pstFrames[motionIndex].t_us : ULLONG_MAX;
			nextTime_us = pcmOffset < pcmSize && pcmTime_us <= motionTime_us ? pcmTime_us : motionTime_us;
			if (startTime_us != 0 && now_us < nextTime_us)
			{
				//Check Stop at least every 10 ms, including gaps in an otherwise valid CSV timeline.
				if (nextTime_us - now_us > 10000)
					nextTime_us = now_us + 10000;

				stDeadline.tv_sec = nextTime_us / 1000000;
				stDeadline.tv_nsec = (nextTime_us % 1000000) * 1000;
				if ( (cleanupRet = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &stDeadline, NULL)) != 0 )
				{
					printf("%s() call clock_nanosleep(script deadline) failed: %s\r\n", __FUNCTION__, strerror(cleanupRet));
					ret = ERR_CLOCK;
					break;
				}
				continue;
			}

			if (pcmOffset < pcmSize && (startTime_us == 0 || pcmTime_us <= motionTime_us))
			{
				memset(pcmFrame, 0, sizeof(pcmFrame));
				copySize = pcmSize - pcmOffset < sizeof(pcmFrame) ? pcmSize - pcmOffset : sizeof(pcmFrame);
				memcpy(pcmFrame, pPcm + pcmOffset, copySize);
				ret = Media_WriteSpeakerPcmFrame(pstPlayer->pstMediaCtrl, pcmFrame, sizeof(pcmFrame));
				if (ret == ERRCODE_RESOURCE_BUSY)
				{
					if (busyStart_us == 0)
						busyStart_us = now_us;

					if (now_us - busyStart_us >= 200000)
						break;

					stDeadline.tv_sec = (now_us + 1000) / 1000000;
					stDeadline.tv_nsec = ((now_us + 1000) % 1000000) * 1000;
					clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &stDeadline, NULL);
					continue;
				}

				if (ret != RET_SUCCESS)
					break;

				busyStart_us = 0;
				if (startTime_us == 0)
					startTime_us = ScriptTimeUs();

				pcmOffset += copySize;
				pcmIndex++;
			}
			else
			{
				ret = Head_SendMotion(pstPlayer->pstHeadCtrl, &pstClip->pstFrames[motionIndex]);
				if (ret != RET_SUCCESS)
					break;

				motionIndex++;
			}
			pthread_mutex_lock(&pstPlayer->mutex);
			pstPlayer->stInfo.firstPcmTime_us = startTime_us;
			if (motionIndex == 1 && pstPlayer->stInfo.firstMotionTime_us == 0)
				pstPlayer->stInfo.firstMotionTime_us = ScriptTimeUs();

			pstPlayer->stInfo.elapsed_us = ScriptTimeUs() - startTime_us;
			pstPlayer->stInfo.sentPcmFrameCnt = pcmIndex;
			pstPlayer->stInfo.sentMotionFrameCnt = motionIndex;
			pthread_mutex_unlock(&pstPlayer->mutex);
			ret = Media_GetSpeakerSendResult(pstPlayer->pstMediaCtrl);
			if (ret != RET_SUCCESS)
				break;
		}

		if (ret == RET_SUCCESS && !__atomic_load_n(&pstPlayer->stopRequested, __ATOMIC_ACQUIRE))
			ret = Media_FinishSpeakerSend(pstPlayer->pstMediaCtrl, 2000);
	}

	if (isSpeakerStarted)
	{
		cleanupRet = Media_StopSpeakerSend(pstPlayer->pstMediaCtrl);
		if (cleanupRet != RET_SUCCESS)
			ret = cleanupRet;
	}
	Head_ReleaseMotionClip(pstClip);
	free(pPcm);
	free(mp3FilePath);
	pthread_mutex_lock(&pstPlayer->mutex);
	if (startTime_us != 0)
		pstPlayer->stInfo.elapsed_us = ScriptTimeUs() - startTime_us;

	pstPlayer->stInfo.result = ret;
	if (ret != RET_SUCCESS)
		pstPlayer->stInfo.state = AHEADFORM_PLAYBACK_FAILED;
	else if (__atomic_load_n(&pstPlayer->stopRequested, __ATOMIC_ACQUIRE))
		pstPlayer->stInfo.state = AHEADFORM_PLAYBACK_STOPPED;
	else
		pstPlayer->stInfo.state = AHEADFORM_PLAYBACK_COMPLETED;

	printf("%s() state=%d result=0x%08X motion=%u PCM=%u first-motion-delay=%llu us\r\n", __FUNCTION__, pstPlayer->stInfo.state, ret, motionIndex, pcmIndex,
		pstPlayer->stInfo.firstMotionTime_us != 0 ? pstPlayer->stInfo.firstMotionTime_us - startTime_us : 0);
	pthread_mutex_unlock(&pstPlayer->mutex);
	return NULL;
}

int Playback_Init(AHEADFORM_PLAYBACK *pstPlayback, AHEADFORM_HEAD_CTRL *pstHeadCtrl, AHEADFORM_MEDIA_CTRL *pstMediaCtrl, AHEADFORM_STATUS *pstStatus)
{
	struct AHEADFORM_SCRIPT_PLAYER *pstPlayer;
	int ret;

	if (pstPlayback == NULL || pstHeadCtrl == NULL || pstMediaCtrl == NULL || pstStatus == NULL)
		return ERR_ARGV;

	if ( (pstPlayer = calloc(1, sizeof(*pstPlayer))) == NULL )
		return ERR_MEMORY_ALLOC;

	if ( (ret = pthread_mutex_init(&pstPlayer->mutex, NULL)) != 0 )
	{
		printf("%s() call pthread_mutex_init(script) failed: %s\r\n", __FUNCTION__, strerror(ret));
		free(pstPlayer);
		return ERR_MUTEX_INIT;
	}
	pstPlayer->pstHeadCtrl = pstHeadCtrl;
	pstPlayer->pstMediaCtrl = pstMediaCtrl;
	pstPlayer->pstStatus = pstStatus;
	pstPlayback->pstPlayer = pstPlayer;
	return RET_SUCCESS;
}

int Playback_StopScript(AHEADFORM_PLAYBACK *pstPlayback)
{
	struct AHEADFORM_SCRIPT_PLAYER *pstPlayer;
	int ret;

	if (pstPlayback == NULL)
		return ERR_ARGV_1;

	pstPlayer = pstPlayback->pstPlayer;
	if (pstPlayer == NULL || !pstPlayer->hasThread)
		return RET_SUCCESS;

	__atomic_store_n(&pstPlayer->stopRequested, 1, __ATOMIC_RELEASE);
	if ( (ret = pthread_join(pstPlayer->thread, NULL)) != 0 )
	{
		printf("%s() call pthread_join(script) failed: %s\r\n", __FUNCTION__, strerror(ret));
		return ERR_PTHREAD_CFG;
	}
	pstPlayer->hasThread = 0;
	free(pstPlayer->csvFilePath);
	pstPlayer->csvFilePath = NULL;
	return RET_SUCCESS;
}

int Playback_PlayScript(AHEADFORM_PLAYBACK *pstPlayback, const char *csvFilePath)
{
	struct AHEADFORM_SCRIPT_PLAYER *pstPlayer;
	AHEADFORM_PLAYBACK_INFO stInfo;
	int ret;

	if (pstPlayback == NULL || pstPlayback->pstPlayer == NULL)
		return ERR_ARGV_1;

	if (csvFilePath == NULL || strlen(csvFilePath) < 5 || strcasecmp(csvFilePath + strlen(csvFilePath) - 4, ".csv") != 0)
		return ERR_ARGV_2;

	pstPlayer = pstPlayback->pstPlayer;
	Playback_GetInfo(pstPlayback, &stInfo);
	if (stInfo.state == AHEADFORM_PLAYBACK_LOADING || stInfo.state == AHEADFORM_PLAYBACK_PLAYING)
		return ERR_RESOURCE_BUSY;

	if ( (ret = Playback_StopScript(pstPlayback)) != RET_SUCCESS )
		return ret;

	if ( (pstPlayer->csvFilePath = strdup(csvFilePath)) == NULL )
		return ERR_MEMORY_ALLOC;

	__atomic_store_n(&pstPlayer->stopRequested, 0, __ATOMIC_RELEASE);
	pthread_mutex_lock(&pstPlayer->mutex);
	memset(&pstPlayer->stInfo, 0, sizeof(pstPlayer->stInfo));
	pstPlayer->stInfo.state = AHEADFORM_PLAYBACK_LOADING;
	pthread_mutex_unlock(&pstPlayer->mutex);
	if ( (ret = pthread_create(&pstPlayer->thread, NULL, SendScript, pstPlayer)) != 0 )
	{
		printf("%s() call pthread_create(script) failed: %s\r\n", __FUNCTION__, strerror(ret));
		free(pstPlayer->csvFilePath);
		pstPlayer->csvFilePath = NULL;
		pthread_mutex_lock(&pstPlayer->mutex);
		pstPlayer->stInfo.state = AHEADFORM_PLAYBACK_FAILED;
		pstPlayer->stInfo.result = ERRCODE_PTHREAD_INIT;
		pthread_mutex_unlock(&pstPlayer->mutex);
		return ERR_PTHREAD_INIT;
	}
	pstPlayer->hasThread = 1;
	return RET_SUCCESS;
}

int Playback_GetInfo(AHEADFORM_PLAYBACK *pstPlayback, AHEADFORM_PLAYBACK_INFO *pstInfo)
{
	if (pstPlayback == NULL || pstPlayback->pstPlayer == NULL)
		return ERR_ARGV_1;

	if (pstInfo == NULL)
		return ERR_ARGV_2;

	pthread_mutex_lock(&pstPlayback->pstPlayer->mutex);
	*pstInfo = pstPlayback->pstPlayer->stInfo;
	pthread_mutex_unlock(&pstPlayback->pstPlayer->mutex);
	return RET_SUCCESS;
}

int Playback_Deinit(AHEADFORM_PLAYBACK *pstPlayback)
{
	int ret;

	if (pstPlayback == NULL)
		return ERR_ARGV_1;

	if (pstPlayback->pstPlayer == NULL)
		return RET_SUCCESS;

	if ( (ret = Playback_StopScript(pstPlayback)) != RET_SUCCESS )
		return ret;

	if ( (ret = pthread_mutex_destroy(&pstPlayback->pstPlayer->mutex)) != 0 )
	{
		printf("%s() call pthread_mutex_destroy(script) failed: %s\r\n", __FUNCTION__, strerror(ret));
		return ERR_MUTEX_OP_FAILED;
	}
	free(pstPlayback->pstPlayer);
	pstPlayback->pstPlayer = NULL;
	return RET_SUCCESS;
}
