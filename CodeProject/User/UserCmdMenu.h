#ifndef _USERCMDMENU_H_
#define _USERCMDMENU_H_

#include "AheadForm_Task.h"


#define USERIN_CMD_LENMAX  1024
#define MAX_ARG_CNT        64
//Menu text, command results and input prompts.
#define STD_INFO_PRINT(fmt, ...) printf(">>"fmt, ##__VA_ARGS__)
#define USERIN_CMD_CNT_MAX 219

typedef struct
{
	char strCmd[USERIN_CMD_LENMAX];
	int (*pFunction)(AHEADFORM_ALL_CTRL *pstAheadForm, int argc, char *argv[]);
	char *strDescription;
} USERIN_TASK;

typedef struct
{
	int cmdId;
	int argc;
	char *argv[MAX_ARG_CNT];
} USERIN_CMD;


extern const USERIN_TASK gUserInTaskAry[USERIN_CMD_CNT_MAX];

int PrintCmdResult(char *strCmd, int cmdRet);
//Accept decimal or 0x/0X hexadecimal; reject signs, empty strings and multi-digit leading-zero forms.
int IsDigitStr(char *strCheck);
/*
	arg:
		needArgc == NULL    means not exist or don't check;
		needArgc == "3;4"   means that argc ought to be 3 or 4;

		digArgvPos == NULL  means not need digitial arg or don't check;
		digArgvPos == "1,3" means that  argv[1] and argv[3] ought to be digitial arg (digitial arg support all radix);
		digArgvPos == "1,3;1,2,3" means that "argv[1] and argv[3]" or "argv[1] and argv[2] and argv[3]" ought to be digtial arg;
		digArgvPos == " ;1,2,3" means that "no digtial arg" or "argv[1] and argv[2] and argv[3]" ought to be digtial arg;
		digArgvPos == "1,2,3; " means that "argv[1] and argv[2] and argv[3]" ought to be digtial arg or "no digtial arg";
		digArgvPos == "all" means that "argv[1] ~ argv[argc - 1]" all ought to be digitial arg.

	return: 
		0 means right; else return ERR_ARGC or ERR_ARGV
*/
int IsUserInError(int userInArgc, char *argv[], char *needArgc, char *digArgvPos);

#endif
