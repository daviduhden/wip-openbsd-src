#!/bin/sh
#	$OpenBSD$
#
# Host-side compile matrix for the six new headers.  Each header is compiled
# on its own and all of them together, under C11, C17 and C23, with strict
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
status=0

try()
{
	# $1 = standard, $2 = source text
	printf '%s\n' "$2" | $CC -std=$1 $CFLAGS -I"$SHIM" -fsyntax-only -x c - \
	    2>/tmp/hdr-matrix.err || {
		echo "FAIL: -std=$1: $2" >&2
		sed 's/^/    /' /tmp/hdr-matrix.err >&2
		status=1
	}
}

for h in stdalign stdnoreturn stdatomic stdbit stdckdint threads; do
	for std in c11 c17 c23; do
		try "$std" "#include <$h.h>
int main(void) { return 0; }"
	done
done

# The tree's header, not a compiler or system copy, must be selected.
try c11 "#include <stdalign.h>
#ifndef _STDALIGN_H_
#error wrong stdalign.h
#endif
int main(void) { return 0; }"
try c11 "#include <stdnoreturn.h>
#ifndef _STDNORETURN_H_
#error wrong stdnoreturn.h
#endif
int main(void) { return 0; }"
try c11 "#include <stdatomic.h>
#ifndef _STDATOMIC_H_
#error wrong stdatomic.h
#endif
int main(void) { return 0; }"
try c23 "#include <stdbit.h>
#ifndef _STDBIT_H_
#error wrong stdbit.h
#endif
int main(void) { return 0; }"
try c23 "#include <stdckdint.h>
#ifndef _STDCKDINT_H_
#error wrong stdckdint.h
#endif
int main(void) { return 0; }"
try c11 "#include <threads.h>
#ifndef _THREADS_H_
#error wrong threads.h
#endif
int main(void) { return 0; }"

# All six together, both orders.
try c23 "#include <stdalign.h>
#include <stdatomic.h>
#include <stdbit.h>
#include <stdckdint.h>
#include <stdnoreturn.h>
#include <threads.h>
int main(void) { return 0; }"
try c23 "#include <threads.h>
#include <stdnoreturn.h>
#include <stdckdint.h>
#include <stdbit.h>
#include <stdatomic.h>
#include <stdalign.h>
int main(void) { return 0; }"

# C++: the headers must be includable from C++ (compile-only; the
# type-generic stdbit interface is provided by overloads).
CXX=${CXX:-c++}
if command -v "$CXX" >/dev/null 2>&1; then
	for std in c++17 c++23; do
		printf '%s\n' '#include <stdalign.h>
#include <stdatomic.h>
#include <stdbit.h>
#include <stdckdint.h>
#include <stdnoreturn.h>
#include <threads.h>
int main() {
	unsigned int z = stdc_leading_zeros(1u);
	unsigned char f = stdc_bit_floor((unsigned char)200);
	bool h = stdc_has_single_bit(8u);
	int r = 0;
	bool o = ckd_add(&r, 20, 22);
	return (int)z + (int)f + (int)h + (int)o + r; }' | \
		    $CXX -std=$std $CFLAGS -I"$SHIM" \
		    -fsyntax-only -x c++ - 2>/tmp/hdr-cxx.err || {
			echo "FAIL: $CXX -std=$std (C++)" >&2
			sed 's/^/    /' /tmp/hdr-cxx.err >&2
			status=1
		}
	done
fi

rm -f /tmp/hdr-matrix.err /tmp/hdr-cxx.err
exit $status
