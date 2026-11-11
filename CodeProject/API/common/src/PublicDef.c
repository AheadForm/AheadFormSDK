#include "PublicDef.h"

int IsIntValNearlyEqual(int actualVal, int expectedVal, int tolerance)
{
    return tolerance >= 0 && llabs((long long)actualVal - (long long)expectedVal) <= (long long)tolerance;
}

ENUM_DOUBLE_COMPARE Compare_double(double a, double b, double epsilon)
{
    double diff = fabs(a - b);

    //printf("a=%lf, b=%lf, diff=%lf, epsilon=%lf\r\n", a, b, diff, epsilon);
    if (diff < epsilon)
        return DB_EQL;
    else if (a > b)
        return DB_BIGGER;
    else
        return DB_LESS;
}

unsigned int CeilDoubleToU32Val(double val)
{
    if (!isfinite(val) && val > 0.0)
        return UINT_MAX;

    if (!isfinite(val) || val <= 0.0)
        return 0;

    if (val >= UINT_MAX)
        return UINT_MAX;

    return (unsigned int)ceil(val);
}

int PrintDataArray(const unsigned char *buf, unsigned int len)
{
    unsigned int i;

    if (len > 0 && buf == NULL)
        return ERR_ARGV_1;

    printf("len=%u: ", len);
    if (len > 0)
    {
        for (i = 0; i < len; i++)
            printf("0x%hhX ", buf[i]);
    }
    printf("\r\n");
    return RET_SUCCESS;
}
