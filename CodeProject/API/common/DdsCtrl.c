#include "DdsCtrl.h"

static int DdsCtrl_AppendXmlText(char *strXml, size_t xmlCapacity, size_t *pXmlLen, const char *strText)
{
	size_t textLen = strlen(strText);

	if (*pXmlLen + textLen >= xmlCapacity)
		return 0;

	memcpy(strXml + *pXmlLen, strText, textLen);
	*pXmlLen += textLen;
	strXml[*pXmlLen] = 0;
	return 1;
}

static int DdsCtrl_AppendEscapedInterfaceToXml(char *strXml, size_t xmlCapacity, size_t *pXmlLen, const char *strNetworkInterface)
{
	const char *strReplacement = NULL;
	char strChar[2] = {0};
	size_t i;

	for (i = 0; strNetworkInterface[i] != 0; i++)
	{
		switch (strNetworkInterface[i])
		{
			case '&':
				strReplacement = "&amp;";
				break;

			case '<':
				strReplacement = "&lt;";
				break;

			case '>':
				strReplacement = "&gt;";
				break;

			case '\"':
				strReplacement = "&quot;";
				break;

			case '\'':
				strReplacement = "&apos;";
				break;

			default:
				strChar[0] = strNetworkInterface[i];
				strReplacement = strChar;
				break;
		}

		if (!DdsCtrl_AppendXmlText(strXml, xmlCapacity, pXmlLen, strReplacement))
			return 0;
	}

	return 1;
}

int DdsCtrl_Init(AHEADFORM_DDS_CTRL *pstDdsCtrl, const char *strNetworkInterface, unsigned int domain_id)
{
	char strCycloneConfig[512] = {0};
	size_t xmlLen = 0;
	dds_entity_t ddsEntity;

	if (pstDdsCtrl == NULL)
		return ERR_ARGV_1;

	if (strNetworkInterface == NULL)
		return ERR_ARGV_2;

	if (strNetworkInterface[0] == 0)
		return ERR_NETWORK_INTERFACE_INVALID;

	if (strlen(strNetworkInterface) >= AHEADFORM_NETWORK_INTERFACE_LEN_MAX)
		return ERR_NETWORK_INTERFACE_INVALID;

	memset(pstDdsCtrl, 0, sizeof(*pstDdsCtrl));
	if (!DdsCtrl_AppendXmlText(strCycloneConfig, sizeof(strCycloneConfig), &xmlLen, "<CycloneDDS><Domain><General><Interfaces><NetworkInterface name=\""))
		return ERR_OVER_BUF;

	if (!DdsCtrl_AppendEscapedInterfaceToXml(strCycloneConfig, sizeof(strCycloneConfig), &xmlLen, strNetworkInterface))
		return ERR_OVER_BUF;

	if (!DdsCtrl_AppendXmlText(strCycloneConfig, sizeof(strCycloneConfig), &xmlLen, "\"/></Interfaces></General></Domain></CycloneDDS>"))
		return ERR_OVER_BUF;

	if ( (ddsEntity = dds_create_domain(domain_id, strCycloneConfig)) < 0 )
		return ERR_DDS_INIT_FAILED;

	pstDdsCtrl->ddsDomain = ddsEntity;
	if ( (ddsEntity = dds_create_participant(domain_id, NULL, NULL)) < 0 )
		return ERR_DDS_INIT_FAILED;

	snprintf(pstDdsCtrl->strNetworkInterface, sizeof(pstDdsCtrl->strNetworkInterface), "%s", strNetworkInterface);
	pstDdsCtrl->domain_id = domain_id;
	pstDdsCtrl->ddsParticipant = ddsEntity;
	return RET_SUCCESS;
}

int DdsCtrl_Deinit(AHEADFORM_DDS_CTRL *pstDdsCtrl)
{
	if (pstDdsCtrl == NULL)
		return ERR_ARGV_1;

	if (pstDdsCtrl->ddsParticipant > 0)
	{
		if (dds_delete(pstDdsCtrl->ddsParticipant) < 0)
			return ERR_DDS_ENTITY_DELETE_FAILED;

		pstDdsCtrl->ddsParticipant = 0;
	}

	if (pstDdsCtrl->ddsDomain > 0)
	{
		if (dds_delete(pstDdsCtrl->ddsDomain) < 0)
			return ERR_DDS_ENTITY_DELETE_FAILED;

		pstDdsCtrl->ddsDomain = 0;
	}
	memset(pstDdsCtrl, 0, sizeof(*pstDdsCtrl));
	return RET_SUCCESS;
}
