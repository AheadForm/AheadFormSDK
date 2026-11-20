#include "ParseInitConfig.h"
#include "ini.h"

#include <arpa/inet.h>

typedef struct
{
	AHEADFORM_INIT_CONFIG *pstCfg;
	unsigned char isDdsParsed;
	unsigned char isHeadParsed;
	unsigned char isMediaParsed;
} INIT_CONFIG_PARSE_CTX;

static int CallBack_Parse1iniSection(INI_SECTION *pstSection, void *pRetCfg);
static int ParseDdsSection(INI_SECTION *pstSection, INIT_CONFIG_PARSE_CTX *pstCtx);
static int ParseHeadSection(INI_SECTION *pstSection, INIT_CONFIG_PARSE_CTX *pstCtx);
static int ParseMediaSection(INI_SECTION *pstSection, INIT_CONFIG_PARSE_CTX *pstCtx);
static int ValidateAheadFormInitConfig(const AHEADFORM_INIT_CONFIG *pstCfg, const INIT_CONFIG_PARSE_CTX *pstCtx);


int ParseAheadFormInitConfig(const char *iniConfigFilePath, AHEADFORM_INIT_CONFIG *pstRetCfg)
{
	int ret;
	INIT_CONFIG_PARSE_CTX stParseCtx = {0};

	if (iniConfigFilePath == NULL || iniConfigFilePath[0] == 0)
		return ERR_ARGV_1;

	if (pstRetCfg == NULL)
		return ERR_ARGV_2;

	memset(pstRetCfg, 0, sizeof(*pstRetCfg));
	stParseCtx.pstCfg = pstRetCfg;
	if ( (ret = ini_parse(iniConfigFilePath, CallBack_Parse1iniSection, &stParseCtx)) != RET_SUCCESS )
	{
		if (ret < 0)
		{
			printf("%s() call ini_parse(%s) failed: %s\r\n", __FUNCTION__, iniConfigFilePath, strerror(errno));
			return ret;
		}
		else
		{
			printf("%s() call ini_parse(%s) failed at line %d.\r\n", __FUNCTION__, iniConfigFilePath, ret);
			return ERR_CONFIG_PARSE_FAILED;
		}
	}

	if (!ValidateAheadFormInitConfig(pstRetCfg, &stParseCtx))
	{
		printf("%s() ValidateAheadFormInitConfig(%s) failed.\r\n", __FUNCTION__, iniConfigFilePath);
		return ERR_CONFIG_VALUE_INVALID;
	}
	printf("%s() %s succeed.\r\n", __FUNCTION__, iniConfigFilePath);
	return RET_SUCCESS;
}

int IsValidHeadSourceName(const char *sourceName)
{
	unsigned int i;

	if (sourceName == NULL || sourceName[0] < 'a' || sourceName[0] > 'z')
		return 0;

	for (i = 1; i < AHEADFORM_HEAD_SOURCE_NAME_LEN_MAX; i++)
	{
		if (sourceName[i] == 0)
			return 1;

		if (!((sourceName[i] >= 'a' && sourceName[i] <= 'z') || (sourceName[i] >= '0' && sourceName[i] <= '9') || sourceName[i] == '_'))
			return 0;
	}

	return 0;
}

static int CallBack_Parse1iniSection(INI_SECTION *pstSection, void *pRetCfg)
{
	INIT_CONFIG_PARSE_CTX *pstCtx = (INIT_CONFIG_PARSE_CTX*)pRetCfg;

	if (pstSection == NULL || pstSection->sectionName == NULL)
	{
		PRINT_FUNCTION_ERRINFO(ERR_ARGV_1, __FUNCTION__);
		return 0;
	}

	if (pstCtx == NULL || pstCtx->pstCfg == NULL)
	{
		PRINT_FUNCTION_ERRINFO(ERR_ARGV_2, __FUNCTION__);
		return 0;
	}

	if (strcmp(pstSection->sectionName, "dds") == 0)
		return ParseDdsSection(pstSection, pstCtx);
	else if (strcmp(pstSection->sectionName, "head") == 0)
		return ParseHeadSection(pstSection, pstCtx);
	else if (strcmp(pstSection->sectionName, "media") == 0)
		return ParseMediaSection(pstSection, pstCtx);

	printf("%s() unknown section[%s].\r\n", __FUNCTION__, pstSection->sectionName);
	return 0;
}

//Each DDS setting is required exactly once.
static int ParseDdsSection(INI_SECTION *pstSection, INIT_CONFIG_PARSE_CTX *pstCtx)
{
	char *pEnd = NULL;
	unsigned int i;
	unsigned char isNetworkInterfaceParsed = 0, isDomainIdParsed = 0;
	long long parsedDomainId;

	if (pstCtx->isDdsParsed)
	{
		printf("%s() duplicated [dds] section.\r\n", __FUNCTION__);
		return 0;
	}

	for (i = 0; i < pstSection->keyCnt; i++)
	{
		if (strcmp(pstSection->szstKey[i].name, "strNetworkInterface") == 0)
		{
			if (isNetworkInterfaceParsed)
			{
				printf("%s() duplicated [dds].strNetworkInterface.\r\n", __FUNCTION__);
				return 0;
			}

			if (pstSection->szstKey[i].value[0] == 0 || strlen(pstSection->szstKey[i].value) >= sizeof(pstCtx->pstCfg->stDds.strNetworkInterface))
			{
				printf("%s() [dds].strNetworkInterface invalid.\r\n", __FUNCTION__);
				return 0;
			}
			strcpy(pstCtx->pstCfg->stDds.strNetworkInterface, pstSection->szstKey[i].value);
			isNetworkInterfaceParsed = 1;
		}
		else if (strcmp(pstSection->szstKey[i].name, "domain_id") == 0)
		{
			if (isDomainIdParsed)
			{
				printf("%s() duplicated [dds].domain_id.\r\n", __FUNCTION__);
				return 0;
			}
			parsedDomainId = strtoll(pstSection->szstKey[i].value, &pEnd, 10);
			if (pEnd == pstSection->szstKey[i].value || *pEnd != 0 || parsedDomainId < 0 || parsedDomainId > UINT_MAX)
			{
				printf("%s() [dds].domain_id invalid: %s\r\n", __FUNCTION__, pstSection->szstKey[i].value);
				return 0;
			}
			pstCtx->pstCfg->stDds.domain_id = (unsigned int)parsedDomainId;
			isDomainIdParsed = 1;
		}
		else
		{
			printf("%s() unknown key[%s] in [dds].\r\n", __FUNCTION__, pstSection->szstKey[i].name);
			return 0;
		}
	}

	if (!isNetworkInterfaceParsed || !isDomainIdParsed)
	{
		printf("%s() [dds] requires strNetworkInterface and domain_id.\r\n", __FUNCTION__);
		return 0;
	}
	pstCtx->isDdsParsed = 1;
	return 1;
}

static int ParseHeadSection(INI_SECTION *pstSection, INIT_CONFIG_PARSE_CTX *pstCtx)
{
	unsigned int i;
	unsigned char isSourceNameParsed = 0;

	if (pstCtx->isHeadParsed)
	{
		printf("%s() duplicated [head] section.\r\n", __FUNCTION__);
		return 0;
	}

	for (i = 0; i < pstSection->keyCnt; i++)
	{
		if (strcmp(pstSection->szstKey[i].name, "sourceName") == 0)
		{
			if (isSourceNameParsed)
			{
				printf("%s() duplicated [head].sourceName.\r\n", __FUNCTION__);
				return 0;
			}

			if (pstSection->szstKey[i].value[0] == 0 || strlen(pstSection->szstKey[i].value) >= sizeof(pstCtx->pstCfg->stHead.sourceName))
			{
				printf("%s() [head].sourceName invalid.\r\n", __FUNCTION__);
				return 0;
			}
			strcpy(pstCtx->pstCfg->stHead.sourceName, pstSection->szstKey[i].value);
			isSourceNameParsed = 1;
		}
		else
		{
			printf("%s() unknown key[%s] in [head].\r\n", __FUNCTION__, pstSection->szstKey[i].name);
			return 0;
		}
	}

	if (!isSourceNameParsed)
	{
		printf("%s() [head] requires sourceName.\r\n", __FUNCTION__);
		return 0;
	}
	pstCtx->isHeadParsed = 1;
	return 1;
}

static int ParseMediaSection(INI_SECTION *pstSection, INIT_CONFIG_PARSE_CTX *pstCtx)
{
	static const char *const szKeyNames[] =
	{
		"isVideoCaptureOn", "isMicCaptureOn", "isSdkPlayAudioOn", "multicastAddress", "strRobotIPv4",
		"videoPort", "micPort", "spkPort", "videoPayloadType", "micPayloadType", "spkPayloadType", "jitterBufferLatencyMax_ms", "rtpMtu", "videoFrameSizeMax"
	};
	unsigned char isParsed[sizeof(szKeyNames) / sizeof(szKeyNames[0])] = {0};
	struct in_addr stAddress;
	char *pEnd = NULL;
	unsigned int i, keyIndex;
	long long value;

	if (pstCtx->isMediaParsed)
	{
		printf("%s() duplicated [media] section.\r\n", __FUNCTION__);
		return 0;
	}

	for (i = 0; i < pstSection->keyCnt; i++)
	{
		for (keyIndex = 0; keyIndex < sizeof(szKeyNames) / sizeof(szKeyNames[0]); keyIndex++)
		{
			if (strcmp(pstSection->szstKey[i].name, szKeyNames[keyIndex]) == 0)
				break;
		}

		if (keyIndex == sizeof(szKeyNames) / sizeof(szKeyNames[0]))
		{
			printf("%s() unknown key[%s] in [media].\r\n", __FUNCTION__, pstSection->szstKey[i].name);
			return 0;
		}

		if (isParsed[keyIndex])
		{
			printf("%s() duplicated [media].%s.\r\n", __FUNCTION__, szKeyNames[keyIndex]);
			return 0;
		}

		if (keyIndex == 3 || keyIndex == 4)
		{
			if (strlen(pstSection->szstKey[i].value) >= AHEADFORM_IPV4_ADDR_LEN_MAX || inet_pton(AF_INET, pstSection->szstKey[i].value, &stAddress) != 1)
			{
				printf("%s() [media].%s invalid IPv4: %s\r\n", __FUNCTION__, szKeyNames[keyIndex], pstSection->szstKey[i].value);
				return 0;
			}

			if (keyIndex == 3)
			{
				if (!IN_MULTICAST(ntohl(stAddress.s_addr)))
				{
					printf("%s() [media].multicastAddress is not multicast: %s\r\n", __FUNCTION__, pstSection->szstKey[i].value);
					return 0;
				}
				strcpy(pstCtx->pstCfg->stMedia.multicastAddress, pstSection->szstKey[i].value);
			}
			else
			{
				if (IN_MULTICAST(ntohl(stAddress.s_addr)) || stAddress.s_addr == INADDR_BROADCAST)
				{
					printf("%s() [media].strRobotIPv4 is not a unicast target: %s\r\n", __FUNCTION__, pstSection->szstKey[i].value);
					return 0;
				}
				strcpy(pstCtx->pstCfg->stMedia.strRobotIPv4, pstSection->szstKey[i].value);
			}
			isParsed[keyIndex] = 1;
			continue;
		}
		value = strtoll(pstSection->szstKey[i].value, &pEnd, 10);
		if (pEnd == pstSection->szstKey[i].value || *pEnd != 0 || value < 0 || value > UINT_MAX)
		{
			printf("%s() [media].%s invalid: %s\r\n", __FUNCTION__, szKeyNames[keyIndex], pstSection->szstKey[i].value);
			return 0;
		}

		if ((keyIndex < 3 && value > 1) || (keyIndex >= 5 && keyIndex <= 7 && (value == 0 || value > 65535)) || (keyIndex >= 8 && keyIndex <= 10 && value > 127) || (keyIndex == 12 && (value < 652 || value > 65507)))
		{
			printf("%s() [media].%s out of range: %s\r\n", __FUNCTION__, szKeyNames[keyIndex], pstSection->szstKey[i].value);
			return 0;
		}

		switch (keyIndex)
		{
			case 0:
				pstCtx->pstCfg->stMedia.isVideoCaptureOn = (unsigned char)value;
				break;

			case 1:
				pstCtx->pstCfg->stMedia.isMicCaptureOn = (unsigned char)value;
				break;

			case 2:
				pstCtx->pstCfg->stMedia.isSdkPlayAudioOn = (unsigned char)value;
				break;

			case 5:
				pstCtx->pstCfg->stMedia.videoPort = (unsigned int)value;
				break;

			case 6:
				pstCtx->pstCfg->stMedia.micPort = (unsigned int)value;
				break;

			case 7:
				pstCtx->pstCfg->stMedia.spkPort = (unsigned int)value;
				break;

			case 8:
				pstCtx->pstCfg->stMedia.videoPayloadType = (unsigned int)value;
				break;

			case 9:
				pstCtx->pstCfg->stMedia.micPayloadType = (unsigned int)value;
				break;

			case 10:
				pstCtx->pstCfg->stMedia.spkPayloadType = (unsigned int)value;
				break;

			case 11:
				pstCtx->pstCfg->stMedia.jitterBufferLatencyMax_ms = (unsigned int)value;
				break;

			case 12:
				pstCtx->pstCfg->stMedia.rtpMtu = (unsigned int)value;
				break;

			case 13:
				pstCtx->pstCfg->stMedia.videoFrameSizeMax = (unsigned int)value;
				break;
		}
		isParsed[keyIndex] = 1;
	}

	for (i = 0; i < sizeof(szKeyNames) / sizeof(szKeyNames[0]); i++)
	{
		if (!isParsed[i])
		{
			printf("%s() missing required [media].%s.\r\n", __FUNCTION__, szKeyNames[i]);
			return 0;
		}
	}

	if (pstCtx->pstCfg->stMedia.videoPort == pstCtx->pstCfg->stMedia.micPort)
	{
		printf("%s() [media].videoPort and micPort must differ.\r\n", __FUNCTION__);
		return 0;
	}

	if (pstCtx->pstCfg->stMedia.isVideoCaptureOn && pstCtx->pstCfg->stMedia.videoFrameSizeMax == 0)
	{
		printf("%s() enabled video capture requires videoFrameSizeMax > 0.\r\n", __FUNCTION__);
		return 0;
	}

	if (pstCtx->pstCfg->stMedia.isSdkPlayAudioOn && strcmp(pstCtx->pstCfg->stMedia.strRobotIPv4, "0.0.0.0") == 0)
	{
		printf("%s() enabled SDK audio playback requires the robot IPv4.\r\n", __FUNCTION__);
		return 0;
	}
	pstCtx->isMediaParsed = 1;
	return 1;
}

static int ValidateAheadFormInitConfig(const AHEADFORM_INIT_CONFIG *pstCfg, const INIT_CONFIG_PARSE_CTX *pstCtx)
{
	if (!pstCtx->isDdsParsed)
	{
		printf("%s() missing required [dds] configuration.\r\n", __FUNCTION__);
		return 0;
	}

	if (!pstCtx->isHeadParsed)
	{
		printf("%s() missing required [head] configuration.\r\n", __FUNCTION__);
		return 0;
	}

	if (!pstCtx->isMediaParsed)
	{
		printf("%s() missing required [media] configuration.\r\n", __FUNCTION__);
		return 0;
	}

	if (!IsValidHeadSourceName(pstCfg->stHead.sourceName))
	{
		printf("%s() [head].sourceName must match ^[a-z][a-z0-9_]{0,31}$.\r\n", __FUNCTION__);
		return 0;
	}

	return 1;
}
