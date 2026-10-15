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

/* ISO C23 <stdbit.h>: bit counting, searching and power-of-two helpers. */

#include <assert.h>
#include <limits.h>
#include <stdbit.h>
#include <stdbool.h>

int
main(void)
{
	/* leading / trailing zeros and ones */
	assert(stdc_leading_zeros(0u) == 32);
	assert(stdc_leading_zeros(1u) == 31);
	assert(stdc_leading_zeros(0x80000000u) == 0);
	assert(stdc_leading_zeros_uc((unsigned char)0) == 8);
	assert(stdc_leading_zeros_us((unsigned short)1) == 15);
	assert(stdc_leading_zeros_ull(1ull) == 63);
	assert(stdc_leading_ones(0xffffffffu) == 32);
	assert(stdc_leading_ones(0u) == 0);
	assert(stdc_leading_ones(0xf0000000u) == 4);
	assert(stdc_leading_ones_uc((unsigned char)0xff) == 8);
	assert(stdc_trailing_zeros(0u) == 32);
	assert(stdc_trailing_zeros(1u) == 0);
	assert(stdc_trailing_zeros(0x80u) == 7);
	assert(stdc_trailing_zeros_uc((unsigned char)0) == 8);
	assert(stdc_trailing_ones(0xffu) == 8);
	assert(stdc_trailing_ones(0u) == 0);
	assert(stdc_trailing_ones_ull(~0ull) == 64);

	/* population counts */
	assert(stdc_count_ones(0u) == 0);
	assert(stdc_count_ones(0xfu) == 4);
	assert(stdc_count_zeros(0u) == 32);
	assert(stdc_count_zeros(0xffffffffu) == 0);

	/* first set / clear bit positions are one-based, 0 when absent */
	assert(stdc_first_leading_zero(0u) == 1);
	assert(stdc_first_leading_zero(0xffffffffu) == 0);
	assert(stdc_first_leading_zero(0x7fffffffu) == 1);
	assert(stdc_first_leading_one(0u) == 0);
	assert(stdc_first_leading_one(1u) == 32);
	assert(stdc_first_leading_one(0x80000000u) == 1);
	assert(stdc_first_trailing_zero(0xffffffffu) == 0);
	assert(stdc_first_trailing_zero(0xfffffffeu) == 1);
	assert(stdc_first_trailing_one(0u) == 0);
	assert(stdc_first_trailing_one(1u) == 1);
	assert(stdc_first_trailing_one(4u) == 3);

	/* width and powers of two */
	assert(stdc_bit_width(0u) == 0);
	assert(stdc_bit_width(1u) == 1);
	assert(stdc_bit_width(256u) == 9);
	assert(stdc_bit_floor(0u) == 0);
	assert(stdc_bit_floor(1u) == 1);
	assert(stdc_bit_floor(255u) == 128);
	assert(stdc_bit_floor_uc((unsigned char)200) == 128);
	assert(stdc_bit_ceil(0u) == 1);
	assert(stdc_bit_ceil(1u) == 1);
	assert(stdc_bit_ceil(128u) == 128);
	assert(stdc_bit_ceil(129u) == 256);
	/* An unrepresentable ceiling wraps (the standard leaves it undefined).
	 */
	assert(stdc_bit_ceil_ull(0x8000000000000001ull) == 0);

	assert(stdc_has_single_bit(1u) == true);
	assert(stdc_has_single_bit(2u) == true);
	assert(stdc_has_single_bit(3u) == false);
	assert(stdc_has_single_bit(0u) == false);
	assert(stdc_has_single_bit(0x80000000u) == true);

	/* endianness constants are distinct and native is one of them */
	assert(__STDC_ENDIAN_LITTLE__ != __STDC_ENDIAN_BIG__);
	assert(__STDC_ENDIAN_NATIVE__ == __STDC_ENDIAN_LITTLE__ ||
	       __STDC_ENDIAN_NATIVE__ == __STDC_ENDIAN_BIG__);

	return 0;
}
