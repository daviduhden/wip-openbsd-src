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

#ifndef _LIBC_THREADS_H_
#define _LIBC_THREADS_H_

#include_next <threads.h>

PROTO_NORMAL(call_once);
PROTO_NORMAL(cnd_broadcast);
PROTO_NORMAL(cnd_destroy);
PROTO_NORMAL(cnd_init);
PROTO_NORMAL(cnd_signal);
PROTO_NORMAL(cnd_timedwait);
PROTO_NORMAL(cnd_wait);
PROTO_NORMAL(mtx_destroy);
PROTO_NORMAL(mtx_init);
PROTO_NORMAL(mtx_lock);
PROTO_NORMAL(mtx_timedlock);
PROTO_NORMAL(mtx_trylock);
PROTO_NORMAL(mtx_unlock);
PROTO_NORMAL(thrd_create);
PROTO_NORMAL(thrd_current);
PROTO_NORMAL(thrd_detach);
PROTO_NORMAL(thrd_equal);
PROTO_NORMAL(thrd_exit);
PROTO_NORMAL(thrd_join);
PROTO_NORMAL(thrd_sleep);
PROTO_NORMAL(thrd_yield);
PROTO_NORMAL(tss_create);
PROTO_NORMAL(tss_delete);
PROTO_NORMAL(tss_get);
PROTO_NORMAL(tss_set);

#endif /* !_LIBC_THREADS_H_ */
