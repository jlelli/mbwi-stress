#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <linux/unistd.h>
#include <sys/syscall.h>
#include <pthread.h>
#include <math.h>
#include <fcntl.h>
#include "rt_utils.h"
#include "misc_utils.h"

static ftrace_data_t ft_data = {
	.debugfs = "/debug",
	.trace_fd = -1,
	.marker_fd = -1,
};

pthread_mutex_t r1;
pthread_mutexattr_t r1_attr;
int mbwi = 0;

static void usage()
{
	printf(
               "test1\n" \
               "USAGE: test1                         - execute with defaults\n" \
               "       test1 [OPTIONS]               - specify options\n" \
               "\n" \
               "options:\n" \
               "    -B                    MBWI enabled\n" \
	       "\n" \
	      );
}

static inline void busywait(struct timespec *to)
{
	struct timespec t_step;
	while (1) {
		clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t_step);
		if (!timespec_lower(&t_step, to))
			break;
	}
}

static void run(struct timespec *exec)
{
	struct timespec t_start, t_exec, t_totexec = *exec;
	
	/* get the start time */
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t_start);

	/* compute finish time for CPUTIME_ID clock */
	t_exec = timespec_add(&t_start, &t_totexec);
	busywait(&t_exec);
}


void *thread_low(void *data)
{
	unsigned int runtime, deadline, period;
	long slack;
	unsigned int ms_run[5], ms_crit[5];
	long int tid;
	struct timespec t_next, t_slack, t_deadline, t_abs_deadline, t_period;
	struct timespec t_start, t_end, t_diff, t_run[5], t_crit[5];

	tid = gettid();
	printf("low thread started [%ld]\n", tid);

	/**
	 * Parameters (9/18 bw):
	 * budget = 10ms
	 * deadline = 18ms
	 * period = 18ms
	 */
	runtime = 10;
	period = deadline = 18;
	
	t_deadline = nsec_to_timespec(deadline * NSEC_PER_MSEC);
	t_period = nsec_to_timespec(period * NSEC_PER_MSEC);

	/* runs for 2 time units */
	ms_run[0] = 2;
	t_run[0] = usec_to_timespec(ms_run[0] * USEC_PER_MSEC);
	/* locks R1 */
	/* executes for 5 time units inside critical section */
	ms_crit[0] = 5;
	t_crit[0] = usec_to_timespec(ms_crit[0] * USEC_PER_MSEC);
	/* unlocks R1 */
	/* runs for 2 time units */
	ms_run[1] = 2;
	t_run[1] = usec_to_timespec(ms_run[1] * USEC_PER_MSEC);

	/* create a reservation */
	set_deadline(runtime, deadline, period);

	clock_gettime(CLOCK_MONOTONIC, &t_next);
	t_abs_deadline = timespec_add(&t_next, &t_deadline);

	log_ftrace(ft_data.marker_fd, "low thread starts\n");
	clock_gettime(CLOCK_MONOTONIC, &t_start);
	run(&t_run[0]);
	clock_gettime(CLOCK_MONOTONIC, &t_end);
	t_diff = timespec_sub(&t_end, &t_start);
	log_ftrace(ft_data.marker_fd, "low thread executed for %ld\n",
					timespec_to_lusec(&t_diff));
	
	log_ftrace(ft_data.marker_fd, "low thread locks R1\n");
	pthread_mutex_lock(&r1);
	clock_gettime(CLOCK_MONOTONIC, &t_start);
	run(&t_crit[0]);
	clock_gettime(CLOCK_MONOTONIC, &t_end);
	pthread_mutex_unlock(&r1);
	t_diff = timespec_sub(&t_end, &t_start);
	log_ftrace(ft_data.marker_fd, "low thread executed for %ld\n",
					timespec_to_lusec(&t_diff));
	log_ftrace(ft_data.marker_fd, "low thread unlocks R1\n");

	clock_gettime(CLOCK_MONOTONIC, &t_start);
	run(&t_run[1]);
	clock_gettime(CLOCK_MONOTONIC, &t_end);
	t_diff = timespec_sub(&t_end, &t_start);
	log_ftrace(ft_data.marker_fd, "low thread executed for %ld\n",
					timespec_to_lusec(&t_diff));

	clock_gettime(CLOCK_MONOTONIC, &t_end);
	t_slack = timespec_sub(&t_abs_deadline, &t_end);
	slack = timespec_to_lusec(&t_slack);
	if (slack < 0) {
		perror("!!!DEADLINE MISS!!!");
		exit(-1);
	}

	log_ftrace(ft_data.marker_fd, "low thread dies\n");
	printf("low thread dies [%ld]\n", gettid());
	return NULL;
}

int main (int argc, char **argv)
{
	pthread_t thread;
	char tmp[PATH_LENGTH];
	int c;

	while((c = getopt(argc, argv, "+Mh")) != -1) {

		switch(c) {
		case '0':
		case 'M':
			mbwi = 1;
			break;
		case 'h':
			usage();
                        return(0);
		default:
			usage();
			return(1);
		}
	}

	/* Initialize mutex variable objects */
	pthread_mutexattr_init(&r1_attr);
	if (mbwi) {
		printf("MBWI enabled\n");
		pthread_mutexattr_setprotocol(&r1_attr,
					      PTHREAD_PRIO_INHERIT);
	}
	pthread_mutex_init(&r1, &r1_attr);

	printf("main thread [%ld]\n", gettid());
	strcpy(tmp, ft_data.debugfs);
	strcat(tmp, "/tracing/tracing_on");
	ft_data.trace_fd = open(tmp, O_WRONLY);
	if (ft_data.trace_fd < 0) {
		printf("Cannot open trace_fd file %s", tmp);
		exit(EXIT_FAILURE);
	}

	strcpy(tmp, ft_data.debugfs);
	strcat(tmp, "/tracing/trace_marker");
	ft_data.marker_fd = open(tmp, O_WRONLY);
	if (ft_data.trace_fd < 0) {
		printf("Cannot open trace_marker file %s", tmp);
		exit(EXIT_FAILURE);
	}

	log_ftrace(ft_data.trace_fd, "1");
	log_ftrace(ft_data.marker_fd, "main creates threads\n");

	pthread_create(&thread, NULL, thread_low, NULL);

	pthread_join(thread, NULL);

	log_ftrace(ft_data.trace_fd, "0");
	close(ft_data.trace_fd);
	close(ft_data.marker_fd);

	printf("main exits [%ld]\n", gettid());

	return 0;
}
