/*	$OpenBSD$	*/

/*
 * Host-side functional test for the C23 <stdbit.h> header.
 * Not part of the OpenBSD build graph; see libc/tests/Makefile.
 */

#include <stdbit.h>
#include <stdio.h>

#ifndef _STDBIT_H_
#error "the tree's <stdbit.h> was not the header that was included"
#endif

/* Defined in stdbit-link-helper.c. */
unsigned int (*stdbit_link_addr_uc(void))(unsigned char);
unsigned long long (*stdbit_link_addr_ull(void))(unsigned long long);

static int fails;

#define CHECK(expr, want)                                                      \
	do {                                                                   \
		unsigned long long g = (unsigned long long)(expr);             \
		unsigned long long w = (unsigned long long)(want);             \
		if (g != w) {                                                  \
			printf("FAIL line %d: %s = %llu, want %llu\n",         \
			    __LINE__, #expr, g, w);                            \
			fails++;                                               \
		}                                                              \
	} while (0)

#define CHECKB(expr, want)                                                     \
	do {                                                                   \
		if (((expr) ? 1 : 0) != ((want) ? 1 : 0)) {                    \
			printf("FAIL line %d: %s\n", __LINE__, #expr);         \
			fails++;                                               \
		}                                                              \
	} while (0)

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L && \
    !defined(__BITINT_MAXWIDTH__)
#error "C23 bit tests require compiler bit-precise integer support"
#endif

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L && \
    defined(__BITINT_MAXWIDTH__)
/* C23 requires bit-precise types at standard/extended integer widths. */
#define CHECK_BITINT(width) \
	do { \
		typedef unsigned _BitInt(width) bit_t; \
		bit_t zero = 0, one = 1, maximum = (bit_t)-1; \
		bit_t top = (bit_t)1 << ((width) - 1); \
		unsigned int bits = (width); \
		int calls = 0; \
		/* Check both the value and the input precision. */ \
		CHECKB(stdc_leading_zeros(zero) == bits, true); \
		CHECKB(stdc_trailing_zeros(zero) == bits, true); \
		CHECKB(stdc_count_ones(zero) == 0, true); \
		CHECKB(stdc_count_zeros(zero) == bits, true); \
		CHECKB(stdc_leading_ones(maximum) == bits, true); \
		CHECKB(stdc_trailing_ones(maximum) == bits, true); \
		CHECKB(stdc_first_leading_zero(maximum) == 0, true); \
		CHECKB(stdc_first_leading_one(zero) == 0, true); \
		CHECKB(stdc_first_trailing_zero(maximum) == 0, true); \
		CHECKB(stdc_first_trailing_one(zero) == 0, true); \
		CHECKB(stdc_first_leading_zero(zero) == 1, true); \
		CHECKB(stdc_first_leading_one(one) == bits, true); \
		CHECKB(stdc_first_trailing_zero(zero) == 1, true); \
		CHECKB(stdc_first_trailing_one(top) == bits, true); \
		CHECKB(stdc_has_single_bit(top), true); \
		CHECKB(stdc_bit_width(top) == bits, true); \
		CHECKB(stdc_bit_floor(top) == top, true); \
		CHECKB(stdc_bit_ceil(top) == top, true); \
		CHECKB(stdc_bit_floor(zero) == zero, true); \
		CHECKB(stdc_bit_ceil(zero) == one, true); \
		CHECKB(stdc_bit_ceil((bit_t)(top - 1)) == top, true); \
		CHECKB(_Generic(stdc_bit_floor(one), bit_t: 1, default: 0), \
		    true); \
		CHECKB(_Generic(stdc_bit_ceil(one), bit_t: 1, default: 0), \
		    true); \
		CHECKB(stdc_count_zeros((calls++, one)) == bits - 1, true); \
		CHECKB(calls == 1, true); \
		CHECKB(stdc_bit_floor((calls++, top)) == top, true); \
		CHECKB(calls == 2, true); \
		CHECKB(stdc_bit_ceil((calls++, top)) == top, true); \
		CHECKB(calls == 3, true); \
	} while (0)

static void
test_bitint(void)
{
	CHECK_BITINT(CHAR_BIT);
#if __BITINT_MAXWIDTH__ >= __SIZEOF_SHORT__ * CHAR_BIT
	CHECK_BITINT(__SIZEOF_SHORT__ * CHAR_BIT);
#endif
#if __BITINT_MAXWIDTH__ >= __SIZEOF_INT__ * CHAR_BIT
	CHECK_BITINT(__SIZEOF_INT__ * CHAR_BIT);
#endif
#if __BITINT_MAXWIDTH__ >= __SIZEOF_LONG__ * CHAR_BIT
	CHECK_BITINT(__SIZEOF_LONG__ * CHAR_BIT);
#endif
#if __BITINT_MAXWIDTH__ >= __SIZEOF_LONG_LONG__ * CHAR_BIT
	CHECK_BITINT(__SIZEOF_LONG_LONG__ * CHAR_BIT);
#endif
#if defined(__SIZEOF_INT128__) && __BITINT_MAXWIDTH__ >= 128
	CHECK_BITINT(128);
#endif
}

#undef CHECK_BITINT
#endif

int
main(void)
{
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L && \
    defined(__BITINT_MAXWIDTH__)
	test_bitint();
#endif
	CHECK(stdc_leading_zeros(0u), 32);
	CHECK(stdc_leading_zeros(1u), 31);
	CHECK(stdc_leading_zeros(0x80000000u), 0);
	CHECK(stdc_leading_zeros_uc((unsigned char)0), 8);
	CHECK(stdc_leading_zeros_us((unsigned short)1), 15);
	CHECK(stdc_leading_zeros_ull(1ull), 63);
	CHECK(stdc_leading_ones(0xffffffffu), 32);
	CHECK(stdc_leading_ones(0u), 0);
	CHECK(stdc_leading_ones(0xf0000000u), 4);
	CHECK(stdc_leading_ones_uc((unsigned char)0xff), 8);
	CHECK(stdc_trailing_zeros(0u), 32);
	CHECK(stdc_trailing_zeros(1u), 0);
	CHECK(stdc_trailing_zeros(0x80u), 7);
	CHECK(stdc_trailing_zeros_uc((unsigned char)0), 8);
	CHECK(stdc_trailing_ones(0xffu), 8);
	CHECK(stdc_trailing_ones(0u), 0);
	CHECK(stdc_trailing_ones_ull(~0ull), 64);
	CHECK(stdc_count_ones(0u), 0);
	CHECK(stdc_count_ones(0xfu), 4);
	CHECK(stdc_count_zeros(0u), 32);
	CHECK(stdc_count_zeros(0xffffffffu), 0);
	CHECK(stdc_first_leading_zero(0u), 1);
	CHECK(stdc_first_leading_zero(0xffffffffu), 0);
	CHECK(stdc_first_leading_zero(0x7fffffffu), 1);
	CHECK(stdc_first_leading_one(0u), 0);
	CHECK(stdc_first_leading_one(1u), 32);
	CHECK(stdc_first_leading_one(0x80000000u), 1);
	CHECK(stdc_first_trailing_zero(0xffffffffu), 0);
	CHECK(stdc_first_trailing_zero(0xfffffffeu), 1);
	CHECK(stdc_first_trailing_one(0u), 0);
	CHECK(stdc_first_trailing_one(1u), 1);
	CHECK(stdc_first_trailing_one(4u), 3);
	CHECK(stdc_bit_width(0u), 0);
	CHECK(stdc_bit_width(1u), 1);
	CHECK(stdc_bit_width(256u), 9);
	CHECK(stdc_bit_floor(0u), 0);
	CHECK(stdc_bit_floor(1u), 1);
	CHECK(stdc_bit_floor(255u), 128);
	CHECK(stdc_bit_floor_uc((unsigned char)200), 128);
	CHECK(stdc_bit_ceil(0u), 1);
	CHECK(stdc_bit_ceil(1u), 1);
	CHECK(stdc_bit_ceil(128u), 128);
	CHECK(stdc_bit_ceil(129u), 256);
	CHECK(stdc_bit_ceil_ull(0x8000000000000001ull), 0);
	CHECKB(stdc_has_single_bit(1u), 1);
	CHECKB(stdc_has_single_bit(2u), 1);
	CHECKB(stdc_has_single_bit(3u), 0);
	CHECKB(stdc_has_single_bit(0u), 0);
	CHECKB(stdc_has_single_bit(0x80000000u), 1);
	CHECK(__STDC_ENDIAN_LITTLE__ != __STDC_ENDIAN_BIG__, 1);
	CHECK(__STDC_ENDIAN_NATIVE__ == __STDC_ENDIAN_LITTLE__ ||
	          __STDC_ENDIAN_NATIVE__ == __STDC_ENDIAN_BIG__,
	    1);

	/* extended 128-bit integer types, if the compiler has them */
#if defined(__SIZEOF_INT128__)
	{
		__extension__ typedef unsigned __int128 u128;
		u128 ux = (u128)1 << 100;

		CHECK(stdc_leading_zeros(ux), 27);
		CHECK(stdc_bit_width(ux), 101);
		CHECK(stdc_count_ones(ux), 1);
		CHECK(stdc_trailing_zeros((u128)1 << 40), 40);
		CHECK(stdc_trailing_ones((u128)-1), 128);
		CHECKB(stdc_has_single_bit(ux), 1);
		if (stdc_bit_floor(ux) != ((u128)1 << 100) ||
		    stdc_bit_floor((u128)0x123456789abcdef0ULL) !=
		        (u128)0x1000000000000000ULL) {
			printf("FAIL 128-bit bit_floor\n");
			fails++;
		}
		if (stdc_bit_ceil((u128)1) != 1 ||
		    stdc_bit_ceil(((u128)1 << 100) - 1) != ((u128)1 << 100)) {
			printf("FAIL 128-bit bit_ceil\n");
			fails++;
		}
	}
#endif

	/*
	 * The typed functions are real external symbols: the address taken
	 * here must equal the address taken in another translation unit.
	 */
	if (stdbit_link_addr_uc() != &stdc_leading_zeros_uc) {
		printf("FAIL: stdc_leading_zeros_uc has no stable address\n");
		fails++;
	}
	if (stdbit_link_addr_ull() != &stdc_bit_floor_ull) {
		printf("FAIL: stdc_bit_floor_ull has no stable address\n");
		fails++;
	}
	CHECK(stdc_leading_zeros_uc(1), 7);

	if (fails == 0)
		printf("stdbit: all ok\n");
	return fails != 0;
}
