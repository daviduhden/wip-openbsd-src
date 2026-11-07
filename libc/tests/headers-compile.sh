#!/bin/sh
#	$OpenBSD$
#
# Host-side compile matrix for the six new headers.  Each header is compiled
# on its own and all of them together, under C99, C11, C17 and C23, with strict
# warnings.  The shim directory contains only the headers under test, so the
# tree's headers are provably the ones selected (each test checks the tree's
# include guard).
#
# Not part of the OpenBSD build graph; see libc/tests/Makefile.

set -u

CC=${CC:-cc}
# __dead is provided by the OpenBSD <sys/cdefs.h>; the host C library has no
# such macro, so it is neutralised for the standalone host compile.
CFLAGS="-O2 -Wall -Wextra -Werror -Wpedantic -D__dead="
SHIM=.shim
HEADERS="stdalign stdnoreturn stdatomic stdbit stdckdint threads"
tmp=$(mktemp -d "${TMPDIR:-/tmp}/libc-headers.XXXXXXXX") || exit 1
trap 'rm -rf "$tmp"' 0
trap 'exit 1' HUP INT TERM
status=0

try()
{
	# $1 = standard, $2 = source text
	printf '%s\n' "$2" | $CC -std=$1 $CFLAGS -I"$SHIM" -fsyntax-only -x c - \
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
	for std in c99 c11 c17 c23; do
		try "$std" "#include <$h.h>
$check
int main(void) { return 0; }"
	done
done

# All six together, both orders and every supported C mode.
for std in c99 c11 c17 c23; do
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

# C++: the headers must be includable from C++ (compile-only; the
# type-generic stdbit interface is provided by overloads).
CXX=${CXX:-c++}
if command -v "$CXX" >/dev/null 2>&1; then
	for std in c++17 c++23; do
		printf '%s\n' "#include <stdalign.h>
#include <stdatomic.h>
#include <stdbit.h>
#include <stdckdint.h>
#include <stdnoreturn.h>
#include <threads.h>
$guards
int main() {
	unsigned int z = stdc_leading_zeros(1u);
	unsigned char f = stdc_bit_floor((unsigned char)200);
	bool h = stdc_has_single_bit(8u);
	int r = 0;
	bool o = ckd_add(&r, 20, 22);
	return (int)z + (int)f + (int)h + (int)o + r; }" | \
		    $CXX -std=$std $CFLAGS -I"$SHIM" \
		    -fsyntax-only -x c++ - 2>"$tmp/cxx.err" || {
			echo "FAIL: $CXX -std=$std (C++)" >&2
			sed 's/^/    /' "$tmp/cxx.err" >&2
			status=1
		}
	done
else
	printf 'SKIP: C++ compiler %s is unavailable\n' "$CXX" >&2
fi

exit $status
