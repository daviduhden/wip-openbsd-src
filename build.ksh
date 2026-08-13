#!/bin/ksh

# Copyright (c) 2025-2026 David Uhden Collado <david@uhden.dev>
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
# Usage:
#   ./build.ksh [pax|fvwm|all] [install|clean]
#
# The resulting binaries are placed in ./build/ and can be installed
# to system paths with:
#   ./build.ksh install

set -e

log() {
	print "$(date '+%Y-%m-%d %H:%M:%S')" \
		"[INFO] $*"
}
warn() {
	print "$(date '+%Y-%m-%d %H:%M:%S')" \
		"[WARN] $*" >&2
}
error() {
	print "$(date '+%Y-%m-%d %H:%M:%S')" \
		"[ERROR] $*" >&2
}

SCRIPT_DIR=$(
	unset CDPATH
	cd -- "$(dirname -- "$0")" 2>/dev/null && pwd -P
)
BUILD_DIR="$SCRIPT_DIR/build"

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
	for dep in cc make install; do
		if ! command -v "$dep" >/dev/null 2>&1; then
			missing="$missing $dep"
		fi
	done
	if [ -n "$missing" ]; then
		error "Missing required tools:$missing"
		exit 1
	fi
}

build_pax() {
	log "Building pax..."
	mkdir -p "$BUILD_DIR/pax"
	cd "$SCRIPT_DIR/pax"
	make -f Makefile BINDIR="$BUILD_DIR/pax" \
		MANDIR="$BUILD_DIR/pax/man" \
		obj 2>/dev/null || true
	make -f Makefile BINDIR="$BUILD_DIR/pax" \
		MANDIR="$BUILD_DIR/pax/man"
	log "pax built successfully."
}

build_fvwm() {
	log "Building fvwm..."
	mkdir -p "$BUILD_DIR/fvwm"
	cd "$SCRIPT_DIR/fvwm"
	make -f Makefile X11BASE="${X11BASE:-/usr/X11R6}" \
		BINDIR="$BUILD_DIR/fvwm" \
		MANDIR="$BUILD_DIR/fvwm/man" \
		obj 2>/dev/null || true
	make -f Makefile X11BASE="${X11BASE:-/usr/X11R6}" \
		BINDIR="$BUILD_DIR/fvwm" \
		MANDIR="$BUILD_DIR/fvwm/man"
	log "fvwm built successfully."
}

install_bin() {
	local src="$1"
	local dst="$2"
	if [ -f "$src" ]; then
		install -m 0555 -o root -g bin "$src" "$dst"
		log "Installed $src -> $dst"
	else
		warn "Binary not found: $src"
	fi
}

install_pax() {
	log "Installing pax to system paths..."
	install_bin "$BUILD_DIR/pax/pax" "/bin/pax"
	ln -sf /bin/pax /bin/tar 2>/dev/null || true
	ln -sf /bin/pax /bin/cpio 2>/dev/null || true
	log "pax installed. Symlinks tar/cpio created."
}

install_fvwm() {
	log "Installing fvwm to system paths..."
	local fvwmdir="${FVWMLIBDIR:-/usr/X11R6/lib/X11/fvwm}"
	mkdir -p "$fvwmdir"
	install_bin "$BUILD_DIR/fvwm/fvwm" "/usr/X11R6/bin/fvwm"
	install_bin "$BUILD_DIR/fvwm/fvwm_exec" "$fvwmdir/fvwm_exec"
	for m in "$BUILD_DIR"/fvwm/modules/*; do
		[ -f "$m" ] || continue
		install_bin "$m" "$fvwmdir/$(basename "$m")"
	done
	log "fvwm installed."
}

clean() {
	log "Cleaning build artifacts..."
	rm -rf "$BUILD_DIR"
	cd "$SCRIPT_DIR/pax"
	make -f Makefile clean 2>/dev/null || true
	cd "$SCRIPT_DIR/fvwm"
	make -f Makefile clean 2>/dev/null || true
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
			[ -d "$BUILD_DIR/pax" ] && install_pax
			[ -d "$BUILD_DIR/fvwm" ] && install_fvwm
			;;
		clean)
			clean
			;;
		*)
			usage
			;;
		esac
		shift
	done
}

main "$@"
