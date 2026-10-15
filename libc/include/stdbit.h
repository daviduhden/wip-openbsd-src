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
 * ISO/IEC 9899:2024 (C23) 7.18: bit and byte utilities <stdbit.h>.
 *
 * The typed functions are real libc entry points (see gen/stdbit.c), so
 * that they have external linkage and a stable address as the standard
 * requires, and so that a program that declares its own prototype links
 * against the same symbol.  The type-generic macros use the compiler's
 * __builtin_stdc_* primitives when they exist and otherwise dispatch to the
 * typed functions with _Generic; the derived macros (leading ones, the
 * first-bit searches, bit width, bit floor and bit ceil) are written in
 * terms of the three primitive operations and the width of the argument, so
 * they do not need per-type support.
 *
 * Every helper takes the width of the operation explicitly and treats an
 * all-zero operand as the full width, so no shift is ever out of range and
 * clz/ctz are never applied to zero.  Nothing depends on the endianness or
 * the word size of the machine; the endianness constants come from the
 * compiler's own byte-order macros and __STDC_ENDIAN_NATIVE__ equals exactly
 * one of the other two.
 */

#ifndef _STDBIT_H_
#define _STDBIT_H_

#include <sys/cdefs.h>

#include <limits.h>
#include <stdbool.h>

#define __STDC_VERSION_STDBIT_H__ 202311L

#define __STDC_ENDIAN_LITTLE__ 1234
#define __STDC_ENDIAN_BIG__ 4321

#if !defined(__BYTE_ORDER__) || !defined(__ORDER_LITTLE_ENDIAN__)
#error "<stdbit.h> requires the compiler byte-order macros"
#endif
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define __STDC_ENDIAN_NATIVE__ __STDC_ENDIAN_LITTLE__
#else
#define __STDC_ENDIAN_NATIVE__ __STDC_ENDIAN_BIG__
#endif

#if defined(__has_builtin)
#define __stdc_has_builtin(x) __has_builtin(x)
#else
#define __stdc_has_builtin(x) 0
#endif

__BEGIN_DECLS

unsigned int stdc_leading_zeros_uc(unsigned char);
unsigned int stdc_leading_zeros_us(unsigned short);
unsigned int stdc_leading_zeros_ui(unsigned int);
unsigned int stdc_leading_zeros_ul(unsigned long);
unsigned int stdc_leading_zeros_ull(unsigned long long);
unsigned int stdc_leading_ones_uc(unsigned char);
unsigned int stdc_leading_ones_us(unsigned short);
unsigned int stdc_leading_ones_ui(unsigned int);
unsigned int stdc_leading_ones_ul(unsigned long);
unsigned int stdc_leading_ones_ull(unsigned long long);
unsigned int stdc_trailing_zeros_uc(unsigned char);
unsigned int stdc_trailing_zeros_us(unsigned short);
unsigned int stdc_trailing_zeros_ui(unsigned int);
unsigned int stdc_trailing_zeros_ul(unsigned long);
unsigned int stdc_trailing_zeros_ull(unsigned long long);
unsigned int stdc_trailing_ones_uc(unsigned char);
unsigned int stdc_trailing_ones_us(unsigned short);
unsigned int stdc_trailing_ones_ui(unsigned int);
unsigned int stdc_trailing_ones_ul(unsigned long);
unsigned int stdc_trailing_ones_ull(unsigned long long);
unsigned int stdc_first_leading_zero_uc(unsigned char);
unsigned int stdc_first_leading_zero_us(unsigned short);
unsigned int stdc_first_leading_zero_ui(unsigned int);
unsigned int stdc_first_leading_zero_ul(unsigned long);
unsigned int stdc_first_leading_zero_ull(unsigned long long);
unsigned int stdc_first_leading_one_uc(unsigned char);
unsigned int stdc_first_leading_one_us(unsigned short);
unsigned int stdc_first_leading_one_ui(unsigned int);
unsigned int stdc_first_leading_one_ul(unsigned long);
unsigned int stdc_first_leading_one_ull(unsigned long long);
unsigned int stdc_first_trailing_zero_uc(unsigned char);
unsigned int stdc_first_trailing_zero_us(unsigned short);
unsigned int stdc_first_trailing_zero_ui(unsigned int);
unsigned int stdc_first_trailing_zero_ul(unsigned long);
unsigned int stdc_first_trailing_zero_ull(unsigned long long);
unsigned int stdc_first_trailing_one_uc(unsigned char);
unsigned int stdc_first_trailing_one_us(unsigned short);
unsigned int stdc_first_trailing_one_ui(unsigned int);
unsigned int stdc_first_trailing_one_ul(unsigned long);
unsigned int stdc_first_trailing_one_ull(unsigned long long);
unsigned int stdc_count_zeros_uc(unsigned char);
unsigned int stdc_count_zeros_us(unsigned short);
unsigned int stdc_count_zeros_ui(unsigned int);
unsigned int stdc_count_zeros_ul(unsigned long);
unsigned int stdc_count_zeros_ull(unsigned long long);
unsigned int stdc_count_ones_uc(unsigned char);
unsigned int stdc_count_ones_us(unsigned short);
unsigned int stdc_count_ones_ui(unsigned int);
unsigned int stdc_count_ones_ul(unsigned long);
unsigned int stdc_count_ones_ull(unsigned long long);
bool stdc_has_single_bit_uc(unsigned char);
bool stdc_has_single_bit_us(unsigned short);
bool stdc_has_single_bit_ui(unsigned int);
bool stdc_has_single_bit_ul(unsigned long);
bool stdc_has_single_bit_ull(unsigned long long);
unsigned int stdc_bit_width_uc(unsigned char);
unsigned int stdc_bit_width_us(unsigned short);
unsigned int stdc_bit_width_ui(unsigned int);
unsigned int stdc_bit_width_ul(unsigned long);
unsigned int stdc_bit_width_ull(unsigned long long);
unsigned char stdc_bit_floor_uc(unsigned char);
unsigned short stdc_bit_floor_us(unsigned short);
unsigned int stdc_bit_floor_ui(unsigned int);
unsigned long stdc_bit_floor_ul(unsigned long);
unsigned long long stdc_bit_floor_ull(unsigned long long);
unsigned char stdc_bit_ceil_uc(unsigned char);
unsigned short stdc_bit_ceil_us(unsigned short);
unsigned int stdc_bit_ceil_ui(unsigned int);
unsigned long stdc_bit_ceil_ul(unsigned long);
unsigned long long stdc_bit_ceil_ull(unsigned long long);

__END_DECLS

#ifndef __cplusplus

/*
 * Dispatch over the standard integer types.  Signed and plain char are
 * accepted as a convenience and converted to the corresponding unsigned
 * type.  When the compiler defines a 128-bit integer type, the 128-bit
 * branches are added too, so extended types are handled exactly even on
 * compilers without the __builtin_stdc_* primitives; the default arm still
 * names the widest standard type.
 */
#if defined(__SIZEOF_INT128__) &&                                              \
    (__stdc_has_builtin(__builtin_clzll) || defined(__GNUC__))
__extension__ typedef unsigned __int128 __stdc_uint128;
__extension__ typedef __int128 __stdc_int128;

static __inline unsigned int
__stdc_lz_u128(__stdc_uint128 __x)
{
	unsigned long long __hi = (unsigned long long)(__x >> 64);

	if (__hi != 0)
		return (unsigned int)__builtin_clzll(__hi);
	__hi = (unsigned long long)__x;
	if (__hi != 0)
		return 64U + (unsigned int)__builtin_clzll(__hi);
	return 128U;
}

static __inline unsigned int
__stdc_tz_u128(__stdc_uint128 __x)
{
	unsigned long long __lo = (unsigned long long)__x;

	if (__lo != 0)
		return (unsigned int)__builtin_ctzll(__lo);
	__lo = (unsigned long long)(__x >> 64);
	if (__lo != 0)
		return 64U + (unsigned int)__builtin_ctzll(__lo);
	return 128U;
}

static __inline unsigned int
__stdc_co_u128(__stdc_uint128 __x)
{
	return (unsigned int)__builtin_popcountll((unsigned long long)__x) +
	       (unsigned int)__builtin_popcountll(
	           (unsigned long long)(__x >> 64));
}

#define __stdc_128_types(name128)                                              \
	__stdc_uint128:                                                        \
	name128, __stdc_int128 : name128,
#else
#define __stdc_128_types(name128)
#endif

#define __stdc_generic(name, name128, x)                                       \
	_Generic((x),                                                          \
	    _Bool: name##_uc,                                                  \
	    char: name##_uc,                                                   \
	    signed char: name##_uc,                                            \
	    unsigned char: name##_uc,                                          \
	    short: name##_us,                                                  \
	    unsigned short: name##_us,                                         \
	    int: name##_ui,                                                    \
	    unsigned int: name##_ui,                                           \
	    long: name##_ul,                                                   \
	    unsigned long: name##_ul,                                          \
	    long long: name##_ull,                                             \
	    unsigned long long: name##_ull,                                    \
	    __stdc_128_types(name128) default: name##_ull)(x)

#if __stdc_has_builtin(__builtin_stdc_leading_zeros)
#define __stdc_leading_zeros(x) __builtin_stdc_leading_zeros(x)
#else
#define __stdc_leading_zeros(x)                                                \
	__stdc_generic(stdc_leading_zeros, __stdc_lz_u128, x)
#endif
#if __stdc_has_builtin(__builtin_stdc_trailing_zeros)
#define __stdc_trailing_zeros(x) __builtin_stdc_trailing_zeros(x)
#else
#define __stdc_trailing_zeros(x)                                               \
	__stdc_generic(stdc_trailing_zeros, __stdc_tz_u128, x)
#endif
#if __stdc_has_builtin(__builtin_stdc_count_ones)
#define __stdc_count_ones(x) __builtin_stdc_count_ones(x)
#else
#define __stdc_count_ones(x) __stdc_generic(stdc_count_ones, __stdc_co_u128, x)
#endif

#define __stdc_arg_width(x) ((unsigned int)(sizeof(x) * CHAR_BIT))

#define stdc_leading_zeros(x) __stdc_leading_zeros(x)
#define stdc_trailing_zeros(x) __stdc_trailing_zeros(x)
#define stdc_count_ones(x) __stdc_count_ones(x)
#define stdc_leading_ones(x) __stdc_leading_zeros((__typeof__(x))~(x))
#define stdc_trailing_ones(x) __stdc_trailing_zeros((__typeof__(x))~(x))
#define stdc_count_zeros(x) (__stdc_arg_width(x) - __stdc_count_ones(x))
#define stdc_bit_width(x) (__stdc_arg_width(x) - __stdc_leading_zeros(x))
#define stdc_has_single_bit(x)                                                 \
	__extension__({                                                        \
		__auto_type __stdc_x = (x);                                    \
		(bool)(__stdc_x != 0 && (__stdc_x & (__stdc_x - 1)) == 0);     \
	})

#define stdc_first_leading_zero(x)                                             \
	__extension__({                                                        \
		unsigned int __stdc_n = stdc_leading_ones(x);                  \
		__stdc_n < __stdc_arg_width(x) ? __stdc_n + 1 : 0U;            \
	})
#define stdc_first_leading_one(x)                                              \
	__extension__({                                                        \
		unsigned int __stdc_n = stdc_leading_zeros(x);                 \
		__stdc_n < __stdc_arg_width(x) ? __stdc_n + 1 : 0U;            \
	})
#define stdc_first_trailing_zero(x)                                            \
	__extension__({                                                        \
		unsigned int __stdc_n = stdc_trailing_ones(x);                 \
		__stdc_n < __stdc_arg_width(x) ? __stdc_n + 1 : 0U;            \
	})
#define stdc_first_trailing_one(x)                                             \
	__extension__({                                                        \
		unsigned int __stdc_n = stdc_trailing_zeros(x);                \
		__stdc_n < __stdc_arg_width(x) ? __stdc_n + 1 : 0U;            \
	})

/*
 * bit_floor and bit_ceil yield the type of their argument.  The shift count
 * is always strictly less than the width of that type, so no shift is out
 * of range and no operation has undefined behaviour; a value whose ceiling
 * is not representable wraps modulo 2**width (the standard leaves that
 * case undefined).
 */
#define stdc_bit_floor(x)                                                      \
	__extension__({                                                        \
		__auto_type __stdc_x = (x);                                    \
		(__typeof__(__stdc_x))(__stdc_x == 0                           \
		                           ? 0                                 \
					   : ((__typeof__(__stdc_x))1          \
		                                 << (stdc_bit_width(           \
		                                         __stdc_x) -           \
		                                        1)));                  \
	})
#define stdc_bit_ceil(x)                                                                  \
	__extension__({                                                                   \
		__auto_type __stdc_x = (x);                                               \
		(__typeof__(__stdc_x))(__stdc_x <= 1                                      \
		                           ? 1                                            \
					   : ((__typeof__(__stdc_x))2                     \
		                                 << (stdc_bit_width((                     \
		                                         __typeof__(__stdc_x))(__stdc_x - \
		                                                               1)) -      \
		                                        1)));                             \
	})

#else /* __cplusplus */

/*
 * C++ has no _Generic, so the type-generic interface is provided with
 * overloaded inline functions that forward to the typed C functions.
 * Only the unsigned integer types are overloaded, as the standard
 * specifies for the type-generic macros.
 */
#define __stdc_cxx_ui(bare)                                                    \
	inline unsigned int stdc_##bare(unsigned char __x)                     \
	{                                                                      \
		return stdc_##bare##_uc(__x);                                  \
	}                                                                      \
	inline unsigned int stdc_##bare(unsigned short __x)                    \
	{                                                                      \
		return stdc_##bare##_us(__x);                                  \
	}                                                                      \
	inline unsigned int stdc_##bare(unsigned int __x)                      \
	{                                                                      \
		return stdc_##bare##_ui(__x);                                  \
	}                                                                      \
	inline unsigned int stdc_##bare(unsigned long __x)                     \
	{                                                                      \
		return stdc_##bare##_ul(__x);                                  \
	}                                                                      \
	inline unsigned int stdc_##bare(unsigned long long __x)                \
	{                                                                      \
		return stdc_##bare##_ull(__x);                                 \
	}

__stdc_cxx_ui(leading_zeros) __stdc_cxx_ui(leading_ones)
    __stdc_cxx_ui(trailing_zeros) __stdc_cxx_ui(trailing_ones)
        __stdc_cxx_ui(first_leading_zero) __stdc_cxx_ui(first_leading_one)
            __stdc_cxx_ui(first_trailing_zero) __stdc_cxx_ui(first_trailing_one)
                __stdc_cxx_ui(count_zeros) __stdc_cxx_ui(count_ones)
                    __stdc_cxx_ui(bit_width)
#undef __stdc_cxx_ui

#define __stdc_cxx_bool(bare)                                                  \
	inline bool stdc_##bare(unsigned char __x)                             \
	{                                                                      \
		return stdc_##bare##_uc(__x);                                  \
	}                                                                      \
	inline bool stdc_##bare(unsigned short __x)                            \
	{                                                                      \
		return stdc_##bare##_us(__x);                                  \
	}                                                                      \
	inline bool stdc_##bare(unsigned int __x)                              \
	{                                                                      \
		return stdc_##bare##_ui(__x);                                  \
	}                                                                      \
	inline bool stdc_##bare(unsigned long __x)                             \
	{                                                                      \
		return stdc_##bare##_ul(__x);                                  \
	}                                                                      \
	inline bool stdc_##bare(unsigned long long __x)                        \
	{                                                                      \
		return stdc_##bare##_ull(__x);                                 \
	}

                        __stdc_cxx_bool(has_single_bit)
#undef __stdc_cxx_bool

#define __stdc_cxx_self(bare)                                                  \
	inline unsigned char stdc_##bare(unsigned char __x)                    \
	{                                                                      \
		return stdc_##bare##_uc(__x);                                  \
	}                                                                      \
	inline unsigned short stdc_##bare(unsigned short __x)                  \
	{                                                                      \
		return stdc_##bare##_us(__x);                                  \
	}                                                                      \
	inline unsigned int stdc_##bare(unsigned int __x)                      \
	{                                                                      \
		return stdc_##bare##_ui(__x);                                  \
	}                                                                      \
	inline unsigned long stdc_##bare(unsigned long __x)                    \
	{                                                                      \
		return stdc_##bare##_ul(__x);                                  \
	}                                                                      \
	inline unsigned long long stdc_##bare(unsigned long long __x)          \
	{                                                                      \
		return stdc_##bare##_ull(__x);                                 \
	}

                            __stdc_cxx_self(bit_floor) __stdc_cxx_self(bit_ceil)
#undef __stdc_cxx_self

#endif /* !__cplusplus */

#endif /* !_STDBIT_H_ */
