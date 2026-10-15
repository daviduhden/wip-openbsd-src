/*	$OpenBSD$	*/
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

#ifndef _LIBC_STDATOMIC_H_
#define _LIBC_STDATOMIC_H_

#include_next <stdatomic.h>

PROTO_NORMAL(atomic_flag_clear);
PROTO_NORMAL(atomic_flag_clear_explicit);
PROTO_NORMAL(atomic_flag_test_and_set);
PROTO_NORMAL(atomic_flag_test_and_set_explicit);
PROTO_NORMAL(atomic_signal_fence);
PROTO_NORMAL(atomic_thread_fence);

#endif /* !_LIBC_STDATOMIC_H_ */
