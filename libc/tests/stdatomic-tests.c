/*	$OpenBSD$	*/

/*
 * Host-side functional test for the C11 <stdatomic.h> header, exercising
 * real contention with the host pthread implementation.
 * Not part of the OpenBSD build graph; see libc/tests/Makefile.
 */

#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>

#ifndef _STDATOMIC_H_
#error "the tree's <stdatomic.h> was not the header that was included"
#endif

#define NTHREADS 8
#define ITERATIONS 100000

static atomic_int counter;
static atomic_flag lock = ATOMIC_FLAG_INIT;
static int fails;

#define OK(cond)                                                               \
	do {                                                                   \
		if (!(cond)) {                                                 \
			printf("FAIL line %d: %s\n", __LINE__, #cond);         \
			fails++;                                               \
		}                                                              \
	} while (0)

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

/*
 * Acquire/release publication: the producer writes data[] before a release
 * store to ready; the consumer spins with an acquire load and must observe
 * every element.  Repeated to give the reordering a chance to show up.
 */
#define NPUBLISH 2000

static char published[64];
static atomic_int ready;

static void *
publisher(void *arg)
{
	int i, j;

	(void)arg;
	for (i = 0; i < NPUBLISH; i++) {
		while (atomic_load_explicit(&ready, memory_order_acquire) != 0)
			;
		for (j = 0; j < (int)sizeof(published); j++)
			published[j] = (char)(i + j);
		atomic_store_explicit(&ready, i + 1, memory_order_release);
	}
	return NULL;
}

static void *
consumer(void *arg)
{
	int i, j, seen = 0;

	(void)arg;
	for (i = 0; i < NPUBLISH; i++) {
		while ((seen = atomic_load_explicit(
		            &ready, memory_order_acquire)) == 0)
			;
		for (j = 0; j < (int)sizeof(published); j++) {
			if (published[j] != (char)((seen - 1) + j)) {
				printf("FAIL publish/consume at %d/%d\n", i, j);
				fails++;
				break;
			}
		}
		atomic_store_explicit(&ready, 0, memory_order_release);
	}
	return NULL;
}

/*
 * Store buffering under memory_order_seq_cst: after both threads store to
 * their own flag and then load the other's, at least one load must observe
 * the other store.  A run in which neither does is a defect in the mapping
 * of the ISO C memory orders onto the compiler builtins.
 */
#define NSBITER 20000

static atomic_int sb_x, sb_y;
static pthread_barrier_t sb_barrier;
static int sb_r1, sb_r2;

static void *
sb_thread(void *arg)
{
	int i, me = (int)(long)arg;

	for (i = 0; i < NSBITER; i++) {
		atomic_store_explicit(&sb_x, 0, memory_order_seq_cst);
		atomic_store_explicit(&sb_y, 0, memory_order_seq_cst);
		pthread_barrier_wait(&sb_barrier);
		if (me == 0) {
			atomic_store_explicit(&sb_x, 1, memory_order_seq_cst);
			sb_r1 =
			    atomic_load_explicit(&sb_y, memory_order_seq_cst);
		} else {
			atomic_store_explicit(&sb_y, 1, memory_order_seq_cst);
			sb_r2 =
			    atomic_load_explicit(&sb_x, memory_order_seq_cst);
		}
		pthread_barrier_wait(&sb_barrier);
		if (sb_r1 == 0 && sb_r2 == 0) {
			printf("FAIL store buffering under seq_cst at %d\n", i);
			fails++;
			return NULL;
		}
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

	OK(!test_set(&flag));
	OK(test_set_explicit(&flag, memory_order_relaxed));
	clear(&flag);
	OK(!test_set_explicit(&flag, memory_order_acquire));
	clear_explicit(&flag, memory_order_release);
	OK(!test_set(&flag));
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
	pthread_t pub, con;

	test_atomic_symbols();

	for (i = 0; i < NTHREADS; i++)
		OK(pthread_create(&th[i], NULL, bump, NULL) == 0);
	for (i = 0; i < NTHREADS; i++)
		OK(pthread_join(th[i], NULL) == 0);
	OK(atomic_load(&counter) == NTHREADS * ITERATIONS);

	atomic_store(&counter, 0);
	for (i = 0; i < NTHREADS; i++)
		OK(pthread_create(&th[i], NULL, locked_bump, NULL) == 0);
	for (i = 0; i < NTHREADS; i++)
		OK(pthread_join(th[i], NULL) == 0);
	OK(atomic_load(&counter) == NTHREADS * ITERATIONS);

	/* acquire/release publication */
	OK(pthread_create(&pub, NULL, publisher, NULL) == 0);
	OK(pthread_create(&con, NULL, consumer, NULL) == 0);
	OK(pthread_join(pub, NULL) == 0);
	OK(pthread_join(con, NULL) == 0);

	/* store buffering, seq_cst */
	if (pthread_barrier_init(&sb_barrier, NULL, 2) == 0) {
		OK(pthread_create(&pub, NULL, sb_thread, (void *)0) == 0);
		OK(pthread_create(&con, NULL, sb_thread, (void *)1) == 0);
		OK(pthread_join(pub, NULL) == 0);
		OK(pthread_join(con, NULL) == 0);
		pthread_barrier_destroy(&sb_barrier);
	}

	/* atomics on pointer types */
	{
		int arr[4];
		_Atomic(int *) ap = &arr[0];
		int *old;

		old = atomic_exchange_explicit(
		    &ap, &arr[2], memory_order_acq_rel);
		OK(old == &arr[0]);
		OK(atomic_load(&ap) == &arr[2]);
		old = atomic_fetch_add_explicit(&ap, 1, memory_order_seq_cst);
		OK(old == &arr[2]);
		OK(atomic_load(&ap) == &arr[3]);
		OK(atomic_is_lock_free(&ap));
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
		OK(old == arr);
		OK((uintptr_t)atomic_load(&ap) == base + delta);
		old = atomic_fetch_sub(&ap, 8);
		OK((uintptr_t)old == base + delta);
		OK(atomic_load(&ap) == arr);

		old = atomic_fetch_add(&ap, -8);
		OK(old == arr);
		OK((uintptr_t)atomic_load(&ap) == base - delta);
		old = atomic_fetch_sub(&ap, -8);
		OK((uintptr_t)old == base - delta);
		OK(atomic_load(&ap) == arr);
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

		OK(atomic_fetch_add(&value, bits.step) == 0);
		OK(atomic_load(&value) == 1);
		OK(atomic_fetch_sub(&value, bits.step) == 1);
		OK(atomic_load(&value) == 0);
		old = atomic_fetch_add(&ap, bits.step);
		OK(old == arr);
		OK(atomic_load(&ap) == &arr[1]);
		old = atomic_fetch_sub(&ap, bits.step);
		OK(old == &arr[1]);
		OK(atomic_load(&ap) == arr);
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
		OK(old == &arr[1]);
		OK(atomic_load(&ap) == &arr[2]);
		OK(objects == 1 && operands == 1 && orders == 1);

		atomic_store(&ap, &arr[4]);
		objects = operands = orders = 0;
		old = atomic_fetch_sub_explicit((objects++, &ap),
		    (atomic_store(&ap, &arr[3]), operands++, 1),
		    (orders++, memory_order_seq_cst));
		OK(old == &arr[3]);
		OK(atomic_load(&ap) == &arr[2]);
		OK(objects == 1 && operands == 1 && orders == 1);
	}

	atomic_store(&counter, 10);
	expected = 10;
	desired = 20;
	OK(atomic_compare_exchange_strong(&counter, &expected, desired));
	OK(atomic_load(&counter) == 20);
	expected = 10;
	OK(!atomic_compare_exchange_strong(&counter, &expected, desired));
	OK(expected == 20);

	OK(atomic_exchange(&counter, 0xff) == 20);
	OK(atomic_fetch_or(&counter, 0x100) == 0xff);
	OK(atomic_fetch_and(&counter, ~0x1) == 0x1ff);
	OK(atomic_fetch_xor(&counter, 0xff) == 0x1fe);

	OK(atomic_is_lock_free(&counter));
	atomic_thread_fence(memory_order_seq_cst);
	atomic_signal_fence(memory_order_acquire);

	/* atomic aggregate load/store/compare-exchange */
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
		OK(w.a == 3 && w.b == 4);
		expect = w;
		v.a = 5;
		v.b = 6;
		OK(atomic_compare_exchange_strong(&var, &expect, v));
		w = atomic_load(&var);
		OK(w.a == 5 && w.b == 6);
	}

	if (fails == 0)
		printf("stdatomic: all ok\n");
	return fails != 0;
}
