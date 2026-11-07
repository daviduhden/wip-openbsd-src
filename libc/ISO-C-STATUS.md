# ISO C interface status and implementation audit

This component already contains an original implementation of six headers;
this audit extends that work rather than replacing it with compiler headers.
The official OpenBSD-current reference remains
`711709eb319e1b619533c82fb9931392e43061cd` (2026-11-07 17:16:58 UTC),
confirmed again with git ls-remote. None of the six public headers occurs in
that revision's include tree or installation list. C17 adds no new headers.
The complete libc/include/private sources and necessary pthread/architecture
roots are retained; no installed header or library was changed.

## Interpretation of status

Implemented means the named interface exists and was reviewed in source; it
is not a claim that an ISO edition or complete library has been certified.
All compiler, linker, concurrency and runtime checks added in this audit are
**unexecuted**, because this session prohibits compilation. Historical test
passes in REPORT.md do not validate these changed files.

| Header | C89/C90, C95, C99 | C11 | C17 | C23 | C++ boundary |
| --- | --- | --- | --- | --- | --- |
| stdalign.h | Not a standard header | Implemented macros | Same | Empty header; keywords preserved | Legacy markers only; no keyword macros |
| stdnoreturn.h | Not a standard header | Implemented macro | Same | Deprecated interface retained | Macro left undefined |
| stdatomic.h | Not a standard header | Partial language support; macro APIs implemented | Same | Partial; updated types/initializers | Clang inclusion only; no full C++ API claim |
| threads.h | Not a standard header | Implemented in libpthread | Same | Interface retained; keyword guard | Inclusion only |
| stdbit.h | Not a standard header | Explicit inclusion is an extension | Same | Implemented scalar interfaces with limits below | Standard unsigned overloads; no general extended-type promise |
| stdckdint.h | Not a standard header | Explicit inclusion is an extension | Same | Implemented for valid integer arguments | Compiler-supported bool wrapper |

Older-language canaries cover existing standard headers without indirectly
including modern interfaces. Optional inclusion in an older language mode
does not make a modern header part of that ISO edition. The native build
retains its normal language level; only relevant regression targets require
C23 to exercise bit-precise integers.

## Implementation and provider inventory

| Area | Existing OpenBSD facilities | Local implementation and dependency |
| --- | --- | --- |
| Alignment/noreturn | Compiler language keywords, cdefs attributes | Self-contained original public headers; no runtime symbols |
| Atomics | Compiler intrinsics, native architecture operations | Original public header, six real fence/flag exports, nine fallback helpers in libc |
| Threads | libc synchronization/TLS plus lib/librthread lifecycle and attributes | Original threads.h and 25 wrappers in lib/librthread/thrd.c; link -pthread |
| Bit operations | Compiler primitive intrinsics | Original header and 70 typed libc functions; C23 generic dispatch and 128-bit helpers |
| Checked arithmetic | Compiler checked arithmetic intrinsics | Original stdckdint wrapper; no external header/runtime delegation |

The native libc include order now selects private wrappers followed by the
public source include directory, ahead of compiler resources. librthread
similarly puts its private wrappers before the public source headers.
Header provenance guards remain in the matrices; the private stdbit wrapper
also rejects a compiler-only substitute. This needs a matching OpenBSD build
and header dependency tree; adding include flags is not native validation.

## Corrections from this audit

### Atomics

Fallback copies, comparisons and arithmetic now access atomic storage through
volatile-qualified byte pointers while retaining the existing address locks.
The carry step uses CHAR_BIT and unsupported byte order diagnoses rather
than silently selecting the big-endian path.
Lock-free queries use the same non-atomic value-size/typical-alignment policy
as operation macros, evaluate the object argument once, and no longer confuse
padded atomic storage with payload size. A compile-time guard rejects targets
where the one-byte atomic_flag representation cannot be always lock-free.
Atomic integer aliases now use the corresponding public stdint/stddef/uchar
types rather than compiler type guesses. In particular, OpenBSD's fast
integer types can differ from __INT_FAST*_TYPE__ definitions. Exact-type
assertions cover the aliases. This can correct type/size/alignment of an
existing local alias: all consumers of the development header must be
recompiled; historical OpenBSD exports are not changed.
Fetch fallback conversion selects scaled uintptr_t pointer displacement
before casting to the value type, avoiding width-mismatched integer-to-pointer
casts while retaining full-width integer operands. Pointer scaling assumes
the imported flat OpenBSD ABIs and complete object pointees; incomplete,
void/function and variably modified pointee cases are not claimed validated.
Six ISO fence/flag symbols now have strong aliases matching their hidden
PROTO_NORMAL declarations. Host tests compile the real atomic.c provider and
both host/native tests take pointers to all six exports.

**Remaining atomic limitation:** compiler-generated direct atomic loads,
assignments, increments or compound expressions do not use the macro fallback
ABI. They may require unavailable runtime symbols or use a different atomic
access protocol, including when padded atomic storage is hardware lock-free
but its payload is routed through these helpers. Such accesses must not be
mixed with fallback macro operations. Full language-level support therefore
remains partial; the existing lock-based macro tests are not proof otherwise.
No compiler runtime ABI is invented without target evidence.

### Threads

POSIX errors are mapped according to each ISO function's permitted result
set. thrd_nomem is reserved here for thrd_create, mtx_init and cnd_init;
TSS creation/set failures and other restricted-result APIs return thrd_error.
The real native EAGAIN/ENOMEM paths motivated these changes. Tests exhaust
actual TSS keys and release all successful allocations, without fake syscall
results. The native test no longer deletes a key before joined workers use
it; detached workers have no dependency on that key's lifetime.

Last-thread exit semantics delegate to OpenBSD's documented pthread_exit
contract. Native atexit, cancellation, destructor iteration and last-thread
execution have not been revalidated; no speculative thread-count scheme was
added.

### Bit operations and namespace

C23 fallback dispatch now recognizes distinct unsigned _BitInt types whose
widths match standard unsigned types or the supported extended 128-bit type.
Equal widths are deduplicated for ILP32/LP64 layouts. Unrecognized fallback
arguments diagnose instead of silently converting/truncating to unsigned
long long. Unknown byte order now diagnoses instead of being labelled big.
The 70 typed functions have matching hidden declarations and strong aliases,
retaining their existing public export names.

Raw bit-field arguments to stdbit/stdckdint generic interfaces are not
promised. [WG14 issue 1077](https://www.open-std.org/JTC1/SC22/WG14/issues/c23/issue1077.html)
remains open on exactly that question and on observed bit-field types.
Convert to the intended eligible integer type first, or use a typed bit
function. This is kept distinct from the fixed _BitInt-width bug.

### Checked arithmetic and simple headers

The checked arithmetic implementation retains exact-result builtins, mixed
signedness and single argument evaluation. No defect in valid calls justified
rewriting it. stdalign/stdnoreturn keyword behavior is preserved; inaccurate
C++ commentary and unsubstantiated release-history claims were corrected.
The six manual pages describe local development interfaces, not an already
released OpenBSD feature set.

## Validation inventory

| Check | Result in this audit |
| --- | --- |
| Official revision/tree metadata | Retrieved from the official Git origin; pristine source matched by blob IDs and Git modes |
| Complete import and six-header absence | Verified against official source path inventory |
| stdbit public/private aliases/exports | 70 matching entries, statically checked |
| Native Threads exports | Existing 25 retained; provider unchanged from the prior corrected integration |
| Flag/fence real symbols | Six strong aliases and function-pointer test references checked |
| POSIX shell syntax | sh -n only; compiler commands were not executed |
| Manual pages | mandoc lint; missing host manual database references are distinguished from source diagnostics |
| Patch/reconstruction | Regenerated against the verified pristine revision; final results recorded below |
| Host/native test execution | Not run in this audit |
| Seven architecture builds and native static/shared linking | Not run; target environment and compilation permission required |

Planned tests preserve earlier cases and add volatile aggregates/128-bit
RMW, three-byte payload/query consistency, real-symbol linkage, TSS resource
exhaustion, matching-width _BitInt values/types/side effects, legacy language
canaries and C23 keyword guards. Forced bit fallbacks run C11 then C23.
Both compilers must use this tree's guards; accidentally testing compiler
resource headers is a failure, not an acceptable substitution.

## References and evidence boundary

The public committee drafts [N1570](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1570.pdf)
and [N3096](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n3096.pdf)
were used to review interface contracts (alignment/noreturn, atomics,
threads, bit operations and checked arithmetic). They are committee drafts,
not a claim of possession or certification against the published ISO text.
Implementation code remains project-authored; compiler intrinsics are used
as primitives, not as imported header implementations.


## Planned compiler matrix (not executed)

With the required host toolchains and permission to compile, run the host
suite with Clang and GCC in each atomic mode, for example:

```sh
bmake -C libc/tests clean run CC=clang CXX=clang++ ATOMIC_STD=c11
bmake -C libc/tests clean run CC=clang CXX=clang++ ATOMIC_STD=c17
bmake -C libc/tests clean run CC=clang CXX=clang++ ATOMIC_STD=c23
bmake -C libc/tests clean run CC=gcc CXX=clang++ ATOMIC_STD=c11
bmake -C libc/tests clean run CC=gcc CXX=clang++ ATOMIC_STD=c17
bmake -C libc/tests clean run CC=gcc CXX=clang++ ATOMIC_STD=c23
```

The C++ joint case deliberately uses Clang: GCC C++ lacks the required
_Atomic language extension and is not advertised as supported. Bit tests
select C23 and require bit-precise support rather than silently skipping
that coverage; the forced fallback suite repeats C11 and C23. All of these
commands are instructions for later validation, not results from this audit.
On OpenBSD, use the native regress harness in a matching isolated test
system with these libraries and headers; host runs are not substitutes.


## Source inventory against the pinned upstream revision

The complete patch has 35 additions and 18 modifications; no upstream
source is deleted. The focused ISO patch has 33 additions and 14
modifications. It excludes the preserved login_cap, glob and time fixes
from earlier work. These patches are alternatives against the same pristine
revision and must not be applied on top of each other.

### Added source paths

```text
include/stdalign.h
include/stdatomic.h
include/stdbit.h
include/stdckdint.h
include/stdnoreturn.h
include/threads.h
lib/libc/gen/stdalign.3
lib/libc/gen/stdatomic.3
lib/libc/gen/stdbit.3
lib/libc/gen/stdbit.c
lib/libc/gen/stdckdint.3
lib/libc/gen/stdnoreturn.3
lib/libc/hidden/stdatomic.h
lib/libc/hidden/stdbit.h
lib/libc/thread/atomic.c
lib/libc/thread/atomic_fallback.c
lib/libpthread/man/threads.3
lib/librthread/thrd.c
regress/lib/libc/iso-c-headers/Makefile
regress/lib/libc/iso-c-headers/joint.c
regress/lib/libc/iso-c-headers/matrix.sh
regress/lib/libc/login_cap/Makefile
regress/lib/libc/login_cap/login_cap.c
regress/lib/libc/stdalign/Makefile
regress/lib/libc/stdalign/stdalign.c
regress/lib/libc/stdatomic/Makefile
regress/lib/libc/stdatomic/stdatomic.c
regress/lib/libc/stdbit/Makefile
regress/lib/libc/stdbit/stdbit.c
regress/lib/libc/stdckdint/Makefile
regress/lib/libc/stdckdint/stdckdint.c
regress/lib/libc/stdnoreturn/Makefile
regress/lib/libc/stdnoreturn/stdnoreturn.c
regress/lib/libc/threads/Makefile
regress/lib/libc/threads/threads.c
```

### Modified source paths

```text
include/Makefile
lib/libc/Makefile.inc
lib/libc/Symbols.list
lib/libc/gen/Makefile.inc
lib/libc/gen/login_cap.3
lib/libc/gen/login_cap.c
lib/libc/shlib_version
lib/libc/thread/Makefile.inc
lib/libpthread/man/Makefile.inc
lib/librthread/Makefile
lib/librthread/Symbols.map
lib/librthread/pthread.h
lib/librthread/rthread.c
lib/librthread/rthread_mutexattr.c
lib/librthread/shlib_version
regress/lib/libc/Makefile
regress/lib/libc/glob/globtest.in
regress/lib/libc/time/time_conversion/timetest.c
```

Host-only changes are confined to libc/tests (Makefile, inclusion driver,
atomic and bit fallback tests, and thread tests). Delivery metadata includes
README.md, REPORT.md, UPSTREAM, SYNC.md, ISO-C-STATUS.md, .gitattributes
and the two patches. The development and build skills document the reusable
header-selection, integer-type, fallback and test-matrix rules. Existing
changes to other components have been preserved.

## Knowledge maintenance review

The development and build skills were reviewed and substantively updated
with the provider, header lookup, atomic type and fallback invariants, and
the unexecuted compiler matrix. The read-only knowledge validator completed
with PASS, zero errors and one AK004 warning requiring manual relevance
review. The guidance above was reviewed for relevance; the validator also
reports pre-existing knowledge edits elsewhere in the dirty worktree.
The validator's own regression suite was not rerun in this static-only audit.

## Architecture and ABI boundary

| Target | Imported dependency review | Build, linking and runtime |
| --- | --- | --- |
| amd64 | Source paths and Git modes checked | Not executed |
| i386 | Source paths and equal-width generic cases reviewed | Not executed |
| arm64 | Source paths and Git modes checked | Not executed |
| armv7 | armv7 plus shared arm headers retained | Not executed |
| riscv64 | Source paths and Git modes checked | Not executed |
| powerpc64 | Source paths; explicit byte-order checks retained | Not executed |
| sparc64 | Source paths; explicit byte-order checks retained | Not executed |

This is an import/dependency inventory, not proof that each target supports
every compiler intrinsic, atomic width or extended integer type. Historical
upstream symbols remain; the local additions retain libc 104.1 and
libpthread 28.2. Corrected public atomic aliases can alter local extension
types and layouts: recompile consumers. Native static/shared linkage and
compiler-generated atomic runtime compatibility remain unverified.

## Final patch verification results

Both patches applied with --fuzz=0 to separate pristine copies of the
official revision, with no offsets, fuzz or rejects. Reconstructed path
sets, file bytes, symlink targets and Git executable modes all matched
the respective intended trees.

| Package | Added | Modified | Deleted | Reconstructed paths |
| --- | --- | --- | --- | --- |
| patch | 35 | 18 | 0 | 3239 |
| iso-c.patch | 33 | 14 | 0 | 3237 |

SHA-256:

```text
a4388570740e1e531d8efc2a8d44d9ec7cc73524e9f97bfbda248a40dad0517d  patch
bbd1a64e03cf5e8e60aa394586d6af607a74e92affb37e831bb50db2c3c3264a  iso-c.patch
```

The unstaged git diff --check passed before new files were indexed. The
staged check additionally reports ten inherited whitespace findings in
newly imported librthread sources: namespace.h:43, rthread.c:413,
rthread_attr.c:183/241, rthread_mutexattr.c:94, rthread_np.c:80,
rthread_rwlockattr.c:59/67, rthread_sched.c:145 and sched_prio.c:44.
Those pristine upstream lines are preserved; they are distinct from local
patch added-line checks and are not a clean staged-whitespace result.
Shell syntax checks passed for the two
host drivers and native header matrix. No merge markers were found in the
modified header, atomic, thread or matrix directories. Temporary source
snapshots, downloaded Git metadata and patch-application workspaces were
removed after recording these results; actual regression sources remain.
No compiler, linker or project runtime test was invoked.
