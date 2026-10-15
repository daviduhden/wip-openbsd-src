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
 * ISO C23 <stdckdint.h>: checked integer arithmetic.  The overflow flag and
 * the stored (wrapped) value are checked together, including the exact
 * boundaries of the destination type.
 */

#include <assert.h>
#include <limits.h>
#include <stdbool.h>
#include <stdckdint.h>

static int eval_count;
static int ptr_count;

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
	int i;
	unsigned int u;
	long long ll;
	unsigned char uc;

	/* signed addition, both boundaries */
	assert(!ckd_add(&i, 1, 2) && i == 3);
	assert(!ckd_add(&i, INT_MAX - 1, 1) && i == INT_MAX);
	assert(ckd_add(&i, INT_MAX, 1));
	assert(i == INT_MIN);
	assert(!ckd_add(&i, INT_MIN + 1, -1) && i == INT_MIN);
	assert(ckd_add(&i, INT_MIN, -1));
	assert(i == INT_MAX);
	/* unsigned addition is checked like any other: UINT_MAX + 1 is not
	 * representable in unsigned int, so overflow is reported and the
	 * wrapped value is stored. */
	assert(ckd_add(&u, UINT_MAX, 1u) && u == 0);

	/* subtraction */
	assert(!ckd_sub(&i, 5, 7) && i == -2);
	assert(!ckd_sub(&i, INT_MAX, INT_MAX) && i == 0);
	assert(ckd_sub(&i, INT_MIN, 1));
	assert(i == INT_MAX);

	/* multiplication */
	assert(!ckd_mul(&i, 6, 7) && i == 42);
	assert(ckd_mul(&i, INT_MAX, 2));
	assert(ckd_mul(&i, INT_MIN, -1));
	assert(i == INT_MIN);

	/* mixed widths and signedness */
	assert(!ckd_add(&ll, (long long)INT_MAX, 1LL) &&
	       ll == (long long)INT_MAX + 1);
	assert(ckd_mul(&ll, 1LL << 40, 1LL << 30));
	assert(!ckd_add(&uc, (unsigned char)200, (unsigned char)55));
	assert(uc == 255);
	assert(ckd_add(&uc, (unsigned char)200, (unsigned char)56));
	assert(uc == 0);

	/* each operand and the result pointer are evaluated once */
	{
		int a = 0;
		int *p;

		assert(!ckd_add(&a, eval_operand(20), 22));
		assert(a == 42 && eval_count == 1);
		assert(!ckd_add(&a, 20, eval_operand(22)));
		assert(a == 42 && eval_count == 2);
		p = get_ptr(&a);
		assert(ptr_count == 1);
		assert(!ckd_add(p, 1, 1) && a == 2);
	}

	/* the result is a bool */
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L
	assert(__STDC_VERSION_STDCKDINT_H__ == 202311L);
#endif
	{
		_Bool overflow = ckd_add(&i, INT_MAX, 1);
		assert(overflow == true);
	}

	return 0;
}
