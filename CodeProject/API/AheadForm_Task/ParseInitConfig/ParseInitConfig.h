#ifndef __PARSE_INIT_CONFIG_H__
#define __PARSE_INIT_CONFIG_H__

#include "DdsCtrl.h"

#define DEFAULT_INI_FILE_PATH "AheadFormSDK_Config.ini"
#define AHEADFORM_HEAD_SOURCE_NAME_LEN_MAX 33
#define AHEADFORM_IPV4_ADDR_LEN_MAX 16

typedef struct
{
	char strNetworkInterface[AHEADFORM_NETWORK_INTERFACE_LEN_MAX];
	unsigned int domain_id;
} AHEADFORM_DDS_CFG;

typedef struct
{
	char sourceName[AHEADFORM_HEAD_SOURCE_NAME_LEN_MAX];
} AHEADFORM_HEAD_CFG;

typedef struct
{
	unsigned char isVideoCaptureOn;
	unsigned char isMicCaptureOn;
	unsigned char isSdkPlayAudioOn;
	char multicastAddress[AHEADFORM_IPV4_ADDR_LEN_MAX];
	char strRobotIPv4[AHEADFORM_IPV4_ADDR_LEN_MAX];
	unsigned int videoPort;
	unsigned int micPort;
	unsigned int spkPort;
	unsigned int videoPayloadType;
	unsigned int micPayloadType;
	unsigned int spkPayloadType;
	unsigned int jitterBufferLatencyMax_ms;
	unsigned int rtpMtu;
	unsigned int videoFrameSizeMax;
} AHEADFORM_MEDIA_CFG;

typedef struct
{
	AHEADFORM_DDS_CFG stDds;
	AHEADFORM_HEAD_CFG stHead;
	AHEADFORM_MEDIA_CFG stMedia;
} AHEADFORM_INIT_CONFIG;

int ParseAheadFormInitConfig(const char *iniConfigFilePath, AHEADFORM_INIT_CONFIG *pstRetCfg);
//Match the arbiter rule: ^[a-z][a-z0-9_]{0,31}$.
int IsValidHeadSourceName(const char *sourceName);

#endif
