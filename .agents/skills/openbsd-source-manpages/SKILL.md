---
name: openbsd-source-manpages
description: Write and review OpenBSD manual pages in mdoc(7). Use when documenting a new option or behavior, synchronizing a page with code, adding .Xr references, fixing section order or dates, or validating pages with mandoc -T lint.
---

# OpenBSD manual pages

## When to use

Use whenever a user-visible option, default, file, error, or security behavior
changes, or when adding a new manual page.

Do not use to rewrite imported pages wholesale or to convert legacy FVWM
module pages to mdoc without a separate, reviewed change.

## Objective

Keep the manual page an accurate, lint-clean description of the implemented
behavior, using OpenBSD mdoc conventions.

## Prerequisites

- The implementation and its option parsing are available.
- The page's current `.Dd`, `.Dt`, `.Os`, and section order are known.
- `mandoc` is installed.
- For a new page, the correct section (1, 3, 5, 8) is identified.

## Procedure

1. **Read the implementation and the existing page together.** List every
   option, argument, default, file, environment variable, exit status, and
   error message the change affects.
2. **Update the SYNOPSIS.** Every new flag appears in the correct `.Op`/`.Fl`
   group; mutually exclusive forms are separate synopsis lines.
3. **Document the option in DESCRIPTION.** State the effect, the default, and
   any interaction with other flags. Do not document an option that the parser
   does not implement.
4. **Use mdoc markup.**
   - `.Nm`, `.Nd`, `.Sh`, `.Ss`, `.Pp`
   - `.Op`, `.Fl`, `.Ar`, `.Cm`, `.Pa`, `.Ev`, `.Dv`, `.Va`
   - `.Bl -tag -width Ds` / `.It` / `.El`
   - `.Xr name section` for cross references
   - `.Ex -std` for standard exit status
5. **Set the date.** In this Git repository use a literal date:
   ```mdoc
   .Dd October 24, 2026
   ```
   `$Mdocdate$` only expands in CVS and renders as the current date in Git.
6. **Keep section order conventional.** A typical page is NAME, SYNOPSIS,
   DESCRIPTION, ENVIRONMENT, FILES, EXIT STATUS, EXAMPLES, DIAGNOSTICS,
   SEE ALSO, STANDARDS, HISTORY, AUTHORS, CAVEATS, SECURITY CONSIDERATIONS.
   Custom sections such as COMPATIBILITY belong before SEE ALSO, and SECURITY
   CONSIDERATIONS belongs last.
7. **Check cross references.** `.Xr` entries should be ordered by section, then
   name. Do not reference a page that does not exist in the target tree.
8. **Update HISTORY/AUTHORS only if the change affects them.** Do not invent a
   first-appearance version.
9. **Validate.**
   ```sh
   mandoc -T lint page.1
   make manlint
   mandoc -T ascii page.1 | less
   ```
10. **Check the rendered output.** Tables, examples, and long lines must remain
    readable at 80 columns.

## Repository-specific notes

- `pax/pax.1`, `pax/tar.1`, `pax/cpio.1` are mdoc. `pax.1` documents the local
  `-o` extensions (`invalid=`, `linkdata`, `listopt=`, `times`).
- `pax -P` and `tar -P` have different meanings; keep them distinct.
- `fvwm/fvwm/fvwm2.1` and `fvwm_exec.1` are mdoc; the 17 module pages under
  `fvwm/modules/*/` and `fvwm/utils/xpmroot.1` are legacy man(7). Do not mix
  markup in a single change.
- `ext4fs/sbin/mount_ext4fs/mount_ext4fs.8` is mdoc; its HISTORY must name
  OpenBSD, not FreeBSD.
- New libc pages (`stdalign.3`, `stdatomic.3`, `stdbit.3`, `stdckdint.3`,
  `threads.3`, `stdnoreturn.3`) use literal dates in this tree.

## Validation and acceptance

- `mandoc -T lint` reports no ERROR or WARNING caused by the change.
- Host `man.db` "referenced manual not found" style messages are environmental,
  not source defects; state that if they appear.
- Every documented flag is implemented; every implemented flag is documented.
- The date is literal and reflects the last substantive change.
- Section order is conventional and `.Xr` ordering is correct.
- Examples use the real program name and real flags.

## Common problems and diagnosis

| Symptom | Fix |
|---|---|
| `unknown standard specifier: St -p1003.1-2024` | Old mandoc database; verify with current mandoc before changing the page |
| `sections out of conventional order` | Move custom sections before SEE ALSO; keep SECURITY last |
| `unusual Xr order` | Sort by section number, then name |
| `new sentence, new line` | Put each sentence on its own input line |
| `referenced manual not found` | Host `man.db` gap; do not add a fake `.Xr` |
| Date changes every render | Replace `$Mdocdate$` with a literal date |

## References

- `mdoc(7)`: <https://man.openbsd.org/mdoc.7>
- `mandoc(1)`: <https://man.openbsd.org/mandoc.1>
- `man(7)`: <https://man.openbsd.org/man.7>
- `style(9)`: <https://man.openbsd.org/style.9>

## Maintenance & Continuous Learning

Whenever this skill is used to change the repository, review this skill after
the implementation. Correct procedures that proved wrong, update commands,
paths, configuration and examples, incorporate reusable findings, and remove
outdated assumptions. Review the applicable root and nested `AGENTS.md` files,
specialized agents and related skills; update only the knowledge affected by
the work. Validate local references and run
`python3 scripts/validate-agent-knowledge.py --scope worktree` plus the
applicable project checks. For pull request comparisons, use the documented
`--scope range --base BASE_REF --head HEAD_REF` command. The canonical validator
is read-only and is required even when a task restricts execution of project
scripts; this does not authorize running installers or changing the host.
Report checks actually performed. A structural pass does not prove semantic
relevance, and a task is incomplete until synchronization is validated.
