#ifndef __AHEADFORM_HEAD_H__
#define __AHEADFORM_HEAD_H__

#include "AheadFormTopics.h"

typedef struct
{
	unsigned int frameCnt;
	AHEADFORM_MOTION_FRAME *pstFrames;
} AHEADFORM_MOTION_CLIP;

typedef struct
{
	AHEADFORM_TOPICS *pstTopics;
	AHEADFORM_HEAD_CFG stCfg;
	AHEADFORM_TOPIC_WRITER *pstMotionWriter;
	AHEADFORM_TOPIC_WRITER *pstFaceWriter;
	AHEADFORM_TOPIC_WRITER *pstNeckWriter;
	AHEADFORM_TOPIC_WRITER *pstServoWriter;
	AHEADFORM_TOPIC_WRITER *pstPresetAskWriter;
	AHEADFORM_TOPIC_READER *pstPresetReplyReader;
	unsigned long long nextCommandSeq;
} AHEADFORM_HEAD_CTRL;

//pstTopics must be successfully initialized first.
int Head_Init(AHEADFORM_HEAD_CTRL *pstHeadCtrl, AHEADFORM_TOPICS *pstTopics, const AHEADFORM_HEAD_CFG *pstHeadCfg);
int Head_Deinit(AHEADFORM_HEAD_CTRL *pstHeadCtrl);

//Success reports local publication only; robot arbitration and hardware limits remain authoritative.
int Head_SendMotion(AHEADFORM_HEAD_CTRL *pstHeadCtrl, const AHEADFORM_MOTION_FRAME *pstFrame);
int Head_SendFace(AHEADFORM_HEAD_CTRL *pstHeadCtrl, const AHEADFORM_EYE_POSE *pstEyes, const float *pBlendshapes);
int Head_SendNeck(AHEADFORM_HEAD_CTRL *pstHeadCtrl, const AHEADFORM_NECK_POSE *pstNeck);
int Head_SendServoTargets(AHEADFORM_HEAD_CTRL *pstHeadCtrl, const double *pTargetLogic_deg);

/*
	Build assigns identity; Send reads only during the call. Async builds, copies matching values and publishes.
	Completion storage/eventfd follow TopicReader_RegisterAsyncRequest; inspect the remote reply after Check returns 1.
	MatchReplyWithReq reads a complete request object. Register a customer Reader before publishing for manual matching.
*/
int Head_BuildPresetMotionRequest(AHEADFORM_HEAD_CTRL *pstHeadCtrl, unsigned char action, AHEADFORM_PRESET_MOTION_ASK *pstRequest);
int Head_SendPresetMotionRequest(AHEADFORM_HEAD_CTRL *pstHeadCtrl, const AHEADFORM_PRESET_MOTION_ASK *pstRequest);
int Head_PresetMotionAsync(AHEADFORM_HEAD_CTRL *pstHeadCtrl, unsigned char action, unsigned long long MatchReplyTimeout_us, int notifyEventfd, AHEADFORM_PRESET_MOTION_REPLY_SAMPLE *pstReply);
int Head_CheckIsPresetMotionAsyncRequestReplied(AHEADFORM_HEAD_CTRL *pstHeadCtrl, int notifyEventfd);
int Head_CancelPresetMotionAsyncRequest(AHEADFORM_HEAD_CTRL *pstHeadCtrl, int notifyEventfd);
AHEADFORM_TOPIC_READER* Head_RegisterPresetMotionReader(AHEADFORM_HEAD_CTRL *pstHeadCtrl, const AHEADFORM_TOPIC_READER_PARA *pstReaderPara);
int Head_UnregisterPresetMotionReader(AHEADFORM_HEAD_CTRL *pstHeadCtrl, AHEADFORM_TOPIC_READER *pstReader);
int Head_MatchPresetMotionReplyWithReq(const void *pReplyElem, const void *pRequest);

//Dictionary v1 uses the arbiter's fixed 52-channel order; returned names are static, read-only strings.
extern const char * const gAheadFormBlendshapeNames[AHEADFORM_BLENDSHAPE_CNT];
/*
	Only the intro+clzy schema is accepted: timestamp (seconds), istracked, all 52 dictionary names,
	neckPitch/neckYaw/neckRoll and eyePitchLeft/eyeYawLeft/eyeRollLeft/eyePitchRight/eyeYawRight/eyeRollRight.
	Angles are radians. Eye roll is validated but has no DDS destination. The first timestamp must be zero.
	Load owns its independent frame storage; the caller releases it. Sampling clamps to the first/last frame.
*/
int Head_LoadMotionCsv(const char *filePath, AHEADFORM_MOTION_CLIP **ppClip);
void Head_ReleaseMotionClip(AHEADFORM_MOTION_CLIP *pstClip);
int Head_SampleMotionClip(const AHEADFORM_MOTION_CLIP *pstClip, unsigned long long t_us, AHEADFORM_MOTION_FRAME *pstFrame);

#endif
