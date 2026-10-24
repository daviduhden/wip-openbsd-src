# AGENTS.md

Instructions for AI agents working in `wip-openbsd-src`.

This repository is **not** a copy of `/usr/src` or `/usr/xenocara`. It is a
Git working area for a small set of OpenBSD base-system and Xenocara
components. Read this file before changing code; load the specialized skill
that matches the task.

## Repository architecture

### Purpose

`wip-openbsd-src` develops and stages changes for:

| Local path | Upstream location | Delivery model |
|---|---|---|
| `pax/` | `src/bin/pax` | whole-directory replacement |
| `fvwm/` | `xenocara/app/fvwm` | whole-directory replacement |
| `libc/` | `src/include`, `src/lib/libc`, `src/lib/libpthread`, `src/sys/**` | patch-style import |
| `ext4fs/` | `src/sys/ufs/ext4fs`, `src/sbin/mount_ext4fs` | new files + `sys-patches.diff` |
| `stdckdint/` | `src/include/stdckdint.h`, `src/regress/lib/libc/stdckdint` | patch-style contribution |
| `tests/` | none | host-side developer tests |

`pax/` and `fvwm/` build standalone. The other components are copied into a
full checkout or applied as patches. `fetch-src.ksh` handles only `pax` and
`fvwm`; see `FETCHING.md` and the `openbsd-source-sync` skill.

### Git versus CVS

- OpenBSD development uses CVS. This repository uses Git for local work.
- Git commits, branches, and diffs are **not** CVS metadata. A Git change does
  not register added files, removed files, or new directories in CVS.
- `fetch-src.ksh --copy-only` preserves nested `CVS/` directories and records
  added/removed paths, but it never runs `cvs add` or `cvs remove`.
- Prepare upstream changes with CVS commands and `cvs diff -uNp`; see the
  `openbsd-source-cvs-diff` skill.

### OpenBSD-current

Targets are OpenBSD 7.9 and 8.0-beta, with OpenBSD-current conventions. The
imported libc records its exact upstream revision in `libc/UPSTREAM`. Do not
assume that an upstream change seen on the web already exists locally, or that
a local change exists upstream. Check both sides before claiming either.

### Base system versus Xenocara

- `src` contains the kernel, libc, and base userland. `pax` belongs here.
- Xenocara is the X11 build tree. `fvwm` belongs under `xenocara/app/`.
- They are separate checkouts and separate build systems. Do not place a
  Xenocara component under `src`, or vice versa.

### Local layout rules

- Keep each component's internal layout identical to its upstream directory
  unless a documented standalone build requires otherwise (`pax/Makefile`,
  `fvwm/Makefile*`).
- Do not add a top-level directory for a new component without updating
  `README.md`, `FETCHING.md`, this file, and (if the component is copied as a
  directory) `fetch-src.ksh`.
- Patch-style components keep source-relative paths inside their local
  directory, so the mapping to upstream is visible in the path itself.
- Never create `CVS/` directories by hand. Use CVS in a real checkout.

### Adding a new component

1. Identify the exact upstream path and revision.
2. Copy sources, `Makefile`s, headers, man pages, and license files.
3. Preserve `$OpenBSD$` tags, copyright notices, and license text.
4. Decide whether it is a directory replacement or a patch-style component.
5. Record the mapping in `README.md` and `FETCHING.md`.
6. If it is a directory replacement, extend `fetch-src.ksh` in a separate,
   reviewed change.
7. Compare against upstream, build, and test as far as the platform allows.
8. Prepare CVS additions/removals separately from unrelated changes.

Do not modify `fetch-src.ksh` or import a new component as a side effect of an
unrelated task.

## Programming conventions

### KNF and `style(9)`

Apply `style(9)`/KNF to new and modified code:

- 8-column tabs for indentation; spaces for continuation alignment.
- Return type on its own line for function definitions; opening brace at
  column 0.
- `if`, `for`, `while`, and `switch` braces on the same line as the keyword.
- `case` labels at the same indentation as `switch`.
- `return (value);` in imported/KNF code; do not mass-convert existing files.
- Use `NULL` in imported OpenBSD code and `nullptr` only where the local file
  already uses C23. Do not mix styles gratuitously.
- Use `__dead`, `__packed`, `__unused`, `__printflike`, and `__nonnull` from
  `<sys/cdefs.h>` instead of raw `__attribute__` in OpenBSD code.

The repository's `.clang-format` is an approximation, not an authority. Never
reformat a whole file or tree to satisfy it. Existing style wins, and
`// clang-format off` blocks around includes are intentional.

### C standard

- `pax`, `fvwm`, and the host tests build as ISO C23 (`-std=c23`).
- `ext4fs` is kernel code and stays C11-compatible (`_Static_assert`), because
  the OpenBSD kernel build does not select C23.
- `libc` additions must remain usable by the C11/C17/C23 header matrix and by
  the OpenBSD libc build.
- Do not introduce compiler extensions unless the surrounding file already
  uses them and OpenBSD's clang supports them.
- When preparing an upstream patch, do not assume upstream accepts C23.
  Follow the target file's existing language level.

### Headers

- Public headers use `_FOO_H_` guards immediately after the copyright block.
- Internal project headers may use `PAX_H`-style names, but must have a guard.
- Declare functions with `(void)` when they take no arguments.
- Keep include order stable; do not let a formatter reorder `pax.h` before
  `extern.h`, `tar.h`, or `cpio.h`.

### Interfaces and libraries

Prefer OpenBSD and POSIX interfaces already used in the component:

- `strlcpy`, `strlcat`, `strtonum`, `reallocarray`, `recallocarray`
- `fts(3)`, `vis(3)`, `strmode(3)`, `getline(3)`
- `pledge(2)`, `unveil(2)`, `closefrom(3)`
- `pathconfat(2)` and `*at(2)` variants where the component already uses them
- `arc4random(3)` for randomness, not `rand(3)`

Do not add a third-party dependency to the base system. Do not add GNU make,
Bash, GNU sed/awk/find, or a downloaded build tool when a native tool exists.

### Shell

- Use `#!/bin/ksh` and pdksh-compatible constructs for user-facing scripts.
  Avoid ksh93-only `${var//pat/rep}`; use `tr` or `sed`.
- Use `#!/bin/sh` for portable test/validation drivers.
- Quote expansions unless word splitting is deliberate.
- Set a trusted `PATH` before privileged operations.
- Do not add `set -e` to a script whose failure handling is explicit; do not
  remove existing error checks.

### Makefiles

- Use BSD make syntax and `.include <bsd.prog.mk>`, `bsd.lib.mk`,
  `bsd.subdir.mk`, or `bsd.regress.mk` as appropriate.
- Recipes use tabs; directives use tabs only where the include file expects it.
- `NOOBJ = Yes` is used by in-place Xenocara builds; do not add `obj`
  assumptions to those directories.
- Do not add `-std=` to source-tree Makefiles (`ext4fs/`); the tree build owns
  the language level.
- Top-level `make` recurses into `pax` and `fvwm`; it does not build `libc`,
  `ext4fs`, or `stdckdint`.

### Manual pages

- Write mdoc(7), not man(7), for new pages.
- Use `.Dd`, `.Dt`, `.Os`, `.Sh`, `.Nm`, `.Nd`, `.Op`, `.Fl`, `.Ar`, `.Xr`.
- Use a literal `.Dd` date in this Git repository; `$Mdocdate$` does not expand
  outside CVS.
- Keep the man page and the implementation in the same change.
- Validate with `mandoc -T lint`; do not treat host `man.db` "referenced manual
  not found" warnings as source defects.

## Development rules

1. Prefer small, reviewable changes. One logical change per commit-sized unit.
2. Preserve functional compatibility with upstream unless the task explicitly
   changes behavior.
3. Do not add external dependencies to the base system.
4. Do not introduce GNU Make, Bash, or GNU-only tools when native alternatives
   exist.
5. Keep existing copyright notices and licenses. New code is ISC unless the
   surrounding component says otherwise.
6. Do not add author names or copyright lines without justification.
7. Keep manual pages synchronized with actual behavior.
8. Do not modify code outside the task's scope, including drive-by formatting.
9. Do not remove files or features without checking dependencies and callers.
10. Never claim a build, test, or runtime check that was not actually run.
11. Do not modify the installed system, `/usr/src`, `/usr/xenocara`, users,
    permissions, or global configuration without explicit authorization.
12. Do not add build or download mechanisms outside OpenBSD conventions.

## Working with subagents

Use subagents for independent investigation: one component, one question, one
patch area. Give each subagent an explicit scope and require a report with:

- files and revisions examined;
- defects or risks found, with file:line evidence;
- recommended changes and their blast radius;
- evidence, assumptions, and limitations;
- tests or static checks that are still required.

The main agent must integrate the reports, resolve contradictions, and verify
that no subagent changed files outside its assigned scope.

## Git policy

- Preserve pre-existing modifications. Inspect `git status` and `git diff`
  before editing.
- Do not alter the configured Git identity.
- Do not add `Co-authored-by`, "Generated by AI", or similar trailers unless the
  user explicitly asks.
- Do not create commits, amend, rebase, tag, or push unless the task authorizes
  it.
- Before delivering, run `git diff --check`, inspect `git status`, and review
  the complete diff. Do not include build artifacts or temporary files.
- Keep temporary analysis outside the repository or under a directory that is
  demonstrably ignored; verify with `git check-ignore`.

## Maintenance

- Change this file when a **general** convention changes.
- Change a `SKILL.md` when a **specific procedure** changes.
- When a component's layout changes, update every mapping table and path
  reference in this file and in the affected skills.
- When a tool or procedure disappears, delete its instructions instead of
  leaving stale guidance.
- Keep documentation synchronized with code, but do not edit it on every
  commit.

## Skills

Load the narrowest skill that covers the task:

| Skill | Use when |
|---|---|
| `openbsd-source-development` | changing C code in `pax` or another component |
| `openbsd-source-sync` | copying components into a checkout, backups, conflicts |
| `openbsd-source-build` | building one component or the whole tree |
| `openbsd-source-regress` | adding or running regress tests |
| `openbsd-source-security` | auditing pledge/unveil, parsing, memory safety |
| `openbsd-source-manpages` | writing or reviewing mdoc pages |
| `openbsd-source-cvs-diff` | preparing and checking upstream diffs |
| `openbsd-pax-development` | pax/tar/cpio formats, extraction, metadata |
| `openbsd-fvwm-development` | FVWM, modules, X11, fvwm_exec |
