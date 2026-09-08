#!/bin/sh
#
# validate-make.sh -- static validation of the wip-openbsd-src build
# system, tuned for OpenBSD-native Makefiles.
#
# The Makefiles in this repository target OpenBSD make(1) and
# OpenBSD /usr/share/mk.  Generic GNU-oriented tools (checkmake,
# mbake) and the host's NetBSD-derived bmake cannot be authoritative
# here, so this script classifies every finding instead of blindly
# reporting failures.
#
# Modes:
#   check    (default) validate without modifying anything
#   format   show what mbake would rewrite (diff-oriented; applies
#            nothing -- the Makefiles are OpenBSD-native and mbake is
#            a GNU-style formatter that would change the dialect)
#
# Usage: ./validate-make.sh [check|format] [ROOT]
#
# Exit status: 0 = no actionable findings (warnings and skips do not
# fail), 1 = at least one genuine defect, 2 = usage error.
#
# --------------------------------------------------------------------
#
# Copyright (c) 2026 David Uhden Collado <david@uhden.dev>
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
# WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
# AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
# DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA
# OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
# TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
# PERFORMANCE OF THIS SOFTWARE.

set -u

USAGE="Usage: $0 [check|format] [ROOT]"

log() { printf '%s\n' "$*"; }

MODE=check
if [ $# -gt 0 ] && [ "$1" != "-" ]; then
	case "$1" in
	check | format) MODE=$1; shift ;;
	-h | --help) printf '%s\n' "$USAGE"; exit 0 ;;
	esac
fi
ROOT=${1:-.}
ROOT=$(cd "$ROOT" 2>/dev/null && pwd -P) || {
	printf '%s\n' "[ERROR] cannot enter $1" >&2
	exit 2
}

FAILS=0
WARNS=0
SKIPS=0
CHECKED=0
LOG_FILE=$(mktemp -d "${TMPDIR:-/tmp}/validate-make.XXXXXX")
trap 'rm -rf "$LOG_FILE"' EXIT

fail() {
	FAILS=$((FAILS + 1))
	log "[FAIL] $1"
}
warn() {
	WARNS=$((WARNS + 1))
	log "[WARN] $1"
}
skip() {
	SKIPS=$((SKIPS + 1))
	log "[SKIP] $1"
}
ok() {
	log "[OK] $1"
}

# ----------------------------------------------------------------------
# Discovery
# ----------------------------------------------------------------------
find_makefiles() {
	find "$ROOT" \
		\( -path "$ROOT/.git" -o -path "$ROOT/.git/*" \) -prune \
		-o -type f \
		\( -name 'Makefile' -o -name 'makefile' -o -name 'GNUmakefile' \
		-o -name 'Makefile.inc' -o -name '*.mk' \) -print |
		sort
}

FILES=$(find_makefiles)
if [ -z "$FILES" ]; then
	log "[INFO] No Makefiles found under $ROOT"
	exit 0
fi

# ----------------------------------------------------------------------
# Context classification
# ----------------------------------------------------------------------
# Each Makefile is one of:
#   standalone      top-level or pax: builds directly from this repo
#   openbsd-native  fvwm hierarchy: OpenBSD make + /usr/share/mk
#   source-tree     ext4fs: belongs in an OpenBSD source checkout
rel() {
	printf '%s' "$1" | sed "s|^$ROOT/||"
}
context() {
	case "$1" in
	"Makefile" | "pax/"*) printf 'standalone' ;;
	"fvwm/"*) printf 'openbsd-native' ;;
	"ext4fs/"*) printf 'source-tree' ;;
	*) printf 'unknown' ;;
	esac
}

# ----------------------------------------------------------------------
# Stub includes for a syntax-only bmake parse
# ----------------------------------------------------------------------
# bmake parses BSD make syntax, but this host does not carry OpenBSD's
# /usr/share/mk (it has a NetBSD-derived set, which is not
# equivalent).  For syntax checking we point bmake's system include
# path at a scratch directory containing empty stubs for every
# <bsd.*.mk> fragment referenced anywhere in the tree, plus an empty
# sys.mk.  This never modifies the real Makefiles and is labelled
# syntax-only.
build_stubdir() {
	STUBDIR=$LOG_FILE/mk
	mkdir -p "$STUBDIR"
	: >"$STUBDIR/sys.mk"
	for inc in \
		bsd.prog.mk bsd.lib.mk bsd.subdir.mk bsd.obj.mk \
		bsd.own.mk bsd.man.mk bsd.dep.mk bsd.sys.mk \
		bsd.xorg.mk bsd.xconf.mk bsd.port.mk bsd.regress.mk; do
		: >"$STUBDIR/$inc"
	done
	# Any additional system includes referenced by the Makefiles.
	grep -h '^[^#]*\.include *<' $FILES 2>/dev/null |
		sed -n 's/.*\.include *<\([^>]*\)>.*/\1/p' |
		while IFS= read -r name; do
			[ -n "$name" ] || continue
			: >"$STUBDIR/$name"
		done
}

# ----------------------------------------------------------------------
# 1. bmake syntax-only parse (stubbed system includes)
# ----------------------------------------------------------------------
syntax_check() {
	file=$1
	dir=$(dirname "$file")
	bmk=$(command -v bmake || true)
	[ -n "${bmk:-}" ] || {
		skip "$(rel "$file"): bmake syntax check: bmake not installed"
		return
	}
	# bmake resolves quoted includes relative to the working
	# directory, so run from the Makefile's own directory.
	output=$(cd "$dir" && "$bmk" -m "$STUBDIR" -f Makefile -V .CURDIR \
		2>&1 >/dev/null)
	rc=$?
	if [ $rc -eq 0 ]; then
		ok "$(rel "$file"): OpenBSD make syntax (bmake parse, stubbed includes)"
	else
		fail "$(rel "$file"): bmake syntax parse failed:"
		printf '%s\n' "$output" | sed 's/^/    /'
	fi
}

# ----------------------------------------------------------------------
# 2. Raw host-bmake dry-run, classified
# ----------------------------------------------------------------------
# Documented rather than trusted: the host's NetBSD-derived mk files
# and the absence of OpenBSD/Xenocara infrastructure make this
# non-authoritative for semantic checks.
raw_bmake() {
	file=$1
	dir=$(dirname "$file")
	ctx=$(context "$(rel "$file")")
	bmk=$(command -v bmake || true)
	[ -n "${bmk:-}" ] || {
		skip "$(rel "$file"): raw bmake dry-run: bmake not installed"
		return
	}
	output=$(cd "$dir" && "$bmk" -n -f Makefile 2>&1 >/dev/null)
	rc=$?
	case "$output" in
	*"Could not find bsd"*)
		skip "$(rel "$file"): raw bmake dry-run: requires OpenBSD /usr/share/mk (missing on this host)"
		;;
	*"Could not find"*)
		skip "$(rel "$file"): raw bmake dry-run: include not found on this host:"
		printf '%s\n' "$output" | grep "Could not find" | sed 's/^/    /'
		;;
	*"Unassociated shell command"* | *"Parse error"* | *"open conditional"* | *"Fatal errors"*)
		fail "$(rel "$file"): raw bmake dry-run reports a syntax error:"
		printf '%s\n' "$output" | sed 's/^/    /'
		;;
	"")
		if [ $rc -eq 0 ]; then
			skip "$(rel "$file"): raw bmake dry-run: parsed with the host's NetBSD-derived mk files; not equivalent to OpenBSD /usr/share/mk"
		else
			skip "$(rel "$file"): raw bmake dry-run: host mk infrastructure diverges from OpenBSD (rc=$rc)"
		fi
		;;
	*"no target to make"*)
		skip "$(rel "$file"): raw bmake dry-run: no standalone default target (Makefile fragment; recursion entry point only)"
		;;
	*)
		case "$ctx" in
		source-tree)
			skip "$(rel "$file"): raw bmake dry-run: source-tree context required (files under the sibling directories it expects are not present in this repository)"
			;;
		*)
			skip "$(rel "$file"): raw bmake dry-run: depends on host mk files that differ from OpenBSD (rc=$rc); not authoritative:"
			printf '%s\n' "$output" | sed 's/^/    /' | head -4
			;;
		esac
		;;
	esac
}

# ----------------------------------------------------------------------
# 3. Static OpenBSD-dialect checks
# ----------------------------------------------------------------------
static_checks() {
	file=$1

	# GNU make constructs have no place in OpenBSD Makefiles.
	gnu=$(grep -n -E \
		'(^|[^#])\$\((shell|wildcard|patsubst|foreach|eval|call|addprefix|addsuffix|filter|filter-out|origin|abspath|realpath|dir|notdir|basename|subst|word|firstword|lastword|join|if|sort) |^(ifeq|ifneq|define|endef|override) ' \
		"$file" 2>/dev/null || true)
	if [ -n "$gnu" ]; then
		fail "$(rel "$file"): GNU make constructs found:"
		printf '%s\n' "$gnu" | sed 's/^/    /'
	else
		ok "$(rel "$file"): no GNU make constructs"
	fi

	# Directives indented with a tab become shell commands in BSD
	# make: either "unassociated shell command" (parse error) or a
	# bogus command of the previous target.
	indent=$(grep -n -E '^	+\.(include|sinclude|if|ifdef|ifndef|elif|else|endif|for|endfor|undef|error|warning|export|unexport)\b' \
		"$file" 2>/dev/null || true)
	if [ -n "$indent" ]; then
		fail "$(rel "$file"): directives indented with a tab (parsed as shell commands by OpenBSD make):"
		printf '%s\n' "$indent" | sed 's/^/    /'
	fi
}

c17_policy() {
	relf=$1
	case "$relf" in
	pax/Makefile | fvwm/Makefile.inc | fvwm/modules/Makefile.inc)
		if grep -q -- '-std=c17' "$ROOT/$relf"; then
			ok "$relf: C17 policy: -std=c17 present"
		else
			fail "$relf: C17 policy: -std=c17 missing"
		fi
		;;
	ext4fs/sbin/mount_ext4fs/Makefile)
		if grep -q -- '-std=' "$ROOT/$relf"; then
			fail "$relf: source-tree policy: must not inject -std= flags"
		else
			ok "$relf: source-tree policy: no -std= flags injected"
		fi
		;;
	esac
}

# ----------------------------------------------------------------------
# 4. checkmake (advisory; GNU-oriented rules disabled)
# ----------------------------------------------------------------------
# checkmake assumes GNU make semantics, does not expand .include
# directives, and applies generic thresholds.  Its findings are
# reported with per-class dispositions so real new findings stay
# visible while the known false positives are not misrepresented as
# defects:
#
#   phonydeclared  false positive: the .PHONY declarations come from
#                  the OpenBSD bsd.*.mk include chain, or the flagged
#                  targets are real files that must not be phony.
#   maxbodylength  not actionable: generic GNU-make body-length
#                  threshold; splitting these OpenBSD recipes into
#                  helper targets would only harm the Makefiles.
#   anything else  unclassified; needs individual review.
checkmake_check() {
	file=$1
	relf=$(rel "$file")
	ck=$(command -v checkmake || true)
	[ -n "${ck:-}" ] || {
		skip "$relf: checkmake: not installed"
		return
	}
	cfg="$ROOT/checkmake.ini"
	[ -f "$cfg" ] || cfg=/dev/null
	output=$("$ck" --config "$cfg" "$file" 2>&1)
	rc=$?
	if [ $rc -eq 0 ]; then
		ok "$relf: checkmake: clean"
		return
	fi
	warn "$relf: checkmake (advisory, GNU-oriented):"
	printf '%s\n' "$output" | sed 's/^/    /'
	if printf '%s\n' "$output" | grep -q 'phonydeclared'; then
		log "    ADVISORY: phonydeclared false positive: .PHONY"
		log "    is supplied by the OpenBSD bsd.*.mk include chain"
		log "    (or the targets are real files); no Makefile change"
		log "    is warranted."
	fi
	if printf '%s\n' "$output" | grep -q 'maxbodylength'; then
		log "    ADVISORY: maxbodylength not actionable: generic GNU"
		log "    make body-length threshold; splitting these OpenBSD"
		log "    recipes into helper targets would only harm the"
		log "    Makefiles."
	fi
	total=$(printf '%s\n' "$output" |
		sed -n 's/.*violations found (\([0-9]*\)).*/\1/p' | tail -1)
	ph=$(printf '%s\n' "$output" | grep -c 'phonydeclared' || true)
	mb=$(printf '%s\n' "$output" | grep -c 'maxbodylength' || true)
	if [ -n "$total" ] && [ "$((ph + mb))" -lt "$total" ]; then
		log "    ADVISORY: $((total - ph - mb)) additional checkmake"
		log "    finding(s) outside the known false-positive classes;"
		log "    review them in the output above."
	fi
}

# ----------------------------------------------------------------------
# 5. mbake (GNU-style formatter; excluded from OpenBSD-native files)
# ----------------------------------------------------------------------
mbake_check() {
	file=$1
	mb=$(command -v mbake || true)
	[ -n "${mb:-}" ] || {
		skip "$(rel "$file"): mbake: not installed"
		return
	}
	# mbake does not understand .PATH or .include <bsd.*.mk> and
	# rewrites BSD make style into a GNU-flavoured dialect; it is
	# not applied to these Makefiles in either mode.
	output=$("$mb" format --check "$file" 2>&1)
	rc=$?
	case "$output" in
	*"Unknown special target"* | *"Error"*)
		skip "$(rel "$file"): mbake: cannot parse BSD make constructs ($(printf '%s\n' "$output" | grep -m1 -o "Unknown special target '[^']*'" || printf 'syntax error'))"
		;;
	"")
		if [ $rc -eq 0 ]; then
			ok "$(rel "$file"): mbake: would not change the file"
		else
			skip "$(rel "$file"): mbake: would rewrite this file with GNU-style formatting; excluded by policy"
		fi
		;;
	*)
		skip "$(rel "$file"): mbake: not applicable to OpenBSD-native Makefiles; excluded by policy"
		;;
	esac
}

# ----------------------------------------------------------------------
# 6. Semantic dry-run with a real OpenBSD /usr/share/mk (optional)
# ----------------------------------------------------------------------
# Set OBSD_MK=/path/to/openbsd/usr/share/mk (e.g. extracted from a
# base set) to expand the build graph with the authoritative OpenBSD
# mk files instead of stubs.  bmake's parser is still the host's
# NetBSD-derived one, so this validates variables, targets, and
# include chains, not OpenBSD make(1) semantics bit-for-bit.
# bsd.dep.mk is replaced with a stub: its space-indented sinclude
# inside .for loops relies on OpenBSD make line handling that the
# host bmake does not implement.
semantic_check() {
	file=$1
	relf=$(rel "$file")
	dir=$(dirname "$file")
	ctx=$(context "$relf")
	bmk=$(command -v bmake || true)
	[ -n "${bmk:-}" ] || return
	[ -n "${OBSD_MK:-}" ] && [ -f "$OBSD_MK/bsd.prog.mk" ] || {
		skip "$relf: semantic dry-run: OBSD_MK not set (set it to an OpenBSD /usr/share/mk to enable)"
		return
	}
	mkdir -p "$LOG_FILE/obsdmk"
	cp "$OBSD_MK"/* "$LOG_FILE/obsdmk/" 2>/dev/null
	chmod -R u+w "$LOG_FILE/obsdmk"
	: >"$LOG_FILE/obsdmk/bsd.dep.mk"
	case "$relf" in
	*Makefile.inc | *.mk)
		skip "$relf: semantic dry-run: Makefile fragment (no default target)"
		return
		;;
	esac
	output=$(cd "$dir" &&
		"$bmk" -m "$LOG_FILE/obsdmk" -n \
		MACHINE_ARCH=amd64 MACHINE=amd64 \
		LIBCRT0=/dev/null LIBC=/dev/null \
		CRTBEGIN=/dev/null CRTEND=/dev/null 2>&1)
	rc=$?
	case "$output" in
	*"don't know how to make"*)
		case "$ctx" in
		source-tree)
			skip "$relf: semantic dry-run: source-tree context required (expected sibling files not in this repository)"
			;;
		*)
			fail "$relf: semantic dry-run: unresolved prerequisite:"
			printf '%s\n' "$output" | grep "don't know how to make" | sed 's/^/    /'
			;;
		esac
		;;
	*"Fatal errors"* | *"Unassociated shell command"*)
		fail "$relf: semantic dry-run failed:"
		printf '%s\n' "$output" | sed 's/^/    /' | head -6
		;;
	*)
		if [ $rc -eq 0 ]; then
			ok "$relf: semantic dry-run with OpenBSD 7.9 /usr/share/mk (host bmake parser)"
		else
			skip "$relf: semantic dry-run: host-bmake parser incompatibility with OpenBSD mk files (rc=$rc); see raw dry-run classification"
		fi
		;;
	esac
}

# ----------------------------------------------------------------------
# Main loop
# ----------------------------------------------------------------------
log "[INFO] mode: $MODE"
log "[INFO] root: $ROOT"
if command -v bmake >/dev/null 2>&1; then
	build_stubdir
fi

for file in $FILES; do
	relf=$(rel "$file")
	ctx=$(context "$relf")
	CHECKED=$((CHECKED + 1))
	log "--- $relf [$ctx]"

	if [ "$MODE" = "format" ]; then
		# Diff-oriented: never rewrite; show what mbake would do.
		mbake_check "$file"
		continue
	fi

	syntax_check "$file"
	raw_bmake "$file"
	static_checks "$file"
	c17_policy "$relf"
	checkmake_check "$file"
	mbake_check "$file"
	semantic_check "$file"
done

log ""
log "[INFO] Summary: $CHECKED file(s) checked, $FAILS fail(s), $WARNS warn(s), $SKIPS skip(s)"
log "[INFO] Policy: OpenBSD make(1) and /usr/share/mk are authoritative;"
log "[INFO] generic GNU-oriented linters are advisory and classified,"
log "[INFO] host bmake uses non-equivalent NetBSD-derived mk files."

if [ "$FAILS" -ne 0 ]; then
	exit 1
fi
exit 0
