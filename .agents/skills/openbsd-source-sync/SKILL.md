---
name: openbsd-source-sync
description: Synchronize local components with OpenBSD CVS checkouts using fetch-src.ksh. Use when listing components, copying pax or fvwm into /usr/src or /usr/xenocara, inspecting backups and change lists, resolving a failed copy, or updating a checkout from OpenBSD-current. Distinguishes safe listing from destructive checkout and provisioning.
---

# OpenBSD source synchronization

## When to use

Use when moving `pax/` or `fvwm/` into a real checkout, inspecting
`fetch-src.ksh`, recovering from a copy, fetching the website tree (`www`)
into `/usr/www`, or planning an update from OpenBSD-current.

Do not use this skill to run a destructive mode without explicit authorization.
Do not use it for `libc/`, `ext4fs/`, or `stdckdint/`; those are patch-style
components handled separately.

## Objective

Copy local component sources into the correct tree without destroying CVS
metadata, without losing the previous version, and with a verifiable record of
added and removed paths.

## Prerequisites

- An OpenBSD host; local copies require `/usr/src` and/or `/usr/xenocara`
  checked out from CVS. Website checkout uses `/usr/www`.
- Write access to the target tree; `doas` for system paths.
- A clean understanding of the difference between:
  - `--list`: read-only;
  - `--copy-only TREE [component...]`: replaces one component, keeps a backup;
  - default and `--no-provision`: stage a fresh CVS checkout and replace
    the selected tree after confirmation, retaining the previous tree.

## Modes

| Invocation | Writes | Scope |
|---|---|---|
| `./fetch-src.ksh --list` | No | List the two local components |
| `./fetch-src.ksh --copy-only TREE [component ...]` | Yes | Replaces matching components and retains backups |
| `doas ./fetch-src.ksh` | Yes | Checks out and replaces `src`, `xenocara`, or `www` |
| `doas ./fetch-src.ksh --no-provision` | Yes | Alias for the interactive checkout |

The checkout uses a sibling staging directory under `/usr`. Existing trees
require confirmation. On success the previous tree is saved at the printed
backup location. On failure before replacement the existing tree remains
available. Shell profile edits, accounts, and doas configuration belong
outside this workflow. The website tree has no local replacement component.

Mapping:

- `pax` or `src/bin/pax` -> `TREE/bin/pax`
- `fvwm` or `xenocara/app/fvwm` -> `TREE/app/fvwm`

The tree is identified by shape: `bin/` + `sys/` means `src`; `app/` + `lib/`
means `xenocara`. A symlinked root, parent, component, or `.wip-*` directory is
rejected.

## Procedure

1. **Inspect before touching anything.**
   ```sh
   ./fetch-src.ksh --list
   ksh -n ./fetch-src.ksh          # syntax only
   ```
2. **Confirm the target tree and component.**
   ```sh
   test -f /usr/src/Makefile && test -d /usr/src/bin && test -d /usr/src/sys
   test -f /usr/xenocara/Makefile && test -d /usr/xenocara/app
   ```
3. **Review local changes in the target component.** If `/usr/src/bin/pax` has
   changes not present in this repository, save them first.
4. **Copy one component.**
   ```sh
   doas ./fetch-src.ksh --copy-only /usr/src src/bin/pax
   doas ./fetch-src.ksh --copy-only /usr/xenocara xenocara/app/fvwm
   ```
   Short names (`pax`, `fvwm`) are equivalent.
5. **Read the change lists.**
   ```sh
   cat /usr/src/.wip-changes/pax/created.txt
   cat /usr/src/.wip-changes/pax/deleted.txt
   cat /usr/src/.wip-changes/pax/created-dirs.txt
   ```
   They contain component-relative paths, CVS metadata excluded. Symlinks are
   now listed alongside regular files.
6. **Verify the copy.**
   ```sh
   cd /usr/src/bin/pax && cvs diff -uNp
   find . -type d -name CVS -prune -o -print | head
   ```
7. **Register additions and removals in CVS** using
   `openbsd-source-cvs-diff`. `fetch-src.ksh` never runs `cvs add/remove`.
8. **Recover from a failed copy.** The previous component is at
   `TREE/.wip-backups/NAME.XXXXXXXX/component`. A failed final rename attempts
   automatic rollback; if rollback also failed, the error names the backup.
   Restore with `mv` only after verifying the path and the backup contents.

## Safe and destructive commands

```sh
# Safe
./fetch-src.ksh --list

# Replaces one component; recoverable backup
doas ./fetch-src.ksh --copy-only /usr/src pax

# Replaces the selected checkout and retains a backup
doas ./fetch-src.ksh --no-provision

```

Use read-only listing or static review for validation that does not require
replacing a checkout. The checkout stages CVS contents before moving the live
tree. Component copies stage both the source and its manifests before moving
the live directories. After an interrupted publication, inspect the live
paths and the printed backup location together before restoring either.

## Updating from OpenBSD-current

`fetch-src.ksh` performs a fresh `cvs checkout -P` of HEAD; it does not merge.
For an existing tree, update with CVS and then re-apply local components:

```sh
cd /usr/src && cvs -q up -Pd -A
cd /usr/xenocara && cvs -q up -Pd -A
cd /usr/www && cvs -q up -Pd -A
```

Resolve CVS conflicts before copying local components over the tree. Record the
upstream revision used for any comparison.

## Validation and acceptance

- `--list` prints the expected two mappings and writes nothing.
- `www` checkout uses `/usr/www`, rejects symlinked or mounted destinations and preserves the old tree on
  CVS failure, and never prompts to copy local components. Validate these
  branches statically when execution or system writes are prohibited; do
  not run a destructive checkout as a test.
- The target component exists before and after the copy.
- `TREE/.wip-backups/NAME.*/component` contains the previous version.
- `TREE/.wip-changes/NAME/created.txt`, `deleted.txt`, and `created-dirs.txt`
  exist when the target component existed before.
- Nested `CVS/` directories are still present under the target.
- No sibling component or parent `Makefile` was removed.
- A `cvs diff -uNp` in the component shows only intended changes.

## Common problems and diagnosis

| Symptom | Cause and action |
|---|---|
| `Not a source checkout` | Wrong TREE; point at the checkout root, not a subdirectory |
| `Cannot identify src or xenocara tree` | Tree shape is wrong; verify `bin/`+`sys/` or `app/`+`lib/` |
| `Unknown component` | Use `pax`, `fvwm`, `src/bin/pax`, or `xenocara/app/fvwm` |
| Copy says success but build fails | The repository's standalone `Makefile` replaced the upstream one; review `FETCHING.md` |
| New files missing from `cvs diff` | Run `cvs add` from `created.txt`; directories first |
| Deleted files still in CVS | Run `cvs remove` from `deleted.txt` |
| Rollback message | Inspect the named backup before moving anything |

## References

- `FETCHING.md`
- `fetch-src.ksh`
- OpenBSD Anoncvs: <https://www.openbsd.org/anoncvs.html>
- OpenBSD FAQ 5: <https://www.openbsd.org/faq/faq5.html>
- `cvs(1)`: <https://man.openbsd.org/cvs.1>

## Maintenance & Continuous Learning

Whenever this skill is used to change the repository, review this skill after
the implementation. Correct procedures that proved wrong, update commands,
paths, configuration and examples, incorporate reusable findings, and remove
outdated assumptions. Review the applicable root and nested `AGENTS.md` files,
specialized agents and related skills; update only the knowledge affected by
the work. Validate local references and run
`perl scripts/validate-agent-knowledge.pl --scope worktree` plus the
applicable project checks. For committed comparisons, use the documented
`--scope range --base BASE_REF --head HEAD_REF` command. The canonical validator
is read-only and is required even when a task restricts execution of project
scripts; this does not authorize running installers or changing the host.
Report checks actually performed. A structural pass does not prove semantic
relevance, and a task is incomplete until synchronization is validated.

For component copies, the Git working copy supplies tracked and unignored
paths. Stage intended deletions before copying. Use the manifest directory
for the installed component generation, with the previous set held with the
backup. Inspect locks and backups after any interrupted operation before
manual cleanup.
