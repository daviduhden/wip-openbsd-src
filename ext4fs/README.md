# ext4fs

Fourth Extended Filesystem driver for the OpenBSD kernel, plus
`mount_ext4fs(8)`.  This component is not a standalone build: it is
stored at OpenBSD source-tree-relative paths and is copied into a full
`src` checkout.

## Layout

| Local path | Destination in `src` |
|---|---|
| `sys/ufs/ext4fs/*` | `sys/ufs/ext4fs/` |
| `sbin/mount_ext4fs/*` | `sbin/mount_ext4fs/` |
| `sys-patches.diff` | hunks for existing tree files (apply from the `src` root) |

`sys-patches.diff` registers the driver in `sys/conf/files`,
`sys/conf/GENERIC`, `sys/kern/vfs_init.c`, `sys/sys/{disklabel,mount,vnode}.h`
and the shared UFS code under `sys/ufs/ufs/`.

The patch is based on OpenBSD `src` revision
`484ecc5061dce11274ca0829ca8933cd67f5642e` (2026-10-31). Apply it from the
root of that source revision; regenerate it after changing the target
revision or any shared UFS integration.

## Integration

```sh
cp -Rp ext4fs/sys/ufs/ext4fs /usr/src/sys/ufs/
cp -Rp ext4fs/sbin/mount_ext4fs /usr/src/sbin/
cd /usr/src && patch -p1 < /path/to/ext4fs/sys-patches.diff
```

Review the patch before applying it: it edits shared kernel headers and
filesystem dispatch tables.  Keep the source checkout at the revision the
patch was written against, and do not apply it twice.

## Testing

The host-side tests in `tests/ext4fs-*.c` exercise journal descriptor
parsing, checksum verification and inode checksums against the
production sources.  They are not part of the OpenBSD build graph.  A
real kernel build and mount test requires an OpenBSD source tree and a
test image; nothing in this repository performs that.

## C language compatibility

The driver sources use C99 syntax. Fixed on-disk layout checks use named
array typedefs with a negative bound on failure, so they remain compile-time
checks without `_Static_assert`. Inode block and extent overlays use named
C99 unions and structures; member aliases retain existing consumer spelling.
The packed structure attribute remains a required OpenBSD compiler extension
for the filesystem ABI. Do not remove it to satisfy an ISO-only checker.
The source-tree build owns compiler flags; the driver does not set `-std=`.
