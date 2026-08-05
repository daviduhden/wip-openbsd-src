/*
 * fvwm_sandbox.h -- common sandbox helper for fvwm processes.
 *
 * Provides pledge/unveil setup patterns used across the fvwm module set.
 * Each module calls only the helpers it needs; there is no "one size fits
 * all" policy.
 *
 * All pledge/unveil calls check return values and fail with diagnostics.
 *
 * IMPORTANT: Every policy declared here requires verification on a real
 * OpenBSD system with ktrace(1) and a full X11 session.
 */

/*
 * Copyright (c) 2026 David Uhden Collado <david@uhden.dev>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef FVWM_SANDBOX_H
#define FVWM_SANDBOX_H

#include <err.h>

#ifndef FVWMLIBDIR
#define FVWMLIBDIR "/usr/X11R6/lib/X11/fvwm"
#endif

/*
 * sandbox_x11_only -- process that only needs X11 + stdio + fvwm pipes.
 * No filesystem access, no network, no process creation.
 * Used by: FvwmAuto, FvwmBanner, FvwmBacker, FvwmIdent, FvwmIconBox,
 *          FvwmPager, FvwmScroll, FvwmTalk, FvwmWinList
 */
static inline void
sandbox_x11_only(const char *progname)
{
	if (pledge("stdio", NULL) == -1)
		err(1, "%s: pledge stdio", progname);
}

/*
 * sandbox_x11_config -- X11 + read-only config file access.
 * No write, no network, no process creation.
 * Used by: FvwmButtons, FvwmIconMan (after config read),
 *          FvwmForm (after /dev/null open), FvwmRearrange
 */
static inline void
sandbox_x11_config(const char *progname)
{
	if (pledge("stdio rpath", NULL) == -1)
		err(1, "%s: pledge stdio rpath", progname);
}

/*
 * sandbox_save_state -- X11 + write to home directory.
 * No network, no process creation.
 * Used by: FvwmSave, FvwmSaveDesk
 *
 * Unveil is set up BEFORE calling this to restrict to the exact file.
 */
static inline void
sandbox_save_state(const char *progname)
{
	if (pledge("stdio rpath wpath cpath", NULL) == -1)
		err(1, "%s: pledge stdio rpath wpath cpath", progname);
}

/*
 * sandbox_cpp_preproc -- X11 + fork/exec cpp + tmp + dns.
 * Used by: FvwmCpp
 *
 * Unveil is set up BEFORE calling this.
 */
static inline void
sandbox_cpp_preproc(const char *progname)
{
	if (pledge("stdio rpath wpath cpath proc exec dns getpw",
	    NULL) == -1)
		err(1, "%s: pledge", progname);
}

/*
 * sandbox_m4_preproc -- X11 + popen m4 + tmp + dns.
 * Used by: FvwmM4
 *
 * Unveil is set up BEFORE calling this.
 */
static inline void
sandbox_m4_preproc(const char *progname)
{
	if (pledge("stdio rpath wpath cpath proc exec dns getpw",
	    NULL) == -1)
		err(1, "%s: pledge", progname);
}

/*
 * sandbox_xpmroot -- X11-only utility, needs rpath to read image.
 */
static inline void
sandbox_xpmroot(const char *progname)
{
	if (pledge("stdio rpath", NULL) == -1)
		err(1, "%s: pledge stdio rpath", progname);
}

/*
 * sandbox_main_fvwm -- main window manager process.
 * After startup: retains proc for module fork, exec for helper launch,
 * rpath for config reads.
 */
static inline void
sandbox_main_fvwm(const char *progname)
{
	if (unveil(FVWMLIBDIR, "rx") == -1)
		err(1, "%s: unveil %s", progname, FVWMLIBDIR);
	if (unveil("/etc/X11/fvwm", "r") == -1)
		err(1, "%s: unveil /etc/X11/fvwm", progname);
	if (unveil("/tmp", "rwc") == -1)
		err(1, "%s: unveil /tmp", progname);
	if (unveil(NULL, NULL) == -1)
		err(1, "%s: unveil lock", progname);

	if (pledge("stdio rpath proc exec", NULL) == -1)
		err(1, "%s: pledge", progname);
}

/*
 * sandbox_exec_helper -- fvwm_exec helper process.
 * Receives imsg requests, forks children, and execs them.
 */
static inline void
sandbox_exec_helper(const char *progname)
{
	if (pledge("stdio proc exec", NULL) == -1)
		err(1, "%s: pledge", progname);
}

/*
 * sandbox_exec_child -- child of the execution helper, about to exec.
 */
static inline void
sandbox_exec_child(const char *progname)
{
	if (pledge("stdio exec", NULL) == -1)
		err(1, "%s: pledge", progname);
}

/*
 * unveil_tempdir -- unveil the temporary directory (from $TMPDIR or /tmp).
 * For modules that create temp files (FvwmCpp, FvwmM4).
 */
static inline void
unveil_tempdir(const char *progname)
{
	const char *tmp;

	tmp = getenv("TMPDIR");
	if (tmp == NULL)
		tmp = "/tmp";
	if (unveil(tmp, "rwc") == -1)
		err(1, "%s: unveil %s", progname, tmp);
}

/*
 * unveil_home_read -- unveil $HOME for reading only.
 */
static inline void
unveil_home_read(const char *progname)
{
	const char *home;

	home = getenv("HOME");
	if (home == NULL || *home == '\0') {
		warnx("%s: HOME not set, cannot unveil", progname);
		return;
	}
	if (unveil(home, "r") == -1)
		err(1, "%s: unveil %s", progname, home);
}

/*
 * unveil_home_write -- unveil $HOME for read/write/create.
 */
static inline void
unveil_home_write(const char *progname)
{
	const char *home;

	home = getenv("HOME");
	if (home == NULL || *home == '\0') {
		warnx("%s: HOME not set, cannot unveil", progname);
		return;
	}
	if (unveil(home, "rwc") == -1)
		err(1, "%s: unveil %s", progname, home);
}

#endif /* FVWM_SANDBOX_H */
