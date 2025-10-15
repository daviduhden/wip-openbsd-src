#!/bin/ksh

# Copyright (c) 2024-2025 David Uhden Collado <david@uhden.dev>
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

# Verify that the script is run as root
check_root() {
    if [ "$(id -u)" -ne 0 ]; then
        print "This script must be run as root." >&2
        exit 1
    fi
}

# Function to remove the src directory
remove_src_directory() {
    rm -rf /usr/src
}

# Function to remove the xenocara directory
remove_xenocara_directory() {
    rm -rf /usr/xenocara
}

# Function to permanently set the CVSROOT environment variable if not already set
set_cvsroot() {
    if ! grep -q "export CVSROOT=anoncvs@anoncvs.eu.openbsd.org:/cvs" ~/.profile; then
        print "export CVSROOT=anoncvs@anoncvs.eu.openbsd.org:/cvs" >> ~/.profile
        print "CVSROOT variable added to ~/.profile"
    else
        print "CVSROOT variable already exists in ~/.profile"
    fi
    export CVSROOT="anoncvs@anoncvs.eu.openbsd.org:/cvs"
}

# Function to select which repository to checkout (src or xenocara)
select_repository_to_checkout() {
    print "Select the repository to clone (checkout):"
    select SELECTED_REPO in src xenocara; do
        if [ -n "$SELECTED_REPO" ]; then
            print "You selected $SELECTED_REPO"
            break
        else
            print "Invalid selection. Please try again."
        fi
    done
}

# Function to checkout the selected tree using CVS
checkout_selected_repository() {
    cd /usr || exit 1
    case "$SELECTED_REPO" in
        src)
            remove_src_directory
            cvs -qd anoncvs@anoncvs.eu.openbsd.org:/cvs checkout -P src
            ;;
        xenocara)
            remove_xenocara_directory
            cvs -qd anoncvs@anoncvs.eu.openbsd.org:/cvs checkout -P xenocara
            ;;
        *)
            print "Unknown repository: $SELECTED_REPO" >&2
            exit 1
            ;;
    esac
}

# Function to ask if user wants to copy from wip-openbsd-src (optional)
ask_copy_from_wip() {
    print "Do you want to copy a directory from 'wip-openbsd-src' into the checked-out tree?"
    select ANSWER in "Yes" "No"; do
        case "$ANSWER" in
            Yes) DO_COPY=1; break ;;
            No)  DO_COPY=0; break ;;
            *)   print "Invalid selection. Please try again." ;;
        esac
    done
}

# Function to change directory to the wip-openbsd-src directory
move_to_wip_openbsd_src() {
    wip_openbsd_src_dir=$(find / -type d -name "wip-openbsd-src" 2>/dev/null | head -n 1)
    if [ -z "$wip_openbsd_src_dir" ]; then
        print "wip-openbsd-src directory not found."
        exit 1
    fi
    cd "$wip_openbsd_src_dir" || exit 1
}

# Function to list directories in the current directory and select one
list_directories() {
    print "Select a directory to copy from wip-openbsd-src:"
    select DIRECTORY in */; do
        if [ -n "$DIRECTORY" ]; then
            print "You selected $DIRECTORY"
            DIRECTORY=${DIRECTORY%/}  # Remove the trailing slash
            break
        else
            print "Invalid selection. Please try again."
        fi
    done
}

# Function to choose between /usr/src or /usr/xenocara as target tree
# Only offers trees that exist (i.e., the one you cloned, or both if present)
choose_target_tree() {
    options=""
    [ -d /usr/src ] && options="$options /usr/src"
    [ -d /usr/xenocara ] && options="$options /usr/xenocara"
    if [ -z "$options" ]; then
        print "No destination trees available under /usr."
        exit 1
    fi
    print "Select the target tree for the copy:"
    select TARGET_TREE in $options; do
        if [ -n "$TARGET_TREE" ]; then
            print "You selected $TARGET_TREE"
            break
        else
            print "Invalid selection. Please try again."
        fi
    done
}

# Function to list subdirectories in the chosen tree and select one
list_tree_subdirectories() {
    print "Select a subdirectory in $TARGET_TREE where the directory will be copied:"
    select SUBDIRECTORY in "$TARGET_TREE"/*/; do
        if [ -n "$SUBDIRECTORY" ]; then
            print "You selected $SUBDIRECTORY"
            SUBDIRECTORY=${SUBDIRECTORY%/}  # Remove the trailing slash
            break
        else
            print "Invalid selection. Please try again."
        fi
    done
}

# Function to copy the selected directory to the chosen subdirectory in target tree
copy_directory() {
    TARGET_DIR="$SUBDIRECTORY/$DIRECTORY"
    if [ -d "$TARGET_DIR" ]; then
        print "Directory $TARGET_DIR already exists. Removing files except 'CVS' directories."
        find "$TARGET_DIR" -mindepth 1 ! -name "CVS" -exec rm -rf {} +
    fi
    cp -R "$DIRECTORY" "$SUBDIRECTORY/"
    print "Directory $DIRECTORY copied to $SUBDIRECTORY/"
}

# Function to create the user 'user' with a random password
create_user_with_random_password() {
    USER_TO_CREATE="user"

    # Generate a random password
    PASSWORD=$(openssl rand -base64 12)

    # Create the user with a home directory and set the shell to /bin/ksh
    useradd -m -s /bin/ksh "$USER_TO_CREATE"

    # Encrypt the password and set it using usermod
    ENCRYPTED_PASSWORD=$(openssl passwd -1 "$PASSWORD")
    usermod -p "$ENCRYPTED_PASSWORD" "$USER_TO_CREATE"

    print "User 'user' created with password: $PASSWORD"
}

# Function to configure doas
configure_doas() {
    cp /etc/examples/doas.conf /etc/doas.conf
    print "permit keepenv persist user" >> /etc/doas.conf
    print "doas configured successfully. /etc/doas.conf updated."
}

# Main function
main() {
    check_root
    set_cvsroot
    select_repository_to_checkout
    checkout_selected_repository
    ask_copy_from_wip
    if [ "${DO_COPY:-0}" -eq 1 ]; then
        move_to_wip_openbsd_src
        list_directories
        choose_target_tree
        list_tree_subdirectories
        copy_directory
    else
        print "Skipping copy from wip-openbsd-src."
    fi
    create_user_with_random_password
    configure_doas
}

# Execute the main function
main