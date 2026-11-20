#ifndef __AHEADFORM_TASK_H__
#define __AHEADFORM_TASK_H__

#include "ParseInitConfig.h"
#include "Head.h"
#include "Media.h"
#include "Status.h"
#include "OperatingMode.h"
#include "EmotePlay.h"
#include "AheadFormTopics.h"

typedef struct
{
	AHEADFORM_HEAD_CTRL stHeadCtrl;
	AHEADFORM_MEDIA_CTRL stMediaCtrl;
	AHEADFORM_STATUS stStatus;
	AHEADFORM_OPERATING_MODE_CTRL stOperatingModeCtrl;
	AHEADFORM_EMOTE_PLAY_CTRL stEmotePlayCtrl;
} AHEADFORM_ROBOT_CTRL;

typedef struct
{
	AHEADFORM_INIT_CONFIG stCfg;
	AHEADFORM_TOPICS stTopics;
	AHEADFORM_ROBOT_CTRL stRobotCtrl;
} AHEADFORM_ALL_CTRL, *AHEADFORM_CTRL_HANDLE;

int AheadForm_Init(AHEADFORM_ALL_CTRL *pstAheadForm, const char *iniConfigFilePath);//Pass DEFAULT_INI_FILE_PATH when the INI file is beside the executable and the working directory is that directory.
int AheadForm_Deinit(AHEADFORM_ALL_CTRL *pstAheadForm);
AHEADFORM_TOPIC_READER* AheadForm_RegisterTopicReader(AHEADFORM_ALL_CTRL *pstAheadForm, ENUM_AHEADFORM_TOPIC_ID topicId, const AHEADFORM_TOPIC_READER_PARA *pstReaderPara);
int AheadForm_UnregisterTopicReader(AHEADFORM_ALL_CTRL *pstAheadForm, AHEADFORM_TOPIC_READER *pstReader);
AHEADFORM_TOPIC_WRITER* AheadForm_RegisterTopicWriter(AHEADFORM_ALL_CTRL *pstAheadForm, ENUM_AHEADFORM_TOPIC_ID topicId);
int AheadForm_UnregisterTopicWriter(AHEADFORM_ALL_CTRL *pstAheadForm, AHEADFORM_TOPIC_WRITER *pstWriter);

#endif
