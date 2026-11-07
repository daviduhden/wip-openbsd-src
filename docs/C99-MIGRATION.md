# C99 migration

Standalone pax, FVWM and host developer tests select `-std=c99`. Host tests
also use `-pedantic-errors`; source-tree kernel Makefiles continue to leave
language flags to OpenBSD. Public symbols, persistent formats and sandbox
promises are preserved. POSIX, OpenBSD, X11 and LibreSSL interfaces are system
dependencies, not ISO language extensions.

## Inventory and conversion decisions

The repository contains imported OpenBSD libc and historical FVWM sources,
local pax changes, new ext4fs code, modern ISO header contributions and host
shims/tests. Recursive C/header and build metadata searches were used across
these trees. This is not a claim of a line-by-line audit of every imported
libc file or a complete native OpenBSD build.

| Area | Findings | Conversion or retained requirement |
| --- | --- | --- |
| pax | null pointer keyword, constexpr integers, empty initializers, attributes, layout assertion, checked multiplication | NULL, integer macros, `{ 0 }`, OpenBSD attributes/comments, negative-array typedef checks, positive multiplication division guard |
| FVWM | same syntax plus inferred locals and unnamed callback parameters | explicit types, named unused parameters, C99 booleans; see the file inventory below |
| ext4fs | C11 assertions and anonymous struct/union overlays | typedef assertions and named overlays with member aliases; packed representation retained |
| host tests | C11 generic type assertion, C23 compiler defaults | compiler type predicate and typedef assertion; C99 strict defaults |
| libc | imported compiler/platform extensions; deliberately implemented modern ISO interfaces | C99 host defaults and header inclusion matrix; modern API exceptions below |
| stdckdint | checked arithmetic compiler builtins; C11 type conformance test | safe builtin implementation and unevaluated compiler type predicate retained |

Integer macros preserve integer constant expressions used in array bounds,
case labels and static initializers. Unsigned constants preserve suffixes.
NULL in variadic pointer sentinels requires the appropriate explicit pointer
cast. Layout assertions remain compile-time checks, including on-disk ext4
structures; they are not replaced with runtime assertions.

## Explicit exceptions

Pure ISO C99 cannot express the existing arbitrary-type `_Atomic(T)` API or
`_Generic` dispatch while preserving their source interface and guarantees.
The imported libc headers, associated API conformance tests and upstream code
therefore retain required language/compiler extensions. Atomic tests default to C11 with explicit C17/C23 modes available; the
forced generic stdbit fallback runs C11 and C23. The header matrix retains
C11/C17/C23 and adds C99 inclusion checks. See `libc/README.md` and
`stdckdint/README.md`. No atomic operation was replaced with a plain access.

Checked integer macros retain Clang/GCC overflow builtins: these preserve
mixed signedness, single argument evaluation and defined stored overflow
results. A naive C99 arithmetic expression would lose those guarantees.
The independent host oracle retains `__int128` under `__extension__`; its
exact-result type check uses compiler type predicates. OpenBSD attributes
for packing, diagnostics and noreturn remain deliberate compiler extensions.
Neither a C23-named header nor an OS function alone implies a C23 syntax
requirement. Full strict ISO C99 portability of every imported source is not
established by this migration.

## Validation and remaining platform checks

Commands and observed results are recorded in the delivery report. The full
host suite requires X11 development headers, including `X11/Intrinsic.h`.
Native pax/FVWM builds, a matching OpenBSD kernel build for ext4fs, filesystem
mount tests and live X11 validation require the target platform. Host parser,
layout and arithmetic tests cannot establish those results.

Review style against <https://man.openbsd.org/style.9>. Preserve imported
licenses and avoid unrelated whole-file formatting. Agent instructions,
build policy validation and applicable skills must track these language
requirements; the agent knowledge validator does not prove runtime behavior.

## Observed checks in this session

Before the user restricted subsequent work to static analysis:

- `bmake -C libc/tests clean run`, with Clang 23.1.5 and GCC 16.2.1:
  six runtime suites, stdbit fallback and header matrices passed.
- C99 `-pedantic-errors` ASan/UBSan builds and execution of FVWM exec,
  pipebuf, checked arithmetic and ext4fs journal/inode tests passed.
- Eight checked-integer compile acceptance/rejection cases passed.
- An extracted production pax multiplication helper passed boundary cases
  with UBSan, including unchanged output on failure.
- The full `bmake -C tests run` stopped at missing `X11/Intrinsic.h`.
- A strict syntax check initially omitted Linux feature macros and failed
  on POSIX declarations; repeating with the Makefile's `-D_GNU_SOURCE` passed.
- Agent knowledge validator tests passed (51 checks).

After that restriction, review uses static inspection only. No further
project compilation or runtime testing is authorized in this session.
`git diff --check` is clean. The read-only agent knowledge validator passed
with AK004's manual relevance warning; the instruction changes were reviewed
and reflect the new C99 defaults and necessary API exceptions.
The build metadata validator was stopped after partial inspection; native
OpenBSD dry runs were unavailable and its complete result is not claimed.

## Modified paths

The following list records source, build, documentation and knowledge changes
from the migration. Later libc synchronization is recorded separately in
`libc/ISO-C-STATUS.md`. The workflow dispatch change is already committed
and is not part of this pending source inventory:

- `.agents/skills/openbsd-fvwm-development/SKILL.md`
- `.agents/skills/openbsd-pax-development/SKILL.md`
- `.agents/skills/openbsd-source-build/SKILL.md`
- `.agents/skills/openbsd-source-development/SKILL.md`
- `AGENTS.md`
- `Makefile`
- `README.md`
- `docs/C99-MIGRATION.md`
- `docs/libc-C99-INVENTORY.md`
- `ext4fs/README.md`
- `ext4fs/sys/ufs/ext4fs/ext4fs.h`
- `ext4fs/sys/ufs/ext4fs/ext4fs_dinode.h`
- `ext4fs/sys/ufs/ext4fs/ext4fs_journal.h`
- `fvwm/Makefile.inc`
- `fvwm/compat/stdckdint.h`
- `fvwm/fvwm/add_window.c`
- `fvwm/fvwm/bindings.c`
- `fvwm/fvwm/borders.c`
- `fvwm/fvwm/builtins.c`
- `fvwm/fvwm/colormaps.c`
- `fvwm/fvwm/colors.c`
- `fvwm/fvwm/complex.c`
- `fvwm/fvwm/decorations.c`
- `fvwm/fvwm/events.c`
- `fvwm/fvwm/exec.c`
- `fvwm/fvwm/focus.c`
- `fvwm/fvwm/functions.c`
- `fvwm/fvwm/fvwm.c`
- `fvwm/fvwm/fvwm.h`
- `fvwm/fvwm/fvwm_exec.c`
- `fvwm/fvwm/fvwm_sandbox.h`
- `fvwm/fvwm/icons.c`
- `fvwm/fvwm/menus.c`
- `fvwm/fvwm/menus.h`
- `fvwm/fvwm/misc.c`
- `fvwm/fvwm/misc.h`
- `fvwm/fvwm/modconf.c`
- `fvwm/fvwm/module.c`
- `fvwm/fvwm/module.h`
- `fvwm/fvwm/move.c`
- `fvwm/fvwm/pipebuf.c`
- `fvwm/fvwm/placement.c`
- `fvwm/fvwm/read.c`
- `fvwm/fvwm/resize.c`
- `fvwm/fvwm/screen.h`
- `fvwm/fvwm/style.c`
- `fvwm/fvwm/virtual.c`
- `fvwm/fvwm/windows.c`
- `fvwm/fvwm/xalloc.h`
- `fvwm/libs/GetFont.c`
- `fvwm/libs/ModParse.c`
- `fvwm/libs/ModParse.h`
- `fvwm/libs/Module.c`
- `fvwm/libs/Parse.c`
- `fvwm/libs/Picture.c`
- `fvwm/libs/Strings.c`
- `fvwm/libs/XResource.c`
- `fvwm/libs/envvar.c`
- `fvwm/libs/fvwmlib.h`
- `fvwm/libs/wild.c`
- `fvwm/modules/FvwmAuto/FvwmAuto.c`
- `fvwm/modules/FvwmBacker/FvwmBacker.c`
- `fvwm/modules/FvwmBacker/FvwmBacker.h`
- `fvwm/modules/FvwmBanner/FvwmBanner.c`
- `fvwm/modules/FvwmButtons/FvwmButtons.c`
- `fvwm/modules/FvwmButtons/button.c`
- `fvwm/modules/FvwmButtons/draw.c`
- `fvwm/modules/FvwmButtons/parse.c`
- `fvwm/modules/FvwmCpp/FvwmCpp.c`
- `fvwm/modules/FvwmCpp/FvwmCpp.h`
- `fvwm/modules/FvwmForm/FvwmForm.c`
- `fvwm/modules/FvwmIconBox/FvwmIconBox.c`
- `fvwm/modules/FvwmIconBox/FvwmIconBox.h`
- `fvwm/modules/FvwmIconBox/icons.c`
- `fvwm/modules/FvwmIconMan/FvwmIconMan.c`
- `fvwm/modules/FvwmIconMan/FvwmIconMan.h`
- `fvwm/modules/FvwmIconMan/debug.c`
- `fvwm/modules/FvwmIconMan/debug.h`
- `fvwm/modules/FvwmIconMan/functions.c`
- `fvwm/modules/FvwmIconMan/fvwm.c`
- `fvwm/modules/FvwmIconMan/globals.c`
- `fvwm/modules/FvwmIconMan/readconfig.c`
- `fvwm/modules/FvwmIconMan/winlist.c`
- `fvwm/modules/FvwmIconMan/x.c`
- `fvwm/modules/FvwmIconMan/xmanager.c`
- `fvwm/modules/FvwmIconMan/xmanager.h`
- `fvwm/modules/FvwmIdent/FvwmIdent.c`
- `fvwm/modules/FvwmIdent/FvwmIdent.h`
- `fvwm/modules/FvwmM4/FvwmM4.c`
- `fvwm/modules/FvwmM4/FvwmM4.h`
- `fvwm/modules/FvwmPager/FvwmPager.c`
- `fvwm/modules/FvwmPager/FvwmPager.h`
- `fvwm/modules/FvwmPager/x_pager.c`
- `fvwm/modules/FvwmRearrange/FvwmRearrange.c`
- `fvwm/modules/FvwmSave/FvwmSave.c`
- `fvwm/modules/FvwmSave/FvwmSave.h`
- `fvwm/modules/FvwmSaveDesk/FvwmSaveDesk.c`
- `fvwm/modules/FvwmSaveDesk/FvwmSaveDesk.h`
- `fvwm/modules/FvwmScroll/FvwmScroll.c`
- `fvwm/modules/FvwmScroll/FvwmScroll.h`
- `fvwm/modules/FvwmScroll/GrabWindow.c`
- `fvwm/modules/FvwmTalk/FvwmTalk.c`
- `fvwm/modules/FvwmTalk/FvwmTalk.h`
- `fvwm/modules/FvwmWinList/ButtonArray.c`
- `fvwm/modules/FvwmWinList/ButtonArray.h`
- `fvwm/modules/FvwmWinList/FvwmWinList.c`
- `fvwm/modules/FvwmWinList/FvwmWinList.h`
- `fvwm/modules/FvwmWinList/List.c`
- `fvwm/modules/Makefile.inc`
- `fvwm/utils/xpmroot.c`
- `libc/README.md`
- `libc/patch`
- `libc/regress/lib/libc/stdckdint/stdckdint.c`
- `libc/regress/lib/libc/time/time_conversion/timetest.c`
- `libc/tests/Makefile`
- `libc/tests/headers-compile.sh`
- `libc/tests/stdbit-fallback.sh`
- `pax/Makefile`
- `pax/ar_io.c`
- `pax/ar_subs.c`
- `pax/buf_subs.c`
- `pax/cpio.c`
- `pax/file_subs.c`
- `pax/ftree.c`
- `pax/gen_subs.c`
- `pax/getoldopt.c`
- `pax/options.c`
- `pax/pat_rep.c`
- `pax/pax.c`
- `pax/pax.h`
- `pax/sel_subs.c`
- `pax/tables.c`
- `pax/tar.c`
- `pax/tar.h`
- `pax/tty_subs.c`
- `stdckdint/README.md`
- `stdckdint/regress/lib/libc/stdckdint/stdckdint.c`
- `stdckdint/stdckdint.patch`
- `tests/Makefile`
- `tests/stdckdint-compile-fail.sh`
- `tests/stdckdint-tests.c`
- `validate-make.sh`

## Static follow-up

The complete lexical sweep read 2,323 tracked C/header files: libc 2,147,
FVWM 124, host tests 21, pax 19, ext4fs 10 and stdckdint 2. Keyword
candidates for post-C99 syntax were confined to libc. This classification
does not certify compiler extensions as ISO C99.

The follow-up session invoked no compiler, build, project runtime test or
compile-only test. Shell syntax inspection (`sh -n`) was used without running
the checked-integer compile script. Recursive lexical scans of the 174
pax/FVWM/ext4fs/host-test C/header files found no remaining executable modern
syntax in the categories searched; comments and string literals were excluded.
Directed review checked boolean header dependencies, named callback parameters,
variadic pointer sentinels, constant consumers and inode overlay aliases.
This evidence does not replace preprocessing, target builds or runtime tests.

The obsolete `FVWM_STDCKDINT_FORCE_FALLBACK` test option was removed: the
vendored C99 shim now always selects the checked overflow builtins. Its
include-path identity check remains. A real empty initializer in the imported
time-conversion regression was replaced by `{ 0 }`, preserving zero
initialization; its libc patch hunk was updated. The stdckdint contribution's exact-result
type assertion now uses an unevaluated compiler predicate in C99 mode, with
its exported patch synchronized. Remaining imported and modern libc interface
requirements are inventoried in `docs/libc-C99-INVENTORY.md`.
