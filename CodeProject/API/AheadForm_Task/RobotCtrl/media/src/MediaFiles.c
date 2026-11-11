#include "Media.h"

#include <errno.h>
#include <limits.h>
#include <stdint.h>

static unsigned short ReadUint16Le(const unsigned char *pValue)
{
	return (unsigned short)pValue[0] | (unsigned short)pValue[1] << 8;
}

static unsigned int ReadUint32Le(const unsigned char *pValue)
{
	return (unsigned int)pValue[0] | (unsigned int)pValue[1] << 8 | (unsigned int)pValue[2] << 16 | (unsigned int)pValue[3] << 24;
}

static void WriteUint16Le(unsigned char *pValue, unsigned short value)
{
	pValue[0] = (unsigned char)value;
	pValue[1] = (unsigned char)(value >> 8);
}

static void WriteUint32Le(unsigned char *pValue, unsigned int value)
{
	pValue[0] = (unsigned char)value;
	pValue[1] = (unsigned char)(value >> 8);
	pValue[2] = (unsigned char)(value >> 16);
	pValue[3] = (unsigned char)(value >> 24);
}

int Media_OpenWavReader(const char *filePath, AHEADFORM_WAV_FILE *pstWav)
{
	FILE *pFile = NULL;
	unsigned char szChunkHeader[8], szFormat[16], szContainerHeader[12];
	unsigned int chunkSize, riffSize, dataSize;
	long fileSize, containerEnd;
	unsigned char isFormatValid = 0;

	if (filePath == NULL || filePath[0] == 0)
		return ERR_ARGV_1;

	if (pstWav == NULL)
		return ERR_ARGV_2;

	if (pstWav->pFile != NULL)
		return ERR_RESOURCE_BUSY;

	if ( (pFile = fopen(filePath, "rb")) == NULL )
	{
		printf("%s() call fopen(%s) failed: %s\r\n", __FUNCTION__, filePath, strerror(errno));
		return errno == ENOENT ? ERR_FILE_NOT_FOUND : ERR_FILE_OP_FAILED;
	}
	if (fseek(pFile, 0, SEEK_END) != 0 || (fileSize = ftell(pFile)) < 0 || fseek(pFile, 0, SEEK_SET) != 0)
	{
		printf("%s() call seek/tell(%s) failed: %s\r\n", __FUNCTION__, filePath, strerror(errno));
		fclose(pFile);
		return ERR_FILE_OP_FAILED;
	}
	if (fileSize < (long)sizeof(szContainerHeader))
	{
		fclose(pFile);
		return ERR_FILE_FORMAT;
	}
	if (fread(szContainerHeader, 1, sizeof(szContainerHeader), pFile) != sizeof(szContainerHeader) || memcmp(szContainerHeader, "RIFF", 4) != 0 || memcmp(szContainerHeader + 8, "WAVE", 4) != 0)
	{
		fclose(pFile);
		return ERR_FILE_FORMAT;
	}
	riffSize = ReadUint32Le(szContainerHeader + 4);
	if (riffSize < 4 || (unsigned long long)riffSize + 8 > (unsigned long long)fileSize)
	{
		fclose(pFile);
		return ERR_FILE_FORMAT;
	}
	containerEnd = (long)riffSize + 8;

	while (ftell(pFile) <= containerEnd - (long)sizeof(szChunkHeader))
	{
		if (fread(szChunkHeader, 1, sizeof(szChunkHeader), pFile) != sizeof(szChunkHeader))
			break;

		chunkSize = ReadUint32Le(szChunkHeader + 4);
		if ((unsigned long long)ftell(pFile) + chunkSize + (chunkSize & 1) > (unsigned long long)containerEnd)
			break;

		if (memcmp(szChunkHeader, "fmt ", 4) == 0)
		{
			if (chunkSize < sizeof(szFormat) || fread(szFormat, 1, sizeof(szFormat), pFile) != sizeof(szFormat))
				break;

			isFormatValid = ReadUint16Le(szFormat) == 1 && ReadUint16Le(szFormat + 2) == AHEADFORM_PCM_CHANNEL_CNT
				&& ReadUint32Le(szFormat + 4) == AHEADFORM_PCM_SAMPLE_RATE && ReadUint32Le(szFormat + 8) == AHEADFORM_PCM_SAMPLE_RATE * 2
				&& ReadUint16Le(szFormat + 12) == 2 && ReadUint16Le(szFormat + 14) == 16;
			if (!isFormatValid)
				break;

			if (fseek(pFile, (long)(chunkSize - sizeof(szFormat)) + (chunkSize & 1), SEEK_CUR) != 0)
				break;
		}
		else if (memcmp(szChunkHeader, "data", 4) == 0)
		{
			if (!isFormatValid || (chunkSize & 1) != 0)
				break;

			dataSize = chunkSize;
			memset(pstWav, 0, sizeof(*pstWav));
			pstWav->pFile = pFile;
			pstWav->dataSize = dataSize;
			pstWav->remainingSize = dataSize;
			return RET_SUCCESS;
		}
		else if (fseek(pFile, (long)chunkSize + (chunkSize & 1), SEEK_CUR) != 0)
		{
			break;
		}
	}

	fclose(pFile);
	return ERR_FILE_FORMAT;
}

int Media_ReadWavPcm(AHEADFORM_WAV_FILE *pstWav, void *pPcm, unsigned int pcmBufSize)
{
	unsigned int readSize;
	size_t actualSize;

	if (pstWav == NULL || pstWav->pFile == NULL || pstWav->isWriting)
		return ERR_ARGV_1;

	if (pPcm == NULL)
		return ERR_ARGV_2;

	if (pcmBufSize < 2)
		return ERR_ARGV_3;

	if (pstWav->remainingSize == 0)
		return 0;

	readSize = pcmBufSize < pstWav->remainingSize ? pcmBufSize : pstWav->remainingSize;
	if (readSize > INT_MAX)
		readSize = INT_MAX;

	readSize &= ~1U;
	actualSize = fread(pPcm, 1, readSize, pstWav->pFile);
	if (actualSize != readSize)
	{
		printf("%s() call fread(WAV PCM) failed: %s\r\n", __FUNCTION__, ferror(pstWav->pFile) ? strerror(errno) : "unexpected end of file");
		return ERR_FILE_OP_FAILED;
	}
	pstWav->remainingSize -= readSize;
	return (int)readSize;
}

int Media_OpenWavWriter(const char *filePath, AHEADFORM_WAV_FILE *pstWav)
{
	unsigned char szHeader[44] = {0};
	FILE *pFile = NULL;

	if (filePath == NULL || filePath[0] == 0)
		return ERR_ARGV_1;

	if (pstWav == NULL)
		return ERR_ARGV_2;

	if (pstWav->pFile != NULL)
		return ERR_RESOURCE_BUSY;

	if ( (pFile = fopen(filePath, "wb+")) == NULL )
	{
		printf("%s() call fopen(%s) failed: %s\r\n", __FUNCTION__, filePath, strerror(errno));
		return ERR_FILE_OP_FAILED;
	}
	memcpy(szHeader, "RIFF", 4);
	memcpy(szHeader + 8, "WAVEfmt ", 8);
	WriteUint32Le(szHeader + 16, 16);
	WriteUint16Le(szHeader + 20, 1);
	WriteUint16Le(szHeader + 22, AHEADFORM_PCM_CHANNEL_CNT);
	WriteUint32Le(szHeader + 24, AHEADFORM_PCM_SAMPLE_RATE);
	WriteUint32Le(szHeader + 28, AHEADFORM_PCM_SAMPLE_RATE * 2);
	WriteUint16Le(szHeader + 32, 2);
	WriteUint16Le(szHeader + 34, 16);
	memcpy(szHeader + 36, "data", 4);
	if (fwrite(szHeader, 1, sizeof(szHeader), pFile) != sizeof(szHeader))
	{
		printf("%s() call fwrite(WAV header) failed: %s\r\n", __FUNCTION__, strerror(errno));
		fclose(pFile);
		return ERR_FILE_OP_FAILED;
	}
	memset(pstWav, 0, sizeof(*pstWav));
	pstWav->pFile = pFile;
	pstWav->isWriting = 1;
	return RET_SUCCESS;
}

int Media_WriteWavPcm(AHEADFORM_WAV_FILE *pstWav, const void *pPcm, unsigned int pcmSize)
{
	if (pstWav == NULL || pstWav->pFile == NULL || !pstWav->isWriting)
		return ERR_ARGV_1;

	if (pPcm == NULL)
		return ERR_ARGV_2;

	if (pcmSize == 0 || (pcmSize & 1) != 0 || pcmSize > UINT_MAX - pstWav->dataSize || pcmSize + pstWav->dataSize > UINT_MAX - 36)
		return ERR_ARGV_3;

	if (fwrite(pPcm, 1, pcmSize, pstWav->pFile) != pcmSize)
	{
		printf("%s() call fwrite(WAV PCM, size = %u) failed: %s\r\n", __FUNCTION__, pcmSize, strerror(errno));
		return ERR_FILE_OP_FAILED;
	}
	pstWav->dataSize += pcmSize;
	return RET_SUCCESS;
}

int Media_CloseWav(AHEADFORM_WAV_FILE *pstWav)
{
	FILE *pFile = NULL;
	unsigned char szRiffSize[4], szDataSize[4];
	int ret = RET_SUCCESS;

	if (pstWav == NULL)
		return ERR_ARGV_1;

	if (pstWav->pFile == NULL)
		return RET_SUCCESS;

	pFile = pstWav->pFile;
	WriteUint32Le(szRiffSize, pstWav->dataSize + 36);
	WriteUint32Le(szDataSize, pstWav->dataSize);
	if (pstWav->isWriting && (fseek(pFile, 4, SEEK_SET) != 0 || fwrite(szRiffSize, 1, 4, pFile) != 4 || fseek(pFile, 40, SEEK_SET) != 0 || fwrite(szDataSize, 1, 4, pFile) != 4))
	{
		printf("%s() call update(WAV size header) failed: %s\r\n", __FUNCTION__, strerror(errno));
		ret = ERR_FILE_OP_FAILED;
	}
	pstWav->pFile = NULL;
	if (fclose(pFile) != 0)
	{
		printf("%s() call fclose(WAV) failed: %s\r\n", __FUNCTION__, strerror(errno));
		ret = ERR_FILE_OP_FAILED;
	}
	memset(pstWav, 0, sizeof(*pstWav));
	return ret;
}

int Media_OpenH264Writer(const char *dataFilePath, const char *timeFilePath, AHEADFORM_H264_FILE *pstFile)
{
	FILE *pDataFile = NULL, *pTimeFile = NULL;

	if (dataFilePath == NULL || dataFilePath[0] == 0)
		return ERR_ARGV_1;

	if (timeFilePath == NULL || timeFilePath[0] == 0)
		return ERR_ARGV_2;

	if (strcmp(dataFilePath, timeFilePath) == 0)
		return ERR_ARGV_2;

	if (pstFile == NULL)
		return ERR_ARGV_3;

	if (pstFile->pDataFile != NULL || pstFile->pTimeFile != NULL)
		return ERR_RESOURCE_BUSY;

	if ( (pDataFile = fopen(dataFilePath, "wb")) == NULL )
	{
		printf("%s() call fopen(%s) failed: %s\r\n", __FUNCTION__, dataFilePath, strerror(errno));
		return ERR_FILE_OP_FAILED;
	}
	if ( (pTimeFile = fopen(timeFilePath, "w")) == NULL )
	{
		printf("%s() call fopen(%s) failed: %s\r\n", __FUNCTION__, timeFilePath, strerror(errno));
		fclose(pDataFile);
		return ERR_FILE_OP_FAILED;
	}
	if (fprintf(pTimeFile, "offset,size,sequence,receive_time_us,pts_epoch_us,pts_ns,pts_valid,width,height\r\n") < 0)
	{
		printf("%s() call fprintf(%s) failed: %s\r\n", __FUNCTION__, timeFilePath, strerror(errno));
		fclose(pTimeFile);
		fclose(pDataFile);
		return ERR_FILE_OP_FAILED;
	}
	memset(pstFile, 0, sizeof(*pstFile));
	pstFile->pDataFile = pDataFile;
	pstFile->pTimeFile = pTimeFile;
	pstFile->isWaitingForIdr = 1;
	return RET_SUCCESS;
}

int Media_WriteH264Frame(AHEADFORM_H264_FILE *pstFile, const AHEADFORM_MEDIA_FRAME *pstFrame)
{
	if (pstFile == NULL || pstFile->pDataFile == NULL || pstFile->pTimeFile == NULL)
		return ERR_ARGV_1;

	if (pstFrame == NULL)
		return ERR_ARGV_2;

	if (pstFrame->format != AHEADFORM_MEDIA_FORMAT_H264_ANNEX_B || pstFrame->dataSize == 0)
		return ERR_ARGV_2;

	if (pstFrame->isDiscontinuous)
		pstFile->isWaitingForIdr = 1;

	if (pstFile->isWaitingForIdr && (!pstFrame->isKeyframe || !pstFrame->hasParameterSets))
		return 0;

	if (pstFrame->dataSize > ULLONG_MAX - pstFile->dataOffset)
		return ERR_OVER_BUF;

	if (fwrite(pstFrame->data, 1, pstFrame->dataSize, pstFile->pDataFile) != pstFrame->dataSize)
	{
		printf("%s() call fwrite(H.264 AU, size = %u) failed: %s\r\n", __FUNCTION__, pstFrame->dataSize, strerror(errno));
		pstFile->isWaitingForIdr = 1;
		return ERR_FILE_OP_FAILED;
	}
	if (fprintf(pstFile->pTimeFile, "%llu,%u,%llu,%llu,%llu,%llu,%u,%u,%u\r\n",
		pstFile->dataOffset, pstFrame->dataSize, pstFrame->sequence, pstFrame->ReceiveTime_us,
		pstFrame->ptsEpoch_us, pstFrame->pts_ns, pstFrame->isPtsValid, pstFrame->width, pstFrame->height) < 0)
	{
		printf("%s() call fprintf(H.264 index) failed: %s\r\n", __FUNCTION__, strerror(errno));
		pstFile->isWaitingForIdr = 1;
		return ERR_FILE_OP_FAILED;
	}
	pstFile->dataOffset += pstFrame->dataSize;
	pstFile->isWaitingForIdr = 0;
	return 1;
}

int Media_CloseH264Writer(AHEADFORM_H264_FILE *pstFile)
{
	FILE *pDataFile = NULL, *pTimeFile = NULL;
	int ret = RET_SUCCESS;

	if (pstFile == NULL)
		return ERR_ARGV_1;

	pDataFile = pstFile->pDataFile;
	pTimeFile = pstFile->pTimeFile;
	memset(pstFile, 0, sizeof(*pstFile));
	if (pTimeFile != NULL && fclose(pTimeFile) != 0)
	{
		printf("%s() call fclose(H.264 index) failed: %s\r\n", __FUNCTION__, strerror(errno));
		ret = ERR_FILE_OP_FAILED;
	}
	if (pDataFile != NULL && fclose(pDataFile) != 0)
	{
		printf("%s() call fclose(H.264 data) failed: %s\r\n", __FUNCTION__, strerror(errno));
		ret = ERR_FILE_OP_FAILED;
	}
	return ret;
}
