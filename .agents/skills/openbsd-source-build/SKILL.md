---
name: openbsd-source-build
description: Build OpenBSD components from this repository or from a full src/xenocara checkout. Use when selecting the correct Makefile, running make obj/make/make clean, choosing a component build versus a full release build, diagnosing compiler or linker failures, handling DESTDIR, or separating build artifacts from sources.
---

# OpenBSD source build

## When to use

Use when building `pax`, `fvwm`, a patch-style component in a real checkout,
or when planning a full `make build` / `make release`. Use
`openbsd-pax-development` or `openbsd-fvwm-development` for component-specific
build details.

Do not use for `make install` into a live system without explicit authorization.

## Objective

Build the intended component with the correct BSD make environment and flags,
keeping sources and objects separate, and report honestly what was built.

## Prerequisites

- OpenBSD 7.9 or 8.0-beta with `make(1)` and `/usr/share/mk`.
- A C compiler (`clang` on OpenBSD).
- For `fvwm`: Xenocara/X11 headers and libraries under `/usr/X11R6`.
- For `ext4fs`: a complete matching `src` checkout; it is not built here.
- For `libc`: a full `src` tree and the base build environment.
- For host tests on non-OpenBSD: `bmake`, clang, and X11 headers for the FVWM
  parse test.

## Which Makefile?

| Goal | Directory | Include chain |
|---|---|---|
| Build pax and fvwm | repository root | `bsd.subdir.mk`, `SUBDIR = pax fvwm` |
| Build pax only | `pax/` | `bsd.prog.mk`, `PROG = pax`, `LINKS = tar cpio` |
| Build fvwm only | `fvwm/` | `bsd.subdir.mk` then `bsd.xorg.mk`; subdirs use `bsd.prog.mk`/`bsd.lib.mk` |
| Build one module | `fvwm/modules/<Name>/` | `../Makefile.inc`, `bsd.prog.mk`, `bsd.xorg.mk` |
| Build libc | `libc/lib/libc/` in a full tree | `bsd.lib.mk`, `Makefile.inc` |
| Build mount_ext4fs | `ext4fs/sbin/mount_ext4fs/` in a full tree | `bsd.prog.mk`, sibling `sbin/mount` |
| Host tests | `tests/` or `libc/tests/` | no `bsd.*.mk` |

Top-level `make` builds only `pax` and `fvwm`. It does not build `libc`,
`ext4fs`, or `stdckdint`.

## Procedure

1. **Read the Makefile and `Makefile.inc`.** Confirm `PROG`, `SRCS`, `MAN`,
   `CFLAGS`, `LDADD`, and any `NOOBJ`.
2. **Create an object directory when the tree supports it.**
   ```sh
   make obj
   ```
   `pax` uses `obj`; `fvwm` sets `NOOBJ = Yes` and builds in place.
3. **Build the component.**
   ```sh
   make -C pax
   make -C fvwm
   ```
   Or use the wrapper:
   ```sh
   ./build.ksh pax
   ./build.ksh fvwm
   ./build.ksh all
   ```
4. **Stage installation instead of installing.**
   ```sh
   make DESTDIR=/tmp/stage install-pax
   DESTDIR=/tmp/stage ./build.ksh pax install
   ```
5. **Clean only what you built.**
   ```sh
   make -C pax clean
   make -C fvwm clean
   make cleandir          # also removes obj links/dependencies where supported
   ```
6. **For a full tree build, use release(8), not the component targets.**
   ```sh
   cd /usr/src
   make obj
   make build
   ```
   Kernel and X builds are separate steps; see `release(8)` and FAQ 5.
7. **Record exactly what was run** and whether it succeeded.

## Component versus full build

- A component build proves only that the component compiles with the current
  headers and libraries. It does not validate the OpenBSD release build.
- `make build` rebuilds the userland and kernel toolchain chain; it expects a
  matching source tree and root privileges for installation.
- `make release` additionally creates release sets and expects a `noperm`
  `DESTDIR`.
- Cross-compilation is not a supported general workflow.

## Useful commands

```sh
# Inspect the build graph without building
make -n
make -C pax -n

# Build with verbose commands
make -C pax
make -C fvwm

# Component install, staged
make DESTDIR=/tmp/stage install-pax
make DESTDIR=/tmp/stage install-fvwm

# Lint manual pages through the build system
make -C pax manlint
make -C fvwm manlint
```

## Validation and acceptance

- The correct `Makefile` was used and the target matches the request.
- No source files were modified by the build.
- Object files and binaries are outside tracked paths or are ignored.
- The build log shows no new warnings in changed code.
- `DESTDIR` was used for any install rehearsal.
- If the build was not run, the report says so.

## Common problems and diagnosis

| Symptom | Check |
|---|---|
| `Could not find bsd.xorg.mk` | OpenBSD `/usr/share/mk` missing, or not on OpenBSD |
| GNU make parses the file incorrectly | Use OpenBSD `make` or `bmake`, never GNU make |
| `don't know how to make obj` | `NOOBJ = Yes` in the component; build in place |
| Missing X11 headers | Install `xbase`/X11 development files, check `X11BASE` |
| `undefined reference` to a local function | `SRCS` or `LDADD` missing the object/library |
| `pax` link lacks `tar`/`cpio` | `LINKS` in `pax/Makefile` |
| `fvwm.1` not installed | `MAN` must include both `fvwm.1` and `fvwm_exec.1` |
| Stale objects after header edits | `make clean` or `make cleandir`, then rebuild |

## References

- `release(8)`: <https://man.openbsd.org/release.8>
- `make(1)`: <https://man.openbsd.org/make.1>
- OpenBSD FAQ 5: <https://www.openbsd.org/faq/faq5.html>
- `bsd.README` in `/usr/share/mk`
- Repository `README.md`, `Makefile`, `build.ksh`
