# Copying the local components into OpenBSD checkouts

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
`libc`/`include`/`libpthread`/`sys` files, the six new headers, their libc
sources, manual pages and regress tests at source-relative paths (see
`libc/README.md` and `libc/REPORT.md`).  The copy helper does not handle
it either.

The `ext4fs/` component is patch-style as well.  Its new files live at
OpenBSD-relative paths (`sys/ufs/ext4fs/`, `sbin/mount_ext4fs/`), while
`ext4fs/sys-patches.diff` contains the hunks for existing tree files such
as `sys/conf/files`, `sys/kern/vfs_init.c` and `sys/sys/mount.h`.  Copy
the new directories into the checkout and apply the patch from the `src`
root; the copy helper does not handle this component.

Use an existing CVS or Git checkout without host provisioning:

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
nested CVS metadata is retained. Before the swap, the files and symlinks that
are about to be removed and added are recorded in
TREE/.wip-changes/NAME.created.txt and TREE/.wip-changes/NAME.deleted.txt
(one path per line, relative to the component root, CVS metadata excluded).
New directories are recorded separately in NAME.created-dirs.txt so that
`cvs add` can register a directory before its files; `cvs remove` and a later
`cvs diff` can then be driven from the lists. Sources are made readable to
build users.

The repository's Makefiles remain the standalone fork's Makefiles. Copying
files is not validation of a full OpenBSD src/xenocara release build, and
installation prefixes/build integration still need native review.

Without options, the existing interactive bootstrap workflow clears the
selected tree, checks out a fresh one, and configures the host. It is
destructive: it removes the contents of `/usr/src` or `/usr/xenocara`, writes
`~/.profile`, and creates the `user` account and a `doas` rule. Do not use
that mode merely to copy sources into an existing checkout. `--no-provision`
keeps the wipe/checkout workflow but skips creating the `user` account and
configuring doas:

```sh
doas ./fetch-src.ksh --no-provision
```

Review the selected tree and take a backup before running either destructive
mode. `--list` is read-only and prints the component mapping; `--copy-only`
replaces only the named component directory and keeps a recoverable backup.
