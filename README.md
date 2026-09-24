# wip-openbsd-src

Standalone fork of selected OpenBSD userland utilities, compiled
independently of the CVS source tree.  Targets OpenBSD 7.9 and
OpenBSD 8.0-beta.  Produces binaries intended to replace the
system-provided versions.

## Components

### pax (tar + cpio)

Archive tool with pledge(2) and unveil(2) sandboxing, following
the POSIX.1-2024 pax/tar/cpio semantics: the default extraction
removes leading '/' from pathnames (POSIX C-1) and the tar
default block size is 10240 bytes (POSIX M-2).  Includes tar(1),
cpio(1), and pax(1) in a single multicall binary, compiled as
ISO C23.

### fvwm

FVWM 2.2.5 window manager with privilege separation.  Both Exec
and PipeRead commands run in the fvwm_exec helper (imsg(3)
protocol over a socketpair), which is started before the main
process locks its filesystem view and deliberately applies no
pledge(2)/unveil(2) -- both are inherited across execve(2) and
would cripple the arbitrary programs fvwm launches.  PipeRead
output is streamed back over IMSG_PIPEREAD_DATA messages and
parsed with the historical line semantics (bounded line lengths,
backslash-newline continuation, NUL truncation, nesting to
MAX_NESTING_DEPTH with per-frame bounded buffering), so PipeRead
commands are no longer restricted by the main process's unveil
sandbox.  The main process, all 17 modules, and xpmroot apply
per-process pledge(2) policies; the main process additionally
restricts its filesystem view with unveil(2) (the modules are
launched by the main process and inherit that view).  Compiled as
ISO C23 with complete function prototypes; numeric configuration
parsing goes through FvwmParseInteger() (historical atoi(3)
prefix semantics with defined overflow behaviour) or strtonum(3)
for strict values.

### ext4fs

Fourth Extended Filesystem kernel driver (read/write, journal
recovery, extents, metadata checksums) and mount_ext4fs(8).
Not built here; the component stores the new files at
OpenBSD-relative paths plus the original patch and the hunks
for modified tree files, for copying into an OpenBSD checkout
(see FETCHING.md).

Journal recovery verifies every descriptor tag before replay:
per-tag checksums are supported for jbd2 checksum v2 (16-bit,
truncated crc32c) and checksum v3 (full 32-bit crc32c), plus the
4-byte descriptor/revoke block tail checksums; plain jbd2 journals
without per-tag checksums are handled without verification.
Journals using async commits or fast commits are rejected at mount
time, as are unknown incompat feature bits.  The checksum input is
crc32c(seed = crc32c(~0, journal UUID), be32 sequence, journaled
block bytes), matching Linux jbd2.

Inode sizes are supported across the full ext4 range: any power of
two from 128 bytes up to the filesystem block size, with the
superblock extra-isize window validated (4-byte aligned, within the
inode) and per-inode i_extra_isize validated before use.  Extended
inode fields (nanosecond timestamps, checksum_hi) are only touched
when the serialized inode actually contains them, and the metadata
checksum covers exactly the bytes the ext4 definition requires, so
it is correct for 128-, 256-, 512-, 1024-, 2048-, and 4096-byte
inodes alike.

## Build

### Requirements

pax:

* OpenBSD 7.9 or 8.0-beta base system with the compiler set:
  make(1), /usr/share/mk, and a C compiler.

fvwm:

* everything required for pax, plus Xenocara/X11 development
  files (xbase set): X11 headers and libraries under /usr/X11R6.

ext4fs:

* a complete matching OpenBSD source checkout and a source build
  environment.  It is not built from this repository.

Compatibility with OpenBSD 7.9 and 8.0-beta is validated
statically against the OpenBSD make(1) source and the installed
7.9 /usr/share/mk; an actual build on OpenBSD has not been
performed from this tree.

### Building

The top-level Makefile builds the standalone components:

```
$ make            # build pax and fvwm
$ make pax        # build only pax
$ make fvwm       # build only fvwm
$ make clean      # remove build artifacts
```

build.ksh is a thin wrapper around make(1) with environment
checks; the Makefiles are the single definition of the build
graph:

```
$ ./build.ksh all            # build everything (default)
$ ./build.ksh pax            # build only pax
$ ./build.ksh fvwm           # build only fvwm
$ ./build.ksh clean          # remove build artifacts
```

### Installing

Installation replaces the system binaries; run it as root or via
doas (never from inside the Makefiles):

```
$ doas make install
$ doas ./build.ksh all install
```

DESTDIR staging is supported everywhere:

```
$ make DESTDIR=/tmp/stage install
$ DESTDIR=/tmp/stage ./build.ksh all install
```

Install destinations:

* pax: /bin/pax with hard links /bin/tar and /bin/cpio;
  man pages in /usr/share/man/man1.
* fvwm and xpmroot: /usr/X11R6/bin; modules, fvwm_exec helper,
  and default configuration in /usr/X11R6/lib/X11/fvwm; icons in
  /usr/X11R6/include/X11/{bitmaps,pixmaps}; man pages in
  /usr/X11R6/man/man.

Configuration examples are installed as system defaults and never
overwrite user files.

## Validation

`validate-make.sh` performs static validation of the build system:

```
$ ./validate-make.sh check
```

It parses every Makefile with BSD make syntax (using bmake with
stubbed /usr/share/mk includes, labelled syntax-only), scans for
GNU make constructs, enforces the C23 policy, and runs checkmake
and mbake in advisory roles with per-finding classification.
OpenBSD make(1) and /usr/share/mk remain authoritative; the script
reports failures only for findings that are genuinely actionable.

The remaining checkmake advisories are classified, not hidden:
`phonydeclared` is a false positive (the .PHONY declarations come
from the OpenBSD bsd.*.mk include chain, or the flagged targets are
real files), and `maxbodylength` is a generic GNU-make body-length
threshold that would only be satisfied by splitting OpenBSD recipes
into artificial helper targets.  These are build-validation
advisories, not source-code defects.

With a copy of the installed OpenBSD 7.9 /usr/share/mk available,
the script additionally expands the whole build graph against the
authoritative mk files:

```
$ OBSD_MK=/path/to/openbsd-7.9/usr/share/mk ./validate-make.sh check
```

## Tests

Host-side developer tests cover the pure parsing and decoding
logic: the FVWM tokenizer, the integer-argument parser
(FvwmParseInteger), the fvwm_exec imsg payload decoder, the
PipeRead line-assembly buffer, the jbd2 descriptor-tag parser and
checksum verification (csum v2/v3, truncation, corruption), and
the ext4 inode checksum across inode sizes (128-4096 bytes),
including superblock inode-size validation.  They run under
ASan/UBSan.  They need clang and X11 headers on the host and are
not part of the OpenBSD build:

```
$ make -C tests CC=clang run
```

Set `X11INCS` to the directory holding the X11 headers when they
are not in the default locations (e.g. `X11INCS=-I/usr/local/include`).

## Security

Every process uses staged pledge(2) and unveil(2) to restrict
system calls and filesystem access to the minimum required.
See source code for per-process policy details.

## License

Components retain their original licenses (BSD, ISC).  New code
is ISC-licensed.  See individual source files for details.
