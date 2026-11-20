#include "HeadMotionFile.h"
#include "ParseFile.h"

#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define MOTION_CSV_REQUIRED_COLUMN_CNT 60
#define MOTION_CSV_USED_COLUMN_CNT 63
#define CALC_INTERPOLATE_MOTION_VALUE(before, after, ratio) ((float)((double)(before) + ((double)(after) - (double)(before)) * (ratio)))

static const char* const gMotionCsvPoseNames[] =
{
	"neckPitch", "neckYaw", "neckRoll",
	"eyePitchLeft", "eyeYawLeft", "eyePitchRight", "eyeYawRight",
	"neckX", "neckY", "neckZ"
};

/*
	szRetFieldColumnIndex contains zero-based CSV column indexes:
	[0]: timestamp; [1..52]: gAheadFormBlendshapeNames[0..51].
	[53..55]: neckPitch, neckYaw, neckRoll.
	[56..59]: eyePitchLeft, eyeYawLeft, eyePitchRight, eyeYawRight.
	[60..62]: neckX, neckY, neckZ; UINT_MAX means an absent optional column.
*/
static int ParseMotionCsvHeader(char *strHeader, unsigned int szRetFieldColumnIndex[], unsigned int *pRetColumnCnt)
{
	char *pName = strHeader, *pNext = NULL, *pPrevious = NULL;
	unsigned int columnCnt = 0, i;

	for (i = 0; i < MOTION_CSV_USED_COLUMN_CNT; i++)
		szRetFieldColumnIndex[i] = UINT_MAX;

	while (pName != NULL)
	{
		pNext = strchr(pName, ',');
		if (pNext != NULL)
			*pNext++ = 0;

		if (*pName == 0 || strchr(pName, '"') != NULL)
			return ERR_FILE_FORMAT;

		for (pPrevious = strHeader; pPrevious < pName; pPrevious += strlen(pPrevious) + 1)
		{
			if (strcmp(pPrevious, pName) == 0)
				return ERR_FILE_FORMAT;
		}

		if (strcmp(pName, "timestamp") == 0)
		{
			szRetFieldColumnIndex[0] = columnCnt;
		}
		else
		{
			for (i = 0; i < AHEADFORM_BLENDSHAPE_CNT; i++)
			{
				if (strcmp(pName, gAheadFormBlendshapeNames[i]) == 0)
					szRetFieldColumnIndex[i + 1] = columnCnt;
			}

			for (i = 0; i < sizeof(gMotionCsvPoseNames) / sizeof(gMotionCsvPoseNames[0]); i++)
			{
				if (strcmp(pName, gMotionCsvPoseNames[i]) == 0)
					szRetFieldColumnIndex[i + 53] = columnCnt;
			}
		}
		columnCnt++;
		pName = pNext;
	}

	for (i = 0; i < MOTION_CSV_REQUIRED_COLUMN_CNT; i++)
	{
		if (szRetFieldColumnIndex[i] == UINT_MAX)
			return ERR_FILE_FORMAT;
	}
	*pRetColumnCnt = columnCnt;
	return RET_SUCCESS;
}

/*
	szFieldColumnIndex contains zero-based CSV column indexes:
	[0]: timestamp; [1..52]: gAheadFormBlendshapeNames[0..51].
	[53..55]: neckPitch, neckYaw, neckRoll.
	[56..59]: eyePitchLeft, eyeYawLeft, eyePitchRight, eyeYawRight.
	[60..62]: neckX, neckY, neckZ; UINT_MAX means an absent optional column.
*/
static int ParseMotionCsvLineToPose(char *strLine, const unsigned int szFieldColumnIndex[], unsigned int csvColumnCnt, AHEADFORM_MOTION_POSE *pstRetPose, long double *pRetTimestamp_s)
{
	char *pField = strLine, *pNext = NULL, *pEnd = NULL;
	long double szValue[MOTION_CSV_USED_COLUMN_CNT] = {0}, value;
	unsigned int column = 0, i;

	while (pField != NULL)
	{
		if (column >= csvColumnCnt)
			return ERR_FILE_FORMAT;

		pNext = strchr(pField, ',');
		if (pNext != NULL)
			*pNext++ = 0;

		value = strtold(pField, &pEnd);
		if (pEnd == pField || !isfinite(value))
			return ERR_MOTION_DATA_INVALID;

		while (*pEnd == ' ' || *pEnd == '\t')
			pEnd++;

		if (*pEnd != 0)
			return ERR_MOTION_DATA_INVALID;

		for (i = 0; i < MOTION_CSV_USED_COLUMN_CNT; i++)
		{
			if (szFieldColumnIndex[i] == column)
				szValue[i] = value;
		}
		column++;
		pField = pNext;
	}

	if (column != csvColumnCnt)
		return ERR_FILE_FORMAT;

	if (szValue[0] < 0.0L || szValue[0] > 600.0L)
		return ERR_TIMELINE_INVALID;

	for (i = 0; i < AHEADFORM_BLENDSHAPE_CNT; i++)
	{
		if (szValue[i + 1] < 0.0L || szValue[i + 1] > 1.0L)
			return ERR_MOTION_DATA_INVALID;

		pstRetPose->blendshapes[i] = (float)szValue[i + 1];
	}

	for (i = 53; i < MOTION_CSV_USED_COLUMN_CNT; i++)
	{
		if (fabsl(szValue[i]) > FLT_MAX)
			return ERR_MOTION_DATA_INVALID;
	}
	pstRetPose->t_us = (unsigned long long)roundl(szValue[0] * 1000000.0L);
	pstRetPose->stNeck.pitch_rad = (float)szValue[53];
	pstRetPose->stNeck.yaw_rad = (float)szValue[54];
	pstRetPose->stNeck.roll_rad = (float)szValue[55];
	pstRetPose->stEyes.leftPitch_rad = (float)szValue[56];
	pstRetPose->stEyes.leftYaw_rad = (float)szValue[57];
	pstRetPose->stEyes.rightPitch_rad = (float)szValue[58];
	pstRetPose->stEyes.rightYaw_rad = (float)szValue[59];
	pstRetPose->stNeck.headX = (float)szValue[60];
	pstRetPose->stNeck.headY = (float)szValue[61];
	pstRetPose->stNeck.headZ = (float)szValue[62];
	*pRetTimestamp_s = szValue[0];
	return RET_SUCCESS;
}

static int ParseMotionCsvContent(char *csvBuf, size_t dataLen, AHEADFORM_MOTION_POSE *pstRetPoseAry, size_t poseArySize, AHEADFORM_MOTION_FILE_INFO *pstRetInfo)
{
	AHEADFORM_MOTION_FILE_INFO stInfo = {0};
	AHEADFORM_MOTION_POSE stPose;
	char *pLine = csvBuf, *pNextLine = NULL;
	/*
		szFieldColumnIndex contains zero-based CSV column indexes:
		[0]: timestamp; [1..52]: gAheadFormBlendshapeNames[0..51].
		[53..55]: neckPitch, neckYaw, neckRoll.
		[56..59]: eyePitchLeft, eyeYawLeft, eyePitchRight, eyeYawRight.
		[60..62]: neckX, neckY, neckZ; UINT_MAX means an absent optional column.
	*/
	unsigned int szFieldColumnIndex[MOTION_CSV_USED_COLUMN_CNT];
	unsigned int columnCnt, lineNo = 1;
	unsigned long long firstTime_us = 0;
	long double time_s, previousTime_s = 0.0L;
	size_t lineLen, maxPoseCnt = poseArySize / sizeof(*pstRetPoseAry);
	int ret;

	if (memchr(csvBuf, 0, dataLen) != NULL)
		return ERR_FILE_FORMAT;

	if (dataLen >= 3 && memcmp(pLine, "\xEF\xBB\xBF", 3) == 0)
		pLine += 3;

	pNextLine = strchr(pLine, '\n');
	if (pNextLine == NULL)
		return ERR_FILE_FORMAT;

	*pNextLine++ = 0;
	lineLen = strlen(pLine);
	if (lineLen > 0 && pLine[lineLen - 1] == '\r')
		pLine[lineLen - 1] = 0;

	if ( (ret = ParseMotionCsvHeader(pLine, szFieldColumnIndex, &columnCnt)) != RET_SUCCESS )
		return ret;

	pLine = pNextLine;
	while (pLine != NULL)
	{
		lineNo++;
		pNextLine = strchr(pLine, '\n');
		if (pNextLine != NULL)
			*pNextLine++ = 0;

		lineLen = strlen(pLine);
		if (lineLen > 0 && pLine[lineLen - 1] == '\r')
			pLine[lineLen - 1] = 0;

		if (*pLine != 0)
		{
			if ( (ret = ParseMotionCsvLineToPose(pLine, szFieldColumnIndex, columnCnt, &stPose, &time_s)) != RET_SUCCESS )
			{
				printf("%s() invalid CSV data at line %u.\r\n", __FUNCTION__, lineNo);
				return ret;
			}

			if (stInfo.frameCnt != 0 && time_s < previousTime_s)
				return ERR_TIMELINE_INVALID;

			previousTime_s = time_s;
			if (stInfo.frameCnt == 0)
				firstTime_us = stPose.t_us;

			stPose.t_us -= firstTime_us;
			if (pstRetPoseAry != NULL)
			{
				if (stInfo.frameCnt >= maxPoseCnt)
					return ERR_OVER_BUF;

				pstRetPoseAry[stInfo.frameCnt] = stPose;
			}
			stInfo.frameCnt++;
			stInfo.duration_us = stPose.t_us;
		}
		pLine = pNextLine;
	}

	if (stInfo.frameCnt == 0)
		return ERR_FILE_FORMAT;

	if (previousTime_s < 0.001L)
		return ERR_TIMELINE_INVALID;

	*pstRetInfo = stInfo;
	return RET_SUCCESS;
}

int Head_CheckMotionCsv(const char *filePath)
{
	AHEADFORM_MOTION_FILE_INFO stInfo;
	char *csvBuf = NULL;
	int ret;

	if (filePath == NULL || filePath[0] == 0)
		return ERR_ARGV_1;

	if ( (csvBuf = (char*)malloc(AHEADFORM_MOTION_CSV_FILE_SIZE_MAX + 1)) == NULL )
		return ERR_MEMORY_ALLOC;

	if ( (ret = GetAllContentOfFile(filePath, csvBuf, AHEADFORM_MOTION_CSV_FILE_SIZE_MAX + 1)) >= 0 )
		ret = ParseMotionCsvContent(csvBuf, (size_t)ret, NULL, 0, &stInfo);

	free(csvBuf);
	return ret;
}

int Head_LoadMotionCsv(const char *filePath, AHEADFORM_MOTION_POSE *pstRetPoseAry, size_t poseArySize, AHEADFORM_MOTION_FILE_INFO *pstRetInfo)
{
	char *csvBuf = NULL;
	int ret;

	if (filePath == NULL || filePath[0] == 0)
		return ERR_ARGV_1;

	if (pstRetPoseAry == NULL)
		return ERR_ARGV_2;

	if (poseArySize < sizeof(*pstRetPoseAry))
		return ERR_ARGV_3;

	if (pstRetInfo == NULL)
		return ERR_ARGV_4;

	if ( (csvBuf = (char*)malloc(AHEADFORM_MOTION_CSV_FILE_SIZE_MAX + 1)) == NULL )
		return ERR_MEMORY_ALLOC;

	if ( (ret = GetAllContentOfFile(filePath, csvBuf, AHEADFORM_MOTION_CSV_FILE_SIZE_MAX + 1)) >= 0 )
		ret = ParseMotionCsvContent(csvBuf, (size_t)ret, pstRetPoseAry, poseArySize, pstRetInfo);

	free(csvBuf);
	return ret;
}

//Return poseCnt if no later pose exists.
static unsigned int FindFirstLaterMotionPoseIndex(const AHEADFORM_MOTION_POSE *pstPoseAry, unsigned int poseCnt, unsigned long long targetTime_us)
{
	unsigned int low = 0, high = poseCnt, middle;

	while (low < high)
	{
		middle = low + (high - low) / 2;
		if (pstPoseAry[middle].t_us <= targetTime_us)
			low = middle + 1;
		else
			high = middle;
	}

	return low;
}

int Head_CalcInterpolateMotionPose(const AHEADFORM_MOTION_POSE *pstPoseAry, unsigned int poseCnt, unsigned long long targetTime_us, AHEADFORM_MOTION_POSE *pstRetPose)
{
	AHEADFORM_MOTION_POSE stPose;
	unsigned int beforeIndex, afterIndex, i;
	double ratio;

	if (pstPoseAry == NULL)
		return ERR_ARGV_1;

	if (poseCnt == 0)
		return ERR_ARGV_2;

	if (pstRetPose == NULL)
		return ERR_ARGV_4;

	if (pstPoseAry[0].t_us != 0)
		return ERR_TIMELINE_INVALID;

	afterIndex = FindFirstLaterMotionPoseIndex(pstPoseAry, poseCnt, targetTime_us);
	beforeIndex = afterIndex - 1;
	stPose = pstPoseAry[beforeIndex];
	if (afterIndex < poseCnt && stPose.t_us != targetTime_us)
	{
		afterIndex = FindFirstLaterMotionPoseIndex(pstPoseAry, poseCnt, pstPoseAry[afterIndex].t_us) - 1;
		if (pstPoseAry[afterIndex].t_us <= stPose.t_us)
			return ERR_TIMELINE_INVALID;

		ratio = (double)(targetTime_us - stPose.t_us) / (double)(pstPoseAry[afterIndex].t_us - stPose.t_us);
		stPose.stNeck.pitch_rad = CALC_INTERPOLATE_MOTION_VALUE(stPose.stNeck.pitch_rad, pstPoseAry[afterIndex].stNeck.pitch_rad, ratio);
		stPose.stNeck.yaw_rad = CALC_INTERPOLATE_MOTION_VALUE(stPose.stNeck.yaw_rad, pstPoseAry[afterIndex].stNeck.yaw_rad, ratio);
		stPose.stNeck.roll_rad = CALC_INTERPOLATE_MOTION_VALUE(stPose.stNeck.roll_rad, pstPoseAry[afterIndex].stNeck.roll_rad, ratio);
		stPose.stNeck.headX = CALC_INTERPOLATE_MOTION_VALUE(stPose.stNeck.headX, pstPoseAry[afterIndex].stNeck.headX, ratio);
		stPose.stNeck.headY = CALC_INTERPOLATE_MOTION_VALUE(stPose.stNeck.headY, pstPoseAry[afterIndex].stNeck.headY, ratio);
		stPose.stNeck.headZ = CALC_INTERPOLATE_MOTION_VALUE(stPose.stNeck.headZ, pstPoseAry[afterIndex].stNeck.headZ, ratio);
		stPose.stEyes.leftPitch_rad = CALC_INTERPOLATE_MOTION_VALUE(stPose.stEyes.leftPitch_rad, pstPoseAry[afterIndex].stEyes.leftPitch_rad, ratio);
		stPose.stEyes.leftYaw_rad = CALC_INTERPOLATE_MOTION_VALUE(stPose.stEyes.leftYaw_rad, pstPoseAry[afterIndex].stEyes.leftYaw_rad, ratio);
		stPose.stEyes.rightPitch_rad = CALC_INTERPOLATE_MOTION_VALUE(stPose.stEyes.rightPitch_rad, pstPoseAry[afterIndex].stEyes.rightPitch_rad, ratio);
		stPose.stEyes.rightYaw_rad = CALC_INTERPOLATE_MOTION_VALUE(stPose.stEyes.rightYaw_rad, pstPoseAry[afterIndex].stEyes.rightYaw_rad, ratio);
		for (i = 0; i < AHEADFORM_BLENDSHAPE_CNT; i++)
			stPose.blendshapes[i] = CALC_INTERPOLATE_MOTION_VALUE(stPose.blendshapes[i], pstPoseAry[afterIndex].blendshapes[i], ratio);
	}
	stPose.t_us = targetTime_us;
	*pstRetPose = stPose;
	return RET_SUCCESS;
}
