#include "rt_utils.h"
#include "dl_syscalls.h"

unsigned long 
timespec_to_usec(struct timespec *ts)
{
	return round((ts->tv_sec * 1E9 + ts->tv_nsec) / 1000.0);
}

long 
timespec_to_lusec(struct timespec *ts)
{
	return round((ts->tv_sec * 1E9 + ts->tv_nsec) / 1000.0);
}

__u64
timespec_to_nsec(struct timespec *ts)
{
	return round(ts->tv_sec * 1E9 + ts->tv_nsec);
}

struct timespec 
usec_to_timespec(unsigned long usec)
{
	struct timespec ts;

	ts.tv_sec = usec / 1000000;
	ts.tv_nsec = (usec % 1000000) * 1000;
	
	return ts;
}

struct timespec 
nsec_to_timespec(unsigned long long nsec)
{
	struct timespec ts;

	ts.tv_sec = nsec / 1000000000;
	ts.tv_nsec = (nsec % 1000000000);
	
	return ts;
}

struct timespec 
timespec_add(struct timespec *t1, struct timespec *t2)
{
	struct timespec ts;

	ts.tv_sec = t1->tv_sec + t2->tv_sec;
	ts.tv_nsec = t1->tv_nsec + t2->tv_nsec;

	while (ts.tv_nsec >= 1E9) {
		ts.tv_nsec -= 1E9;
		ts.tv_sec++;
	}

	return ts;
}

struct timespec 
timespec_sub(struct timespec *t1, struct timespec *t2)
{
	struct timespec ts;
	
	if (t1->tv_nsec < t2->tv_nsec) {
		ts.tv_sec = t1->tv_sec - t2->tv_sec -1;
		ts.tv_nsec = t1->tv_nsec  + 1000000000 - t2->tv_nsec; 
	} else {
		ts.tv_sec = t1->tv_sec - t2->tv_sec;
		ts.tv_nsec = t1->tv_nsec - t2->tv_nsec; 
	}

	return ts;

}

int 
timespec_lower(struct timespec *what, struct timespec *than)
{
	if (what->tv_sec > than->tv_sec)
		return 0;

	if (what->tv_sec < than->tv_sec)
		return 1;

	if (what->tv_nsec < than->tv_nsec)
		return 1;

	return 0;
}

/* set the calling thread to use SCHED_FIFO policy */
void set_prio(int prio)
{
	struct sched_attr attr;
	int ret;
	unsigned int flags = 0;

	attr.size = sizeof(attr);
	attr.sched_flags = 0;
	attr.sched_nice = 0;
	attr.sched_priority = prio;

	attr.sched_policy = SCHED_FIFO;

	ret = sched_setattr(0, &attr, flags);
	if (ret < 0) {
		perror("sched_setattr");
		exit(-1);
	}
}

/* set the calling thread to use SCHED_DEADLINE policy */
void set_deadline(unsigned int runtime,
			unsigned int deadline,
			unsigned int period)
{
	struct sched_attr attr;
	int ret;
	unsigned int flags = 0;

	attr.size = sizeof(attr);
	attr.sched_flags = 0;
	attr.sched_nice = 0;
	attr.sched_priority = 0;

	attr.sched_policy = SCHED_DEADLINE;
	attr.sched_runtime = runtime * NSEC_PER_MSEC;
	attr.sched_period = period * NSEC_PER_MSEC;
	attr.sched_deadline = deadline * NSEC_PER_MSEC;

	ret = sched_setattr(0, &attr, flags);
	if (ret < 0) {
		perror("sched_setattr");
		exit(-1);
	}
}
