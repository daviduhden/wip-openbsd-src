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
 * ISO/IEC 9899:2011 (C11) 7.23: _Noreturn <stdnoreturn.h>.
 *
 * The only thing this header defines is the convenience macro noreturn,
 * which expands to the function specifier _Noreturn.  It is written for C
 * only: in C++ the equivalent facility is the [[noreturn]] attribute, the
 * macro is not part of the C++ standard library, and defining it would
 * collide with the C++ keyword.  As required by the standard the header
 * therefore defines nothing when compiled as C++.
 *
 * C23 retains _Noreturn and the macro (both are deprecated in favour of
 * the [[noreturn]] attribute but remain valid); the macro still expands to
 * _Noreturn so that pre-C23 sources keep compiling unchanged.  This header
 * does not pull in any other header and adds no symbol to libc.
 */

#ifndef _STDNORETURN_H_
#define _STDNORETURN_H_

#ifndef __cplusplus

#define noreturn _Noreturn
#define __noreturn_is_defined 1

#endif /* !__cplusplus */

#endif /* !_STDNORETURN_H_ */
