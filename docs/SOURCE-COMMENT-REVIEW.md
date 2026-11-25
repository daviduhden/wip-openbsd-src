# Source Comment Review

This log tracks the source-comment review requested for this repository. A
file is marked reviewed only after reading the complete file; a function-level
inspection is recorded as partial. Syntax checking alone does not count as a
source review.

## Earlier review inventory

The inventory uses regular files below the worktree and does not follow
symlinks. It excludes `.git`, `CVS`, and `obj` directories.

| Component | C files | Headers | `.ksh` | `.sh` |
| --- | ---: | ---: | ---: | ---: |
| `pax/` | 15 | 4 | 0 | 0 |
| `fvwm/` | 81 | 43 | 0 | 2 |
| `libc/` | 1,196 | 951 | 0 | 7 |
| `ext4fs/` | 5 | 5 | 0 | 0 |
| `stdckdint/` | 1 | 1 | 0 | 0 |
| `tests/` | 7 | 14 | 0 | 1 |
| Repository root | 0 | 0 | 2 | 1 |
| **Total** | **1,305** | **1,018** | **2** | **11** |

Three additional executable Shell scripts have no extension:
`fvwm/modules/FvwmSaveDesk/example/xinitrc`, `fvwm/utils/fvwmrc_convert`, and
`fvwm/utils/quantize_pixmaps`. This gives 16 in-scope Shell files. The imported
`libc/` tree is part of the review scope; its OpenBSD provenance is not a
reason to exclude it.

## Files reviewed in the earlier pass

| Path | Language / module and responsibility | Complexity | Status and comments | Validation | Issues or remaining work |
| --- | --- | --- | --- | --- | --- |
| `build.ksh` | OpenBSD Ksh; top-level component build and install wrapper | Medium | Documented; argument order, install target accumulation, and default install selection clarified | `ksh -n` with host AT&T ksh93u+m; diff review | Not checked with OpenBSD pdksh; script not executed |
| `fetch-src.ksh` | OpenBSD Ksh; checkout, staged component replacement, CVS metadata preservation, optional provisioning | High | Documented; staging and CVS-copy boundaries clarified | `ksh -n` with host AT&T ksh93u+m; static review | Not checked with OpenBSD pdksh; script not executed; early failures can leave temporary staging paths |
| `pax/gen_subs.c` | C; listing output, custom `listopt` conversions, PAX keyword storage, fixed-width numeric conversion | High | Documented; clarified conversion grammar and scratch-string lifetime | Full-file review; `git diff --check` | No build performed |
| `pax/ftree.c` | C; FTS traversal, file-argument sequencing, selection flags, access-time restoration | High | Already documented; existing comments explain traversal and state transitions | Full-file review | No build performed |
| `pax/buf_subs.c` | C; archive buffering, volume changes, append positioning, sparse file transfer | High | Already documented; existing comments describe buffer invariants and error paths | Full-file review | No build performed |
| `pax/sel_subs.c` | C; uid/gid and time-range member selection | Medium | Documented; corrected the `mktime(3)` timezone description | Full-file review; `git diff --check` | Calendar-field bounds are followed by `mktime(3)` normalization; no behavior change made |
| `pax/pax.c` | C; process entry point, option dispatch, sandbox setup, signal cleanup | High | Already documented; existing comments explain operation setup and OpenBSD `pledge(2)`/`unveil(2)` policy | Full-file review | No build performed |
| `pax/tty_subs.c` | C; controlling-terminal input and diagnostics | Low | Already documented; existing comments cover shared terminal streams and warning behavior | Full-file review | No build performed |
| `pax/getoldopt.c` | C; historic tar-style option parsing compatibility | Low | Already documented; file-level comment describes the legacy parsing switch | Full-file review | No build performed |
| `pax/pax.h` | C header; archive-entry state, format callback table, shared constants and predicates | High | Documented; clarified repeated macro evaluation and corrected the format-ID callback return contract | Full-header review; diff inspection | Callers must pass side-effect-free expressions to the repeated-evaluation macros |
| `pax/extern.h` | C header; cross-file `pax` declarations and global interfaces | Medium | Already documented by source-group sections | Full-header review | No build performed |
| `pax/tar.h` | C header; fixed-width tar/ustar layouts and format constants | Medium | Already documented; layout assertions guard on-disk sizes and offsets | Full-header review | No build performed |
| `pax/cpio.h` | C header; fixed-width cpio layouts and byte-order helpers | Medium | Already documented; comments explain binary field representation and padding | Full-header review | No build performed |
| `pax/file_subs.c` | C; node creation, ownership/mode/time restoration, hard links, sparse file writes | High | Already documented; existing comments describe replacement policy, security constraints, directory identity checks, and sparse-block handling | Full-file review | No build performed |
| `pax/ar_subs.c` | C; archive list, extract, append, archive, and filesystem-copy control flows | High | Already documented; existing comments explain format-independent operation sequencing and recovery | Full-file review | No build performed |
| `pax/ar_io.c` | C; archive descriptor lifecycle, device-specific I/O, volume changes, and gzip child process | High | Documented; existing comments cover device block sizing, partial I/O, append positioning, resynchronization, and tape operations | Full-file review; static diff review | Closed and invalidated `arfd` when `chdir(chdname)` fails |
| `pax/cpio.c` | C; portable, SVR4, CRC, and binary cpio header readers/writers | High | Already documented; existing comments cover serialized fields, byte order, padding, and return contracts | Full-file review | No build performed |
| `pax/tar.c` | C; tar/ustar/PAX readers and writers, extended-header parsing, and PAX option state | High | Documented; clarified global versus per-entry extended-header state and payload draining, plus empty-value keyword removal | Full-file review; `git diff --check` | Malformed extended-header record length parsing warrants focused review; no build performed |
| `pax/tables.c` | C; hard-link, update-time, symlink, rename, device mapping, and directory restoration tables | High | Documented; existing comments explain table invariants and deferred processing | Full-file review; static diff review | Corrected the postorder path match in `do_atdir()` and aligned `map_dev()`'s return description with its warning-and-continue behavior |
| `pax/pat_rep.c` | C; pattern matching, pathname policy, substitution, rename mapping | High | Partially documented; reviewed `mod_name()` and clarified operation order | Function and callers inspected | Full-file review pending |
| `ext4fs/sbin/mount_ext4fs/mount_ext4fs.c` | C; mount utility argument parsing and `mount(2)` request | Low | Documented; explained the shared `ufs_args` device-path handoff | Full-file review | No build performed |
| `ext4fs/sys/ufs/ext4fs/ext4fs_journal.c` | C; JBD2 journal mapping, scan, revoke, and replay | High | Partially documented; inspected block mapping and scan/replay paths | Selected functions reviewed | Full-file review pending |
| `ext4fs/sys/ufs/ext4fs/ext4fs_vfsops.c` | C; filesystem mount, superblock, inode, and VFS operations | High | Partially documented; mount argument consumption inspected | Selected mount path reviewed | Full-file review pending |
| `ext4fs/sys/ufs/ext4fs/ext4fs_vnops.c` | C; vnode operations and extent-tree allocation/truncation | High | Partially documented; extent-tree helpers inspected | Selected functions reviewed | Full-file review pending |
| `stdckdint/include/stdckdint.h` | C header; C23 checked-integer arithmetic macros | Low | Already documented; existing header explains builtin contract and argument evaluation | Full-header review | No compile performed |

## Coverage at the end of the earlier pass

At this snapshot, 19 C/header files are fully reviewed and 4 more have only
partial function-level review. The remaining 2,304 C/header files are pending,
including most of `libc/`. Two of the 16 Shell files have been reviewed; the
remaining 14 are pending source review. Shell syntax checks performed on
additional scripts do not change their pending status.

Among Shell files, only these have been reviewed: `build.ksh`,
`fetch-src.ksh`. All C/header files not listed above remain pending. Continue
updating this log as files are read and comments are added or found sufficient.

## Static findings from the earlier pass

The `FSUB.id` declaration comment in `pax/pax.h` was corrected to match the
format-ID implementations (zero means valid, and minus one means invalid).
The earlier implementation changes addressed these static findings:

- `pax/tables.c` (`do_atdir()`): the postorder FTS path must match the path
  stored during the preorder visit. The inverted comparison skipped the
  normal match and prevented timely restoration of that directory's times.
- `pax/tables.c` (`map_dev()`): allocation/remapping failures warn that
  archive hard links may be incorrect but deliberately continue; corrected
  the stale comment that promised an error return.
- `pax/ar_io.c` (`ar_open()`): close and invalidate the opened archive
  descriptor, and clear the unlink eligibility flag, when the requested
  `chdir()` fails.

The earlier concern about `strtol()` in `pax/tar.c` was not confirmed. Before
the conversion, the parser requires a space within the available record
bytes; because parsing starts at a digit, that in-buffer space or an earlier
non-digit terminates `strtol()` within the buffer.

## Documentation pass against 32f8de8

The inventory covered all tracked paths and separated executable sources,
interfaces, configuration, build rules, tests and delivery patches from
assets and generated material. Detailed review followed the control and data
paths below. Existing comments were retained where they already described
the implementation adequately.

| Area | Files and contracts documented |
| --- | --- |
| pax | Archive I/O, traversal, format dispatch and member headers in `pax/{ar_io,ar_subs,buf_subs,cpio,ftree,getoldopt,options,pax,tar}.c` and `pax/pax.h`. Extraction, substitution, time tests and deferred link/metadata handling in `pax/{file_subs,pat_rep,sel_subs,tables}.c`. The Makefile comment explains how installed names select option parsing. |
| ext4fs | The mount helper and its Makefile, and the headers and C sources under `ext4fs/sys/ufs/ext4fs/`. The comments cover serialized inode slots, CRC representation, the committed JBD2 replay range, mount references and the order of persistent writes. |
| FVWM | Controller startup, helper IPC, module transport and PipeRead assembly under `fvwm/fvwm/` and `fvwm/libs/Module.c`. Reviewed the controller interactions of all 17 built modules under `fvwm/modules/`. Also clarified shared build rules, the sample configuration and the icon utility. |
| libc and stdckdint | ISO header interfaces and their local implementations, with edits to `stdatomic.h`, `stdatomic.3`, `atomic_fallback.c` and `thrd.c`, native regression drivers, the host harness and checked-integer tests. The three delivery patches mirror the relevant source edits. |
| Tooling and CI | Root Makefile, build and CVS synchronization scripts, Makefile and knowledge validators, workflow, contract tests and host harness. The changes explain publication and rollback, range selection, child pipe handling and the scope of test shims. |

The earlier inventory remains a historical snapshot. A function-by-function
audit of inherited libc and FVWM code remains open, especially libc's
machine-dependent implementations and FVWM's geometry, decoration and X11
resource ownership. Native OpenBSD builds, regressions, X11 sessions and
filesystem recovery tests require a matching system or disposable VM.

### Verification

Compared effective source and configuration with the baseline and checked
the corresponding sections of the three delivery patches. The two Linux
host suites produced identical output from clean builds at the same path.
Nine of twelve executables matched byte for byte. In the three instrumented
executables that differed, compiled instructions matched and the compiler
IR differed only in UBSan source coordinates. The local C99 regressions for
both stdckdint copies also passed and produced identical executable pairs.
The host harness uses X11 headers and kernel shims, so these results cover
the exercised userspace paths.

### Findings from the documentation pass

- Check whether pax's global substitution loop can advance after a
  zero-width match that emits no bytes.
- Exercise ext4fs inode-write failure and retry on OpenBSD. The inode's
  modified flag is cleared before reading its backing buffer. Also measure
  the time spent in linear revocation-table searches during large recoveries.
- Cover FVWM startup actions that need the execution helper, short
  unterminated PipeRead output and the helper's bounded child-reap tracking
  in a native X11 integration test.
- Extend the libc regressions to check the 16-byte atomic exchange return
  value before it is overwritten, and review the getexecpath test's
  argument-count check.

### Corrections and verification

- pax advances after a zero-width match and preserves the unmatched byte.
  A host harness reproduced the stall and verified empty and nonempty
  replacements. The native archive test is `tests/pax-subst-tests.sh`.
- ext4fs retains the modified inode across read and synchronous write
  failures. The JBD2 revoke tree indexes distinct blocks and compares
  sequences relative to the recovery start. Host tests cover failed inode
  I/O, duplicate revocations, sequence wrap and a large set of keys.
- FVWM starts its execution helper before dispatching startup configuration.
  PipeRead sends its current environment in bounded chunks and drains
  a final line at EOF. The helper reaps all exited children even when its
  exit-report slots are full. The transport and line handling have host
  coverage. Startup and real libutil IPC still need an OpenBSD X11 session.
- The libc tests check both results of a 16-byte exchange and require the
  expected pathname argument in getexecpath. The host suite and a targeted
  getexecpath stub exercised the corrected checks.
