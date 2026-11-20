#include "HandleUserInCmd.h"
#include <unistd.h>
#include <sys/select.h>


int main(int argc, const char *argv[])
{
	static AHEADFORM_ALL_CTRL stAheadForm;
	fd_set rdFdSet;
	int max_fd = STDIN_FILENO + 1, ret = RET_SUCCESS;

	if ( (ret = AheadForm_Init(&stAheadForm, DEFAULT_INI_FILE_PATH)) != RET_SUCCESS )
	{
		PrintAllErrLog();
		return ret;
	}
	printf("input help will print cmdInfo...\r\n");
	while (ret != RET_EXIT)
	{
		FD_ZERO(&rdFdSet);
		FD_SET(STDIN_FILENO, &rdFdSet);
		if (select(max_fd, &rdFdSet, NULL, NULL, NULL) < 0)
		{
			printf("%s() call select(stdin fd = %d) failed: %s\r\n", __FUNCTION__, STDIN_FILENO, strerror(errno));
			ret = ERR_SELECT;
			break;
		}

		if (FD_ISSET(STDIN_FILENO, &rdFdSet))
			ret = HandleUserInCmd(&stAheadForm, gUserInTaskAry);
	}
	AheadForm_Deinit(&stAheadForm);
	PrintAllErrLog();
	return ret;
}
