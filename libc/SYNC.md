# Incremental libc synchronization

Use this procedure for every OpenBSD-current update. The effective BASE is
**the last synchronized revision** in UPSTREAM, not the original import
revision or the first SHA found in that file. Pin NEW and record its origin,
commit date and observation time before editing. Preserve the complete LOCAL
worktree, including uncommitted edits; Git HEAD alone is not LOCAL.

## Imported source roots

All paths are relative to OpenBSD src:

- `include/`
- `lib/libc/`
- `lib/libpthread/` (manual pages)
- `lib/librthread/` (`LIB=pthread`, lifecycle and attribute implementation)
- `regress/lib/libc/`
- `sys/sys/`
- `sys/arch/{amd64,i386,arm,arm64,armv7,riscv64,powerpc64,sparc64}/include/`

The shared arm headers are dependencies of armv7. Import only these roots;
do not expand to the complete operating-system tree. Project reports,
metadata, host `tests/` and this procedure do not belong in the source patch.

## Obtain BASE, history and NEW

Use an external Git working directory with official origin
`https://github.com/openbsd/src`, blob filtering and sparse checkout. For a
new clone, `git clone --filter=blob:none --depth=1 --sparse` is suitable.
Select the roots above with `git sparse-checkout set`. Deepen the clone far
enough to contain BASE and every intervening commit; a pair of snapshots is
not sufficient history evidence. Check `git merge-base --is-ancestor BASE
NEW`, `git rev-list --count BASE..NEW`, and `git log BASE..NEW -- ROOTS`.
Inspect renames, deletions and changes to header dependencies. Record all
commands and results without presenting inspection as compilation.

Keep origin and transport distinct: a user Git URL rewrite may use SSH for
an HTTPS origin. Do not alter global Git configuration or identity. Do not
change /usr/src, /usr/xenocara or the installed system during this procedure.

## Integrate semantically

1. Extract pristine BASE and NEW archives into separate temporary trees.
2. Compare BASE with saved LOCAL to inventory every local addition, change
   and deletion. Check source, symbols, hidden aliases, build rules, manuals
   and regression tests together.
3. Classify each local delta: keep, adapt, upstream-equivalent, semantic
   conflict, obsolete, or simplifiable. Demonstrate equivalence before
   removing any local functionality. Preserve licenses and revision tags.
4. For each upstream change compare BASE/LOCAL/NEW. A local file equal to
   BASE can take NEW directly; otherwise perform a three-way merge and
   review its semantics even when there is no textual conflict. Never use a
   global ours/theirs strategy or reset dirty LOCAL files to an archive.
5. Check actual providers. OpenBSD splits pthread support between libc and
   lib/librthread; lib/libpthread is a manual-page directory. Internal
   pthread calls need matching PROTO_NORMAL and DEF_STD aliases. Do not
   silence namespace deprecation warnings globally to conceal a bad split.
6. Preserve all six ISO headers and tests. Inspect cdefs/visibility, C23
   keyword guards, single evaluation, memory ordering, integer widths,
   endianity and alignment for each imported architecture. Source inclusion
   alone is not evidence that all generic macro expansions work.

## Export and reconstruct

Generate `patch` against **pristine NEW**, for the union of imported paths
and local additions. Use `Index:` headers and `diff -u -p` function context;
use `/dev/null` for additions/deletions and source-relative filenames.
Include all legitimate local changes, excluding upstream-only changes.
Traditional patches do not encode executable bits or symlinks: inventory
those explicitly and reject unrepresented metadata changes. Preserve any
existing functionality-specific patches as well as this complete patch.

Apply the complete patch to a fresh copy of NEW, for example with
`patch -p0 -d STAGE < patch`. Require exact application without fuzz, offsets
or rejects. Compare every imported path, bytes, symlink targets and Git file
modes with LOCAL, including unchanged upstream paths and extra/missing files.
An apply check without reconstruction comparison is insufficient. The
container's whitespace attributes exempt structural context prefixes only;
check actual added source lines for whitespace defects separately.

Only after reconstruction succeeds may UPSTREAM advance to NEW. Update
README, the synchronization report, applicable agent instructions and skills.
Inspect git diff --cached --check after staging new imported files: an
unstaged diff cannot inspect untracked imports. Preserve and document
inherited upstream whitespace separately from local additions rather than
reformatting pristine sources or claiming a clean staged check.

Run the read-only knowledge validator and inspect git diff --check and the
final status. Preserve unrelated pre-existing changes and compare against
the initial worktree, not against a falsely assumed clean repository.

## Evidence and restrictions

Use a native OpenBSD build and regress environment when available and
permitted. Host/cross compilation does not establish native linking or
execution. Header matrices must check this tree's guards in every language
mode and C++ case; compiler resource headers must not accidentally satisfy
them. Use private mktemp directories for diagnostics.

If compilation is prohibited, do not invoke compilers even with
-fsyntax-only. Continue history, source, symbol-map, script-syntax, mdoc,
patch-application, reconstruction and metadata validation. Mark added tests
as unexecuted and record static/shared native link checks as pending.


When refining ISO interfaces, use public library integer typedefs for the
corresponding atomic types, rather than compiler guesses about fast/max
aliases. Keep private namespace wrappers before the public source include
path and that path before compiler resources. A clean include-only test does
not establish usable generic macros, native linkage or compiler atomic ABI.
Maintain `iso-c.patch` as an alternative focused ISO snapshot against the
same pinned upstream, excluding unrelated local fixes. Verify it separately
against its intended feature tree; `patch` remains the complete local delta.
