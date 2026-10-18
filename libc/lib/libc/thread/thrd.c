/*	$OpenBSD$	*/

/*-
 * Copyright (c) 2026 David Uhden Collado <david@uhden.dev>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * ISO/IEC 9899:2011 (C11) 7.26: Threads.
 *
 * Thin wrappers over the OpenBSD pthread implementation that lives in libc.
 * pthread_* returns the error number directly, so the return codes are
 * mapped without inspecting errno.  The only substantive work is the
 * trampoline for thrd_create(): a C11 thread start routine returns int,
 * whereas a pthread start routine returns void *, so the start routine and
 * its argument are passed through a small heap block that carries the
 * integer result back as an intptr_t.
 */

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <threads.h>

/*
 * Most of the pthread_* entry points used below are declared
 * PROTO_STD_DEPRECATED in libc/hidden/pthread.h.  That marker is a
 * libc-internal convention meaning "not ordinarily called from inside
 * libc"; it is not a real deprecation.  The ISO C threads wrappers must call
 * them, so the diagnostic is silenced here instead of changing the upstream
 * hidden namespace for the whole library.
 */
#if defined(__clang__)
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#elif defined(__GNUC__)
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

struct _thrd_start {
	thrd_start_t ts_func;
	void	    *ts_arg;
};

static void *
_thrd_trampoline(void *p)
{
	struct _thrd_start *start = p;
	thrd_start_t	    func = start->ts_func;
	void		   *arg = start->ts_arg;
	int		    result;

	free(start);
	result = func(arg);
	return (void *)(intptr_t)result;
}

static int
_thrd_map(int error)
{
	if (error == 0)
		return thrd_success;
	/* Resource exhaustion must be reported as thrd_nomem. */
	if (error == ENOMEM || error == EAGAIN)
		return thrd_nomem;
	return thrd_error;
}

int
thrd_create(thrd_t *thr, thrd_start_t func, void *arg)
{
	struct _thrd_start *start;
	pthread_t	    thread;
	int		    error;

	if ((start = malloc(sizeof(*start))) == NULL)
		return thrd_nomem;
	start->ts_func = func;
	start->ts_arg = arg;
	error = pthread_create(&thread, NULL, _thrd_trampoline, start);
	if (error != 0) {
		free(start);
		return _thrd_map(error);
	}
	*thr = thread;
	return thrd_success;
}
DEF_WEAK(thrd_create);

int
thrd_equal(thrd_t thr0, thrd_t thr1)
{
	return pthread_equal(thr0, thr1);
}
DEF_WEAK(thrd_equal);

thrd_t
thrd_current(void)
{
	return pthread_self();
}
DEF_WEAK(thrd_current);

int
thrd_sleep(const struct timespec *duration, struct timespec *remaining)
{
	if (nanosleep(duration, remaining) == 0)
		return 0;
	return errno == EINTR ? -1 : -2;
}
DEF_WEAK(thrd_sleep);

void
thrd_yield(void)
{
	sched_yield();
}
DEF_WEAK(thrd_yield);

__dead void
thrd_exit(int result)
{
	pthread_exit((void *)(intptr_t)result);
}
DEF_WEAK(thrd_exit);

int
thrd_detach(thrd_t thr)
{
	return _thrd_map(pthread_detach(thr));
}
DEF_WEAK(thrd_detach);

int
thrd_join(thrd_t thr, int *result)
{
	void *value;

	if (pthread_join(thr, &value) != 0)
		return thrd_error;
	if (result != NULL)
		*result = (int)(intptr_t)value;
	return thrd_success;
}
DEF_WEAK(thrd_join);

int
mtx_init(mtx_t *mtx, int type)
{
	pthread_mutexattr_t attr;
	int		    error, ptype;

	if ((type & ~(mtx_recursive | mtx_timed)) != 0)
		return thrd_error;
	ptype = (type & mtx_recursive) ? PTHREAD_MUTEX_RECURSIVE :
					 PTHREAD_MUTEX_NORMAL;
	if (pthread_mutexattr_init(&attr) != 0)
		return thrd_error;
	error = pthread_mutexattr_settype(&attr, ptype);
	if (error == 0)
		error = pthread_mutex_init(mtx, &attr);
	pthread_mutexattr_destroy(&attr);
	return _thrd_map(error);
}
DEF_WEAK(mtx_init);

int
mtx_lock(mtx_t *mtx)
{
	return _thrd_map(pthread_mutex_lock(mtx));
}
DEF_WEAK(mtx_lock);

int
mtx_timedlock(mtx_t *__restrict mtx, const struct timespec *__restrict ts)
{
	int error = pthread_mutex_timedlock(mtx, ts);

	if (error == 0)
		return thrd_success;
	return error == ETIMEDOUT ? thrd_timedout : thrd_error;
}
DEF_WEAK(mtx_timedlock);

int
mtx_trylock(mtx_t *mtx)
{
	int error = pthread_mutex_trylock(mtx);

	if (error == 0)
		return thrd_success;
	return error == EBUSY ? thrd_busy : thrd_error;
}
DEF_WEAK(mtx_trylock);

int
mtx_unlock(mtx_t *mtx)
{
	return _thrd_map(pthread_mutex_unlock(mtx));
}
DEF_WEAK(mtx_unlock);

void
mtx_destroy(mtx_t *mtx)
{
	(void)pthread_mutex_destroy(mtx);
}
DEF_WEAK(mtx_destroy);

int
cnd_init(cnd_t *cond)
{
	return _thrd_map(pthread_cond_init(cond, NULL));
}
DEF_WEAK(cnd_init);

int
cnd_signal(cnd_t *cond)
{
	return _thrd_map(pthread_cond_signal(cond));
}
DEF_WEAK(cnd_signal);

int
cnd_broadcast(cnd_t *cond)
{
	return _thrd_map(pthread_cond_broadcast(cond));
}
DEF_WEAK(cnd_broadcast);

int
cnd_wait(cnd_t *cond, mtx_t *mtx)
{
	return _thrd_map(pthread_cond_wait(cond, mtx));
}
DEF_WEAK(cnd_wait);

int
cnd_timedwait(cnd_t *__restrict cond, mtx_t *__restrict mtx,
    const struct timespec *__restrict ts)
{
	int error = pthread_cond_timedwait(cond, mtx, ts);

	if (error == 0)
		return thrd_success;
	return error == ETIMEDOUT ? thrd_timedout : thrd_error;
}
DEF_WEAK(cnd_timedwait);

void
cnd_destroy(cnd_t *cond)
{
	(void)pthread_cond_destroy(cond);
}
DEF_WEAK(cnd_destroy);

int
tss_create(tss_t *key, tss_dtor_t dtor)
{
	return _thrd_map(pthread_key_create(key, dtor));
}
DEF_WEAK(tss_create);

void
tss_delete(tss_t key)
{
	(void)pthread_key_delete(key);
}
DEF_WEAK(tss_delete);

void *
tss_get(tss_t key)
{
	return pthread_getspecific(key);
}
DEF_WEAK(tss_get);

int
tss_set(tss_t key, void *val)
{
	return _thrd_map(pthread_setspecific(key, val));
}
DEF_WEAK(tss_set);

void
call_once(once_flag *flag, void (*func)(void))
{
	(void)pthread_once(flag, func);
}
DEF_WEAK(call_once);
