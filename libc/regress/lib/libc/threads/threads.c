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
 * ISO C11 <threads.h>: thread creation/joining, plain, recursive and timed
 * mutexes, condition variables (wait, signal, timedwait), call_once and
 * thread-specific storage, including the TSS destructor.
 */

#include <assert.h>
#include <stdatomic.h>
#include <threads.h>
#include <time.h>

#define ITERATIONS 100000

static mtx_t mtx;
static cnd_t cnd;
static int ready;
static int counter;

static int
bump_worker(void *arg)
{
	int i, r;

	for (i = 0; i < ITERATIONS; i++) {
		assert(mtx_lock(&mtx) == thrd_success);
		counter++;
		assert(mtx_unlock(&mtx) == thrd_success);
	}
	assert(mtx_lock(&mtx) == thrd_success);
	ready = 1;
	assert(cnd_signal(&cnd) == thrd_success);
	assert(mtx_unlock(&mtx) == thrd_success);
	r = (int)(long)arg - 50;
	return r;
}

static int
recursive_worker(void *arg)
{
	(void)arg;
	assert(mtx_lock(&mtx) == thrd_success);
	assert(mtx_lock(&mtx) == thrd_success);
	assert(mtx_unlock(&mtx) == thrd_success);
	assert(mtx_unlock(&mtx) == thrd_success);
	return 0;
}

static once_flag once = ONCE_FLAG_INIT;
static atomic_int once_count;

static void
once_func(void)
{
	atomic_fetch_add(&once_count, 1);
}

static tss_t tss_key;
static atomic_int dtor_count;

static void
tss_dtor(void *p)
{
	(void)p;
	atomic_fetch_add(&dtor_count, 1);
}

static int
tss_worker(void *arg)
{
	(void)arg;
	assert(tss_set(tss_key, (void *)0x1234) == thrd_success);
	assert(tss_get(tss_key) == (void *)0x1234);
	return 0;
}

static int
exit_worker(void *arg)
{
	thrd_exit((int)(long)arg);
	return 0; /* not reached */
}

static int
once_worker(void *arg)
{
	(void)arg;
	call_once(&once, once_func);
	return 0;
}

int
main(void)
{
	thrd_t thr, thr2, thr3, thr4;
	int result, i;
	struct timespec ts;

	/* thread create + join + return value */
	assert(mtx_init(&mtx, mtx_plain) == thrd_success);
	assert(cnd_init(&cnd) == thrd_success);
	assert(thrd_create(&thr, bump_worker, (void *)92) == thrd_success);
	assert(mtx_lock(&mtx) == thrd_success);
	while (!ready)
		assert(cnd_wait(&cnd, &mtx) == thrd_success);
	assert(mtx_unlock(&mtx) == thrd_success);
	assert(thrd_join(thr, &result) == thrd_success);
	assert(result == 42);
	assert(counter == ITERATIONS);
	cnd_destroy(&cnd);
	mtx_destroy(&mtx);

	/* recursive mutex */
	assert(mtx_init(&mtx, mtx_recursive) == thrd_success);
	assert(thrd_create(&thr2, recursive_worker, NULL) == thrd_success);
	assert(thrd_join(thr2, NULL) == thrd_success);
	mtx_destroy(&mtx);

	/* trylock and timedlock */
	clock_gettime(CLOCK_REALTIME, &ts);
	assert(mtx_init(&mtx, mtx_timed) == thrd_success);
	assert(mtx_lock(&mtx) == thrd_success);
	assert(mtx_trylock(&mtx) == thrd_busy);
	assert(mtx_unlock(&mtx) == thrd_success);
	assert(mtx_trylock(&mtx) == thrd_success);
	assert(mtx_unlock(&mtx) == thrd_success);
	ts.tv_sec += 1;
	assert(mtx_timedlock(&mtx, &ts) == thrd_success);
	assert(mtx_unlock(&mtx) == thrd_success);
	assert(mtx_lock(&mtx) == thrd_success);
	clock_gettime(CLOCK_REALTIME, &ts);
	assert(mtx_timedlock(&mtx, &ts) == thrd_timedout);
	assert(mtx_unlock(&mtx) == thrd_success);
	mtx_destroy(&mtx);

	/* condition variable timeout */
	assert(mtx_init(&mtx, mtx_plain) == thrd_success);
	assert(cnd_init(&cnd) == thrd_success);
	assert(mtx_lock(&mtx) == thrd_success);
	clock_gettime(CLOCK_REALTIME, &ts);
	ts.tv_sec += 1;
	assert(cnd_timedwait(&cnd, &mtx, &ts) == thrd_timedout);
	assert(mtx_unlock(&mtx) == thrd_success);
	cnd_destroy(&cnd);
	mtx_destroy(&mtx);

	/* call_once runs the routine exactly once */
	call_once(&once, once_func);
	call_once(&once, once_func);
	assert(atomic_load(&once_count) == 1);

	/* thread-specific storage and destructor */
	assert(tss_create(&tss_key, tss_dtor) == thrd_success);
	assert(thrd_create(&thr3, tss_worker, NULL) == thrd_success);
	assert(thrd_join(thr3, NULL) == thrd_success);
	assert(atomic_load(&dtor_count) == 1);
	tss_delete(tss_key);

	/* detach, identity, sleep and yield */
	assert(thrd_create(&thr3, tss_worker, NULL) == thrd_success);
	assert(thrd_detach(thr3) == thrd_success);
	assert(thrd_equal(thrd_current(), thrd_current()));
	ts.tv_sec = 0;
	ts.tv_nsec = 1000000;
	assert(thrd_sleep(&ts, NULL) == 0);
	thrd_yield();

	/* thrd_exit must deliver its value through thrd_join */
	assert(thrd_create(&thr3, exit_worker, (void *)99) == thrd_success);
	assert(thrd_join(thr3, &result) == thrd_success);
	assert(result == 99);

	/* call_once from several threads must run the routine once */
	assert(thrd_create(&thr3, once_worker, NULL) == thrd_success);
	assert(thrd_create(&thr4, once_worker, NULL) == thrd_success);
	assert(thrd_join(thr3, NULL) == thrd_success);
	assert(thrd_join(thr4, NULL) == thrd_success);
	assert(atomic_load(&once_count) == 1);

	/* repeated create/join under stress */
	for (i = 0; i < 2000; i++) {
		assert(thrd_create(&thr3, tss_worker, NULL) == thrd_success);
		assert(thrd_join(thr3, NULL) == thrd_success);
	}

	return 0;
}
