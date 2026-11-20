#ifndef __AHEADFORM_MEDIA_H__
#define __AHEADFORM_MEDIA_H__

#include "Audio.h"
#include "Video.h"

typedef struct
{
	AHEADFORM_AUDIO_CTRL stAudioCtrl;
	AHEADFORM_VIDEO_CTRL stVideoCtrl;
} AHEADFORM_MEDIA_CTRL;

/*
	Media owns its embedded Audio/Video; Topics and parsed configuration must be ready.
	Runtime APIs require successful Init; on Init failure, the application reports the error and exits without Deinit.
	Deinit rejects customer media Readers before teardown; once teardown begins, errors are terminal and Deinit must not be retried.
*/
int Media_Init(AHEADFORM_MEDIA_CTRL *pstMediaCtrl, AHEADFORM_TOPICS *pstTopics, const AHEADFORM_INIT_CONFIG *pstCfg);
int Media_Deinit(AHEADFORM_MEDIA_CTRL *pstMediaCtrl);

#endif
