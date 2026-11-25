#!/bin/sh
# Exercise pathname substitutions through the built pax multicall binary.

if [ "$#" -ne 1 ]; then
	printf 'usage: %s path-to-pax\n' "$0" >&2
	exit 2
fi
case "$1" in
/*) pax=$1 ;;
*) pax=$PWD/$1 ;;
esac

tmp=$(mktemp -d) || exit 1
trap 'rm -rf "$tmp"' EXIT
trap 'exit 1' HUP INT TERM

(
	cd "$tmp" || exit 1
	: >./bc
	"$pax" -w -f sample.pax bc
) || exit 1

check() {
	got=$("$pax" -f "$tmp/sample.pax" -s "$1") || exit 1
	if [ "$got" != "$2" ]; then
		printf '%s: expected <%s>, got <%s>\n' "$1" "$2" "$got" >&2
		exit 1
	fi
}

check '/a*//g' bc
check '/a*/_/g' _b_c_
check '/a*/_/' _bc
check '/[bc]/_/g' __
check '/$/_/g' bc_
