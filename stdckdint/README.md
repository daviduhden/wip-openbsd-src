# stdckdint — ISO C23 `<stdckdint.h>` for the OpenBSD base system

This component is a candidate contribution to the OpenBSD `src` tree.
It adds the C23 checked-integer header as a header-only public interface:
no library code, no exported symbols, and no object to compile.

It is kept separate from the FVWM vendored copy in `fvwm/compat/`.  The two
share the same audited algorithm but have independent integration, tests, and
deployment assumptions: this header becomes `<stdckdint.h>` in libc, while the
FVWM copy must keep working on an unmodified OpenBSD installation that never
receives it.

## Layout

Paths are relative to the OpenBSD `src` root, matching the convention used by
the sibling `ext4fs/` component (source-tree-relative files plus a patch for
the files that already exist upstream).

| Local path | OpenBSD `src` path |
| --- | --- |
| `include/stdckdint.h` | `include/stdckdint.h` (installed as `/usr/include/stdckdint.h`) |
| `regress/lib/libc/stdckdint/Makefile` | `regress/lib/libc/stdckdint/Makefile` |
| `regress/lib/libc/stdckdint/stdckdint.c` | `regress/lib/libc/stdckdint/stdckdint.c` |
| `stdckdint.patch` | full upstream-ready patch (new files + build hooks) |

## Installation integration

A public header is not installed merely by dropping it into `include/`.  The
`include/Makefile` `FILES` list is what `make includes` installs, so the patch
adds `stdckdint.h` to it:

```
	stdio.h stdio_ext.h stdlib.h ...          (unchanged context)
```

and `regress/lib/libc/Makefile` gains `stdckdint` in its `SUBDIR` so the test
is built by the regress suite.  A `.c` file is intentionally **not** added to
`lib/libc`, because the interface is macro-only and needs no runtime symbol.

## Applying the patch

From a checked-out OpenBSD `src` tree:

```
$ patch -p1 < /path/to/stdckdint/stdckdint.patch
$ cd regress/lib/libc/stdckdint && make && ./stdckdint
```

## Compiler compatibility and header precedence

Both base compilers already ship a resource header named `<stdckdint.h>`:

* clang >= 18 (`lib/Headers/stdckdint.h`);
* GCC >= 14 (`ginclude/stdckdint.h`).

Neither is a libc header, and neither replaces the need for one: they are
implementation headers that expect the C library to provide (or complete) the
interface.  Both are written to cooperate with a libc header:

* clang's header, when hosted, uses `#include_next <stdckdint.h>` and defines
  nothing itself if a system header is reachable after it;
* GCC's header defines the macros and then uses `#include_next` unless
  `_LIBC_STDCKDINT_H` is defined.

This header therefore:

* defines the macros with the `#ifndef ckd_*` guards, so whichever of the two
  compiler headers is reached first, the macros are defined exactly once and
  no redefinition diagnostic is produced;
* needs no `#include_next` of its own.

Include-path precedence does not change correctness but does change which
implementation a translation unit sees:

* if the compiler resource directory precedes `/usr/include` (common on
  clang), the resource header is found first and either defers here or the
  compiler defines the identical builtin expansion itself;
* if `/usr/include` precedes the resource directory (common on GCC), this
  header is found first and used directly.

Either way the observable macros and semantics are identical to the ISO C23
contract.  No conflict is introduced.

## Provenance and license

The macros expand to `__builtin_{add,sub,mul}_overflow`, whose contract is
exactly C23 7.20.1: infinite-precision evaluation of the operands, conversion
of the mathematical result to the type of `*result` (wrapping on overflow),
storage of the converted value, and a true result exactly when that conversion
is not value preserving.  The same formulation is used by the GCC and clang
resource headers and by the musl `<stdckdint.h>` proposal (Jens Gustedt,
`musl@`, 2023-05-31, message-id
`<323b1aaf4c7e31010ead4eae127d0d580f319ce6.1685535458.git.Jens.Gustedt@inria.fr>`).

No code is copied from GCC (GPL) or clang (Apache-2.0 with LLVM exceptions);
only the one-line builtin expansions, which are the only reasonable expression
of the builtin contract, are shared.  This file is ISC-licensed, matching the
OpenBSD base system's preferred permissive license.

## Test

`regress/lib/libc/stdckdint/stdckdint.c` checks, for `ckd_add`, `ckd_sub` and
`ckd_mul`:

* zero, one, `-1`, `INT_MIN`/`INT_MAX`, `LLONG_MIN`/`LLONG_MAX`,
  `SIZE_MAX`, `ULLONG_MAX`, and the values immediately beyond those
  boundaries;
* that both the overflow indication **and** the stored value match;
* mixed signed/unsigned operands, narrow destinations, and widening results;
* single evaluation of each operand and of the result pointer;
* the C23 feature-test macro and the `bool` type of the macro result.

On architectures with `__int128` the full boundary grid is additionally
replayed against an independent arbitrary-precision oracle.
