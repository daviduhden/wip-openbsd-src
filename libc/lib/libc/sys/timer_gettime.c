/*	$OpenBSD: timer_gettime.c,v 1.7 2015/09/12 13:13:34 guenther Exp $ */

#include <errno.h>
#include <time.h>

int timer_gettime(timer_t, struct itimerspec *);
PROTO_DEPRECATED(timer_gettime);

int
timer_gettime(timer_t timerid, struct itimerspec *value)
{
	errno = ENOSYS;
	return -1;
}
