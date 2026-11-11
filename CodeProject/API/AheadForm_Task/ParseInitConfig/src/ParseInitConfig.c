#include "ParseInitConfig.h"
#include "ini.h"

typedef struct
{
	AHEADFORM_INIT_CONFIG *pstCfg;
	unsigned char isDdsParsed;
	unsigned char isHeadParsed;
	unsigned char isStatusParsed;
	unsigned char isMediaParsed;
} INIT_CONFIG_PARSE_CTX;

static int CallBack_Parse1iniSection(INI_SECTION *pstSection, void *pParseCtx);
static int ParseDdsSection(INI_SECTION *pstSection, INIT_CONFIG_PARSE_CTX *pstCtx);
static int ParseHeadSection(INI_SECTION *pstSection, INIT_CONFIG_PARSE_CTX *pstCtx);
static int ParseStatusSection(INI_SECTION *pstSection, INIT_CONFIG_PARSE_CTX *pstCtx);
static int ParseMediaSection(INI_SECTION *pstSection, INIT_CONFIG_PARSE_CTX *pstCtx);


int ParseAheadFormInitConfig(const char *iniConfigFilePath, AHEADFORM_INIT_CONFIG *pstRetCfg)
{
	INIT_CONFIG_PARSE_CTX stParseCtx = {0};
	int ret;

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
		printf("%s() call ini_parse(%s) failed at line %d.\r\n", __FUNCTION__, iniConfigFilePath, ret);
		return ERR_CONFIG_PARSE_FAILED;
	}

	if (!stParseCtx.isDdsParsed || !stParseCtx.isHeadParsed || !stParseCtx.isStatusParsed || !stParseCtx.isMediaParsed)
	{
		printf("%s() requires [dds], [head], [status] and [media].\r\n", __FUNCTION__);
		return ERR_CONFIG_VALUE_INVALID;
	}

	return RET_SUCCESS;
}

static int CallBack_Parse1iniSection(INI_SECTION *pstSection, void *pParseCtx)
{
	INIT_CONFIG_PARSE_CTX *pstCtx = (INIT_CONFIG_PARSE_CTX*)pParseCtx;

	if (pstSection->sectionName == NULL)
		return 0;

	if (strcmp(pstSection->sectionName, "dds") == 0)
		return ParseDdsSection(pstSection, pstCtx);
	else if (strcmp(pstSection->sectionName, "head") == 0)
		return ParseHeadSection(pstSection, pstCtx);
	else if (strcmp(pstSection->sectionName, "status") == 0)
		return ParseStatusSection(pstSection, pstCtx);
	else if (strcmp(pstSection->sectionName, "media") == 0)
		return ParseMediaSection(pstSection, pstCtx);

	printf("%s() unknown section[%s].\r\n", __FUNCTION__, pstSection->sectionName);
	return 0;
}

static int ParseDdsSection(INI_SECTION *pstSection, INIT_CONFIG_PARSE_CTX *pstCtx)
{
	char *pEnd = NULL;
	long long parsedValue;
	unsigned int i;
	unsigned char isStrNetworkInterfaceParsed = 0;
	unsigned char isDomain_idParsed = 0;

	if (pstCtx->isDdsParsed)
	{
		printf("%s() duplicated [dds] section.\r\n", __FUNCTION__);
		return 0;
	}

	for (i = 0; i < pstSection->keyCnt; i++)
	{
		if (strcmp(pstSection->szstKey[i].name, "strNetworkInterface") == 0)
		{
			if (isStrNetworkInterfaceParsed)
			{
				printf("%s() duplicated [dds].strNetworkInterface.\r\n", __FUNCTION__);
				return 0;
			}

			if (pstSection->szstKey[i].value[0] == 0 || strlen(pstSection->szstKey[i].value) >= sizeof(pstCtx->pstCfg->stDds.strNetworkInterface))
			{
				printf("%s() [dds].strNetworkInterface invalid: %s\r\n", __FUNCTION__, pstSection->szstKey[i].value);
				return 0;
			}
			strcpy(pstCtx->pstCfg->stDds.strNetworkInterface, pstSection->szstKey[i].value);
			isStrNetworkInterfaceParsed = 1;
		}
		else if (strcmp(pstSection->szstKey[i].name, "domain_id") == 0)
		{
			if (isDomain_idParsed)
			{
				printf("%s() duplicated [dds].domain_id.\r\n", __FUNCTION__);
				return 0;
			}
			parsedValue = strtoll(pstSection->szstKey[i].value, &pEnd, 10);
			if (pstSection->szstKey[i].value[0] < '0' || pstSection->szstKey[i].value[0] > '9' || *pEnd != 0 || parsedValue < 0 || parsedValue > 4294967295)
			{
				printf("%s() [dds].domain_id invalid: %s\r\n", __FUNCTION__, pstSection->szstKey[i].value);
				return 0;
			}
			pstCtx->pstCfg->stDds.domain_id = (unsigned int)parsedValue;
			isDomain_idParsed = 1;
		}
		else
		{
			printf("%s() unknown key[%s] in [dds].\r\n", __FUNCTION__, pstSection->szstKey[i].name);
			return 0;
		}
	}

	if (!isStrNetworkInterfaceParsed || !isDomain_idParsed)
	{
		printf("%s() [dds] missing required key.\r\n", __FUNCTION__);
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
				printf("%s() [head].sourceName invalid: %s\r\n", __FUNCTION__, pstSection->szstKey[i].value);
				return 0;
			}

			if (pstSection->szstKey[i].value[0] < 'a' || pstSection->szstKey[i].value[0] > 'z' || strspn(pstSection->szstKey[i].value, "abcdefghijklmnopqrstuvwxyz0123456789_") != strlen(pstSection->szstKey[i].value))
				return 0;

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
		printf("%s() [head] missing required key.\r\n", __FUNCTION__);
		return 0;
	}
	pstCtx->isHeadParsed = 1;
	return 1;
}

static int ParseStatusSection(INI_SECTION *pstSection, INIT_CONFIG_PARSE_CTX *pstCtx)
{
	char *pEnd = NULL;
	long long parsedValue;
	unsigned int i;
	unsigned char isFaultRecordCntMaxParsed = 0;

	if (pstCtx->isStatusParsed)
	{
		printf("%s() duplicated [status] section.\r\n", __FUNCTION__);
		return 0;
	}

	for (i = 0; i < pstSection->keyCnt; i++)
	{
		if (strcmp(pstSection->szstKey[i].name, "faultRecordCntMax") == 0)
		{
			if (isFaultRecordCntMaxParsed)
			{
				printf("%s() duplicated [status].faultRecordCntMax.\r\n", __FUNCTION__);
				return 0;
			}
			parsedValue = strtoll(pstSection->szstKey[i].value, &pEnd, 10);
			if (pstSection->szstKey[i].value[0] < '0' || pstSection->szstKey[i].value[0] > '9' || *pEnd != 0 || parsedValue < 1 || parsedValue > 4294967295)
			{
				printf("%s() [status].faultRecordCntMax invalid: %s\r\n", __FUNCTION__, pstSection->szstKey[i].value);
				return 0;
			}
			pstCtx->pstCfg->stStatus.faultRecordCntMax = (unsigned int)parsedValue;
			isFaultRecordCntMaxParsed = 1;
		}
		else
		{
			printf("%s() unknown key[%s] in [status].\r\n", __FUNCTION__, pstSection->szstKey[i].name);
			return 0;
		}
	}

	if (!isFaultRecordCntMaxParsed)
	{
		printf("%s() [status] missing required key.\r\n", __FUNCTION__);
		return 0;
	}
	pstCtx->isStatusParsed = 1;
	return 1;
}

static int ParseMediaSection(INI_SECTION *pstSection, INIT_CONFIG_PARSE_CTX *pstCtx)
{
	char *pEnd = NULL;
	long long parsedValue;
	struct in_addr stAddress;
	unsigned int i;
	unsigned char isStrMulticastAddressParsed = 0;
	unsigned char isStrRobotIPv4Parsed = 0;
	unsigned char isVideoPortParsed = 0;
	unsigned char isMicPortParsed = 0;
	unsigned char isSpkPortParsed = 0;
	unsigned char isVideoPayloadTypeParsed = 0;
	unsigned char isMicPayloadTypeParsed = 0;
	unsigned char isSpkPayloadTypeParsed = 0;
	unsigned char isJitterLatency_msParsed = 0;
	unsigned char isRtpMtuParsed = 0;

	if (pstCtx->isMediaParsed)
	{
		printf("%s() duplicated [media] section.\r\n", __FUNCTION__);
		return 0;
	}

	for (i = 0; i < pstSection->keyCnt; i++)
	{
		if (strcmp(pstSection->szstKey[i].name, "strMulticastAddress") == 0)
		{
			if (isStrMulticastAddressParsed)
			{
				printf("%s() duplicated [media].strMulticastAddress.\r\n", __FUNCTION__);
				return 0;
			}

			if (pstSection->szstKey[i].value[0] == 0 || strlen(pstSection->szstKey[i].value) >= sizeof(pstCtx->pstCfg->stMedia.strMulticastAddress) || inet_pton(AF_INET, pstSection->szstKey[i].value, &stAddress) != 1 || !IN_MULTICAST(ntohl(stAddress.s_addr)))
			{
				printf("%s() [media].strMulticastAddress invalid: %s\r\n", __FUNCTION__, pstSection->szstKey[i].value);
				return 0;
			}
			strcpy(pstCtx->pstCfg->stMedia.strMulticastAddress, pstSection->szstKey[i].value);
			isStrMulticastAddressParsed = 1;
		}
		else if (strcmp(pstSection->szstKey[i].name, "strRobotIPv4") == 0)
		{
			if (isStrRobotIPv4Parsed)
			{
				printf("%s() duplicated [media].strRobotIPv4.\r\n", __FUNCTION__);
				return 0;
			}

			if (pstSection->szstKey[i].value[0] == 0 || strlen(pstSection->szstKey[i].value) >= sizeof(pstCtx->pstCfg->stMedia.strRobotIPv4) ||
				inet_pton(AF_INET, pstSection->szstKey[i].value, &stAddress) != 1 || stAddress.s_addr == INADDR_ANY || IN_MULTICAST(ntohl(stAddress.s_addr)) || stAddress.s_addr == INADDR_BROADCAST)
			{
				printf("%s() [media].strRobotIPv4 invalid: %s\r\n", __FUNCTION__, pstSection->szstKey[i].value);
				return 0;
			}
			strcpy(pstCtx->pstCfg->stMedia.strRobotIPv4, pstSection->szstKey[i].value);
			isStrRobotIPv4Parsed = 1;
		}
		else if (strcmp(pstSection->szstKey[i].name, "videoPort") == 0)
		{
			if (isVideoPortParsed)
			{
				printf("%s() duplicated [media].videoPort.\r\n", __FUNCTION__);
				return 0;
			}
			parsedValue = strtoll(pstSection->szstKey[i].value, &pEnd, 10);
			if (pstSection->szstKey[i].value[0] < '0' || pstSection->szstKey[i].value[0] > '9' || *pEnd != 0 || parsedValue < 1 || parsedValue > 65535)
			{
				printf("%s() [media].videoPort invalid: %s\r\n", __FUNCTION__, pstSection->szstKey[i].value);
				return 0;
			}
			pstCtx->pstCfg->stMedia.videoPort = (unsigned int)parsedValue;
			isVideoPortParsed = 1;
		}
		else if (strcmp(pstSection->szstKey[i].name, "micPort") == 0)
		{
			if (isMicPortParsed)
			{
				printf("%s() duplicated [media].micPort.\r\n", __FUNCTION__);
				return 0;
			}
			parsedValue = strtoll(pstSection->szstKey[i].value, &pEnd, 10);
			if (pstSection->szstKey[i].value[0] < '0' || pstSection->szstKey[i].value[0] > '9' || *pEnd != 0 || parsedValue < 1 || parsedValue > 65535)
			{
				printf("%s() [media].micPort invalid: %s\r\n", __FUNCTION__, pstSection->szstKey[i].value);
				return 0;
			}
			pstCtx->pstCfg->stMedia.micPort = (unsigned int)parsedValue;
			isMicPortParsed = 1;
		}
		else if (strcmp(pstSection->szstKey[i].name, "spkPort") == 0)
		{
			if (isSpkPortParsed)
			{
				printf("%s() duplicated [media].spkPort.\r\n", __FUNCTION__);
				return 0;
			}
			parsedValue = strtoll(pstSection->szstKey[i].value, &pEnd, 10);
			if (pstSection->szstKey[i].value[0] < '0' || pstSection->szstKey[i].value[0] > '9' || *pEnd != 0 || parsedValue < 1 || parsedValue > 65535)
			{
				printf("%s() [media].spkPort invalid: %s\r\n", __FUNCTION__, pstSection->szstKey[i].value);
				return 0;
			}
			pstCtx->pstCfg->stMedia.spkPort = (unsigned int)parsedValue;
			isSpkPortParsed = 1;
		}
		else if (strcmp(pstSection->szstKey[i].name, "videoPayloadType") == 0)
		{
			if (isVideoPayloadTypeParsed)
			{
				printf("%s() duplicated [media].videoPayloadType.\r\n", __FUNCTION__);
				return 0;
			}
			parsedValue = strtoll(pstSection->szstKey[i].value, &pEnd, 10);
			if (pstSection->szstKey[i].value[0] < '0' || pstSection->szstKey[i].value[0] > '9' || *pEnd != 0 || parsedValue < 0 || parsedValue > 127)
			{
				printf("%s() [media].videoPayloadType invalid: %s\r\n", __FUNCTION__, pstSection->szstKey[i].value);
				return 0;
			}
			pstCtx->pstCfg->stMedia.videoPayloadType = (unsigned int)parsedValue;
			isVideoPayloadTypeParsed = 1;
		}
		else if (strcmp(pstSection->szstKey[i].name, "micPayloadType") == 0)
		{
			if (isMicPayloadTypeParsed)
			{
				printf("%s() duplicated [media].micPayloadType.\r\n", __FUNCTION__);
				return 0;
			}
			parsedValue = strtoll(pstSection->szstKey[i].value, &pEnd, 10);
			if (pstSection->szstKey[i].value[0] < '0' || pstSection->szstKey[i].value[0] > '9' || *pEnd != 0 || parsedValue < 0 || parsedValue > 127)
			{
				printf("%s() [media].micPayloadType invalid: %s\r\n", __FUNCTION__, pstSection->szstKey[i].value);
				return 0;
			}
			pstCtx->pstCfg->stMedia.micPayloadType = (unsigned int)parsedValue;
			isMicPayloadTypeParsed = 1;
		}
		else if (strcmp(pstSection->szstKey[i].name, "spkPayloadType") == 0)
		{
			if (isSpkPayloadTypeParsed)
			{
				printf("%s() duplicated [media].spkPayloadType.\r\n", __FUNCTION__);
				return 0;
			}
			parsedValue = strtoll(pstSection->szstKey[i].value, &pEnd, 10);
			if (pstSection->szstKey[i].value[0] < '0' || pstSection->szstKey[i].value[0] > '9' || *pEnd != 0 || parsedValue < 0 || parsedValue > 127)
			{
				printf("%s() [media].spkPayloadType invalid: %s\r\n", __FUNCTION__, pstSection->szstKey[i].value);
				return 0;
			}
			pstCtx->pstCfg->stMedia.spkPayloadType = (unsigned int)parsedValue;
			isSpkPayloadTypeParsed = 1;
		}
		else if (strcmp(pstSection->szstKey[i].name, "jitterLatency_ms") == 0)
		{
			if (isJitterLatency_msParsed)
			{
				printf("%s() duplicated [media].jitterLatency_ms.\r\n", __FUNCTION__);
				return 0;
			}
			parsedValue = strtoll(pstSection->szstKey[i].value, &pEnd, 10);
			if (pstSection->szstKey[i].value[0] < '0' || pstSection->szstKey[i].value[0] > '9' || *pEnd != 0 || parsedValue < 0 || parsedValue > 60000)
			{
				printf("%s() [media].jitterLatency_ms invalid: %s\r\n", __FUNCTION__, pstSection->szstKey[i].value);
				return 0;
			}
			pstCtx->pstCfg->stMedia.jitterLatency_ms = (unsigned int)parsedValue;
			isJitterLatency_msParsed = 1;
		}
		else if (strcmp(pstSection->szstKey[i].name, "rtpMtu") == 0)
		{
			if (isRtpMtuParsed)
			{
				printf("%s() duplicated [media].rtpMtu.\r\n", __FUNCTION__);
				return 0;
			}
			parsedValue = strtoll(pstSection->szstKey[i].value, &pEnd, 10);
			if (pstSection->szstKey[i].value[0] < '0' || pstSection->szstKey[i].value[0] > '9' || *pEnd != 0 || parsedValue < 64 || parsedValue > 65507)
			{
				printf("%s() [media].rtpMtu invalid: %s\r\n", __FUNCTION__, pstSection->szstKey[i].value);
				return 0;
			}
			pstCtx->pstCfg->stMedia.rtpMtu = (unsigned int)parsedValue;
			isRtpMtuParsed = 1;
		}
		else
		{
			printf("%s() unknown key[%s] in [media].\r\n", __FUNCTION__, pstSection->szstKey[i].name);
			return 0;
		}
	}

	if (!isStrMulticastAddressParsed || !isStrRobotIPv4Parsed || !isVideoPortParsed || !isMicPortParsed || !isSpkPortParsed || !isVideoPayloadTypeParsed || !isMicPayloadTypeParsed || !isSpkPayloadTypeParsed || !isJitterLatency_msParsed || !isRtpMtuParsed)
	{
		printf("%s() [media] missing required key.\r\n", __FUNCTION__);
		return 0;
	}
	pstCtx->isMediaParsed = 1;
	return 1;
}
