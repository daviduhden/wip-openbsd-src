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
 * ISO/IEC 9899:2011 (C11) 7.17: Atomics, runtime support.
 *
 * The type-generic operations in <stdatomic.h> are macros over the compiler
 * builtins.  The fence and flag operations are additionally defined here as
 * real functions so that their address can be taken and so that the
 * declarations in the header are backed by exported symbols, as 7.17
 * requires.  The header's function-like macros are undefined first so that
 * the definitions below are not rewritten by the preprocessor.
 */

#include <stdatomic.h>

#undef atomic_thread_fence

void
atomic_thread_fence(memory_order order)
{
	__atomic_thread_fence(order);
}
DEF_WEAK(atomic_thread_fence);

#undef atomic_signal_fence

void
atomic_signal_fence(memory_order order)
{
	__atomic_signal_fence(order);
}
DEF_WEAK(atomic_signal_fence);

#undef atomic_flag_test_and_set

_Bool
atomic_flag_test_and_set(volatile atomic_flag *object)
{
	return __atomic_test_and_set(object, __ATOMIC_SEQ_CST);
}
DEF_WEAK(atomic_flag_test_and_set);

#undef atomic_flag_test_and_set_explicit

_Bool
atomic_flag_test_and_set_explicit(
    volatile atomic_flag *object, memory_order order)
{
	return __atomic_test_and_set(object, order);
}
DEF_WEAK(atomic_flag_test_and_set_explicit);

#undef atomic_flag_clear

void
atomic_flag_clear(volatile atomic_flag *object)
{
	__atomic_clear(object, __ATOMIC_SEQ_CST);
}
DEF_WEAK(atomic_flag_clear);

#undef atomic_flag_clear_explicit

void
atomic_flag_clear_explicit(volatile atomic_flag *object, memory_order order)
{
	__atomic_clear(object, order);
}
DEF_WEAK(atomic_flag_clear_explicit);
