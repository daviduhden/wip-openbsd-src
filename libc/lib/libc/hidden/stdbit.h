/*	$OpenBSD$	*/
/* SPDX-License-Identifier: ISC */

#ifndef _LIBC_STDBIT_H_
#define _LIBC_STDBIT_H_

#include_next <stdbit.h>

#ifndef _STDBIT_H_
#error "libc requires its own <stdbit.h>"
#endif

/* Internal ISO calls use hidden names; gen/stdbit.c supplies aliases. */
PROTO_NORMAL(stdc_leading_zeros_uc);
PROTO_NORMAL(stdc_leading_zeros_us);
PROTO_NORMAL(stdc_leading_zeros_ui);
PROTO_NORMAL(stdc_leading_zeros_ul);
PROTO_NORMAL(stdc_leading_zeros_ull);
PROTO_NORMAL(stdc_leading_ones_uc);
PROTO_NORMAL(stdc_leading_ones_us);
PROTO_NORMAL(stdc_leading_ones_ui);
PROTO_NORMAL(stdc_leading_ones_ul);
PROTO_NORMAL(stdc_leading_ones_ull);
PROTO_NORMAL(stdc_trailing_zeros_uc);
PROTO_NORMAL(stdc_trailing_zeros_us);
PROTO_NORMAL(stdc_trailing_zeros_ui);
PROTO_NORMAL(stdc_trailing_zeros_ul);
PROTO_NORMAL(stdc_trailing_zeros_ull);
PROTO_NORMAL(stdc_trailing_ones_uc);
PROTO_NORMAL(stdc_trailing_ones_us);
PROTO_NORMAL(stdc_trailing_ones_ui);
PROTO_NORMAL(stdc_trailing_ones_ul);
PROTO_NORMAL(stdc_trailing_ones_ull);
PROTO_NORMAL(stdc_first_leading_zero_uc);
PROTO_NORMAL(stdc_first_leading_zero_us);
PROTO_NORMAL(stdc_first_leading_zero_ui);
PROTO_NORMAL(stdc_first_leading_zero_ul);
PROTO_NORMAL(stdc_first_leading_zero_ull);
PROTO_NORMAL(stdc_first_leading_one_uc);
PROTO_NORMAL(stdc_first_leading_one_us);
PROTO_NORMAL(stdc_first_leading_one_ui);
PROTO_NORMAL(stdc_first_leading_one_ul);
PROTO_NORMAL(stdc_first_leading_one_ull);
PROTO_NORMAL(stdc_first_trailing_zero_uc);
PROTO_NORMAL(stdc_first_trailing_zero_us);
PROTO_NORMAL(stdc_first_trailing_zero_ui);
PROTO_NORMAL(stdc_first_trailing_zero_ul);
PROTO_NORMAL(stdc_first_trailing_zero_ull);
PROTO_NORMAL(stdc_first_trailing_one_uc);
PROTO_NORMAL(stdc_first_trailing_one_us);
PROTO_NORMAL(stdc_first_trailing_one_ui);
PROTO_NORMAL(stdc_first_trailing_one_ul);
PROTO_NORMAL(stdc_first_trailing_one_ull);
PROTO_NORMAL(stdc_count_zeros_uc);
PROTO_NORMAL(stdc_count_zeros_us);
PROTO_NORMAL(stdc_count_zeros_ui);
PROTO_NORMAL(stdc_count_zeros_ul);
PROTO_NORMAL(stdc_count_zeros_ull);
PROTO_NORMAL(stdc_count_ones_uc);
PROTO_NORMAL(stdc_count_ones_us);
PROTO_NORMAL(stdc_count_ones_ui);
PROTO_NORMAL(stdc_count_ones_ul);
PROTO_NORMAL(stdc_count_ones_ull);
PROTO_NORMAL(stdc_has_single_bit_uc);
PROTO_NORMAL(stdc_has_single_bit_us);
PROTO_NORMAL(stdc_has_single_bit_ui);
PROTO_NORMAL(stdc_has_single_bit_ul);
PROTO_NORMAL(stdc_has_single_bit_ull);
PROTO_NORMAL(stdc_bit_width_uc);
PROTO_NORMAL(stdc_bit_width_us);
PROTO_NORMAL(stdc_bit_width_ui);
PROTO_NORMAL(stdc_bit_width_ul);
PROTO_NORMAL(stdc_bit_width_ull);
PROTO_NORMAL(stdc_bit_floor_uc);
PROTO_NORMAL(stdc_bit_floor_us);
PROTO_NORMAL(stdc_bit_floor_ui);
PROTO_NORMAL(stdc_bit_floor_ul);
PROTO_NORMAL(stdc_bit_floor_ull);
PROTO_NORMAL(stdc_bit_ceil_uc);
PROTO_NORMAL(stdc_bit_ceil_us);
PROTO_NORMAL(stdc_bit_ceil_ui);
PROTO_NORMAL(stdc_bit_ceil_ul);
PROTO_NORMAL(stdc_bit_ceil_ull);

#endif /* _LIBC_STDBIT_H_ */
