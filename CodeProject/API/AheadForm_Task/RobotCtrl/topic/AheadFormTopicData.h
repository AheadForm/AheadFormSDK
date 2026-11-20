#ifndef __AHEADFORM_TOPIC_DATA_H__
#define __AHEADFORM_TOPIC_DATA_H__

#define AHEADFORM_SOURCE_NAME_LEN_MAX 64
#define AHEADFORM_VOLUME_OPERATION_SET 1
#define AHEADFORM_VOLUME_OPERATION_GET 2
#define AHEADFORM_BLENDSHAPE_CNT 52
#define AHEADFORM_FACE_SERVO_CNT 20
#define AHEADFORM_MOTION_VERSION 2
//String capacities include the terminating NUL.
#define AHEADFORM_AUDIO_DEVICE_ID_LEN_MAX 16
#define AHEADFORM_AUDIO_DEVICE_NAME_LEN_MAX 64
#define AHEADFORM_AUDIO_DEVICE_CNT_MAX 8
#define AHEADFORM_NECK_MOTOR_CNT_MAX 3
#define AHEADFORM_FAULT_CNT_MAX 64
#define AHEADFORM_FAULT_SOURCE_LEN_MAX 64
#define AHEADFORM_FAULT_REASON_LEN_MAX 256
#define AHEADFORM_FAULT_TIMESTAMP_LEN_MAX 20
#define AHEADFORM_EMOTE_SOURCE_LEN_MAX 257
#define AHEADFORM_EMOTE_CLIP_CNT_MAX 64
#define AHEADFORM_EMOTE_REPLY_REASON_LEN_MAX 257
#define AHEADFORM_EMOTE_EVENT_STATE_LEN_MAX 64
#define AHEADFORM_EMOTE_EVENT_REASON_LEN_MAX 1024
#define AHEADFORM_EMOTE_EVENT_SOURCE_LEN_MAX 32896

typedef enum
{
	AHEADFORM_OPERATING_MODE_UNKNOWN = 0,
	AHEADFORM_OPERATING_MODE_DEV,
	AHEADFORM_OPERATING_MODE_INTERACTIVE
} ENUM_AHEADFORM_OPERATING_MODE;

typedef enum
{
	AHEADFORM_OPERATING_MODE_RESULT_UNKNOWN = 0,
	AHEADFORM_OPERATING_MODE_RESULT_SUCCEEDED,
	AHEADFORM_OPERATING_MODE_RESULT_FAILED
} ENUM_AHEADFORM_OPERATING_MODE_RESULT;

typedef enum
{
	AHEADFORM_SENSOR_STATE_INITIALIZING = 0,
	AHEADFORM_SENSOR_STATE_READY,
	AHEADFORM_SENSOR_STATE_RECOVERABLE_FAULT,
	AHEADFORM_SENSOR_STATE_ESTOP_LATCHED
} ENUM_AHEADFORM_SENSOR_STATE;

typedef enum
{
	AHEADFORM_NECK_STATE_INITIALIZING = 0,
	AHEADFORM_NECK_STATE_MOTION_TARGET_DISABLED,
	AHEADFORM_NECK_STATE_MOTION_TARGET_ENABLED,
	AHEADFORM_NECK_STATE_STOPPING_TO_SAFE_POSE,
	AHEADFORM_NECK_STATE_ESTOP_LATCHED,
	AHEADFORM_NECK_STATE_RECOVERABLE_FAULT,
	AHEADFORM_NECK_STATE_UNRECOVERABLE_FAULT
} ENUM_AHEADFORM_NECK_STATE;

typedef enum
{
	AHEADFORM_AUDIO_DEVICE_OPERATION_ROUTE = 1,
	AHEADFORM_AUDIO_DEVICE_OPERATION_GET_LIST,
	AHEADFORM_AUDIO_DEVICE_OPERATION_GET_CURRENT
} ENUM_AHEADFORM_AUDIO_DEVICE_OPERATION;

typedef enum
{
	AHEADFORM_AUDIO_DEVICE_TYPE_MIC = 0,
	AHEADFORM_AUDIO_DEVICE_TYPE_SPK
} ENUM_AHEADFORM_AUDIO_DEVICE_TYPE;

typedef enum
{
	AHEADFORM_EMOTE_CMD_PLAY = 0,
	AHEADFORM_EMOTE_CMD_PAUSE,
	AHEADFORM_EMOTE_CMD_RESUME,
	AHEADFORM_EMOTE_CMD_STOP
} ENUM_AHEADFORM_EMOTE_CMD;

typedef enum
{
	AHEADFORM_EMOTE_SOURCE_ID = 0,
	AHEADFORM_EMOTE_SOURCE_NAME
} ENUM_AHEADFORM_EMOTE_SOURCE_TYPE;

typedef enum
{
	AHEADFORM_EMOTE_GAP_DEFAULT = 0,
	AHEADFORM_EMOTE_GAP_HOLD,
	AHEADFORM_EMOTE_GAP_INTERPOLATE
} ENUM_AHEADFORM_EMOTE_GAP_BEHAVIOR;

typedef enum
{
	AHEADFORM_EMOTE_RESULT_SUCCEEDED = 0,
	AHEADFORM_EMOTE_RESULT_FAILED,
	AHEADFORM_EMOTE_RESULT_SUPERSEDED,
	AHEADFORM_EMOTE_RESULT_STOPPED
} ENUM_AHEADFORM_EMOTE_RESULT;

typedef struct
{
	float micVolumePercent;
	float spkVolumePercent;
} AHEADFORM_VOLUME;

typedef struct
{
	unsigned long long askId;
	AHEADFORM_VOLUME stVolume;
	char sourceName[AHEADFORM_SOURCE_NAME_LEN_MAX];
	unsigned char operation;
	unsigned char isSetMicVolume;
	unsigned char isSetSpkVolume;
} AHEADFORM_VOLUME_ASK;

typedef struct
{
	unsigned long long ReceiveTime_us;
	unsigned long long askId;
	AHEADFORM_VOLUME stVolume;
	int retCode;
	char sourceName[AHEADFORM_SOURCE_NAME_LEN_MAX];
	unsigned char operation;
	unsigned char queryDataValid;
} AHEADFORM_VOLUME_REPLY;

typedef struct
{
	float pitch_rad;
	float yaw_rad;
	float roll_rad;
	float headX;
	float headY;
	float headZ;
} AHEADFORM_NECK_POSE;

typedef struct
{
	float leftPitch_rad;
	float leftYaw_rad;
	float rightPitch_rad;
	float rightYaw_rad;
} AHEADFORM_EYE_POSE;

typedef struct
{
	unsigned long long t_us;
	AHEADFORM_NECK_POSE stNeck;
	AHEADFORM_EYE_POSE stEyes;
	float blendshapes[AHEADFORM_BLENDSHAPE_CNT];
} AHEADFORM_MOTION_POSE;

typedef struct
{
	int sec;
	unsigned int ns;
} AHEADFORM_TIME;

typedef struct
{
	unsigned long long requestId;
	ENUM_AHEADFORM_OPERATING_MODE mode;
} AHEADFORM_OPERATING_MODE_ASK;

typedef struct
{
	unsigned long long ReceiveTime_us;
	unsigned long long requestId;
	ENUM_AHEADFORM_OPERATING_MODE_RESULT result;
} AHEADFORM_OPERATING_MODE_REPLY;

typedef struct
{
	unsigned long long ReceiveTime_us;
	ENUM_AHEADFORM_OPERATING_MODE mode;
} AHEADFORM_OPERATING_MODE_STATE;

typedef struct
{
	char strDeviceId[AHEADFORM_AUDIO_DEVICE_ID_LEN_MAX];
	char deviceName[AHEADFORM_AUDIO_DEVICE_NAME_LEN_MAX];
} AHEADFORM_AUDIO_DEVICE_INFO;

typedef struct
{
	//An empty ID leaves the corresponding route unchanged.
	char micDeviceId[AHEADFORM_AUDIO_DEVICE_ID_LEN_MAX];
	char spkDeviceId[AHEADFORM_AUDIO_DEVICE_ID_LEN_MAX];
} AHEADFORM_AUDIO_DEVICE_ROUTE;

typedef struct
{
	unsigned long long askId;
	char sourceName[AHEADFORM_SOURCE_NAME_LEN_MAX];
	ENUM_AHEADFORM_AUDIO_DEVICE_OPERATION operation;
	ENUM_AHEADFORM_AUDIO_DEVICE_TYPE deviceType;
	AHEADFORM_AUDIO_DEVICE_ROUTE stRoute;
} AHEADFORM_AUDIO_DEVICE_ASK;

typedef struct
{
	unsigned long long ReceiveTime_us;
	AHEADFORM_TIME stTime;
	unsigned long long askId;
	char sourceName[AHEADFORM_SOURCE_NAME_LEN_MAX];
	int retCode;
	ENUM_AHEADFORM_AUDIO_DEVICE_OPERATION operation;
	ENUM_AHEADFORM_AUDIO_DEVICE_TYPE deviceType;
	unsigned char queryDataValid;
	unsigned int deviceCnt;
	//Only elements in [0, deviceCnt) are valid.
	AHEADFORM_AUDIO_DEVICE_INFO szstDeviceInfo[AHEADFORM_AUDIO_DEVICE_CNT_MAX];
} AHEADFORM_AUDIO_DEVICE_REPLY;

typedef struct
{
	unsigned long long ReceiveTime_us;
	unsigned long long statusSeq;
	unsigned long long statusBuildTime_us;
	ENUM_AHEADFORM_SENSOR_STATE state;
	int errCode;
	unsigned int imuRpyFeedbackAge_ms;
	float roll_rad;
	float pitch_rad;
	float yaw_rad;
	unsigned char isReady;
} AHEADFORM_SENSOR_STATE;

typedef struct
{
	double roll_rad;
	double pitch_rad;
	double yaw_rad;
} AHEADFORM_NECK_POSE_FEEDBACK;

typedef struct
{
	unsigned char motorId;
	unsigned char isTemperatureValid;
	float Temperature_C;
	unsigned char isCurrentValid;
	float Current_A;
	unsigned char isVoltageValid;
	float Voltage_V;
	unsigned char isMotorFaultCodeValid;
	int motorFaultCode;
	unsigned char isForceValid;
	double Force_N;
} AHEADFORM_NECK_MOTOR_SAMPLE;

typedef struct
{
	unsigned long long ReceiveTime_us;
	ENUM_AHEADFORM_NECK_STATE state;
	int errCode;
	unsigned char isReady;
	unsigned char isFaultActive;
	unsigned long long lastAcceptedTargetId;
	unsigned char isTargetReached;
	unsigned char isPoseValid;
	AHEADFORM_NECK_POSE_FEEDBACK stPose;
	unsigned int motorCnt;
	AHEADFORM_NECK_MOTOR_SAMPLE szstMotor[AHEADFORM_NECK_MOTOR_CNT_MAX];
} AHEADFORM_NECK_STATE;

typedef struct
{
	char source[AHEADFORM_FAULT_SOURCE_LEN_MAX];
	unsigned int processFaultCode;
	unsigned char level;
	char reason[AHEADFORM_FAULT_REASON_LEN_MAX];
	unsigned int motorFaultCode;
	char timestamp[AHEADFORM_FAULT_TIMESTAMP_LEN_MAX];
} AHEADFORM_FAULT_RECORD;

/*
	One complete list copied from one DDS FaultPool sample; only [0, faultCnt) is valid.
	ReceiveTime_us is local monotonic receive-processing time, not a fault occurrence time.
	An empty list is a valid snapshot; each timestamp is copied from its source record.
*/
typedef struct
{
	unsigned long long ReceiveTime_us;
	unsigned int faultCnt;
	AHEADFORM_FAULT_RECORD szstFault[AHEADFORM_FAULT_CNT_MAX];
} AHEADFORM_FAULT_POOL;

typedef struct
{
	char strSource[AHEADFORM_EMOTE_SOURCE_LEN_MAX];
	unsigned int startAt_ms;
	unsigned int clipBegin_ms;
	unsigned int clipEnd_ms;
	ENUM_AHEADFORM_EMOTE_GAP_BEHAVIOR gapBehavior;
} AHEADFORM_EMOTE_CLIP;

typedef struct
{
	unsigned long long requestId;
	ENUM_AHEADFORM_EMOTE_CMD cmd;
	ENUM_AHEADFORM_EMOTE_SOURCE_TYPE sourceType;
	AHEADFORM_EMOTE_CLIP szstMotionClips[AHEADFORM_EMOTE_CLIP_CNT_MAX];
	unsigned int motionClipCnt;
	AHEADFORM_EMOTE_CLIP szstAudioClips[AHEADFORM_EMOTE_CLIP_CNT_MAX];
	unsigned int audioClipCnt;
	unsigned char isLoop;
} AHEADFORM_EMOTE_ASK;

typedef struct
{
	unsigned long long ReceiveTime_us;
	unsigned long long requestId;
	ENUM_AHEADFORM_EMOTE_RESULT result;
	char strReason[AHEADFORM_EMOTE_REPLY_REASON_LEN_MAX];
} AHEADFORM_EMOTE_REPLY;

/*
	requestId identifies the original PLAY request.
	Strings are copied verbatim; source may contain a resource name or ID, or be empty.
	ReceiveTime_us is local monotonic receive-processing time.
*/
typedef struct
{
	unsigned long long ReceiveTime_us;
	unsigned long long requestId;
	char strState[AHEADFORM_EMOTE_EVENT_STATE_LEN_MAX];
	char strReason[AHEADFORM_EMOTE_EVENT_REASON_LEN_MAX];
	char strSource[AHEADFORM_EMOTE_EVENT_SOURCE_LEN_MAX];
	unsigned char isLoop;
	unsigned char isReset;
} AHEADFORM_EMOTE_EVENT;

extern const char* const gAheadFormBlendshapeNames[AHEADFORM_BLENDSHAPE_CNT];

#endif
