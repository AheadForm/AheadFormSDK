#include "Status.h"

int Status_Init(AHEADFORM_STATUS *pstStatus, AHEADFORM_TOPICS *pstTopics)
{
	AHEADFORM_TOPIC_READER_PARA stReaderPara = {0};
	unsigned int i;

	if (pstStatus == NULL)
		return ERR_ARGV_1;

	if (pstTopics == NULL || pstTopics->stDdsCtrl.ddsParticipant <= 0)
		return ERR_ARGV_2;

	memset(pstStatus, 0, sizeof(*pstStatus));
	pstStatus->pstTopics = pstTopics;
	for (i = 0; i < AHEADFORM_STATUS_TOPIC_CNT; i++)
	{
		pstStatus->szpstReader[i] = AheadFormTopics_RegisterReader(pstTopics, (ENUM_AHEADFORM_TOPIC_ID)(AHEADFORM_TOPIC_ID_HARDWARE_STATE + i), &stReaderPara);
		if (pstStatus->szpstReader[i] == NULL)
			return ERRCODE_INIT_FAIL;
	}

	return RET_SUCCESS;
}

int Status_Deinit(AHEADFORM_STATUS *pstStatus)
{
	unsigned int i;
	int ret;

	if (pstStatus == NULL)
		return ERR_ARGV_1;

	for (i = 0; i < AHEADFORM_STATUS_TOPIC_CNT; i++)
	{
		if (pstStatus->szpstReader[i] != NULL)
		{
			if ( (ret = AheadFormTopics_UnregisterReader(pstStatus->pstTopics, pstStatus->szpstReader[i])) != RET_SUCCESS )
				return ret;

			pstStatus->szpstReader[i] = NULL;
		}
	}
	pstStatus->pstTopics = NULL;
	return RET_SUCCESS;
}

AHEADFORM_TOPIC_READER* Status_RegisterHardwareStateReader(AHEADFORM_STATUS *pstStatus, const AHEADFORM_TOPIC_READER_PARA *pstReaderPara)
{
	if (pstStatus == NULL || pstStatus->pstTopics == NULL)
	{
		PRINT_FUNCTION_ERRINFO(ERR_ARGV_1, __FUNCTION__);
		return NULL;
	}

	return AheadFormTopics_RegisterReader(pstStatus->pstTopics, AHEADFORM_TOPIC_ID_HARDWARE_STATE, pstReaderPara);
}

int Status_ReadLatestHardwareState(AHEADFORM_STATUS *pstStatus, AHEADFORM_HARDWARE_STATE_SAMPLE *pstSample)
{
	AHEADFORM_TOPIC_READER *pstReader = NULL;
	int ret;

	if (pstStatus == NULL)
		return ERR_ARGV_1;

	if (pstSample == NULL)
		return ERR_ARGV_2;

	pstReader = pstStatus->szpstReader[AHEADFORM_TOPIC_ID_HARDWARE_STATE - AHEADFORM_TOPIC_ID_HARDWARE_STATE];
	if (pstReader == NULL)
		return ERR_SDK_NOT_READY;

	if ( (ret = TopicReader_GetLastReceiveResult(pstReader)) != RET_SUCCESS )
		return ret;

	if ( (ret = TopicReader_ReadLatest1Elem(pstReader, pstSample)) != 1 )
		return ret;

	if ( (ret = TopicReader_GetLastReceiveResult(pstReader)) != RET_SUCCESS )
		return ret;

	return 1;
}

AHEADFORM_TOPIC_READER* Status_RegisterFaceStateReader(AHEADFORM_STATUS *pstStatus, const AHEADFORM_TOPIC_READER_PARA *pstReaderPara)
{
	if (pstStatus == NULL || pstStatus->pstTopics == NULL)
	{
		PRINT_FUNCTION_ERRINFO(ERR_ARGV_1, __FUNCTION__);
		return NULL;
	}

	return AheadFormTopics_RegisterReader(pstStatus->pstTopics, AHEADFORM_TOPIC_ID_FACE_STATE, pstReaderPara);
}

int Status_ReadLatestFaceState(AHEADFORM_STATUS *pstStatus, AHEADFORM_FACE_STATE_SAMPLE *pstSample)
{
	AHEADFORM_TOPIC_READER *pstReader = NULL;
	int ret;

	if (pstStatus == NULL)
		return ERR_ARGV_1;

	if (pstSample == NULL)
		return ERR_ARGV_2;

	pstReader = pstStatus->szpstReader[AHEADFORM_TOPIC_ID_FACE_STATE - AHEADFORM_TOPIC_ID_HARDWARE_STATE];
	if (pstReader == NULL)
		return ERR_SDK_NOT_READY;

	if ( (ret = TopicReader_GetLastReceiveResult(pstReader)) != RET_SUCCESS )
		return ret;

	if ( (ret = TopicReader_ReadLatest1Elem(pstReader, pstSample)) != 1 )
		return ret;

	if ( (ret = TopicReader_GetLastReceiveResult(pstReader)) != RET_SUCCESS )
		return ret;

	return 1;
}

AHEADFORM_TOPIC_READER* Status_RegisterNeckStateReader(AHEADFORM_STATUS *pstStatus, const AHEADFORM_TOPIC_READER_PARA *pstReaderPara)
{
	if (pstStatus == NULL || pstStatus->pstTopics == NULL)
	{
		PRINT_FUNCTION_ERRINFO(ERR_ARGV_1, __FUNCTION__);
		return NULL;
	}

	return AheadFormTopics_RegisterReader(pstStatus->pstTopics, AHEADFORM_TOPIC_ID_NECK_STATE, pstReaderPara);
}

int Status_ReadLatestNeckState(AHEADFORM_STATUS *pstStatus, AHEADFORM_NECK_STATE_SAMPLE *pstSample)
{
	AHEADFORM_TOPIC_READER *pstReader = NULL;
	int ret;

	if (pstStatus == NULL)
		return ERR_ARGV_1;

	if (pstSample == NULL)
		return ERR_ARGV_2;

	pstReader = pstStatus->szpstReader[AHEADFORM_TOPIC_ID_NECK_STATE - AHEADFORM_TOPIC_ID_HARDWARE_STATE];
	if (pstReader == NULL)
		return ERR_SDK_NOT_READY;

	if ( (ret = TopicReader_GetLastReceiveResult(pstReader)) != RET_SUCCESS )
		return ret;

	if ( (ret = TopicReader_ReadLatest1Elem(pstReader, pstSample)) != 1 )
		return ret;

	if ( (ret = TopicReader_GetLastReceiveResult(pstReader)) != RET_SUCCESS )
		return ret;

	return 1;
}

AHEADFORM_TOPIC_READER* Status_RegisterMediaStateReader(AHEADFORM_STATUS *pstStatus, const AHEADFORM_TOPIC_READER_PARA *pstReaderPara)
{
	if (pstStatus == NULL || pstStatus->pstTopics == NULL)
	{
		PRINT_FUNCTION_ERRINFO(ERR_ARGV_1, __FUNCTION__);
		return NULL;
	}

	return AheadFormTopics_RegisterReader(pstStatus->pstTopics, AHEADFORM_TOPIC_ID_MEDIA_STATE, pstReaderPara);
}

int Status_ReadLatestMediaState(AHEADFORM_STATUS *pstStatus, AHEADFORM_MEDIA_STATE_SAMPLE *pstSample)
{
	AHEADFORM_TOPIC_READER *pstReader = NULL;
	int ret;

	if (pstStatus == NULL)
		return ERR_ARGV_1;

	if (pstSample == NULL)
		return ERR_ARGV_2;

	pstReader = pstStatus->szpstReader[AHEADFORM_TOPIC_ID_MEDIA_STATE - AHEADFORM_TOPIC_ID_HARDWARE_STATE];
	if (pstReader == NULL)
		return ERR_SDK_NOT_READY;

	if ( (ret = TopicReader_GetLastReceiveResult(pstReader)) != RET_SUCCESS )
		return ret;

	if ( (ret = TopicReader_ReadLatest1Elem(pstReader, pstSample)) != 1 )
		return ret;

	if ( (ret = TopicReader_GetLastReceiveResult(pstReader)) != RET_SUCCESS )
		return ret;

	return 1;
}

AHEADFORM_TOPIC_READER* Status_RegisterPowerStateReader(AHEADFORM_STATUS *pstStatus, const AHEADFORM_TOPIC_READER_PARA *pstReaderPara)
{
	if (pstStatus == NULL || pstStatus->pstTopics == NULL)
	{
		PRINT_FUNCTION_ERRINFO(ERR_ARGV_1, __FUNCTION__);
		return NULL;
	}

	return AheadFormTopics_RegisterReader(pstStatus->pstTopics, AHEADFORM_TOPIC_ID_POWER_STATE, pstReaderPara);
}

int Status_ReadLatestPowerState(AHEADFORM_STATUS *pstStatus, AHEADFORM_POWER_STATE_SAMPLE *pstSample)
{
	AHEADFORM_TOPIC_READER *pstReader = NULL;
	int ret;

	if (pstStatus == NULL)
		return ERR_ARGV_1;

	if (pstSample == NULL)
		return ERR_ARGV_2;

	pstReader = pstStatus->szpstReader[AHEADFORM_TOPIC_ID_POWER_STATE - AHEADFORM_TOPIC_ID_HARDWARE_STATE];
	if (pstReader == NULL)
		return ERR_SDK_NOT_READY;

	if ( (ret = TopicReader_GetLastReceiveResult(pstReader)) != RET_SUCCESS )
		return ret;

	if ( (ret = TopicReader_ReadLatest1Elem(pstReader, pstSample)) != 1 )
		return ret;

	if ( (ret = TopicReader_GetLastReceiveResult(pstReader)) != RET_SUCCESS )
		return ret;

	return 1;
}

AHEADFORM_TOPIC_READER* Status_RegisterSensorStateReader(AHEADFORM_STATUS *pstStatus, const AHEADFORM_TOPIC_READER_PARA *pstReaderPara)
{
	if (pstStatus == NULL || pstStatus->pstTopics == NULL)
	{
		PRINT_FUNCTION_ERRINFO(ERR_ARGV_1, __FUNCTION__);
		return NULL;
	}

	return AheadFormTopics_RegisterReader(pstStatus->pstTopics, AHEADFORM_TOPIC_ID_SENSOR_STATE, pstReaderPara);
}

int Status_ReadLatestSensorState(AHEADFORM_STATUS *pstStatus, AHEADFORM_SENSOR_STATE_SAMPLE *pstSample)
{
	AHEADFORM_TOPIC_READER *pstReader = NULL;
	int ret;

	if (pstStatus == NULL)
		return ERR_ARGV_1;

	if (pstSample == NULL)
		return ERR_ARGV_2;

	pstReader = pstStatus->szpstReader[AHEADFORM_TOPIC_ID_SENSOR_STATE - AHEADFORM_TOPIC_ID_HARDWARE_STATE];
	if (pstReader == NULL)
		return ERR_SDK_NOT_READY;

	if ( (ret = TopicReader_GetLastReceiveResult(pstReader)) != RET_SUCCESS )
		return ret;

	if ( (ret = TopicReader_ReadLatest1Elem(pstReader, pstSample)) != 1 )
		return ret;

	if ( (ret = TopicReader_GetLastReceiveResult(pstReader)) != RET_SUCCESS )
		return ret;

	return 1;
}

AHEADFORM_TOPIC_READER* Status_RegisterFaultSetReader(AHEADFORM_STATUS *pstStatus, const AHEADFORM_TOPIC_READER_PARA *pstReaderPara)
{
	if (pstStatus == NULL || pstStatus->pstTopics == NULL)
	{
		PRINT_FUNCTION_ERRINFO(ERR_ARGV_1, __FUNCTION__);
		return NULL;
	}

	return AheadFormTopics_RegisterReader(pstStatus->pstTopics, AHEADFORM_TOPIC_ID_FAULT_SET, pstReaderPara);
}

AHEADFORM_TOPIC_READER* Status_RegisterFaultEventReader(AHEADFORM_STATUS *pstStatus, const AHEADFORM_TOPIC_READER_PARA *pstReaderPara)
{
	if (pstStatus == NULL || pstStatus->pstTopics == NULL)
	{
		PRINT_FUNCTION_ERRINFO(ERR_ARGV_1, __FUNCTION__);
		return NULL;
	}

	return AheadFormTopics_RegisterReader(pstStatus->pstTopics, AHEADFORM_TOPIC_ID_FAULT_EVENT, pstReaderPara);
}

int Status_ReadLatestFaultEvent(AHEADFORM_STATUS *pstStatus, AHEADFORM_FAULT_EVENT_SAMPLE *pstSample)
{
	AHEADFORM_TOPIC_READER *pstReader = NULL;
	int ret;

	if (pstStatus == NULL)
		return ERR_ARGV_1;

	if (pstSample == NULL)
		return ERR_ARGV_2;

	pstReader = pstStatus->szpstReader[AHEADFORM_TOPIC_ID_FAULT_EVENT - AHEADFORM_TOPIC_ID_HARDWARE_STATE];
	if (pstReader == NULL)
		return ERR_SDK_NOT_READY;

	if ( (ret = TopicReader_GetLastReceiveResult(pstReader)) != RET_SUCCESS )
		return ret;

	if ( (ret = TopicReader_ReadLatest1Elem(pstReader, pstSample)) != 1 )
		return ret;

	if ( (ret = TopicReader_GetLastReceiveResult(pstReader)) != RET_SUCCESS )
		return ret;

	return 1;
}

int Status_UnregisterReader(AHEADFORM_STATUS *pstStatus, AHEADFORM_TOPIC_READER *pstReader)
{
	unsigned int i;

	if (pstStatus == NULL || pstStatus->pstTopics == NULL)
		return ERR_ARGV_1;

	if (pstReader == NULL || pstReader->topicId < AHEADFORM_TOPIC_ID_HARDWARE_STATE || pstReader->topicId > AHEADFORM_TOPIC_ID_FAULT_EVENT)
		return ERR_ARGV_2;

	for (i = 0; i < AHEADFORM_STATUS_TOPIC_CNT; i++)
	{
		if (pstReader == pstStatus->szpstReader[i])
			return ERR_ARGV_2;
	}

	return AheadFormTopics_UnregisterReader(pstStatus->pstTopics, pstReader);
}

int Status_ReadLatestFaultSet(AHEADFORM_STATUS *pstStatus, AHEADFORM_FAULT_SET_SAMPLE *pstSample, unsigned int sampleBufSize)
{
	if (pstStatus == NULL)
		return ERR_ARGV_1;

	return StatusReader_ReadLatestFaultSet(pstStatus->szpstReader[AHEADFORM_TOPIC_ID_FAULT_SET - AHEADFORM_TOPIC_ID_HARDWARE_STATE], pstSample, sampleBufSize);
}

int StatusReader_ReadLatestFaultSet(AHEADFORM_TOPIC_READER *pstReader, AHEADFORM_FAULT_SET_SAMPLE *pstSample, unsigned int sampleBufSize)
{
	int ret;

	if (pstReader == NULL || pstReader->topicId != AHEADFORM_TOPIC_ID_FAULT_SET)
		return ERR_ARGV_1;

	if (pstSample == NULL)
		return ERR_ARGV_2;

	if (sampleBufSize < TopicReader_GetElemSize(pstReader))
		return ERR_ARGV_3;

	if ( (ret = TopicReader_GetLastReceiveResult(pstReader)) != RET_SUCCESS )
		return ret;

	if ( (ret = TopicReader_ReadLatest1Elem(pstReader, pstSample)) != 1 )
		return ret;

	if ( (ret = TopicReader_GetLastReceiveResult(pstReader)) != RET_SUCCESS )
		return ret;

	return 1;
}

int StatusReader_PopFaultSet(AHEADFORM_TOPIC_READER *pstReader, AHEADFORM_FAULT_SET_SAMPLE *pstSample, unsigned int sampleBufSize)
{
	if (pstReader == NULL || pstReader->topicId != AHEADFORM_TOPIC_ID_FAULT_SET)
		return ERR_ARGV_1;

	if (pstSample == NULL)
		return ERR_ARGV_2;

	if (sampleBufSize < TopicReader_GetElemSize(pstReader))
		return ERR_ARGV_3;

	return TopicReader_PopElem(pstReader, pstSample);
}
