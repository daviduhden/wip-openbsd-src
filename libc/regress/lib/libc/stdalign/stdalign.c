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

/* ISO C11 <stdalign.h>: macros, keywords and alignment semantics. */

#include <assert.h>
#include <stdalign.h>
#include <stddef.h>
#include <stdint.h>

struct aligned {
	char c;
	alignas(64) char pad[64];
};

int
main(void)
{
	alignas(16) char buf[16];

	/* The object really is 64-byte aligned. */
	assert((uintptr_t)&buf % 16 == 0);
	assert(offsetof(struct aligned, pad) % 64 == 0);

	/* alignof yields size_t and is at least 1 for any type. */
	assert(alignof(char) >= 1);
	assert(alignof(struct aligned) >= 64);

#ifdef __STDC_VERSION__
#if __STDC_VERSION__ < 202311L
	/* Pre-C23 the markers exist and expand to 1. */
	assert(__alignas_is_defined == 1);
	assert(__alignof_is_defined == 1);
#endif
#endif

	return 0;
}
