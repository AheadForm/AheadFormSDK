#ifndef __THREAD_SCHEDULE_H
#define __THREAD_SCHEDULE_H

#include "PublicDef.h"
#include <pthread.h>
#include <sched.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

#define GET_TID() ((pid_t)syscall(SYS_gettid))

#define CPU_CORE_AUTO_BALANCE  -1


int CreateNormalThread(pthread_t *pRetTaskThreadId, void* (*ThreadFunction)(void*), void *pThreadArg, int runInCpuCoreId);
int CreateRThread(pthread_t *pRetTaskThreadId, void* (*ThreadFunction)(void*), void *pThreadArg, int runInCpuCoreId, int schedPriority);
int BecomeRThread(int runInCpuCoreId, int schedPriority);
int BindThreadRunInCpuCore(pthread_t threadId, int runInCpuCoreId);
int ThreadsOf1ProcessScheduleBalance(pthread_t pthreadIdAry[], int threadCnt);
int ScheduleBalance(pid_t threadTidAry[], int threadCnt);





#endif

