#ifndef __AHEADFORM_TASK_H__
#define __AHEADFORM_TASK_H__

#include "ParseInitConfig.h"
#include "Head.h"
#include "Media.h"
#include "Status.h"
#include "Playback.h"
#include "Record.h"
#include "AheadFormTopics.h"

typedef struct
{
	unsigned char action;
	unsigned char device;
} AHEADFORM_HARDWARE_CONTROL_PARA;

typedef struct
{
	unsigned char action;
	unsigned char faceMotionPermitted;
	unsigned char neckMotionPermitted;
} AHEADFORM_SAFETY_CONTROL_PARA;

typedef struct
{
} AHEADFORM_THREAD_CTRL;

typedef struct
{
	AHEADFORM_HEAD_CTRL stHeadCtrl;
	AHEADFORM_MEDIA_CTRL stMediaCtrl;
	AHEADFORM_STATUS stStatus;
} AHEADFORM_ROBOT_CTRL;

typedef struct
{
	AHEADFORM_INIT_CONFIG stCfg;
	AHEADFORM_THREAD_CTRL stThreadCtrl;
	AHEADFORM_TOPICS stTopics;
	AHEADFORM_PLAYBACK stPlayback;
	AHEADFORM_RECORD stRecord;
	AHEADFORM_ROBOT_CTRL stRobotCtrl;
	AHEADFORM_TOPIC_WRITER *pstHardwareControlAskWriter;
	AHEADFORM_TOPIC_READER *pstHardwareControlReplyReader;
	AHEADFORM_TOPIC_WRITER *pstSafetyControlAskWriter;
	AHEADFORM_TOPIC_READER *pstSafetyControlReplyReader;
	AHEADFORM_TOPIC_WRITER *pstOperatingModeAskWriter;
	AHEADFORM_TOPIC_READER *pstOperatingModeReplyReader;

} AHEADFORM_ALL_CTRL, *AHEADFORM_CTRL_HANDLE;

int AheadForm_Init(AHEADFORM_ALL_CTRL *pstAheadForm, const char *iniConfigFilePath);//Pass DEFAULT_INI_FILE_PATH when the INI file is beside the executable and the working directory is that directory.
int AheadForm_Deinit(AHEADFORM_ALL_CTRL *pstAheadForm);
AHEADFORM_TOPIC_READER* AheadForm_RegisterTopicReader(AHEADFORM_ALL_CTRL *pstAheadForm, ENUM_AHEADFORM_TOPIC_ID topicId, const AHEADFORM_TOPIC_READER_PARA *pstReaderPara);
int AheadForm_UnregisterTopicReader(AHEADFORM_ALL_CTRL *pstAheadForm, AHEADFORM_TOPIC_READER *pstReader);
AHEADFORM_TOPIC_WRITER* AheadForm_RegisterTopicWriter(AHEADFORM_ALL_CTRL *pstAheadForm, ENUM_AHEADFORM_TOPIC_ID topicId);
int AheadForm_UnregisterTopicWriter(AHEADFORM_ALL_CTRL *pstAheadForm, AHEADFORM_TOPIC_WRITER *pstWriter);

/*
	Build assigns identity; Send reads only during the call. Async builds, copies matching values and publishes.
	Completion storage/eventfd follow TopicReader_RegisterAsyncRequest; inspect the remote reply after Check returns 1.
	MatchReplyWithReq reads a complete request object. Register a customer Reader before publishing for manual matching.
*/
int AheadForm_BuildHardwareControlRequest(AHEADFORM_ALL_CTRL *pstAheadForm, const AHEADFORM_HARDWARE_CONTROL_PARA *pstPara, AHEADFORM_HARDWARE_CONTROL_ASK *pstRequest);
int AheadForm_SendHardwareControlRequest(AHEADFORM_ALL_CTRL *pstAheadForm, const AHEADFORM_HARDWARE_CONTROL_ASK *pstRequest);
int AheadForm_HardwareControlAsync(AHEADFORM_ALL_CTRL *pstAheadForm, const AHEADFORM_HARDWARE_CONTROL_PARA *pstPara, unsigned long long MatchReplyTimeout_us, int notifyEventfd, AHEADFORM_HARDWARE_CONTROL_REPLY_SAMPLE *pstReply);
int AheadForm_CheckIsHardwareControlAsyncRequestReplied(AHEADFORM_ALL_CTRL *pstAheadForm, int notifyEventfd);
int AheadForm_CancelHardwareControlAsyncRequest(AHEADFORM_ALL_CTRL *pstAheadForm, int notifyEventfd);
AHEADFORM_TOPIC_READER* AheadForm_RegisterHardwareControlReader(AHEADFORM_ALL_CTRL *pstAheadForm, const AHEADFORM_TOPIC_READER_PARA *pstReaderPara);
int AheadForm_UnregisterHardwareControlReader(AHEADFORM_ALL_CTRL *pstAheadForm, AHEADFORM_TOPIC_READER *pstReader);
int AheadForm_MatchHardwareControlReplyWithReq(const void *pReplyElem, const void *pRequest);

int AheadForm_BuildSafetyControlRequest(AHEADFORM_ALL_CTRL *pstAheadForm, const AHEADFORM_SAFETY_CONTROL_PARA *pstPara, AHEADFORM_SAFETY_CONTROL_ASK *pstRequest);
int AheadForm_SendSafetyControlRequest(AHEADFORM_ALL_CTRL *pstAheadForm, const AHEADFORM_SAFETY_CONTROL_ASK *pstRequest);
int AheadForm_SafetyControlAsync(AHEADFORM_ALL_CTRL *pstAheadForm, const AHEADFORM_SAFETY_CONTROL_PARA *pstPara, unsigned long long MatchReplyTimeout_us, int notifyEventfd, AHEADFORM_SAFETY_CONTROL_REPLY_SAMPLE *pstReply);
int AheadForm_CheckIsSafetyControlAsyncRequestReplied(AHEADFORM_ALL_CTRL *pstAheadForm, int notifyEventfd);
int AheadForm_CancelSafetyControlAsyncRequest(AHEADFORM_ALL_CTRL *pstAheadForm, int notifyEventfd);
AHEADFORM_TOPIC_READER* AheadForm_RegisterSafetyControlReader(AHEADFORM_ALL_CTRL *pstAheadForm, const AHEADFORM_TOPIC_READER_PARA *pstReaderPara);
int AheadForm_UnregisterSafetyControlReader(AHEADFORM_ALL_CTRL *pstAheadForm, AHEADFORM_TOPIC_READER *pstReader);
int AheadForm_MatchSafetyControlReplyWithReq(const void *pReplyElem, const void *pRequest);

int AheadForm_BuildOperatingModeRequest(AHEADFORM_ALL_CTRL *pstAheadForm, unsigned int mode, AHEADFORM_OPERATING_MODE_ASK *pstRequest);
int AheadForm_SendOperatingModeRequest(AHEADFORM_ALL_CTRL *pstAheadForm, const AHEADFORM_OPERATING_MODE_ASK *pstRequest);
int AheadForm_OperatingModeAsync(AHEADFORM_ALL_CTRL *pstAheadForm, unsigned int mode, unsigned long long MatchReplyTimeout_us, int notifyEventfd, AHEADFORM_OPERATING_MODE_REPLY_SAMPLE *pstReply);
int AheadForm_CheckIsOperatingModeAsyncRequestReplied(AHEADFORM_ALL_CTRL *pstAheadForm, int notifyEventfd);
int AheadForm_CancelOperatingModeAsyncRequest(AHEADFORM_ALL_CTRL *pstAheadForm, int notifyEventfd);
AHEADFORM_TOPIC_READER* AheadForm_RegisterOperatingModeReader(AHEADFORM_ALL_CTRL *pstAheadForm, const AHEADFORM_TOPIC_READER_PARA *pstReaderPara);
int AheadForm_UnregisterOperatingModeReader(AHEADFORM_ALL_CTRL *pstAheadForm, AHEADFORM_TOPIC_READER *pstReader);
int AheadForm_MatchOperatingModeReplyWithReq(const void *pReplyElem, const void *pRequest);

//Asynchronous local CSV + same-basename MP3; see Playback.h for ownership and completion semantics.
int AheadForm_PlayScript(AHEADFORM_ALL_CTRL *pstAheadForm, const char *csvFilePath);
int AheadForm_StopScript(AHEADFORM_ALL_CTRL *pstAheadForm);
int AheadForm_GetScriptPlaybackInfo(AHEADFORM_ALL_CTRL *pstAheadForm, AHEADFORM_PLAYBACK_INFO *pstInfo);

#endif
