---
name: openbsd-source-cvs-diff
description: Prepare OpenBSD contributions as CVS diffs. Use when comparing local work with the official tree, generating cvs diff -uNp, registering added or removed files and directories, checking that a patch applies, splitting independent changes, or preparing a submission for an OpenBSD mailing list.
---

# OpenBSD CVS diff preparation

## When to use

Use when turning local changes into an upstream-ready patch, reviewing a patch
before sending it, or checking that a copied component is complete in CVS.

Do not assume that a Git commit contains everything CVS needs. Git and CVS
represent additions, removals, and directories differently.

## Objective

Produce a complete, minimal, whitespace-correct diff based on OpenBSD-current,
including new files and removals, that applies cleanly to the official tree.

## Prerequisites

- A CVS checkout of the relevant tree (`src` or `xenocara`), based on
  OpenBSD-current.
- `cvs(1)` and `patch(1)`.
- Knowledge of every file added, removed, or renamed.
- A clean baseline: no unrelated local modifications.

## Procedure

1. **Base on current.**
   ```sh
   cd /usr/src
   cvs -q up -Pd -A
   ```
   Do this before comparing; a diff against an old tree will not apply.
2. **Review the tree state.**
   ```sh
   cvs status | grep -E 'Status: (Needs|Unresolved|Locally)'
   cvs diff -uNp
   ```
3. **Register new files and directories.**
   ```sh
   cd /usr/src/bin/pax
   cvs add newfile.c
   cd /usr/src
   cvs add newdir            # register the directory before its files
   ```
   `cvs add` on a file in an unregistered directory fails; add the directory
   first.
4. **Register removals.**
   ```sh
   cvs remove oldfile.c
   ```
   Do not simply delete the working file; CVS must record the removal.
5. **Handle renames as add + remove.** CVS has no rename operation. Preserve
   history with a copy where appropriate and explain the rename in the commit
   message.
6. **Generate the diff.**
   ```sh
   cvs diff -uNp > /tmp/change.diff
   ```
   `-u` unified, `-N` includes new/removed files, `-p` shows the function
   context.
7. **Check completeness.** Every path from `fetch-src.ksh`'s created/deleted
   lists must be represented:
   ```sh
   cat /usr/src/.wip-changes/pax.created.txt
   cat /usr/src/.wip-changes/pax.deleted.txt
   ```
8. **Check application.** In a clean copy of the same revision:
   ```sh
   patch -p1 --dry-run < /tmp/change.diff
   # or, for a git-format patch
   git apply --check /tmp/change.diff
   ```
9. **Split independent changes.** One diff per logical change: build fix,
   behavior fix, documentation, and test should be separable when they can be
   reviewed independently.
10. **Review whitespace and metadata.** No trailing whitespace, no CRLF, no
    editor backup files, no generated binaries, no unrelated mode changes.

## Git mirrors

If the comparison is done in a Git mirror of OpenBSD, the FAQ recommends:

```sh
git config diff.noprefix true
git config diff.renames false
git diff --relative .
```

Do not send a Git patch that renames files implicitly; CVS cannot represent it.
Convert renames to explicit add/remove and explain them.

## Submission conventions

- Send the diff inline in the message; do not use an attachment that a mailer
  may reformat.
- Use plain text; disable flowed text and HTML.
- State the tree (`src` or `xenocara`), the exact files, the problem, the fix,
  and the tests performed.
- Do not mix unrelated cleanup.
- Preserve existing copyright and license text.
- Follow the OpenBSD mailing-list etiquette; `tech@` is the usual list for
  source changes.

## Validation and acceptance

- The diff is based on a current checkout.
- New files appear with `--- /dev/null` or equivalent and `cvs add` was run.
- Removed files appear in the diff and `cvs remove` was run.
- New directories were registered before their files.
- The patch applies cleanly to a pristine copy of the same revision.
- The diff contains no accidental changes, generated files, or binaries.
- `git diff --check` (for the Git-side copy) is clean.

## Common problems and diagnosis

| Symptom | Cause |
|---|---|
| New file missing from `cvs diff` | `cvs add` not run |
| `cvs add: no such file or directory` | Wrong directory or file not created |
| `cvs add: cannot add file on non-branch tag` | Wrong checkout state |
| Removal not in diff | `cvs remove` not run, or file only deleted from disk |
| Patch does not apply | Diff based on a stale checkout or different tree |
| Whitespace mangled in email | HTML/flowed mail; resend as plain text inline |
| Rename appears as delete only | CVS has no rename; add the new path explicitly |

## References

- OpenBSD FAQ 5, Preparing a Diff: <https://www.openbsd.org/faq/faq5.html#Diff>
- OpenBSD Anoncvs: <https://www.openbsd.org/anoncvs.html>
- `cvs(1)`: <https://man.openbsd.org/cvs.1>
- `patch(1)`: <https://man.openbsd.org/patch.1>
- Repository: `FETCHING.md`, `.wip-changes` lists
