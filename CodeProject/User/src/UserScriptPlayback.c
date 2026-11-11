#include "UserCmdMenu.h"

int UserCmd_PlayScript(AHEADFORM_ALL_CTRL *pstAheadForm, int argc, char *argv[])
{
	if (argc != 2)
		return ERR_ARGC;

	return AheadForm_PlayScript(pstAheadForm, argv[1]);
}

int UserCmd_StopScript(AHEADFORM_ALL_CTRL *pstAheadForm, int argc, char *argv[])
{
	if (argc != 1)
		return ERR_ARGC;

	return AheadForm_StopScript(pstAheadForm);
}

int UserCmd_ScriptStatus(AHEADFORM_ALL_CTRL *pstAheadForm, int argc, char *argv[])
{
	AHEADFORM_PLAYBACK_INFO stInfo;
	int ret;

	if (argc != 1)
		return ERR_ARGC;

	if ( (ret = AheadForm_GetScriptPlaybackInfo(pstAheadForm, &stInfo)) != RET_SUCCESS )
		return ret;

	printf("script state=%d result=0x%08X elapsed=%.3fs duration=%.3fs motion=%u/%u PCM=%u/%u first-motion-delay=%llu us\r\n",
		stInfo.state, stInfo.result, stInfo.elapsed_us / 1000000.0, stInfo.duration_us / 1000000.0,
		stInfo.sentMotionFrameCnt, stInfo.motionFrameCnt, stInfo.sentPcmFrameCnt, stInfo.pcmFrameCnt,
		stInfo.firstMotionTime_us > stInfo.firstPcmTime_us ? stInfo.firstMotionTime_us - stInfo.firstPcmTime_us : 0);
	return RET_SUCCESS;
}
