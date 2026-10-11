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
 * ISO C23 7.20: checked integer arithmetic.
 *
 * The macros expand to the compiler's overflow builtins, which implement
 * exactly the C23 contract: each operand is taken at its mathematical value,
 * the exact sum/difference/product is converted to the type of *result
 * (wrapping when it is not representable), the converted value is stored, and
 * the macro evaluates to true exactly when that conversion is not value
 * preserving.  Every argument is evaluated exactly once.
 *
 * No library code is required: this header defines only macros, adds no
 * symbols, and needs no object to be compiled or installed.
 *
 * The #ifndef ckd_* guards let the compiler's own <stdckdint.h> (GCC and
 * clang both ship one in their resource header directory) reach this header
 * with #include_next and defer to it without redefining the macros.  The
 * implementation is the formulation used by the compiler headers and by the
 * musl <stdckdint.h> proposal; no code is copied from either.
 */

#ifndef	_STDCKDINT_H_
#define	_STDCKDINT_H_

#if !defined(__STDC_VERSION_STDCKDINT_H__)
#define	__STDC_VERSION_STDCKDINT_H__	202311L
#endif

#if defined(__has_builtin)
#if !__has_builtin(__builtin_add_overflow) || \
    !__has_builtin(__builtin_sub_overflow) || \
    !__has_builtin(__builtin_mul_overflow)
#error "<stdckdint.h> requires the compiler overflow builtins"
#endif
#endif

#ifndef	ckd_add
#define	ckd_add(result, a, b) \
	((_Bool)__builtin_add_overflow((a), (b), (result)))
#endif
#ifndef	ckd_sub
#define	ckd_sub(result, a, b) \
	((_Bool)__builtin_sub_overflow((a), (b), (result)))
#endif
#ifndef	ckd_mul
#define	ckd_mul(result, a, b) \
	((_Bool)__builtin_mul_overflow((a), (b), (result)))
#endif

#endif /* !_STDCKDINT_H_ */
