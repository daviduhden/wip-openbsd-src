#!/bin/ksh

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

# Build script for wip-openbsd-src -- a fork of OpenBSD userland
# utilities that compiles standalone on OpenBSD 7.9 or 8.0-beta and
# produces binaries intended to replace the system versions.
#
# This is a thin wrapper around make(1).  The Makefiles are the single
# definition of the build graph; this script only adds environment
# checks and a friendly interface.
#
# Usage:
#   ./build.ksh [pax|fvwm|all] [install|clean]
#
#   all      Build pax and fvwm (default)
#   pax      Build pax only
#   fvwm     Build fvwm only
#   install  Install built binaries and man pages to system paths;
#            run as root or via doas.  DESTDIR is honored if set.
#   clean    Remove build artifacts
#
# Examples:
#   ./build.ksh all
#   ./build.ksh all install
#   doas ./build.ksh install
#   DESTDIR=/tmp/stage ./build.ksh all install

set -eu

log() {
	print "$(date '+%Y-%m-%d %H:%M:%S')" "[INFO] $*"
}
warn() {
	print "$(date '+%Y-%m-%d %H:%M:%S')" "[WARN] $*" >&2
}
error() {
	print "$(date '+%Y-%m-%d %H:%M:%S')" "[ERROR] $*" >&2
}

SCRIPT_DIR=$(
	unset CDPATH
	cd -- "$(dirname -- "$0")" 2>/dev/null && pwd -P
)

# The active make implementation; overridable via the environment.
MAKE=${MAKE:-make}

check_openbsd() {
	if [ "$(uname -s)" != "OpenBSD" ]; then
		error "This build script targets OpenBSD only."
		exit 1
	fi
	OSREL=$(uname -r)
	case "$OSREL" in
	7.9 | 7.9-beta | 8.0 | 8.0-beta) ;;
	*)
		warn "Untested OpenBSD version: $OSREL. Expected 7.9 or 8.0-beta."
		;;
	esac
}

check_deps() {
	missing=""
	for dep in cc "$MAKE" install; do
		if ! command -v "$dep" >/dev/null 2>&1; then
			missing="$missing $dep"
		fi
	done
	if [ -n "$missing" ]; then
		error "Missing required tools:$missing"
		exit 1
	fi
}

make_args() {
	if [ -n "${DESTDIR:-}" ]; then
		print -- "DESTDIR=$DESTDIR"
	fi
}

build_pax() {
	log "Building pax..."
	"$MAKE" -C "$SCRIPT_DIR/pax" $(make_args)
	log "pax built successfully."
}

build_fvwm() {
	log "Building fvwm..."
	"$MAKE" -C "$SCRIPT_DIR/fvwm" $(make_args)
	log "fvwm built successfully."
}

do_install() {
	if [ "$(id -u)" -ne 0 ] && [ -z "${DESTDIR:-}" ]; then
		warn "Installing to system paths needs root; use doas or DESTDIR."
	fi
	log "Installing pax and fvwm..."
	"$MAKE" -C "$SCRIPT_DIR" $(make_args) install
	log "Install complete."
}

do_clean() {
	log "Cleaning build artifacts..."
	"$MAKE" -C "$SCRIPT_DIR" clean
	log "Clean complete."
}

usage() {
	print "Usage: $0 [pax|fvwm|all] [install|clean]"
	print ""
	print "  pax      Build pax (tar + cpio) only"
	print "  fvwm     Build fvwm only"
	print "  all      Build everything (default)"
	print "  install  Install built binaries to system paths"
	print "  clean    Remove build artifacts"
	exit 1
}

main() {
	[ $# -gt 0 ] || set -- all

	check_openbsd
	check_deps

	while [ $# -gt 0 ]; do
		case "$1" in
		all)
			build_pax
			build_fvwm
			;;
		pax)
			build_pax
			;;
		fvwm)
			build_fvwm
			;;
		install)
			do_install
			;;
		clean)
			do_clean
			;;
		*)
			usage
			;;
		esac
		shift
	done
}

main "$@"
