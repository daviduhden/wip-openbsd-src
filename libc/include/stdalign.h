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
 * ISO/IEC 9899:2011 (C11) 7.15: alignment <stdalign.h>.
 *
 * C11 provides the macros alignas and alignof, which expand to the
 * language keywords _Alignas and _Alignof, and the macros
 * __alignas_is_defined and __alignof_is_defined, which expand to 1.
 *
 * ISO/IEC 9899:2024 (C23) turned alignas and alignof into real keywords
 * and removed all four macros from the header.  The header is therefore
 * empty in C23, and nothing in it may shadow the keywords.  In C++ the
 * spelling alignas/alignof is a keyword as well (C++11 and later), so the
 * two function-like names are never defined there; the historical
 * __alignas_is_defined/__alignof_is_defined markers are retained for
 * source compatibility with the pre-C23 header.
 *
 * This header is self-contained: it depends on no other header and
 * defines no symbol.
 */

#ifndef _STDALIGN_H_
#define _STDALIGN_H_

#if !(defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L)

#ifndef __cplusplus
#define alignas _Alignas
#define alignof _Alignof
#endif

#define __alignas_is_defined 1
#define __alignof_is_defined 1

#endif /* !C23 */

#endif /* !_STDALIGN_H_ */
