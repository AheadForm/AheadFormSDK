#include "Thread_Schedule.h"


int CreateNormalThread(pthread_t *pRetTaskThreadId, void* (*ThreadFunction)(void*), void *pThreadArg, int runInCpuCoreId)
{
    pthread_attr_t stAttr;
    cpu_set_t mask;
    int ret;

    if (pRetTaskThreadId == NULL)
        return ERR_ARGV_1;

    if (ThreadFunction == NULL)
        return ERR_ARGV_2;

    if (runInCpuCoreId != CPU_CORE_AUTO_BALANCE && (runInCpuCoreId < 0 || runInCpuCoreId >= CPU_SETSIZE))
        return ERR_ARGV_4;

    if ( (ret = pthread_attr_init(&stAttr)) != RET_SUCCESS )
    {
        printf("%s() call pthread_attr_init() failed: %s\r\n", __FUNCTION__, strerror(ret));
        return ERR_PTHREAD_INIT;
    }

    if ( (ret = pthread_attr_setinheritsched(&stAttr, PTHREAD_EXPLICIT_SCHED)) != RET_SUCCESS )//Do not inherit parent scheduling attributes, so child thread attributes can be set successfully.
    {
        printf("%s() call pthread_attr_setinheritsched(PTHREAD_EXPLICIT_SCHED) failed: %s\r\n", __FUNCTION__, strerror(ret));
        pthread_attr_destroy(&stAttr);
        return ERR_PTHREAD_CFG;
    }

    if ( (ret = pthread_attr_setschedpolicy(&stAttr, SCHED_OTHER)) != RET_SUCCESS )
    {
        printf("%s() call pthread_attr_setschedpolicy(SCHED_OTHER) failed: %s\r\n", __FUNCTION__, strerror(ret));
        pthread_attr_destroy(&stAttr);
        return ERR_PTHREAD_CFG;
    }

    if (runInCpuCoreId != CPU_CORE_AUTO_BALANCE)
    {
        CPU_ZERO(&mask);
        CPU_SET(runInCpuCoreId, &mask);
        if ( (ret = pthread_attr_setaffinity_np(&stAttr, sizeof(mask), &mask)) != RET_SUCCESS )
        {
            printf("%s() call pthread_attr_setaffinity_np() with CPU core %d failed: %s\r\n", __FUNCTION__, runInCpuCoreId, strerror(ret));
            pthread_attr_destroy(&stAttr);
            return ERR_PTHREAD_CFG;
        }
    }

    if ( (ret = pthread_create(pRetTaskThreadId, &stAttr, ThreadFunction, pThreadArg)) != RET_SUCCESS )
    {
        printf("%s() call pthread_create() failed: %s\r\n", __FUNCTION__, strerror(ret));
        pthread_attr_destroy(&stAttr);
        return ERR_PTHREAD_INIT;
    }
    pthread_attr_destroy(&stAttr);
    return RET_SUCCESS;
}

int CreateRThread(pthread_t *pRetTaskThreadId, void* (*ThreadFunction)(void*), void *pThreadArg, int runInCpuCoreId, int schedPriority)
{
    pthread_attr_t stAttr;
    cpu_set_t mask;
    struct sched_param stParam;
    int ret, schedPriorityMin, schedPriorityMax;

    if (pRetTaskThreadId == NULL)
        return ERR_ARGV_1;

    if (ThreadFunction == NULL)
        return ERR_ARGV_2;

    if (runInCpuCoreId != CPU_CORE_AUTO_BALANCE && (runInCpuCoreId < 0 || runInCpuCoreId >= CPU_SETSIZE))
        return ERR_ARGV_4;

    if ( (schedPriorityMin = sched_get_priority_min(SCHED_RR)) < 0 )
        return ERR_PTHREAD_CFG;

    if ( (schedPriorityMax = sched_get_priority_max(SCHED_RR)) < 0 )
        return ERR_PTHREAD_CFG;

    if (schedPriority < schedPriorityMin || schedPriority > schedPriorityMax)
        return ERR_ARGV_5;

    if ( (ret = pthread_attr_init(&stAttr)) != RET_SUCCESS )
    {
        printf("%s() call pthread_attr_init() failed: %s\r\n", __FUNCTION__, strerror(ret));
        return ERR_PTHREAD_INIT;
    }

    if ( (ret = pthread_attr_setinheritsched(&stAttr, PTHREAD_EXPLICIT_SCHED)) != RET_SUCCESS )//Do not inherit parent scheduling attributes, so child thread attributes can be set successfully.
    {
        printf("%s() call pthread_attr_setinheritsched(PTHREAD_EXPLICIT_SCHED) failed: %s\r\n", __FUNCTION__, strerror(ret));
        pthread_attr_destroy(&stAttr);
        return ERR_PTHREAD_CFG;
    }

    //if (pthread_attr_setschedpolicy(&stAttr, SCHED_FIFO) != RET_SUCCESS)
    if ( (ret = pthread_attr_setschedpolicy(&stAttr, SCHED_RR)) != RET_SUCCESS )
    {
        printf("%s() call pthread_attr_setschedpolicy(SCHED_RR) failed: %s\r\n", __FUNCTION__, strerror(ret));
        pthread_attr_destroy(&stAttr);
        return ERR_PTHREAD_CFG;
    }
    stParam.sched_priority = schedPriority;
    if ( (ret = pthread_attr_setschedparam(&stAttr, &stParam)) != RET_SUCCESS )//Set thread priority; this affects PRI in htop, and a smaller PRI means higher priority.
    {
        printf("%s() call pthread_attr_setschedparam() with sched_priority %d failed: %s\r\n", __FUNCTION__, stParam.sched_priority, strerror(ret));
        pthread_attr_destroy(&stAttr);
        return ERR_PTHREAD_CFG;
    }

    if (runInCpuCoreId != CPU_CORE_AUTO_BALANCE)
    {
        CPU_ZERO(&mask);
        CPU_SET(runInCpuCoreId, &mask);
        if ( (ret = pthread_attr_setaffinity_np(&stAttr, sizeof(mask), &mask)) != RET_SUCCESS )
        {
            printf("%s() call pthread_attr_setaffinity_np() with CPU core %d failed: %s\r\n", __FUNCTION__, runInCpuCoreId, strerror(ret));
            pthread_attr_destroy(&stAttr);
            return ERR_PTHREAD_CFG;
        }
    }

    if ( (ret = pthread_create(pRetTaskThreadId, &stAttr, ThreadFunction, pThreadArg)) != RET_SUCCESS )
    {
        printf("%s() call pthread_create() failed: %s\r\n", __FUNCTION__, strerror(ret));
        pthread_attr_destroy(&stAttr);
        return ERR_PTHREAD_INIT;
    }
    pthread_attr_destroy(&stAttr);
    printf("thread[%llu] sched_priority=%d\r\n", (unsigned long long)*pRetTaskThreadId, stParam.sched_priority);
    return RET_SUCCESS;
}

int BecomeRThread(int runInCpuCoreId, int schedPriority)
{
    cpu_set_t mask;
    struct sched_param stParam;
    pthread_t threadId = pthread_self();
    int ret, schedPriorityMin, schedPriorityMax;

    if (runInCpuCoreId != CPU_CORE_AUTO_BALANCE && (runInCpuCoreId < 0 || runInCpuCoreId >= CPU_SETSIZE))
        return ERR_ARGV_1;

    if ( (schedPriorityMin = sched_get_priority_min(SCHED_RR)) < 0 )
        return ERR_PTHREAD_CFG;

    if ( (schedPriorityMax = sched_get_priority_max(SCHED_RR)) < 0 )
        return ERR_PTHREAD_CFG;

    if (schedPriority < schedPriorityMin || schedPriority > schedPriorityMax)
        return ERR_ARGV_2;

    if (runInCpuCoreId != CPU_CORE_AUTO_BALANCE)
    {
        CPU_ZERO(&mask);
        CPU_SET(runInCpuCoreId, &mask);
        if ( (ret = pthread_setaffinity_np(threadId, sizeof(cpu_set_t), &mask)) != RET_SUCCESS )
        {
            printf("%s() call pthread_setaffinity_np() thread %llu with CPU core %d failed: %s\r\n", __FUNCTION__, (unsigned long long)threadId, runInCpuCoreId, strerror(ret));
            return ERR_PTHREAD_CFG;
        }
    }
    stParam.sched_priority = schedPriority;
    if ( (ret = pthread_setschedparam(threadId, SCHED_RR, &stParam)) != RET_SUCCESS )
    {
        printf("%s() call pthread_setschedparam() thread %llu with SCHED_RR priority %d failed: %s\r\n", __FUNCTION__, (unsigned long long)threadId, stParam.sched_priority, strerror(ret));
        return ERR_PTHREAD_CFG;
    }

    return RET_SUCCESS;
}

int BindThreadRunInCpuCore(pthread_t threadId, int runInCpuCoreId)
{
    int cpuCoreId, ret;
    cpu_set_t mask;

    if (runInCpuCoreId != CPU_CORE_AUTO_BALANCE && (runInCpuCoreId < 0 || runInCpuCoreId >= CPU_SETSIZE))
        return ERR_ARGV_2;

    CPU_ZERO(&mask);
    if (runInCpuCoreId == CPU_CORE_AUTO_BALANCE)
    {
        for (cpuCoreId = 0; cpuCoreId < CPU_SETSIZE; cpuCoreId++)
            CPU_SET(cpuCoreId, &mask);
    }
    else
    {
        CPU_SET(runInCpuCoreId, &mask);
    }

    if ( (ret = pthread_setaffinity_np(threadId, sizeof(cpu_set_t), &mask)) != RET_SUCCESS )
    {
        printf("%s() call pthread_setaffinity_np() thread %llu with runInCpuCoreId %d failed: %s\r\n", __FUNCTION__, (unsigned long long)threadId, runInCpuCoreId, strerror(ret));
        return ERR_PTHREAD_CFG;
    }

    return RET_SUCCESS;
}

int ThreadsOf1ProcessScheduleBalance(pthread_t pthreadIdAry[], int threadCnt)
{
    int i, cpuCoreId, ret;
    long cpuCoreCnt;
    cpu_set_t mask;

    if (pthreadIdAry == NULL)
        return ERR_ARGV_1;

    if (threadCnt <= 0)
        return ERR_ARGV_2;

    if ( (cpuCoreCnt = sysconf(_SC_NPROCESSORS_ONLN)) <= 0 )
    {
        printf("%s() call sysconf(_SC_NPROCESSORS_ONLN) returned invalid online CPU count %ld.\r\n", __FUNCTION__, cpuCoreCnt);
        return ERR_PTHREAD_CFG;
    }

    for (i = 0; i < threadCnt; i++)
    {
        CPU_ZERO(&mask);
        cpuCoreId = i % cpuCoreCnt;
        CPU_SET(cpuCoreId, &mask);
        if ( (ret = pthread_setaffinity_np(pthreadIdAry[i], sizeof(mask), &mask)) != RET_SUCCESS )
        {
            printf("%s() call pthread_setaffinity_np() thread %llu with CPU core %d failed: %s\r\n", __FUNCTION__, (unsigned long long)pthreadIdAry[i], cpuCoreId, strerror(ret));
            return ERR_PTHREAD_CFG;
        }
    }

    return RET_SUCCESS;
}

int ScheduleBalance(pid_t threadTidAry[], int threadCnt)
{
    int i, cpuCoreId;
    long cpuCoreCnt;
    cpu_set_t mask;

    if (threadTidAry == NULL)
        return ERR_ARGV_1;

    if (threadCnt <= 0)
        return ERR_ARGV_2;

    if ( (cpuCoreCnt = sysconf(_SC_NPROCESSORS_ONLN)) <= 0 )
    {
        printf("%s() call sysconf(_SC_NPROCESSORS_ONLN) returned invalid online CPU count %ld.\r\n", __FUNCTION__, cpuCoreCnt);
        return ERR_PTHREAD_CFG;
    }

    for (i = 0; i < threadCnt; i++)
    {
        if (threadTidAry[i] <= 0)
            return ERR_ARGV_1;

        CPU_ZERO(&mask);
        cpuCoreId = i % cpuCoreCnt;
        CPU_SET(cpuCoreId, &mask);
        if (sched_setaffinity(threadTidAry[i], sizeof(mask), &mask) != RET_SUCCESS)
        {
            printf("%s() call sched_setaffinity() thread tid %d with CPU core %d failed: %s\r\n", __FUNCTION__, threadTidAry[i], cpuCoreId, strerror(errno));
            return ERR_PTHREAD_CFG;
        }
    }

    return RET_SUCCESS;
}
