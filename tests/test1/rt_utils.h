#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <linux/unistd.h>
#include <sys/syscall.h>
#include <pthread.h>
#include <math.h>
#include "dl_syscalls.h"

#define gettid() syscall(__NR_gettid)
#define USEC_PER_MSEC 1000L
#define NSEC_PER_MSEC USEC_PER_MSEC * 1000

unsigned long timespec_to_usec(struct timespec *ts);
long timespec_to_lusec(struct timespec *ts);
__u64 timespec_to_nsec(struct timespec *ts);
struct timespec usec_to_timespec(unsigned long usec);
struct timespec nsec_to_timespec(unsigned long long nsec);
struct timespec timespec_add(struct timespec *t1, struct timespec *t2);
struct timespec timespec_sub(struct timespec *t1, struct timespec *t2);
int timespec_lower(struct timespec *what, struct timespec *than);

/* set the calling thread to use SCHED_FIFO policy */
void set_prio(int prio);
/* set the calling thread to use SCHED_DEADLINE policy */
void set_deadline(unsigned int runtime, unsigned int deadline, unsigned int period);
