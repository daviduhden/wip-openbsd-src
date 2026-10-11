#!/bin/sh
# Negative-compilation tests for the vendored FVWM <stdckdint.h>.
#
# C23 7.20.1 restricts the operands and the object pointed to by the result to
# integer types other than plain char, bool, a bit-precise integer type, and
# enumerated types.  These tests assert that the FVWM shim (and the compiler
# overflow builtins underneath it) reject the invalid uses that the supported
# compilers diagnose.  Diagnostic text is not compared, only acceptance.
#
# A plain char result/operand and an enumerated operand are knowingly not
# covered: GCC's builtins accept them without a diagnostic.  They are
# documented in the audit report and never occur in a valid C23 program.

set -u

TESTS_DIR=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
COMPAT_DIR="${TESTS_DIR}/../fvwm/compat"

CC=${CC:-clang}
CFLAGS=${CFLAGS:--O2 -pipe}
WARNINGS=${WARNINGS:--Wall -Wextra -Wpedantic}
CSTD=${CSTD:--std=c23}
FORCE=${FORCE:-FVWM_STDCKDINT_FORCE_FALLBACK}

tmp=$(mktemp -d) || exit 1
trap 'rm -rf "$tmp"' EXIT HUP INT TERM

fail=0

must_reject() {
	printf '%s\n' "$2" >"$tmp/case.c"
	if # shellcheck disable=SC2086
		"$CC" $CFLAGS $CSTD $WARNINGS -isystem "$COMPAT_DIR" -D"$FORCE" \
			-fsyntax-only "$tmp/case.c" >/dev/null 2>&1
	then
		printf 'FAIL: %s was accepted\n' "$1"
		fail=1
	else
		printf 'ok: %s rejected\n' "$1"
	fi
}

must_accept() {
	printf '%s\n' "$2" >"$tmp/case.c"
	if # shellcheck disable=SC2086
		"$CC" $CFLAGS $CSTD $WARNINGS -isystem "$COMPAT_DIR" -D"$FORCE" \
			-fsyntax-only "$tmp/case.c" >/dev/null 2>&1
	then
		printf 'ok: %s accepted\n' "$1"
	else
		printf 'FAIL: %s was rejected\n' "$1"
		fail=1
	fi
}

PRE='#include <stdckdint.h>'

must_accept valid_control "$PRE
int main(void) { int r = 0; return ckd_add(&r, 1, 2); }"

must_reject bool_destination "$PRE
int main(void) { _Bool b = 0; return ckd_add(&b, 1, 2); }"

must_reject float_destination "$PRE
int main(void) { float f = 0; return ckd_add(&f, 1, 2); }"

must_reject non_pointer_destination "$PRE
int main(void) { int r = 0; return ckd_add(r, 1, 2); }"

must_reject enum_destination "$PRE
enum e { A };
int main(void) { enum e v = A; return ckd_add(&v, 1, 2); }"

must_reject float_operand "$PRE
int main(void) { int r = 0; float f = 0; return ckd_add(&r, f, 1); }"

must_reject pointer_operand "$PRE
int main(void) { int r = 0; int *p = 0; return ckd_add(&r, p, 1); }"

must_accept size_t_destination "$PRE
#include <stddef.h>
int main(void) { size_t z = 0; return ckd_mul(&z, 2, 3); }"

if [ "$fail" -ne 0 ]; then
	printf 'stdckdint compile-fail: failures\n' >&2
	exit 1
fi
printf 'stdckdint compile-fail: all checks passed\n'
