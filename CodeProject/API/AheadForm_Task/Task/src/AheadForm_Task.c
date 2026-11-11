#include "AheadForm_Task.h"

int AheadForm_Init(AHEADFORM_ALL_CTRL *pstAheadForm, const char *iniConfigFilePath)
{
	AHEADFORM_TOPIC_READER_PARA stReaderPara = {0};
	int ret;

	if (pstAheadForm == NULL)
		return ERR_ARGV_1;

	if (iniConfigFilePath == NULL || iniConfigFilePath[0] == 0)
		return ERR_ARGV_2;

	memset(pstAheadForm, 0, sizeof(*pstAheadForm));
	if ( (ret = ParseAheadFormInitConfig(iniConfigFilePath, &pstAheadForm->stCfg)) != RET_SUCCESS )
		return ret;

	if ( (ret = AheadFormTopics_Init(&pstAheadForm->stTopics, &pstAheadForm->stCfg.stDds, &pstAheadForm->stCfg.stStatus)) != RET_SUCCESS )
		return ret;

	if ( (ret = Head_Init(&pstAheadForm->stRobotCtrl.stHeadCtrl, &pstAheadForm->stTopics, &pstAheadForm->stCfg.stHead)) != RET_SUCCESS )
		return ret;

	if ( (ret = Media_Init(&pstAheadForm->stRobotCtrl.stMediaCtrl, &pstAheadForm->stTopics, &pstAheadForm->stCfg.stMedia)) != RET_SUCCESS )
		return ret;

	if ( (ret = Status_Init(&pstAheadForm->stRobotCtrl.stStatus, &pstAheadForm->stTopics)) != RET_SUCCESS )
		return ret;

	if ( (pstAheadForm->pstHardwareControlAskWriter = AheadFormTopics_RegisterWriter(&pstAheadForm->stTopics, AHEADFORM_TOPIC_ID_HARDWARE_CONTROL_ASK)) == NULL )
		return ERRCODE_INIT_FAIL;

	if ( (pstAheadForm->pstHardwareControlReplyReader = AheadFormTopics_RegisterReader(&pstAheadForm->stTopics, AHEADFORM_TOPIC_ID_HARDWARE_CONTROL_REPLY, &stReaderPara)) == NULL )
		return ERRCODE_INIT_FAIL;

	if ( (pstAheadForm->pstSafetyControlAskWriter = AheadFormTopics_RegisterWriter(&pstAheadForm->stTopics, AHEADFORM_TOPIC_ID_SAFETY_CONTROL_ASK)) == NULL )
		return ERRCODE_INIT_FAIL;

	if ( (pstAheadForm->pstSafetyControlReplyReader = AheadFormTopics_RegisterReader(&pstAheadForm->stTopics, AHEADFORM_TOPIC_ID_SAFETY_CONTROL_REPLY, &stReaderPara)) == NULL )
		return ERRCODE_INIT_FAIL;

	if ( (pstAheadForm->pstOperatingModeAskWriter = AheadFormTopics_RegisterWriter(&pstAheadForm->stTopics, AHEADFORM_TOPIC_ID_OPERATING_MODE_ASK)) == NULL )
		return ERRCODE_INIT_FAIL;

	if ( (pstAheadForm->pstOperatingModeReplyReader = AheadFormTopics_RegisterReader(&pstAheadForm->stTopics, AHEADFORM_TOPIC_ID_OPERATING_MODE_REPLY, &stReaderPara)) == NULL )
		return ERRCODE_INIT_FAIL;

	if ( (ret = Playback_Init(&pstAheadForm->stPlayback, &pstAheadForm->stRobotCtrl.stHeadCtrl, &pstAheadForm->stRobotCtrl.stMediaCtrl, &pstAheadForm->stRobotCtrl.stStatus)) != RET_SUCCESS )
		return ret;

	if ( (ret = Record_Init(&pstAheadForm->stRecord)) != RET_SUCCESS )
		return ret;

	return RET_SUCCESS;
}

int AheadForm_Deinit(AHEADFORM_ALL_CTRL *pstAheadForm)
{
	int ret;

	if (pstAheadForm == NULL)
		return ERR_ARGV_1;

	if ( (ret = Record_Deinit(&pstAheadForm->stRecord)) != RET_SUCCESS )
		return ret;

	if ( (ret = Playback_Deinit(&pstAheadForm->stPlayback)) != RET_SUCCESS )
		return ret;

	if (pstAheadForm->pstOperatingModeReplyReader != NULL)
	{
		if ( (ret = AheadFormTopics_UnregisterReader(&pstAheadForm->stTopics, pstAheadForm->pstOperatingModeReplyReader)) != RET_SUCCESS )
			return ret;

		pstAheadForm->pstOperatingModeReplyReader = NULL;
	}

	if (pstAheadForm->pstOperatingModeAskWriter != NULL)
	{
		if ( (ret = AheadFormTopics_UnregisterWriter(&pstAheadForm->stTopics, pstAheadForm->pstOperatingModeAskWriter)) != RET_SUCCESS )
			return ret;

		pstAheadForm->pstOperatingModeAskWriter = NULL;
	}

	if (pstAheadForm->pstSafetyControlReplyReader != NULL)
	{
		if ( (ret = AheadFormTopics_UnregisterReader(&pstAheadForm->stTopics, pstAheadForm->pstSafetyControlReplyReader)) != RET_SUCCESS )
			return ret;

		pstAheadForm->pstSafetyControlReplyReader = NULL;
	}

	if (pstAheadForm->pstSafetyControlAskWriter != NULL)
	{
		if ( (ret = AheadFormTopics_UnregisterWriter(&pstAheadForm->stTopics, pstAheadForm->pstSafetyControlAskWriter)) != RET_SUCCESS )
			return ret;

		pstAheadForm->pstSafetyControlAskWriter = NULL;
	}

	if (pstAheadForm->pstHardwareControlReplyReader != NULL)
	{
		if ( (ret = AheadFormTopics_UnregisterReader(&pstAheadForm->stTopics, pstAheadForm->pstHardwareControlReplyReader)) != RET_SUCCESS )
			return ret;

		pstAheadForm->pstHardwareControlReplyReader = NULL;
	}

	if (pstAheadForm->pstHardwareControlAskWriter != NULL)
	{
		if ( (ret = AheadFormTopics_UnregisterWriter(&pstAheadForm->stTopics, pstAheadForm->pstHardwareControlAskWriter)) != RET_SUCCESS )
			return ret;

		pstAheadForm->pstHardwareControlAskWriter = NULL;
	}

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

int AheadForm_PlayScript(AHEADFORM_ALL_CTRL *pstAheadForm, const char *csvFilePath)
{
	if (pstAheadForm == NULL)
		return ERR_ARGV_1;

	return Playback_PlayScript(&pstAheadForm->stPlayback, csvFilePath);
}

int AheadForm_StopScript(AHEADFORM_ALL_CTRL *pstAheadForm)
{
	if (pstAheadForm == NULL)
		return ERR_ARGV_1;

	return Playback_StopScript(&pstAheadForm->stPlayback);
}

int AheadForm_GetScriptPlaybackInfo(AHEADFORM_ALL_CTRL *pstAheadForm, AHEADFORM_PLAYBACK_INFO *pstInfo)
{
	if (pstAheadForm == NULL)
		return ERR_ARGV_1;

	return Playback_GetInfo(&pstAheadForm->stPlayback, pstInfo);
}
