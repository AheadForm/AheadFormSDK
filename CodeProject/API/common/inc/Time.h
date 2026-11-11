#ifndef __TIME__H_
#define __TIME__H_

#include <arpa/inet.h>
#include <time.h>
#include <sys/time.h>
#include <sys/timerfd.h>
#include <stdio.h>
#include <string.h>

typedef struct
{
    unsigned short year;       //full year, e.g. 2026
    unsigned short ms;         //0-999
    unsigned char month;       //1-12
    unsigned char day;         //1-31
    unsigned char hour;        //0-23
    unsigned char minute;      //0-59
    unsigned char second;      //0-59
} SYSTIME_RET;


unsigned long long GetTime100usCnt_UpToNow(void);
unsigned long long GetTimeUsCnt_UpToNow(void);
unsigned long long GetElapsedMonotonicTime_ms(unsigned long long StartTime_us);
unsigned long long CalcElapsedTime_ms(unsigned long long StartTime_us, unsigned long long Now_us);
int SetTimerPeriod_ms(int timer_fd, unsigned long Period_ms);
int SetTimerPeriod_us(int timer_fd, unsigned long Period_us);
int SysTimeSet(int year, int month, int day, int hour, int min, int sec, int ms);
int SysTimeGet(SYSTIME_RET *pstRetSysTime);
int SysTimeCmp(const SYSTIME_RET *pstSysTime1, const SYSTIME_RET *pstSysTime2);
long long CalcTimeDiff_us(struct timespec ts1, struct timespec ts2);


#endif
