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
		printf(">>");
		if ( (pstCmd = ParseUserIn(userInTaskAry)) != NULL )
		{
			ret = DispatchTask(pstAheadForm, userInTaskAry, pstCmd);
			PrintCmdResult(pstCmd->argv[0], ret);
			if (ret == RET_EXIT)
				return ret;
		}

		if (feof(stdin))
			return RET_EXIT;

		if (ioctl(STDIN_FILENO, FIONREAD, &bytes) < 0)
		{
			printf("%s() call ioctl(stdin, FIONREAD) failed: %s\r\n", __FUNCTION__, strerror(errno));
			return ERR_EVENT;
		}
	} while (bytes != 0);

	return ret;
}

void ClearStdIn(void)
{
    struct timeval timeout = {.tv_sec = 1, .tv_usec = 0};
    fd_set rdFdSet;
    int c, ret;

    FD_ZERO(&rdFdSet);
    FD_SET(STDIN_FILENO, &rdFdSet);
    if ( (ret = select(STDIN_FILENO + 1, &rdFdSet, NULL, NULL, &timeout)) <= 0 )
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
	char *pCursor = NULL;
	char quote;
	static char strCmdInfo[USERIN_CMD_LENMAX] = {0};
	static USERIN_CMD stCmdMsg;

	memset(&stCmdMsg, 0, sizeof(stCmdMsg));
	if (fgets(strCmdInfo, USERIN_CMD_LENMAX, stdin) == NULL)
    {
        if (!feof(stdin))
            printf("%s() call fgets(stdin) failed: %s\r\n", __FUNCTION__, strerror(errno));

        return NULL;
    }
    
	strCmdInfo[strcspn(strCmdInfo, "\r\n")] = 0;//delete \r\n
	pCursor = strCmdInfo;
	while (*pCursor != 0)
	{
		while (*pCursor == ' ' || *pCursor == '\t')
			pCursor++;

		if (*pCursor == 0)
			break;

		if (stCmdMsg.argc >= MAX_ARG_CNT)
			return NULL;

		quote = *pCursor == '"' || *pCursor == '\'' ? *pCursor++ : 0;
		stCmdMsg.argv[stCmdMsg.argc++] = pCursor;
		while (*pCursor != 0 && (quote != 0 ? *pCursor != quote : *pCursor != ' ' && *pCursor != '\t'))
			pCursor++;

		if (quote != 0 && *pCursor != quote)
		{
			printf("%s() unmatched path quote\r\n", __FUNCTION__);
			return NULL;
		}

		if (*pCursor != 0)
			*pCursor++ = 0;
	}

	if (stCmdMsg.argc == 0)
		return NULL;

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

