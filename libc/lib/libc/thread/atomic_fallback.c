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
 * the address of the object. All fallback accesses to one object use the
 * same lock. The lock uses sequentially consistent byte test-and-set and
 * clear, regardless of the order passed by the caller. The lock operations
 * themselves are lock-free. The object operations may spin while another
 * thread holds the lock.
 *
 * The arithmetic behind atomic_fetch_add and atomic_fetch_sub is performed
 * on the value's bytes in machine byte order. The bitwise operations work
 * on corresponding bytes and are independent of byte order.
 */

#include <sys/cdefs.h>

#include <limits.h>
#include <stdatomic.h>
#include <stddef.h>

#if !defined(__BYTE_ORDER__) || !defined(__ORDER_LITTLE_ENDIAN__) || \
    !defined(__ORDER_BIG_ENDIAN__) || \
    (__BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__ && \
    __BYTE_ORDER__ != __ORDER_BIG_ENDIAN__)
#error "atomic fallback requires a supported compiler byte order"
#endif

#define FALLBACK_NLOCKS 64

static unsigned char fallback_locks[FALLBACK_NLOCKS];

static unsigned char *
fallback_lock(const volatile void *obj)
{
	unsigned long v = (unsigned long)obj;

	/* Hash object addresses to a fixed table; collisions only serialize work. */
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

/* Keep the volatile qualification of the atomic object's accesses. */
static void
fallback_copy_out(void *dst, const volatile void *src, size_t size)
{
	unsigned char *d = dst;
	const volatile unsigned char *s = src;
	size_t i;

	for (i = 0; i < size; i++)
		d[i] = s[i];
}

static void
fallback_copy_in(volatile void *dst, const void *src, size_t size)
{
	volatile unsigned char *d = dst;
	const unsigned char *s = src;
	size_t i;

	for (i = 0; i < size; i++)
		d[i] = s[i];
}

static int
fallback_equal(const volatile void *obj, const void *expected, size_t size)
{
	const volatile unsigned char *o = obj;
	const unsigned char *e = expected;
	size_t i;

	for (i = 0; i < size; i++)
		if (o[i] != e[i])
			return (0);
	return (1);
}

void
__atomic_fallback_load(
    size_t size, const volatile void *obj, void *ret, int order)
{
	unsigned char *l = fallback_lock(obj);

	(void)order;
	fallback_acquire(l);
	fallback_copy_out(ret, obj, size);
	fallback_release(l);
}

void
__atomic_fallback_store(
    size_t size, volatile void *obj, const void *val, int order)
{
	unsigned char *l = fallback_lock(obj);

	(void)order;
	fallback_acquire(l);
	fallback_copy_in(obj, val, size);
	fallback_release(l);
}

void
__atomic_fallback_exchange(
    size_t size, volatile void *obj, const void *val, void *ret, int order)
{
	unsigned char *l = fallback_lock(obj);

	(void)order;
	fallback_acquire(l);
	fallback_copy_out(ret, obj, size);
	fallback_copy_in(obj, val, size);
	fallback_release(l);
}

int
__atomic_fallback_compare_exchange(size_t size, volatile void *obj,
    void *expected, const void *desired, int weak, int success, int failure)
{
	unsigned char *l = fallback_lock(obj);
	int	       ret;

	/* Compare object representations. Weak mode uses the same check. */
	(void)weak;
	(void)success;
	(void)failure;
	fallback_acquire(l);
	if (fallback_equal(obj, expected, size)) {
		fallback_copy_in(obj, desired, size);
		ret = 1;
	} else {
		fallback_copy_out(expected, obj, size);
		ret = 0;
	}
	fallback_release(l);
	return ret;
}

/*
 * Add from least to most significant byte and discard the final carry.
 * Subtraction adds the complement of b with an initial carry of one.
 */
static void
fallback_add(volatile unsigned char *a, const unsigned char *b, size_t size,
    int sub)
{
	unsigned carry = sub ? 1 : 0;
	size_t	 i;

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
	for (i = 0; i < size; i++) {
		unsigned t = a[i] + (unsigned char)(sub ? ~b[i] : b[i]) + carry;
		a[i] = (unsigned char)t;
		carry = t >> CHAR_BIT;
	}
#else
	for (i = size; i-- > 0;) {
		unsigned t = a[i] + (unsigned char)(sub ? ~b[i] : b[i]) + carry;
		a[i] = (unsigned char)t;
		carry = t >> CHAR_BIT;
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
	fallback_copy_out(ret, obj, size);
	fallback_add(obj, val, size, sub);
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
	volatile unsigned char *o = obj;
	const unsigned char *v = val;
	size_t		     i;

	(void)order;
	fallback_acquire(l);
	fallback_copy_out(ret, obj, size);
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
