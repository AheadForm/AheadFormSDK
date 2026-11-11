#ifndef __AHEADFORM_TOPIC_DATA_H__
#define __AHEADFORM_TOPIC_DATA_H__

#define AHEADFORM_SOURCE_NAME_LEN_MAX 64
#define AHEADFORM_MOTION_SOURCE_NAME_LEN_MAX 33
#define AHEADFORM_BLENDSHAPE_CNT 52
#define AHEADFORM_FACE_SERVO_CNT 20
#define AHEADFORM_AUDIO_DEVICE_CNT_MAX 8
#define AHEADFORM_AUDIO_DEVICE_ID_LEN_MAX 16
#define AHEADFORM_AUDIO_DEVICE_NAME_LEN_MAX 64
#define AHEADFORM_DAEMON_NAME_LEN_MAX 64
#define AHEADFORM_ERROR_REASON_LEN_MAX 129
#define AHEADFORM_NECK_MOTOR_CNT 3
#define AHEADFORM_POWER_TEMP_CNT 10
#define AHEADFORM_HARDWARE_DEVICE_FACE 1
#define AHEADFORM_HARDWARE_DEVICE_NECK 2
#define AHEADFORM_HARDWARE_DEVICE_MEDIA 3
#define AHEADFORM_HARDWARE_DEVICE_POWER 4
#define AHEADFORM_HARDWARE_DEVICE_SENSOR 5
#define AHEADFORM_HARDWARE_ACTION_PRINT_ERROR_LOG 2
#define AHEADFORM_HARDWARE_ACTION_EXIT_DAEMON 3
#define AHEADFORM_SAFETY_ACTION_MOTION_PERMISSION 1
#define AHEADFORM_SAFETY_ACTION_SAFE_STOP 2
#define AHEADFORM_SAFETY_ACTION_CLEAR_FAULT 3
#define AHEADFORM_SAFETY_ACTION_ESTOP 4
#define AHEADFORM_FAULT_EVENT_ADDED 1
#define AHEADFORM_FAULT_EVENT_CHANGED 2
#define AHEADFORM_FAULT_EVENT_CLEARED 3
#define AHEADFORM_OPERATING_MODE_RESULT_SUCCEEDED 1
#define AHEADFORM_OPERATING_MODE_RESULT_FAILED 2
#define AHEADFORM_VOLUME_OPERATION_SET 1
#define AHEADFORM_VOLUME_OPERATION_GET 2

#define AHEADFORM_PRESET_ACTION_SLEEP 1
#define AHEADFORM_PRESET_ACTION_CENTER 2
#define AHEADFORM_AUDIO_DEVICE_OPERATION_ROUTE 1
#define AHEADFORM_AUDIO_DEVICE_OPERATION_GET_LIST 2
#define AHEADFORM_AUDIO_DEVICE_OPERATION_GET_CURRENT 3
#define AHEADFORM_AUDIO_DEVICE_MIC 0
#define AHEADFORM_AUDIO_DEVICE_SPK 1
#define AHEADFORM_CHANNEL_KEEP 0
#define AHEADFORM_CHANNEL_DISABLE 1
#define AHEADFORM_CHANNEL_ENABLE 2
#define AHEADFORM_OPERATING_MODE_DEV 1
#define AHEADFORM_OPERATING_MODE_INTERACTIVE 2
#define AHEADFORM_MOTION_DICTIONARY_VERSION 1
#define AHEADFORM_MOTION_CSV_VERSION 1

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
} AHEADFORM_VOLUME_REPLY_SAMPLE;

typedef struct
{
	float leftPitch_rad;
	float leftYaw_rad;
	float rightPitch_rad;
	float rightYaw_rad;
} AHEADFORM_EYE_POSE;

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
	int sec;
	unsigned int nanosec;
} AHEADFORM_SOURCE_TIME;

typedef struct
{
	AHEADFORM_SOURCE_TIME stSourceTime;
	unsigned long long askId;
	char sourceName[AHEADFORM_MOTION_SOURCE_NAME_LEN_MAX];
	unsigned char action;
} AHEADFORM_PRESET_MOTION_ASK;

typedef struct
{
	AHEADFORM_SOURCE_TIME stSourceTime;
	unsigned long long askId;
	char sourceName[AHEADFORM_SOURCE_NAME_LEN_MAX];
	unsigned char videoCtrl;
	unsigned char micCtrl;
	unsigned char playbackCtrl;
	unsigned char vadCtrl;
} AHEADFORM_MEDIA_CHANNEL_ASK;

typedef struct
{
	AHEADFORM_SOURCE_TIME stSourceTime;
	unsigned long long askId;
	char sourceName[AHEADFORM_SOURCE_NAME_LEN_MAX];
	unsigned char operation;
	unsigned char deviceType;
	unsigned char hasMicDevice;
	char micDeviceId[AHEADFORM_AUDIO_DEVICE_ID_LEN_MAX];
	unsigned char hasSpkDevice;
	char spkDeviceId[AHEADFORM_AUDIO_DEVICE_ID_LEN_MAX];
} AHEADFORM_AUDIO_DEVICE_ASK;

typedef struct
{
	AHEADFORM_SOURCE_TIME stSourceTime;
	unsigned long long askId;
	char sourceName[AHEADFORM_SOURCE_NAME_LEN_MAX];
	unsigned char action;
	unsigned char device;
} AHEADFORM_HARDWARE_CONTROL_ASK;

typedef struct
{
	AHEADFORM_SOURCE_TIME stSourceTime;
	unsigned long long askId;
	char sourceName[AHEADFORM_SOURCE_NAME_LEN_MAX];
	unsigned char action;
	unsigned char faceMotionPermitted;
	unsigned char neckMotionPermitted;
} AHEADFORM_SAFETY_CONTROL_ASK;

typedef struct
{
	unsigned long long ReceiveTime_us;
	AHEADFORM_SOURCE_TIME stSourceTime;
	unsigned long long askId;
	char sourceName[AHEADFORM_MOTION_SOURCE_NAME_LEN_MAX];
	int retCode;
} AHEADFORM_PRESET_MOTION_REPLY_SAMPLE;

typedef struct
{
	unsigned long long ReceiveTime_us;
	AHEADFORM_SOURCE_TIME stSourceTime;
	unsigned long long askId;
	char sourceName[AHEADFORM_SOURCE_NAME_LEN_MAX];
	int retCode;
} AHEADFORM_MEDIA_CHANNEL_REPLY_SAMPLE;

typedef struct
{
	char deviceId[AHEADFORM_AUDIO_DEVICE_ID_LEN_MAX];
	char deviceName[AHEADFORM_AUDIO_DEVICE_NAME_LEN_MAX];
} AHEADFORM_AUDIO_DEVICE_INFO;

typedef struct
{
	unsigned long long ReceiveTime_us;
	AHEADFORM_SOURCE_TIME stSourceTime;
	unsigned long long askId;
	char sourceName[AHEADFORM_SOURCE_NAME_LEN_MAX];
	unsigned char operation;
	int retCode;
	unsigned char deviceType;
	unsigned char queryDataValid;
	unsigned int deviceCnt;
	AHEADFORM_AUDIO_DEVICE_INFO szstDevice[AHEADFORM_AUDIO_DEVICE_CNT_MAX];
} AHEADFORM_AUDIO_DEVICE_REPLY_SAMPLE;

typedef struct
{
	unsigned long long ReceiveTime_us;
	AHEADFORM_SOURCE_TIME stSourceTime;
	unsigned long long askId;
	char sourceName[AHEADFORM_SOURCE_NAME_LEN_MAX];
	unsigned char action;
	unsigned char device;
	int retCode;
} AHEADFORM_HARDWARE_CONTROL_REPLY_SAMPLE;

typedef struct
{
	unsigned long long ReceiveTime_us;
	AHEADFORM_SOURCE_TIME stSourceTime;
	unsigned long long askId;
	char sourceName[AHEADFORM_SOURCE_NAME_LEN_MAX];
	unsigned char action;
	int retCode;
} AHEADFORM_SAFETY_CONTROL_REPLY_SAMPLE;

typedef struct
{
	unsigned long long ReceiveTime_us;
	AHEADFORM_SOURCE_TIME stSourceTime;
	unsigned char arbiterAlive;
	int arbiterErrorCode;
	char arbiterErrorReason[AHEADFORM_ERROR_REASON_LEN_MAX];
	unsigned char allOnline;
	unsigned char allReady;
	unsigned char anyFaultActive;
	unsigned char anyEstopLatched;
	char activeMotionSourceName[AHEADFORM_MOTION_SOURCE_NAME_LEN_MAX];
	unsigned char faceMotionPermitted;
	unsigned char neckMotionPermitted;
	unsigned char safeStopMotionBlockActive;
} AHEADFORM_HARDWARE_STATE_SAMPLE;

typedef struct
{
	unsigned char handshaked;
	unsigned char hasControl;
	unsigned char statusPushGranted;
	unsigned char estopPermitGranted;
	unsigned short daemonEndpoint;
	unsigned int daemonState;
	int helloErrCode;
	unsigned char faultActive;
	char daemonName[AHEADFORM_DAEMON_NAME_LEN_MAX];
} AHEADFORM_DEVICE_CONNECTION_STATE;

typedef struct
{
	unsigned char online;
	unsigned char ready;
	unsigned int state;
	int errCode;
	unsigned char faultActive;
	unsigned char ownerOnline;
	unsigned char canOk;
	unsigned char reached;
	unsigned long long lastAcceptedTargetId;
	unsigned int targetHasExec_ms;
	unsigned int trajectoryPlanRun_ms;
} AHEADFORM_DEVICE_OPERATIONAL_STATUS;

typedef struct
{
	AHEADFORM_DEVICE_CONNECTION_STATE stConnection;
	AHEADFORM_DEVICE_OPERATIONAL_STATUS stOperationalStatus;
} AHEADFORM_DEVICE_STATE;

typedef struct
{
	unsigned short servoId;
	double targetLogic_deg;
	double motionRefLogic_deg;
	double feedbackTargetLogic_deg;
	double currentLogic_deg;
	double logicSpeed_deg_s;
	float temperatureC;
	float voltageV;
	float currentA;
	unsigned int feedbackAge_ms;
	unsigned char hlsPresentStatus;
	int faultCode;
} AHEADFORM_FACE_SERVO_STATUS;

typedef struct
{
	AHEADFORM_SOURCE_TIME stSourceTime;
	unsigned long long sampleSeq;
	unsigned long long msgId;
	unsigned long long refMsgId;
	unsigned short daemonId;
	unsigned int state;
	int errCode;
	unsigned char ready;
	unsigned char online;
	unsigned char faultActive;
	unsigned char ownerOnline;
	unsigned long long lastAcceptedTargetId;
	unsigned long long statusSeq;
	unsigned long long statusBuildTime_us;
	int faceCtrlFaultCode;
	unsigned char allServoReady;
	unsigned int servoCount;
	AHEADFORM_FACE_SERVO_STATUS szstServo[AHEADFORM_FACE_SERVO_CNT];
} AHEADFORM_FACE_STATUS;

typedef struct
{
	unsigned long long ReceiveTime_us;
	AHEADFORM_SOURCE_TIME stSourceTime;
	AHEADFORM_DEVICE_STATE stDeviceState;
	unsigned char hasLatestSample;
	AHEADFORM_FACE_STATUS stLatestSample;
} AHEADFORM_FACE_STATE_SAMPLE;

typedef struct
{
	unsigned char motorId;
	unsigned char feedbackTimeout;
	unsigned char lkMotorState;
	unsigned char errorState;
	unsigned int sampleAge_ms;
	float target_deg;
	float lastMotionRef_deg;
	float currentRaw_deg;
	float currentFiltered_deg;
	float speed_deg_s;
	float temperatureC;
	float voltageV;
	float currentA;
} AHEADFORM_NECK_MOTOR_SAMPLE;

typedef struct
{
	AHEADFORM_SOURCE_TIME stSourceTime;
	unsigned long long sampleSeq;
	unsigned long long msgId;
	unsigned long long refMsgId;
	unsigned short daemonId;
	unsigned int state;
	int errCode;
	unsigned char ready;
	unsigned char online;
	unsigned char faultActive;
	unsigned char ownerOnline;
	unsigned char canOk;
	unsigned char reached;
	unsigned long long lastAcceptedTargetId;
	unsigned long long statusSeq;
	unsigned long long statusBuildTime_us;
	unsigned int targetHasExec_ms;
	unsigned int trajectoryPlanRun_ms;
	AHEADFORM_NECK_POSE stPose;
	AHEADFORM_NECK_MOTOR_SAMPLE szstMotor[AHEADFORM_NECK_MOTOR_CNT];
} AHEADFORM_NECK_STATUS;

typedef struct
{
	unsigned long long ReceiveTime_us;
	AHEADFORM_SOURCE_TIME stSourceTime;
	AHEADFORM_DEVICE_STATE stDeviceState;
	unsigned char hasLatestSample;
	AHEADFORM_NECK_STATUS stLatestSample;
} AHEADFORM_NECK_STATE_SAMPLE;

typedef struct
{
	unsigned int channelState;
	unsigned char deviceAvailable;
	unsigned int activeReaderMask;
	unsigned int readableFrameCount;
	unsigned long long lastSequence;
	unsigned long long droppedFrameCount;
} AHEADFORM_MEDIA_CHANNEL_STATUS;

typedef struct
{
	AHEADFORM_SOURCE_TIME stSourceTime;
	unsigned long long sampleSeq;
	unsigned long long msgId;
	unsigned long long refMsgId;
	unsigned short daemonId;
	unsigned int state;
	int errCode;
	unsigned char ready;
	unsigned char online;
	unsigned char faultActive;
	unsigned char ownerOnline;
	unsigned char mediaTransClientConnected;
	unsigned char pulseAudioAecReady;
	AHEADFORM_MEDIA_CHANNEL_STATUS video;
	AHEADFORM_MEDIA_CHANNEL_STATUS mic;
	AHEADFORM_MEDIA_CHANNEL_STATUS playback;
	AHEADFORM_MEDIA_CHANNEL_STATUS vad;
} AHEADFORM_MEDIA_STATUS;

typedef struct
{
	unsigned long long ReceiveTime_us;
	AHEADFORM_SOURCE_TIME stSourceTime;
	AHEADFORM_DEVICE_STATE stDeviceState;
	unsigned char hasLatestSample;
	AHEADFORM_MEDIA_STATUS stLatestSample;
} AHEADFORM_MEDIA_STATE_SAMPLE;

typedef struct
{
	AHEADFORM_SOURCE_TIME stSourceTime;
	unsigned long long sampleSeq;
	unsigned long long msgId;
	unsigned long long refMsgId;
	unsigned long long statusBuildTime_us;
	unsigned short daemonId;
	unsigned int state;
	int errCode;
	unsigned char ready;
	unsigned char online;
	unsigned char faultActive;
	unsigned char ownerOnline;
	unsigned char requestUpperPowerOn;
	unsigned char requestLowerPowerOn;
	unsigned char upperPowerOn;
	unsigned char lowerPowerOn;
	unsigned char requestMainPowerOff;
	unsigned char camWorkStateLedOn;
	unsigned char powerSwitchReleased;
	unsigned char fanRotationSpeedPercent;
	unsigned char tempLimitC;
	signed char upperTemp_C[AHEADFORM_POWER_TEMP_CNT];
	unsigned int upperBoardFeedbackAge_ms;
	unsigned int lowerBoardFeedbackAge_ms;
} AHEADFORM_POWER_STATUS;

typedef struct
{
	unsigned long long ReceiveTime_us;
	AHEADFORM_SOURCE_TIME stSourceTime;
	AHEADFORM_DEVICE_STATE stDeviceState;
	unsigned char hasLatestSample;
	AHEADFORM_POWER_STATUS stLatestSample;
} AHEADFORM_POWER_STATE_SAMPLE;

typedef struct
{
	AHEADFORM_SOURCE_TIME stSourceTime;
	unsigned long long sampleSeq;
	unsigned long long msgId;
	unsigned long long refMsgId;
	unsigned short daemonId;
	unsigned int state;
	int errCode;
	unsigned char ready;
	unsigned char online;
	unsigned char faultActive;
	unsigned char ownerOnline;
	unsigned long long statusSeq;
	unsigned long long statusBuildTime_us;
	unsigned int imuRpyFeedbackAge_ms;
	float roll_rad;
	float pitch_rad;
	float yaw_rad;
} AHEADFORM_SENSOR_STATUS;

typedef struct
{
	unsigned long long ReceiveTime_us;
	AHEADFORM_SOURCE_TIME stSourceTime;
	AHEADFORM_DEVICE_STATE stDeviceState;
	unsigned char hasLatestSample;
	AHEADFORM_SENSOR_STATUS stLatestSample;
} AHEADFORM_SENSOR_STATE_SAMPLE;

typedef struct
{
	unsigned char device;
	unsigned char source;
	unsigned int objectId;
	int errCode;
} AHEADFORM_FAULT_RECORD;

typedef struct
{
	unsigned long long ReceiveTime_us;
	AHEADFORM_SOURCE_TIME stSourceTime;
	unsigned long long sourceSeq;
	unsigned char eventType;
	AHEADFORM_FAULT_RECORD stFault;
} AHEADFORM_FAULT_EVENT_SAMPLE;

typedef struct
{
	unsigned long long t_us;
	AHEADFORM_NECK_POSE stNeck;
	AHEADFORM_EYE_POSE stEyes;
	float blendshapes[AHEADFORM_BLENDSHAPE_CNT];
} AHEADFORM_MOTION_FRAME;

typedef struct
{
	unsigned long long ReceiveTime_us;
	int sourceTime_sec;
	unsigned int sourceTime_nanosec;
	unsigned int faultRecordCnt;
	AHEADFORM_FAULT_RECORD szstFaultRecord[];
} AHEADFORM_FAULT_SET_SAMPLE;

typedef struct
{
	unsigned long long requestId;
	unsigned int mode;
} AHEADFORM_OPERATING_MODE_ASK;

typedef struct
{
	unsigned long long ReceiveTime_us;
	unsigned long long requestId;
	unsigned int result;
} AHEADFORM_OPERATING_MODE_REPLY_SAMPLE;


typedef struct
{
	char sourceName[AHEADFORM_MOTION_SOURCE_NAME_LEN_MAX];
	AHEADFORM_MOTION_FRAME stFrame;
} AHEADFORM_MOTION_COMMAND;

typedef struct
{
	char sourceName[AHEADFORM_MOTION_SOURCE_NAME_LEN_MAX];
	unsigned long long commandSeq;
	AHEADFORM_NECK_POSE stNeck;
} AHEADFORM_NECK_COMMAND;

typedef struct
{
	char sourceName[AHEADFORM_MOTION_SOURCE_NAME_LEN_MAX];
	unsigned long long commandSeq;
	AHEADFORM_EYE_POSE stEyes;
	float blendshapes[AHEADFORM_BLENDSHAPE_CNT];
} AHEADFORM_FACE_COMMAND;

typedef struct
{
	unsigned long long commandSeq;
	double targetLogic_deg[AHEADFORM_FACE_SERVO_CNT];
} AHEADFORM_SERVO_COMMAND;

#endif
