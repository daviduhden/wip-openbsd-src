/*
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
 * stdckdint-tests.c -- unit tests for compat/stdckdint.h.
 *
 * Built with -DFVWM_STDCKDINT_FORCE_FALLBACK so the fallback
 * implementation is exercised even on hosts whose toolchain ships a
 * native <stdckdint.h>.  xalloc.h is included so that the real consumer
 * of the API (xreallocarray, which only uses size_t) is compiled and
 * exercised against the fallback as well.
 *
 * Boundary values and the "wrapped" stored value are checked for every
 * case; C23 7.20.1 specifies that on overflow *result receives the
 * mathematical result wrapped to its width, which is exactly what the
 * overflow builtins store.
 */
#include <limits.h>
#include <stdbool.h>
#include <stdckdint.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "xalloc.h"

#if !defined(FVWM_COMPAT_STDCKDINT_FALLBACK)
#error "build stdckdint-tests with -DFVWM_STDCKDINT_FORCE_FALLBACK"
#endif

static int failures;

#define CHECK(cond, msg)                                                       \
	do {                                                                   \
		if (!(cond)) {                                                 \
			fprintf(stderr, "FAIL: %s (%s:%d)\n", (msg), __FILE__, \
			    __LINE__);                                         \
			failures++;                                            \
		}                                                              \
	} while (0)

static void
test_add_unsigned(void)
{
	size_t	      sz;
	unsigned      u;
	unsigned long ul;
	uint8_t	      b;

	sz = 99;
	CHECK(!ckd_add(&sz, (size_t)0, (size_t)0), "size_t 0+0 flag");
	CHECK(sz == 0, "size_t 0+0 value");

	sz = 0;
	CHECK(!ckd_add(&sz, (size_t)1, (size_t)1), "size_t 1+1 flag");
	CHECK(sz == 2, "size_t 1+1 value");

	sz = 0;
	CHECK(!ckd_add(&sz, SIZE_MAX, (size_t)0), "size_t MAX+0 flag");
	CHECK(sz == SIZE_MAX, "size_t MAX+0 value");

	sz = 99;
	CHECK(ckd_add(&sz, SIZE_MAX, (size_t)1), "size_t MAX+1 flag");
	CHECK(sz == 0, "size_t MAX+1 wrap");

	u = 99;
	CHECK(ckd_add(&u, UINT_MAX, 1u), "unsigned MAX+1 flag");
	CHECK(u == 0u, "unsigned MAX+1 wrap");

	ul = 99;
	CHECK(ckd_add(&ul, ULONG_MAX, 1ul), "ulong MAX+1 flag");
	CHECK(ul == 0ul, "ulong MAX+1 wrap");

	b = 99;
	CHECK(ckd_add(&b, (uint8_t)255, (uint8_t)1), "uint8_t MAX+1 flag");
	CHECK(b == 0, "uint8_t MAX+1 wrap");
}

static void
test_add_signed(void)
{
	int	    r;
	long	    l;
	long long   ll;
	ptrdiff_t   p;
	signed char c;

	r = 0;
	CHECK(!ckd_add(&r, 0, 0), "int 0+0 flag");
	CHECK(r == 0, "int 0+0 value");

	r = 0;
	CHECK(!ckd_add(&r, 1, 1), "int 1+1 flag");
	CHECK(r == 2, "int 1+1 value");

	r = 0;
	CHECK(!ckd_add(&r, INT_MAX, 0), "int MAX+0 flag");
	CHECK(r == INT_MAX, "int MAX+0 value");

	r = 0;
	CHECK(ckd_add(&r, INT_MAX, 1), "int MAX+1 flag");
	CHECK(r == INT_MIN, "int MAX+1 wrap");

	r = 0;
	CHECK(ckd_add(&r, INT_MIN, -1), "int MIN-1 flag");
	CHECK(r == INT_MAX, "int MIN-1 wrap");

	l = 0;
	CHECK(ckd_add(&l, LONG_MAX, 1l), "long MAX+1 flag");
	CHECK(l == LONG_MIN, "long MAX+1 wrap");

	ll = 0;
	CHECK(ckd_add(&ll, LLONG_MIN, -1ll), "llong MIN-1 flag");
	CHECK(ll == LLONG_MAX, "llong MIN-1 wrap");

	p = 0;
	CHECK(ckd_add(&p, PTRDIFF_MAX, (ptrdiff_t)1), "ptrdiff MAX+1 flag");
	CHECK(p == PTRDIFF_MIN, "ptrdiff MAX+1 wrap");

	/* Result type narrower than the operands. */
	c = 0;
	CHECK(!ckd_add(&c, (signed char)50, (signed char)50), "char fits flag");
	CHECK(c == 100, "char fits value");

	c = 0;
	CHECK(ckd_add(&c, (signed char)100, (signed char)100),
	    "char overflow flag");
	CHECK(c == (signed char)-56, "char overflow wrap");
}

static void
test_sub(void)
{
	size_t	 sz;
	unsigned u;
	int	 r;

	sz = 99;
	CHECK(!ckd_sub(&sz, (size_t)0, (size_t)0), "size_t 0-0 flag");
	CHECK(sz == 0, "size_t 0-0 value");

	sz = 0;
	CHECK(ckd_sub(&sz, (size_t)0, (size_t)1), "size_t 0-1 flag");
	CHECK(sz == SIZE_MAX, "size_t 0-1 wrap");

	sz = 0;
	CHECK(!ckd_sub(&sz, SIZE_MAX, (size_t)0), "size_t MAX-0 flag");
	CHECK(sz == SIZE_MAX, "size_t MAX-0 value");

	u = 99;
	CHECK(ckd_sub(&u, 0u, 1u), "unsigned 0-1 flag");
	CHECK(u == UINT_MAX, "unsigned 0-1 wrap");

	r = 0;
	CHECK(ckd_sub(&r, INT_MIN, 1), "int MIN-1 flag");
	CHECK(r == INT_MAX, "int MIN-1 wrap");

	r = 0;
	CHECK(ckd_sub(&r, INT_MAX, -1), "int MAX--1 flag");
	CHECK(r == INT_MIN, "int MAX--1 wrap");

	r = 0;
	CHECK(!ckd_sub(&r, INT_MIN, INT_MIN), "int MIN-MIN flag");
	CHECK(r == 0, "int MIN-MIN value");
}

static void
test_mul(void)
{
	size_t	 sz;
	int	 r;
	uint8_t	 b;
	unsigned u;

	sz = 99;
	CHECK(!ckd_mul(&sz, (size_t)1, (size_t)1), "size_t 1*1 flag");
	CHECK(sz == 1, "size_t 1*1 value");

	sz = 0;
	CHECK(!ckd_mul(&sz, SIZE_MAX, (size_t)1), "size_t MAX*1 flag");
	CHECK(sz == SIZE_MAX, "size_t MAX*1 value");

	sz = 0;
	CHECK(ckd_mul(&sz, SIZE_MAX, (size_t)2), "size_t MAX*2 flag");
	CHECK(sz == SIZE_MAX - 1, "size_t MAX*2 wrap");

	sz = 99;
	CHECK(!ckd_mul(&sz, (size_t)0, SIZE_MAX), "size_t 0*MAX flag");
	CHECK(sz == 0, "size_t 0*MAX value");

	r = 0;
	CHECK(!ckd_mul(&r, 2, 3), "int 2*3 flag");
	CHECK(r == 6, "int 2*3 value");

	r = 0;
	CHECK(!ckd_mul(&r, -2, 3), "int -2*3 flag");
	CHECK(r == -6, "int -2*3 value");

	r = 0;
	CHECK(ckd_mul(&r, INT_MAX, 2), "int MAX*2 flag");
	CHECK(r == -2, "int MAX*2 wrap");

	r = 0;
	CHECK(ckd_mul(&r, INT_MIN, -1), "int MIN*-1 flag");
	CHECK(r == INT_MIN, "int MIN*-1 wrap");

	r = 99;
	CHECK(!ckd_mul(&r, INT_MIN, 1), "int MIN*1 flag");
	CHECK(r == INT_MIN, "int MIN*1 value");

	b = 99;
	CHECK(ckd_mul(&b, (uint8_t)16, (uint8_t)16), "uint8_t 16*16 flag");
	CHECK(b == 0, "uint8_t 16*16 wrap");

	u = 99;
	CHECK(!ckd_mul(&u, 3u, 4u), "unsigned 3*4 flag");
	CHECK(u == 12u, "unsigned 3*4 value");
}

static void
test_mixed_types(void)
{
	int	 r;
	unsigned u;
	size_t	 sz;

	/* Operands of different signedness/width. */
	r = 99;
	CHECK(!ckd_add(&r, -1, 1u), "mixed -1+1u flag");
	CHECK(r == 0, "mixed -1+1u value");

	u = 0;
	CHECK(ckd_add(&u, -1, 0u), "mixed -1+0u flag");
	CHECK(u == UINT_MAX, "mixed -1+0u wrap");

	sz = 0;
	CHECK(ckd_add(&sz, (int)-1, (size_t)0), "mixed -1+0sz flag");
	CHECK(sz == SIZE_MAX, "mixed -1+0sz wrap");

	/* Result wider than operands. */
	r = 0;
	CHECK(!ckd_add(&r, (signed char)-100, (signed char)-100),
	    "mixed char+char flag");
	CHECK(r == -200, "mixed char+char value");
}

static int bump_count;

static int
bump(void)
{
	return ++bump_count;
}

static void
test_single_evaluation(void)
{
	int r;

	bump_count = 0;
	r = 0;
	CHECK(!ckd_add(&r, bump(), bump()), "single eval flag");
	CHECK(bump_count == 2, "each argument evaluated exactly once");
	CHECK(r == 3, "single eval value");
}

static void
test_xalloc_integration(void)
{
	void *p;

	/* Exercises xreallocarray() -> ckd_mul(&total, nmemb, size). */
	p = xreallocarray(NULL, (size_t)4, (size_t)4);
	CHECK(p != NULL, "xreallocarray succeeds");
	free(p);
}

int
main(void)
{
	test_add_unsigned();
	test_add_signed();
	test_sub();
	test_mul();
	test_mixed_types();
	test_single_evaluation();
	test_xalloc_integration();

	if (failures != 0) {
		fprintf(stderr, "stdckdint-tests: %d failure(s)\n", failures);
		return 1;
	}
	printf("stdckdint-tests: all tests passed\n");
	return 0;
}
