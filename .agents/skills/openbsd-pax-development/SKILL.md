---
name: openbsd-pax-development
description: Maintain pax, tar, and cpio in this repository. Use when changing archive formats, option parsing, path sanitization, symlink/hardlink extraction, metadata restoration, malformed-archive handling, or the pax sandbox; covers POSIX compatibility and the multicall binary.
---

# pax development

## When to use

Use for any change under `pax/`: format writers/readers, option parsing,
extraction, listing, append, copy mode, metadata, or sandbox policy.

Do not invent formats or options. The format table and option strings in
`pax/options.c` are the authority.

## Objective

Change one archive behavior or format path without breaking the other two
personalities, and keep extraction, metadata, and the sandbox consistent with
the documented POSIX/OpenBSD behavior.

## Prerequisites

- `pax/` builds with OpenBSD make or bmake.
- The relevant format definition and option parser have been read.
- A set of test archives exists for each affected personality.
- For sandbox changes, an OpenBSD host is available for `ktrace` review.

## Architecture

| File | Responsibility |
|---|---|
| `pax.c` | `main()`, global flags, `unveil`/`pledge`, temp-file path |
| `ar_io.c` | archive descriptor, volumes, tape ioctls, compressor child |
| `ar_subs.c` | `list()`, `extract()`, `archive()`, `append()`, `copy()` |
| `buf_subs.c` | block buffering, read/write, sparse copy |
| `options.c` | pax/tar/cpio option parsing, format table, `-o` parser |
| `getoldopt.c` | historic tar option parser |
| `tar.c` / `tar.h` | old tar, ustar, pax extended headers, GNU long names |
| `cpio.c` / `cpio.h` | bcpio, odc, sv4cpio, sv4crc |
| `file_subs.c` | node creation, attributes, sparse writes, CRC |
| `ftree.c` | `fts(3)` traversal and file lists |
| `pat_rep.c` | patterns, replacement, rename, path sanitization |
| `sel_subs.c` | selection ranges |
| `tables.c` | hardlink, symlink, rename, directory-time tables |
| `gen_subs.c` | list formatting, pax keyword storage |
| `tty_subs.c` | `/dev/tty`, `paxwarn`, `syswarn`, `safe_print` |

`pax`, `tar`, and `cpio` are one binary; `argv[0]` selects the personality.

## Supported formats

`bcpio`, `cpio` (odc), `sv4cpio`, `sv4crc`, `tar`, `ustar`, `pax`. Defaults:
`pax` writes pax format; `tar` writes pax format unless `-O`/`-o`; `cpio`
writes `sv4crc` unless `-c`/`-6`.

Compression is external: `-z` gzip, `-j` bzip2, `-Z` compress. XZ is detected
on input but there is no `-J` writer.

## Procedure

1. **Read the option parser for the affected personality.** The same letter can
   mean different things (`pax -P` = do not follow symlinks; `tar -P` = do not
   strip leading slashes; `cpio -c` = odc format).
2. **Check the format definitions.** Fixed-width fields, checksums, extended
   header record lengths, and block sizes live in `tar.c`/`cpio.c` and their
   headers.
3. **Handle names before touching the filesystem.** `mod_name()` strips leading
   `/` and leading `..` when `rmleadslash` is set. Do not add a second,
   contradictory sanitizer.
4. **Preserve the symlink deferral.** Absolute/`..` symlink targets become
   `O_EXCL` placeholders and are resolved after extraction with dev/ino
   verification.
5. **Preserve metadata rules.** Clear setuid/setgid when chown fails or `-p`
   was not requested. Directories get temporary owner access and are restored
   with a dev/ino check.
6. **Validate malformed input.** Extended header records, cpio name lengths,
   component lengths, truncated blocks, and oversized numeric fields must be
   rejected before indexing or allocating.
7. **Check the sandbox stage.** `unveil` inputs during option parsing and archive
   paths in `main()`; lock once; then `pledge`. Compressed modes are
   intentionally unpledged. Append + `-u` needs `cpath`.
8. **Test all three personalities.** A change to shared code is not complete
   until `pax`, `tar`, and `cpio` modes are considered.

## Useful commands

```sh
# Build
cd pax && make

# Create, list, extract
./pax -w -f /tmp/a.tar dir
./pax -f /tmp/a.tar
./pax -r -f /tmp/a.tar -C /tmp/out

# tar and cpio personalities
./tar -cf /tmp/a.tar dir
./cpio -o < filelist > /tmp/a.cpio

# Compare a round trip
mkdir /tmp/in /tmp/out
# populate /tmp/in, archive it, extract under /tmp/out, then compare
diff -r /tmp/in /tmp/out

# Malformed input
truncate -s 100 /tmp/a.tar
./pax -f /tmp/a.tar
```

## Validation and acceptance

- Round trip preserves file contents, modes, times, owners/groups when
  permitted, hardlinks, symlinks, directories, and device nodes.
- Listing output matches the archive contents.
- A truncated or malformed archive produces a diagnostic and a nonzero exit
  status, not a crash.
- Absolute paths and `..` components are handled as documented; `-P` behavior
  is unchanged.
- `-p` and non-`-p` behavior match the man page and the POSIX rules.
- The man pages (`pax.1`, `tar.1`, `cpio.1`) match the implementation.
- No new format is claimed without code and tests.

## Common problems and diagnosis

| Symptom | Check |
|---|---|
| Extraction creates files outside `-C` | `mod_name`, `rmleadslash`, symlink/hardlink handling |
| Setuid bit appears unexpectedly | `pmode`/`SETBITS` and chown success |
| `pax -a -w -u` fails with `ENOSYS`/`EPERM` | `cpath` missing from the append promise set |
| Compressed mode aborts after pledge | Compressed modes must stay unpledged |
| Extended header rejected | Record length/`=` bounds and `invalid=` policy |
| `tar -P` behaves like `pax -P` | The two options have different meanings |
| cpio name overflow | `rd_nm()`/`rd_ln_nm()` bounds |

## References

- `pax(1)`: <https://man.openbsd.org/pax.1>
- `tar(1)`: <https://man.openbsd.org/tar.1>
- `cpio(1)`: <https://man.openbsd.org/cpio.1>
- POSIX.1-2024 pax specification
- `openbsd-source-security`, `openbsd-source-regress`
