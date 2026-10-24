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
PATH=/usr/bin:/bin:/usr/sbin:/sbin
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

# Function to remove the src content
remove_src_content() {
	if [ -L /usr/src ]; then
		error "Refusing to clear symlinked /usr/src"
		return 1
	fi
	rm -rf /usr/src/*
}

# Function to remove the xenocara content
remove_xenocara_content() {
	if [ -L /usr/xenocara ]; then
		error "Refusing to clear symlinked /usr/xenocara"
		return 1
	fi
	rm -rf /usr/xenocara/*
}

check_openbsd() {
	if [ "$(uname -s)" != "OpenBSD" ]; then
		error "This mode is supported only on OpenBSD."
		exit 1
	fi
}

# Set CVSROOT in .profile if not already configured
set_cvsroot() {
	if [ -z "${HOME:-}" ]; then
		warn "HOME is unset; not updating .profile"
		return 1
	fi
	if [ ! -f "$HOME/.profile" ] ||
		! grep -Fq "export CVSROOT=anoncvs@anoncvs.eu.openbsd.org:/cvs" \
			"$HOME/.profile"; then
		print "export CVSROOT=anoncvs@anoncvs.eu.openbsd.org:/cvs" \
			>>"$HOME/.profile"
		log "CVSROOT variable added to ~/.profile"
	else
		log "CVSROOT variable already exists in ~/.profile"
	fi
	export CVSROOT="anoncvs@anoncvs.eu.openbsd.org:/cvs"
}

# Function to select which repository to checkout (src or xenocara)
select_repository_to_checkout() {
	log "Select the repository to clone (checkout):"
	select SELECTED_REPO in src xenocara; do
		if [ -n "$SELECTED_REPO" ]; then
			log "You selected $SELECTED_REPO"
			break
		else
			warn "Invalid selection. Please try again."
		fi
	done
}

# Function to checkout the selected tree using CVS
checkout_selected_repository() {
	cd /usr || exit 1
	case "$SELECTED_REPO" in
	src)
		remove_src_content || return 1
		log "Checking out src from anoncvs..."
		cvs -qd anoncvs@anoncvs.eu.openbsd.org:/cvs checkout -P src ||
			{
				error "cvs checkout of src failed;" \
					"/usr/src is incomplete"
				return 1
			}
		;;
	xenocara)
		remove_xenocara_content || return 1
		log "Checking out xenocara from anoncvs..."
		cvs -qd anoncvs@anoncvs.eu.openbsd.org:/cvs \
			checkout -P xenocara ||
			{
				error "cvs checkout of xenocara failed;" \
					"/usr/xenocara is incomplete"
				return 1
			}
		;;
	*)
		error "Unknown repository: $SELECTED_REPO"
		exit 1
		;;
	esac
}

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

# Flatten a path into a single filename component.  Only pdksh constructs
# are used: ${var//pat/rep} is ksh93-only and the ksh shipped with the
# OpenBSD base system (pdksh) rejects it with "bad substitution".
flatten_path() {
	print -r -- "$1" | tr '/' '_'
}

# Emit the tracked files under a component, relative to its root and sorted,
# excluding the CVS metadata directories.  Used to build the created/deleted
# lists that drive 'cvs add' and 'cvs remove'.
list_component_files() {
	(
		cd "$1" || exit 1
		find . -type d -name CVS -prune -o \
			\( -type f -o -type l \) -print
	) | sed -e 's|^\./||' -e '/^$/d' | LC_ALL=C sort
}

# Like list_component_files(), but directories only.  CVS has to register a
# new directory before the files it contains, so the created-directory list is
# kept apart from the created-file list.
list_component_dirs() {
	(
		cd "$1" || exit 1
		find . -type d -name CVS -prune -o -type d -print
	) | sed -e 's|^\./||' -e '/^$/d' | LC_ALL=C sort
}

# Record the files that disappeared from and appeared in a replaced
# component.  The two plain-text lists live in $TARGET_TREE/.wip-changes/ and
# contain one path per line, relative to the component root: the created list
# feeds 'cvs add', the deleted list feeds 'cvs remove'.  This is what makes it
# possible to build a correct 'cvs diff' after a wholesale directory
# replacement, since the new files come from a Git checkout and have no CVS
# metadata of their own.
write_change_lists() {
	typeset old=$1 new=$2 label=$3
	typeset dir="$TARGET_TREE/.wip-changes"
	typeset tmp

	if [ -L "$dir" ]; then
		warn "Refusing symlinked changes directory: $dir"
		return 1
	fi
	mkdir -p "$dir" || return 1
	tmp=$(mktemp -d "$dir/.$label.XXXXXXXX") || return 1
	if ! list_component_files "$old" >"$tmp/oldfiles" ||
		! list_component_files "$new" >"$tmp/newfiles" ||
		! list_component_dirs "$old" >"$tmp/olddirs" ||
		! list_component_dirs "$new" >"$tmp/newdirs" ||
		! LC_ALL=C comm -13 "$tmp/oldfiles" "$tmp/newfiles" \
			>"$dir/$label.created.txt" ||
		! LC_ALL=C comm -23 "$tmp/oldfiles" "$tmp/newfiles" \
			>"$dir/$label.deleted.txt" ||
		! LC_ALL=C comm -13 "$tmp/olddirs" "$tmp/newdirs" \
			>"$dir/$label.created-dirs.txt"; then
		rm -rf "$tmp"
		return 1
	fi
	rm -rf "$tmp"
	log "Change lists: $dir/$label.created.txt" \
		"$dir/$label.deleted.txt" \
		"$dir/$label.created-dirs.txt"
}

# Replace a single component, keeping a recoverable backup and all nested
# CVS metadata. Sibling components and parent Makefiles are never removed.
copy_directory() {
	typeset target parent stage backup="" cvs
	validate_selection "$DIRECTORY" || return 1
	parent=${COMPONENT_PATH%/*}
	target="$TARGET_TREE/$COMPONENT_PATH"
	if [ ! -d "$TARGET_TREE/$parent" ] ||
		[ -L "$TARGET_TREE/$parent" ] || [ -L "$target" ] ||
		{ [ -e "$target" ] && [ ! -d "$target" ]; }; then
		error "Invalid component destination: $target"
		return 1
	fi
	stage=$(mktemp -d "$TARGET_TREE/.wip-src.XXXXXXXX") || {
		error "Cannot create staging directory under $TARGET_TREE"
		return 1
	}
	cp -Rp "$COMPONENT_DIR" "$stage/component" || return 1
	find "$stage/component" -type d -exec chmod a+rx,go-w {} + || return 1
	find "$stage/component" -type f -exec chmod a+r,go-w {} + || return 1
	find "$stage/component" -type f -perm -0100 -exec chmod a+x {} + || return 1
	if [ -d "$target" ]; then
		(cd "$target" && find . -type d -name CVS -prune) |
			while IFS= read -r cvs; do
				mkdir -p "$stage/component/${cvs%/*}" || exit 1
				cp -Rp "$target/$cvs" "$stage/component/$cvs" || exit 1
			done || return 1
		write_change_lists "$target" "$stage/component" \
			"$COMPONENT_DIR" || return 1
		if [ -L "$TARGET_TREE/.wip-backups" ]; then
			error "Refusing symlinked backup directory"
			return 1
		fi
		mkdir -p "$TARGET_TREE/.wip-backups" || return 1
		backup=$(mktemp -d \
			"$TARGET_TREE/.wip-backups/$COMPONENT_DIR.XXXXXXXX") || {
			error "Cannot create backup directory" \
				"under $TARGET_TREE/.wip-backups"
			return 1
		}
		mv "$target" "$backup/component" || return 1
		log "Previous $COMPONENT_PATH saved in $backup/component"
	fi
	if ! mv "$stage/component" "$target"; then
		if [ -n "$backup" ]; then
			if mv "$backup/component" "$target"; then
				warn "Restored previous $COMPONENT_PATH" \
					"from $backup/component"
			else
				error "Rollback failed; previous" \
					"$COMPONENT_PATH is in" \
					"$backup/component"
			fi
		fi
		error "Copy failed; staging directory: $stage"
		return 1
	fi
	rmdir "$stage" ||
		warn "Could not remove staging directory: $stage"
	log "$COMPONENT_DIR copied to $target"
}

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

# Function to create the user 'user' with a random password
create_user_with_random_password() {
	USER_TO_CREATE="user"

	# Generate a random password
	PASSWORD=$(openssl rand -base64 12) || {
		error "Could not generate a password"
		return 1
	}
	[ -n "$PASSWORD" ] || {
		error "Generated password is empty"
		return 1
	}

	# Create the user with a home directory and set the shell to /bin/ksh
	useradd -m -s /bin/ksh "$USER_TO_CREATE" || {
		error "Could not create user '$USER_TO_CREATE'"
		return 1
	}

	# Encrypt the password and set it using usermod
	ENCRYPTED_PASSWORD=$(openssl passwd -6 "$PASSWORD") || {
		error "Could not encrypt the password"
		return 1
	}
	usermod -p "$ENCRYPTED_PASSWORD" "$USER_TO_CREATE" || {
		error "Could not set the password for '$USER_TO_CREATE'"
		return 1
	}

	# Do not print the password to the terminal or a log.  Store it
	# root-readable in the account's home directory instead.
	if [ -z "${HOME:-}" ]; then
		error "HOME is unset; cannot store the generated password"
		return 1
	fi
	PASSWORD_FILE="$HOME/.wip-openbsd-src-user-password"
	(umask 077 && print -r -- "$PASSWORD" >"$PASSWORD_FILE") || {
		error "Could not write $PASSWORD_FILE"
		return 1
	}
	chmod 600 "$PASSWORD_FILE" || {
		error "Could not restrict permissions on $PASSWORD_FILE"
		return 1
	}
	log "User '$USER_TO_CREATE' created; password in $PASSWORD_FILE"
}

# Function to configure doas
configure_doas() {
	if [ -f /etc/doas.conf ]; then
		if grep -Fq "permit keepenv persist user" /etc/doas.conf; then
			log "doas is already configured for user."
			return 0
		fi
		cp -p /etc/doas.conf \
			"/etc/doas.conf.wip.$(date '+%Y%m%d%H%M%S')" || {
			error "Could not back up /etc/doas.conf"
			return 1
		}
	else
		cp /etc/examples/doas.conf /etc/doas.conf || {
			error "Could not install /etc/doas.conf"
			return 1
		}
	fi
	print "permit keepenv persist user" >>/etc/doas.conf || {
		error "Could not update /etc/doas.conf"
		return 1
	}
	log "doas configured successfully. /etc/doas.conf updated."
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
	NO_PROVISION=0
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
		NO_PROVISION=1
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
	set_cvsroot || warn "Continuing without updating ~/.profile"
	select_repository_to_checkout
	checkout_selected_repository || return 1
	ask_copy_from_wip
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
	if [ "$NO_PROVISION" -eq 1 ]; then
		log "Skipping user creation and doas configuration."
	else
		create_user_with_random_password || return 1
		configure_doas || return 1
	fi
}

# Execute the main function
main "$@"
