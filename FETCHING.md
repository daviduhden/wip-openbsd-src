# Fetching OpenBSD trees and copying local components

The interactive checkout menu supports `src`, `xenocara` and `www`, using
CVS HEAD and the destinations `/usr/src`, `/usr/xenocara` and `/usr/www`.
`www` contains the OpenBSD website sources and has no local replacement
component; selecting it skips the local-copy question. It is not a
`--copy-only` target and does not add an entry to `--list`.

Run `doas ./fetch-src.ksh` and select the tree. Existing destinations
require confirmation. The script checks out into a sibling directory,
checks for CVS metadata, and then replaces the destination. The previous
tree remains at the backup path printed by the script. Symlinked or
mounted trees are rejected. `CVSROOT` can override the default server
without any profile edits. `--no-provision` remains accepted as an alias.
Manage accounts and doas rules separately with OpenBSD tools.

To update an existing checkout while retaining local edits, use CVS:

```sh
cd /usr/www && cvs -q up -Pd -A
```

The standalone build layout is unchanged. The copy helper maps:

| Local source | Destination |
|---|---|
| pax/ | src checkout: bin/pax/ |
| fvwm/ | xenocara checkout: app/fvwm/ |

The `stdckdint/` component is a patch-style contribution rather than a
directory replacement: it adds `include/stdckdint.h` and a regress test and
hooks both into the existing `include/Makefile` and `regress/lib/libc/Makefile`.
The copy helper does not handle it; apply `stdckdint/stdckdint.patch` from the
`src` root instead (see `stdckdint/README.md`).

The `libc/` component is also patch-style: it keeps the imported OpenBSD
`lib/libc`, `include`, `lib/libpthread` manuals, `lib/librthread` pthread
provider and `sys` files, plus the six new headers, their library sources,
manual pages and regress tests at source-relative paths (see
`libc/README.md` and `libc/REPORT.md`).  The copy helper does not handle
it either.

The `ext4fs/` component is patch-style as well.  Its new files live at
OpenBSD-relative paths (`sys/ufs/ext4fs/`, `sbin/mount_ext4fs/`), while
`ext4fs/sys-patches.diff` contains the hunks for existing tree files such
as `sys/conf/files`, `sys/kern/vfs_init.c` and `sys/sys/mount.h`.  Copy
the new directories into the checkout and apply the patch from the `src`
root; the copy helper does not handle this component.

Copy from the local Git working tree into an existing CVS or Git checkout. Component copies require Git and base pax:

```sh
./fetch-src.ksh --list
doas ./fetch-src.ksh --copy-only /usr/src src/bin/pax
doas ./fetch-src.ksh --copy-only /usr/xenocara xenocara/app/fvwm
```

The short names pax and fvwm are accepted too. With no component arguments,
only components belonging to the detected target tree are copied.
The helper rejects the wrong tree, unknown components and symlinked roots.
It does not replace parent directories or neighboring utilities. Old component
directories are recoverable under TREE/.wip-backups/NAME.XXXXXXXX/component;
nested CVS metadata is retained. Sources come from the Git working tree,
including tracked files and unignored additions. Stage intended removals
in Git before copying, so missing tracked paths do not make the copy fail.

The latest manifest set lives in `TREE/.wip-changes/NAME/` as
`created.txt`, `deleted.txt`, and `created-dirs.txt`. Each list contains
one relative path per line and excludes CVS metadata. Update consumers
of the former flat `NAME.*.txt` manifests to use this directory. The previous manifest set,
when present, is saved under `changes/` beside the component backup.
The lock serializes operations by checkout. If an interruption leaves a
lock or backup behind, inspect the target, backup and manifests before
removing the lock or restoring files.

The repository's Makefiles remain the standalone fork's Makefiles. Copying
files is not validation of a full OpenBSD src/xenocara release build, and
installation prefixes/build integration still need native review.

`--list` is read-only and prints the component mapping. For the interactive
checkout, review the selected destination and confirm only if the old tree
can be moved to the sibling backup.
