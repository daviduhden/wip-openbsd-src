---
name: openbsd-source-security
description: Audit OpenBSD code for memory-safety, parsing, path-traversal, privilege, and sandbox defects. Use when reviewing pax archive handling, FVWM X11 property parsing, pledge/unveil staging, setuid/permission restoration, integer overflow, TOCTOU, or attacker-controlled on-disk and X11 input.
---

# OpenBSD source security

## When to use

Use for security review of archive extraction, parsers, filesystem code, X11
property handling, privileged operations, and pledge/unveil policy.

Do not use to claim that a process is fully isolated, or to add a sandbox
policy without understanding the program's full lifecycle.

## Objective

Find and fix reachable defects while preserving legitimate functionality, and
state precisely what was verified statically versus at runtime on OpenBSD.

## Threat model checklist

- **Inputs:** archive members, extended headers, cpio headers, file lists,
  command-line options, config files, X11 properties, on-disk filesystem
  metadata, environment variables (`TMPDIR`, `HOME`, `DISPLAY`).
- **Trust boundary:** unprivileged user vs. root; same-UID module vs.
  controller; X client vs. window manager; untrusted archive vs. extracting
  user.
- **Assets:** files outside the extraction directory, setuid/setgid bits,
  owned files, the X11 connection, the execution helper, kernel memory.

## Procedure

1. **Map the lifecycle.** For each process: who starts it, what it opens before
   the sandbox, what it needs later, and whether it forks/execs.
2. **Trace untrusted data.** Follow the buffer from read to use. Check length,
   type, format, and termination before indexing or casting.
3. **Check arithmetic.** Every size computation, offset, and multiplication.
   Prefer `<stdckdint.h>` where the file uses it; otherwise explicit range
   checks. Watch signed/unsigned conversions and `off_t`/`size_t` truncation.
4. **Check memory and descriptors.** Use-after-free, double-close, leaks on
   error paths, `O_CLOEXEC`, `closefrom(3)`, partial allocation cleanup.
5. **Check filesystem operations.** Absolute paths, `..`, symlinks, hardlinks,
   device nodes, `AT_SYMLINK_NOFOLLOW`, `O_EXCL`, `O_NOFOLLOW`, directory
   identity checks, `umask`, setuid/setgid clearing.
6. **Check race windows.** `lstat` then `open`, `access` then `open`, temp-file
   name reuse, and directory replacement. Use `openat(2)`/`fstatat(2)` where
   the component does.
7. **Check `pledge`/`unveil` staging.**
   - Build the minimum promise set for each mode and stage.
   - Call `pledge` only after all lazy library loads and file opens that need
     broader access.
   - Lock `unveil` with `unveil(NULL, NULL)` only after every path is known.
   - Remember that `execve(2)` clears the unveil view regardless of
     execpromises.
   - Do not add `proc exec` to a launcher that deliberately leaves the child
     unsandboxed.
8. **Check privilege separation claims.** A same-UID helper with no sandbox is
   not a privilege boundary. A controller that can ask it to execute arbitrary
   commands is still fully exposed.
9. **Write a regression test** or a precise manual procedure. Do not claim
   runtime enforcement without OpenBSD.

## Repository-specific facts

### pax

- Default `rmleadslash = 1`: leading `/` and leading `..` components are
  removed. `-P` disables this for pax; `tar -P` has different semantics.
- Symlinks with absolute or `..` targets are deferred as `O_EXCL` placeholders
  and replaced after extraction with dev/ino verification.
- `pmode` restoration must not restore setuid/setgid unless the chown succeeded
  and `-p` requested it.
- The `unveil(".", "rwc")` view is the backstop for extraction. Do not remove it
  without replacing the path checks.
- Compressed modes are deliberately unpledged because a later volume may need
  another compressor. Do not describe pax as unconditionally sandboxed.
- `APPND` needs `cpath` when `-u` uses a scratch file.

### fvwm

- The controller is unpledged and holds the X11 connection.
- `fvwm_exec` is unpledged, unveiled, and runs as the same user; it is not a
  privilege boundary.
- Modules are pledged after `XOpenDisplay`; a lazy Xlib load after `pledge` can
  abort the process (the `c0b4b4e` bug class).
- X11 properties are untrusted even from the same session. Check
  `actual_format`, `actual_type`, and `nitems` before casting.
- `FvwmM4` interpolates `TMPDIR`/`-outfile` into a shell command; reject unsafe
  characters or quote correctly.

### ext4fs

- All on-disk counts (`eh_entries`, `eh_max`, `reclen`, `namlen`, inode size)
  must be validated against block/inode capacity before indexing.
- Journal replay must validate descriptor tags before use.
- Widen shifts through `u_int64_t` where the on-disk field is unsigned.

## Verification on OpenBSD

```sh
# Syscall and path audit
ktrace -di ./pax -xf archive.tar
kdump -d ktrace.out

# Sandbox violations
lastcomm | grep -E ' p | U '

# W^X and mapping failures
ktrace -di ./fvwm
```

Static review cannot prove that a promise set is sufficient. Record the
limitation explicitly.

## Validation and acceptance

- The defect is reachable from a named input or state.
- The fix does not remove a documented feature.
- Error paths preserve `errno` and free/close correctly.
- A regression test or a precise manual reproduction exists.
- Sandbox claims are labeled "static" or "runtime-verified on OpenBSD".
- No partial restriction is presented as complete isolation.

## Common problems and diagnosis

| Symptom | Likely cause |
|---|---|
| SIGABRT after pledge | Missing promise, or lazy `dlopen`/`mmap(PROT_EXEC)` after pledge |
| `ENOENT` after unveil | Path opened after the lock but never unveiled |
| Setuid bit restored unexpectedly | `pmode` path did not verify chown success |
| Extraction escapes the tree | `-P`, hardlink target, or pre-existing symlink |
| Heap over-read on X property | Missing `format`/`nitems` check |
| Truncated archive accepted | Record length checked after use |

## References

- `pledge(2)`: <https://man.openbsd.org/pledge.2>
- `unveil(2)`: <https://man.openbsd.org/unveil.2>
- `style(9)`: <https://man.openbsd.org/style.9>
- OpenBSD security: <https://www.openbsd.org/security.html>
- `ktrace(1)`, `kdump(1)`, `lastcomm(1)`
