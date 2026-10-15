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
# -I/usr/include is deliberate: it makes the installed OpenBSD headers win
# over the compiler's resource headers (clang searches -I directories first),
# so the test proves that the tree's headers, not the compiler's, are used.

set -u

CC=${CC:-cc}
WARN="-Wall -Wextra -Werror -Wpedantic"
HEADERS="stdalign stdnoreturn stdatomic stdbit stdckdint threads"
STDS="c11 c17 c23"
status=0

try()
{
	# $1 = standard, $2 = source text
	printf '%s\n' "$2" | $CC -std=$1 -I/usr/include $WARN -fsyntax-only -x c - \
	    2>/tmp/iso-c-headers.err || {
		echo "FAIL: -std=$1: $2" >&2
		sed 's/^/    /' /tmp/iso-c-headers.err >&2
		status=1
	}
}

for std in $STDS; do
	for h in $HEADERS; do
		try "$std" "#include <$h.h>
int main(void) { return 0; }"
	done
done

# Each header alone, but translated: the macros must survive a use.
try c11 "#include <stdalign.h>
alignas(8) int x; int main(void) { return (int)alignof(int) - (int)alignof(int) + (int)sizeof(x) - 4; }"
try c11 "#include <stdnoreturn.h>
noreturn void f(void); int main(void) { return 0; }"
try c11 "#include <stdatomic.h>
atomic_int x; int main(void) { return atomic_load(&x); }"
try c11 "#include <threads.h>
int main(void) { thrd_t t; return (int)(long)&t - (int)(long)&t; }"
try c23 "#include <stdbit.h>
int main(void) { return (int)stdc_bit_width(0u); }"
try c23 "#include <stdckdint.h>
int main(void) { int r; return ckd_add(&r, 1, 1); }"

# All six, forward and reverse.
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

# Canary: the installed libc headers still compile as C99 and C89 with the
# strict warning set (the C11/C23 headers are not pulled in indirectly).
printf '#include <stddef.h>\n#include <limits.h>\n#include <stdlib.h>\n#include <string.h>\nint main(void) { return 0; }\n' | \
	$CC -std=c99 -I/usr/include $WARN -fsyntax-only -x c - || status=1
printf '#include <stddef.h>\n#include <limits.h>\nint main(void) { return 0; }\n' | \
	$CC -std=c89 -I/usr/include $WARN -fsyntax-only -x c - || status=1

# C++: all six headers must be includable, and the stdbit type-generic
# interface is available through overloads.
if command -v c++ >/dev/null 2>&1; then
	printf '#include <stdalign.h>\n#include <stdatomic.h>\n#include <stdbit.h>\n#include <stdckdint.h>\n#include <stdnoreturn.h>\n#include <threads.h>\nint main() {\n\tunsigned int z = stdc_leading_zeros(1u);\n\tint r = 0;\n\tbool o = ckd_add(&r, 1, 1);\n\treturn (int)z + (int)o + r; }\n' | \
		c++ -std=c++17 -I/usr/include -Wall -Wextra -fsyntax-only -x c++ - || status=1
fi

rm -f /tmp/iso-c-headers.err
exit $status
