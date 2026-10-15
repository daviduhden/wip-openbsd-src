# Phase II technical report -- ISO C11/C17/C23 headers for OpenBSD libc

This report accompanies the implementation in this directory.  It records
what was audited, what defects were found and fixed, what was verified and
what was not.  The validation status of every claim is stated explicitly;
no OpenBSD execution is claimed.

## 1. Audit summary

The Phase I tree was re-read in full: the six headers, the libc sources
(`thrd.c`, `atomic.c`), the hidden-namespace headers, the build
integration, the manual pages, the regress tests, the host tests and the
patch.  Each Phase I claim was checked against the code and, where
possible, against the compiler in a controlled environment built from the
imported OpenBSD headers.

The audit was performed with:

* clang 23 and GCC 16 on Linux, C11/C17/C23, `-Wall -Wextra -Werror
  -Wpedantic`;
* a controlled include environment (`-nostdinc` with the imported
  `include/`, `sys/sys/` and the per-architecture `machine/` headers);
* `clang --target=<arch>-unknown-openbsd -fsyntax-only` for seven
  architectures;
* clang's static analyzer, GCC `-fanalyzer`, AddressSanitizer and
  UndefinedBehaviorSanitizer;
* differential testing of `stdbit.h` and `stdckdint.h` against
  independent models.

## 2. Defects found

| # | Severity | Area | Defect | Resolution |
| - | --- | --- | --- | --- |
| 1 | critical | `threads.h` | `#error` on `__STDC_NO_THREADS__`, which the base clang predefines for the OpenBSD target, made the header unusable on the target system | the header withdraws the stale macro |
| 2 | critical | `stdatomic.h` (GCC) | pointer `atomic_fetch_add/sub` did not scale the operand by the pointed-to size | detected with `__builtin_classify_type` and served by a compare-exchange loop |
| 3 | high | `stdbit.h` | typed functions were `static inline` (internal linkage); the standard declares them as functions | moved to `gen/stdbit.c` as real libc symbols |
| 4 | high | `stdatomic.h` | non-lock-free types needed the compiler's atomic runtime (libatomic), absent from base | lock-based fallback in libc; no external runtime |
| 5 | medium | `thrd.c` | `pthread_create` returning `EAGAIN` was reported as `thrd_error` | `EAGAIN`/`ENOMEM` now map to `thrd_nomem` |
| 6 | low | `thrd.c` | dead store in `mtx_init` (static analyzer) | rewritten |
| 7 | low | `stdbit.h` | defined the compiler's `__has_builtin` | private `__stdc_has_builtin` |
| 8 | medium | tree | only `amd64` machine headers were imported | seven architectures imported and cross-checked |
| 9 | low | docs | limitations undocumented | reported here and in README |
| 10 | high (found while fixing 4) | `stdatomic.h` | the fallback operand temp was not converted to the object's value type, so a narrow operand was read as an object-sized value | operand is converted with an explicit cast to the object value type |
| 11 | medium (regression, fixed) | `stdatomic.h` | the first fallback rewrite passed a pointer where clang's CAS builtin takes the value by value | corrected |
| 12 | medium | `stdbit.h` | the `_Generic` fallback truncated extended types wider than `unsigned long long` | 128-bit branches added to the fallback |
| 13 | low | `stdbit.h`, `stdckdint.h` | not usable from C++ | C++ overloads for `<stdbit.h>` and `bool` for `<stdckdint.h>` |

No defect was found in `stdalign.h` or `stdnoreturn.h`; their semantics
were verified against the standard.

## 3. Fixes applied

1. `threads.h` withdraws a stale `__STDC_NO_THREADS__` and provides the
   interface regardless.
2. GCC-path pointer `atomic_fetch_add/sub` scale by the pointed-to size.
3. `stdbit.h` typed functions have external linkage and real definitions.
4. `lib/libc/thread/atomic_fallback.c` provides a lock-based fallback used
   whenever `__atomic_always_lock_free()` is false, so non-lock-free
   aggregates of any size and 128-bit integers work without libatomic.
   The constant test is folded away and the compiler's runtime is never
   referenced.
5. `EAGAIN` maps to `thrd_nomem`.
6. The `mtx_init` dead store was removed.
7. `stdbit.h` uses a private builtin probe.
8. The `i386`, `arm`, `arm64`, `armv7`, `riscv64`, `powerpc64` and
   `sparc64` machine headers were imported and the headers cross-compiled.
9. Documentation and manual pages updated; this report added.
10. The fallback operand is converted to the object's value type.
11. The clang CAS macros pass the desired value by value.
12. The `_Generic` fallback handles 128-bit integer types.
13. `<stdbit.h>` provides C++ overloads and `<stdckdint.h>` uses `bool`
    in C++.

## 4. ISO C conformance matrix

`R` = revision that introduces the interface.  Status: **C** complete,
**P** partial, **N/A** not applicable.

### `<stdalign.h>` (C11)

| Interface | R | Requirement | Implementation | Test | Status |
| --- | --- | --- | --- | --- | --- |
| `alignas` / `alignof` | C11 | expand to `_Alignas`/`_Alignof` | macro pre-C23, keyword C23 | regress `stdalign` | C |
| `__alignas_is_defined` / `__alignof_is_defined` | C11 | `1` | defined pre-C23 | regress `stdalign` | C |

### `<stdnoreturn.h>` (C11)

| Interface | R | Requirement | Implementation | Test | Status |
| --- | --- | --- | --- | --- | --- |
| `noreturn` | C11 | expands to `_Noreturn` | macro; nothing in C++ | regress `stdnoreturn` | C |

### `<stdatomic.h>` (C11, C17, C23)

| Interface | R | Requirement | Implementation | Limitation | Test | Status |
| --- | --- | --- | --- | --- | --- | --- |
| `atomic_bool` ... `atomic_uintmax_t` | C11 | `_Atomic` typedefs | `_Atomic(E)` over the compiler type macros | none | host + regress | C |
| `atomic_char8_t` | C23 | `_Atomic char8_t` | guarded on C23 | none | build | C |
| `memory_order` + 6 values | C11 | distinct enumeration | the compiler `__ATOMIC_*` values | none | host | C |
| `ATOMIC_*_LOCK_FREE` | C11, `CHAR8_T` C23 | the standard constants | `__GCC_ATOMIC_*_LOCK_FREE` | none | host | C |
| `ATOMIC_VAR_INIT` | C11/C17 | initializer | `(value)`, removed in C23 | none | regress | C |
| `atomic_init` | C11 | relaxed store | macro | none | host | C |
| `atomic_load`/`atomic_store` | C11 | value / void, all orders | builtins, or the fallback | none | host | C |
| `atomic_exchange` | C11 | old value | builtins, or the fallback | none | host | C |
| `atomic_compare_exchange_{strong,weak}` | C11 | update expected on failure | builtins, or the fallback | failure order restricted by the compiler | host + regress | C |
| `atomic_fetch_{add,sub}` | C11 | integer and pointer arithmetic | builtins; pointer scaling fixed; fallback | none | host + regress | C |
| `atomic_fetch_{or,xor,and}` | C11 | bitwise RMW | builtins, or the fallback | integer types only (standard) | host | C |
| `atomic_flag` + `ATOMIC_FLAG_INIT` | C11 | lock-free struct, static init | struct with atomic member, `{0}` | none | host + regress | C |
| `atomic_flag_test_and_set` / `_clear` | C11 | `_Bool` / void | real symbols + macros | none | host | C |
| `atomic_thread_fence` / `atomic_signal_fence` | C11 | fences | real symbols + builtin macros | none | host | C |
| `atomic_is_lock_free` | C11 | compile-time, per object | `__atomic_always_lock_free` | none | host | C |
| `kill_dependency` | C11 | identity | `(y)` | none | host | C |
| non-lock-free types | C11 | require a lock-based runtime | libc fallback (`__atomic_fallback_*`) | none | fallback + regress | C |

### `<threads.h>` (C11, C17, C23)

| Interface | R | Requirement | Implementation | Test | Status |
| --- | --- | --- | --- | --- | --- |
| `thrd_t`, `thrd_start_t` | C11 | object type, start fn | pthread types | host + regress | C |
| `thrd_create` | C11 | `thrd_nomem` on exhaustion | trampoline over `pthread_create` | host + regress | C |
| `thrd_join`, `thrd_detach` | C11 | result/error mapping | `pthread_join`/`pthread_detach` | host + regress | C |
| `thrd_current`, `thrd_equal` | C11 | identity | `pthread_self`/`pthread_equal` | host + regress | C |
| `thrd_sleep` | C11 | 0/-1/-2, remaining | `nanosleep` | host + regress | C |
| `thrd_yield` | C11 | void | `sched_yield` | host | C |
| `thrd_exit` | C11 | noreturn | `pthread_exit`, value via `intptr_t` | host + regress | C |
| `mtx_t` + `mtx_{plain,recursive,timed}` | C11 | bit flags | pthread mutex, bit flags | host + regress | C |
| `mtx_init`/`lock`/`unlock`/`destroy` | C11 | codes / void | pthread equivalents | host + regress | C |
| `mtx_trylock` | C11 | `thrd_busy` | `EBUSY` | host + regress | C |
| `mtx_timedlock` | C11 | `thrd_timedout` | `pthread_mutex_timedlock` | host + regress | C |
| `cnd_t` + condition variable | C11 | wait/timedwait/signal | pthread cond (CLOCK_REALTIME) | host + regress | C |
| `tss_t` + TSS | C11 | storage and destructor | pthread key | host + regress | C |
| `TSS_DTOR_ITERATIONS` | C11 | constant | `PTHREAD_DESTRUCTOR_ITERATIONS` | build | C |
| `ONCE_FLAG_INIT`, `once_flag`, `call_once` | C11 | exact-once | `pthread_once` | host + regress | C |
| `thread_local` | C11 | `_Thread_local` | macro pre-C23 | build | C |

### `<stdbit.h>` (C23)

| Interface | R | Requirement | Implementation | Test | Status |
| --- | --- | --- | --- | --- | --- |
| `__STDC_ENDIAN_{LITTLE,BIG,NATIVE}__` | C23 | distinct, native one of them | 1234/4321 from the compiler byte order | host + cross | C |
| `stdc_leading_zeros`/`leading_ones` | C23 | generic + typed | real functions; generic builtin/`_Generic` incl. 128-bit | host + regress | C |
| `stdc_trailing_zeros`/`trailing_ones` | C23 | generic + typed | as above | host + regress | C |
| `stdc_first_{leading,trailing}_{zero,one}` | C23 | one-based, 0 if none | as above | host + regress | C |
| `stdc_count_{zeros,ones}` | C23 | generic + typed | as above | host + regress | C |
| `stdc_has_single_bit` | C23 | `bool` | as above | host + regress | C |
| `stdc_bit_width` | C23 | generic + typed | as above | host + regress | C |
| `stdc_bit_floor`/`stdc_bit_ceil` | C23 | returns argument type | as above | host + regress | C |
| `stdc_rotate_left`/`right` | C29 | -- | not provided | -- | N/A |

### `<stdckdint.h>` (C23)

| Interface | R | Requirement | Implementation | Test | Status |
| --- | --- | --- | --- | --- | --- |
| `ckd_add`/`ckd_sub`/`ckd_mul` | C23 | exact result, wrap, `bool` | `__builtin_{add,sub,mul}_overflow` | oracle + regress | C |
| `__STDC_VERSION_STDCKDINT_H__` | C23 | `202311L` | defined | build | C |

## 5. Clang and GCC compatibility

| Header | clang 23 | GCC 16 | Notes |
| --- | --- | --- | --- |
| `stdalign.h` | yes | yes | |
| `stdnoreturn.h` | yes | yes | |
| `stdatomic.h` | yes | yes | different builtins; pointer arithmetic and non-lock-free types handled on both |
| `threads.h` | yes | yes | OpenBSD target withdraws `__STDC_NO_THREADS__` |
| `stdbit.h` | yes | yes | builtins and the fallback; 128-bit types; C++ overloads |
| `stdckdint.h` | yes | yes | `_Bool` in C, `bool` in C++ |

Both compilers were run with `-Wall -Wextra -Werror -Wpedantic` under
`-std=c11`, `-std=c17` and `-std=c23`; the libc sources were compiled under
the default GNU mode with the real libc flags; C++ was checked with
clang++.

### Header selection versus the compiler resource headers

clang searches its resource headers before `/usr/include` and defers to
this tree (`stdatomic.h` and `stdckdint.h` `#include_next`; clang has no
`stdbit.h`; its `stdalign.h`/`stdnoreturn.h` are equivalents).  GCC's
resource `stdckdint.h` also `#include_next`s, but GCC's `stdatomic.h` does
**not**, so GCC builds use the compiler's header.  This is a compiler-side
property that libc cannot change; the base compiler (clang) uses this
implementation.

## 6. Architecture compatibility

Cross-compilation (`clang --target=<arch>-unknown-openbsd -fsyntax-only`)
of a translation unit that includes all six headers:

| Architecture | Pointer width | Endianness | All six headers |
| --- | --- | --- | --- |
| amd64 | 64 | little | compiles |
| i386 | 32 | little | compiles |
| arm64 | 64 | little | compiles |
| armv7 | 32 | little | compiles |
| riscv64 | 64 | little | compiles |
| powerpc64 | 64 | big | compiles |
| sparc64 | 64 | big | compiles |

`__STDC_ENDIAN_NATIVE__` was asserted (`_Static_assert`) to be `BIG` on
`powerpc64`/`sparc64` and `LITTLE` on the others.  No header has an
x86-specific assumption.

## 7. Compilation results

* Six headers: independent and joint inclusion, both orders, C11/C17/C23,
  clang and GCC, `-Wall -Wextra -Werror -Wpedantic`, and C++ inclusion:
  clean.
* `stdbit.c`, `thrd.c`, `atomic.c`, `atomic_fallback.c`: compiled in the
  controlled OpenBSD include environment with the libc flags
  (`-Werror=deprecated-declarations`, `-include namespace.h`,
  `-D__LIBC__`): clean.
* The regression tests: compiled against the imported headers under
  C11/C17/C23 with clang and GCC: clean.

## 8. Execution results

Run on Linux with the host C library standing in for OpenBSD.  Seven
binaries plus two shell tests:

| Test | What it runs | Result |
| --- | --- | --- |
| `stdbit-tests` (+ link helper) | full operation matrix incl. 128-bit types, cross-TU address equality | pass |
| `stdatomic-tests` | contention, acquire/release publication, `seq_cst` store-buffering litmus, compare-exchange, aggregate and pointer atomics | pass |
| `stdatomic-fallback-tests` | 16/24/40-byte aggregates and 16-byte integers through the lock fallback, under contention | pass |
| `threads-tests` | plain/recursive/timed mutexes, condition wait/timedwait, `call_once` from two threads, TSS, `thrd_exit`, detached threads, 2000 create/join iterations | pass |
| `stdckdint-tests` | boundaries, mixed widths, single evaluation | pass |
| `stdckdint-oracle-tests` | 16 combinations x 50000 randomised cases against an `__int128` oracle | pass |
| `stdbit-fallback.sh` | portable (non-builtin) code paths, incl. 128-bit | pass |
| `headers-compile.sh` | independent/joint inclusion, provenance, C++ | pass |

The full matrix was run with clang 23 and GCC 16 under `-std=c11`,
`-std=c17` and `-std=c23`.  A C++ program linking against the six headers
(and the `<stdbit.h>` overloads) was compiled and run with clang++.

## 9. Sanitizers and static analysis

* AddressSanitizer + UndefinedBehaviorSanitizer, clang 23,
  `-fno-sanitize-recover=all`: all tests pass.  LeakSanitizer could not be
  enabled in this environment (it needs ptrace, which the sandbox
  restricts); `detect_leaks=0` was used, so **no leak check was run**
  beyond the allocator reports.
* clang static analyzer (`clang --analyze`) on `stdbit.c`, `thrd.c`,
  `atomic.c`, `atomic_fallback.c`: clean.
* GCC `-fanalyzer` on the same files: clean.
* ThreadSanitizer could not start in this environment ("failed to allocate
  slab region"); **TSan was not run**.  The memory model is instead
  exercised by the store-buffering litmus test and the acquire/release
  publication test; this is weaker than TSan and is stated as such.

## 10. Fuzzing and stress

* `stdckdint-oracle-tests` performs a deterministic randomised property
  test (16 type/operation combinations, 50000 cases each) against an
  independent 128-bit mathematical model.
* `threads-tests` stresses creation/join (2000 iterations), detached
  threads (500) and `call_once` from several threads.
* `stdatomic-tests` runs eight threads x 100000 operations for both the
  fetch-add and the flag-spinlock cases, plus a 2000-iteration
  publication loop and a 20000-iteration store-buffering loop.
* `stdatomic-fallback-tests` runs eight threads x 100000 fetch-add
  operations on a 16-byte integer and exercises 16/24/40-byte aggregates.
* `stdbit-fallback.sh` is a differential test of the two implementations
  of every `<stdbit.h>` operation.
* No coverage-guided fuzzer was used; there is no input parser to fuzz,
  and the property tests above cover the input space systematically.

## 11. OpenBSD integration status

The build-system integration follows the documented libc conventions:
`include/Makefile` `FILES`, `lib/libc/gen/Makefile.inc` and
`lib/libc/thread/Makefile.inc` `SRCS`, `lib/libc/Symbols.list`, the
`hidden/` `PROTO_NORMAL` headers, and the `MAN` lists.  The new symbols
were inserted following the existing layout and checked for duplicates.

**Not validated on OpenBSD.** There is no OpenBSD machine in this
environment; the libc was not built with the base toolchain and the
regress suite was not run there.  Static and cross-compilation checks were
performed instead, as recorded above.

## 12. ABI changes

The following new symbols are exported by libc (new names only; no
existing symbol changed type or semantics):

* `stdatomic.h`: `atomic_thread_fence`, `atomic_signal_fence`,
  `atomic_flag_test_and_set`, `atomic_flag_test_and_set_explicit`,
  `atomic_flag_clear`, `atomic_flag_clear_explicit`,
  `__atomic_fallback_load`, `__atomic_fallback_store`,
  `__atomic_fallback_exchange`, `__atomic_fallback_compare_exchange`,
  `__atomic_fallback_fetch_add`, `__atomic_fallback_fetch_sub`,
  `__atomic_fallback_fetch_and`, `__atomic_fallback_fetch_or`,
  `__atomic_fallback_fetch_xor`.
* `threads.h`: `thrd_create`, `thrd_equal`, `thrd_current`, `thrd_sleep`,
  `thrd_yield`, `thrd_exit`, `thrd_detach`, `thrd_join`, `mtx_init`,
  `mtx_lock`, `mtx_timedlock`, `mtx_trylock`, `mtx_unlock`,
  `mtx_destroy`, `cnd_init`, `cnd_signal`, `cnd_broadcast`, `cnd_wait`,
  `cnd_timedwait`, `cnd_destroy`, `tss_create`, `tss_delete`, `tss_get`,
  `tss_set`, `call_once`.
* `stdbit.h`: the 70 `stdc_*_{uc,us,ui,ul,ull}` typed functions.

All additions are ABI-compatible; the base system should bump the
`lib/libc/shlib_version` minor by the usual release process.

## 13. Pending limitations

1. The base clang predefines `__STDC_NO_THREADS__` for OpenBSD.  The
   header withdraws it, so an implementation that includes `<threads.h>`
   sees a consistent view, but the compiler target description should be
   fixed upstream, and code that tests the macro *before* including
   anything still sees the stale value.
2. GCC's resource `stdatomic.h` does not `#include_next`, so GCC builds use
   the compiler's header.  This is a compiler-side property; the base
   compiler (clang) uses this implementation.
3. Cross-compilation is syntax-only; no target binaries were executed.
4. `stdbit.h` and `stdckdint.h` are C23 interfaces; the C++ support is a
   convenience, not a standardised C++ interface.
5. There is no OpenBSD machine in this environment, so the libc build and
   the regress suite were not executed on OpenBSD.

## 14. Comparison with Phase I

| Aspect | Phase I | Phase II |
| --- | --- | --- |
| `stdbit.h` typed functions | `static inline` (internal linkage) | real libc symbols |
| stdatomic GCC pointer fetch | byte-scaled (incorrect) | element-scaled (conforming) |
| Non-lock-free atomics | needed libatomic (absent) | lock-based libc fallback, self-contained |
| `threads.h` on OpenBSD target | broken (`__STDC_NO_THREADS__` error) | usable (macro withdrawn) |
| `EAGAIN` from `pthread_create` | `thrd_error` | `thrd_nomem` |
| `stdbit.h` extended types | truncated in the fallback | 128-bit supported |
| C++ inclusion | not for stdbit/stdckdint | overloads / `bool` |
| Architectures checked | amd64 only | seven |
| Static analysis | not run | clang analyzer + GCC `-fanalyzer` clean |
| Sanitizers | not run | ASan+UBSan clean; TSan blocked |
| `stdckdint` testing | boundary cases | boundary cases + 800000-case oracle |
| Patches | one monolithic + splits | none (the directory is the change) |

## 15. File inventory

New (33) and modified (6) files, none deleted, relative to upstream:

* new: the six headers; `lib/libc/thread/thrd.c`;
  `lib/libc/thread/atomic.c`; `lib/libc/thread/atomic_fallback.c`;
  `lib/libc/gen/stdbit.c`; `lib/libc/hidden/threads.h`;
  `lib/libc/hidden/stdatomic.h`; the six manual pages; and the regress
  tests (`stdalign`, `stdnoreturn`, `stdatomic`, `stdbit`, `stdckdint`,
  `threads`, `iso-c-headers`).
* modified: `include/Makefile`, `lib/libc/Symbols.list`,
  `lib/libc/thread/Makefile.inc`, `lib/libc/gen/Makefile.inc`,
  `lib/libpthread/man/Makefile.inc`, `regress/lib/libc/Makefile`.

The `sys/arch/*/include` trees are unchanged upstream copies.  The
host-side harness under `tests/` and this report are project-only.

## 16. Patch status

There is no patch file in the tree.  The directory itself is the change,
and a patch against the recorded upstream revision can be produced with
`diff -ruN` (or `git diff` against a checkout) at any time.  The
provenance of the import is in `UPSTREAM`.

## 17. Items requiring upstream review

1. **clang target description.**  clang predefines `__STDC_NO_THREADS__`
   for `openbsd`; once libc installs `<threads.h>`, the compiler's
   per-OS "has threads" list must be updated so the macro is no longer
   defined.
2. **ABI bump.**  The new symbols are an ABI extension; upstream should
   bump the `lib/libc/shlib_version` minor.
3. **`hidden/pthread.h`.**  `thrd.c` calls pthread entry points marked
   `PROTO_STD_DEPRECATED`; the diagnostic is suppressed locally with a
   documented justification.  Upstream may prefer to promote those entries
   to `PROTO_NORMAL` with matching `DEF_WEAK` definitions.
4. **`__noreturn_is_defined`.**  `stdnoreturn.h` defines this
   non-standard marker for clang compatibility; upstream may drop it.
5. **GCC header shadowing.**  GCC's resource `stdatomic.h` does not
   `#include_next`; if GCC compatibility is a goal, GCC's header would
   have to cooperate or the base system would have to install the header
   in a directory GCC searches first.

## Validation categories

* **Validated on OpenBSD:** nothing.
* **Validated by cross-compilation (syntax only):** the six headers for
  amd64, i386, arm64, armv7, riscv64, powerpc64, sparc64.
* **Validated on Linux with clang 23:** host tests, header matrix,
  fallback, C++ inclusion, ASan+UBSan, static analyzer; C11/C17/C23.
* **Validated on Linux with GCC 16:** host tests, header matrix;
  C11/C17/C23; `-fanalyzer`.
* **Verified only statically:** libc source compilation in the controlled
  OpenBSD include environment; manual-page lint.
* **Not verified:** OpenBSD build and run; ThreadSanitizer (environment
  blocked it); LeakSanitizer; execution of cross-compiled binaries.
