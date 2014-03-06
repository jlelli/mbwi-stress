#include "misc_utils.h"

void ftrace_write(int mark_fd, const char *fmt, ...)
{
	va_list ap;
	int n, size = BUF_SIZE;
	char *tmp, *ntmp;

	if (mark_fd < 0) {
		printf("invalid mark_fd");
		exit(EXIT_FAILURE);
	}

	if ((tmp = malloc(size)) == NULL) {
		printf("Cannot allocate ftrace buffer");
		exit(EXIT_FAILURE);
	}
	
	while(1) {
		/* Try to print in the allocated space */
		va_start(ap, fmt);
		n = vsnprintf(tmp, BUF_SIZE, fmt, ap);
		va_end(ap);
		/* If it worked return success */
		if (n > -1 && n < size) {
			write(mark_fd, tmp, n);
			free(tmp);
			return;
		}
		/* Else try again with more space */
		if (n > -1)	/* glibc 2.1 */
			size = n+1;
		else		/* glibc 2.0 */
			size *= 2;
		if ((ntmp = realloc(tmp, size)) == NULL) {
			free(tmp);
			printf("Cannot reallocate ftrace buffer");
			exit(EXIT_FAILURE);
		} else {
			tmp = ntmp;
		}
	}

}
