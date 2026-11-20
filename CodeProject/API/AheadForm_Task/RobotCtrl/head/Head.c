#include "Head.h"
#include "AheadFormDdsMsg.h"

#include <float.h>
#include <math.h>
#include <time.h>

const char* const gAheadFormBlendshapeNames[AHEADFORM_BLENDSHAPE_CNT] =
{
	"eyeBlinkLeft",
	"eyeLookDownLeft",
	"eyeLookInLeft",
	"eyeLookOutLeft",
	"eyeLookUpLeft",
	"eyeSquintLeft",
	"eyeWideLeft",
	"eyeBlinkRight",
	"eyeLookDownRight",
	"eyeLookInRight",
	"eyeLookOutRight",
	"eyeLookUpRight",
	"eyeSquintRight",
	"eyeWideRight",
	"jawForward",
	"jawLeft",
	"jawRight",
	"jawOpen",
	"mouthClose",
	"mouthFunnel",
	"mouthPucker",
	"mouthLeft",
	"mouthRight",
	"mouthSmileLeft",
	"mouthSmileRight",
	"mouthFrownLeft",
	"mouthFrownRight",
	"mouthDimpleLeft",
	"mouthDimpleRight",
	"mouthStretchLeft",
	"mouthStretchRight",
	"mouthRollLower",
	"mouthRollUpper",
	"mouthShrugLower",
	"mouthShrugUpper",
	"mouthPressLeft",
	"mouthPressRight",
	"mouthLowerDownLeft",
	"mouthLowerDownRight",
	"mouthUpperUpLeft",
	"mouthUpperUpRight",
	"browDownLeft",
	"browDownRight",
	"browInnerUp",
	"browOuterUpLeft",
	"browOuterUpRight",
	"cheekPuff",
	"cheekSquintLeft",
	"cheekSquintRight",
	"noseSneerLeft",
	"noseSneerRight",
	"tongueOut"
};

int Head_Init(AHEADFORM_HEAD_CTRL *pstHeadCtrl, AHEADFORM_TOPICS *pstTopics, const AHEADFORM_HEAD_CFG *pstHeadCfg)
{
	if (pstHeadCtrl == NULL)
		return ERR_ARGV_1;

	if (pstTopics == NULL)
		return ERR_ARGV_2;

	if (pstHeadCfg == NULL)
		return ERR_ARGV_3;

	if (pstTopics->stDdsCtrl.ddsParticipant <= 0)
		return ERR_ARGV_2;

	if (!IsValidHeadSourceName(pstHeadCfg->sourceName))
		return ERR_ARGV_3;

	memset(pstHeadCtrl, 0, sizeof(*pstHeadCtrl));
	pstHeadCtrl->pstTopics = pstTopics;
	pstHeadCtrl->stCfg = *pstHeadCfg;
	if ( (pstHeadCtrl->pstMotionWriter = AheadFormTopics_RegisterWriter(pstTopics, AHEADFORM_TOPIC_ID_MOTION_POSE)) == NULL )
		return ERRCODE_INIT_FAIL;

	if ( (pstHeadCtrl->pstFaceWriter = AheadFormTopics_RegisterWriter(pstTopics, AHEADFORM_TOPIC_ID_FACE)) == NULL )
		return ERRCODE_INIT_FAIL;

	if ( (pstHeadCtrl->pstNeckWriter = AheadFormTopics_RegisterWriter(pstTopics, AHEADFORM_TOPIC_ID_NECK_POSE)) == NULL )
		return ERRCODE_INIT_FAIL;

	if ( (pstHeadCtrl->pstServosWriter = AheadFormTopics_RegisterWriter(pstTopics, AHEADFORM_TOPIC_ID_SERVO_TARGETS)) == NULL )
		return ERRCODE_INIT_FAIL;

	return RET_SUCCESS;
}

int Head_Deinit(AHEADFORM_HEAD_CTRL *pstHeadCtrl)
{
	int ret;

	if (pstHeadCtrl == NULL)
		return ERR_ARGV_1;

	if (pstHeadCtrl->pstMotionWriter != NULL)
	{
		if ( (ret = AheadFormTopics_UnregisterWriter(pstHeadCtrl->pstTopics, pstHeadCtrl->pstMotionWriter)) != RET_SUCCESS )
			return ret;

		pstHeadCtrl->pstMotionWriter = NULL;
	}

	if (pstHeadCtrl->pstFaceWriter != NULL)
	{
		if ( (ret = AheadFormTopics_UnregisterWriter(pstHeadCtrl->pstTopics, pstHeadCtrl->pstFaceWriter)) != RET_SUCCESS )
			return ret;

		pstHeadCtrl->pstFaceWriter = NULL;
	}

	if (pstHeadCtrl->pstNeckWriter != NULL)
	{
		if ( (ret = AheadFormTopics_UnregisterWriter(pstHeadCtrl->pstTopics, pstHeadCtrl->pstNeckWriter)) != RET_SUCCESS )
			return ret;

		pstHeadCtrl->pstNeckWriter = NULL;
	}

	if (pstHeadCtrl->pstServosWriter != NULL)
	{
		if ( (ret = AheadFormTopics_UnregisterWriter(pstHeadCtrl->pstTopics, pstHeadCtrl->pstServosWriter)) != RET_SUCCESS )
			return ret;

		pstHeadCtrl->pstServosWriter = NULL;
	}
	memset(pstHeadCtrl, 0, sizeof(*pstHeadCtrl));
	return RET_SUCCESS;
}

int Head_Send1MotionPose(AHEADFORM_HEAD_CTRL *pstHeadCtrl, const AHEADFORM_MOTION_POSE *pstPose)
{
	arbitration_msgs_msg_AheadMotion stDdsMotion;
	unsigned int i;

	if (pstHeadCtrl == NULL)
		return ERR_ARGV_1;

	if (pstPose == NULL)
		return ERR_ARGV_2;

	if (pstHeadCtrl->pstTopics == NULL || pstHeadCtrl->pstMotionWriter == NULL)
		return ERR_SDK_NOT_READY;

	if (!isfinite(pstPose->stNeck.pitch_rad) || !isfinite(pstPose->stNeck.yaw_rad) || !isfinite(pstPose->stNeck.roll_rad))
		return ERR_ARGV_2;

	if (!isfinite(pstPose->stNeck.headX) || !isfinite(pstPose->stNeck.headY) || !isfinite(pstPose->stNeck.headZ))
		return ERR_ARGV_2;

	if (!isfinite(pstPose->stEyes.leftPitch_rad) || !isfinite(pstPose->stEyes.leftYaw_rad) || !isfinite(pstPose->stEyes.rightPitch_rad) || !isfinite(pstPose->stEyes.rightYaw_rad))
		return ERR_ARGV_2;

	for (i = 0; i < AHEADFORM_BLENDSHAPE_CNT; i++)
	{
		if (!isfinite(pstPose->blendshapes[i]) || pstPose->blendshapes[i] < 0.0f || pstPose->blendshapes[i] > 1.0f)
			return ERR_ARGV_2;
	}
	stDdsMotion.version = AHEADFORM_MOTION_VERSION;
	stDdsMotion.timestamp = (double)pstPose->t_us / 1000000.0;
	strcpy(stDdsMotion.source_name, pstHeadCtrl->stCfg.sourceName);
	stDdsMotion.neck.pitch = pstPose->stNeck.pitch_rad;
	stDdsMotion.neck.yaw = pstPose->stNeck.yaw_rad;
	stDdsMotion.neck.roll = pstPose->stNeck.roll_rad;
	stDdsMotion.neck.head_x = pstPose->stNeck.headX;
	stDdsMotion.neck.head_y = pstPose->stNeck.headY;
	stDdsMotion.neck.head_z = pstPose->stNeck.headZ;
	stDdsMotion.eyes.left_eye_pitch = pstPose->stEyes.leftPitch_rad;
	stDdsMotion.eyes.left_eye_yaw = pstPose->stEyes.leftYaw_rad;
	stDdsMotion.eyes.right_eye_pitch = pstPose->stEyes.rightPitch_rad;
	stDdsMotion.eyes.right_eye_yaw = pstPose->stEyes.rightYaw_rad;
	memcpy(stDdsMotion.blendshapes, pstPose->blendshapes, sizeof(stDdsMotion.blendshapes));
	return TopicWriter_Write(pstHeadCtrl->pstMotionWriter, &stDdsMotion);
}

int Head_Send1Face(AHEADFORM_HEAD_CTRL *pstHeadCtrl, const AHEADFORM_EYE_POSE *pstEyes, const float szBlendshapes[AHEADFORM_BLENDSHAPE_CNT])
{
	arbitration_msgs_msg_FaceControl stDdsFace;
	struct timespec stTime;
	unsigned int i;

	if (pstHeadCtrl == NULL)
		return ERR_ARGV_1;

	if (pstEyes == NULL)
		return ERR_ARGV_2;

	if (szBlendshapes == NULL)
		return ERR_ARGV_3;

	if (pstHeadCtrl->pstTopics == NULL || pstHeadCtrl->pstFaceWriter == NULL)
		return ERR_SDK_NOT_READY;

	if (!isfinite(pstEyes->leftPitch_rad) || !isfinite(pstEyes->leftYaw_rad) || !isfinite(pstEyes->rightPitch_rad) || !isfinite(pstEyes->rightYaw_rad))
		return ERR_ARGV_2;

	for (i = 0; i < AHEADFORM_BLENDSHAPE_CNT; i++)
	{
		if (!isfinite(szBlendshapes[i]) || szBlendshapes[i] < 0.0f || szBlendshapes[i] > 1.0f)
			return ERR_ARGV_3;
	}

	if (clock_gettime(CLOCK_MONOTONIC, &stTime) < 0)
	{
		printf("%s() call clock_gettime(CLOCK_MONOTONIC) failed: %s\r\n", __FUNCTION__, strerror(errno));
		return ERR_CLOCK;
	}
	stDdsFace.timestamp = (double)stTime.tv_sec + (double)stTime.tv_nsec / 1000000000.0;
	stDdsFace.command_seq = ++pstHeadCtrl->nextCmdSeq;
	strcpy(stDdsFace.source_name, pstHeadCtrl->stCfg.sourceName);
	stDdsFace.eyes.left_eye_pitch = pstEyes->leftPitch_rad;
	stDdsFace.eyes.left_eye_yaw = pstEyes->leftYaw_rad;
	stDdsFace.eyes.right_eye_pitch = pstEyes->rightPitch_rad;
	stDdsFace.eyes.right_eye_yaw = pstEyes->rightYaw_rad;
	memcpy(stDdsFace.blendshapes, szBlendshapes, sizeof(stDdsFace.blendshapes));
	return TopicWriter_Write(pstHeadCtrl->pstFaceWriter, &stDdsFace);
}

int Head_Send1NeckPose(AHEADFORM_HEAD_CTRL *pstHeadCtrl, float pitch_rad, float yaw_rad, float roll_rad)
{
	arbitration_msgs_msg_NeckControl stDdsNeck;
	struct timespec stTime;

	if (pstHeadCtrl == NULL)
		return ERR_ARGV_1;

	if (!isfinite(pitch_rad))
		return ERR_ARGV_2;

	if (!isfinite(yaw_rad))
		return ERR_ARGV_3;

	if (!isfinite(roll_rad))
		return ERR_ARGV_4;

	if (pstHeadCtrl->pstTopics == NULL || pstHeadCtrl->pstNeckWriter == NULL)
		return ERR_SDK_NOT_READY;

	if (clock_gettime(CLOCK_MONOTONIC, &stTime) < 0)
	{
		printf("%s() call clock_gettime(CLOCK_MONOTONIC) failed: %s\r\n", __FUNCTION__, strerror(errno));
		return ERR_CLOCK;
	}
	stDdsNeck.timestamp = (double)stTime.tv_sec + (double)stTime.tv_nsec / 1000000000.0;
	stDdsNeck.command_seq = ++pstHeadCtrl->nextCmdSeq;
	strcpy(stDdsNeck.source_name, pstHeadCtrl->stCfg.sourceName);
	stDdsNeck.pitch = pitch_rad;
	stDdsNeck.yaw = yaw_rad;
	stDdsNeck.roll = roll_rad;
	return TopicWriter_Write(pstHeadCtrl->pstNeckWriter, &stDdsNeck);
}

int Head_SendServosTargetDeg(AHEADFORM_HEAD_CTRL *pstHeadCtrl, const double szTargetLogicalDeg[AHEADFORM_FACE_SERVO_CNT])
{
	arbitration_msgs_msg_FaceServoDirectTarget stDdsTargets;
	struct timespec stTime;
	unsigned int i;

	if (pstHeadCtrl == NULL)
		return ERR_ARGV_1;

	if (szTargetLogicalDeg == NULL)
		return ERR_ARGV_2;

	if (pstHeadCtrl->pstTopics == NULL || pstHeadCtrl->pstServosWriter == NULL)
		return ERR_SDK_NOT_READY;

	for (i = 0; i < AHEADFORM_FACE_SERVO_CNT; i++)
	{
		if (!isfinite(szTargetLogicalDeg[i]) || szTargetLogicalDeg[i] < -FLT_MAX || szTargetLogicalDeg[i] > FLT_MAX)
			return ERR_ARGV_2;
	}

	if (clock_gettime(CLOCK_REALTIME, &stTime) < 0)
	{
		printf("%s() call clock_gettime(CLOCK_REALTIME) failed: %s\r\n", __FUNCTION__, strerror(errno));
		return ERR_CLOCK;
	}
	stDdsTargets.stamp.sec = stTime.tv_sec;
	stDdsTargets.stamp.nanosec = stTime.tv_nsec;
	stDdsTargets.command_seq = ++pstHeadCtrl->nextCmdSeq;
	memcpy(stDdsTargets.target_logic_deg, szTargetLogicalDeg, sizeof(stDdsTargets.target_logic_deg));
	return TopicWriter_Write(pstHeadCtrl->pstServosWriter, &stDdsTargets);
}
