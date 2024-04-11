/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
#ifndef _LINUX_TIME_H
#define _LINUX_TIME_H

#include <time.h>
#include "linux/types.h"
#include "linux/time_types.h"

#ifndef _STRUCT_TIMESPEC
#define _STRUCT_TIMESPEC
struct timespec {
	__kernel_old_time_t	tv_sec;		/* seconds */
	long			tv_nsec;	/* nanoseconds */
};
#endif

struct linux_timeval {
	__kernel_old_time_t	tv_sec;		/* seconds */
	__kernel_suseconds_t	tv_usec;	/* microseconds */
};

struct itimerspec {
	struct timespec it_interval;/* timer period */
	struct timespec it_value;	/* timer expiration */
};

struct itimerval {
	struct linux_timeval it_interval;/* timer interval */
	struct linux_timeval it_value;	/* current value */
};

#if 0
struct timezone {
	int	tz_minuteswest;	/* minutes west of Greenwich */
	int	tz_dsttime;	/* type of dst correction */
};
#endif

/*
 * Names of the interval timers, and structure
 * defining a timer setting:
 */
#define	ITIMER_REAL		0
#define	ITIMER_VIRTUAL		1
#define	ITIMER_PROF		2

/*
 * The IDs of the various system clocks (for POSIX.1b interval timers):
 */
#define CLOCK_REALTIME			0
#define CLOCK_MONOTONIC			1
#define CLOCK_PROCESS_CPUTIME_ID	2
#define CLOCK_THREAD_CPUTIME_ID		3
#define CLOCK_MONOTONIC_RAW		4
#define CLOCK_REALTIME_COARSE		5
#define CLOCK_MONOTONIC_COARSE		6
#define CLOCK_BOOTTIME			7
#define CLOCK_REALTIME_ALARM		8
#define CLOCK_BOOTTIME_ALARM		9
/*
 * The driver implementing this got removed. The clock ID is kept as a
 * place holder. Do not reuse!
 */
#define CLOCK_SGI_CYCLE			10
#define CLOCK_TAI			11

#define MAX_CLOCKS			16
#define CLOCKS_MASK			(CLOCK_REALTIME | CLOCK_MONOTONIC)
#define CLOCKS_MONO			CLOCK_MONOTONIC

/*
 * The various flags for setting POSIX.1b interval timers:
 */
#define TIMER_ABSTIME			0x01

typedef __s64 time64_t;
typedef __u64 timeu64_t;

#if __BITS_PER_LONG == 64
# define timespec64 timespec
#define itimerspec64 itimerspec
#else
struct timespec64 {
        time64_t        tv_sec;                 /* seconds */
        long            tv_nsec;                /* nanoseconds */
};

struct itimerspec64 {
        struct timespec64 it_interval;
        struct timespec64 it_value;
};
#endif

static inline struct timespec timespec64_to_timespec(const struct timespec64 ts64)
{
        struct timespec ret;

        ret.tv_sec = (time_t)ts64.tv_sec;
        ret.tv_nsec = ts64.tv_nsec;
        return ret;
}

static inline struct timespec64 timespec_to_timespec64(const struct timespec ts)
{
        struct timespec64 ret;

        ret.tv_sec = ts.tv_sec;
        ret.tv_nsec = ts.tv_nsec;
        return ret;
}

#endif /* _LINUX_TIME_H */
