"""CFFI declarations only; layouts and constants come from the SDK headers."""

from cffi import FFI

API_DECLARATIONS = r"""
typedef struct { ...; } AHEADFORM_INIT_CONFIG;
typedef struct { ...; } AHEADFORM_TOPICS;
typedef struct { ...; } AHEADFORM_HEAD_CTRL;
typedef struct { ...; } AHEADFORM_AUDIO_CTRL;
typedef struct { ...; } AHEADFORM_VIDEO_CTRL;
typedef struct { ...; } AHEADFORM_STATUS;
typedef struct { ...; } AHEADFORM_OPERATING_MODE_CTRL;
typedef struct { ...; } AHEADFORM_EMOTE_PLAY_CTRL;
typedef struct {
    AHEADFORM_AUDIO_CTRL stAudioCtrl;
    AHEADFORM_VIDEO_CTRL stVideoCtrl;
    ...;
} AHEADFORM_MEDIA_CTRL;
typedef struct {
    AHEADFORM_HEAD_CTRL stHeadCtrl;
    AHEADFORM_MEDIA_CTRL stMediaCtrl;
    AHEADFORM_STATUS stStatus;
    AHEADFORM_OPERATING_MODE_CTRL stOperatingModeCtrl;
    AHEADFORM_EMOTE_PLAY_CTRL stEmotePlayCtrl;
    ...;
} AHEADFORM_ROBOT_CTRL;
typedef struct {
    AHEADFORM_INIT_CONFIG stCfg;
    AHEADFORM_TOPICS stTopics;
    AHEADFORM_ROBOT_CTRL stRobotCtrl;
    ...;
} AHEADFORM_ALL_CTRL, *AHEADFORM_CTRL_HANDLE;
typedef enum {
    AHEADFORM_OPERATING_MODE_UNKNOWN,
    AHEADFORM_OPERATING_MODE_DEV,
    AHEADFORM_OPERATING_MODE_INTERACTIVE,
    ...
} ENUM_AHEADFORM_OPERATING_MODE;
typedef enum {
    AHEADFORM_OPERATING_MODE_RESULT_UNKNOWN,
    AHEADFORM_OPERATING_MODE_RESULT_SUCCEEDED,
    AHEADFORM_OPERATING_MODE_RESULT_FAILED,
    ...
} ENUM_AHEADFORM_OPERATING_MODE_RESULT;
typedef enum { AHEADFORM_NECK_STATE_INITIALIZING, ... } ENUM_AHEADFORM_NECK_STATE;
typedef struct { float micVolumePercent; float spkVolumePercent; ...; } AHEADFORM_VOLUME;
typedef struct { ...; } AHEADFORM_VOLUME_REPLY;
typedef struct { ...; } AHEADFORM_OPERATING_MODE_ASK;
typedef struct { ENUM_AHEADFORM_OPERATING_MODE mode; ...; } AHEADFORM_OPERATING_MODE_STATE;
typedef struct { ENUM_AHEADFORM_OPERATING_MODE_RESULT result; ...; } AHEADFORM_OPERATING_MODE_REPLY;
typedef struct { double pitch_rad; double yaw_rad; double roll_rad; ...; } AHEADFORM_NECK_POSE_FEEDBACK;
typedef struct {
    unsigned long long ReceiveTime_us;
    ENUM_AHEADFORM_NECK_STATE state;
    int errCode;
    unsigned char isReady;
    unsigned char isFaultActive;
    unsigned long long lastAcceptedTargetId;
    unsigned char isTargetReached;
    unsigned char isPoseValid;
    AHEADFORM_NECK_POSE_FEEDBACK stPose;
    ...;
} AHEADFORM_NECK_STATE;
#define ERRCODE_ARGC ...
#define ERRCODE_ID_NOT_FOUND ...
#define ERRCODE_REQUEST_REJECTED ...
#define ERRCODE_DATA_PACKET_FORMAT ...
int AheadForm_Init(AHEADFORM_ALL_CTRL *, const char *);
int AheadForm_Deinit(AHEADFORM_ALL_CTRL *);
int Audio_QueryVolumeAndWaitReply(AHEADFORM_AUDIO_CTRL *, AHEADFORM_VOLUME_REPLY *, int);
int Audio_GetVolumeFromReply(const AHEADFORM_VOLUME_REPLY *, AHEADFORM_VOLUME *);
int OperatingMode_ReadStateSync(AHEADFORM_OPERATING_MODE_CTRL *, AHEADFORM_OPERATING_MODE_STATE *, int);
int OperatingMode_BuildRequest(AHEADFORM_OPERATING_MODE_CTRL *, ENUM_AHEADFORM_OPERATING_MODE, AHEADFORM_OPERATING_MODE_ASK *);
int OperatingMode_SubmitAsyncRequestAndWaitReply(AHEADFORM_OPERATING_MODE_CTRL *, const AHEADFORM_OPERATING_MODE_ASK *, AHEADFORM_OPERATING_MODE_REPLY *, int);
int Status_ReadLatestNeckState(AHEADFORM_STATUS *, AHEADFORM_NECK_STATE *);
int Head_Send1NeckPose(AHEADFORM_HEAD_CTRL *, float, float, float);
unsigned long long GetTimeUsCnt_UpToNow(void);
"""

USER_DECLARATIONS = r"""
#define MAX_ARG_CNT ...
typedef struct {
    char strCmd[...];
    int (*pFunction)(AHEADFORM_ALL_CTRL *, int, char *[]);
    char *strDescription;
    ...;
} USERIN_TASK;
extern const USERIN_TASK gUserInTaskAry[...];
int PrintCmdResult(char *, int);
typedef ... FILE;
extern FILE *stdout;
int fflush(FILE *);
"""


def api_builder(extra_declarations=""):
    ffi = FFI()
    ffi.cdef(API_DECLARATIONS + extra_declarations)
    ffi.set_source("_aheadform_api", '#include "AheadForm_Task.h"\n#include "Time.h"\n')
    return ffi


def user_builder(api=None):
    ffi = FFI()
    ffi.include(api if api is not None else api_builder())
    ffi.cdef(USER_DECLARATIONS)
    ffi.set_source("_aheadform_user", '#include "UserCmdMenu.h"\n')
    return ffi
