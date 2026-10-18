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
 * ISO/IEC 9899:2011 (C11) 7.17: Atomics, lock-based support.
 *
 * A type whose size is not lock-free cannot be manipulated with a single
 * atomic instruction.  <stdatomic.h> detects that case at compile time and
 * calls these functions instead of letting the compiler emit a call to its
 * atomic runtime (libatomic), which the base system does not provide.
 *
 * Each function holds one of a small, fixed number of spinlocks selected by
 * the address of the object.  The lock is taken with sequential-consistency
 * ordering, which is stronger than any memory order the standard asks for,
 * so the requested order is always satisfied and the order argument is
 * ignored.  The lock itself is implemented with the compiler's atomic
 * byte test-and-set/clear builtins, which are lock-free on every supported
 * architecture, so no unbounded spinning on a non-atomic value occurs.
 *
 * The arithmetic behind atomic_fetch_add and atomic_fetch_sub is performed
 * on the operand's own bytes, propagating carry or borrow in the machine's
 * byte order; the bitwise operations are byte-wise and order-independent.
 */

#include <sys/cdefs.h>

#include <stdatomic.h>
#include <string.h>

#define FALLBACK_NLOCKS 64

static unsigned char fallback_locks[FALLBACK_NLOCKS];

static unsigned char *
fallback_lock(const volatile void *obj)
{
	unsigned long v = (unsigned long)obj;

	return &fallback_locks[(v >> 4) & (FALLBACK_NLOCKS - 1)];
}

static void
fallback_acquire(unsigned char *l)
{
	while (__atomic_test_and_set(l, __ATOMIC_SEQ_CST))
		;
}

static void
fallback_release(unsigned char *l)
{
	__atomic_clear(l, __ATOMIC_SEQ_CST);
}

void
__atomic_fallback_load(
    size_t size, const volatile void *obj, void *ret, int order)
{
	unsigned char *l = fallback_lock(obj);

	(void)order;
	fallback_acquire(l);
	memcpy(ret, (const void *)obj, size);
	fallback_release(l);
}

void
__atomic_fallback_store(
    size_t size, volatile void *obj, const void *val, int order)
{
	unsigned char *l = fallback_lock(obj);

	(void)order;
	fallback_acquire(l);
	memcpy((void *)obj, val, size);
	fallback_release(l);
}

void
__atomic_fallback_exchange(
    size_t size, volatile void *obj, const void *val, void *ret, int order)
{
	unsigned char *l = fallback_lock(obj);

	(void)order;
	fallback_acquire(l);
	memcpy(ret, (const void *)obj, size);
	memcpy((void *)obj, val, size);
	fallback_release(l);
}

int
__atomic_fallback_compare_exchange(size_t size, volatile void *obj,
    void *expected, const void *desired, int weak, int success, int failure)
{
	unsigned char *l = fallback_lock(obj);
	int	       ret;

	(void)weak;
	(void)success;
	(void)failure;
	fallback_acquire(l);
	if (memcmp((const void *)obj, expected, size) == 0) {
		memcpy((void *)obj, desired, size);
		ret = 1;
	} else {
		memcpy(expected, (const void *)obj, size);
		ret = 0;
	}
	fallback_release(l);
	return ret;
}

/* a = a + b, or a = a - b when sub is non-zero, byte by byte. */
static void
fallback_add(unsigned char *a, const unsigned char *b, size_t size, int sub)
{
	unsigned carry = sub ? 1 : 0;
	size_t	 i;

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
	for (i = 0; i < size; i++) {
		unsigned t = a[i] + (unsigned char)(sub ? ~b[i] : b[i]) + carry;
		a[i] = (unsigned char)t;
		carry = t >> 8;
	}
#else
	for (i = size; i-- > 0;) {
		unsigned t = a[i] + (unsigned char)(sub ? ~b[i] : b[i]) + carry;
		a[i] = (unsigned char)t;
		carry = t >> 8;
	}
#endif
}

static void
fallback_fetch_add(size_t size, volatile void *obj, const void *val, void *ret,
    int order, int sub)
{
	unsigned char *l = fallback_lock(obj);

	(void)order;
	fallback_acquire(l);
	memcpy(ret, (const void *)obj, size);
	fallback_add((unsigned char *)(void *)obj, val, size, sub);
	fallback_release(l);
}

void
__atomic_fallback_fetch_add(
    size_t size, volatile void *obj, const void *val, void *ret, int order)
{
	fallback_fetch_add(size, obj, val, ret, order, 0);
}

void
__atomic_fallback_fetch_sub(
    size_t size, volatile void *obj, const void *val, void *ret, int order)
{
	fallback_fetch_add(size, obj, val, ret, order, 1);
}

static void
fallback_fetch_bitwise(size_t size, volatile void *obj, const void *val,
    void *ret, int order, int op)
{
	unsigned char	    *l = fallback_lock(obj);
	unsigned char	    *o = (unsigned char *)(void *)obj;
	const unsigned char *v = val;
	size_t		     i;

	(void)order;
	fallback_acquire(l);
	memcpy(ret, (const void *)obj, size);
	for (i = 0; i < size; i++) {
		switch (op) {
		case 1:
			o[i] &= v[i];
			break;
		case 2:
			o[i] |= v[i];
			break;
		default:
			o[i] ^= v[i];
			break;
		}
	}
	fallback_release(l);
}

void
__atomic_fallback_fetch_and(
    size_t size, volatile void *obj, const void *val, void *ret, int order)
{
	fallback_fetch_bitwise(size, obj, val, ret, order, 1);
}

void
__atomic_fallback_fetch_or(
    size_t size, volatile void *obj, const void *val, void *ret, int order)
{
	fallback_fetch_bitwise(size, obj, val, ret, order, 2);
}

void
__atomic_fallback_fetch_xor(
    size_t size, volatile void *obj, const void *val, void *ret, int order)
{
	fallback_fetch_bitwise(size, obj, val, ret, order, 3);
}
