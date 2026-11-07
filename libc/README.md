# libc -- imported OpenBSD libc and ISO C11/C17/C23 headers

This component imports the OpenBSD `-current` C library source and adds
the six ISO C standard headers that OpenBSD libc does not currently
install, implemented from scratch on top of the compiler and the existing
pthread implementation:

| Header | Standard | Status |
| --- | --- | --- |
| `stdalign.h`   | C11 (deprecated C23) | implemented |
| `stdnoreturn.h`| C11 (deprecated C23) | implemented |
| `stdatomic.h`  | C11 / C17 / C23 | macro APIs implemented; direct fallback expressions remain partial |
| `threads.h`    | C11 / C17 / C23 | implemented |
| `stdbit.h`     | C23 | implemented, including 128-bit types |
| `stdckdint.h`  | C23 | implemented |

C17 introduced no new headers.

The upstream revision and import method are recorded in [UPSTREAM](UPSTREAM).
The source-relative delta against that exact revision is maintained in
[patch](patch), which excludes host-only tests and project reports.  The
detailed audit, conformance matrix and test results are in
[REPORT.md](REPORT.md).

For future synchronization, follow [SYNC.md](SYNC.md): pin the effective
last synchronized revision as BASE, save the complete LOCAL worktree including
uncommitted changes, and inspect the intervening history to pinned NEW.
Classify and merge local deltas semantically. Do not remove an interface until
upstream equivalence has been demonstrated. Regenerate and replay `patch`
against pristine NEW, then compare bytes, modes, links and path sets before
advancing `UPSTREAM`. Current results and unexecuted checks are recorded in
[SYNC-2026-11-07.md](SYNC-2026-11-07.md).

## Layout

The tree mirrors the OpenBSD source layout with the leading `src/`
dropped, the convention already used by the sibling `ext4fs/` and
`stdckdint/` components:

| This tree | OpenBSD `src` path |
| --- | --- |
| `include/` | `include/` |
| `lib/libc/` | `lib/libc/` |
| `lib/libpthread/` | `lib/libpthread/` (manual pages) |
| `lib/librthread/` | `lib/librthread/` (`LIB=pthread`) |
| `sys/sys/` | `sys/sys/` (dependency of the public headers) |
| `sys/arch/<arch>/include/` | `sys/arch/<arch>/include/` (dependency) |
| `regress/lib/libc/` | `regress/lib/libc/` |
| `tests/` | host-side tests, not part of OpenBSD |

OpenBSD splits pthread support between `lib/libc/thread/` and
`lib/librthread/`; the latter builds `libpthread`. `lib/libpthread/`
contains the manuals. The C11 `<threads.h>` functions are built in
`lib/librthread/thrd.c` and require `-pthread`, because thread lifecycle
and mutex-attribute operations are supplied by that library. No new library
is created. Atomics and typed bit operations remain in libc.

## What was added or changed

New public headers (`include/`):

* `stdalign.h`, `stdnoreturn.h`, `stdatomic.h`, `threads.h`, `stdbit.h`,
  `stdckdint.h`.

New library sources:

* `lib/librthread/thrd.c` -- the C11 `<threads.h>` entry points.
* `lib/libc/thread/atomic.c` -- the fence and `atomic_flag` entry points.
* `lib/libc/thread/atomic_fallback.c` -- the lock-based support for atomic
  values routed through the macro fallback; direct compiler operations
  require separate runtime/ABI review.
* `lib/libc/gen/stdbit.c` -- the C23 `<stdbit.h>` typed functions.

New hidden-namespace header (`lib/libc/hidden/`): `stdatomic.h`.
The threads wrapper uses librthread's private pthread namespace.

Manual pages: `stdalign.3`, `stdnoreturn.3`, `stdatomic.3`, `stdbit.3`,
`stdckdint.3` (in `lib/libc/gen/`) and `threads.3` (in
`lib/libpthread/man/`).

Regress tests (`regress/lib/libc/`): `stdalign/`, `stdnoreturn/`,
`stdatomic/`, `stdbit/`, `stdckdint/`, `threads/`, `login_cap/` and
`iso-c-headers/`.

Modified build files: `include/Makefile`, `lib/libc/thread/Makefile.inc`,
`lib/libc/gen/Makefile.inc`, `lib/libc/Symbols.list`,
`lib/libpthread/man/Makefile.inc`, `lib/librthread/Makefile`,
`lib/librthread/Symbols.map`, and `regress/lib/libc/Makefile`.

Architecture dependencies: the `machine/` headers of `amd64`, `i386`,
`arm64`, `armv7`, `riscv64`, `powerpc64`, `sparc64` and the shared `arm`
headers were copied unchanged so the whole tree can be compiled for each
of the supported architectures.  These are byte-for-byte upstream.

## Header details

### `stdalign.h` (C11)

Defines `alignas`, `alignof`, `__alignas_is_defined` and
`__alignof_is_defined`.  In C23 `alignas` and `alignof` are keywords and
the header defines nothing; in C++ the two names are keywords and only
the `_is_defined` markers are kept.

### `stdnoreturn.h` (C11)

Defines `noreturn` as `_Noreturn`.  Defines nothing in C++, where the
facility is the `[[noreturn]]` attribute.

### `stdatomic.h` (C11/C17/C23)

Provides the standard atomic types, `atomic_flag`, `memory_order`, the
`ATOMIC_*_LOCK_FREE` constants, `kill_dependency`, `atomic_is_lock_free`
and the whole set of type-generic operations, plus the `_explicit` forms
and `ATOMIC_VAR_INIT` for C17 and earlier.

* The generic operations are macros over the compiler's atomic builtins.
  clang's `__c11_atomic_*` builtins are used when available; GCC's
  `__atomic_*` builtins with statement-expression temporaries otherwise.
  Each argument is evaluated once. Atomic integer aliases follow the public
  library typedefs, including OpenBSD fast types, rather than compiler guesses.
* `atomic_fetch_add`/`atomic_fetch_sub` on an atomic pointer object scale
  the operand by the size of the pointed-to type, as ISO C requires, on
  both compilers. GCC scales a captured unsigned pointer-width displacement
  before its raw builtin; it does not introduce ordinary C pointer arithmetic
  outside the original array.
* **Non-lock-free types are supported.**  Every operation first tests
  `__atomic_always_lock_free()` (a compile-time constant) and, when it is
  false, calls a small lock-based fallback in libc
  (`__atomic_fallback_*`) instead of letting the compiler emit a call to
  its atomic runtime for these macro calls. Direct compiler-generated
  atomic loads/stores/compound expressions do not use this fallback ABI
  and must not be mixed with macro accesses routed through it. They may
  require an unavailable external runtime, including where atomic storage
  differs from payload size. Full language-level atomic support is partial.
  The fallback uses a per-address spinlock with
  sequential-consistency ordering, which is stronger than any order the
  standard requests.
* `atomic_char8_t` and `ATOMIC_CHAR8_T_LOCK_FREE` are provided in C23;
  `ATOMIC_VAR_INIT` is removed in C23.
* `atomic_thread_fence`, `atomic_signal_fence`,
  `atomic_flag_test_and_set[_explicit]` and `atomic_flag_clear[_explicit]`
  are real libc symbols so that their address can be taken.

C++ inclusion is supported when the compiler implements `_Atomic(T)`
(clang); the GCC C++ front end is diagnosed with an `#error`.

### `threads.h` (C11/C17/C23)

A thin, standardised skin over the pthread implementation in libpthread
and libc, with the C11 entry points supplied by libpthread:

* `thrd_t`, `mtx_t`, `cnd_t`, `tss_t` and `once_flag` are the
  corresponding pthread types; `thrd_create` uses a trampoline because a
  C11 start routine returns `int` while a pthread start routine returns
  `void *`.  The start block is freed before the routine runs, so a
  routine that calls `thrd_exit` or is cancelled cannot leak it.
* Return values are the ISO C `thrd_*` codes; `ENOMEM` and `EAGAIN` map
  to `thrd_nomem`.
* `mtx_plain`, `mtx_recursive` and `mtx_timed` are bit flags, so
  `mtx_recursive | mtx_timed` works.
* The base clang predefines `__STDC_NO_THREADS__` when targeting OpenBSD
  because the base system historically lacked `<threads.h>`.  This header
  provides the interface, so it withdraws the stale macro (`#undef`) to
  keep the translation unit consistent.  The compiler's target
  description should still be updated upstream.
* `thread_local` is defined before C23 only.

### `stdbit.h` (C23)

Provides the type-generic interface and all typed functions for
`unsigned char` ... `unsigned long long`, the endianness constants, and the
bit counting/searching/power-of-two operations.

* The typed functions are real libc entry points in `gen/stdbit.c`, so
  they have external linkage and a stable address.
* The type-generic interface uses the compiler's `__builtin_stdc_*`
  primitives when present and otherwise dispatches with `_Generic` to the
  typed functions; on compilers with a 128-bit integer type, the 128-bit
  branches are added, so extended types are handled exactly either way.
* In C++ the same interface is provided with overloaded inline functions.
* No shift is out of range and clz/ctz are never applied to zero; the
  width is always passed explicitly, so the behaviour does not depend on
  the machine word size or on endianness.  A portable bit-by-bit fallback
  is provided and tested.
* The C29 `stdc_rotate_*` additions are not part of C23 and are not
  provided.

### `stdckdint.h` (C23)

`ckd_add`, `ckd_sub` and `ckd_mul`, implemented as macros over the
compiler's overflow builtins.  Each operand is evaluated once, the exact
mathematical result is converted to the destination type and stored, and
the macro yields `true` exactly when that conversion is not value
preserving.  The result type is `_Bool` in C and `bool` in C++.  No
library code and no symbols are required.

## Architecture support

The six headers are architecture-neutral.  Compilation for the OpenBSD
target was checked for:

| Architecture | Endianness | Result |
| --- | --- | --- |
| amd64   | little | compiles |
| i386    | little | compiles |
| arm64   | little | compiles |
| armv7   | little | compiles |
| riscv64 | little | compiles |
| powerpc64 | big  | compiles |
| sparc64 | big    | compiles |

`__STDC_ENDIAN_NATIVE__` was asserted to be big-endian on `powerpc64` and
`sparc64` and little-endian on the rest.  These are cross-compilation
checks (`clang --target=<arch>-unknown-openbsd -fsyntax-only`), not
executed binaries.

## Building and integrating

`include/Makefile` installs the headers. `lib/libc/thread/Makefile.inc`
and `lib/libc/gen/Makefile.inc` build atomics and bit operations, exported
through libc's `Symbols.list` with local version 104.1. C11 threads are
built by `lib/librthread/Makefile` and exported through `Symbols.map`,
with local libpthread version 28.2 (upstream 28.1). This corrects the former
local placement in libc, which left lifecycle dependencies unresolved.
Historical OpenBSD pthread symbols and public structure layouts are retained;
consumers of the local C11 thread extension must link with `-pthread`.
The old local libc thread exports and unused hidden header are removed.

## Testing

### OpenBSD regress

`regress/lib/libc/{stdalign,stdnoreturn,stdatomic,stdbit,stdckdint,threads}`
contain functional tests, and `regress/lib/libc/iso-c-headers` compiles
each header alone and all together, in both orders, under `-std=c11`,
`-std=c17` and `-std=c23` with `-Wall -Wextra -Werror -Wpedantic`, plus a
C++ inclusion check.  Run with `make && ./<name>` in each directory.

### Host-side tests

`tests/` runs on Linux/macOS with the host C library standing in for
OpenBSD where the interface is identical; `make -C tests run` builds and
runs:

* `stdbit-tests` -- the full matrix of bit operations, including 128-bit
  types, plus a check that the typed functions have a stable address;
* `stdatomic-tests` -- contention, acquire/release publication, a
  `seq_cst` store-buffering litmus test, compare-exchange, aggregate and
  pointer atomics;
* `stdatomic-fallback-tests` -- non-lock-free aggregates and 16-byte
  integers through the lock-based fallback, under contention;
* `threads-tests` -- creation/joining, plain/recursive/timed mutexes,
  condition variables, `call_once` from several threads, TSS,
  `thrd_exit`, detached threads and repeated create/join stress;
* `stdckdint-tests` and `stdckdint-oracle-tests` -- boundary cases and a
  randomised property test checked against an independent `__int128`
  model;
* `stdbit-fallback.sh` -- the portable (non-builtin) code paths;
* `headers-compile.sh` -- independent and joint inclusion, provenance, the
  C11/C17/C23 matrix and a C++ check.

The earlier audit recorded passes with clang 23 and GCC 16 under C11, C17
and C23 with
`-Wall -Wextra -Werror -Wpedantic`.  See REPORT.md for the exact results.

### Not executed here

The OpenBSD regress suite and the actual libc build require an OpenBSD
machine.  The earlier revision was compile-checked against its imported headers for all
of the supported architectures but not built or run on OpenBSD from this
tree; no OpenBSD run is claimed.

## Compatibility summary

| | C89/C90 | C95 | C99 | C11 | C17 | C23 | C++ |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `stdalign.h` | n/a | n/a | n/a | yes | yes | empty header | markers only |
| `stdnoreturn.h` | n/a | n/a | n/a | yes | yes | yes | empty header |
| `stdatomic.h` | -- | -- | -- | partial | partial | partial | clang inclusion |
| `threads.h` | -- | -- | -- | yes | yes | yes | headers only |
| `stdbit.h` | -- | -- | -- | -- | -- | yes | overloads |
| `stdckdint.h` | -- | -- | -- | -- | -- | yes | yes |

## Remaining limitations

* The base clang predefines `__STDC_NO_THREADS__` for OpenBSD; the header
  withdraws it, but the compiler target description should be fixed
  upstream.
* GCC's resource `stdatomic.h` does not `#include_next` and can shadow an
  installed header in ordinary user builds. The native source builds and
  ISO regressions now explicitly select this tree's public source include
  path before compiler resources; guard checks reject wrong headers.
* Direct compiler-generated atomic operations on fallback objects are not
  integrated with the macro helper ABI; do not mix the two. Native runtime
  lowering and linkage need further work before full atomics conformance.
* Raw bit-field arguments to stdbit/stdckdint generic interfaces are not
  guaranteed; use an explicit eligible integer conversion. WG14 issue1077
  remains open. Matching-width unsigned _BitInt operands are covered in
  C23 stdbit dispatch; other widths are outside that contract.

### C99 implementation checks and language API exceptions

The host runtime tests default to `-std=c99 -pedantic-errors`, with strict
warnings treated as errors. This checks the typed bit functions, checked
integer arithmetic, and pthread-backed thread wrappers without requiring a
C23 translation unit. The host `DEF_WEAK` replacement gives each expansion
its own typedef name: repeated typedef declarations require C11 even when
the declared type is identical. The standalone header inclusion matrix also
checks C99; inclusion alone does not prove that every optional macro can be
expanded in that language mode.

Tests that explicitly declare `_Atomic(T)` remain C11 tests. The bit test
selects C23 to exercise _BitInt; its forced fallback runs both C11 and C23. These are deliberate language interface exceptions, not ordinary
application dependencies. Replacing arbitrary atomic types with ordinary
objects would change their alignment, compiler semantics and possibly ABI;
replacing atomics with volatile accesses would lose synchronization.
Likewise, C99 has no portable expression that dispatches on an arbitrary
operand type while preserving single evaluation and the exact result type.
The modern header API tests and C11/C17/C23 matrix therefore remain present.

The compiler overflow builtins in `stdckdint.h` are another explicit
extension, available in C99 mode on the supported compilers. They preserve
mixed-width, mixed-sign, single-evaluation arithmetic without undefined
signed overflow. The C23 name of the header is not a C23 syntax dependency.
The imported upstream tree also contains OpenBSD compiler attributes,
assembly, architecture types, and kernel header extensions. Those are
platform contracts; a host test run does not establish strict ISO C99
conformance of the complete imported libc or its kernel dependencies.

The checked-integer regressions retain exact result-type checks through an
unevaluated compiler type predicate and a negative-array typedef, including
in the exported patches. The imported time-conversion regression uses C99
zero initializers. The remaining file-granular lexical candidates and audit
limitations are recorded in `../docs/libc-C99-INVENTORY.md`.


The latest interface-level audit and per-standard limitations are recorded in
[ISO-C-STATUS.md](ISO-C-STATUS.md). New regressions described there have not
been compiled or executed in this session. Native build include paths select
the public source headers after private wrappers, ahead of compiler resources.

The complete workspace delta is [patch](patch). The alternative
[iso-c.patch](iso-c.patch) contains the focused six-header contribution,
excluding prior login/glob/time-fixture changes. Both target the recorded
upstream revision; they are alternative snapshots with overlapping changes.
