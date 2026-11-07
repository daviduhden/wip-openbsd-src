#!/bin/sh
#	$OpenBSD$
#
# Copyright (c) 2026 David Uhden Collado <david@uhden.dev>
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
#
# Compile each of the six headers on its own, and all of them together in
# both include orders, under C11, C17 and C23, with strict warnings.  The
# A regular source include directory precedes compiler resources. Guard
# checks reject compiler substitutes even if the search configuration changes.

set -u

CC=${CC:-cc}
PUBLIC_INCDIR=${PUBLIC_INCDIR:-$(dirname "$0")/../../../../include}
WARN="-Wall -Wextra -Werror -Wpedantic"
HEADERS="stdalign stdnoreturn stdatomic stdbit stdckdint threads"
STDS="c11 c17 c23"
tmp=$(mktemp -d "${TMPDIR:-/tmp}/iso-c-headers.XXXXXXXX") || exit 1
trap 'rm -rf "$tmp"' 0
trap 'exit 1' HUP INT TERM
status=0

try()
{
	# $1 = standard, $2 = source text
	printf '%s\n' "$2" | $CC -std=$1 -I"$PUBLIC_INCDIR" $WARN -fsyntax-only -x c - \
	    2>"$tmp/c.err" || {
		echo "FAIL: -std=$1: $2" >&2
		sed 's/^/    /' "$tmp/c.err" >&2
		status=1
	}
}

guards=''
for h in $HEADERS; do
	guard=$(printf '_%s_H_' "$h" | tr '[:lower:]' '[:upper:]')
	check="
#ifndef $guard
#error wrong $h.h
#endif"
	guards="$guards$check"
	for std in $STDS; do
		try "$std" "#include <$h.h>
$check
int main(void) { return 0; }"
	done
done

# Each header alone, but translated: the macros must survive a use.
try c11 "#include <stdalign.h>
#ifndef _STDALIGN_H_
#error wrong stdalign.h
#endif
alignas(8) int x; int main(void) { return (int)alignof(int) - (int)alignof(int) + (int)sizeof(x) - 4; }"
try c11 "#include <stdnoreturn.h>
#ifndef _STDNORETURN_H_
#error wrong stdnoreturn.h
#endif
noreturn void f(void); int main(void) { return 0; }"
try c11 "#include <stdatomic.h>
#ifndef _STDATOMIC_H_
#error wrong stdatomic.h
#endif
atomic_int x; int main(void) { return atomic_load(&x); }"
try c11 "#include <threads.h>
#ifndef _THREADS_H_
#error wrong threads.h
#endif
int main(void) { thrd_t t; return (int)(long)&t - (int)(long)&t; }"
try c23 "#include <stdbit.h>
#ifndef _STDBIT_H_
#error wrong stdbit.h
#endif
int main(void) { return (int)stdc_bit_width(0u); }"
try c23 "#include <stdckdint.h>
#ifndef _STDCKDINT_H_
#error wrong stdckdint.h
#endif
int main(void) { int r; return ckd_add(&r, 1, 1); }"

# All six, forward and reverse, in every supported C mode.
for std in $STDS; do
	try "$std" "#include <stdalign.h>
#include <stdatomic.h>
#include <stdbit.h>
#include <stdckdint.h>
#include <stdnoreturn.h>
#include <threads.h>
$guards
int main(void) { return 0; }"
	try "$std" "#include <threads.h>
#include <stdnoreturn.h>
#include <stdckdint.h>
#include <stdbit.h>
#include <stdatomic.h>
#include <stdalign.h>
$guards
int main(void) { return 0; }"
done

# Older-language canaries must not pull in the modern headers indirectly.
for std in c89 iso9899:199409 c99; do
	try "$std" "#include <stddef.h>
#include <limits.h>
#include <string.h>
#include <stdlib.h>
#if defined(_STDALIGN_H_) || defined(_STDNORETURN_H_) || \
    defined(_STDATOMIC_H_) || defined(_THREADS_H_) || \
    defined(_STDBIT_H_) || defined(_STDCKDINT_H_)
#error modern headers leaked into a legacy inclusion
#endif
int main(void) { return 0; }"
done

# C23 keywords must not be hidden by the former convenience macros.
try c23 "#include <stdalign.h>
#include <stdatomic.h>
#include <threads.h>
#ifndef _STDALIGN_H_
#error wrong stdalign.h
#endif
#ifndef _STDATOMIC_H_
#error wrong stdatomic.h
#endif
#ifndef _THREADS_H_
#error wrong threads.h
#endif
#if defined(alignas) || defined(alignof) || defined(thread_local) || \
    defined(__alignas_is_defined) || defined(__alignof_is_defined)
#error a C23 keyword or removed marker is still a macro
#endif
#ifdef ATOMIC_VAR_INIT
#error ATOMIC_VAR_INIT was removed in C23
#endif
#if __STDC_VERSION_STDATOMIC_H__ != 202311L || \
    __STDC_VERSION_THREADS_H__ != 202311L
#error wrong C23 header version
#endif
typedef char atomic_char8_matches[_Generic((atomic_char8_t *)0,
    _Atomic(unsigned char) *: 1, default: 0) ? 1 : -1];
alignas(16) int aligned_object;
thread_local int local_object;
int main(void) { return local_object + aligned_object; }"

# The pre-C23 initialization interface remains available in C11/C17.
for std in c11 c17; do
	try "$std" "#include <stdatomic.h>
#ifndef _STDATOMIC_H_
#error wrong stdatomic.h
#endif
#ifndef ATOMIC_VAR_INIT
#error missing pre-C23 atomic initializer
#endif
atomic_int initialized = ATOMIC_VAR_INIT(1);
int main(void) { return atomic_load(&initialized) - 1; }"
done

# C++: all six headers must be includable, and the stdbit type-generic
# interface is available through overloads.
CXX=${CXX:-c++}
if command -v "$CXX" >/dev/null 2>&1; then
	printf '%s\n' "#include <stdalign.h>
#include <stdatomic.h>
#include <stdbit.h>
#include <stdckdint.h>
#include <stdnoreturn.h>
#include <threads.h>
$guards
int main() {
	unsigned int z = stdc_leading_zeros(1u);
	int r = 0;
	bool o = ckd_add(&r, 1, 1);
	return (int)z + (int)o + r; }" | \
	    "$CXX" -std=c++17 -I"$PUBLIC_INCDIR" -Wall -Wextra \
	    -fsyntax-only -x c++ - || status=1
else
	printf 'SKIP: C++ compiler %s is unavailable\n' "$CXX" >&2
fi

exit $status
