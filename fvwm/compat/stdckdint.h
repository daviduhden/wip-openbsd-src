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
 * fvwm/compat/stdckdint.h -- vendored C23 <stdckdint.h> for FVWM.
 *
 * This header is FVWM's own copy of the C23 checked-integer interface.  It is
 * deliberately independent of the OpenBSD libc candidate kept elsewhere in
 * this repository: it must build FVWM on an unmodified OpenBSD installation
 * that has not installed any new libc header.  fvwm/compat is placed ahead of
 * the system include directories, so
 *
 *	#include <stdckdint.h>
 *
 * finds this file.  The implementation uses compiler overflow builtins in
 * C99 mode; it does not depend on a system C23 header.  These builtins are
 * an intentional compiler extension: they preserve mixed signed/unsigned
 * operand semantics and single evaluation without overflowing intermediate
 * C expressions.  OpenBSD's Clang supports them.
 *
 * The fallback matches C23 7.20.1: each operand is taken at its mathematical
 * value, the exact sum/difference/product is converted to the type of
 * *result (wrapping on overflow), stored, and the macro yields true exactly
 * when that conversion is not value preserving.  Every argument is evaluated
 * exactly once.  This is the same formulation as the GCC and Clang resource
 * headers and the musl <stdckdint.h> proposal; no code is copied from them.
 *
 * FVWM_COMPAT_STDCKDINT_FALLBACK identifies this implementation for tests;
 * the same overflow operations are used on every supported host.
 */

#ifndef FVWM_COMPAT_STDCKDINT_H
#define FVWM_COMPAT_STDCKDINT_H

#define FVWM_COMPAT_STDCKDINT_FALLBACK 1

#ifndef __STDC_VERSION_STDCKDINT_H__
#define __STDC_VERSION_STDCKDINT_H__ 202311L
#endif

#if defined(__has_builtin)
#if !__has_builtin(__builtin_add_overflow) ||                                  \
    !__has_builtin(__builtin_sub_overflow) ||                                  \
    !__has_builtin(__builtin_mul_overflow)
#error "fvwm: no <stdckdint.h> and no compiler overflow builtins"
#endif
#elif !defined(__GNUC__) && !defined(__clang__)
#error "fvwm: no <stdckdint.h> and no compiler overflow builtins"
#endif

#ifndef ckd_add
#define ckd_add(result, a, b)                                                  \
	((_Bool)__builtin_add_overflow((a), (b), (result)))
#endif
#ifndef ckd_sub
#define ckd_sub(result, a, b)                                                  \
	((_Bool)__builtin_sub_overflow((a), (b), (result)))
#endif
#ifndef ckd_mul
#define ckd_mul(result, a, b)                                                  \
	((_Bool)__builtin_mul_overflow((a), (b), (result)))
#endif

#endif /* FVWM_COMPAT_STDCKDINT_H */
