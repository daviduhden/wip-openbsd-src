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

# Privileged commands must not be resolved through an untrusted PATH.
PATH=/usr/bin:/bin:/usr/sbin:/sbin:/usr/local/bin
export PATH

log() {
	print "$(date '+%Y-%m-%d %H:%M:%S')" \
		"[INFO] $*" >&2
}
warn() {
	print "$(date '+%Y-%m-%d %H:%M:%S')" \
		"[WARN] $*" >&2
}
error() {
	print "$(date '+%Y-%m-%d %H:%M:%S')" \
		"[ERROR] $*" >&2
}

# Verify that the script is run as root
check_root() {
	if [ "$(id -u)" -ne 0 ]; then
		error "This script must be run as root."
		exit 1
	fi
}

check_openbsd() {
	if [ "$(uname -s)" != "OpenBSD" ]; then
		error "This mode is supported only on OpenBSD."
		exit 1
	fi
}

# Function to select which repository to checkout (src, xenocara or www)
select_repository_to_checkout() {
	SELECTED_REPO=""
	log "Select the repository to clone (checkout):"
	select SELECTED_REPO in src xenocara www; do
		if [ -n "$SELECTED_REPO" ]; then
			log "You selected $SELECTED_REPO"
			break
		else
			warn "Invalid selection. Please try again."
		fi
	done
	[ -n "$SELECTED_REPO" ]
}

# Use the same lock for checkout replacement and component copies.
lock_path() {
	case "$1" in
	/usr/src | /usr/xenocara | /usr/www)
		print -r -- "/usr/.wip-${1##*/}.lock"
		;;
	*)
		print -r -- "$1/.wip-copy.lock"
		;;
	esac
}

# Fetch into a sibling directory while the old checkout is still in place.
# The exit trap restores the old tree if publication leaves the target absent.
checkout_selected_repository() (
	typeset target lock stage="" backup="" answer mounts
	case "$SELECTED_REPO" in
	src | xenocara | www) ;;
	*) return 1 ;;
	esac
	target="/usr/$SELECTED_REPO"
	lock=$(lock_path "$target") || return 1
	mkdir "$lock" || return 1
	trap '
		if [ -n "$backup" ] && [ -d "$backup/tree" ] &&
		    [ ! -e "$target" ]; then
			mv "$backup/tree" "$target" ||
			    error "Restore the previous tree from $backup/tree"
		fi
		[ -z "$stage" ] || rm -rf "$stage"
		rmdir "$lock"
	' EXIT
	trap 'exit 1' HUP INT TERM

	if [ -L "$target" ] || { [ -e "$target" ] && [ ! -d "$target" ]; }; then
		error "Invalid checkout destination: $target"
		return 1
	fi
	mounts=$(mount) || return 1
	if print -r -- "$mounts" | awk -v path="$target" '
	    $2 == "on" && ($3 == path || index($3, path "/") == 1) { found = 1 }
	    END { exit !found }'; then
		error "Refusing to replace a mounted tree: $target"
		return 1
	fi
	if [ -d "$target" ]; then
		print -r -- "Replace $target and keep the previous tree as a backup? [y/N]" >&2
		IFS= read -r answer || return 1
		case "$answer" in
		y | Y | yes | YES) ;;
		*) return 1 ;;
		esac
	fi
	stage=$(mktemp -d "/usr/.wip-$SELECTED_REPO.XXXXXXXX") || return 1
	(cd "$stage" && cvs -qd "${CVSROOT:-anoncvs@anoncvs.eu.openbsd.org:/cvs}" \
		checkout -P "$SELECTED_REPO") || return 1
	[ -d "$stage/$SELECTED_REPO/CVS" ] || return 1

	if [ -d "$target" ]; then
		backup=$(mktemp -d "/usr/.wip-$SELECTED_REPO.backup.XXXXXXXX") ||
			return 1
		mv "$target" "$backup/tree" || return 1
	fi
	mv "$stage/$SELECTED_REPO" "$target" || return 1
	[ -z "$backup" ] || log "Previous tree saved in $backup/tree"
)

# Function to ask if user wants to copy from wip-openbsd-src (optional)
ask_copy_from_wip() {
	log "Copy a directory from 'wip-openbsd-src'" \
		"into the checked-out tree?"
	select ANSWER in \
		"One directory" "Selected directories" \
		"All directories" "No"; do
		case "$ANSWER" in
		"One directory")
			DO_COPY=1
			COPY_ALL=0
			COPY_LIST=0
			break
			;;
		"Selected directories")
			DO_COPY=1
			COPY_ALL=0
			COPY_LIST=1
			break
			;;
		"All directories")
			DO_COPY=1
			COPY_ALL=1
			COPY_LIST=0
			break
			;;
		No)
			DO_COPY=0
			break
			;;
		*) warn "Invalid selection. Please try again." ;;
		esac
	done
}

# Resolve local wip-openbsd-src path. Checks, in order:
#   1. WIP_OPENBSD_SRC_DIR environment variable
#   2. Script's own directory (if named wip-openbsd-src)
#   3. Common locations under $HOME and /usr
#   4. Current working directory or its parent
#   5. Limited filesystem search
resolve_local_src_dir() {
	typeset dir

	# 1. Explicit environment variable
	if [ -n "${WIP_OPENBSD_SRC_DIR:-}" ] &&
		[ -d "$WIP_OPENBSD_SRC_DIR/.git" ]; then
		log "Using WIP_OPENBSD_SRC_DIR=$WIP_OPENBSD_SRC_DIR"
		print "$WIP_OPENBSD_SRC_DIR"
		return 0
	fi

	# 2. Script directory (walk up to find the repo)
	dir=$(
		unset CDPATH
		cd -- "$(dirname -- "$0")" 2>/dev/null && pwd -P
	)
	while [ -n "$dir" ] && [ "$dir" != "/" ]; do
		if [ "$(basename "$dir")" = "wip-openbsd-src" ] &&
			[ -d "$dir/.git" ]; then
			log "Found wip-openbsd-src at $dir"
			print "$dir"
			return 0
		fi
		dir=$(dirname "$dir")
	done

	# 3. Common locations
	for dir in \
		"${HOME:-}/wip-openbsd-src" \
		/root/wip-openbsd-src \
		/usr/wip-openbsd-src \
		"${HOME:-}/git/wip-openbsd-src"; do
		if [ -d "$dir/.git" ]; then
			log "Found wip-openbsd-src at $dir"
			print "$dir"
			return 0
		fi
	done

	# 4. Scan home directories
	for dir in /home/*/wip-openbsd-src /home/*/git/wip-openbsd-src; do
		if [ -d "$dir/.git" ]; then
			log "Found wip-openbsd-src at $dir"
			print "$dir"
			return 0
		fi
	done

	# 5. Current directory or parent
	if [ -d "$PWD/.git" ] &&
		[ "$(basename "$PWD")" = "wip-openbsd-src" ]; then
		print "$PWD"
		return 0
	fi
	if [ -d "$PWD/wip-openbsd-src/.git" ]; then
		print "$PWD/wip-openbsd-src"
		return 0
	fi

	# 6. Limited search under /home, /usr, /root
	warn "Searching /home and /usr for wip-openbsd-src..."
	dir=$(find /home /usr /root -maxdepth 5 \
		-type d -name "wip-openbsd-src" \
		-exec test -d '{}/.git' ';' \
		-print 2>/dev/null | sed -n '1p')
	if [ -n "$dir" ]; then
		log "Found wip-openbsd-src at $dir"
		print "$dir"
		return 0
	fi

	error "wip-openbsd-src directory not found." \
		"Set WIP_OPENBSD_SRC_DIR to its location."
	exit 1
}

# Change to the wip-openbsd-src directory.
move_to_wip_openbsd_src() {
	wip_openbsd_src_dir=$(resolve_local_src_dir)
	cd "$wip_openbsd_src_dir" || {
		error "Could not cd to $wip_openbsd_src_dir"
		exit 1
	}
}

# Function to list directories in the current directory and select one
list_directories() {
	typeset directories
	directories=$(list_all_directories) || return 1
	log "Select a directory to copy from wip-openbsd-src:"
	select DIRECTORY in $directories; do
		if [ -n "$DIRECTORY" ]; then
			log "You selected $DIRECTORY"
			DIRECTORY=${DIRECTORY%/} # Remove the trailing slash
			break
		else
			warn "Invalid selection. Please try again."
		fi
	done
}

# List known components by their canonical upstream destinations.
# Keep the standalone source/build layout in this repository unchanged.
list_all_directories() {
	typeset component
	for component in src/bin/pax xenocara/app/fvwm; do
		resolve_component "$component" || return 1
		if [ -z "${TARGET_KIND:-}" ] ||
			[ "$TARGET_KIND" = "$COMPONENT_TREE" ]; then
			print -r -- "$component"
		fi
	done
}

# Function to prompt for a space-separated list of directories.
prompt_selected_directories() {
	log "Enter component paths separated by spaces (src/bin/pax or xenocara/app/fvwm):"
	print -n "> "
	read -r SELECTED_DIRECTORIES
	[ -n "${SELECTED_DIRECTORIES:-}" ] || {
		error "No directories entered."
		exit 1
	}
}

# Function to choose between /usr/src or /usr/xenocara as target tree
# Offer trees that exist (the cloned one, or both if present)
choose_target_tree() {
	options=""
	[ -d /usr/src ] && options="$options /usr/src"
	[ -d /usr/xenocara ] && options="$options /usr/xenocara"
	if [ -z "$options" ]; then
		error "No destination trees available under /usr."
		exit 1
	fi
	log "Select the target tree for the copy:"
	select TARGET_TREE in $options; do
		if [ -n "$TARGET_TREE" ]; then
			log "You selected $TARGET_TREE"
			break
		else
			warn "Invalid selection. Please try again."
		fi
	done
}

# Exact mapping avoids placing pax at the src root, or FVWM in src.
resolve_component() {
	case "$1" in
	pax | src/bin/pax)
		COMPONENT_TREE=src
		COMPONENT_PATH=bin/pax
		COMPONENT_DIR="pax"
		;;
	fvwm | xenocara/app/fvwm)
		COMPONENT_TREE=xenocara
		COMPONENT_PATH=app/fvwm
		COMPONENT_DIR=fvwm
		;;
	*)
		error "Unknown component: $1"
		return 1
		;;
	esac
	if [ -L "$COMPONENT_DIR" ] || [ -L "$COMPONENT_DIR/Makefile" ] ||
		[ ! -f "$COMPONENT_DIR/Makefile" ]; then
		error "Missing or symlinked component: $COMPONENT_DIR"
		return 1
	fi
}

detect_target_tree() {
	[ -f "$TARGET_TREE/Makefile" ] || {
		error "Not a source checkout: $TARGET_TREE"
		return 1
	}
	if [ -d "$TARGET_TREE/bin" ] && [ -d "$TARGET_TREE/sys" ]; then
		TARGET_KIND=src
	elif [ -d "$TARGET_TREE/app" ] && [ -d "$TARGET_TREE/lib" ]; then
		TARGET_KIND=xenocara
	else
		error "Cannot identify src or xenocara tree: $TARGET_TREE"
		return 1
	fi
}

validate_selection() {
	resolve_component "$1" || return 1
	if [ "$COMPONENT_TREE" != "$TARGET_KIND" ]; then
		error "$1 belongs in $COMPONENT_TREE, not $TARGET_KIND"
		return 1
	fi
}

# Compare component-relative paths in staging for later CVS bookkeeping.
# Exclude CVS metadata and use the same collation order for sort and comm.
# The manifests track path additions and removals. CVS detects file edits.
write_change_lists() {
	typeset old=$1 new=$2 stage=$3 kind side tree
	for side in old new; do
		case "$side" in
		old) tree=$old ;;
		new) tree=$new ;;
		esac
		for kind in files dirs; do
			if [ "$kind" = files ]; then
				(cd "$tree" && find . -type d -name CVS -prune -o \
					\( -type f -o -type l \) -print) >"$stage/paths" ||
					return 1
			else
				(cd "$tree" && find . -type d -name CVS -prune -o \
					-type d -print) >"$stage/paths" || return 1
			fi
			sed -e 's|^\./||' -e '/^\.$/d' "$stage/paths" >"$stage/relative" ||
				return 1
			LC_ALL=C sort "$stage/relative" >"$stage/$side.$kind" ||
				return 1
		done
	done
	mkdir "$stage/changes" || return 1
	LC_ALL=C comm -13 "$stage/old.files" "$stage/new.files" \
		>"$stage/changes/created.txt" &&
		LC_ALL=C comm -23 "$stage/old.files" "$stage/new.files" \
			>"$stage/changes/deleted.txt" &&
		LC_ALL=C comm -13 "$stage/old.dirs" "$stage/new.dirs" \
			>"$stage/changes/created-dirs.txt"
}

# Stage the component and manifests before moving either live directory.
# Each rename stays on the target filesystem. Until the staged manifest
# directory is published, the exit trap attempts to restore both backups.
copy_directory() (
	typeset target parent stage="" backup="" lock cvs reports previous=""
	typeset publishing=0 parent_dev path path_dev source_root
	validate_selection "$DIRECTORY" || return 1
	parent=${COMPONENT_PATH%/*}
	target="$TARGET_TREE/$COMPONENT_PATH"
	lock=$(lock_path "$TARGET_TREE") || return 1
	mkdir "$lock" || return 1
	trap '
		if [ "$publishing" -eq 1 ] && [ -d "$stage/changes" ]; then
			if [ ! -d "$stage/component" ] && [ -d "$target" ]; then
				mv "$target" "$stage/component" ||
				    error "Could not move the new component back to $stage"
			fi
			if [ -d "$backup/component" ] && [ ! -e "$target" ]; then
				mv "$backup/component" "$target" ||
				    error "Restore the previous component from $backup/component"
			fi
			if [ -n "$previous" ] && [ -d "$previous" ]; then
				mv "$previous" "$reports" ||
				    error "Restore the previous manifests from $previous"
			fi
		fi
		[ -z "$stage" ] || rm -rf "$stage"
		rmdir "$lock"
	' EXIT
	trap 'exit 1' HUP INT TERM
	if [ ! -d "$TARGET_TREE/$parent" ] ||
		[ -L "$TARGET_TREE/$parent" ] || [ -L "$target" ] ||
		{ [ -e "$target" ] && [ ! -d "$target" ]; }; then
		error "Invalid component destination: $target"
		return 1
	fi
	if [ -L "$TARGET_TREE/.wip-backups" ] ||
		[ -L "$TARGET_TREE/.wip-changes" ]; then
		error "Refusing symlinked backup or changes directory"
		return 1
	fi
	reports="$TARGET_TREE/.wip-changes/$COMPONENT_DIR"
	if [ -L "$reports" ] || { [ -e "$reports" ] && [ ! -d "$reports" ]; }; then
		error "Invalid change manifest destination: $reports"
		return 1
	fi
	stage=$(mktemp -d "$TARGET_TREE/$parent/.wip-src.XXXXXXXX") || return 1
	mkdir "$stage/component" "$stage/empty" || return 1
	# Include working copies of tracked paths and deliberate, unignored additions.
	source_root=$(pwd -P) || return 1
	(cd "$COMPONENT_DIR" &&
		git -c safe.directory="$source_root" -c core.fsmonitor=false \
			ls-files -z --cached --others --exclude-standard -- . \
			':(exclude,glob)**/CVS' ':(exclude,glob)**/CVS/**') >"$stage/source-files" || return 1
	[ -s "$stage/source-files" ] || return 1
	(cd "$COMPONENT_DIR" && pax -rw -0 -pe "$stage/component" \
		<"$stage/source-files") || return 1
	find "$stage/component" -type d -exec chmod a+rx,go-w {} + || return 1
	find "$stage/component" -type f -exec chmod a+r,go-w {} + || return 1
	find "$stage/component" -type f -perm -0100 -exec chmod a+x {} + || return 1

	# Carry CVS metadata over even for directories absent from the new source,
	# so files removed from the component can still be registered with CVS.
	if [ -d "$target" ]; then
		(cd "$target" && find . -type d -name CVS -prune) >"$stage/cvs" ||
			return 1
		while IFS= read -r cvs; do
			mkdir -p "$stage/component/${cvs%/*}" || return 1
			cp -Rp "$target/$cvs" "$stage/component/$cvs" || return 1
		done <"$stage/cvs"
		write_change_lists "$target" "$stage/component" "$stage" || return 1
	else
		write_change_lists "$stage/empty" "$stage/component" "$stage" || return 1
	fi
	mkdir -p "$TARGET_TREE/.wip-backups" "$TARGET_TREE/.wip-changes" ||
		return 1
	parent_dev=$(stat -f %d "$TARGET_TREE/$parent") || return 1
	for path in "$TARGET_TREE/.wip-backups" "$TARGET_TREE/.wip-changes" \
		"$target" "$reports"; do
		[ -e "$path" ] || continue
		path_dev=$(stat -f %d "$path") || return 1
		if [ "$parent_dev" != "$path_dev" ]; then
			error "Cannot rename across filesystems: $path"
			return 1
		fi
	done
	backup=$(mktemp -d "$TARGET_TREE/.wip-backups/$COMPONENT_DIR.XXXXXXXX") ||
		return 1
	publishing=1
	if [ -d "$target" ]; then
		mv "$target" "$backup/component" || return 1
	fi
	if [ -d "$reports" ]; then
		previous="$backup/changes"
		mv "$reports" "$previous" || return 1
	fi
	mv "$stage/component" "$target" || return 1
	mv "$stage/changes" "$reports" || return 1
	log "$COMPONENT_DIR copied to $target"
	log "Backup directory: $backup; change lists: $reports"
)

copy_all_directories() {
	typeset directories
	directories=$(list_all_directories) || return 1
	for DIRECTORY in $directories; do
		copy_directory || return 1
	done
}

copy_selected_directories() {
	# Validate the complete selection before changing any destination.
	for DIRECTORY in $SELECTED_DIRECTORIES; do
		validate_selection "$DIRECTORY" || return 1
	done
	for DIRECTORY in $SELECTED_DIRECTORIES; do
		copy_directory || return 1
	done
}

# Resolve a user-selected checkout without CDPATH output or a symlinked root.
canonical_target_tree() {
	typeset path=$1
	while [ "${path%/}" != "$path" ] && [ "$path" != "/" ]; do
		path=${path%/}
	done
	if [ -L "$path" ]; then
		error "Refusing symlinked checkout root: $path"
		return 1
	fi
	(
		unset CDPATH
		cd -- "$path" && pwd -P
	)
}

# Main function
main() {
	TARGET_KIND=""
	case "${1:-}" in
	--list)
		[ "$#" -eq 1 ] || {
			error "Usage: $0 --list"
			return 1
		}
		move_to_wip_openbsd_src
		list_all_directories
		return $?
		;;
	--copy-only)
		[ "$#" -ge 2 ] || {
			error "Usage: $0 --copy-only TREE [component ...]"
			return 1
		}
		[ -d "$2" ] || {
			error "Not a directory: $2"
			return 1
		}
		TARGET_TREE=$(canonical_target_tree "$2") || return 1
		shift 2
		detect_target_tree || return 1
		move_to_wip_openbsd_src
		if [ "$#" -eq 0 ]; then
			copy_all_directories
		else
			for DIRECTORY in "$@"; do
				validate_selection "$DIRECTORY" || return 1
			done
			SELECTED_DIRECTORIES="$*"
			copy_selected_directories
		fi
		return $?
		;;
	--no-provision)
		shift
		[ "$#" -eq 0 ] || {
			error "Usage: $0 [--list | --copy-only TREE [component ...] | --no-provision]"
			return 1
		}
		;;
	"") ;;
	*)
		error "Usage: $0 [--list | --copy-only TREE [component ...] | --no-provision]"
		return 1
		;;
	esac
	check_openbsd
	check_root
	select_repository_to_checkout || return 1
	checkout_selected_repository || return 1
	if [ "$SELECTED_REPO" = www ]; then
		# No local component maps to the website checkout.
		DO_COPY=0
	else
		ask_copy_from_wip
	fi
	if [ "${DO_COPY:-0}" -eq 1 ]; then
		move_to_wip_openbsd_src
		choose_target_tree
		TARGET_TREE=$(canonical_target_tree "$TARGET_TREE") || return 1
		detect_target_tree || return 1
		if [ "${COPY_ALL:-0}" -eq 1 ]; then
			copy_all_directories || return 1
		elif [ "${COPY_LIST:-0}" -eq 1 ]; then
			prompt_selected_directories
			copy_selected_directories || return 1
		else
			list_directories
			copy_directory || return 1
		fi
	else
		log "Skipping copy from wip-openbsd-src."
	fi

}

# Execute the main function
main "$@"
