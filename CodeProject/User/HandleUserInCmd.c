#include "HandleUserInCmd.h"

#include <strings.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/select.h>


static USERIN_CMD* ParseUserIn(const USERIN_TASK *userInTaskAry);
static int DispatchTask(AHEADFORM_ALL_CTRL *pstAheadForm, const USERIN_TASK *userInTaskAry, USERIN_CMD *pstCmd);

int HandleUserInCmd(AHEADFORM_ALL_CTRL *pstAheadForm, const USERIN_TASK *userInTaskAry)
{
	int bytes = 0, ret = RET_SUCCESS;
	USERIN_CMD *pstCmd = NULL;

	do
	{
		if ( (pstCmd = ParseUserIn(userInTaskAry)) != NULL )
		{
			ret = DispatchTask(pstAheadForm, userInTaskAry, pstCmd);
			PrintCmdResult(pstCmd->argv[0], ret);
			if (ret == RET_EXIT)
				return ret;
		}

		if (ioctl(STDIN_FILENO, FIONREAD, &bytes) < 0)
		{
			perror("ioctl(STDIN_FILENO, FIONREAD, &bytes):");
			return ERR_EVENT;
		}
	} while (bytes != 0);

	printf(">>");
	fflush(stdout);
	return ret;
}

void ClearStdIn(void)
{
	struct timeval stTimeout = {.tv_sec = 1, .tv_usec = 0};
	fd_set rdFdSet;
	int c, ret;

	FD_ZERO(&rdFdSet);
	FD_SET(STDIN_FILENO, &rdFdSet);
	if ( (ret = select(STDIN_FILENO + 1, &rdFdSet, NULL, NULL, &stTimeout)) <= 0 )
	{
		if (ret < 0)
			printf("%s() call select(stdin fd = %d) failed: %s\r\n", __FUNCTION__, STDIN_FILENO, strerror(errno));
	}
	else
	{
		while ((c = getchar()) != '\n' && c != EOF)
		{}
	}
}

static USERIN_CMD* ParseUserIn(const USERIN_TASK *userInTaskAry)
{
	int i;
	char *strArg = NULL;
	static char strCmdInfo[USERIN_CMD_LENMAX] = {0};
	static USERIN_CMD stCmdMsg;

	memset(&stCmdMsg, 0, sizeof(stCmdMsg));
	if (fgets(strCmdInfo, USERIN_CMD_LENMAX, stdin) == NULL)
    {
        perror("fgets:");
        return NULL;
    }
    
	strCmdInfo[strcspn(strCmdInfo, "\r\n")] = 0;//delete \r\n
	if ( (stCmdMsg.argv[stCmdMsg.argc++] = strtok(strCmdInfo, " ")) == NULL)
		return NULL;

	while ( (strArg = strtok(NULL, " ")) != NULL )
	{
		if (stCmdMsg.argc >= MAX_ARG_CNT)
			return NULL;

		stCmdMsg.argv[stCmdMsg.argc++] = strArg;
	}

	for (i = 0; userInTaskAry[i].pFunction != NULL; i++)
	{
		if (strcasecmp(stCmdMsg.argv[0], userInTaskAry[i].strCmd) == 0)
			break;
	}

	if (userInTaskAry[i].pFunction == NULL)
	{
		STD_INFO_PRINT("cmd [%s] don't exist!\r\n", strCmdInfo);
		return NULL;
	}	
	stCmdMsg.cmdId = i;
	return &stCmdMsg;
}


static int DispatchTask(AHEADFORM_ALL_CTRL *pstAheadForm, const USERIN_TASK *userInTaskAry, USERIN_CMD *pstCmd)
{
	return userInTaskAry[pstCmd->cmdId].pFunction(pstAheadForm, pstCmd->argc, pstCmd->argv);
}

