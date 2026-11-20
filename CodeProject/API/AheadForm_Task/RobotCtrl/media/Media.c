#include "Media.h"

int Media_Init(AHEADFORM_MEDIA_CTRL *pstMediaCtrl, AHEADFORM_TOPICS *pstTopics, const AHEADFORM_INIT_CONFIG *pstCfg)
{
	int ret;

	if (pstMediaCtrl == NULL)
		return ERR_ARGV_1;

	if (pstTopics == NULL)
		return ERR_ARGV_2;

	if (pstTopics->stDdsCtrl.ddsParticipant <= 0)
		return ERR_ARGV_2;

	if (pstCfg == NULL)
		return ERR_ARGV_3;

	memset(pstMediaCtrl, 0, sizeof(*pstMediaCtrl));
	if ( (ret = Audio_Init(&pstMediaCtrl->stAudioCtrl, pstTopics, pstCfg)) != RET_SUCCESS )
		return ret;

	if ( (ret = Video_Init(&pstMediaCtrl->stVideoCtrl, pstCfg)) != RET_SUCCESS )
		return ret;

	printf("%s succeed.\r\n", __FUNCTION__);
	return RET_SUCCESS;
}

int Media_Deinit(AHEADFORM_MEDIA_CTRL *pstMediaCtrl)
{
	int ret;

	if (pstMediaCtrl == NULL)
		return ERR_ARGV_1;

	if (pstMediaCtrl->stVideoCtrl.pstReaders != NULL || pstMediaCtrl->stAudioCtrl.stMicCapture.pstReaders != NULL)
		return ERRCODE_RESOURCE_BUSY;

	if ( (ret = Video_Deinit(&pstMediaCtrl->stVideoCtrl)) != RET_SUCCESS )
		return ret;

	if ( (ret = Audio_Deinit(&pstMediaCtrl->stAudioCtrl)) != RET_SUCCESS )
		return ret;

	memset(pstMediaCtrl, 0, sizeof(*pstMediaCtrl));
	return RET_SUCCESS;
}
