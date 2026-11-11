#ifndef _PARSEFILE_H
#define _PARSEFILE_H

#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <sys/stat.h>
#include "PublicDef.h"

//Returns data size read from file, or ERR_* on error.
int GetAllContentOfFile(const char *filePath, char *buf, unsigned int bufLen);
int StoreStrToNewTxtFile(const char *filePath, const char *storeStr);
int CreateParentDir(const char *filePath);


#endif
