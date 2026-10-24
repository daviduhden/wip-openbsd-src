---
name: openbsd-fvwm-development
description: Maintain FVWM in Xenocara. Use when changing window management, event handling, module IPC, fvwm_exec, configuration parsing, X11 resources, signal handling, or the pledge/unveil policy of the controller, modules, or xpmroot.
---

# FVWM development

## When to use

Use for changes under `fvwm/`: the window manager, `fvwm_exec`, modules,
`xpmroot`, or the sample configuration.

Do not require a graphical session for logic that can be tested with the
host-side unit tests. Do not add a dependency that is not in Xenocara/X11.

## Objective

Change one FVWM behavior or process policy while preserving the X11 event
model, module protocol, configuration compatibility, and the separation
between the controller, `fvwm_exec`, and modules.

## Prerequisites

- Xenocara/X11 development files under `/usr/X11R6`, or a host with the X11
  headers for the logic tests.
- The affected module/controller process and its sandbox stage are identified.
- For live testing, a disposable X server or nested session is available.
- For logic-only changes, `bmake` and the host test suite are available.

## Architecture

| Path | Responsibility |
|---|---|
| `fvwm/fvwm/` | window manager core, event loop, windows, menus, modules, `fvwm_exec` |
| `fvwm/libs/` | static `libfvwm.a`: parsing, strings, system helpers |
| `fvwm/modules/` | 17 module processes (`FvwmButtons`, `FvwmPager`, ...) |
| `fvwm/utils/` | `xpmroot` |
| `fvwm/icons/` | bitmaps and pixmaps |
| `fvwm/sample.fvwmrc/` | default configuration examples |
| `fvwm/compat/` | vendored `<stdckdint.h>` |
| `fvwm/docs/` | HTML/text documentation, not built |

Build integration:

- `fvwm/Makefile` recurses into `libs fvwm sample.fvwmrc utils icons modules`.
- `fvwm/Makefile.inc` sets `X11BASE`, `CFLAGS_COMMON`, `NOOBJ = Yes`, and
  `LDADD += -L${.CURDIR}/../libs -lfvwm`.
- `fvwm/modules/Makefile.inc` provides the module flags and `XLIB`.
- `bsd.xorg.mk` supplies Xenocara install paths and man directories.

## Process model

- **Controller (`fvwm`)**: owns the X11 connection, runs the event loop,
  launches modules, restarts. Deliberately unpledged; locks an unveil view.
- **`fvwm_exec`**: same-UID helper started before the controller locks unveil.
  Handles `Exec` and `PipeRead` over an imsg protocol. Deliberately unpledged
  and unveiled; `closefrom(3)` before `execve`. It is not a privilege boundary.
- **Modules**: separate processes, each with its own pledge policy. `execve`
  clears the controller's unveil view, so modules get a fresh filesystem view.
- **`xpmroot`**: X11 utility; pledge after `XOpenDisplay`.

## Procedure

1. **Identify the process.** A change in `fvwm/fvwm/` affects the controller; a
   module has its own `main` and its own sandbox. Do not apply a module policy
   to the controller.
2. **Check the event path.** X events enter through `events.c`; window creation,
   mapping, and properties go through `add_window.c`, `placement.c`, and
   `decorations.c`. Follow the data to its use.
3. **Validate X11 properties.** Before casting a property pointer, require
   `actual_format == 32` (or 8 for strings), the expected `actual_type`, and
   `nitems >= 1`. Free the property on every path.
4. **Check configuration parsing.** Use `FvwmParseInteger()` for historical
   `atoi(3)`-style prefix semantics, or `strtonum(3)` for strict values. Do not
   add a new parser with different overflow behavior.
5. **Check process creation.** `Exec`/`PipeRead` go through `fvwm_exec`; module
   startup uses `fork`/`execvp` in the controller. A new launcher must consider
   unveil `x` permissions and the absence of `proc`/`exec` promises.
6. **Check signals.** `DeadPipe` and shutdown handlers must be
   async-signal-safe where possible. Do not add stdio or `exit()` to a handler
   that may interrupt stdio.
7. **Check sandbox placement.** Pledge only after `XOpenDisplay`, cursor
   creation, font/GC setup, and any lazy Xlib load. `pledge` before a lazy
   `dlopen`/`mmap(PROT_EXEC)` aborts the process.
8. **Check both build styles.** The controller and modules build from different
   `Makefile.inc` files but share `-std=c23` and the X11 flags.

## Useful commands

```sh
# Build everything
cd fvwm && make

# Build one module
cd fvwm/modules/FvwmPager && make

# Inspect the build graph
make -n

# Host-side logic tests (no X server needed)
bmake -C tests fvwm-parse-tests fvwm-exec-tests fvwm-pipebuf-tests
```

## Testing

- **Without X11:** `tests/fvwm-parse-tests`, `tests/fvwm-exec-tests`, and
  `tests/fvwm-pipebuf-tests` exercise the tokenizer, imsg decoder, and PipeRead
  buffer. They need the X11 headers for types but no server.
- **With X11:** start a disposable X server or nested session, run `fvwm -f`
  with a minimal config, start each affected module, and check `ps`/`lastcomm`
  for pledge status and SIGABRT.
- **Security-sensitive changes:** use `ktrace -di ./fvwm` and inspect
  `kdump` for unexpected paths or `PROT_EXEC` mappings.

Do not claim a live-session test that was not performed.

## Validation and acceptance

- The controller and every affected module start and exit cleanly.
- No SIGABRT from a missing pledge promise.
- X11 property reads check format/type/nitems.
- Configuration syntax remains compatible with FVWM 2.2.5 where claimed.
- Module IPC message layouts are unchanged unless the protocol change is
  intentional and both sides are updated.
- The man page (`fvwm2.1`, `fvwm_exec.1`, or the module page) matches behavior.
- No new external dependency outside Xenocara/X11.

## Common problems and diagnosis

| Symptom | Check |
|---|---|
| Module aborts after startup | Pledge before a needed Xlib lazy load; move pledge later |
| `XCreateFontCursor` aborts | Cursor creation after pledge; create cursors first |
| `Exec` silently fails | `fvwm_exec` socket/imsg setup or unveil `x` path |
| `PipeRead` hangs | Pipe buffer limits, EOF handling, helper reaping |
| Menu icons fail | `IconPath` outside the controller's unveil view |
| Property garbage or crash | Missing `actual_format`/`nitems` check |
| Module cannot read config | Fresh unveil view after `execve`, not the controller's |
| `FvwmM4` command injection | Unsafe `TMPDIR`/`-outfile` interpolated into `popen` |

## References

- `fvwm2.1`, `fvwm_exec.1`, module pages under `fvwm/modules/*/`
- `fvwm/CHANGES.txt`, `fvwm/README`
- Xenocara: <https://xenocara.org>
- `pledge(2)`, `unveil(2)`, `imsg_init(3)`, `closefrom(3)`
- `openbsd-source-security`, `openbsd-source-build`
