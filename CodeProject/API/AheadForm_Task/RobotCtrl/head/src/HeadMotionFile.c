#include "Head.h"

#include <math.h>
#include <stdint.h>

#define MOTION_CSV_COLUMN_CNT (2 + AHEADFORM_BLENDSHAPE_CNT + 9)

const char * const gAheadFormBlendshapeNames[AHEADFORM_BLENDSHAPE_CNT] =
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
	"tongueOut",
};

static const char * const gPoseColumnNames[9] =
{
	"neckPitch", "neckYaw", "neckRoll", "eyePitchLeft", "eyeYawLeft", "eyeRollLeft", "eyePitchRight", "eyeYawRight", "eyeRollRight"
};

static int ParseMotionRow(char *strLine, const unsigned int *pColumnMap, AHEADFORM_MOTION_FRAME *pstFrame)
{
	char *pCursor = strLine, *pField = NULL, *pEnd = NULL;
	unsigned int i, column;
	double value;
	float pose[9] = {0};

	memset(pstFrame, 0, sizeof(*pstFrame));
	for (i = 0; i < MOTION_CSV_COLUMN_CNT; i++)
	{
		if ( (pField = strsep(&pCursor, ",")) == NULL || pField[0] == 0 )
			return ERR_FILE_FORMAT;

		value = strtod(pField, &pEnd);
		if (pEnd == pField || *pEnd != 0 || !isfinite(value))
			return ERR_FILE_FORMAT;

		column = pColumnMap[i];
		if (column == 0)
		{
			if (value < 0 || value >= (double)ULLONG_MAX / 1000000)
				return ERR_FILE_FORMAT;

			pstFrame->t_us = (unsigned long long)(value * 1000000 + 0.5);
		}
		else if (column == 1)
		{
			if (value != 0 && value != 1)
				return ERR_FILE_FORMAT;
		}
		else if (column < 2 + AHEADFORM_BLENDSHAPE_CNT)
		{
			if (value < 0 || value > 1)
				return ERR_FILE_FORMAT;

			pstFrame->blendshapes[column - 2] = (float)value;
		}
		else
		{
			pose[column - 2 - AHEADFORM_BLENDSHAPE_CNT] = (float)value;
			if (!isfinite(pose[column - 2 - AHEADFORM_BLENDSHAPE_CNT]))
				return ERR_FILE_FORMAT;
		}
	}

	if (pCursor != NULL)
		return ERR_FILE_FORMAT;

	pstFrame->stNeck.pitch_rad = pose[0];
	pstFrame->stNeck.yaw_rad = pose[1];
	pstFrame->stNeck.roll_rad = pose[2];
	pstFrame->stEyes.leftPitch_rad = pose[3];
	pstFrame->stEyes.leftYaw_rad = pose[4];
	pstFrame->stEyes.rightPitch_rad = pose[6];
	pstFrame->stEyes.rightYaw_rad = pose[7];
	return RET_SUCCESS;
}

int Head_LoadMotionCsv(const char *filePath, AHEADFORM_MOTION_CLIP **ppClip)
{
	FILE *pFile = NULL;
	AHEADFORM_MOTION_CLIP *pstClip = NULL;
	AHEADFORM_MOTION_FRAME *pstFrames = NULL;
	char *strLine = NULL, *pCursor = NULL, *pField = NULL;
	size_t lineCapacity = 0;
	ssize_t lineLen;
	unsigned int columnMap[MOTION_CSV_COLUMN_CNT], column, i, j, capacity = 0, lineNO = 0;
	unsigned char seen[MOTION_CSV_COLUMN_CNT] = {0};
	int ret = RET_SUCCESS;

	if (filePath == NULL || filePath[0] == 0)
		return ERR_ARGV_1;

	if (ppClip == NULL)
		return ERR_ARGV_2;

	*ppClip = NULL;
	if ( (pFile = fopen(filePath, "rb")) == NULL )
	{
		printf("%s() call fopen(%s) failed: %s\r\n", __FUNCTION__, filePath, strerror(errno));
		return ERR_FILE_OP_FAILED;
	}

	if ( (pstClip = calloc(1, sizeof(*pstClip))) == NULL )
		ret = ERR_MEMORY_ALLOC;

	while (ret == RET_SUCCESS && (lineLen = getline(&strLine, &lineCapacity, pFile)) >= 0)
	{
		lineNO++;
		while (lineLen > 0 && (strLine[lineLen - 1] == '\r' || strLine[lineLen - 1] == '\n'))
			strLine[--lineLen] = 0;

		if (lineNO == 1)
		{
			pCursor = strLine;
			if (strncmp(pCursor, "\xEF\xBB\xBF", 3) == 0)
				pCursor += 3;

			for (i = 0; i < MOTION_CSV_COLUMN_CNT; i++)
			{
				pField = strsep(&pCursor, ",");
				column = MOTION_CSV_COLUMN_CNT;
				if (pField == NULL)
				{
					ret = ERR_FILE_FORMAT;
					break;
				}

				if (strcmp(pField, "timestamp") == 0)
					column = 0;
				else if (strcmp(pField, "istracked") == 0)
					column = 1;

				for (j = 0; j < AHEADFORM_BLENDSHAPE_CNT; j++)
				{
					if (strcmp(pField, gAheadFormBlendshapeNames[j]) == 0)
						column = j + 2;
				}

				for (j = 0; j < 9; j++)
				{
					if (strcmp(pField, gPoseColumnNames[j]) == 0)
						column = AHEADFORM_BLENDSHAPE_CNT + 2 + j;
				}

				if (column == MOTION_CSV_COLUMN_CNT || seen[column])
				{
					ret = ERR_FILE_FORMAT;
					break;
				}
				seen[column] = 1;
				columnMap[i] = column;
			}

			if (pCursor != NULL)
				ret = ERR_FILE_FORMAT;
		}
		else
		{
			if (pstClip->frameCnt == capacity)
			{
				if (capacity > UINT_MAX - 1024 || (size_t)capacity + 1024 > SIZE_MAX / sizeof(*pstFrames))
				{
					ret = ERR_OVER_BUF;
					break;
				}

				if ( (pstFrames = realloc(pstClip->pstFrames, (size_t)(capacity + 1024) * sizeof(*pstFrames))) == NULL )
				{
					ret = ERR_MEMORY_ALLOC;
					break;
				}
				pstClip->pstFrames = pstFrames;
				capacity += 1024;
			}
			ret = ParseMotionRow(strLine, columnMap, &pstClip->pstFrames[pstClip->frameCnt]);
			if (ret == RET_SUCCESS && pstClip->frameCnt != 0 && pstClip->pstFrames[pstClip->frameCnt].t_us <= pstClip->pstFrames[pstClip->frameCnt - 1].t_us)
				ret = ERR_FILE_FORMAT;

			if (ret == RET_SUCCESS)
				pstClip->frameCnt++;
		}
	}

	if (ferror(pFile))
	{
		printf("%s() call getline(%s) failed: %s\r\n", __FUNCTION__, filePath, strerror(errno));
		ret = ERR_FILE_OP_FAILED;
	}

	if (fclose(pFile) != RET_SUCCESS)
	{
		printf("%s() call fclose(%s) failed: %s\r\n", __FUNCTION__, filePath, strerror(errno));
		ret = ERR_FILE_OP_FAILED;
	}
	free(strLine);
	if (ret == RET_SUCCESS && (lineNO < 2 || pstClip->frameCnt == 0 || pstClip->pstFrames[0].t_us != 0))
		ret = ERR_FILE_FORMAT;

	if (ret != RET_SUCCESS)
	{
		printf("%s() rejected %s near line %u: ret=0x%08X\r\n", __FUNCTION__, filePath, lineNO, ret);
		Head_ReleaseMotionClip(pstClip);
		return ret;
	}
	*ppClip = pstClip;
	return RET_SUCCESS;
}

void Head_ReleaseMotionClip(AHEADFORM_MOTION_CLIP *pstClip)
{
	if (pstClip == NULL)
		return;

	free(pstClip->pstFrames);
	free(pstClip);
}

int Head_SampleMotionClip(const AHEADFORM_MOTION_CLIP *pstClip, unsigned long long t_us, AHEADFORM_MOTION_FRAME *pstFrame)
{
	const AHEADFORM_MOTION_FRAME *pstLeft = NULL, *pstRight = NULL;
	unsigned int left = 0, right, mid, i;
	double fraction;

	if (pstClip == NULL || pstClip->frameCnt == 0 || pstClip->pstFrames == NULL)
		return ERR_ARGV_1;

	if (pstFrame == NULL)
		return ERR_ARGV_3;

	right = pstClip->frameCnt - 1;
	if (t_us <= pstClip->pstFrames[0].t_us || t_us >= pstClip->pstFrames[right].t_us)
	{
		*pstFrame = pstClip->pstFrames[t_us <= pstClip->pstFrames[0].t_us ? 0 : right];
		return RET_SUCCESS;
	}

	while (right - left > 1)
	{
		mid = left + (right - left) / 2;
		if (pstClip->pstFrames[mid].t_us <= t_us)
			left = mid;
		else
			right = mid;
	}
	pstLeft = &pstClip->pstFrames[left];
	pstRight = &pstClip->pstFrames[right];
	fraction = (double)(t_us - pstLeft->t_us) / (pstRight->t_us - pstLeft->t_us);
	pstFrame->t_us = t_us;
	pstFrame->stNeck.pitch_rad = pstLeft->stNeck.pitch_rad + (pstRight->stNeck.pitch_rad - pstLeft->stNeck.pitch_rad) * fraction;
	pstFrame->stNeck.yaw_rad = pstLeft->stNeck.yaw_rad + (pstRight->stNeck.yaw_rad - pstLeft->stNeck.yaw_rad) * fraction;
	pstFrame->stNeck.roll_rad = pstLeft->stNeck.roll_rad + (pstRight->stNeck.roll_rad - pstLeft->stNeck.roll_rad) * fraction;
	pstFrame->stNeck.headX = pstLeft->stNeck.headX + (pstRight->stNeck.headX - pstLeft->stNeck.headX) * fraction;
	pstFrame->stNeck.headY = pstLeft->stNeck.headY + (pstRight->stNeck.headY - pstLeft->stNeck.headY) * fraction;
	pstFrame->stNeck.headZ = pstLeft->stNeck.headZ + (pstRight->stNeck.headZ - pstLeft->stNeck.headZ) * fraction;
	pstFrame->stEyes.leftPitch_rad = pstLeft->stEyes.leftPitch_rad + (pstRight->stEyes.leftPitch_rad - pstLeft->stEyes.leftPitch_rad) * fraction;
	pstFrame->stEyes.leftYaw_rad = pstLeft->stEyes.leftYaw_rad + (pstRight->stEyes.leftYaw_rad - pstLeft->stEyes.leftYaw_rad) * fraction;
	pstFrame->stEyes.rightPitch_rad = pstLeft->stEyes.rightPitch_rad + (pstRight->stEyes.rightPitch_rad - pstLeft->stEyes.rightPitch_rad) * fraction;
	pstFrame->stEyes.rightYaw_rad = pstLeft->stEyes.rightYaw_rad + (pstRight->stEyes.rightYaw_rad - pstLeft->stEyes.rightYaw_rad) * fraction;
	for (i = 0; i < AHEADFORM_BLENDSHAPE_CNT; i++)
		pstFrame->blendshapes[i] = pstLeft->blendshapes[i] + (pstRight->blendshapes[i] - pstLeft->blendshapes[i]) * fraction;

	return RET_SUCCESS;
}
