#ifndef __AHEADFORM_DDS_CTRL_H__
#define __AHEADFORM_DDS_CTRL_H__

#include "PublicDef.h"

#include <dds/dds.h>

#define AHEADFORM_NETWORK_INTERFACE_LEN_MAX 64

typedef struct
{
	char strNetworkInterface[AHEADFORM_NETWORK_INTERFACE_LEN_MAX];
	unsigned int domain_id;
	dds_entity_t ddsDomain;
	dds_entity_t ddsParticipant;
} AHEADFORM_DDS_CTRL;

int DdsCtrl_Init(AHEADFORM_DDS_CTRL *pstDdsCtrl, const char *strNetworkInterface, unsigned int domain_id);
int DdsCtrl_Deinit(AHEADFORM_DDS_CTRL *pstDdsCtrl);

#endif
