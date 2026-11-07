# libc and stdckdint C99 static inventory (historical)

This inventory predates the subsequent libc synchronization and ISO audit.
Its revision, line numbers and patch observations describe that earlier
snapshot, not the final tree. See `libc/ISO-C-STATUS.md` and `libc/UPSTREAM`
for current status and completed patch reconstruction checks.

This inventory scans every tracked `.c` and `.h` in `libc/` and
`stdckdint/`. The import revision then recorded in `libc/UPSTREAM` was
`484ecc5061dce11274ca0829ca8933cd67f5642e`. The import contains OpenBSD
libc, architecture/kernel header dependencies and regress sources; it is
not all locally authored application code.

No compiler, build or project runtime test was executed for this follow-up.
The scan masks comments and string/character literals before matching;
literal placeholders prevent string initializers from appearing empty.
Line numbers identify lexical candidates for review, not compiler findings.
It does not expand macros, select architecture/preprocessor branches or
prove semantic conformance. GNU statement-expression candidates can include
ordinary parenthesized macro expressions and require contextual review.
Files with no matched feature are scanned, not certified conforming.
No complete line-by-line semantic audit of the imported libc is claimed.

## Decisions and scope

- Modern local headers intentionally implement `_Atomic(T)`, `_Generic`,
  alignment and TLS interfaces. Their source API, alignment, single
  evaluation and synchronization contracts take precedence over a mechanical
  ISO C99 rewrite. See `libc/README.md` for documented exceptions.
- Compiler overflow builtins preserve checked arithmetic, including mixed
  signedness and defined overflow results. They remain explicit extensions.
- Imported assembly, attributes, architecture types and zero-sized layout
  idioms are OpenBSD platform contracts. Replacing them requires individual
  upstream/ABI review; this pass does not edit them speculatively.
- `stdckdint`'s regression result-type assertion now uses an unevaluated
  compiler type predicate with a negative-array typedef. This removes its
  C11 assertion and selection syntax while retaining the exact type check.
  Both copies and their exported patches were synchronized for that source.
- The imported time-conversion regression used empty aggregate initializers;
  C99 `{ 0 }` initializers now preserve their zero-initialization semantics.
  The corresponding delta is included in `libc/patch`.
- Host test defaults use C99; atomic API tests and the forced generic stdbit
  fallback keep explicit C11 modes. Public header inclusion under C99 does
  not prove that every optional macro expansion is ISO C99.

## Static patch checks

All 48 hunks in `libc/patch` have consistent old/new line counts and their
resulting source slices match the current imported paths. The one-line time
initializer delta uses an exact zero-context hunk to avoid treating patch
context prefixes as source indentation errors in `git diff --check`.
All five hunk counts in `stdckdint/stdckdint.patch` are consistent, and its
three available added source targets match the local files exactly. Its two
integration Makefiles are not present in the standalone contribution tree.
No application against a pristine upstream checkout was performed; these
checks do not establish that external integration contexts still apply.
`git diff --check` is clean for the owned paths. No binaries or temporary
analysis files were created by this follow-up.

## Scan totals

2149 tracked C/header files were scanned; 231 contain one or more
listed lexical candidates.

- GNU-extension: 193 files
- modern-keyword: 10 files
- statement-expression: 10 files
- compiler-builtin: 34 files
- extended-integer: 9 files
- modern-directive: 1 file

## File-granular candidates

| File | Feature and line |
| --- | --- |
| `libc/include/arpa/inet.h` | GNU-extension: 129, 137, 141, 143 |
| `libc/include/assert.h` | modern-keyword: 66 |
| `libc/include/bitstring.h` | statement-expression: 73 |
| `libc/include/bsd_auth.h` | GNU-extension: 64, 84 |
| `libc/include/ctype.h` | GNU-extension: 221, 227, 233, 239, 245, 251, 257, 263, 269, 275, 281, 287, 293, 299 |
| `libc/include/dirent.h` | GNU-extension: 80 |
| `libc/include/err.h` | GNU-extension: 44, 46, 48, 50, 52, 54, 56, 58, 60, 62, 64, 66 |
| `libc/include/kvm.h` | GNU-extension: 73, 75 |
| `libc/include/math.h` | compiler-builtin: 29, 42, 43, 44, 45 |
| `libc/include/md5.h` | GNU-extension: 31, 33, 35, 37, 39, 40 |
| `libc/include/resolv.h` | GNU-extension: 298, 300, 303, 307, 308, 311 |
| `libc/include/rmd160.h` | GNU-extension: 42, 44, 46, 48, 50, 51 |
| `libc/include/sha1.h` | GNU-extension: 25, 27, 29, 31, 33, 34 |
| `libc/include/sha2.h` | GNU-extension: 72, 74, 76, 78, 80, 81, 85, 87, 89, 91, 93, 94, 98, 100, 102, 104, 106, 107, 111, 113, 115, 117, 119, 120, 124, 126, 128, 130, 132, 133 |
| `libc/include/stdalign.h` | modern-keyword: 44, 45 |
| `libc/include/stdatomic.h` | modern-keyword: 167, 168, 169, 170, 171, 172, 173, 174, 175, 176, 177, 178, 180, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191, 192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 206; GNU-extension: 167, 168, 169, 170, 171, 172, 173, 174, 175, 176, 177, 178, 180, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191, 192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 206, 289, 290, 291, 299, 300, 301, 310, 311, 312, 313, 324, 325, 326, 340, 341, 342, 355, 356, 357, 361, 362, 399, 400, 401, 409, 410, 411, 420, 421, 422, 423, 434, 435, 436, 449, 450, 451, 463, 464, 465, 467, 475, 476, 477, 484, 485; compiler-builtin: 479, 480; statement-expression: 289, 299, 310, 324, 340, 355, 399, 409, 420, 434, 449, 463, 475 |
| `libc/include/stdbit.h` | modern-keyword: 200; extended-integer: 155, 156; GNU-extension: 155, 156, 238, 239, 243, 244, 249, 254, 259, 264, 277, 278, 279, 281, 287, 288, 289, 291, 293; compiler-builtin: 154, 164, 167, 177, 180, 187, 188, 215, 216, 221, 222, 227, 228; statement-expression: 243, 249, 254, 259, 264, 277, 287 |
| `libc/include/stdckdint.h` | compiler-builtin: 55, 56, 57, 64, 69, 74 |
| `libc/include/stddef.h` | compiler-builtin: 69 |
| `libc/include/stdio.h` | GNU-extension: 128, 129, 138, 144, 153, 192, 193, 198, 199, 200, 203, 204, 205, 210, 211, 213, 214, 216, 217, 265, 266, 268, 269 |
| `libc/include/stdlib.h` | GNU-extension: 116, 184, 191, 308, 318; modern-directive: 93; compiler-builtin: 267 |
| `libc/include/stdnoreturn.h` | modern-keyword: 40 |
| `libc/include/string.h` | GNU-extension: 66, 67, 69, 70, 72, 82, 85, 93, 97, 102, 118, 124, 126, 131 |
| `libc/include/strings.h` | GNU-extension: 64, 65, 67 |
| `libc/include/tgmath.h` | GNU-extension: 58, 64, 65, 70; compiler-builtin: 58, 73, 74, 79 |
| `libc/include/threads.h` | modern-keyword: 98 |
| `libc/include/time.h` | GNU-extension: 122, 136, 138, 162 |
| `libc/include/unistd.h` | GNU-extension: 333, 336, 346, 369, 380, 389, 402, 404, 416, 438, 440, 442, 462, 472, 500, 513 |
| `libc/include/vis.h` | GNU-extension: 83, 85, 89 |
| `libc/include/wchar.h` | GNU-extension: 124, 135, 139, 144, 155, 156, 158, 159, 161, 186, 189, 211, 238, 240 |
| `libc/lib/libc/arch/DEFS.h` | GNU-extension: 18 |
| `libc/lib/libc/arch/aarch64/gen/_atomic_lock.c` | GNU-extension: 40 |
| `libc/lib/libc/arch/aarch64/gen/infinity.c` | GNU-extension: 11 |
| `libc/lib/libc/arch/aarch64/gen/nan.c` | GNU-extension: 9 |
| `libc/lib/libc/arch/alpha/SYS.h` | GNU-extension: 31 |
| `libc/lib/libc/arch/alpha/gen/flt_rounds.c` | GNU-extension: 51, 52, 53 |
| `libc/lib/libc/arch/alpha/gen/fpgetround.c` | GNU-extension: 44, 45, 46 |
| `libc/lib/libc/arch/alpha/gen/fpsetround.c` | GNU-extension: 45, 46, 47, 54, 55, 56 |
| `libc/lib/libc/arch/alpha/gen/infinity.c` | GNU-extension: 34 |
| `libc/lib/libc/arch/alpha/gen/nan.c` | GNU-extension: 8 |
| `libc/lib/libc/arch/amd64/gen/_atomic_lock.c` | GNU-extension: 21 |
| `libc/lib/libc/arch/amd64/gen/infinity.c` | GNU-extension: 7 |
| `libc/lib/libc/arch/amd64/gen/nan.c` | GNU-extension: 8 |
| `libc/lib/libc/arch/amd64/hash/sha1_amd64.c` | compiler-builtin: 28 |
| `libc/lib/libc/arch/amd64/hash/sha256_amd64.c` | compiler-builtin: 28 |
| `libc/lib/libc/arch/arm/dlfcn/exidx.c` | GNU-extension: 22 |
| `libc/lib/libc/arch/arm/gen/_atomic_lock.c` | GNU-extension: 40 |
| `libc/lib/libc/arch/arm/gen/infinity.c` | GNU-extension: 11 |
| `libc/lib/libc/arch/arm/gen/nan.c` | GNU-extension: 9 |
| `libc/lib/libc/arch/hppa/DEFS.h` | GNU-extension: 28 |
| `libc/lib/libc/arch/hppa/gen/_atomic_lock.c` | GNU-extension: 36 |
| `libc/lib/libc/arch/hppa/gen/fabs.c` | GNU-extension: 14 |
| `libc/lib/libc/arch/hppa/gen/flt_rounds.c` | GNU-extension: 22 |
| `libc/lib/libc/arch/hppa/gen/fpgetmask.c` | GNU-extension: 15 |
| `libc/lib/libc/arch/hppa/gen/fpgetround.c` | GNU-extension: 15 |
| `libc/lib/libc/arch/hppa/gen/fpgetsticky.c` | GNU-extension: 15 |
| `libc/lib/libc/arch/hppa/gen/fpsetmask.c` | GNU-extension: 17, 20 |
| `libc/lib/libc/arch/hppa/gen/fpsetround.c` | GNU-extension: 17, 21 |
| `libc/lib/libc/arch/hppa/gen/fpsetsticky.c` | GNU-extension: 17, 20 |
| `libc/lib/libc/arch/hppa/gen/infinity.c` | GNU-extension: 8 |
| `libc/lib/libc/arch/hppa/gen/nan.c` | GNU-extension: 8 |
| `libc/lib/libc/arch/i386/DEFS.h` | GNU-extension: 18 |
| `libc/lib/libc/arch/i386/gen/_atomic_lock.c` | GNU-extension: 20 |
| `libc/lib/libc/arch/i386/gen/nan.c` | GNU-extension: 8 |
| `libc/lib/libc/arch/m88k/gen/_atomic_lock.c` | GNU-extension: 40 |
| `libc/lib/libc/arch/m88k/gen/flt_rounds.c` | GNU-extension: 27 |
| `libc/lib/libc/arch/m88k/gen/fpgetmask.c` | GNU-extension: 16 |
| `libc/lib/libc/arch/m88k/gen/fpgetround.c` | GNU-extension: 16 |
| `libc/lib/libc/arch/m88k/gen/fpgetsticky.c` | GNU-extension: 16 |
| `libc/lib/libc/arch/m88k/gen/fpsetmask.c` | GNU-extension: 18, 24 |
| `libc/lib/libc/arch/m88k/gen/fpsetround.c` | GNU-extension: 18, 24 |
| `libc/lib/libc/arch/m88k/gen/fpsetsticky.c` | GNU-extension: 18, 24 |
| `libc/lib/libc/arch/m88k/gen/infinity.c` | GNU-extension: 8 |
| `libc/lib/libc/arch/m88k/gen/nan.c` | GNU-extension: 8 |
| `libc/lib/libc/arch/mips64/SYS.h` | GNU-extension: 36 |
| `libc/lib/libc/arch/mips64/gen/_atomic_lock.c` | GNU-extension: 15 |
| `libc/lib/libc/arch/mips64/gen/flt_rounds.c` | GNU-extension: 22, 23 |
| `libc/lib/libc/arch/mips64/gen/fpc_csr.c` | GNU-extension: 31, 40, 41 |
| `libc/lib/libc/arch/mips64/gen/fpgetmask.c` | GNU-extension: 14 |
| `libc/lib/libc/arch/mips64/gen/fpgetround.c` | GNU-extension: 14 |
| `libc/lib/libc/arch/mips64/gen/fpgetsticky.c` | GNU-extension: 14 |
| `libc/lib/libc/arch/mips64/gen/fpsetmask.c` | GNU-extension: 16, 22 |
| `libc/lib/libc/arch/mips64/gen/fpsetround.c` | GNU-extension: 16, 22 |
| `libc/lib/libc/arch/mips64/gen/fpsetsticky.c` | GNU-extension: 16, 22 |
| `libc/lib/libc/arch/mips64/gen/nan.c` | GNU-extension: 9 |
| `libc/lib/libc/arch/powerpc/gen/_atomic_lock.c` | GNU-extension: 29 |
| `libc/lib/libc/arch/powerpc/gen/fabs.c` | GNU-extension: 33 |
| `libc/lib/libc/arch/powerpc/gen/flt_rounds.c` | GNU-extension: 56 |
| `libc/lib/libc/arch/powerpc/gen/fpgetmask.c` | GNU-extension: 41 |
| `libc/lib/libc/arch/powerpc/gen/fpgetround.c` | GNU-extension: 41 |
| `libc/lib/libc/arch/powerpc/gen/fpgetsticky.c` | GNU-extension: 41 |
| `libc/lib/libc/arch/powerpc/gen/fpsetmask.c` | GNU-extension: 43, 46 |
| `libc/lib/libc/arch/powerpc/gen/fpsetround.c` | GNU-extension: 43, 46 |
| `libc/lib/libc/arch/powerpc/gen/fpsetsticky.c` | GNU-extension: 43, 50 |
| `libc/lib/libc/arch/powerpc/gen/infinity.c` | GNU-extension: 8 |
| `libc/lib/libc/arch/powerpc/gen/nan.c` | GNU-extension: 8 |
| `libc/lib/libc/arch/powerpc64/DEFS.h` | GNU-extension: 38 |
| `libc/lib/libc/arch/powerpc64/gen/_atomic_lock.c` | GNU-extension: 29 |
| `libc/lib/libc/arch/powerpc64/gen/fabs.c` | GNU-extension: 33 |
| `libc/lib/libc/arch/powerpc64/gen/flt_rounds.c` | GNU-extension: 56 |
| `libc/lib/libc/arch/powerpc64/gen/fpgetmask.c` | GNU-extension: 41 |
| `libc/lib/libc/arch/powerpc64/gen/fpgetround.c` | GNU-extension: 41 |
| `libc/lib/libc/arch/powerpc64/gen/fpgetsticky.c` | GNU-extension: 41 |
| `libc/lib/libc/arch/powerpc64/gen/fpsetmask.c` | GNU-extension: 43, 46 |
| `libc/lib/libc/arch/powerpc64/gen/fpsetround.c` | GNU-extension: 43, 46 |
| `libc/lib/libc/arch/powerpc64/gen/fpsetsticky.c` | GNU-extension: 43, 50 |
| `libc/lib/libc/arch/powerpc64/gen/infinity.c` | GNU-extension: 8 |
| `libc/lib/libc/arch/powerpc64/gen/nan.c` | GNU-extension: 8 |
| `libc/lib/libc/arch/riscv64/DEFS.h` | GNU-extension: 37 |
| `libc/lib/libc/arch/riscv64/gen/_atomic_lock.c` | GNU-extension: 38 |
| `libc/lib/libc/arch/riscv64/gen/infinity.c` | GNU-extension: 9 |
| `libc/lib/libc/arch/riscv64/gen/nan.c` | GNU-extension: 8 |
| `libc/lib/libc/arch/sh/SYS.h` | GNU-extension: 37 |
| `libc/lib/libc/arch/sh/gen/fabs.c` | GNU-extension: 30 |
| `libc/lib/libc/arch/sh/gen/flt_rounds.c` | GNU-extension: 36 |
| `libc/lib/libc/arch/sh/gen/fpgetmask.c` | GNU-extension: 27 |
| `libc/lib/libc/arch/sh/gen/fpgetround.c` | GNU-extension: 27 |
| `libc/lib/libc/arch/sh/gen/fpgetsticky.c` | GNU-extension: 27 |
| `libc/lib/libc/arch/sh/gen/fpsetmask.c` | GNU-extension: 32, 34 |
| `libc/lib/libc/arch/sh/gen/fpsetround.c` | GNU-extension: 30, 37 |
| `libc/lib/libc/arch/sh/gen/fpsetsticky.c` | GNU-extension: 32, 34 |
| `libc/lib/libc/arch/sh/gen/infinity.c` | GNU-extension: 9 |
| `libc/lib/libc/arch/sh/gen/nan.c` | GNU-extension: 9 |
| `libc/lib/libc/arch/sparc64/fpu/fpu_qp.c` | GNU-extension: 54 |
| `libc/lib/libc/arch/sparc64/fpu/fpu_reg.c` | GNU-extension: 36, 39, 42, 45, 48, 51, 54, 57, 60, 63, 66, 69, 72, 75, 78, 81, 84, 87, 90, 93, 96, 99, 102, 105, 108, 111, 114, 117, 120, 123, 126, 129, 132, 135, 138, 141, 144, 147, 150, 153, 156, 159, 162, 165, 168, 171, 174, 177, 180, 183, 186, 189, 192, 195, 198, 201, 204, 207, 210, 213, 216, 219, 222, 225, 235, 238, 241, 244, 247, 250, 253, 256, 259, 262, 265, 268, 271, 274, 277, 280, 283, 286, 289, 292, 295, 298, 301, 304, 307, 310, 313, 316, 319, 322, 325, 328, 340, 343, 346, 349, 352, 355, 358, 361, 364, 367, 370, 373, 376, 379, 382, 385, 388, 391, 394, 397, 400, 403, 406, 409, 412, 415, 418, 421, 424, 427, 430, 433, 436, 439, 442, 445, 448, 451, 454, 457, 460, 463, 466, 469, 472, 475, 478, 481, 484, 487, 490, 493, 496, 499, 502, 505, 508, 511, 514, 517, 520, 523, 526, 529, 542, 545, 548, 551, 554, 557, 560, 563, 566, 569, 572, 575, 578, 581, 584, 587, 590, 593, 596, 599, 602, 605, 608, 611, 614, 617, 620, 623, 626, 629, 632, 635 |
| `libc/lib/libc/arch/sparc64/gen/_atomic_lock.c` | GNU-extension: 38 |
| `libc/lib/libc/arch/sparc64/gen/flt_rounds.c` | GNU-extension: 24 |
| `libc/lib/libc/arch/sparc64/gen/fpgetmask.c` | GNU-extension: 15 |
| `libc/lib/libc/arch/sparc64/gen/fpgetround.c` | GNU-extension: 15 |
| `libc/lib/libc/arch/sparc64/gen/fpgetsticky.c` | GNU-extension: 15 |
| `libc/lib/libc/arch/sparc64/gen/fpsetmask.c` | GNU-extension: 16, 22 |
| `libc/lib/libc/arch/sparc64/gen/fpsetround.c` | GNU-extension: 16, 22 |
| `libc/lib/libc/arch/sparc64/gen/fpsetsticky.c` | GNU-extension: 16, 22 |
| `libc/lib/libc/arch/sparc64/gen/infinity.c` | GNU-extension: 8 |
| `libc/lib/libc/arch/sparc64/gen/nan.c` | GNU-extension: 8 |
| `libc/lib/libc/dlfcn/dlfcn_stubs.c` | GNU-extension: 121, 123, 134 |
| `libc/lib/libc/dlfcn/init.c` | GNU-extension: 49, 65, 66, 76, 83 |
| `libc/lib/libc/dlfcn/tib.c` | GNU-extension: 31, 32 |
| `libc/lib/libc/gen/stdbit.c` | compiler-builtin: 44, 47, 65, 68, 84, 85 |
| `libc/lib/libc/include/namespace.h` | GNU-extension: 37, 46, 47, 48, 49, 50, 52, 73, 74, 76 |
| `libc/lib/libc/include/thread_private.h` | GNU-extension: 478 |
| `libc/lib/libc/locale/isctype_l.c` | GNU-extension: 28, 35, 42, 49, 56, 63, 70, 77, 84, 91, 98, 105, 112, 119 |
| `libc/lib/libc/locale/iswctype_l.c` | GNU-extension: 192 |
| `libc/lib/libc/locale/wcscoll_l.c` | GNU-extension: 14 |
| `libc/lib/libc/locale/wcsxfrm_l.c` | GNU-extension: 11 |
| `libc/lib/libc/locale/wctype.c` | GNU-extension: 72 |
| `libc/lib/libc/stdio/findfp.c` | GNU-extension: 125 |
| `libc/lib/libc/stdlib/malloc.c` | GNU-extension: 262, 263, 267, 270, 285, 311; compiler-builtin: 297, 298, 300, 301, 303, 304, 306, 307, 319, 1099 |
| `libc/lib/libc/string/explicit_bzero.c` | GNU-extension: 9 |
| `libc/lib/libc/string/ffsl.c` | compiler-builtin: 16 |
| `libc/lib/libc/string/ffsll.c` | compiler-builtin: 16 |
| `libc/lib/libc/string/strcasecmp_l.c` | GNU-extension: 11, 18 |
| `libc/lib/libc/string/strcoll_l.c` | GNU-extension: 11 |
| `libc/lib/libc/string/strxfrm_l.c` | GNU-extension: 11 |
| `libc/lib/libc/sys/stack_protector.c` | GNU-extension: 47 |
| `libc/lib/libc/thread/rthread_mutex.c` | GNU-extension: 41 |
| `libc/lib/libc/time/strftime_l.c` | GNU-extension: 11 |
| `libc/regress/lib/libc/explicit_bzero/explicit_bzero.c` | GNU-extension: 37 |
| `libc/regress/lib/libc/modf/modf_test.c` | compiler-builtin: 39, 44 |
| `libc/regress/lib/libc/stdatomic/stdatomic.c` | modern-keyword: 106, 128, 151 |
| `libc/regress/lib/libc/stdckdint/stdckdint.c` | extended-integer: 148, 149; GNU-extension: 82, 148, 149; compiler-builtin: 81 |
| `libc/regress/lib/libc/sys/atf-c.h` | GNU-extension: 30 |
| `libc/regress/lib/libc/sys/h_macros.h` | GNU-extension: 57 |
| `libc/regress/lib/libc/sys/t_getrusage.c` | GNU-extension: 68, 70, 72, 74 |
| `libc/sys/arch/amd64/include/_types.h` | compiler-builtin: 125 |
| `libc/sys/arch/amd64/include/cdefs.h` | GNU-extension: 12, 15, 18 |
| `libc/sys/arch/amd64/include/codepatch.h` | GNU-extension: 26 |
| `libc/sys/arch/amd64/include/cpu.h` | GNU-extension: 303; statement-expression: 302 |
| `libc/sys/arch/amd64/include/cpufunc.h` | GNU-extension: 193, 256, 264 |
| `libc/sys/arch/amd64/include/pctr.h` | statement-expression: 55 |
| `libc/sys/arch/amd64/include/pio.h` | compiler-builtin: 48 |
| `libc/sys/arch/amd64/include/tcb.h` | GNU-extension: 43 |
| `libc/sys/arch/arm/include/_types.h` | compiler-builtin: 125 |
| `libc/sys/arch/arm/include/cdefs.h` | GNU-extension: 7, 10, 13 |
| `libc/sys/arch/arm/include/cpufunc.h` | GNU-extension: 316 |
| `libc/sys/arch/arm/include/profile.h` | GNU-extension: 52, 53, 54, 55, 56, 60, 67, 71, 75, 79 |
| `libc/sys/arch/arm64/include/_types.h` | compiler-builtin: 126 |
| `libc/sys/arch/arm64/include/armreg.h` | statement-expression: 40 |
| `libc/sys/arch/arm64/include/bus.h` | GNU-extension: 321 |
| `libc/sys/arch/arm64/include/cdefs.h` | GNU-extension: 7, 10, 13 |
| `libc/sys/arch/arm64/include/codepatch.h` | GNU-extension: 26 |
| `libc/sys/arch/arm64/include/profile.h` | GNU-extension: 29, 64, 66 |
| `libc/sys/arch/arm64/include/reg.h` | extended-integer: 31 |
| `libc/sys/arch/armv7/include/asm.h` | GNU-extension: 4 |
| `libc/sys/arch/i386/include/_types.h` | compiler-builtin: 125 |
| `libc/sys/arch/i386/include/atomic.h` | GNU-extension: 276, 284, 291 |
| `libc/sys/arch/i386/include/cdefs.h` | GNU-extension: 12, 15, 18 |
| `libc/sys/arch/i386/include/pctr.h` | statement-expression: 59, 67, 78 |
| `libc/sys/arch/i386/include/pio.h` | compiler-builtin: 48 |
| `libc/sys/arch/i386/include/tcb.h` | GNU-extension: 41 |
| `libc/sys/arch/powerpc64/include/_types.h` | compiler-builtin: 126 |
| `libc/sys/arch/powerpc64/include/bus.h` | GNU-extension: 314 |
| `libc/sys/arch/powerpc64/include/cdefs.h` | GNU-extension: 12, 15, 18 |
| `libc/sys/arch/powerpc64/include/profile.h` | GNU-extension: 25 |
| `libc/sys/arch/powerpc64/include/reg.h` | extended-integer: 33 |
| `libc/sys/arch/powerpc64/include/signal.h` | extended-integer: 56 |
| `libc/sys/arch/powerpc64/include/tcb.h` | GNU-extension: 37 |
| `libc/sys/arch/riscv64/include/_types.h` | compiler-builtin: 127 |
| `libc/sys/arch/riscv64/include/bus.h` | GNU-extension: 329 |
| `libc/sys/arch/riscv64/include/cdefs.h` | GNU-extension: 7, 10, 13 |
| `libc/sys/arch/riscv64/include/cpu.h` | GNU-extension: 148 |
| `libc/sys/arch/riscv64/include/profile.h` | GNU-extension: 30, 88, 90 |
| `libc/sys/arch/riscv64/include/riscvreg.h` | compiler-builtin: 215; statement-expression: 218, 229, 236, 243, 250 |
| `libc/sys/arch/sparc64/include/_types.h` | compiler-builtin: 125 |
| `libc/sys/arch/sparc64/include/cdefs.h` | GNU-extension: 7, 10, 13 |
| `libc/sys/arch/sparc64/include/cpu.h` | GNU-extension: 184 |
| `libc/sys/arch/sparc64/include/ctlreg.h` | compiler-builtin: 509, 519, 617, 665; statement-expression: 603, 654 |
| `libc/sys/arch/sparc64/include/profile.h` | GNU-extension: 46, 47, 48, 49, 50, 51, 52, 53, 56, 57, 58, 59, 60, 61 |
| `libc/sys/arch/sparc64/include/tcb.h` | GNU-extension: 39 |
| `libc/sys/sys/_endian.h` | compiler-builtin: 87, 89, 91 |
| `libc/sys/sys/_null.h` | modern-keyword: 12 |
| `libc/sys/sys/atomic.h` | GNU-extension: 280, 286, 287; statement-expression: 279, 285 |
| `libc/sys/sys/cdefs.h` | GNU-extension: 103, 107, 115, 123, 129, 135, 141, 153, 167, 230, 231, 235, 239, 253, 254; compiler-builtin: 204, 205 |
| `libc/sys/sys/kthread.h` | GNU-extension: 47 |
| `libc/sys/sys/param.h` | compiler-builtin: 198 |
| `libc/sys/sys/percpu.h` | GNU-extension: 30 |
| `libc/sys/sys/queue.h` | GNU-extension: 345 |
| `libc/sys/sys/srp.h` | GNU-extension: 28 |
| `libc/sys/sys/stdarg.h` | compiler-builtin: 27, 34, 35, 36, 37 |
| `libc/sys/sys/syslog.h` | GNU-extension: 199, 205, 214, 216 |
| `libc/sys/sys/systm.h` | GNU-extension: 160, 162, 164, 166, 168, 170, 172, 175, 185, 186, 189, 190, 192, 194, 197, 198, 200, 201, 203, 206, 208, 211, 213, 224; compiler-builtin: 403, 404, 405, 406, 408, 411, 412 |
| `libc/sys/sys/tprintf.h` | GNU-extension: 41 |
| `libc/sys/sys/tree.h` | GNU-extension: 381, 400 |
| `libc/sys/sys/varargs.h` | GNU-extension: 29; compiler-builtin: 24, 29, 33, 39, 42, 43, 44, 45 |
| `libc/sys/sys/videoio.h` | GNU-extension: 5187, 5296, 5337, 5489, 5490, 5608, 5736, 5974, 5979, 5983, 5994, 6012, 6047, 6059, 6078, 6134, 6232, 6239, 6250 |
| `libc/tests/stdatomic-fallback-tests.c` | modern-keyword: 38, 39, 40, 83; extended-integer: 82; GNU-extension: 82 |
| `libc/tests/stdatomic-tests.c` | modern-keyword: 185, 222 |
| `libc/tests/stdbit-tests.c` | extended-integer: 100; GNU-extension: 100 |
| `libc/tests/stdckdint-oracle-tests.c` | extended-integer: 29, 30; GNU-extension: 29, 30 |
| `stdckdint/include/stdckdint.h` | compiler-builtin: 47, 48, 49, 56, 60, 64 |
| `stdckdint/regress/lib/libc/stdckdint/stdckdint.c` | extended-integer: 148, 149; GNU-extension: 82, 148, 149; compiler-builtin: 81 |
