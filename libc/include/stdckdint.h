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
 * ISO/IEC 9899:2024 (C23) 7.20: checked integer arithmetic
 * <stdckdint.h>.
 *
 * ckd_add, ckd_sub and ckd_mul evaluate their two operands at their
 * mathematical (infinite precision) value, convert the exact result to the
 * type of the object designated by the first argument, store that converted
 * value, and yield true exactly when the conversion is not value
 * preserving.  Every argument is evaluated exactly once and no operation
 * has undefined behaviour, even when the mathematical result is not
 * representable in any integer type.
 *
 * The expansions use the compiler overflow builtins, which implement
 * precisely this contract and are the only reasonable portable expression
 * of it; the C library is not required to provide any runtime support.  The
 * macros are guarded with #ifndef so that a translation unit that reaches
 * this header through the compiler's own <stdckdint.h> (both clang and GCC
 * ship one) does not produce a redefinition diagnostic.
 */

#ifndef _STDCKDINT_H_
#define _STDCKDINT_H_

#define __STDC_VERSION_STDCKDINT_H__ 202311L

/*
 * The result type is bool.  _Bool is a C keyword and needs no header there;
 * in C++ bool is the keyword, so the macros use whichever applies.
 */
#ifdef __cplusplus
#define __stdc_ckd_bool bool
#else
#define __stdc_ckd_bool _Bool
#endif

#if defined(__has_builtin)
#if !__has_builtin(__builtin_add_overflow) ||                                  \
    !__has_builtin(__builtin_sub_overflow) ||                                  \
    !__has_builtin(__builtin_mul_overflow)
#error "<stdckdint.h> requires the compiler overflow builtins"
#endif
#endif

#ifndef ckd_add
#define ckd_add(result, a, b)                                                  \
	((__stdc_ckd_bool)__builtin_add_overflow((a), (b), (result)))
#endif

#ifndef ckd_sub
#define ckd_sub(result, a, b)                                                  \
	((__stdc_ckd_bool)__builtin_sub_overflow((a), (b), (result)))
#endif

#ifndef ckd_mul
#define ckd_mul(result, a, b)                                                  \
	((__stdc_ckd_bool)__builtin_mul_overflow((a), (b), (result)))
#endif

#endif /* !_STDCKDINT_H_ */
