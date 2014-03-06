#ifndef __MISC_UTILS__
#define __MISC_UTILS__
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>

#define PATH_LENGTH 256
#define BUF_SIZE 256

#define log_ftrace(mark_fd, msg, args...)				\
do {									\
    ftrace_write(mark_fd, msg, ##args);					\
} while (0);

typedef struct _ftrace_data_t {
	char *debugfs;
	int trace_fd;
	int marker_fd;
} ftrace_data_t;

void ftrace_write(int mark_fd, const char *fmt, ...);
#endif /* __MISC_UTILS__ */
