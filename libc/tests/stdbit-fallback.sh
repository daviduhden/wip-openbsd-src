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
# Differential test of the portable fallback in <stdbit.h> and gen/stdbit.c:
# copies of both files are generated with every __stdc_has_builtin() probe
# forced to zero, so the type-generic macros take the _Generic path and the
# primitives take the bit-by-bit loop, and the full stdbit test suite is run
# against that build.  Host-side only; not part of the OpenBSD build.

set -eu

CC=${CC:-cc}
CFLAGS=${CFLAGS:--O2 -std=c23 -Wall -Wextra -Werror -Wpedantic}
HERE=$(dirname "$0")
TMP=$(mktemp -d "${TMPDIR:-/tmp}/stdbit-fallback.XXXXXX")
trap 'rm -rf "$TMP"' EXIT

sed -E 's/__stdc_has_builtin\(__builtin_[a-z0-9_]+\)/0/g' \
    "$HERE/../include/stdbit.h" >"$TMP/stdbit.h"
sed -E 's/__stdc_has_builtin\(__builtin_[a-z0-9_]+\)/0/g' \
    "$HERE/../lib/libc/gen/stdbit.c" >"$TMP/stdbit.c"
cp "$HERE/stdbit-tests.c" "$HERE/stdbit-link-helper.c" "$TMP/"

# Keep the builtins disabled only for our two files: passing -U__has_builtin
# is not possible (it is an operator), so the generated copies are used.
${CC} ${CFLAGS} -I"$TMP" "$TMP/stdbit.c" "$TMP/stdbit-link-helper.c" \
    "$TMP/stdbit-tests.c" -o "$TMP/stdbit-fallback"
"$TMP/stdbit-fallback"
