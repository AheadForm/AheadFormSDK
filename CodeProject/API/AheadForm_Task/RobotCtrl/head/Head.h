#ifndef __AHEADFORM_HEAD_H__
#define __AHEADFORM_HEAD_H__

#include "AheadFormTopics.h"
#include "HeadMotionFile.h"

typedef struct
{
	AHEADFORM_TOPICS *pstTopics;
	AHEADFORM_HEAD_CFG stCfg;
	AHEADFORM_TOPIC_WRITER *pstMotionWriter;
	AHEADFORM_TOPIC_WRITER *pstFaceWriter;
	AHEADFORM_TOPIC_WRITER *pstNeckWriter;
	AHEADFORM_TOPIC_WRITER *pstServosWriter;
	unsigned long long nextCmdSeq;
} AHEADFORM_HEAD_CTRL;

/*
	pstTopics must be successfully initialized first.
	Serialize lifecycle operations and send calls using the same Head handle.
	Release registered Readers/Writers before reinitialization.
*/
int Head_Init(AHEADFORM_HEAD_CTRL *pstHeadCtrl, AHEADFORM_TOPICS *pstTopics, const AHEADFORM_HEAD_CFG *pstHeadCfg);
int Head_Deinit(AHEADFORM_HEAD_CTRL *pstHeadCtrl);

/*
	Each call publishes one sample and does not create a playback task.
	Angles in motion, face and neck poses are radians; blendshapes must be in [0,1].
	Success only means local DDS publication succeeded.
*/
int Head_Send1MotionPose(AHEADFORM_HEAD_CTRL *pstHeadCtrl, const AHEADFORM_MOTION_POSE *pstPose);
int Head_Send1Face(AHEADFORM_HEAD_CTRL *pstHeadCtrl, const AHEADFORM_EYE_POSE *pstEyes, const float szBlendshapes[AHEADFORM_BLENDSHAPE_CNT]);
int Head_Send1NeckPose(AHEADFORM_HEAD_CTRL *pstHeadCtrl, float pitch_rad, float yaw_rad, float roll_rad);
//Send all 20 logical servo targets in degrees; the caller supplies at least AHEADFORM_FACE_SERVO_CNT elements.
int Head_SendServosTargetDeg(AHEADFORM_HEAD_CTRL *pstHeadCtrl, const double szTargetLogicalDeg[AHEADFORM_FACE_SERVO_CNT]);

#endif
