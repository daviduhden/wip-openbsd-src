/*	$OpenBSD$	*/
/* SPDX-License-Identifier: ISC */

#include <sys/types.h>

#include <err.h>
#include <errno.h>
#include <grp.h>
#include <limits.h>
#include <login_cap.h>
#include <pwd.h>
#include <stdio.h>
#include <unistd.h>

int
main(void)
{
	login_cap_t lc = { 0 };
	struct passwd *pw;
	gid_t groups[NGROUPS_MAX], rgid, egid, sgid;
	uid_t uid;
	int ngroups;

	uid = getuid();
	if (geteuid() == 0 || uid != geteuid()) {
		puts("SKIP: requires an ordinary unprivileged user");
		return (0);
	}
	if (getresgid(&rgid, &egid, &sgid) == -1)
		err(1, "getresgid");
	if (rgid != egid || rgid != sgid) {
		puts("SKIP: requires equal real, effective and saved groups");
		return (0);
	}
	if ((pw = getpwuid(uid)) == NULL)
		errx(1, "getpwuid: no entry for current user");
	if (pw->pw_gid != rgid) {
		puts("SKIP: current group differs from passwd primary group");
		return (0);
	}

	/* Ensure group setup can reach the supplementary-group operation. */
	if (setresgid(rgid, rgid, rgid) == -1)
		err(1, "setresgid to current group");
	if ((ngroups = getgroups(NGROUPS_MAX, groups)) == -1)
		err(1, "getgroups");
	errno = 0;
	if (setgroups(ngroups, groups) != -1 || errno != EPERM)
		errx(1, "unprivileged setgroups must fail with EPERM");

	/* A failed setgroups must not be reported as successful setup. */
	if (setusercontext(&lc, pw, uid, LOGIN_SETGROUP) != -1)
		errx(1, "setusercontext ignored failed setgroups");
	return (0);
}
