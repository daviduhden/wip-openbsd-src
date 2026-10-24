---
name: openbsd-source-development
description: Develop and modify OpenBSD base-system C code in this repository. Use when changing pax, the imported libc additions, ext4fs, or shared C code; covers style(9)/KNF, OpenBSD interfaces, memory and descriptor handling, error handling, API/ABI review, minimal upstream-oriented changes, and regression checks.
---

# OpenBSD source development

## When to use

Use for any C change under `pax/`, `libc/`, `ext4fs/`, or shared test code.
Use `openbsd-pax-development` for archive-format semantics and
`openbsd-fvwm-development` for Xenocara/FVWM code.

Do not use for documentation-only changes (use `openbsd-source-manpages`),
build-system-only changes (use `openbsd-source-build`), or patch export (use
`openbsd-source-cvs-diff`).

## Objective

Make the smallest correct change that preserves OpenBSD behavior, style, and
upstream compatibility, and that can be reviewed as a standalone diff.

## Prerequisites

- Read `AGENTS.md`.
- Read the component `Makefile` and the file's surrounding conventions.
- Check local divergence: `git log --oneline -- <path>` and `git diff`.
- Have the relevant OpenBSD man page available (`style(9)`, `err(3)`,
  `reallocarray(3)`, `pledge(2)`, `unveil(2)`, component-specific pages).
- Know whether the target is the local C23 build or an upstream C99/C11 file.

## Procedure

1. **Establish the baseline.** Read the whole function and its callers, not
   just the changed line. Run `git log -p -- <path>` for recent local changes.
2. **Identify the contract.** Record inputs, outputs, ownership, locking,
   privileges, and error returns. For library code, check the installed
   prototype and man page.
3. **Check the language level.** `pax/` and `fvwm/` use `-std=c23`; `ext4fs/`
   is kernel C11; `libc/` must remain C11/C17/C23-compatible. Do not introduce
   `nullptr`, `constexpr`, or `static_assert` into upstream-targeted C99 code.
4. **Apply KNF locally.** Tabs for indentation, 8-column tab stops, function
   return type on its own line, `return (value);` in files that use it. Do not
   reformat untouched lines.
5. **Handle allocation and overflow.** Use `reallocarray(3)` or
   `recallocarray(3)`; use `<stdckdint.h>` where the file already does; check
   every multiplication used as a size.
6. **Handle descriptors and memory.** Set `O_CLOEXEC` where the component does;
   close on every error path; free partial allocations; preserve `errno` before
   cleanup calls.
7. **Handle errors in component style.** `pax` uses `paxwarn()`/`syswarn()`
   and returns `-1`; base utilities use `err(3)`/`warn(3)`; library code returns
   error numbers or `-1` as documented. Never silently ignore a failure unless
   the existing code documents why.
8. **Review ABI.** For `libc/`, check `Symbols.list`, `shlib_version`, hidden
   headers, namespace rules, and man pages. For `pax`/`fvwm`, internal ABI
   changes must not break the multicall links or module protocol.
9. **Review behavior changes.** Compare with the previous implementation and
   with upstream. State whether the change is a bug fix, a compatibility
   change, or a local policy change.
10. **Add or adjust tests** using `openbsd-source-regress`; do not claim a test
    that was not run.
11. **Inspect the diff** with `git diff --check` and `git diff -- <paths>`.

## Useful commands

```sh
# Context and local history
git log --oneline -- pax/
git log -p -- pax/pat_rep.c
git diff -- pax/

# Style reference
man style
man 9 style

# Interfaces
man 3 reallocarray
man 3 strtonum
man 2 pledge
man 2 unveil

# Static review
git diff --check
grep -n 'TODO\|XXX\|FIXME' <file>
```

## Validation and acceptance

- The change compiles in the component's real build environment. If it was not
  built, say so explicitly.
- No new warnings under the component flags (`-Wall -Wextra -Wpedantic`).
- No new unchecked allocation, descriptor leak, or error-path use-after-free
  in the changed code.
- Behavior matches the documented contract and the man page.
- The diff contains no unrelated formatting, generated files, or binary
  artifacts.
- `git diff --check` is clean.

## Common problems and diagnosis

| Symptom | Check |
|---|---|
| `static declaration follows non-static` | Declaration in a header or earlier prototype; make both `static` |
| `implicit declaration` | Missing header or wrong feature-test macro |
| Link failure after adding a function | `SRCS`, `LINKS`, `Symbols.list`, or `Makefile.inc` not updated |
| Works locally, fails upstream | C23 construct, local helper, or OpenBSD-only API used in C99 code |
| Behavior changed at a boundary | Off-by-one, signed/unsigned conversion, or `off_t`/`size_t` truncation |
| Archive extraction escapes a directory | Missing `..`/absolute-path handling or symlink deferral |

## References

- `style(9)`: <https://man.openbsd.org/style.9>
- `err(3)`: <https://man.openbsd.org/err.3>
- `reallocarray(3)`: <https://man.openbsd.org/reallocarray.3>
- `pledge(2)`: <https://man.openbsd.org/pledge.2>
- `unveil(2)`: <https://man.openbsd.org/unveil.2>
- `release(8)`: <https://man.openbsd.org/release.8>
- Repository: `AGENTS.md`, `FETCHING.md`, `libc/UPSTREAM`
