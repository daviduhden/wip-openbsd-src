/*	$OpenBSD$	*/

/*
 * Host-side test for the lock-based fallback that <stdatomic.h> uses for
 * atomic objects that are not lock-free (16-byte and other aggregates), so
 * that the compiler's atomic runtime (libatomic) is never needed.
 * Not part of the OpenBSD build graph; see libc/tests/Makefile.
 */

#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>

#ifndef _STDATOMIC_H_
#error "the tree's <stdatomic.h> was not the header that was included"
#endif

static int fails;
#define OK(cond)                                                               \
	do {                                                                   \
		if (!(cond)) {                                                 \
			printf("FAIL line %d: %s\n", __LINE__, #cond);         \
			fails++;                                               \
		}                                                              \
	} while (0)

struct s16 {
	unsigned char b[16];
};
struct s24 {
	unsigned char b[24];
};
struct s40 {
	unsigned char b[40];
};

static _Atomic(struct s16) a16;
static _Atomic(struct s24) a24;
static _Atomic(struct s40) a40;

static void
test_agg(void)
{
	struct s16 v16, w16, e16;
	struct s24 v24, w24, e24;
	struct s40 v40, w40;

	memset(&v16, 0x3c, sizeof(v16));
	atomic_store(&a16, v16);
	w16 = atomic_load(&a16);
	OK(memcmp(&w16, &v16, sizeof(v16)) == 0);
	memset(&e16, 0x00, sizeof(e16));
	OK(!atomic_compare_exchange_strong(&a16, &e16, w16)); /* fails */
	OK(memcmp(&e16, &w16, sizeof(e16)) == 0); /* expected updated */
	memset(&v16, 0xa5, sizeof(v16));
	OK(atomic_compare_exchange_strong(&a16, &e16, v16)); /* succeeds */
	w16 = atomic_load(&a16);
	OK(memcmp(&w16, &v16, sizeof(v16)) == 0);
	memset(&v16, 0x11, sizeof(v16));
	w16 = atomic_exchange(&a16, v16);
	w16 = atomic_load(&a16);
	OK(memcmp(&w16, &v16, sizeof(v16)) == 0);

	memset(&v24, 0x5a, sizeof(v24));
	atomic_store(&a24, v24);
	w24 = atomic_load(&a24);
	OK(memcmp(&w24, &v24, sizeof(v24)) == 0);
	e24 = w24;
	memset(&v24, 0x24, sizeof(v24));
	OK(atomic_compare_exchange_weak(&a24, &e24, v24));
	w24 = atomic_load(&a24);
	OK(memcmp(&w24, &v24, sizeof(v24)) == 0);

	memset(&v40, 0x77, sizeof(v40));
	atomic_store(&a40, v40);
	w40 = atomic_load(&a40);
	OK(memcmp(&w40, &v40, sizeof(v40)) == 0);
}

#if defined(__SIZEOF_INT128__)
__extension__ typedef __int128 i128;
static _Atomic(i128) a128;

static void *
bump128(void *arg)
{
	int i;

	(void)arg;
	for (i = 0; i < 100000; i++)
		atomic_fetch_add_explicit(&a128, 1, memory_order_relaxed);
	return NULL;
}

static void
test_int128(void)
{
	pthread_t th[8];
	int i;

	atomic_store(&a128, 100);
	OK(atomic_fetch_add(&a128, 23) == 100);
	OK(atomic_load(&a128) == 123);
	OK(atomic_fetch_sub(&a128, 23) == 123);
	OK(atomic_load(&a128) == 100);
	atomic_store(&a128, 0x0f);
	OK(atomic_fetch_or(&a128, 0xf0) == 0x0f);
	OK(atomic_load(&a128) == 0xff);
	OK(atomic_fetch_and(&a128, 0x0f) == 0xff);
	OK(atomic_load(&a128) == 0x0f);
	OK(atomic_fetch_xor(&a128, 0xff) == 0x0f);
	OK(atomic_load(&a128) == 0xf0);

	atomic_store(&a128, 0);
	for (i = 0; i < 8; i++)
		OK(pthread_create(&th[i], NULL, bump128, NULL) == 0);
	for (i = 0; i < 8; i++)
		OK(pthread_join(th[i], NULL) == 0);
	OK(atomic_load(&a128) == (i128)800000);
}
#endif

int
main(void)
{
	test_agg();
#if defined(__SIZEOF_INT128__)
	test_int128();
#endif
	if (fails == 0)
		printf("stdatomic fallback: all ok\n");
	return fails != 0;
}
