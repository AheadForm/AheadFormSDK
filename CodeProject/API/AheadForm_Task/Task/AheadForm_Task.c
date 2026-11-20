#include "AheadForm_Task.h"

int AheadForm_Init(AHEADFORM_ALL_CTRL *pstAheadForm, const char *iniConfigFilePath)
{
	int ret;

	if (pstAheadForm == NULL)
		return ERR_ARGV_1;

	if (iniConfigFilePath == NULL || iniConfigFilePath[0] == 0)
		return ERR_ARGV_2;

	memset(pstAheadForm, 0, sizeof(*pstAheadForm));
	if ( (ret = ParseAheadFormInitConfig(iniConfigFilePath, &pstAheadForm->stCfg)) != RET_SUCCESS )
		return ret;

	if ( (ret = AheadFormTopics_Init(&pstAheadForm->stTopics, &pstAheadForm->stCfg.stDds)) != RET_SUCCESS )
		return ret;

	if ( (ret = Head_Init(&pstAheadForm->stRobotCtrl.stHeadCtrl, &pstAheadForm->stTopics, &pstAheadForm->stCfg.stHead)) != RET_SUCCESS )
		return ret;

	if ( (ret = Media_Init(&pstAheadForm->stRobotCtrl.stMediaCtrl, &pstAheadForm->stTopics, &pstAheadForm->stCfg)) != RET_SUCCESS )
		return ret;

	if ( (ret = Status_Init(&pstAheadForm->stRobotCtrl.stStatus, &pstAheadForm->stTopics)) != RET_SUCCESS )
		return ret;

	if ( (ret = OperatingMode_Init(&pstAheadForm->stRobotCtrl.stOperatingModeCtrl, &pstAheadForm->stTopics)) != RET_SUCCESS )
		return ret;

	if ( (ret = EmotePlay_Init(&pstAheadForm->stRobotCtrl.stEmotePlayCtrl, &pstAheadForm->stTopics)) != RET_SUCCESS )
		return ret;

	return RET_SUCCESS;
}

int AheadForm_Deinit(AHEADFORM_ALL_CTRL *pstAheadForm)
{
	int ret;

	if (pstAheadForm == NULL)
		return ERR_ARGV_1;

	if ( (ret = EmotePlay_Deinit(&pstAheadForm->stRobotCtrl.stEmotePlayCtrl)) != RET_SUCCESS )
		return ret;

	if ( (ret = OperatingMode_Deinit(&pstAheadForm->stRobotCtrl.stOperatingModeCtrl)) != RET_SUCCESS )
		return ret;

	if ( (ret = Status_Deinit(&pstAheadForm->stRobotCtrl.stStatus)) != RET_SUCCESS )
		return ret;

	if ( (ret = Media_Deinit(&pstAheadForm->stRobotCtrl.stMediaCtrl)) != RET_SUCCESS )
		return ret;

	if ( (ret = Head_Deinit(&pstAheadForm->stRobotCtrl.stHeadCtrl)) != RET_SUCCESS )
		return ret;

	return AheadFormTopics_Deinit(&pstAheadForm->stTopics);
}

AHEADFORM_TOPIC_READER* AheadForm_RegisterTopicReader(AHEADFORM_ALL_CTRL *pstAheadForm, ENUM_AHEADFORM_TOPIC_ID topicId, const AHEADFORM_TOPIC_READER_PARA *pstReaderPara)
{
	if (pstAheadForm == NULL)
	{
		PRINT_FUNCTION_ERRINFO(ERR_ARGV_1, __FUNCTION__);
		return NULL;
	}

	return AheadFormTopics_RegisterReader(&pstAheadForm->stTopics, topicId, pstReaderPara);
}

int AheadForm_UnregisterTopicReader(AHEADFORM_ALL_CTRL *pstAheadForm, AHEADFORM_TOPIC_READER *pstReader)
{
	if (pstAheadForm == NULL)
		return ERR_ARGV_1;

	return AheadFormTopics_UnregisterReader(&pstAheadForm->stTopics, pstReader);
}

AHEADFORM_TOPIC_WRITER* AheadForm_RegisterTopicWriter(AHEADFORM_ALL_CTRL *pstAheadForm, ENUM_AHEADFORM_TOPIC_ID topicId)
{
	if (pstAheadForm == NULL)
	{
		PRINT_FUNCTION_ERRINFO(ERR_ARGV_1, __FUNCTION__);
		return NULL;
	}

	return AheadFormTopics_RegisterWriter(&pstAheadForm->stTopics, topicId);
}

int AheadForm_UnregisterTopicWriter(AHEADFORM_ALL_CTRL *pstAheadForm, AHEADFORM_TOPIC_WRITER *pstWriter)
{
	if (pstAheadForm == NULL)
		return ERR_ARGV_1;

	return AheadFormTopics_UnregisterWriter(&pstAheadForm->stTopics, pstWriter);
}
