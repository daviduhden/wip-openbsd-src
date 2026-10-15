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
 * ISO C11 <stdatomic.h>: the operations are exercised from several real
 * threads so that the read-modify-write operations and the memory orders
 * see genuine contention, not just a single-threaded simulation.
 */

#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <string.h>

#define NTHREADS 8
#define ITERATIONS 200000

static atomic_int counter;
static atomic_flag lock = ATOMIC_FLAG_INIT;

static void *
bump(void *arg)
{
	int i;

	(void)arg;
	for (i = 0; i < ITERATIONS; i++)
		atomic_fetch_add_explicit(&counter, 1, memory_order_relaxed);
	return NULL;
}

static void *
locked_bump(void *arg)
{
	int i;

	(void)arg;
	for (i = 0; i < ITERATIONS; i++) {
		while (atomic_flag_test_and_set_explicit(
		    &lock, memory_order_acquire))
			;
		counter++;
		atomic_flag_clear_explicit(&lock, memory_order_release);
	}
	return NULL;
}

int
main(void)
{
	pthread_t th[NTHREADS];
	int i, expected, desired;

	for (i = 0; i < NTHREADS; i++)
		assert(pthread_create(&th[i], NULL, bump, NULL) == 0);
	for (i = 0; i < NTHREADS; i++)
		assert(pthread_join(th[i], NULL) == 0);
	assert(atomic_load(&counter) == NTHREADS * ITERATIONS);

	atomic_store(&counter, 0);
	for (i = 0; i < NTHREADS; i++)
		assert(pthread_create(&th[i], NULL, locked_bump, NULL) == 0);
	for (i = 0; i < NTHREADS; i++)
		assert(pthread_join(th[i], NULL) == 0);
	assert(atomic_load(&counter) == NTHREADS * ITERATIONS);

	atomic_store(&counter, 10);
	expected = 10;
	desired = 20;
	assert(atomic_compare_exchange_strong(&counter, &expected, desired));
	assert(atomic_load(&counter) == 20);
	expected = 10;
	assert(!atomic_compare_exchange_strong(&counter, &expected, desired));
	assert(expected == 20);

	assert(atomic_exchange(&counter, 0xff) == 20);
	assert(atomic_fetch_or(&counter, 0x100) == 0xff);
	assert(atomic_fetch_and(&counter, ~0x1) == 0x1ff);
	assert(atomic_fetch_xor(&counter, 0xff) == 0x1fe);

	assert(atomic_is_lock_free(&counter));
	atomic_thread_fence(memory_order_seq_cst);
	atomic_signal_fence(memory_order_acquire);

	/* atomic aggregate load, store and compare-exchange */
	{
		struct pair {
			int a;
			int b;
		};
		_Atomic(struct pair) var;
		struct pair v, w, expect;

		v.a = 1;
		v.b = 2;
		atomic_init(&var, v);
		v.a = 3;
		v.b = 4;
		atomic_store(&var, v);
		w = atomic_load(&var);
		assert(w.a == 3 && w.b == 4);
		expect = w;
		v.a = 5;
		v.b = 6;
		assert(atomic_compare_exchange_strong(&var, &expect, v));
		w = atomic_load(&var);
		assert(w.a == 5 && w.b == 6);
	}

	/* atomic pointer arithmetic is scaled by the pointed-to size */
	{
		int arr[4];
		_Atomic(int *) ap = &arr[0];
		int *old;

		old = atomic_exchange(&ap, &arr[2]);
		assert(old == &arr[0]);
		assert(atomic_load(&ap) == &arr[2]);
		old = atomic_fetch_add(&ap, 1);
		assert(old == &arr[2]);
		assert(atomic_load(&ap) == &arr[3]);
		old = atomic_fetch_sub(&ap, 3);
		assert(old == &arr[3]);
		assert(atomic_load(&ap) == &arr[0]);
		assert(atomic_is_lock_free(&ap));
	}

	/*
	 * A type that is not lock-free is handled by the libc lock-based
	 * fallback, so it links and works without an external atomic runtime.
	 */
	{
		struct big16 {
			unsigned char b[16];
		};
		_Atomic(struct big16) big;
		struct big16 v, w, expect;
		int i;

		for (i = 0; i < 16; i++)
			v.b[i] = (unsigned char)(i + 1);
		atomic_store(&big, v);
		w = atomic_load(&big);
		assert(memcmp(&w, &v, sizeof(v)) == 0);
		expect = w;
		assert(atomic_compare_exchange_strong(&big, &expect, v));
	}

	/* ATOMIC_VAR_INIT was removed in C23. */
#if !(defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L)
	{
		atomic_int var = ATOMIC_VAR_INIT(7);
		assert(atomic_load(&var) == 7);
	}
#endif

	return 0;
}
