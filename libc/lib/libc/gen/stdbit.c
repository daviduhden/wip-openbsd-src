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
 * ISO/IEC 9899:2024 (C23) 7.18: bit and byte utilities, runtime support.
 *
 * These are the external definitions behind the typed functions declared
 * in <stdbit.h>.  The three primitive operations (count leading zeros,
 * count trailing zeros, population count) are implemented with the
 * compiler's clz/ctz/popcount primitives when they exist and with a
 * portable bit-by-bit loop otherwise.  An all-zero operand yields the
 * width, so the primitives are never applied to zero and no shift count is
 * ever out of range.
 *
 * Every derived operation is expressed in terms of the primitives and the
 * width of the operand, so there is a single place per operation where the
 * width is handled.
 */

#include <sys/cdefs.h>

#include <limits.h>
#include <stdbit.h>

/* Number of leading zero bits of x within w bits; x == 0 yields w. */
static __inline unsigned int
__stdc_lz(unsigned long long x, unsigned int w)
{
#if __stdc_has_builtin(__builtin_clzll)
	if (x == 0)
		return w;
	return (unsigned int)__builtin_clzll(x) -
	       ((unsigned int)(sizeof(unsigned long long) * CHAR_BIT) - w);
#else
	unsigned long long m = (unsigned long long)1 << (w - 1);
	unsigned int n = 0;

	while (n < w && (x & m) == 0) {
		n++;
		m >>= 1;
	}
	return n;
#endif
}

/* Number of trailing zero bits of x within w bits; x == 0 yields w. */
static __inline unsigned int
__stdc_tz(unsigned long long x, unsigned int w)
{
#if __stdc_has_builtin(__builtin_ctzll)
	if (x == 0)
		return w;
	return (unsigned int)__builtin_ctzll(x);
#else
	unsigned int n = 0;

	while (n < w && (x & 1) == 0) {
		n++;
		x >>= 1;
	}
	return n;
#endif
}

/* Number of one bits in x. */
static __inline unsigned int
__stdc_co(unsigned long long x)
{
#if __stdc_has_builtin(__builtin_popcountll)
	return (unsigned int)__builtin_popcountll(x);
#else
	unsigned int n = 0;

	while (x != 0) {
		n += (unsigned int)(x & 1);
		x >>= 1;
	}
	return n;
#endif
}

#define __stdc_define(sfx, type)                                               \
	unsigned int stdc_leading_zeros_##sfx(type x)                          \
	{                                                                      \
		return __stdc_lz((unsigned long long)x,                        \
		    (unsigned int)(sizeof(type) * CHAR_BIT));                  \
	}                                                                      \
	unsigned int stdc_trailing_zeros_##sfx(type x)                         \
	{                                                                      \
		return __stdc_tz((unsigned long long)x,                        \
		    (unsigned int)(sizeof(type) * CHAR_BIT));                  \
	}                                                                      \
	unsigned int stdc_leading_ones_##sfx(type x)                           \
	{                                                                      \
		return __stdc_lz((unsigned long long)(type)~x,                 \
		    (unsigned int)(sizeof(type) * CHAR_BIT));                  \
	}                                                                      \
	unsigned int stdc_trailing_ones_##sfx(type x)                          \
	{                                                                      \
		return __stdc_tz((unsigned long long)(type)~x,                 \
		    (unsigned int)(sizeof(type) * CHAR_BIT));                  \
	}                                                                      \
	unsigned int stdc_count_ones_##sfx(type x)                             \
	{                                                                      \
		return __stdc_co((unsigned long long)x);                       \
	}                                                                      \
	unsigned int stdc_count_zeros_##sfx(type x)                            \
	{                                                                      \
		return (unsigned int)(sizeof(type) * CHAR_BIT) -               \
		       stdc_count_ones_##sfx(x);                               \
	}                                                                      \
	unsigned int stdc_bit_width_##sfx(type x)                              \
	{                                                                      \
		return (unsigned int)(sizeof(type) * CHAR_BIT) -               \
		       stdc_leading_zeros_##sfx(x);                            \
	}                                                                      \
	bool stdc_has_single_bit_##sfx(type x)                                 \
	{                                                                      \
		return x != 0 && (x & (type)(x - 1)) == 0;                     \
	}                                                                      \
	unsigned int stdc_first_leading_zero_##sfx(type x)                     \
	{                                                                      \
		unsigned int n = stdc_leading_ones_##sfx(x);                   \
		return n < (unsigned int)(sizeof(type) * CHAR_BIT) ? n + 1     \
		                                                   : 0;        \
	}                                                                      \
	unsigned int stdc_first_leading_one_##sfx(type x)                      \
	{                                                                      \
		unsigned int n = stdc_leading_zeros_##sfx(x);                  \
		return n < (unsigned int)(sizeof(type) * CHAR_BIT) ? n + 1     \
		                                                   : 0;        \
	}                                                                      \
	unsigned int stdc_first_trailing_zero_##sfx(type x)                    \
	{                                                                      \
		unsigned int n = stdc_trailing_ones_##sfx(x);                  \
		return n < (unsigned int)(sizeof(type) * CHAR_BIT) ? n + 1     \
		                                                   : 0;        \
	}                                                                      \
	unsigned int stdc_first_trailing_one_##sfx(type x)                     \
	{                                                                      \
		unsigned int n = stdc_trailing_zeros_##sfx(x);                 \
		return n < (unsigned int)(sizeof(type) * CHAR_BIT) ? n + 1     \
		                                                   : 0;        \
	}                                                                      \
	type stdc_bit_floor_##sfx(type x)                                      \
	{                                                                      \
		if (x == 0)                                                    \
			return 0;                                              \
		return (type)((type)1 << (stdc_bit_width_##sfx(x) - 1));       \
	}                                                                      \
	type stdc_bit_ceil_##sfx(type x)                                       \
	{                                                                      \
		if (x <= 1)                                                    \
			return 1;                                              \
		return (type)((type)2                                          \
		              << (stdc_bit_width_##sfx((type)(x - 1)) - 1));   \
	}

__stdc_define(uc, unsigned char) __stdc_define(us, unsigned short)
    __stdc_define(ui, unsigned int) __stdc_define(ul, unsigned long)
        __stdc_define(ull, unsigned long long)

#undef __stdc_define
