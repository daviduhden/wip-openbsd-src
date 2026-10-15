/*	$OpenBSD$	*/

/*
 * Host-side functional test for the C23 <stdckdint.h> header.
 * Not part of the OpenBSD build graph; see libc/tests/Makefile.
 */

#include <limits.h>
#include <stdbool.h>
#include <stdckdint.h>
#include <stdio.h>

#ifndef _STDCKDINT_H_
#error "the tree's <stdckdint.h> was not the header that was included"
#endif

static int fails;
static int eval_count;
static int ptr_count;

#define OK(cond)                                                               \
	do {                                                                   \
		if (!(cond)) {                                                 \
			printf("FAIL line %d: %s\n", __LINE__, #cond);         \
			fails++;                                               \
		}                                                              \
	} while (0)

static int
eval_operand(int v)
{
	eval_count++;
	return v;
}

static int *
get_ptr(int *p)
{
	ptr_count++;
	return p;
}

int
main(void)
{
	int i, a = 0;
	unsigned int u;
	long long ll;
	unsigned char uc;
	_Bool overflow;

	OK(!ckd_add(&i, 1, 2) && i == 3);
	OK(!ckd_add(&i, INT_MAX - 1, 1) && i == INT_MAX);
	OK(ckd_add(&i, INT_MAX, 1) && i == INT_MIN);
	OK(!ckd_add(&i, INT_MIN + 1, -1) && i == INT_MIN);
	OK(ckd_add(&i, INT_MIN, -1) && i == INT_MAX);
	OK(ckd_add(&u, UINT_MAX, 1u) && u == 0);

	OK(!ckd_sub(&i, 5, 7) && i == -2);
	OK(!ckd_sub(&i, INT_MAX, INT_MAX) && i == 0);
	OK(ckd_sub(&i, INT_MIN, 1) && i == INT_MAX);

	OK(!ckd_mul(&i, 6, 7) && i == 42);
	OK(ckd_mul(&i, INT_MAX, 2));
	OK(ckd_mul(&i, INT_MIN, -1) && i == INT_MIN);

	OK(!ckd_add(&ll, (long long)INT_MAX, 1LL) &&
	    ll == (long long)INT_MAX + 1);
	OK(ckd_mul(&ll, 1LL << 40, 1LL << 30));
	OK(!ckd_add(&uc, (unsigned char)200, (unsigned char)55) && uc == 255);
	OK(ckd_add(&uc, (unsigned char)200, (unsigned char)56) && uc == 0);

	eval_count = 0;
	OK(!ckd_add(&a, eval_operand(20), 22) && a == 42 && eval_count == 1);
	OK(!ckd_add(&a, 20, eval_operand(22)) && a == 42 && eval_count == 2);
	ptr_count = 0;
	OK(!ckd_add(get_ptr(&a), 1, 1) && a == 2 && ptr_count == 1);

	overflow = ckd_add(&i, INT_MAX, 1);
	OK(overflow == true);

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L
	OK(__STDC_VERSION_STDCKDINT_H__ == 202311L);
#endif

	if (fails == 0)
		printf("stdckdint: all ok\n");
	return fails != 0;
}
