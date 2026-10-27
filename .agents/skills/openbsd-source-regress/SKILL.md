---
name: openbsd-source-regress
description: Add, run, or review OpenBSD regress tests. Use when creating a regression under libc/regress or stdckdint/regress, using bsd.regress.mk, reproducing an upstream bug, validating exit codes and boundary cases, or distinguishing static analysis from an executed test.
---

# OpenBSD regress testing

## When to use

Use when adding a test under `libc/regress/`, `stdckdint/regress/`, or a
component's `regress` directory, or when reproducing a bug with a minimal
program.

Do not use for host-only tests in `tests/` or `libc/tests/` without also
checking `openbsd-source-build`; those are not part of the OpenBSD build graph.

## Objective

Produce a deterministic, least-privilege test that fails on the bug and passes
on the fix, using the OpenBSD regress framework and without inventing results.

## Prerequisites

- An OpenBSD host or a full source tree with `/usr/share/mk`.
- The component builds.
- The expected behavior is documented in a man page, standard, or upstream
  commit; not guessed.
- For privileged behavior (raw sockets, devices, mounts), explicit authorization
  and a rollback plan.

## Framework facts

- A regress directory has a `Makefile` with `PROG` and
  `.include <bsd.regress.mk>`.
- `bsd.regress.mk` makes `all` depend on `regress` and provides
  `run-regress-${PROG}`.
- The conventional invocation is `make regress` in the directory, or
  `make regress` in a parent with `SUBDIR`.
- `PROG` names the test binary; it is built from `SRCS` or `${PROG}.c`.
- `CFLAGS+=` and `LDADD+=` are the normal per-test hooks.
- Do not use GNU make constructs. The first line of the Makefile is
  `#	$OpenBSD$` (a make comment, not a C comment).

## Procedure

1. **Reproduce before changing code.** Write the smallest input that
   demonstrates the failure, or record the exact command and output.
2. **Choose the layer.**
   - Pure parsing/arithmetic: a standalone regress binary.
   - Library behavior: a regress under `libc/regress/lib/libc/<area>/`.
   - Kernel/filesystem: a privileged regress or a manual procedure; state the
     privilege requirement.
3. **Create the directory and Makefile.**
   ```make
   #	$OpenBSD$

   PROG=	example

   .include <bsd.regress.mk>
   ```
4. **Write positive, negative, and boundary cases.** Check both the return
   value and the output/state. For integer APIs, include the exact type limits
   and mixed signed/unsigned cases. For parsers, include truncated, oversized,
   and malformed inputs.
5. **Check exit status.** A test that prints `FAIL` but exits 0 is not a test.
6. **Keep privileges minimal.** Prefer an unprivileged test. If root is
   required, skip cleanly when not root and say so.
7. **Wire it into the parent.** Add the directory to the appropriate `SUBDIR`
   list in `libc/regress/lib/libc/Makefile` or the component's regress parent.
8. **Run it.**
   ```sh
   cd libc/regress/lib/libc/example && make && ./example
   # or
   make regress
   ```
9. **Record the exact command, environment, and result** in the change
   description or report. If it was not run, say "not run".

## Useful commands

```sh
# Build and run one test
cd libc/regress/lib/libc/stdckdint && make && ./stdckdint

# Run through the framework
make regress

# Inspect the graph
make -n regress

# Reproduce with a sanitizer where the host supports it
cc -fsanitize=address,undefined -g -o test test.c && ./test
```

## Validation and acceptance

- The test fails on the pre-fix code and passes on the fixed code. If the
  pre-fix state is unavailable, explain why.
- The test is deterministic: no wall-clock, PID, hostname, or filesystem-order
  dependence.
- The test cleans up temporary files and directories.
- The test does not require network access unless the feature is network-only.
- The parent `SUBDIR` list and any `Makefile.inc` are updated.
- No test binary or object file is committed.

## Common problems and diagnosis

| Symptom | Check |
|---|---|
| `Invalid line "/* $OpenBSD$ */"` | Use `#`, not a C comment, at the top of a Makefile |
| `Could not find bsd.regress.mk` | Not on OpenBSD or `/usr/share/mk` missing |
| Test runs but always passes | Assertions do not check the failure path or exit code |
| `make regress` builds nothing | `PROG`/`SRCS` mismatch or wrong `.include` |
| Test passes only as root | Privilege-dependent behavior; split or skip explicitly |
| Flaky ordering | Sort output, use `LC_ALL=C`, avoid directory iteration order |

## Difference between evidence levels

- **Static analysis:** reading code, compiler diagnostics without execution.
- **Compiled:** the test binary was produced.
- **Executed:** the binary ran and its exit status/output were observed.
- **Manual:** a human followed a procedure, e.g. a live X11 session.

Never report one level as another. Do not claim an OpenBSD run performed on a
Linux host, or a kernel test performed without a kernel.

## References

- `bsd.regress.mk(5)`: <https://man.openbsd.org/bsd.regress.mk.5>
- `make(1)`: <https://man.openbsd.org/make.1>
- OpenBSD testing: <https://www.openbsd.org/faq/ports/testing.html>
- Repository: `libc/regress/lib/libc/Makefile`, `stdckdint/regress/`

## Maintenance & Continuous Learning

Whenever this skill is used to change the repository, review this skill after
the implementation. Correct procedures that proved wrong, update commands,
paths, configuration and examples, incorporate reusable findings, and remove
outdated assumptions. Review the applicable root and nested `AGENTS.md` files,
specialized agents and related skills; update only the knowledge affected by
the work. Validate local references and run
`python3 scripts/validate-agent-knowledge.py --scope worktree` plus the
applicable project checks. For pull request comparisons, use the documented
`--scope range --base BASE_REF --head HEAD_REF` command. The canonical validator
is read-only and is required even when a task restricts execution of project
scripts; this does not authorize running installers or changing the host.
Report checks actually performed. A structural pass does not prove semantic
relevance, and a task is incomplete until synchronization is validated.
