# FVWM Source Review

This file tracks the source review and summarizes the architecture observed in
the OpenBSD FVWM 2.2.5 fork. A source file is marked fully reviewed only after
reading the complete file. Selected functions and call paths are recorded as
partial review; unlisted source files remain pending.

## Scope and provenance

The component identifies itself as FVWM 2.2.5 in `README` and `config.h`.
`CHANGES.txt` records the OpenBSD-only modernization relative to the version
previously in Xenocara: portability branches were removed, allocation helpers
were replaced with local fatal-allocation wrappers, and OpenBSD-specific
process and sandbox code was added. The tree contains the historical FVWM
implementation as well as these local adaptations; the history document is
the source for the fork's stated change rationale.

The source inventory contains 81 C files and 43 headers. It also contains two
`.sh` files, three extensionless Shell scripts (`xinitrc`, `fvwmrc_convert`,
and `quantize_pixmaps`), and the Perl utility `BuildXLockMenu`. There are 17
separately built module programs, in addition to the controller, `fvwm_exec`,
and `xpmroot`. Binary icons and pixmaps are assets, not source files.

## Architecture

- `fvwm/fvwm/fvwm.c` initializes the display and per-screen state, selects
  events on each root window, reads configuration, starts the execution
  helper, locks the controller's `unveil(2)` view, and enters `HandleEvents()`.
- `fvwm/fvwm/events.c` maps selected X event types to handlers. The handlers
  update managed-window state and issue the corresponding X requests. The
  event loop also services module pipes and helper IPC while waiting for X
  activity.
- `fvwm/fvwm/add_window.c`, `windows.c`, `misc.c`, and `tables.c` participate
  in client registration, state changes, and cleanup. `FvwmWindow` keeps a
  null-terminated management list separate from the circular stacking ring
  rooted at `Scr.FvwmRoot`.
- `focus.c`, `placement.c`, `move.c`, `resize.c`, and `virtual.c` implement
  focus policy, placement, geometry changes, desks, and viewports.
- `decorations.c`, `borders.c`, `icons.c`, `menus.c`, and `colors.c` create,
  update, and release window-manager decorations and X resources.
- `functions.c`, `builtins.c`, `complex.c`, `read.c`, `parse.h`, and
  `modconf.c` parse configuration and dispatch commands.
- `module.c` launches module processes and transports native-word packets over
  pipes. `libs/Module.c` supplies the corresponding module-side protocol API.
- `libs/` contains shared parsing, string, X resource, picture, color, and
  system helpers. `modules/` contains independent module processes, each with
  its own entry point and sandbox policy.

### Event and window flow

The controller flushes Xlib output, dispatches already queued X events first,
then uses `select(2)` to wait on the X connection, the `fvwm_exec` imsg socket,
module input pipes, and queued module output pipes. Helper and module traffic
is serviced in that wait path; an X event is fetched on a subsequent event
loop iteration.

`MapRequest` creates or finds the managed-client record, consults initial
state hints and desk/page visibility, then requests mapping and records
`MAP_PENDING`. `MapNotify` maps the frame under a server grab and completes
the pending state transition. `UnmapNotify` distinguishes manager-generated
events from client withdrawal, including the ICCCM synthetic-root case; it
may restore the client to the root and discard manager state. The source has
substantial historical comments on these ordering constraints.

`SetFocus()` also applies policy beyond the X input-focus request: it checks
current desk/page visibility, updates click-focus button grabs and recency
ordering, honors `WM_HINTS` input preference, and sends `WM_TAKE_FOCUS` when
requested by the client.

### OpenBSD process model

The main process owns the X connection and window management. It starts
`fvwm_exec` before locking its filesystem view with `unveil(2)`. The helper
receives `Exec` and `PipeRead` requests over `socketpair(2)` and `imsg(3)`,
then forks and executes commands. It runs as the same user and deliberately
has no `pledge(2)` or `unveil(2)` restrictions, so it is not a privilege
boundary. Modules are separate processes and use module-specific pledge and
unveil setup; `execve(2)` gives them a fresh unveil view because no
`execpromises` are installed. `xpmroot` applies its pledge policy only after
opening the X display.

## Comment changes

| Path | Status | Documentation added |
| --- | --- | --- |
| `fvwm/fvwm/events.c` | Partially documented | Dispatch context, unmanaged-window context, and X/module/helper wait-loop relationship |
| `fvwm/fvwm/focus.c` | Documented | Combined focus policy, visibility gate, ICCCM hints, and passive grabs |
| `fvwm/fvwm/fvwm.h` | Partially documented | Distinction between managed-window list and stacking ring |
| `fvwm/fvwm/module.h` | Documented | Native-word module packet layout and architecture-dependent framing |
| `fvwm/libs/Module.c` | Documented | Lifetime of `GetConfigLine()` storage; complete reads and writes across interruptions and partial transfers |
| `fvwm/libs/Picture.c` | Documented | Reference-count contract for cached pictures and X resources |

## Full-file review

| Paths | Language / responsibility | Status and notes |
| --- | --- | --- |
| `fvwm/libs/ClientMsg.c`, `ColorUtils.c`, `GetFont.c`, `Grab.c`, `ModParse.c`, `Module.c`, `Parse.c`, `Picture.c`, `Strings.c`, `System.c`, `XResource.c`, `debug.c`, `envvar.c`, `wild.c` | C library helpers | Already documented or had API comments sufficient for this pass; `Module.c` received a lifetime clarification |
| `fvwm/libs/ModParse.h`, `fvwmlib.h`, `lang-strings.h` | Library headers | Existing declarations and comments reviewed; no extra comments needed |
| `fvwm/utils/xpmroot.c` | X11 root-pixmap utility | Existing comments explain retained server resources and cleanup |
| `fvwm/fvwm/focus.c` | Core focus policy | Documented with an overview of policy stages |
| `fvwm/fvwm/module.h` | Core/module IPC declarations | Documented with packet framing and ABI constraints |
| `fvwm/docs/run_man2html.sh`, `txt2html.sh`, `fvwm/modules/FvwmSaveDesk/example/xinitrc`, `fvwm/utils/fvwmrc_convert`, `fvwm/utils/quantize_pixmaps` | POSIX Shell utilities | Corrected awk discovery/quoting in `fvwmrc_convert`; remaining scripts not executed |
| `fvwm/utils/BuildXLockMenu` | Perl utility | Corrected the interpreter path; script not executed |

## Static fixes made

The following findings were addressed by source changes in this pass. They
were reviewed statically; no FVWM binary or utility was built or run.

- `fvwm/fvwm/fvwm.c`: check the child display connection before querying its
  screen count; report failure from that child process.
- `fvwm/fvwm/module.c`: close the module channel on an oversized command
  instead of truncating the frame and interpreting its remaining payload as
  the continuation field.
- `fvwm/modules/FvwmBacker/FvwmBacker.c`: validate the `M_NEW_DESK` body,
  command table, and desk index before indexing; reject invalid and negative
  configuration desk numbers; use checked array allocation; route command
  writes through the shared complete-write implementation.
- `fvwm/libs/Module.c`: retry interrupted reads/writes and complete partial
  transfers before advancing to the next protocol field. A failed or
  incomplete transfer now stops the current packet operation.
- `fvwm/libs/envvar.c`: calculate expansion size using a private copy so the
  `const char *` input is not modified.
- `fvwm/utils/BuildXLockMenu`: use the OpenBSD Perl path in the actual shebang
  instead of the historical AFS path.
- `fvwm/utils/fvwmrc_convert`: use command-presence checks that remain false
  when awk is absent, and quote the selected awk executable and source path.

## Partial review

| Path | Subsystem inspected | Status / remaining work |
| --- | --- | --- |
| `fvwm/fvwm/fvwm.c` | Startup, root selection, event-mask setup, unveil ordering, restart path | Partial; read the complete file and all initialization/cleanup callers |
| `fvwm/fvwm/events.c` | Dispatch table, selected handlers, event wait loop | Partial; most handlers and event interactions remain to review |
| `fvwm/fvwm/fvwm.h` | `FvwmWindow` and shared declarations | Partial; remaining types and fields need review |
| `fvwm/fvwm/add_window.c` | Window-list insertion and stacking-ring insertion | Partial; full creation and teardown paths remain to review |
| `fvwm/fvwm/module.c` | Module packet writer and input command framing | Partial; full process and descriptor lifecycle remains to review |
| `fvwm/modules/FvwmBacker/FvwmBacker.c` | Desk message dispatch and configuration command indexing | Partial; full module lifecycle remains to review |
| `fvwm/modules/FvwmIconMan/fvwm.c` | Packet records and batched draw/configure behavior | Partial; full module review remains pending |

All other C files and headers in `fvwm/` are pending source review. The
remaining module implementations, core subsystems, and module-local headers
have not been audited exhaustively in this pass.

## Static findings (not fixed)

These are source-level observations, not runtime reproductions. They require
review in the target OpenBSD environment before any further code change:

- **Color resource accounting:** `ColorUtils.c` ignores `XAllocColor()`'s
  result and has no paired `XFreeColors()` in the helper. Callers and server
  resource lifetime need tracing before deciding whether this leaks references.
- **Destructive developer utility:** `quantize_pixmaps` uses fixed paths under
  `$HOME/tmp-icons` and removes or replaces files. It was not run.

The fixes above were not runtime-tested. Remaining color-resource concerns
were not changed because their ownership and lifetime are not established by
the inspected call paths.
