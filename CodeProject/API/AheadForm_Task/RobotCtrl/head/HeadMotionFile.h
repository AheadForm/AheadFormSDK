#ifndef __AHEADFORM_HEAD_MOTION_FILE_H__
#define __AHEADFORM_HEAD_MOTION_FILE_H__

#include "PublicDef.h"
#include "AheadFormTopicData.h"

#define AHEADFORM_MOTION_CSV_FILE_SIZE_MAX (32 * 1024 * 1024)

typedef struct
{
	unsigned int frameCnt;
	unsigned long long duration_us;
} AHEADFORM_MOTION_FILE_INFO;

int Head_CheckMotionCsv(const char *filePath);

/*
	poseArySize is bytes; use output only after RET_SUCCESS.
	Failure preserves info but may partly write poses. Outputs must not overlap.
*/
int Head_LoadMotionCsv(const char *filePath, AHEADFORM_MOTION_POSE *pstRetPoseAry, size_t poseArySize, AHEADFORM_MOTION_FILE_INFO *pstRetInfo);

/*
	Use an unchanged loaded array; output must not overlap it. Calls are independent.
	Time is relative to the first frame; equal times use the last row, and the endpoint is held after the end.
	Output t_us equals targetTime_us; failure preserves output.
*/
int Head_CalcInterpolateMotionPose(const AHEADFORM_MOTION_POSE *pstPoseAry, unsigned int poseCnt, unsigned long long targetTime_us, AHEADFORM_MOTION_POSE *pstRetPose);

#endif
