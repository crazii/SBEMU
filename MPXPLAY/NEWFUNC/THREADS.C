//**************************************************************************
//*                     This file is part of the                           *
//*                      Mpxplay - audio player.                           *
//*                  The source code of Mpxplay is                         *
//*        (C) copyright 1998-2016 by PDSoft (Attila Padar)                *
//*                http://mpxplay.sourceforge.net                          *
//*                  email: mpxplay@freemail.hu                            *
//**************************************************************************
//*  This program is distributed in the hope that it will be useful,       *
//*  but WITHOUT ANY WARRANTY; without even the implied warranty of        *
//*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                  *
//*  Please contact with the author (with me) if you want to use           *
//*  or modify this source.                                                *
//**************************************************************************
//function: plattform (in)dependent thread, mutex and timer handling

//#define MPXPLAY_USE_DEBUGF 1
#define MPXPLAY_DEBUG_OUTPUT stdout

#include "mpxplay.h"
#include "newfunc.h"
#if defined(__GNUC__) && !defined(DJGPP) && (defined(PDS_THREADS_POSIX_THREAD) || defined(PDS_THREADS_POSIX_TIMER) || !defined(PDS_THREADS_MUTEX_DEBUG))
 #include <pthread.h>
 #include <time.h>
#endif
#if defined(MPXPLAY_WIN32) //&& !defined(PDS_THREADS_POSIX_THREAD)
#define WIN32_LEAN_AND_MEAN 1
#include <windows.h>
#include <mmsystem.h>
#include <process.h>
#include <aclapi.h>
#endif

#if defined(MPXPLAY_USE_SMP) && !defined(PDS_THREADS_MUTEX_DEBUG)
 #if defined(__GNUC__)
  #define PDS_THREADS_POSIX_MUTEX 1
 #elif defined(MPXPLAY_WIN32)
  #define PDS_THREADS_WIN32_MUTEX 1
 #endif
#endif

#define PDS_THREADS_MUTEX_TIMEOUTMS 2000

#define PDS_THREADS_MUTEX_COUNTER_MASK 0x0FFFFFFF
#define PDS_THREADS_MUTEX_IS_LOCKED    (1<<31)

#if !defined(MPXPLAY_WIN32) && !defined(PDS_THREADS_POSIX_TIMER)
extern volatile unsigned long int08counter;
#endif

#ifdef MPXPLAY_USE_SMP
extern unsigned long mpxplay_programcontrol;
#endif

#if !defined(PDS_THREADS_POSIX_MUTEX) && !defined(PDS_THREADS_WIN32_MUTEX)
static volatile unsigned long mutex_counter;
#endif
#ifdef PDS_THREADS_MUTEX_DEBUG
#define PDS_THREADS_MUTEX_MAX_DEBUG 256
#define PDS_THREADS_MUTEX_MAX_NAMELEN 32

typedef struct pds_threads_mutex_debug_data_s
{
 char *filename;
 char *function_name;
 unsigned int linenum;
 int timeoutms;
}pds_threads_mutex_debug_data_s;

static pds_threads_mutex_debug_data_s threads_mutex_lockfunc_datas[PDS_THREADS_MUTEX_MAX_DEBUG];
#endif

int pds_threads_mutex_new(void **mup)
{
    if(!mup)
        return MPXPLAY_ERROR_MUTEX_ARGS;
#ifdef PDS_THREADS_POSIX_MUTEX
    if(pthread_mutex_init((pthread_mutex_t *)mup, NULL) < 0){
        mpxplay_debugf(MPXPLAY_DEBUG_OUTPUT, "MUTEX create failed!");
        return MPXPLAY_ERROR_MUTEX_CREATE;
    }
#elif PDS_THREADS_WIN32_MUTEX
    *mup = (void *)CreateMutexA(NULL, FALSE, NULL);
    if(!(*mup)) {
        mpxplay_debugf(MPXPLAY_DEBUG_OUTPUT, "MUTEX create failed!");
        return MPXPLAY_ERROR_MUTEX_CREATE;
    }
#else
    mutex_counter++;
    mutex_counter &= PDS_THREADS_MUTEX_COUNTER_MASK;
    if(!mutex_counter)
        mutex_counter++;
    funcbit_smp_pointer_put(*mup, (void *)(mpxp_ptrsize_t)mutex_counter);
#endif
    return MPXPLAY_ERROR_OK;
}

void pds_threads_mutex_del(void **mup)
{
    if(!mup || !*mup)
        return;
#ifdef PDS_THREADS_POSIX_MUTEX
    pthread_mutex_destroy((pthread_mutex_t *)mup);
#elif PDS_THREADS_WIN32_MUTEX
    CloseHandle((HANDLE *)*mup);
#endif
    funcbit_smp_pointer_put(*mup, NULL);
}

#ifdef PDS_THREADS_MUTEX_DEBUG
int pds_threads_mutex_lock_debug(void **mup, int timeoutms, const char *filename, const char *func_name, unsigned int linenum)
{
    unsigned long c, mn;
    if(!mup)
        return MPXPLAY_ERROR_MUTEX_ARGS;
    c = *((unsigned long *)mup);
    if(!c){
        mpxplay_debugf(MPXPLAY_DEBUG_OUTPUT, "%s->%s->%d mutex is not initialized!", pds_getfilename_from_fullname((char *)filename), func_name, linenum);
        return MPXPLAY_ERROR_MUTEX_UNINIT;
    }
    mn = c & PDS_THREADS_MUTEX_COUNTER_MASK;
    if(mn >= PDS_THREADS_MUTEX_MAX_DEBUG)
        mn = 0;
    if(c & PDS_THREADS_MUTEX_IS_LOCKED) {
#ifdef MPXPLAY_WIN32
        if(timeoutms < 0)
            timeoutms = PDS_THREADS_MUTEX_TIMEOUTMS;
        timeoutms /= 10;
        if(!timeoutms) {
            mpxplay_debugf(MPXPLAY_DEBUG_OUTPUT, "(%d) %s->%s LOCK BY %s", mn , pds_getfilename_from_fullname((char *)filename), func_name, threads_mutex_lockfunc_datas[mn].function_name);
            Sleep(0);
            return MPXPLAY_ERROR_MUTEX_LOCKED;
        }
        while(timeoutms && funcbit_test(*((unsigned long *)mup), PDS_THREADS_MUTEX_IS_LOCKED)) { timeoutms--; Sleep(10); }
        if(funcbit_test(*((unsigned long *)mup), PDS_THREADS_MUTEX_IS_LOCKED)) {
            mpxplay_debugf(MPXPLAY_DEBUG_OUTPUT, "(%d) %s->%s->%d ts:%d LOCKED BY %s->%s->%d ts:%d", mn , pds_getfilename_from_fullname((char *)filename), func_name, linenum, timeoutms,
             threads_mutex_lockfunc_datas[mn].filename, threads_mutex_lockfunc_datas[mn].function_name, threads_mutex_lockfunc_datas[mn].linenum, threads_mutex_lockfunc_datas[mn].timeoutms);
            return MPXPLAY_ERROR_MUTEX_LOCKED;
        }
#else
        return MPXPLAY_ERROR_MUTEX_LOCKED;
#endif
    }
    threads_mutex_lockfunc_datas[mn].filename =  pds_getfilename_from_fullname((char *)filename);
    threads_mutex_lockfunc_datas[mn].function_name =  (char *)func_name;
    threads_mutex_lockfunc_datas[mn].linenum = linenum;
    threads_mutex_lockfunc_datas[mn].timeoutms = timeoutms;
    funcbit_smp_enable(*((unsigned long *)mup), PDS_THREADS_MUTEX_IS_LOCKED);
    return MPXPLAY_ERROR_OK;
}

#else // PDS_THREADS_MUTEX_DEBUG

int pds_threads_mutex_lock(void **mup, int timeoutms)
{
    unsigned long c;
#ifndef __DOS__
    int timeout_ms = (timeoutms >= 0)? timeoutms : PDS_THREADS_MUTEX_TIMEOUTMS;
#endif
#ifdef PDS_THREADS_POSIX_MUTEX
    struct timespec abs_time;//, ts_lock = {(timeout_ms / 1000), (timeout_ms % 1000) * 1000000};
#endif
    if(!mup)
        return MPXPLAY_ERROR_MUTEX_ARGS;
    if(!*mup)
        return MPXPLAY_ERROR_MUTEX_UNINIT;
#ifdef PDS_THREADS_POSIX_MUTEX
    clock_gettime(CLOCK_REALTIME , &abs_time);
    abs_time.tv_sec += timeout_ms / 1000;
    abs_time.tv_nsec += (timeout_ms % 1000) * 1000000;
    if(abs_time.tv_nsec >= 1000000000){
        abs_time.tv_sec++;
        abs_time.tv_nsec -= 1000000000;
    }
    return pthread_mutex_timedlock((pthread_mutex_t *)mup, &abs_time);
#elif PDS_THREADS_WIN32_MUTEX
    c = WaitForSingleObject((HANDLE *)*mup, timeout_ms);
    switch (c) {
        case WAIT_OBJECT_0: break;
        default: if(timeout_ms){ mpxplay_debugf(MPXPLAY_DEBUG_OUTPUT, "MUTEX lock failed!");} return MPXPLAY_ERROR_MUTEX_LOCKED;
    }
    return MPXPLAY_ERROR_OK;
#else // PDS_THREADS_WIN32_MUTEX
    c = *((unsigned long *)mup);
    if(!c)
        return MPXPLAY_ERROR_MUTEX_UNINIT;
    if(c & PDS_THREADS_MUTEX_IS_LOCKED) {
#ifdef MPXPLAY_WIN32
        timeout_ms /= 10;
        if(!timeout_ms) {
            Sleep(0);
            return MPXPLAY_ERROR_MUTEX_LOCKED;
        }
        while(timeout_ms && funcbit_test(*((unsigned long *)mup), PDS_THREADS_MUTEX_IS_LOCKED)) { timeout_ms--; Sleep(10); }
        if(!timeout_ms)
#endif // MPXPLAY_WIN32
            return MPXPLAY_ERROR_MUTEX_LOCKED;
    }
    funcbit_smp_enable(*((unsigned long *)mup), PDS_THREADS_MUTEX_IS_LOCKED);
    return MPXPLAY_ERROR_OK;
#endif // PDS_THREADS_WIN32_MUTEX
}
#endif // PDS_THREADS_MUTEX_DEBUG

void pds_threads_mutex_unlock(void **mup)
{
    if(!mup || !*mup)
        return;
#ifdef PDS_THREADS_POSIX_MUTEX
    pthread_mutex_unlock((pthread_mutex_t *)mup);
#elif PDS_THREADS_WIN32_MUTEX
    ReleaseMutex((HANDLE *)*mup);
#else
    funcbit_smp_disable(*((unsigned long *)mup), PDS_THREADS_MUTEX_IS_LOCKED);
#ifdef PDS_THREADS_MUTEX_DEBUG
    {
        unsigned long mn = *((unsigned long *)mup) & PDS_THREADS_MUTEX_COUNTER_MASK;
        if(mn >= PDS_THREADS_MUTEX_MAX_DEBUG)
            mn = 0;
        threads_mutex_lockfunc_datas[mn].filename =  NULL;
        threads_mutex_lockfunc_datas[mn].function_name =  NULL;
        threads_mutex_lockfunc_datas[mn].linenum = 0;
        threads_mutex_lockfunc_datas[mn].timeoutms = 0;
    }
#endif // PDS_THREADS_MUTEX_DEBUG
#endif // PDS_THREADS_WIN32_MUTEX
}

/* ----------------------------------------------------------------------------------- */
#if 0 // unused yet
int pds_threads_cond_new(void **cop)
{
    if(!cop)
        return MPXPLAY_ERROR_MUTEX_ARGS;
#ifdef PDS_THREADS_POSIX_MUTEX
    if(pthread_cond_init((pthread_cond_t *)cop, NULL) < 0){
        mpxplay_debugf(MPXPLAY_DEBUG_OUTPUT, "COND create failed!");
        return MPXPLAY_ERROR_MUTEX_CREATE;
    }
#elif PDS_THREADS_WIN32_MUTEX

#else

#endif
    return MPXPLAY_ERROR_OK;
}

void pds_threads_cond_del(void **cop)
{
    if(!cop)
        return;
#ifdef PDS_THREADS_POSIX_MUTEX
    pthread_cond_destroy((pthread_cond_t *)cop);
#elif PDS_THREADS_WIN32_MUTEX

#else

#endif
}

int pds_threads_cond_wait(void **cop, void **mup)
{
    if(!cop || !mup)
        return MPXPLAY_ERROR_MUTEX_ARGS;
#ifdef PDS_THREADS_POSIX_MUTEX
    return pthread_cond_wait((pthread_cond_t *)cop, (pthread_mutex_t *)mup);
#elif PDS_THREADS_WIN32_MUTEX

#else

#endif
    return MPXPLAY_ERROR_OK;
}

int pds_threads_cond_timedwait(void **cop, void **mup, int timeoutms)
{
    if(!cop || !mup)
        return MPXPLAY_ERROR_MUTEX_ARGS;
#ifdef PDS_THREADS_POSIX_MUTEX
    int timeout_ms = (timeoutms >= 0)? timeoutms : PDS_THREADS_MUTEX_TIMEOUTMS;
//    const time_t timesec = msecs / 1000;
    struct timespec abs_time;//t = {timesec, (msecs - (timesec * 1000)) * 1000};
    clock_gettime(CLOCK_REALTIME , &abs_time);
    abs_time.tv_sec += timeout_ms / 1000;
    abs_time.tv_nsec += (timeout_ms % 1000) * 1000000;
    if(abs_time.tv_nsec >= 1000000000){
        abs_time.tv_sec++;
        abs_time.tv_nsec -= 1000000000;
    }
    return pthread_cond_timedwait((pthread_cond_t *)cop, (pthread_mutex_t *)mup, &abs_time);
#elif PDS_THREADS_WIN32_MUTEX

#else

#endif
    return MPXPLAY_ERROR_OK;
}

int pds_threads_cond_signal(void **cop)
{
    if(!cop)
        return MPXPLAY_ERROR_MUTEX_ARGS;
#ifdef PDS_THREADS_POSIX_MUTEX
    return pthread_cond_signal((pthread_cond_t *)cop);
#elif PDS_THREADS_WIN32_MUTEX

#else

#endif
    return MPXPLAY_ERROR_OK;
}
#endif
/* ----------------------------------------------------------------------------------- */
#if !defined(__DOS__)

#define MPXPLAY_THREAD_RIGHTS (THREAD_TERMINATE|THREAD_SUSPEND_RESUME|THREAD_SET_INFORMATION)
#define MPXPLAY_THREAD_AFFINITY_SINGLECORE_MASK 0x00000001

#define MPXPLAY_THREAD_HT_CORE_SELECT 0 // disable HT on Core0

static unsigned int int08_timer_period = 10;

int pds_threads_get_number_of_processors(void)
{
    int nb_proc = 0;
#if defined(__GNUC__) && defined(__WINPTHREADS_VERSION)
    nb_proc = pthread_num_processors_np();
#endif // TODO: other operating systems
    if(nb_proc < 1)
        nb_proc = 1;
    return nb_proc;
}

void pds_threads_set_singlecore(void)
{
#if defined(MPXPLAY_WIN32)
 #ifdef MPXPLAY_USE_SMP
    if(mpxplay_programcontrol & MPXPLAY_PROGRAMC_DISABLE_SMP)
 #endif
    {
        HANDLE curr_process = GetCurrentProcess();
        if(curr_process)
            SetProcessAffinityMask(curr_process, MPXPLAY_THREAD_AFFINITY_SINGLECORE_MASK);
    }
#endif
}

#if defined(MPXPLAY_THREADS_HYPERTREADING_DISABLE)
#if defined(MPXPLAY_WIN32) && defined(MPXPLAY_GUI_QT) && defined(MPXPLAY_USE_SMP)
static unsigned int CountSetBits(ULONG_PTR bitMask)
{
    DWORD LSHIFT = sizeof(ULONG_PTR) * 8 - 1;
    DWORD bitSetCount = 0;
    ULONG_PTR bitTest = (ULONG_PTR)1 << LSHIFT;
    DWORD i;

    for (i = 0; i <= LSHIFT; ++i)
    {
        bitSetCount += ((bitMask & bitTest)?1:0);
        bitTest/=2;
    }

    return bitSetCount;
}

static WINBOOL pds_threads_get_number_of_threads_per_cores(unsigned int *number_of_cores, unsigned int *number_of_logical_processors)
{
    WINBOOL result = FALSE;
    DWORD returnLength = 0;
    DWORD logicalProcessorCount = 0;
    DWORD processorCoreCount = 0;
    PSYSTEM_LOGICAL_PROCESSOR_INFORMATION Buffer = NULL, ptr = NULL;
    *number_of_cores = 1;
    *number_of_logical_processors = 1;

    do
    {
        result = GetLogicalProcessorInformation(Buffer, &returnLength);
        if(returnLength <= 0)
        {
            result = FALSE;
            goto err_out_th;
        }
        if((result == TRUE) && Buffer)
            break;
        if(Buffer)
            pds_free(Buffer);
        Buffer = pds_calloc(returnLength, sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION));
        if(!Buffer)
            return FALSE;
    }while(TRUE);

    ptr = Buffer;
    returnLength /= sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);
    do
    {
        switch (ptr->Relationship)
        {
        case RelationProcessorCore:
            processorCoreCount++;
            // A hyperthreaded core supplies more than one logical processor.
            logicalProcessorCount += CountSetBits(ptr->ProcessorMask);
            break;
        }
        ptr++;
    }while(--returnLength);

    *number_of_cores = processorCoreCount;
    *number_of_logical_processors = logicalProcessorCount;

err_out_th:
    if(Buffer)
        pds_free(Buffer);

    return result;
}
#endif // defined(MPXPLAY_WIN32) && defined(MPXPLAY_GUI_QT) && defined(MPXPLAY_USE_SMP)

void pds_threads_hyperthreading_disable(void)
{
#if defined(MPXPLAY_WIN32) && defined(MPXPLAY_GUI_QT) && defined(MPXPLAY_USE_SMP)
    if(!(mpxplay_programcontrol & (MPXPLAY_PROGRAMC_DISABLE_SMP|MPXPLAY_PROGRAMC_ENABLE_HYPERTH)))
    {
        unsigned int number_of_cores = 0;
        unsigned int number_of_logical_processors = 0;
        if( pds_threads_get_number_of_threads_per_cores(&number_of_cores, &number_of_logical_processors)
         && (number_of_cores >= 2) && (number_of_logical_processors >= 4)  // FIXME: we disable HT only from 2C/4T CPUs (check the limit)
         && !(number_of_logical_processors % number_of_cores)
        ){
            unsigned int threads_per_core = number_of_logical_processors / number_of_cores;
            if(threads_per_core > 1)
            {
                DWORD_PTR cpu_affinity_mask;
                HANDLE curr_process;
                unsigned int i, j;
#if 0           // disable HT on all core
                cpu_affinity_mask = 0;
                for(i = 0; i < number_of_cores; i++)
                {
                    cpu_affinity_mask |= 1 << (i * threads_per_core);
                }
#else            // disable HT on first core only
                cpu_affinity_mask = 0;
                for(j = 0; j < number_of_cores; j++)
                {
                    for(i = 0; i < ((j == MPXPLAY_THREAD_HT_CORE_SELECT)? 1 : threads_per_core); i++)
                    {
                        cpu_affinity_mask |= (1 << (j * threads_per_core + i));
                    }
                }
#endif
                curr_process = GetCurrentProcess();
                if(curr_process)
                {
                    SetSecurityInfo((HANDLE)curr_process, SE_KERNEL_OBJECT, MPXPLAY_THREAD_RIGHTS, NULL, NULL, NULL, NULL);
                    SetProcessAffinityMask(curr_process, cpu_affinity_mask);
                }
            }
        }
    }
#endif // defined(MPXPLAY_WIN32) && defined(MPXPLAY_GUI_QT) && defined(MPXPLAY_USE_SMP)
}
#endif // defined(MPXPLAY_THREADS_HYPERTREADING_DISABLE)

void pds_threads_thread_set_affinity(mpxp_thread_id_type threadId, mpxp_ptrsize_t affinity_mask)
{
    DWORD_PTR result = 0, k;
    if(threadId && affinity_mask)
    {
        SetSecurityInfo((HANDLE)threadId, SE_KERNEL_OBJECT, (MPXPLAY_THREAD_RIGHTS | THREAD_QUERY_INFORMATION), NULL, NULL, NULL, NULL);
        result = SetThreadAffinityMask((HANDLE)threadId, (DWORD_PTR)affinity_mask);
    }
    k = result;
}

mpxp_thread_id_type pds_threads_thread_create(mpxp_thread_func function, void *arg, mpxp_threadpriority_type priority, mpxp_ptrsize_t affinity)
{
#if defined(PDS_THREADS_POSIX_THREAD)
    pthread_attr_t attr;
    pthread_t thread = 0;

    if(function == NULL) {
        mpxplay_debugf(MPXPLAY_DEBUG_OUTPUT, "thread function error");
    } else if(pthread_attr_init (&attr)) {
        mpxplay_debugf(MPXPLAY_DEBUG_OUTPUT, "thread attribute set error");
    } else if(pthread_attr_setdetachstate (&attr, PTHREAD_CREATE_DETACHED)) {
        mpxplay_debugf(MPXPLAY_DEBUG_OUTPUT, "thread detach state set error");
    } else if(pthread_create (&thread, &attr, function, arg) != 0) {
        thread = 0;
    } else {
        pds_threads_thread_set_priority(thread, priority);
    }

    return (mpxp_thread_id_type)thread;
#elif defined(MPXPLAY_WIN32)
    mpxp_thread_id_type threadId = (mpxp_thread_id_type)_beginthreadex(NULL, 0, function, arg, CREATE_SUSPENDED, NULL);
    if(threadId){
//#ifdef MPXPLAY_USE_SMP
        // if(mpxplay_programcontrol & MPXPLAY_PROGRAMC_DISABLE_SMP) // FIXME: Mpxplay's threads run on the same (1st) core (other threads can run on a different core too)
//#endif
        pds_threads_thread_set_affinity(threadId, affinity);
        pds_threads_thread_set_priority(threadId, priority);
        ResumeThread((HANDLE)threadId);
    }
    return threadId;
#else
    return 0;
#endif
}

void pds_threads_thread_close(mpxp_thread_id_type thread)
{
    if(thread != 0) {
#if defined(PDS_THREADS_POSIX_THREAD)
        pthread_cancel((pthread_t) thread);
#elif defined(MPXPLAY_WIN32)
        TerminateThread((HANDLE)thread,0);
        CloseHandle((HANDLE)thread);
#endif
    }
}

mpxp_thread_id_type pds_threads_threadid_current(void)
{
#if defined(PDS_THREADS_POSIX_THREAD)
    return pthread_self();
#elif defined(MPXPLAY_WIN32)
    return (mpxp_thread_id_type)GetCurrentThreadId();
#else
    return 0;
#endif
}

void pds_threads_thread_set_priority(mpxp_thread_id_type thread, mpxp_threadpriority_type priority)
{
    if(thread != 0) {
#if defined(PDS_THREADS_POSIX_THREAD)
        int policy;
        struct sched_param param;
        if(priority > MPXPLAY_THREAD_PRIORITY_HIGHEST)
            priority = MPXPLAY_THREAD_PRIORITY_HIGHEST;
        pthread_getschedparam(thread, &policy, &param);
        param.sched_priority = (priority + MPXPLAY_THREAD_PRIORITY_RT) * sched_get_priority_max(policy) / (MPXPLAY_THREAD_PRIORITY_RT * 2);
        pthread_setschedparam(thread, policy, &param);
#elif defined(MPXPLAY_WIN32)
        SetThreadPriority((HANDLE)thread, priority);
#endif
    }
}

void pds_threads_thread_suspend(mpxp_thread_id_type thread)
{
    pds_threads_thread_set_priority(thread, MPXPLAY_THREAD_PRIORITY_IDLE);
//    if(thread != 0) {
//#if defined(PDS_THREADS_POSIX_THREAD)
//        pds_threads_thread_set_priority(thread, MPXPLAY_THREAD_PRIORITY_IDLE);
//#elif defined(MPXPLAY_WIN32)
//        SuspendThread((HANDLE)thread);
//#endif
//    }
}

void pds_threads_thread_resume(mpxp_thread_id_type thread, mpxp_threadpriority_type priority)
{
    pds_threads_thread_set_priority(thread, priority);
//    if(thread != 0) {
//#if defined(PDS_THREADS_POSIX_THREAD)
//        pds_threads_thread_set_priority(thread, priority);
//#elif defined(MPXPLAY_WIN32)
//        ResumeThread((HANDLE)thread);
//#endif
//    }
}

/* --------------------------------------------------------------------------------------------------- */
int pds_threads_timer_period_set(unsigned int timer_period_ms)
{
#if defined(MPXPLAY_WIN32)
    TIMECAPS tc;
    timeGetDevCaps(&tc, sizeof(tc));
    if(tc.wPeriodMin < 1)
        tc.wPeriodMin = 1;
    if(timer_period_ms < tc.wPeriodMin)
        timer_period_ms = tc.wPeriodMin;
    timeBeginPeriod(timer_period_ms);
    int08_timer_period = timer_period_ms;
#endif
    return timer_period_ms;
}

void pds_threads_timer_period_reset(unsigned int timer_period_ms)
{
#if defined(MPXPLAY_WIN32)
    timeEndPeriod(timer_period_ms);
#endif
}

#if 0 // unused
unsigned int pds_threads_timer_waitable_create(unsigned int period_ms)
{
    unsigned int ret_handler = 0;
#if defined(MPXPLAY_WIN32)
    HANDLE timer_handle = CreateWaitableTimer(NULL,0,NULL);
    LARGE_INTEGER DueTime;
    if(timer_handle) {
        DueTime.QuadPart = -(period_ms * 10000);
        SetWaitableTimer(timer_handle, &DueTime, period_ms, NULL, NULL, 0);
    }
    ret_handler = (unsigned int)timer_handle;
#endif
    return ret_handler;
}

void pds_threads_timer_waitable_lock(unsigned int handler, unsigned int period_ms)
{
#if defined(MPXPLAY_WIN32)
    if(handler)
        WaitForSingleObject((HANDLE)handler, period_ms);
    else
        Sleep((period_ms + 1) / 2);
#endif
}

void pds_threads_timer_waitable_close(unsigned int handler)
{
#if defined(MPXPLAY_WIN32)
    CancelWaitableTimer((HANDLE)handler);
    CloseHandle((HANDLE)handler);
#endif
}
#endif // 0

#endif // !defined(__DOS__)

int pds_threads_timer_tick_get(void)
{
    int counter;
#if defined(PDS_THREADS_POSIX_TIMER)
    struct timespec res;
    clock_getres(CLOCK_MONOTONIC, &res);
    counter = ((mpxp_int64_t)res.tv_sec * 1000 + res.tv_nsec / 1000) / int08_timer_period;
#elif defined(MPXPLAY_WIN32)
    //LARGE_INTEGER lpPerformanceCount;
    //QueryPerformanceCounter(&lpPerformanceCount);
    //counter = (mpxp_int64_t)lpPerformanceCount;
    counter = GetTickCount() / int08_timer_period;
#else
    counter = int08counter;
#endif
    return counter;
}

void pds_threads_sleep(int timeoutms)
{
#if defined(PDS_THREADS_POSIX_TIMER)
    if(timeoutms == 0)
        sched_yield();
    else {
        const int timeout_sec = timeoutms / 1000;
        const struct timespec interval = {timeout_sec, (timeoutms - (timeout_sec * 1000)) * 1000};
        pthread_delay_np(&interval);
    }
#elif defined(MPXPLAY_WIN32)
    Sleep(timeoutms);
#endif
}
