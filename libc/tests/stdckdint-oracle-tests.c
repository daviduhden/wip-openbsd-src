/*	$OpenBSD$	*/

/*
 * Host-side property test for the C23 <stdckdint.h> macros, checked against
 * an independent oracle: the exact result is computed in 128-bit arithmetic
 * and compared with both the overflow indication and the stored (wrapped)
 * value.  The operands are drawn from a deterministic generator biased
 * towards the boundary values of each type.  Not part of the OpenBSD build
 * graph; see libc/tests/Makefile.
 */

#include <limits.h>
#include <stdckdint.h>
#include <stdio.h>

#ifndef _STDCKDINT_H_
#error "the tree's <stdckdint.h> was not the header that was included"
#endif

#ifndef __SIZEOF_INT128__
int
main(void)
{
	printf("stdckdint-oracle: skipped (no __int128)\n");
	return 0;
}
#else

__extension__ typedef __int128 i128;
__extension__ typedef unsigned __int128 u128;

typedef unsigned int uint;
typedef unsigned long long ull;
typedef signed char schar;
typedef unsigned char uchar;
typedef long long llong;

#define ITERS 50000

static int fails;
static unsigned long long rs = 0x9e3779b97f4a7c15ULL;

static unsigned long long
rng(void)
{
	rs = rs * 6364136223846793005ULL + 1442695040888963407ULL;
	return rs >> 17;
}

static long long
pick_signed(void)
{
	static const long long edge[] = {0, 1, -1, 2, -2, 3, -3, LLONG_MAX,
	    LLONG_MIN, LLONG_MAX - 1, LLONG_MIN + 1, (int)INT_MAX, (int)INT_MIN,
	    65535LL, -65536LL};

	if ((rng() & 3) != 0)
		return (long long)rng();
	return edge[rng() % (sizeof(edge) / sizeof(edge[0]))];
}

static unsigned long long
pick_unsigned(void)
{
	static const unsigned long long edge[] = {0, 1, 2, 3, ULLONG_MAX,
	    ULLONG_MAX - 1, (unsigned)UINT_MAX, 65535ULL,
	    0x8000000000000000ULL};

	if ((rng() & 3) != 0)
		return rng();
	return edge[rng() % (sizeof(edge) / sizeof(edge[0]))];
}

static void
report(const char *what, int n, const char *msg)
{
	printf("FAIL %s iteration %d: %s\n", what, n, msg);
	fails++;
}

static i128
op_add(i128 x, i128 y)
{
	return x + y;
}
static i128
op_sub(i128 x, i128 y)
{
	return x - y;
}
static i128
op_mul(i128 x, i128 y)
{
	return x * y;
}
static u128
uop_add(u128 x, u128 y)
{
	return x + y;
}
static u128
uop_sub(u128 x, u128 y)
{
	return x - y;
}
static u128
uop_mul(u128 x, u128 y)
{
	return x * y;
}

#define DEFINE_SIGNED(T, MINV, MAXV, OP)                                       \
	static void test_##T##_##OP(void)                                      \
	{                                                                      \
		int n;                                                         \
                                                                               \
		for (n = 0; n < ITERS; n++) {                                  \
			T a = (T)pick_signed();                                \
			T b = (T)pick_signed();                                \
			i128 exact = op_##OP((i128)a, (i128)b);                \
			T got;                                                 \
			int ovf = ckd_##OP(&got, a, b);                        \
			int want =                                             \
			    exact < (i128)(MINV) || exact > (i128)(MAXV);      \
			T exp = (T)(unsigned long long)exact;                  \
                                                                               \
			if (ovf != want)                                       \
				report(#T "/" #OP, n, "overflow flag");        \
			else if (got != exp)                                   \
				report(#T "/" #OP, n, "stored value");         \
		}                                                              \
	}

#define DEFINE_UNSIGNED(T, MAXV, OP)                                           \
	static void test_##T##_##OP(void)                                      \
	{                                                                      \
		int n;                                                         \
                                                                               \
		for (n = 0; n < ITERS; n++) {                                  \
			T a = (T)pick_unsigned();                              \
			T b = (T)pick_unsigned();                              \
			u128 exact = uop_##OP((u128)a, (u128)b);               \
			T got;                                                 \
			int ovf = ckd_##OP(&got, a, b);                        \
			int want = exact > (u128)(MAXV);                       \
			T exp = (T)exact;                                      \
                                                                               \
			if (ovf != want)                                       \
				report(#T "/" #OP, n, "overflow flag");        \
			else if (got != exp)                                   \
				report(#T "/" #OP, n, "stored value");         \
		}                                                              \
	}

DEFINE_SIGNED(int, INT_MIN, INT_MAX, add)
DEFINE_SIGNED(int, INT_MIN, INT_MAX, sub)
DEFINE_SIGNED(int, INT_MIN, INT_MAX, mul)
DEFINE_SIGNED(llong, LLONG_MIN, LLONG_MAX, add)
DEFINE_SIGNED(llong, LLONG_MIN, LLONG_MAX, sub)
DEFINE_SIGNED(llong, LLONG_MIN, LLONG_MAX, mul)
DEFINE_SIGNED(schar, SCHAR_MIN, SCHAR_MAX, add)
DEFINE_SIGNED(schar, SCHAR_MIN, SCHAR_MAX, mul)

DEFINE_UNSIGNED(uint, UINT_MAX, add)
DEFINE_UNSIGNED(uint, UINT_MAX, sub)
DEFINE_UNSIGNED(uint, UINT_MAX, mul)
DEFINE_UNSIGNED(ull, ULLONG_MAX, add)
DEFINE_UNSIGNED(ull, ULLONG_MAX, sub)
DEFINE_UNSIGNED(ull, ULLONG_MAX, mul)
DEFINE_UNSIGNED(uchar, UCHAR_MAX, add)
DEFINE_UNSIGNED(uchar, UCHAR_MAX, mul)

/*
 * Mixed result/operand widths: the exact value is representable in the
 * wider destination in the first pair and the narrowing in the second pair
 * must be detected.
 */
static void
test_mixed(void)
{
	int n;

	for (n = 0; n < ITERS; n++) {
		long long a = pick_signed();
		long long b = pick_signed();
		long long got;
		int ovf;

		/* (long long) destination from (int) operands: never overflows
		 */
		ovf = ckd_mul(&got, (int)a, (int)b);
		if (ovf != 0) {
			printf("FAIL mixed: int*int into long long reported "
			       "overflow\n");
			fails++;
		}

		/* (int) destination from (long long) operands */
		{
			int igot;
			i128 exact = (i128)a * (i128)b;
			int want =
			    exact < (i128)INT_MIN || exact > (i128)INT_MAX;
			int g = ckd_mul(&igot, a, b);
			if (g != want ||
			    igot != (int)(unsigned long long)exact) {
				printf("FAIL mixed: long long*long long into "
				       "int\n");
				fails++;
			}
		}
	}
}

int
main(void)
{
	test_int_add();
	test_int_sub();
	test_int_mul();
	test_llong_add();
	test_llong_sub();
	test_llong_mul();
	test_schar_add();
	test_schar_mul();
	test_uint_add();
	test_uint_sub();
	test_uint_mul();
	test_ull_add();
	test_ull_sub();
	test_ull_mul();
	test_uchar_add();
	test_uchar_mul();
	test_mixed();

	if (fails == 0)
		printf("stdckdint oracle: all ok\n");
	return fails != 0;
}
#endif /* __SIZEOF_INT128__ */
