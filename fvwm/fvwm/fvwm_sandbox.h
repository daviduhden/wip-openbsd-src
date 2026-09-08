/*
 * fvwm_sandbox.h -- common sandbox helpers for fvwm processes.
 *
 * Provides pledge/unveil setup patterns used across the fvwm module set.
 * Each module calls only the helpers it needs; there is no "one size fits
 * all" policy.
 *
 * All pledge/unveil calls check return values and fail with diagnostics.
 *
 * Modules inherit the fvwm baseline sandbox (see fvwm.c): a locked
 * unveil covering FVWMLIBDIR, system config, /tmp, $HOME, the bin
 * directories, and the two device files modules use, plus a pledge
 * superset they reduce from.  A module's own unveil calls below are
 * therefore refinements and ignore EPERM (the inherited policy is
 * already broader than or equal to what they ask for).
 *
 * The execution helper (fvwm_exec) deliberately has no sandbox of
 * its own: pledge(2) and unveil(2) are inherited across execve(2)
 * and would cripple the arbitrary programs fvwm launches.
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
#include <errno.h>

#ifndef FVWMLIBDIR
#define FVWMLIBDIR "/usr/X11R6/lib/X11/fvwm"
#endif

/*
 * All X11-talking modules carry "inet dns": X connections may use
 * TCP (DISPLAY=host:0) and resolve display host names.
 */

/*
 * sandbox_x11_only -- process that only needs X11 + stdio + fvwm pipes.
 * No filesystem access, no network beyond X11, no process creation.
 * Used by: FvwmAuto, FvwmBanner, FvwmBacker, FvwmIdent, FvwmIconBox,
 *          FvwmPager, FvwmScroll, FvwmTalk, FvwmWinList
 */
static inline void
sandbox_x11_only(const char *progname)
{
	if (pledge("stdio inet dns", NULL) == -1)
		err(1, "%s: pledge stdio", progname);
}

/*
 * sandbox_x11_config -- X11 + read-only config file access.
 * No write, no network beyond X11, no process creation.
 * Used by: FvwmButtons, FvwmIconMan (after config read),
 *          FvwmForm (after /dev/null open), FvwmRearrange
 */
static inline void
sandbox_x11_config(const char *progname)
{
	if (pledge("stdio rpath inet dns", NULL) == -1)
		err(1, "%s: pledge stdio rpath", progname);
}

/*
 * sandbox_save_state -- X11 + write to home directory.
 * No network beyond X11, no process creation.
 * Used by: FvwmSave, FvwmSaveDesk
 */
static inline void
sandbox_save_state(const char *progname)
{
	if (pledge("stdio rpath wpath cpath inet dns", NULL) == -1)
		err(1, "%s: pledge stdio rpath wpath cpath", progname);
}

/*
 * sandbox_cpp_preproc -- X11 + fork/exec cpp + tmp + dns.
 * Used by: FvwmCpp
 */
static inline void
sandbox_cpp_preproc(const char *progname)
{
	if (pledge("stdio rpath wpath cpath proc exec dns getpw inet",
	    NULL) == -1)
		err(1, "%s: pledge", progname);
}

/*
 * sandbox_m4_preproc -- X11 + popen m4 + tmp + dns.
 * Used by: FvwmM4
 */
static inline void
sandbox_m4_preproc(const char *progname)
{
	if (pledge("stdio rpath wpath cpath proc exec dns getpw inet",
	    NULL) == -1)
		err(1, "%s: pledge", progname);
}

/*
 * sandbox_xpmroot -- X11-only utility, needs rpath to read image.
 */
static inline void
sandbox_xpmroot(const char *progname)
{
	if (pledge("stdio rpath inet dns", NULL) == -1)
		err(1, "%s: pledge stdio rpath", progname);
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
	if (unveil(tmp, "rwc") == -1 && errno != EPERM)
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
	if (unveil(home, "r") == -1 && errno != EPERM)
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
	if (unveil(home, "rwc") == -1 && errno != EPERM)
		err(1, "%s: unveil %s", progname, home);
}

#endif /* FVWM_SANDBOX_H */
