#ifndef __AHEADFORM_AUDIO_FILE_H__
#define __AHEADFORM_AUDIO_FILE_H__

#include "PublicDef.h"

typedef struct
{
	unsigned int SampleRate_Hz;
	unsigned int channelCnt;
} AHEADFORM_AUDIO_PCM_FORMAT;

typedef struct
{
	AHEADFORM_AUDIO_PCM_FORMAT stFormat;
	unsigned long long sampleFrameCnt;
	unsigned long long duration_us;
} AHEADFORM_AUDIO_FILE_INFO;

/*
	Input is detected from content; format support depends on installed plugins.
	Output is 16000 Hz mono S16LE; pcmBufSize is bytes.
	Outputs must not overlap; failure preserves info but may partly write PCM.
	Return RET_SUCCESS or ERR_*; use output only after success.
*/
int Audio_DecodeFileToPcm(const char *filePath, void *pRetPcmBuf, size_t pcmBufSize, AHEADFORM_AUDIO_FILE_INFO *pstRetInfo);

#endif
