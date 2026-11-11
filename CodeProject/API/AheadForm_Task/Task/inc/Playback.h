#ifndef __AHEADFORM_PLAYBACK_H__
#define __AHEADFORM_PLAYBACK_H__

#include "Head.h"
#include "Media.h"
#include "Status.h"

#define AHEADFORM_SCRIPT_MOTION_DELAY_US 150000

typedef enum
{
	AHEADFORM_PLAYBACK_IDLE = 0,
	AHEADFORM_PLAYBACK_LOADING,
	AHEADFORM_PLAYBACK_PLAYING,
	AHEADFORM_PLAYBACK_COMPLETED,
	AHEADFORM_PLAYBACK_STOPPED,
	AHEADFORM_PLAYBACK_FAILED
} ENUM_AHEADFORM_PLAYBACK_STATE;

typedef struct
{
	ENUM_AHEADFORM_PLAYBACK_STATE state;
	int result;
	unsigned long long elapsed_us;
	unsigned long long duration_us;
	unsigned long long firstPcmTime_us;
	unsigned long long firstMotionTime_us;
	unsigned int motionFrameCnt;
	unsigned int sentMotionFrameCnt;
	unsigned int pcmFrameCnt;
	unsigned int sentPcmFrameCnt;
} AHEADFORM_PLAYBACK_INFO;

typedef struct
{
	struct AHEADFORM_SCRIPT_PLAYER *pstPlayer;
} AHEADFORM_PLAYBACK;

/*
	Serialize Init/Play/Stop/Deinit. GetInfo may run concurrently while initialized.
	Play copies the path and starts asynchronous loading; inspect state/result for file or transport errors.
	The prescribed CSV and same-basename MP3 remain on the SDK computer. No robot file transfer occurs.
	Playback owns speaker sending until it exits; do not call speaker Start/Write/Stop concurrently.
	Stop joins the worker before returning. Completion means local transmission, not physical robot completion.
*/
int Playback_Init(AHEADFORM_PLAYBACK *pstPlayback, AHEADFORM_HEAD_CTRL *pstHeadCtrl, AHEADFORM_MEDIA_CTRL *pstMediaCtrl, AHEADFORM_STATUS *pstStatus);
int Playback_Deinit(AHEADFORM_PLAYBACK *pstPlayback);
int Playback_PlayScript(AHEADFORM_PLAYBACK *pstPlayback, const char *csvFilePath);
int Playback_StopScript(AHEADFORM_PLAYBACK *pstPlayback);
int Playback_GetInfo(AHEADFORM_PLAYBACK *pstPlayback, AHEADFORM_PLAYBACK_INFO *pstInfo);

#endif
