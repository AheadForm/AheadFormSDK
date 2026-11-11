#include "Time.h"
#include "PublicDef.h"
#include <limits.h>


unsigned long long GetTime100usCnt_UpToNow(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (ts.tv_sec * 10000 + ts.tv_nsec / 100000);
}

unsigned long long GetTimeUsCnt_UpToNow(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (ts.tv_sec * 1000000 + ts.tv_nsec / 1000);
}

unsigned long long GetElapsedMonotonicTime_ms(unsigned long long StartTime_us)
{
    unsigned long long Now_us = GetTimeUsCnt_UpToNow();

    if (StartTime_us > Now_us)
        return ULLONG_MAX;

    return (Now_us - StartTime_us) / 1000;
}

unsigned long long CalcElapsedTime_ms(unsigned long long StartTime_us, unsigned long long Now_us)
{
    if (StartTime_us > Now_us)
        return ULLONG_MAX;

    return (Now_us - StartTime_us) / 1000;
}

int SetTimerPeriod_ms(int timer_fd, unsigned long Period_ms)
{
    struct itimerspec stTimerSpec;

    if (timer_fd <= 0)
        return ERR_ARGV_1;

    if (Period_ms == 0)
        return ERR_ARGV_2;

    memset(&stTimerSpec, 0, sizeof(stTimerSpec));
    stTimerSpec.it_interval.tv_sec = Period_ms / 1000UL;
    stTimerSpec.it_interval.tv_nsec = (long)((Period_ms % 1000UL) * 1000000UL);
    stTimerSpec.it_value = stTimerSpec.it_interval;
    if (timerfd_settime(timer_fd, 0, &stTimerSpec, NULL) != RET_SUCCESS)
    {
        printf("%s() call timerfd_settime() timer fd %d with period %lu ms failed: %s\r\n", __FUNCTION__, timer_fd, Period_ms, strerror(errno));
        return ERR_CLOCK;
    }

    return RET_SUCCESS;
}

int SetTimerPeriod_us(int timer_fd, unsigned long Period_us)
{
    struct itimerspec stTimerSpec;

    if (timer_fd <= 0)
        return ERR_ARGV_1;

    if (Period_us == 0)
        return ERR_ARGV_2;

    memset(&stTimerSpec, 0, sizeof(stTimerSpec));
    stTimerSpec.it_interval.tv_sec = Period_us / 1000000UL;
    stTimerSpec.it_interval.tv_nsec = (long)((Period_us % 1000000UL) * 1000UL);
    stTimerSpec.it_value = stTimerSpec.it_interval;
    if (timerfd_settime(timer_fd, 0, &stTimerSpec, NULL) != RET_SUCCESS)
    {
        printf("%s() call timerfd_settime() timer fd %d with period %lu us failed: %s\r\n", __FUNCTION__, timer_fd, Period_us, strerror(errno));
        return ERR_CLOCK;
    }

    return RET_SUCCESS;
}

int SysTimeSet(int year, int month, int day, int hour, int min, int sec, int ms)
{
    struct tm stSysTime =
    {
        .tm_sec = sec,
        .tm_min = min,
        .tm_hour = hour,
        .tm_mday = day,
        .tm_mon = month - 1, //from month1
        .tm_year = year - 1900, //from year1900
        .tm_isdst = 0 //Must be set; otherwise mktime may return -1.
    };
    struct timeval tv = {.tv_sec = mktime(&stSysTime), .tv_usec = ms * 1000};

    if (settimeofday(&tv, NULL) != RET_SUCCESS)
    {
        printf("%s() call settimeofday() failed: %s\r\n", __FUNCTION__, strerror(errno));
        return ERR_CLOCK;
    }

    return RET_SUCCESS;
}

int SysTimeGet(SYSTIME_RET *pstRetSysTime)
{
    struct tm *pstSysTime = NULL;
    struct timeval tv;

    if (pstRetSysTime == NULL)
        return ERR_ARGV_1;

    if (gettimeofday(&tv, NULL) != RET_SUCCESS)
        return ERR_CLOCK;

    if ( (pstSysTime = localtime(&tv.tv_sec)) == NULL )
        return ERR_CLOCK;

    pstRetSysTime->year = (unsigned short)(pstSysTime->tm_year + 1900);
    pstRetSysTime->month = pstSysTime->tm_mon + 1; //from month1
    pstRetSysTime->day = pstSysTime->tm_mday;
    pstRetSysTime->hour = pstSysTime->tm_hour;
    pstRetSysTime->minute = pstSysTime->tm_min;
    pstRetSysTime->second = pstSysTime->tm_sec;
    pstRetSysTime->ms = (unsigned short)(tv.tv_usec / 1000);
    //printf("ret:year=%hu, month=%hhu, day=%hhu, hour=%hhu, minute=%hhu, second=%hhu, ms=%hu\r\n", 
    //pstRetSysTime->year, pstRetSysTime->month, pstRetSysTime->day, pstRetSysTime->hour, pstRetSysTime->minute, pstRetSysTime->second, pstRetSysTime->ms);
    return RET_SUCCESS;
}

int SysTimeCmp(const SYSTIME_RET *pstSysTime1, const SYSTIME_RET *pstSysTime2)
{
    if (pstSysTime1->year != pstSysTime2->year)
        return (pstSysTime1->year > pstSysTime2->year) ? 1 : -1;

    if (pstSysTime1->month != pstSysTime2->month)
        return (pstSysTime1->month > pstSysTime2->month) ? 1 : -1;

    if (pstSysTime1->day != pstSysTime2->day)
        return (pstSysTime1->day > pstSysTime2->day) ? 1 : -1;

    if (pstSysTime1->hour != pstSysTime2->hour)
        return (pstSysTime1->hour > pstSysTime2->hour) ? 1 : -1;

    if (pstSysTime1->minute != pstSysTime2->minute)
        return (pstSysTime1->minute > pstSysTime2->minute) ? 1 : -1;

    if (pstSysTime1->second != pstSysTime2->second)
        return (pstSysTime1->second > pstSysTime2->second) ? 1 : -1;

    if (pstSysTime1->ms != pstSysTime2->ms)
        return (pstSysTime1->ms > pstSysTime2->ms) ? 1 : -1;

    return 0;
}

long long CalcTimeDiff_us(struct timespec ts1, struct timespec ts2)
{
    return ( (ts2.tv_sec - ts1.tv_sec) * 1000000 + (ts2.tv_nsec - ts1.tv_nsec) / 1000 );
}
