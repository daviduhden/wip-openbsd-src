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
#include <stdint.h>
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

/* Exercise the six real symbols as well as the function-like macros. */
static void
test_atomic_symbols(void)
{
	atomic_flag flag = ATOMIC_FLAG_INIT;
	void (*thread_fence)(memory_order) = atomic_thread_fence;
	void (*signal_fence)(memory_order) = atomic_signal_fence;
	_Bool (*test_set)(volatile atomic_flag *) = atomic_flag_test_and_set;
	_Bool (*test_set_explicit)(volatile atomic_flag *, memory_order) =
	    atomic_flag_test_and_set_explicit;
	void (*clear)(volatile atomic_flag *) = atomic_flag_clear;
	void (*clear_explicit)(volatile atomic_flag *, memory_order) =
	    atomic_flag_clear_explicit;

	assert(!test_set(&flag));
	assert(test_set_explicit(&flag, memory_order_relaxed));
	clear(&flag);
	assert(!test_set_explicit(&flag, memory_order_acquire));
	clear_explicit(&flag, memory_order_release);
	assert(!test_set(&flag));
	clear_explicit(&flag, memory_order_relaxed);
	thread_fence(memory_order_acq_rel);
	signal_fence(memory_order_seq_cst);
}

/* The public atomic aliases must use the library's corresponding types. */
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L
#define ATOMIC_TYPE_ASSERT static_assert
#else
#define ATOMIC_TYPE_ASSERT _Static_assert
#endif
#define CHECK_ATOMIC_TYPE(alias, type) \
	ATOMIC_TYPE_ASSERT(_Generic((alias *)0, _Atomic(type) *: 1, default: 0), \
	    #alias " has the wrong element type")
CHECK_ATOMIC_TYPE(atomic_char16_t, char16_t);
CHECK_ATOMIC_TYPE(atomic_char32_t, char32_t);
CHECK_ATOMIC_TYPE(atomic_wchar_t, wchar_t);
CHECK_ATOMIC_TYPE(atomic_intptr_t, intptr_t);
CHECK_ATOMIC_TYPE(atomic_uintptr_t, uintptr_t);
CHECK_ATOMIC_TYPE(atomic_size_t, size_t);
CHECK_ATOMIC_TYPE(atomic_ptrdiff_t, ptrdiff_t);
CHECK_ATOMIC_TYPE(atomic_intmax_t, intmax_t);
CHECK_ATOMIC_TYPE(atomic_uintmax_t, uintmax_t);
CHECK_ATOMIC_TYPE(atomic_int_least8_t, int_least8_t);
CHECK_ATOMIC_TYPE(atomic_int_least16_t, int_least16_t);
CHECK_ATOMIC_TYPE(atomic_int_least32_t, int_least32_t);
CHECK_ATOMIC_TYPE(atomic_int_least64_t, int_least64_t);
CHECK_ATOMIC_TYPE(atomic_int_fast8_t, int_fast8_t);
CHECK_ATOMIC_TYPE(atomic_int_fast16_t, int_fast16_t);
CHECK_ATOMIC_TYPE(atomic_int_fast32_t, int_fast32_t);
CHECK_ATOMIC_TYPE(atomic_int_fast64_t, int_fast64_t);
CHECK_ATOMIC_TYPE(atomic_uint_least8_t, uint_least8_t);
CHECK_ATOMIC_TYPE(atomic_uint_least16_t, uint_least16_t);
CHECK_ATOMIC_TYPE(atomic_uint_least32_t, uint_least32_t);
CHECK_ATOMIC_TYPE(atomic_uint_least64_t, uint_least64_t);
CHECK_ATOMIC_TYPE(atomic_uint_fast8_t, uint_fast8_t);
CHECK_ATOMIC_TYPE(atomic_uint_fast16_t, uint_fast16_t);
CHECK_ATOMIC_TYPE(atomic_uint_fast32_t, uint_fast32_t);
CHECK_ATOMIC_TYPE(atomic_uint_fast64_t, uint_fast64_t);
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L
CHECK_ATOMIC_TYPE(atomic_char8_t, unsigned char);
#endif
#undef CHECK_ATOMIC_TYPE
#undef ATOMIC_TYPE_ASSERT

int
main(void)
{
	pthread_t th[NTHREADS];
	int i, expected, desired;

	test_atomic_symbols();

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

#if defined(__GNUC__) && !defined(__clang__)
	/* An out-of-array atomic address must not use C pointer arithmetic. */
	{
		int arr[2];
		_Atomic(int *) ap = arr;
		uintptr_t base = (uintptr_t)arr;
		uintptr_t delta = (uintptr_t)8 * sizeof(arr[0]);
		int *old;

		old = atomic_fetch_add(&ap, 8);
		assert(old == arr);
		assert((uintptr_t)atomic_load(&ap) == base + delta);
		old = atomic_fetch_sub(&ap, 8);
		assert((uintptr_t)old == base + delta);
		assert(atomic_load(&ap) == arr);

		old = atomic_fetch_add(&ap, -8);
		assert(old == arr);
		assert((uintptr_t)atomic_load(&ap) == base - delta);
		old = atomic_fetch_sub(&ap, -8);
		assert((uintptr_t)old == base - delta);
		assert(atomic_load(&ap) == arr);
	}
#endif

	/* Bit-field operands undergo the usual integer promotions. */
	{
		struct {
			unsigned int step : 3;
		} bits = { 1 };
		atomic_int value = 0;
		int arr[3];
		_Atomic(int *) ap = arr;
		int *old;

		assert(atomic_fetch_add(&value, bits.step) == 0);
		assert(atomic_load(&value) == 1);
		assert(atomic_fetch_sub(&value, bits.step) == 1);
		assert(atomic_load(&value) == 0);
		old = atomic_fetch_add(&ap, bits.step);
		assert(old == arr);
		assert(atomic_load(&ap) == &arr[1]);
		old = atomic_fetch_sub(&ap, bits.step);
		assert(old == &arr[1]);
		assert(atomic_load(&ap) == arr);
	}

	/* Evaluate pointer-fetch arguments before the RMW, exactly once. */
	{
		int arr[5];
		_Atomic(int *) ap = &arr[0];
		int *old;
		int objects = 0, operands = 0, orders = 0;

		old = atomic_fetch_add_explicit((objects++, &ap),
		    (atomic_store(&ap, &arr[1]), operands++, 1),
		    (orders++, memory_order_seq_cst));
		assert(old == &arr[1]);
		assert(atomic_load(&ap) == &arr[2]);
		assert(objects == 1 && operands == 1 && orders == 1);

		atomic_store(&ap, &arr[4]);
		objects = operands = orders = 0;
		old = atomic_fetch_sub_explicit((objects++, &ap),
		    (atomic_store(&ap, &arr[3]), operands++, 1),
		    (orders++, memory_order_seq_cst));
		assert(old == &arr[3]);
		assert(atomic_load(&ap) == &arr[2]);
		assert(objects == 1 && operands == 1 && orders == 1);
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

	/* Match the query to the operations with rounded atomic storage. */
	{
		struct s3 {
			unsigned char bytes[3];
		};
		_Atomic(struct s3) object;
		struct s3 first = { { 1, 2, 3 } }, value, expected;
		int evaluations = 0;

		assert(atomic_is_lock_free((evaluations++, &object)) ==
		    __atomic_always_lock_free(sizeof(struct s3), 0));
		assert(evaluations == 1);
		atomic_store(&object, first);
		value = atomic_load(&object);
		assert(memcmp(&value, &first, sizeof(value)) == 0);
		expected = first;
		assert(atomic_compare_exchange_strong(&object, &expected, first));
	}

	/* Volatile aggregates retain qualifiers through the libc fallback. */
	{
		struct volatile_value {
			unsigned char bytes[24];
		};
		volatile _Atomic(struct volatile_value) object;
		struct volatile_value first, second, value, expected;

		memset(&first, 0x12, sizeof(first));
		memset(&second, 0x34, sizeof(second));
		atomic_init(&object, first);
		value = atomic_load(&object);
		assert(memcmp(&value, &first, sizeof(value)) == 0);
		value = atomic_exchange_explicit(&object, second,
		    memory_order_acq_rel);
		assert(memcmp(&value, &first, sizeof(value)) == 0);
		expected = first;
		assert(!atomic_compare_exchange_strong(&object, &expected, first));
		assert(memcmp(&expected, &second, sizeof(expected)) == 0);
		assert(atomic_compare_exchange_strong(&object, &expected, first));
		value = atomic_load_explicit(&object, memory_order_acquire);
		assert(memcmp(&value, &first, sizeof(value)) == 0);
		atomic_store_explicit(&object, second, memory_order_release);
		value = atomic_load(&object);
		assert(memcmp(&value, &second, sizeof(value)) == 0);
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
