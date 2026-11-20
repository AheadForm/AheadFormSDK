#ifndef __PUBLICDEF__H_
#define __PUBLICDEF__H_

#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <math.h>
#include "Error.h"

#define DBG_PRINT() printf("%s l(%d)\r\n", __FUNCTION__, __LINE__)

typedef enum
{
    DB_LESS = -1,
    DB_EQL = 0,
    DB_BIGGER = 1
} ENUM_DOUBLE_COMPARE;

int IsIntValNearlyEqual(int actualVal, int expectedVal, int tolerance);
ENUM_DOUBLE_COMPARE Compare_double(double a, double b, double epsilon);
unsigned int CeilDoubleToU32Val(double val);
int PrintDataArray(const unsigned char *buf, unsigned int len);

#endif
