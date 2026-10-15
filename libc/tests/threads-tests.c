/*	$OpenBSD$	*/

/*
 * Host-side functional test for the C11 <threads.h> interface and its libc
 * wrappers.  The wrappers from lib/libc/thread/thrd.c are linked in and run
 * on top of the host pthread implementation.
 * Not part of the OpenBSD build graph; see libc/tests/Makefile.
 */

#include <stdio.h>
#include <threads.h>
#include <time.h>

#ifndef _THREADS_H_
#error "the tree's <threads.h> was not the header that was included"
#endif

static int fails;

#define OK(cond)                                                               \
	do {                                                                   \
		if (!(cond)) {                                                 \
			printf("FAIL line %d: %s\n", __LINE__, #cond);         \
			fails++;                                               \
		}                                                              \
	} while (0)

static mtx_t m;
static cnd_t c;
static int ready;
static int counter;

static int
worker(void *arg)
{
	int i;

	for (i = 0; i < 10000; i++) {
		OK(mtx_lock(&m) == thrd_success);
		counter++;
		OK(mtx_unlock(&m) == thrd_success);
	}
	OK(mtx_lock(&m) == thrd_success);
	ready = 1;
	OK(cnd_signal(&c) == thrd_success);
	OK(mtx_unlock(&m) == thrd_success);
	return (int)(long)arg + 1;
}

static int
trivial(void *arg)
{
	return (int)(long)arg;
}

static int
recursive_worker(void *arg)
{
	(void)arg;
	OK(mtx_lock(&m) == thrd_success);
	OK(mtx_lock(&m) == thrd_success);
	OK(mtx_unlock(&m) == thrd_success);
	OK(mtx_unlock(&m) == thrd_success);
	return 0;
}

static once_flag once = ONCE_FLAG_INIT;
static int once_count;

static void
once_fn(void)
{
	once_count++;
}

static tss_t key;

static void
tss_dtor(void *p)
{
	(void)p;
}

static int
tss_worker(void *arg)
{
	(void)arg;
	OK(tss_set(key, (void *)0x1234) == thrd_success);
	OK(tss_get(key) == (void *)0x1234);
	return 0;
}

static int
exit_worker(void *arg)
{
	thrd_exit((int)(long)arg);
	return 0; /* not reached */
}

static once_flag once2 = ONCE_FLAG_INIT;
static int once2_count;

static void
once2_fn(void)
{
	once2_count++;
}

static int
once_worker(void *arg)
{
	(void)arg;
	call_once(&once2, once2_fn);
	return 0;
}

int
main(void)
{
	thrd_t t, t2, t3, t5;
	int res = -1, i;
	struct timespec ts;

	OK(mtx_init(&m, mtx_plain) == thrd_success);
	OK(cnd_init(&c) == thrd_success);
	OK(thrd_create(&t, worker, (void *)41) == thrd_success);
	OK(mtx_lock(&m) == thrd_success);
	while (!ready)
		OK(cnd_wait(&c, &m) == thrd_success);
	OK(mtx_unlock(&m) == thrd_success);
	OK(thrd_join(t, &res) == thrd_success);
	OK(res == 42);
	OK(counter == 10000);
	cnd_destroy(&c);
	mtx_destroy(&m);

	OK(mtx_init(&m, mtx_recursive) == thrd_success);
	OK(thrd_create(&t2, recursive_worker, NULL) == thrd_success);
	OK(thrd_join(t2, NULL) == thrd_success);
	mtx_destroy(&m);

	OK(mtx_init(&m, mtx_plain) == thrd_success);
	OK(mtx_lock(&m) == thrd_success);
	OK(mtx_trylock(&m) == thrd_busy);
	OK(mtx_unlock(&m) == thrd_success);
	OK(mtx_trylock(&m) == thrd_success);
	OK(mtx_unlock(&m) == thrd_success);
	clock_gettime(CLOCK_REALTIME, &ts);
	ts.tv_sec += 1;
	OK(mtx_timedlock(&m, &ts) == thrd_success);
	OK(mtx_unlock(&m) == thrd_success);
	clock_gettime(CLOCK_REALTIME, &ts);
	OK(mtx_lock(&m) == thrd_success);
	OK(mtx_timedlock(&m, &ts) == thrd_timedout);
	OK(mtx_unlock(&m) == thrd_success);
	mtx_destroy(&m);

	OK(cnd_init(&c) == thrd_success);
	OK(mtx_init(&m, mtx_plain) == thrd_success);
	clock_gettime(CLOCK_REALTIME, &ts);
	ts.tv_sec += 1;
	OK(mtx_lock(&m) == thrd_success);
	OK(cnd_timedwait(&c, &m, &ts) == thrd_timedout);
	OK(mtx_unlock(&m) == thrd_success);
	cnd_destroy(&c);
	mtx_destroy(&m);

	call_once(&once, once_fn);
	call_once(&once, once_fn);
	OK(once_count == 1);

	OK(tss_create(&key, tss_dtor) == thrd_success);
	OK(thrd_create(&t3, tss_worker, NULL) == thrd_success);
	OK(thrd_join(t3, NULL) == thrd_success);
	tss_delete(key);

	OK(thrd_create(&t3, trivial, (void *)7) == thrd_success);
	OK(thrd_detach(t3) == thrd_success);
	OK(thrd_equal(thrd_current(), thrd_current()));
	OK(thrd_create(&t5, trivial, (void *)9) == thrd_success);
	OK(thrd_join(t5, &res) == thrd_success);
	OK(res == 9);

	ts.tv_sec = 0;
	ts.tv_nsec = 1000000;
	OK(thrd_sleep(&ts, NULL) == 0);
	thrd_yield();

	/* repeated create/join: ownership and result handling under stress */
	for (i = 0; i < 2000; i++) {
		OK(thrd_create(&t5, trivial, (void *)(long)i) == thrd_success);
		OK(thrd_join(t5, &res) == thrd_success);
		if (res != i) {
			printf("FAIL stress result %d != %d\n", res, i);
			fails++;
			break;
		}
	}

	/* thrd_exit must deliver its value through thrd_join */
	OK(thrd_create(&t5, exit_worker, (void *)99) == thrd_success);
	OK(thrd_join(t5, &res) == thrd_success);
	OK(res == 99);

	/* a run of detached threads must not leak their start blocks */
	for (i = 0; i < 500; i++) {
		OK(thrd_create(&t5, trivial, NULL) == thrd_success);
		OK(thrd_detach(t5) == thrd_success);
	}

	/* call_once from several threads runs the function once */
	OK(thrd_create(&t, once_worker, NULL) == thrd_success);
	OK(thrd_create(&t2, once_worker, NULL) == thrd_success);
	OK(thrd_join(t, NULL) == thrd_success);
	OK(thrd_join(t2, NULL) == thrd_success);
	OK(once2_count == 1);

	if (fails == 0)
		printf("threads: all ok\n");
	return fails != 0;
}
