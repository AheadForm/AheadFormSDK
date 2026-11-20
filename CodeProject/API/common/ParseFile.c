#include "ParseFile.h"
#include <errno.h>
#include <libgen.h>
#include <limits.h>
#include <stdio.h>
#include <unistd.h>


//Returns data size read from file, or ERR_* on error.
int GetAllContentOfFile(const char *filePath, char *buf, unsigned int bufLen)
{
    FILE *fp = NULL;
	int fileLen = 0;
	struct stat fst;

    if (filePath == NULL)
        return ERR_ARGV_1;

    if (buf == NULL)
        return ERR_ARGV_2;

    if (bufLen == 0)
        return ERR_ARGV_3;

    if ( (fp = fopen(filePath, "rb")) == NULL )
        return ERR_FILE_OP_FAILED;

    printf("fopen succeed, file: %s\r\n", filePath);
    if (fstat(fileno(fp), &fst) != RET_SUCCESS)
    {
        printf("%s() call fstat() file %s failed: %s\r\n", __FUNCTION__, filePath, strerror(errno));
        fclose(fp);
        return ERR_FILE_OP_FAILED;
    }

    if (fst.st_size < 0 || (unsigned long long)fst.st_size >= bufLen || (unsigned long long)fst.st_size > INT_MAX)
    {
        fclose(fp);
        return ERR_OVER_BUF;
    }
	fileLen = (int)fst.st_size;
	if (fileLen > 0 && fread(buf, 1, (size_t)fileLen, fp) != (size_t)fileLen)
    {
        fclose(fp);
        return ERR_FILE_OP_FAILED;
    }
    buf[fileLen] = 0;
	fclose(fp);
	return fileLen;
}

int StoreStrToNewTxtFile(const char *filePath, const char *storeStr)
{
    FILE *fp = NULL;
    int ret = RET_SUCCESS;

    if (filePath == NULL)
        return ERR_ARGV_1;

    if (storeStr == NULL)
        return ERR_ARGV_2;

    if ( (fp = fopen(filePath, "w+")) == NULL )
    {
        printf("%s() call fopen(%s, w+) failed: %s\r\n", __FUNCTION__, filePath, strerror(errno));
        return ERR_FILE_OP_FAILED;
    }

	if (fprintf(fp, "%s", storeStr) < 0)
		ret = ERR_FILE_OP_FAILED;

	if (fclose(fp) != RET_SUCCESS)
		ret = ERR_FILE_OP_FAILED;

	return ret;
}

int CreateParentDir(const char *filePath)
{
    char filePathBuf[512];
    char *parentDir = NULL, *pCh = NULL;
    struct stat st;

    if (filePath == NULL || filePath[0] == 0 || strlen(filePath) >= sizeof(filePathBuf))
        return ERR_ARGV_1;

    strcpy(filePathBuf, filePath);
    parentDir = dirname(filePathBuf);
    if (parentDir == NULL || parentDir[0] == 0 || strcmp(parentDir, ".") == 0 || strcmp(parentDir, "/") == 0)
        return RET_SUCCESS;

    for (pCh = parentDir + 1; *pCh != 0; pCh++)
    {
        if (*pCh != '/')
            continue;

        *pCh = 0;
        if (mkdir(parentDir, 0755) != RET_SUCCESS)
        {
            if (errno != EEXIST)
            {
                printf("%s() call mkdir(%s) failed: %s\r\n", __FUNCTION__, parentDir, strerror(errno));
                return ERR_FILE_OP_FAILED;
            }

            if (stat(parentDir, &st) != RET_SUCCESS)
            {
                printf("%s() call stat(%s) failed: %s\r\n", __FUNCTION__, parentDir, strerror(errno));
                return ERR_FILE_OP_FAILED;
            }

            if (!S_ISDIR(st.st_mode))
            {
                printf("%s exists but is not a directory.\r\n", parentDir);
                return ERR_FILE_OP_FAILED;
            }
        }
        *pCh = '/';
    }

    if (mkdir(parentDir, 0755) != RET_SUCCESS)
    {
        if (errno != EEXIST)
        {
            printf("%s() call mkdir(%s) failed: %s\r\n", __FUNCTION__, parentDir, strerror(errno));
            return ERR_FILE_OP_FAILED;
        }

        if (stat(parentDir, &st) != RET_SUCCESS)
        {
            printf("%s() call stat(%s) failed: %s\r\n", __FUNCTION__, parentDir, strerror(errno));
            return ERR_FILE_OP_FAILED;
        }

        if (!S_ISDIR(st.st_mode))
        {
            printf("%s exists but is not a directory.\r\n", parentDir);
            return ERR_FILE_OP_FAILED;
        }
    }

    return RET_SUCCESS;
}
