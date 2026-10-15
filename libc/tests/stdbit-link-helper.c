/*	$OpenBSD$	*/

/*
 * Host-side helper used to prove that the C23 <stdbit.h> typed functions
 * have external linkage: it returns the address of a typed function as seen
 * from a different translation unit, so the test can compare it with the
 * address taken in its own translation unit.  Not part of the OpenBSD
 * build graph.
 */

#include <stdbit.h>

unsigned int (*stdbit_link_addr_uc(void))(unsigned char)
{
	return stdc_leading_zeros_uc;
}

unsigned long long (*stdbit_link_addr_ull(void))(unsigned long long)
{
	return stdc_bit_floor_ull;
}
