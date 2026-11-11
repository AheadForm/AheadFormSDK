#include "Error.h"
#include <string.h>


typedef struct
{
    int errCode;
    const char *strDescription;
} ERR_NAME_MAP;

static ERR_STACK_CTX gstErrStackCtx = {.errPrintMutex = PTHREAD_MUTEX_INITIALIZER};

static const ERR_NAME_MAP gErrNameMap[ERR_GROUP_CNT][GROUP_ITEMCNT_MAX] =
{
	//##ERR_GROUP_0_GENERAL............................................................................................
	{
		{ERRCODE_ARGC, "ERR_ARGC"},
        {ERRCODE_ARGV_1, "ERR_ARGV_1"},
        {ERRCODE_ARGV_2, "ERR_ARGV_2"},
        {ERRCODE_ARGV_3, "ERR_ARGV_3"},
        {ERRCODE_ARGV_4, "ERR_ARGV_4"},
        {ERRCODE_ARGV_5, "ERR_ARGV_5"},
        {ERRCODE_ARGV, "ERR_ARGV"},
        {ERRCODE_TIMEOUT, "ERR_TIMEOUT"},
        {ERRCODE_FILE_NOT_FOUND, "ERR_FILE_NOT_FOUND"},
        {ERRCODE_FILE_OP_FAILED, "ERR_FILE_OP_FAILED"},
        {ERRCODE_FILE_FORMAT, "ERR_FILE_FORMAT"},
        {ERRCODE_MEMORY_ALLOC, "ERR_MEMORY_ALLOC"},
        {ERRCODE_OVER_BUF, "ERR_OVER_BUF"},
        {ERRCODE_PTHREAD_INIT, "ERR_PTHREAD_INIT"},
        {ERRCODE_PTHREAD_CFG, "ERR_PTHREAD_CFG"},
        {ERRCODE_MUTEX_INIT, "ERR_MUTEX_INIT"},
        {ERRCODE_CONFIG_PARSE_FAILED, "ERR_CONFIG_PARSE_FAILED"},
        {ERRCODE_CONFIG_VALUE_INVALID, "ERR_CONFIG_VALUE_INVALID"},
        {ERRCODE_UART_CONFIG_FAILED, "ERR_UART_CONFIG_FAILED"},
        {ERRCODE_SELECT, "ERR_SELECT"},
        {ERRCODE_EVENT, "ERR_EVENT"},
        {ERRCODE_INIT_FAIL, "ERR_INIT_FAIL"},
        {ERRCODE_CLOCK, "ERR_CLOCK"},
        {ERRCODE_EPOLL, "ERR_EPOLL"},
        {ERRCODE_UDEV_OP_FAILED, "ERR_UDEV_OP_FAILED"},
        {ERRCODE_ID_NOT_FOUND, "ERR_ID_NOT_FOUND"},
        {ERRCODE_RUNNABLE, "ERR_RUNNABLE"},
        {ERRCODE_SHUT_DOWN, "ERR_SHUT_DOWN"}
	},
	//##ERR_GROUP_1_IO_IPC.............................................................................................
	{
        {ERRCODE_SOCKET_IO, "ERR_SOCKET_IO"},
        {ERRCODE_SOCKET_OP_FAILED, "ERR_SOCKET_OP_FAILED"},
        {ERRCODE_RX_FAILED, "ERR_RX_FAILED"},
        {ERRCODE_NO_FRESH_FEEDBACK, "ERR_NO_FRESH_FEEDBACK"},
        {ERRCODE_EVENTFD_CREATE_FAILED, "ERR_EVENTFD_CREATE_FAILED"},
        {ERRCODE_EVENTFD_IO_FAILED, "ERR_EVENTFD_IO_FAILED"},
        {ERRCODE_SEM_OP_FAILED, "ERR_SEM_OP_FAILED"},
        {ERRCODE_COMM_RING_EMPTY, "ERR_COMM_RING_EMPTY"},
        {ERRCODE_COMM_RING_FULL, "ERR_COMM_RING_FULL"},
        {ERRCODE_COMM_RING_ELEM_WR_BUSY, "ERR_COMM_RING_ELEM_WR_BUSY"},
        {ERRCODE_COMM_RING_ELEM_MISSED, "ERR_COMM_RING_ELEM_MISSED"},
        {ERRCODE_COMM_RING_OP_FAIL, "ERR_COMM_RING_OP_FAIL"},
        {ERRCODE_NETWORK_INTERFACE_INVALID, "ERR_NETWORK_INTERFACE_INVALID"},
        {ERRCODE_MULTICAST_JOIN_FAILED, "ERR_MULTICAST_JOIN_FAILED"},
        {ERRCODE_DDS_INIT_FAILED, "ERR_DDS_INIT_FAILED"},
        {ERRCODE_DDS_TOPIC_CREATE_FAILED, "ERR_DDS_TOPIC_CREATE_FAILED"},
        {ERRCODE_DDS_ENDPOINT_CREATE_FAILED, "ERR_DDS_ENDPOINT_CREATE_FAILED"},
        {ERRCODE_DDS_DISCOVERY_TIMEOUT, "ERR_DDS_DISCOVERY_TIMEOUT"},
        {ERRCODE_DDS_PUBLISH_FAILED, "ERR_DDS_PUBLISH_FAILED"},
        {ERRCODE_DDS_TAKE_FAILED, "ERR_DDS_TAKE_FAILED"},
        {ERRCODE_DDS_QOS_INCOMPATIBLE, "ERR_DDS_QOS_INCOMPATIBLE"},
        {ERRCODE_DATA_PACKET_FORMAT, "ERR_DATA_PACKET_FORMAT"},
        {ERRCODE_DDS_ENTITY_DELETE_FAILED, "ERR_DDS_ENTITY_DELETE_FAILED"},
        {ERRCODE_MUTEX_OP_FAILED, "ERR_MUTEX_OP_FAILED"}
	},
	//##ERR_GROUP_2_USER_CUSTOM........................................................................................
	{
        {ERRCODE_SDK_NOT_READY, "ERR_SDK_NOT_READY"},
        {ERRCODE_STATE_TRANSITION_INVALID, "ERR_STATE_TRANSITION_INVALID"},
        {ERRCODE_FEATURE_UNAVAILABLE, "ERR_FEATURE_UNAVAILABLE"},
        {ERRCODE_ROBOT_OFFLINE, "ERR_ROBOT_OFFLINE"},
        {ERRCODE_RESOURCE_BUSY, "ERR_RESOURCE_BUSY"},
        {ERRCODE_REQUEST_TIMEOUT, "ERR_REQUEST_TIMEOUT"},
        {ERRCODE_REQUEST_REJECTED, "ERR_REQUEST_REJECTED"},
        {ERRCODE_REQUEST_CANCELLED, "ERR_REQUEST_CANCELLED"},
        {ERRCODE_TOPIC_NOT_FOUND, "ERR_TOPIC_NOT_FOUND"},
        {ERRCODE_TOPIC_DIRECTION_INVALID, "ERR_TOPIC_DIRECTION_INVALID"},
        {ERRCODE_SUBSCRIPTION_NOT_FOUND, "ERR_SUBSCRIPTION_NOT_FOUND"},
        {ERRCODE_TARGET_OUT_OF_RANGE, "ERR_TARGET_OUT_OF_RANGE"},
        {ERRCODE_TARGET_REJECTED_BY_STATE, "ERR_TARGET_REJECTED_BY_STATE"},
        {ERRCODE_MOTION_DATA_INVALID, "ERR_MOTION_DATA_INVALID"},
        {ERRCODE_MEDIA_FORMAT_UNSUPPORTED, "ERR_MEDIA_FORMAT_UNSUPPORTED"},
        {ERRCODE_RESOURCE_NOT_FOUND, "ERR_RESOURCE_NOT_FOUND"},
        {ERRCODE_TIMELINE_INVALID, "ERR_TIMELINE_INVALID"},
        {ERRCODE_TASK_NOT_FOUND, "ERR_TASK_NOT_FOUND"},
        {ERRCODE_DATA_INCOMPLETE, "ERR_DATA_INCOMPLETE"},
        {ERRCODE_TIMEBASE_UNAVAILABLE, "ERR_TIMEBASE_UNAVAILABLE"},
        {ERRCODE_AUDIO_DEVICE_NOT_FOUND, "ERR_AUDIO_DEVICE_NOT_FOUND"},
        {ERRCODE_MEDIA_CHANNEL_NOT_READY, "ERR_MEDIA_CHANNEL_NOT_READY"}
	}
};

static int BitFieldValToBitId(unsigned int bitField, unsigned int bitCnt);
static const ERR_NAME_MAP* GetErrNameMap(unsigned int group, unsigned int item);
static const char* GetSingleErrName(unsigned int groupField, unsigned int itemField);
static inline int IsValidErrItem(unsigned int group, unsigned int item);
static int CopyErrStack(STACK_ERRLOG *pStackCopy);

int SaveErrLog(const char *funcName, int errLine, ENUM_ERR_GROUP group, unsigned int item)
{
	unsigned int expected = 0, groupVal = (unsigned int)group;
	unsigned int top;
	int errCode;

	if (funcName == NULL)
		return ERRCODE_ARGV_1;

	if (errLine < 0)
		return ERRCODE_ARGV_2;

	if (groupVal >= BITCNT_GROUP)
		return ERRCODE_ARGV_3;

	if (!IsValidErrItem(groupVal, item))
		return ERRCODE_ARGV_4;

	errCode = ERRCODE(groupVal, item);
	if (!__atomic_compare_exchange_n(&gstErrStackCtx.isWriting, &expected, 1, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST))
	{
		__atomic_fetch_add(&gstErrStackCtx.droppedErrLogCntForBusy, 1, __ATOMIC_SEQ_CST);
		return errCode;
	}

	if ( (top = __atomic_load_n(&gstErrStackCtx.stack.top, __ATOMIC_SEQ_CST)) < ERRSTACK_ITEMCNT_MAX )
	{
		gstErrStackCtx.stack.szstErrLog[top].sign = 1;
		gstErrStackCtx.stack.szstErrLog[top].groupField = 1 << groupVal;
		gstErrStackCtx.stack.szstErrLog[top].itemField = 1 << item;
		gstErrStackCtx.stack.szstErrLog[top].funcName = funcName;
		gstErrStackCtx.stack.szstErrLog[top].errLine = errLine;
		__atomic_store_n(&gstErrStackCtx.stack.top, top + 1, __ATOMIC_SEQ_CST);
	}
	else
	{
		__atomic_fetch_add(&gstErrStackCtx.droppedErrLogCntForFull, 1, __ATOMIC_SEQ_CST);
	}
	__atomic_store_n(&gstErrStackCtx.isWriting, 0, __ATOMIC_SEQ_CST);
	return errCode;
}

unsigned int GetDroppedErrLogCntForBusy(void)
{
	return __atomic_load_n(&gstErrStackCtx.droppedErrLogCntForBusy, __ATOMIC_SEQ_CST);
}

unsigned int GetDroppedErrLogCntForFull(void)
{
	return __atomic_load_n(&gstErrStackCtx.droppedErrLogCntForFull, __ATOMIC_SEQ_CST);
}

void ClearDroppedErrLogCnt(void)
{
	__atomic_store_n(&gstErrStackCtx.droppedErrLogCntForBusy, 0, __ATOMIC_SEQ_CST);
	__atomic_store_n(&gstErrStackCtx.droppedErrLogCntForFull, 0, __ATOMIC_SEQ_CST);
}

int ClearErrStack(void)
{
	unsigned int expected = 0;

	if (!__atomic_compare_exchange_n(&gstErrStackCtx.isWriting, &expected, 1, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST))
	{
		printf("ClearErrStack busy, clear skipped\r\n");
		return RET_SKIPPED;
	}
	__atomic_store_n(&gstErrStackCtx.stack.top, 0, __ATOMIC_SEQ_CST);
	ClearDroppedErrLogCnt();
	__atomic_store_n(&gstErrStackCtx.isWriting, 0, __ATOMIC_SEQ_CST);
	return RET_SUCCESS;
}

int PrintFunctionErrInfo(int funcret, const char *funcName)
{
	int i, j;
	const char *errName = NULL;
	const ERR_NAME_MAP *pstMap = NULL;
	char strFuncName[192] = {0};
	STRUCT_ERRLIST stErrLog = {.funcName = funcName, .errcode = funcret};

	if (funcName)
		snprintf(strFuncName, sizeof(strFuncName), "%s() ", funcName);

	pthread_mutex_lock(&gstErrStackCtx.errPrintMutex);
	if (funcret == RET_SUCCESS)
	{
		printf("%ssucceed.\r\n", strFuncName);
		pthread_mutex_unlock(&gstErrStackCtx.errPrintMutex);
		return funcret;
	}
	else if (funcret == RET_SKIPPED)
	{
		printf("[Skipped]:%s skipped, ret=RET_SKIPPED.\r\n", strFuncName);
		pthread_mutex_unlock(&gstErrStackCtx.errPrintMutex);
		return funcret;
	}
	else if (funcret == RET_EXIT)
	{
		printf("[Exit]:%s exit, ret=RET_EXIT.\r\n", strFuncName);
		pthread_mutex_unlock(&gstErrStackCtx.errPrintMutex);
		return funcret;
	}

	if (stErrLog.sign == 0 || stErrLog.groupField == 0 || stErrLog.itemField == 0)
	{
		printf("[Error]:%s failed, ret=0x%08X.\r\n", strFuncName, funcret);
		pthread_mutex_unlock(&gstErrStackCtx.errPrintMutex);
		return funcret;
	}

	for (i = 0; i < BITCNT_GROUP; i++)
	{
		if (stErrLog.groupField == (1 << i))
		{
			printf("[Error]:%s failed, ErrCode(", strFuncName);
			for (j = 0; j < BITCNT_ITEM; j++)
			{
				if (stErrLog.itemField & (1 << j))
				{
                    pstMap = GetErrNameMap((unsigned int)i, (unsigned int)j);
					errName = (pstMap != NULL) ? pstMap->strDescription : "ERR_UNKNOWN";
					printf(" %s ", errName);
				}
			}
			printf(")!\r\n");
			break;
		}
		else if (stErrLog.groupField & (1 << i))
		{
			printf("multiple error groups detected; %u error logs are currently recorded, call PrintAllErrLog() to show all.\r\n", __atomic_load_n(&gstErrStackCtx.stack.top, __ATOMIC_SEQ_CST));
			break;
		}
	}
	pthread_mutex_unlock(&gstErrStackCtx.errPrintMutex);
	return funcret;
}

int PrintFunctionErrDbgInfo(int funcret, const char *funcName, const char *fmt, ...)
{
	int i, j, isERRCODE = 1;
	const char *errName = NULL;
	const ERR_NAME_MAP *pstMap = NULL;
	char strFuncName[192] = {0};
	STRUCT_ERRLIST stErrLog = {.funcName = funcName, .errcode = funcret};
	va_list argptr;

	if (funcName)
		snprintf(strFuncName, sizeof(strFuncName), "%s() ", funcName);

	pthread_mutex_lock(&gstErrStackCtx.errPrintMutex);
	if (funcret == RET_SUCCESS)
	{
		printf("%ssucceed.\r\n", strFuncName);
		isERRCODE = 0;
	}
	else if (funcret == RET_SKIPPED)
	{
		printf("[Skipped]:%s skipped, ret = RET_SKIPPED.\r\n", strFuncName);
		isERRCODE = 0;
	}
	else if (funcret == RET_EXIT)
	{
		printf("[Exit]:%s exit, ret=RET_EXIT.\r\n", strFuncName);
		isERRCODE = 0;
	}
	else if (stErrLog.sign == 0 || stErrLog.groupField == 0 || stErrLog.itemField == 0)
	{
		printf("[Error]:%s failed, ret=0x%08X.\r\n", strFuncName, funcret);
		isERRCODE = 0;
	}

	if (isERRCODE)
	{
		for (i = 0; i < BITCNT_GROUP; i++)
		{
			if (stErrLog.groupField == (1 << i))
			{
				printf("[Error]:%s failed, ErrCode(", strFuncName);
				for (j = 0; j < BITCNT_ITEM; j++)
				{
					if (stErrLog.itemField & (1 << j))
					{
                        pstMap = GetErrNameMap((unsigned int)i, (unsigned int)j);
						errName = (pstMap != NULL) ? pstMap->strDescription : "ERR_UNKNOWN";
						printf(" %s ", errName);
					}
				}
				printf(")!\r\n");
				break;
			}
			else if (stErrLog.groupField & (1 << i))
			{
				printf("multiple error groups detected; %u error logs are currently recorded, call PrintAllErrLog() to show all.\r\n", __atomic_load_n(&gstErrStackCtx.stack.top, __ATOMIC_SEQ_CST));
				break;
			}
		}
	}
	va_start(argptr, fmt);
	if (fmt != NULL)
		vprintf(fmt, argptr);

	va_end(argptr);
	printf("\r\n");
	pthread_mutex_unlock(&gstErrStackCtx.errPrintMutex);
	return funcret;
}

int StoreAllErrLogToFile(const char *storeFilePath) //store all error logs since system power-on if ClearErrStack() has not been called
{
	int i, ret;
	unsigned int errLogCnt;
	STACK_ERRLOG stackSnapshot;
	FILE *fp = NULL;

	if (storeFilePath == NULL)
		return ERR_ARGV_1;

	if ( (ret = CopyErrStack(&stackSnapshot)) != RET_SUCCESS )
	{
		if (ret == RET_SKIPPED)
			printf("StoreAllErrLogToFile skipped: error stack is being written.\r\n");
		else
			printf("StoreAllErrLogToFile failed, ret=0x%08X.\r\n", ret);

		return ret;
	}

	if ( (errLogCnt = stackSnapshot.top) == 0 )
		return RET_SUCCESS;

	if ( (fp = fopen(storeFilePath, "w")) == NULL )
		return ERR_FILE_OP_FAILED;

	fprintf(fp, "-------------------------------Error Log(total %u items)--------------------------------------\r\n", errLogCnt);
	for (i = (int)errLogCnt - 1; i >= 0; i--)
	{
		fprintf(fp, "[Error]: %s() failed at line %d, ErrCode(%s)!\r\n", stackSnapshot.szstErrLog[i].funcName, stackSnapshot.szstErrLog[i].errLine, GetSingleErrName(stackSnapshot.szstErrLog[i].groupField, stackSnapshot.szstErrLog[i].itemField));
	}
	fclose(fp);
	return RET_SUCCESS;
}

int PrintAllErrLog(void)
{
	int i, ret;
	unsigned int errLogCnt;
	STACK_ERRLOG stackSnapshot;

	pthread_mutex_lock(&gstErrStackCtx.errPrintMutex);
	if ( (ret = CopyErrStack(&stackSnapshot)) != RET_SUCCESS )
	{
		if (ret == RET_SKIPPED)
			printf("PrintAllErrLog skipped: error stack is being written.\r\n");
		else
			printf("PrintAllErrLog failed, ret=0x%08X.\r\n", ret);

		pthread_mutex_unlock(&gstErrStackCtx.errPrintMutex);
		return ret;
	}
	errLogCnt = stackSnapshot.top;
	if (errLogCnt)
		printf("-------------------------------Error Log(total %u items)--------------------------------------\r\n", errLogCnt);
	else
		printf("error stack is empty.\r\n");

	for (i = (int)errLogCnt - 1; i >= 0; i--)
	{
		printf("[Error]: %s() failed at line %d, ErrCode(%s)!\r\n", stackSnapshot.szstErrLog[i].funcName, stackSnapshot.szstErrLog[i].errLine, GetSingleErrName(stackSnapshot.szstErrLog[i].groupField, stackSnapshot.szstErrLog[i].itemField));
	}
	pthread_mutex_unlock(&gstErrStackCtx.errPrintMutex);
	return RET_SUCCESS;
}

int Print1stErrLog(void)
{
	int ret;
	STRUCT_ERRLIST stErrLog;

	pthread_mutex_lock(&gstErrStackCtx.errPrintMutex);
	if ( (ret = Copy1stErrLogOfErrList(&stErrLog)) != RET_SUCCESS )
	{
		if (ret == RET_SKIPPED)
			printf("Print1stErrLog skipped: error stack is being written.\r\n");
		else
			printf("Print1stErrLog failed, ret=0x%08X.\r\n", ret);

		pthread_mutex_unlock(&gstErrStackCtx.errPrintMutex);
		return ret;
	}

	if (stErrLog.errcode == RET_SUCCESS)
		printf("error stack is empty.\r\n");
	else
		printf("[1stError]: %s() failed at line %d, ErrCode(%s)!\r\n", stErrLog.funcName, stErrLog.errLine, GetSingleErrName(stErrLog.groupField, stErrLog.itemField));

	pthread_mutex_unlock(&gstErrStackCtx.errPrintMutex);
	return RET_SUCCESS;
}

int PrintLatest1ErrLog(void)
{
	int ret;
	STRUCT_ERRLIST stErrLog;

	pthread_mutex_lock(&gstErrStackCtx.errPrintMutex);
	if ( (ret = CopyLatest1ErrLogOfErrList(&stErrLog)) != RET_SUCCESS )
	{
		if (ret == RET_SKIPPED)
			printf("PrintLatest1ErrLog skipped: error stack is being written.\r\n");
		else
			printf("PrintLatest1ErrLog failed, ret=0x%08X.\r\n", ret);

		pthread_mutex_unlock(&gstErrStackCtx.errPrintMutex);
		return ret;
	}

	if (stErrLog.errcode == RET_SUCCESS)
		printf("error stack is empty.\r\n");
	else
		printf("[LastError]: %s() failed at line %d, ErrCode(%s)!\r\n", stErrLog.funcName, stErrLog.errLine, GetSingleErrName(stErrLog.groupField, stErrLog.itemField));

	pthread_mutex_unlock(&gstErrStackCtx.errPrintMutex);
	return RET_SUCCESS;
}

int Copy1stErrLogOfErrList(STRUCT_ERRLIST *pstErrLogCopy)
{
	unsigned int expected = 0, top;

	if (pstErrLogCopy == NULL)
		return ERRCODE_ARGV_1;

	if (!__atomic_compare_exchange_n(&gstErrStackCtx.isWriting, &expected, 1, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST))
		return RET_SKIPPED;

	top = __atomic_load_n(&gstErrStackCtx.stack.top, __ATOMIC_SEQ_CST);
	if (top == 0)
		memset(pstErrLogCopy, 0, sizeof(*pstErrLogCopy));
	else
		*pstErrLogCopy = gstErrStackCtx.stack.szstErrLog[0];

	__atomic_store_n(&gstErrStackCtx.isWriting, 0, __ATOMIC_SEQ_CST);
	return RET_SUCCESS;
}

int CopyLatest1ErrLogOfErrList(STRUCT_ERRLIST *pstErrLogCopy)
{
	unsigned int expected = 0, top;

	if (pstErrLogCopy == NULL)
		return ERRCODE_ARGV_1;

	if (!__atomic_compare_exchange_n(&gstErrStackCtx.isWriting, &expected, 1, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST))
		return RET_SKIPPED;

	top = __atomic_load_n(&gstErrStackCtx.stack.top, __ATOMIC_SEQ_CST);
	if (top == 0)
		memset(pstErrLogCopy, 0, sizeof(*pstErrLogCopy));
	else
		*pstErrLogCopy = gstErrStackCtx.stack.szstErrLog[top - 1];

	__atomic_store_n(&gstErrStackCtx.isWriting, 0, __ATOMIC_SEQ_CST);
	return RET_SUCCESS;
}

static int BitFieldValToBitId(unsigned int bitField, unsigned int bitCnt)
{
	int bitId;

	for (bitId = 0; bitId < (int)bitCnt; bitId++)
	{
		if (bitField == (1 << bitId))
			return bitId;
	}

	return ERRCODE_ID_NOT_FOUND;
}

static const ERR_NAME_MAP* GetErrNameMap(unsigned int group, unsigned int item)
{
    if (group >= BITCNT_GROUP || item >= BITCNT_ITEM)
        return NULL;

    if (gErrNameMap[group][item].strDescription == NULL)
        return NULL;

    if (gErrNameMap[group][item].errCode != ERRCODE(group, item))
        return NULL;

    return &gErrNameMap[group][item];
}

static const char* GetSingleErrName(unsigned int groupField, unsigned int itemField)
{
	int groupIdx = BitFieldValToBitId(groupField, BITCNT_GROUP);
	int itemIdx = BitFieldValToBitId(itemField, BITCNT_ITEM);
	const ERR_NAME_MAP *pstMap = NULL;

	if (groupIdx < 0 || groupIdx >= BITCNT_GROUP || itemIdx < 0 || itemIdx >= BITCNT_ITEM)
		return "ERR_UNKNOWN";

	if ( (pstMap = GetErrNameMap((unsigned int)groupIdx, (unsigned int)itemIdx)) == NULL )
		return "ERR_UNKNOWN";

	return pstMap->strDescription;
}

static inline int IsValidErrItem(unsigned int group, unsigned int item)
{
	return (GetErrNameMap(group, item) != NULL);
}

static int CopyErrStack(STACK_ERRLOG *pStackCopy)
{
	unsigned int expected = 0;
	unsigned int top;

	if (pStackCopy == NULL)
		return ERRCODE_ARGV_1;

	if (!__atomic_compare_exchange_n(&gstErrStackCtx.isWriting, &expected, 1, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST))
		return RET_SKIPPED;

	if ( (top = __atomic_load_n(&gstErrStackCtx.stack.top, __ATOMIC_SEQ_CST)) > ERRSTACK_ITEMCNT_MAX )
	{
		printf("error stack top=%u is out of range; clearing the stack.\r\n", top);
		__atomic_store_n(&gstErrStackCtx.stack.top, 0, __ATOMIC_SEQ_CST);
		__atomic_store_n(&gstErrStackCtx.isWriting, 0, __ATOMIC_SEQ_CST);
		return ERRCODE_OVER_BUF;
	}
	pStackCopy->top = top;
	if (top > 0)
		memcpy(pStackCopy->szstErrLog, gstErrStackCtx.stack.szstErrLog, (size_t)top * sizeof(pStackCopy->szstErrLog[0]));

	__atomic_store_n(&gstErrStackCtx.isWriting, 0, __ATOMIC_SEQ_CST);
	return RET_SUCCESS;
}
