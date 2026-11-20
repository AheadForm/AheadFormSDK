#include "Status.h"

int Status_Init(AHEADFORM_STATUS *pstStatus, AHEADFORM_TOPICS *pstTopics)
{
	AHEADFORM_TOPIC_READER_PARA stReaderPara = {0};

	if (pstStatus == NULL)
		return ERR_ARGV_1;

	if (pstTopics == NULL)
		return ERR_ARGV_2;

	if (pstTopics->stDdsCtrl.ddsParticipant <= 0)
	{
		printf("%s() received invalid pstTopics; call AheadFormTopics_Init() successfully before use.\r\n", __FUNCTION__);
		return ERR_ARGV_2;
	}
	memset(pstStatus, 0, sizeof(*pstStatus));
	pstStatus->pstTopics = pstTopics;
	if ( (pstStatus->pstSensorStateReader = AheadFormTopics_RegisterReader(pstTopics, AHEADFORM_TOPIC_ID_SENSOR_STATE, &stReaderPara)) == NULL )
		return ERRCODE_INIT_FAIL;

	if ( (pstStatus->pstNeckStateReader = AheadFormTopics_RegisterReader(pstTopics, AHEADFORM_TOPIC_ID_NECK_STATE, &stReaderPara)) == NULL )
		return ERRCODE_INIT_FAIL;

	if ( (pstStatus->pstFaultPoolReader = AheadFormTopics_RegisterReader(pstTopics, AHEADFORM_TOPIC_ID_FAULT_POOL, &stReaderPara)) == NULL )
		return ERRCODE_INIT_FAIL;

	return RET_SUCCESS;
}

int Status_Deinit(AHEADFORM_STATUS *pstStatus)
{
	int ret;

	if (pstStatus == NULL)
		return ERR_ARGV_1;

	if (pstStatus->pstFaultPoolReader != NULL)
	{
		if ( (ret = AheadFormTopics_UnregisterReader(pstStatus->pstTopics, pstStatus->pstFaultPoolReader)) != RET_SUCCESS )
			return ret;

		pstStatus->pstFaultPoolReader = NULL;
	}

	if (pstStatus->pstNeckStateReader != NULL)
	{
		if ( (ret = AheadFormTopics_UnregisterReader(pstStatus->pstTopics, pstStatus->pstNeckStateReader)) != RET_SUCCESS )
			return ret;

		pstStatus->pstNeckStateReader = NULL;
	}

	if (pstStatus->pstSensorStateReader != NULL)
	{
		if ( (ret = AheadFormTopics_UnregisterReader(pstStatus->pstTopics, pstStatus->pstSensorStateReader)) != RET_SUCCESS )
			return ret;

		pstStatus->pstSensorStateReader = NULL;
	}
	memset(pstStatus, 0, sizeof(*pstStatus));
	return RET_SUCCESS;
}

int Status_ReadLatestSensorState(AHEADFORM_STATUS *pstStatus, AHEADFORM_SENSOR_STATE *pstSensorState)
{
	if (pstStatus == NULL)
		return ERR_ARGV_1;

	if (pstSensorState == NULL)
		return ERR_ARGV_2;

	if (pstStatus->pstSensorStateReader == NULL)
		return ERR_SDK_NOT_READY;

	return TopicReader_ReadLatest1Elem(pstStatus->pstSensorStateReader, pstSensorState);
}

int Status_ReadSensorRecentState(AHEADFORM_STATUS *pstStatus, AHEADFORM_SENSOR_STATE *pstSensorStateAry, unsigned int rdStateCnt)
{
	if (pstStatus == NULL)
		return ERR_ARGV_1;

	if (pstSensorStateAry == NULL)
		return ERR_ARGV_2;

	if (pstStatus->pstSensorStateReader == NULL)
		return ERR_SDK_NOT_READY;

	return TopicReader_ReadRecentElems(pstStatus->pstSensorStateReader, pstSensorStateAry, rdStateCnt);
}

int Status_ReadLatestNeckState(AHEADFORM_STATUS *pstStatus, AHEADFORM_NECK_STATE *pstNeckState)
{
	if (pstStatus == NULL)
		return ERR_ARGV_1;

	if (pstNeckState == NULL)
		return ERR_ARGV_2;

	if (pstStatus->pstNeckStateReader == NULL)
		return ERR_SDK_NOT_READY;

	return TopicReader_ReadLatest1Elem(pstStatus->pstNeckStateReader, pstNeckState);
}

int Status_ReadNeckRecentState(AHEADFORM_STATUS *pstStatus, AHEADFORM_NECK_STATE *pstNeckStateAry, unsigned int rdStateCnt)
{
	if (pstStatus == NULL)
		return ERR_ARGV_1;

	if (pstNeckStateAry == NULL)
		return ERR_ARGV_2;

	if (pstStatus->pstNeckStateReader == NULL)
		return ERR_SDK_NOT_READY;

	return TopicReader_ReadRecentElems(pstStatus->pstNeckStateReader, pstNeckStateAry, rdStateCnt);
}

int Status_ReadLatestFaultPool(AHEADFORM_STATUS *pstStatus, AHEADFORM_FAULT_POOL *pstFaultPool)
{
	if (pstStatus == NULL)
		return ERR_ARGV_1;

	if (pstFaultPool == NULL)
		return ERR_ARGV_2;

	if (pstStatus->pstFaultPoolReader == NULL)
		return ERR_SDK_NOT_READY;

	return TopicReader_ReadLatest1Elem(pstStatus->pstFaultPoolReader, pstFaultPool);
}

int Status_ReadRecentFaultPools(AHEADFORM_STATUS *pstStatus, AHEADFORM_FAULT_POOL *pstFaultPoolAry, unsigned int rdPoolCnt)
{
	if (pstStatus == NULL)
		return ERR_ARGV_1;

	if (pstFaultPoolAry == NULL)
		return ERR_ARGV_2;

	if (pstStatus->pstFaultPoolReader == NULL)
		return ERR_SDK_NOT_READY;

	return TopicReader_ReadRecentElems(pstStatus->pstFaultPoolReader, pstFaultPoolAry, rdPoolCnt);
}

AHEADFORM_TOPIC_READER* Status_RegisterReader(AHEADFORM_STATUS *pstStatus, ENUM_AHEADFORM_TOPIC_ID topicId, const AHEADFORM_TOPIC_READER_PARA *pstReaderPara)
{
	if (pstStatus == NULL)
	{
		PRINT_FUNCTION_ERRINFO(ERR_ARGV_1, __FUNCTION__);
		return NULL;
	}

	if (topicId != AHEADFORM_TOPIC_ID_SENSOR_STATE && topicId != AHEADFORM_TOPIC_ID_NECK_STATE && topicId != AHEADFORM_TOPIC_ID_FAULT_POOL)
	{
		PRINT_FUNCTION_ERRINFO(ERR_ARGV_2, __FUNCTION__);
		return NULL;
	}

	if (pstReaderPara == NULL)
	{
		PRINT_FUNCTION_ERRINFO(ERR_ARGV_3, __FUNCTION__);
		return NULL;
	}

	if (pstStatus->pstTopics == NULL)
	{
		PRINT_FUNCTION_ERRINFO(ERR_SDK_NOT_READY, __FUNCTION__);
		return NULL;
	}

	return AheadFormTopics_RegisterReader(pstStatus->pstTopics, topicId, pstReaderPara);
}

int Status_UnregisterReader(AHEADFORM_STATUS *pstStatus, AHEADFORM_TOPIC_READER *pstReader)
{
	if (pstStatus == NULL)
		return ERR_ARGV_1;

	if (pstReader == NULL)
		return ERR_ARGV_2;

	if (pstStatus->pstTopics == NULL)
		return ERR_SDK_NOT_READY;

	if (pstReader == pstStatus->pstSensorStateReader || pstReader == pstStatus->pstNeckStateReader || pstReader == pstStatus->pstFaultPoolReader)
		return ERR_RESOURCE_BUSY;

	if (pstReader->topicId != AHEADFORM_TOPIC_ID_SENSOR_STATE && pstReader->topicId != AHEADFORM_TOPIC_ID_NECK_STATE && pstReader->topicId != AHEADFORM_TOPIC_ID_FAULT_POOL)
		return ERR_ARGV_2;

	return AheadFormTopics_UnregisterReader(pstStatus->pstTopics, pstReader);
}
