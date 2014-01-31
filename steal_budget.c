#define _GNU_SOURCE
#include <sched.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <math.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include "rt-app_utils.h"
#include "libdl/dl_syscalls.h"

#define NSEC_PER_MSEC 1000000U
#define NSEC_PER_SEC  1000000000U
#define RTIME_URUN 90
#define NRUN 5
#define MAX_THREADS 2 
#define gettid() syscall(__NR_gettid)

int trace_fd = -1;
int marker_fd = -1;
int bwi_enabled = 0;

pthread_mutex_t my_mutex;
pthread_mutexattr_t my_mutex_attr;

struct timespec
nsec_to_timespec(__u64 *nsec)
{
        struct timespec ts;

        ts.tv_sec = *nsec / NSEC_PER_SEC;
        ts.tv_nsec = (*nsec % NSEC_PER_SEC);

        return ts;
}

void sighandler()
{
	ftrace_write(marker_fd, "main killed!\n");
	write(trace_fd, "0", 1);
	close(trace_fd);
	close(marker_fd);
	exit(-1);	
}

static inline busywait(__u64 len)
{
	struct timespec t_len, t_now, t_exec, t_step;
	__u64 real_exec = (len / 100) * RTIME_URUN;

	t_len = nsec_to_timespec(&real_exec);
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t_now);
	t_exec = timespec_add(&t_now, &t_len);
	while (1) {
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t_step);
		if (!timespec_lower(&t_step, &t_exec))
			break;
	}
}

void *t_1(void *thread_params) {
	struct sched_param2 dl_params;
	struct timespec t_next, t_period, t_start, t_stop, ran_for,
			t_now, t_crit, t_exec;
	long tid = gettid();
	int retval, i;
	cpu_set_t mask;
	__u64 crit, run1, runtime, deadline, period;

	/*
	 * t_1 should go in budget overflow while in critical section
	 */
	run1 = 8U * NSEC_PER_MSEC;
	crit = 12U * NSEC_PER_MSEC;
	runtime =  run1 + crit + (8U * NSEC_PER_MSEC);
	deadline = 40U * NSEC_PER_MSEC;
	period = deadline;
	t_period = nsec_to_timespec(&period);
	t_crit = nsec_to_timespec(&crit);

	signal(SIGHUP, sighandler);
	signal(SIGINT, sighandler);
	signal(SIGQUIT, sighandler);

	CPU_ZERO(&mask);
	CPU_SET(0, &mask);
	retval = sched_setaffinity(0, sizeof(mask), &mask);
	if (retval) {
		fprintf(stderr, "WARNING: could not set task affinity\n");
		exit(-1);
	}

	memset(&dl_params, 0, sizeof(dl_params));
	dl_params.sched_priority = 0;
	dl_params.sched_runtime = runtime;
	dl_params.sched_deadline = deadline;
	dl_params.sched_period = period;
	ftrace_write(marker_fd, "[thread %ld (t_1)]: setting rt=%llums dl=%llums\n", tid,
	       runtime/NSEC_PER_MSEC,
	       deadline/NSEC_PER_MSEC);
	retval = sched_setscheduler2(0, SCHED_DEADLINE, &dl_params);
	if (retval) {
		fprintf(stderr, "WARNING: could not set SCHED_DEADLINE"
				" policy!\n");
		exit(-1);
	}

	clock_gettime(CLOCK_MONOTONIC, &t_next);
	for (i = 0; i < NRUN; i++) {
		ftrace_write(marker_fd, "[t_1] run starts\n");
		clock_gettime(CLOCK_MONOTONIC, &t_start);
		ftrace_write(marker_fd, "[t_1] exec for %lluns\n", run1);
		busywait(run1);
		ftrace_write(marker_fd, "[t_1] locks mutex\n");
		pthread_mutex_lock(&my_mutex);
		ftrace_write(marker_fd, "[t_1] exec for %lluns\n", crit);
		busywait(crit);
		ftrace_write(marker_fd, "[t_1] unlocks mutex\n");
		pthread_mutex_unlock(&my_mutex);
		clock_gettime(CLOCK_MONOTONIC, &t_stop);
		t_next = timespec_add(&t_next, &t_period);
		ran_for = timespec_sub(&t_stop, &t_start);
		printf("[thread %ld]: run %d for %lluus\n",
			tid,
			i,
			timespec_to_usec(&ran_for));
		ftrace_write(marker_fd, "[t_1] run ends\n");
		clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &t_next, NULL);
	}

	retval = sched_setscheduler2(0, SCHED_OTHER, &dl_params);
	if (retval) {
		fprintf(stderr, "WARNING: could not set SCHED_OTHER"
				"policy!\n");
		exit(-1);
	}
}

void *t_2(void *thread_params) {
	struct sched_param2 dl_params;
	struct timespec t_next, t_period, t_start, t_stop, ran_for;
	long tid = gettid();
	int retval, i;
	cpu_set_t mask;
	__u64 run1, run2, crit, wait, runtime, deadline, period;

	/*
	 * t_2 should become t_1's proxy and donate its budget when
	 * needed.
	 */
	run1 = 4U * NSEC_PER_MSEC;
	crit = 12U * NSEC_PER_MSEC;
	runtime =  run1 + crit - (8U * NSEC_PER_MSEC);
	deadline = 48U * NSEC_PER_MSEC;
	period = deadline;
	t_period = nsec_to_timespec(&period);

	signal(SIGHUP, sighandler);
	signal(SIGINT, sighandler);
	signal(SIGQUIT, sighandler);

	CPU_ZERO(&mask);
	CPU_SET(1, &mask);
	retval = sched_setaffinity(0, sizeof(mask), &mask);
	if (retval) {
		fprintf(stderr, "WARNING: could not set task affinity\n");
		exit(-1);
	}

	memset(&dl_params, 0, sizeof(dl_params));
	dl_params.sched_priority = 0;
	dl_params.sched_runtime = runtime;
	dl_params.sched_deadline = deadline;
	dl_params.sched_period = period;
	ftrace_write(marker_fd, "[thread %ld (t_2)]: setting rt=%llums dl=%llums\n", tid,
	       runtime/NSEC_PER_MSEC,
	       deadline/NSEC_PER_MSEC);
	retval = sched_setscheduler2(0, SCHED_DEADLINE, &dl_params);
	if (retval) {
		fprintf(stderr, "WARNING: could not set SCHED_DEADLINE"
				" policy!\n");
		exit(-1);
	}

	clock_gettime(CLOCK_MONOTONIC, &t_next);
	for (i = 0; i < NRUN; i++) {
		ftrace_write(marker_fd, "[t_2] run starts\n");
		clock_gettime(CLOCK_MONOTONIC, &t_start);
		ftrace_write(marker_fd, "[t_2] exec for %lluns\n", run1);
		busywait(run1);
		ftrace_write(marker_fd, "[t_2] locks mutex\n");
		pthread_mutex_lock(&my_mutex);
		ftrace_write(marker_fd, "[t_2] exec for %lluns\n", crit);
		busywait(crit);
		ftrace_write(marker_fd, "[t_2] unlocks mutex\n");
		pthread_mutex_unlock(&my_mutex);
		clock_gettime(CLOCK_MONOTONIC, &t_stop);
		t_next = timespec_add(&t_next, &t_period);
		ran_for = timespec_sub(&t_stop, &t_start);
		printf("[thread %ld]: run %d for %lluus\n",
			tid,
			i,
			timespec_to_usec(&ran_for));
		ftrace_write(marker_fd, "[t_2] run ends\n");
		clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &t_next, NULL);
	}

	retval = sched_setscheduler2(0, SCHED_OTHER, &dl_params);
	if (retval) {
		fprintf(stderr, "WARNING: could not set SCHED_OTHER"
				"policy!\n");
		exit(-1);
	}
}

int main(int argc, char **argv) {
	struct sched_param2 dl_params;
	int i, retval;
	pthread_t thread[MAX_THREADS];
	char *debugfs = "/debug";
	char path[256];
	long tid = gettid();

	if (argc > 1) {
		printf("[main] MBWI enabled\n");
		bwi_enabled = atoi(argv[1]);
	}

	printf("main opening trace fds\n");
	strcpy(path, debugfs);
	strcat(path, "/tracing/tracing_on");
	trace_fd = open(path, O_WRONLY);
	if (trace_fd < 0) {
		printf("can't open trace_fd!\n");
		exit(-1);
	}

	strcpy(path, debugfs);
	strcat(path, "/tracing/trace_marker");
	marker_fd = open(path, O_WRONLY);
	if (marker_fd < 0) {
		printf("can't open marker_fd!\n");
		exit(-1);
	}

	/* Initialize mutex variable objects */
	pthread_mutexattr_init(&my_mutex_attr);
	if (bwi_enabled)
		pthread_mutexattr_setprotocol(&my_mutex_attr,
					      PTHREAD_PRIO_INHERIT);
	pthread_mutex_init(&my_mutex, &my_mutex_attr);

	printf("[main] creates %d threads\n", MAX_THREADS);
	write(trace_fd, "1", 1);
	ftrace_write(marker_fd, "[main] creates %d threads\n", MAX_THREADS);

	pthread_create(&thread[0], NULL, t_1, NULL);
	pthread_create(&thread[1], NULL, t_2, NULL);

	for (i = 0; i < MAX_THREADS; i++)
		pthread_join(thread[i], NULL);

	write(trace_fd, "0", 1);
	close(trace_fd);
	close(marker_fd);

	printf("[main] exits\n");
	ftrace_write(marker_fd, "[main] exits\n");
}
