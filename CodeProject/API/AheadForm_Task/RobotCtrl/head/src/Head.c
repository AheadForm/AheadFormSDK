#include "Head.h"

int Head_Init(AHEADFORM_HEAD_CTRL *pstHeadCtrl, AHEADFORM_TOPICS *pstTopics, const AHEADFORM_HEAD_CFG *pstHeadCfg)
{
	AHEADFORM_TOPIC_READER_PARA stReaderPara = {0};

	if (pstHeadCtrl == NULL)
		return ERR_ARGV_1;

	if (pstTopics == NULL || pstTopics->stDdsCtrl.ddsParticipant <= 0)
		return ERR_ARGV_2;

	if (pstHeadCfg == NULL || pstHeadCfg->sourceName[0] < 'a' || pstHeadCfg->sourceName[0] > 'z' || strnlen(pstHeadCfg->sourceName, sizeof(pstHeadCfg->sourceName)) == sizeof(pstHeadCfg->sourceName))
		return ERR_ARGV_3;

	if (strspn(pstHeadCfg->sourceName, "abcdefghijklmnopqrstuvwxyz0123456789_") != strlen(pstHeadCfg->sourceName))
		return ERR_ARGV_3;

	memset(pstHeadCtrl, 0, sizeof(*pstHeadCtrl));
	pstHeadCtrl->pstTopics = pstTopics;
	pstHeadCtrl->stCfg = *pstHeadCfg;
	if ( (pstHeadCtrl->pstMotionWriter = AheadFormTopics_RegisterWriter(pstTopics, AHEADFORM_TOPIC_ID_MOTION)) == NULL )
		return ERRCODE_INIT_FAIL;

	if ( (pstHeadCtrl->pstFaceWriter = AheadFormTopics_RegisterWriter(pstTopics, AHEADFORM_TOPIC_ID_FACE)) == NULL )
		return ERRCODE_INIT_FAIL;

	if ( (pstHeadCtrl->pstNeckWriter = AheadFormTopics_RegisterWriter(pstTopics, AHEADFORM_TOPIC_ID_NECK)) == NULL )
		return ERRCODE_INIT_FAIL;

	if ( (pstHeadCtrl->pstServoWriter = AheadFormTopics_RegisterWriter(pstTopics, AHEADFORM_TOPIC_ID_SERVO)) == NULL )
		return ERRCODE_INIT_FAIL;

	if ( (pstHeadCtrl->pstPresetAskWriter = AheadFormTopics_RegisterWriter(pstTopics, AHEADFORM_TOPIC_ID_PRESET_MOTION_ASK)) == NULL )
		return ERRCODE_INIT_FAIL;

	if ( (pstHeadCtrl->pstPresetReplyReader = AheadFormTopics_RegisterReader(pstTopics, AHEADFORM_TOPIC_ID_PRESET_MOTION_REPLY, &stReaderPara)) == NULL )
		return ERRCODE_INIT_FAIL;

	return RET_SUCCESS;
}

int Head_Deinit(AHEADFORM_HEAD_CTRL *pstHeadCtrl)
{
	int ret;

	if (pstHeadCtrl == NULL)
		return ERR_ARGV_1;

	if (pstHeadCtrl->pstPresetReplyReader != NULL)
	{
		if ( (ret = AheadFormTopics_UnregisterReader(pstHeadCtrl->pstTopics, pstHeadCtrl->pstPresetReplyReader)) != RET_SUCCESS )
			return ret;

		pstHeadCtrl->pstPresetReplyReader = NULL;
	}

	if (pstHeadCtrl->pstPresetAskWriter != NULL)
	{
		if ( (ret = AheadFormTopics_UnregisterWriter(pstHeadCtrl->pstTopics, pstHeadCtrl->pstPresetAskWriter)) != RET_SUCCESS )
			return ret;

		pstHeadCtrl->pstPresetAskWriter = NULL;
	}

	if (pstHeadCtrl->pstServoWriter != NULL)
	{
		if ( (ret = AheadFormTopics_UnregisterWriter(pstHeadCtrl->pstTopics, pstHeadCtrl->pstServoWriter)) != RET_SUCCESS )
			return ret;

		pstHeadCtrl->pstServoWriter = NULL;
	}

	if (pstHeadCtrl->pstNeckWriter != NULL)
	{
		if ( (ret = AheadFormTopics_UnregisterWriter(pstHeadCtrl->pstTopics, pstHeadCtrl->pstNeckWriter)) != RET_SUCCESS )
			return ret;

		pstHeadCtrl->pstNeckWriter = NULL;
	}

	if (pstHeadCtrl->pstFaceWriter != NULL)
	{
		if ( (ret = AheadFormTopics_UnregisterWriter(pstHeadCtrl->pstTopics, pstHeadCtrl->pstFaceWriter)) != RET_SUCCESS )
			return ret;

		pstHeadCtrl->pstFaceWriter = NULL;
	}

	if (pstHeadCtrl->pstMotionWriter != NULL)
	{
		if ( (ret = AheadFormTopics_UnregisterWriter(pstHeadCtrl->pstTopics, pstHeadCtrl->pstMotionWriter)) != RET_SUCCESS )
			return ret;

		pstHeadCtrl->pstMotionWriter = NULL;
	}
	memset(pstHeadCtrl, 0, sizeof(*pstHeadCtrl));
	return RET_SUCCESS;
}

int Head_SendMotion(AHEADFORM_HEAD_CTRL *pstHeadCtrl, const AHEADFORM_MOTION_FRAME *pstFrame)
{
	AHEADFORM_MOTION_COMMAND stCommand;

	if (pstHeadCtrl == NULL)
		return ERR_ARGV_1;

	if (pstFrame == NULL)
		return ERR_ARGV_2;

	if (pstHeadCtrl->pstMotionWriter == NULL)
		return ERR_SDK_NOT_READY;

	stCommand.stFrame = *pstFrame;
	strcpy(stCommand.sourceName, pstHeadCtrl->stCfg.sourceName);
	return TopicWriter_Write(pstHeadCtrl->pstMotionWriter, &stCommand);
}

int Head_SendFace(AHEADFORM_HEAD_CTRL *pstHeadCtrl, const AHEADFORM_EYE_POSE *pstEyes, const float *pBlendshapes)
{
	AHEADFORM_FACE_COMMAND stCommand;

	if (pstHeadCtrl == NULL)
		return ERR_ARGV_1;

	if (pstEyes == NULL)
		return ERR_ARGV_2;

	if (pBlendshapes == NULL)
		return ERR_ARGV_3;

	if (pstHeadCtrl->pstFaceWriter == NULL)
		return ERR_SDK_NOT_READY;

	strcpy(stCommand.sourceName, pstHeadCtrl->stCfg.sourceName);
	stCommand.commandSeq = __atomic_add_fetch(&pstHeadCtrl->nextCommandSeq, 1, __ATOMIC_RELAXED);
	stCommand.stEyes = *pstEyes;
	memcpy(stCommand.blendshapes, pBlendshapes, sizeof(stCommand.blendshapes));
	return TopicWriter_Write(pstHeadCtrl->pstFaceWriter, &stCommand);
}

int Head_SendNeck(AHEADFORM_HEAD_CTRL *pstHeadCtrl, const AHEADFORM_NECK_POSE *pstNeck)
{
	AHEADFORM_NECK_COMMAND stCommand;

	if (pstHeadCtrl == NULL)
		return ERR_ARGV_1;

	if (pstNeck == NULL)
		return ERR_ARGV_2;

	if (pstHeadCtrl->pstNeckWriter == NULL)
		return ERR_SDK_NOT_READY;

	strcpy(stCommand.sourceName, pstHeadCtrl->stCfg.sourceName);
	stCommand.commandSeq = __atomic_add_fetch(&pstHeadCtrl->nextCommandSeq, 1, __ATOMIC_RELAXED);
	stCommand.stNeck = *pstNeck;
	return TopicWriter_Write(pstHeadCtrl->pstNeckWriter, &stCommand);
}

int Head_SendServoTargets(AHEADFORM_HEAD_CTRL *pstHeadCtrl, const double *pTargetLogic_deg)
{
	AHEADFORM_SERVO_COMMAND stCommand;

	if (pstHeadCtrl == NULL)
		return ERR_ARGV_1;

	if (pTargetLogic_deg == NULL)
		return ERR_ARGV_2;

	if (pstHeadCtrl->pstServoWriter == NULL)
		return ERR_SDK_NOT_READY;

	stCommand.commandSeq = __atomic_add_fetch(&pstHeadCtrl->nextCommandSeq, 1, __ATOMIC_RELAXED);
	memcpy(stCommand.targetLogic_deg, pTargetLogic_deg, sizeof(stCommand.targetLogic_deg));
	return TopicWriter_Write(pstHeadCtrl->pstServoWriter, &stCommand);
}
