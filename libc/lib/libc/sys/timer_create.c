/*	$OpenBSD: timer_create.c,v 1.7 2015/09/12 13:13:34 guenther Exp $ */

#include <errno.h>
#include <signal.h>
#include <time.h>

struct sigevent;

int timer_create(clockid_t, struct sigevent *, timer_t *);
PROTO_DEPRECATED(timer_create);

int
timer_create(clockid_t clock_id, struct sigevent *evp, timer_t *timerid)
{
	errno = ENOSYS;
	return -1;
}
