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
 * All six headers in one translation unit, each included twice in opposite
 * orders, so that the include guards are exercised and the headers are
 * proven to be mutually compatible.  The program only checks a couple of
 * values from every header; its purpose is to link, not to re-test.
 */

#include <assert.h>
#include <stdalign.h>
#include <stdatomic.h>
#include <stdbit.h>
#include <stdckdint.h>
#include <stdint.h>
#include <stdnoreturn.h>
#include <threads.h>

static atomic_int joint_counter = 0;
static once_flag joint_once = ONCE_FLAG_INIT;

static void
joint_once_func(void)
{
	atomic_fetch_add(&joint_counter, 1);
}

noreturn static void
joint_die(void)
{
	thrd_exit(1);
}

int
main(void)
{
	alignas(8) char buf[8];
	int sum = 0;
	mtx_t mtx;

	assert(__STDC_ENDIAN_NATIVE__ == __STDC_ENDIAN_LITTLE__ ||
	       __STDC_ENDIAN_NATIVE__ == __STDC_ENDIAN_BIG__);
	assert(!ckd_add(&sum, 20, 22) && sum == 42);
	assert(stdc_bit_width(256u) == 9);
	assert(stdc_bit_ceil(5u) == 8);
	atomic_store(&joint_counter, 5);
	assert(atomic_load(&joint_counter) == 5);
	call_once(&joint_once, joint_once_func);
	assert(atomic_load(&joint_counter) == 6);
	assert(mtx_init(&mtx, mtx_plain) == thrd_success);
	assert(mtx_lock(&mtx) == thrd_success);
	assert(mtx_unlock(&mtx) == thrd_success);
	mtx_destroy(&mtx);
	assert((uintptr_t)buf % 8 == 0);

	(void)joint_die;
	return 0;
}
