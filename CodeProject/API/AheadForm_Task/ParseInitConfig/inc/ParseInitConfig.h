#ifndef __PARSE_INIT_CONFIG_H__
#define __PARSE_INIT_CONFIG_H__

#include <arpa/inet.h>
#include "DdsCtrl.h"
#include "AheadFormTopicData.h"

#define DEFAULT_INI_FILE_PATH "AheadFormSDK_Config.ini"

typedef struct
{
	char strNetworkInterface[AHEADFORM_NETWORK_INTERFACE_LEN_MAX];
	unsigned int domain_id;
} AHEADFORM_DDS_CFG;

typedef struct
{
	char sourceName[AHEADFORM_MOTION_SOURCE_NAME_LEN_MAX];
} AHEADFORM_HEAD_CFG;

typedef struct
{
	unsigned int faultRecordCntMax;
} AHEADFORM_STATUS_CFG;

typedef struct
{
	char strMulticastAddress[INET_ADDRSTRLEN];
	char strRobotIPv4[INET_ADDRSTRLEN];
	unsigned int videoPort;
	unsigned int micPort;
	unsigned int spkPort;
	unsigned int videoPayloadType;
	unsigned int micPayloadType;
	unsigned int spkPayloadType;
	unsigned int jitterLatency_ms;
	unsigned int rtpMtu;
} AHEADFORM_MEDIA_CFG;

typedef struct
{
	AHEADFORM_DDS_CFG stDds;
	AHEADFORM_HEAD_CFG stHead;
	AHEADFORM_STATUS_CFG stStatus;
	AHEADFORM_MEDIA_CFG stMedia;
} AHEADFORM_INIT_CONFIG;

int ParseAheadFormInitConfig(const char *iniConfigFilePath, AHEADFORM_INIT_CONFIG *pstRetCfg);

#endif
