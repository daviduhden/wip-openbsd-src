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
 * compat/stdckdint.h -- portable stand-in for the C23 <stdckdint.h>.
 *
 * Some C23 libc/toolchain combinations (notably the one shipped with
 * OpenBSD 7.9/8.0-beta) do not provide <stdckdint.h> yet, even though
 * the compiler supports the C23 checked-integer API.  This header is
 * placed on the include path ahead of the system headers, so existing
 *
 *	#include <stdckdint.h>
 *
 * directives keep working unchanged.  When the toolchain does provide a
 * real header, this wrapper defers to it with #include_next; otherwise
 * it defines ckd_add(), ckd_sub() and ckd_mul() in terms of the
 * compiler's overflow builtins.
 *
 * The builtins implement exactly the semantics required by C23 7.20.1:
 * the operation is evaluated as if in an infinite-range signed integer
 * type, the mathematical result is converted to the type of *result
 * (wrapping around on overflow), and the macro returns true if and only
 * if that conversion is not value preserving.  They are defined for all
 * argument values (so the fallback itself has no undefined behaviour),
 * evaluate each argument exactly once, and accept operands and result
 * of differing signedness and width.
 *
 * C23 additionally restricts the operands and *result to integer types
 * other than plain char, bool, bit-precise integer types and enumerated
 * types; the builtins accept a superset of that and reject the same
 * bool/enum result cases, so no divergence matters for valid programs.
 */
#ifndef FVWM_COMPAT_STDCKDINT_H
#define FVWM_COMPAT_STDCKDINT_H

/*
 * Prefer the implementation provided by the toolchain.  #include_next
 * skips this directory, so the lookup cannot recurse back into this
 * file.  __has_include_next() is available with every compiler this
 * project supports (GCC and Clang).
 */
#if !defined(FVWM_STDCKDINT_FORCE_FALLBACK)
#  if defined(__has_include_next)
#    if __has_include_next(<stdckdint.h>)
#      include_next <stdckdint.h>
#      define FVWM_COMPAT_STDCKDINT_HAVE_NATIVE 1
#    endif
#  endif
#endif

/* Fallback: compiler overflow builtins. */
#ifndef FVWM_COMPAT_STDCKDINT_HAVE_NATIVE
#  define FVWM_COMPAT_STDCKDINT_FALLBACK 1
#  define __STDC_VERSION_STDCKDINT_H__ 202311L

#  if defined(__has_builtin)
#    if !__has_builtin(__builtin_add_overflow)
#      error "fvwm: <stdckdint.h> is unavailable and this compiler lacks overflow builtins"
#    endif
#  elif !defined(__GNUC__) && !defined(__clang__)
#    error "fvwm: <stdckdint.h> is unavailable and this compiler lacks overflow builtins"
#  endif

#  define ckd_add(result, a, b) __builtin_add_overflow((a), (b), (result))
#  define ckd_sub(result, a, b) __builtin_sub_overflow((a), (b), (result))
#  define ckd_mul(result, a, b) __builtin_mul_overflow((a), (b), (result))
#endif /* !FVWM_COMPAT_STDCKDINT_HAVE_NATIVE */

#endif /* FVWM_COMPAT_STDCKDINT_H */
