/*	$OpenBSD: rm.c,v 1.45 2025/04/20 13:47:54 kn Exp $	*/
/*	$NetBSD: rm.c,v 1.19 1995/09/07 06:48:50 jtc Exp $	*/

/*-
 * Copyright (c) 1990, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mount.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <fts.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <pwd.h>
#include <grp.h>

#define MAXIMUM(a, b)	(((a) > (b)) ? (a) : (b))

extern char *__progname;

/* Global flags and state (POSIX semantics preserved; -d, -P, -v are extensions). */
int dflag, eval, fflag, iflag, Pflag, vflag, stdin_ok;

/* Warn only once per run when -P is ineffective on the filesystem. */
static int warned_p_ineffective;

/* Prototypes. */
static int  fs_supports_inplace_overwrite(const struct statfs *);
int	check(char *, char *, struct stat *);
void	checkdot(char **);
void	rm_file(char **);
int	rm_overwrite(char *, struct stat *);
int	pass(int, off_t, char *, size_t);
void	rm_tree(char **);
void	usage(void);

/*
 * rm --
 *	Conforms to POSIX.1-2024 for -f, -i, -R/-r, prompts to stderr,
 *	diagnostics and exit status. Extensions: -d, -P, -v.
 */
int
main(int argc, char *argv[])
{
	int ch, rflag;

	Pflag = rflag = 0;
	while ((ch = getopt(argc, argv, "dfiPRrv")) != -1)
		switch(ch) {
		case 'd':	/* extension */
			dflag = 1;
			break;
		case 'f':
			fflag = 1;
			iflag = 0;
			break;
		case 'i':
			fflag = 0;
			iflag = 1;
			break;
		case 'P':	/* extension */
			Pflag = 1;
			break;
		case 'R':
		case 'r':
			rflag = 1;
			break;
		case 'v':	/* extension */
			vflag = 1;
			break;
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (Pflag) {
		if (pledge("stdio rpath wpath cpath getpw", NULL) == -1)
			err(1, "pledge");
	} else {
		if (pledge("stdio rpath cpath getpw", NULL) == -1)
			err(1, "pledge");
	}

	if (argc < 1 && fflag == 0)
		usage();

	checkdot(argv);

	if (*argv) {
		stdin_ok = isatty(STDIN_FILENO);
		if (rflag)
			rm_tree(argv);
		else
			rm_file(argv);
	}
	return (eval);
}

/*
 * Remove a file hierarchy. POSIX: do not follow symlinks (-R does not
 * dereference), prompt to stderr with -i, ignore ENOENT with -f.
 */
void
rm_tree(char **argv)
{
	FTS *fts;
	FTSENT *p;
	int needstat;
	int flags;

	/* If forcing (-f), or interactive (-i), or cannot ask (!stdin_ok), avoid extra stat. */
	needstat = !fflag && !iflag && stdin_ok;

	/* If -i is specified, allow skipping directories pre-order. */
#define	SKIPPED	1

	flags = FTS_PHYSICAL;
	if (!needstat)
		flags |= FTS_NOSTAT;
	if (!(fts = fts_open(argv, flags, NULL)))
		err(1, NULL);

	while ((p = fts_read(fts)) != NULL) {
		switch (p->fts_info) {
		case FTS_DNR:
			if (!fflag || p->fts_errno != ENOENT) {
				warnc(p->fts_errno, "%s", p->fts_path);
				eval = 1;
			}
			continue;
		case FTS_ERR:
			errc(1, p->fts_errno, "%s", p->fts_path);
		case FTS_NS:
			/* If we can't stat (and we needed to), warn/continue. */
			if (!needstat)
				break;
			if (!fflag || p->fts_errno != ENOENT) {
				warnc(p->fts_errno, "%s", p->fts_path);
				eval = 1;
			}
			continue;
		case FTS_D:
			/* Pre-order prompt for directories with -i. */
			if (!fflag && !check(p->fts_path, p->fts_accpath, p->fts_statp)) {
				(void)fts_set(fts, p, FTS_SKIP);
				p->fts_number = SKIPPED;
			}
			continue;
		case FTS_DP:
			if (p->fts_number == SKIPPED)
				continue;
			break;
		default:
			if (!fflag && !check(p->fts_path, p->fts_accpath, p->fts_statp))
				continue;
		}

		switch (p->fts_info) {
		case FTS_DP:
		case FTS_DNR:
			if (!rmdir(p->fts_accpath)) {
				if (vflag)
					fprintf(stdout, "%s\n", p->fts_path);
				continue;
			}
			if (fflag && errno == ENOENT)
				continue;
			break;

		case FTS_F:
		case FTS_NSOK:
			if (Pflag) {
				/* If overwrite is ineffective/fails, record failure. */
				if (!rm_overwrite(p->fts_accpath, p->fts_info == FTS_NSOK ? NULL : p->fts_statp))
					; /* eval updated inside rm_overwrite() */
			}
			/* FALLTHROUGH */
		default:
			if (!unlink(p->fts_accpath)) {
				if (vflag)
					fprintf(stdout, "%s\n", p->fts_path);
				continue;
			}
			if (fflag && errno == ENOENT)
				continue;
		}
		warn("%s", p->fts_path);
		eval = 1;
	}
	if (errno)
		err(1, "fts_read");
	fts_close(fts);
}

/*
 * Remove a single file (POSIX: error for directories unless -R/-r or -d extension).
 */
void
rm_file(char **argv)
{
	struct stat sb;
	int rval;
	char *f;

	while ((f = *argv++) != NULL) {
		if (lstat(f, &sb)) {
			if (!fflag || errno != ENOENT) {
				warn("%s", f);
				eval = 1;
			}
			continue;
		}

		if (S_ISDIR(sb.st_mode) && !dflag) {
			warnx("%s: is a directory", f);
			eval = 1;
			continue;
		}
		if (!fflag && !check(f, f, &sb))
			continue;
		else if (S_ISDIR(sb.st_mode))
			rval = rmdir(f);
		else {
			if (Pflag) {
				/* If overwrite is ineffective/fails, record failure. */
				if (!rm_overwrite(f, &sb))
					; /* eval updated inside rm_overwrite() */
			}
			rval = unlink(f);
		}
		if (rval && (!fflag || errno != ENOENT)) {
			warn("%s", f);
			eval = 1;
		} else if (rval == 0 && vflag)
			(void)fprintf(stdout, "%s\n", f);
	}
}

/* Return non-zero if the filesystem likely overwrites in place (extensions). */
static int
fs_supports_inplace_overwrite(const struct statfs *fsb)
{
	/* Keep support strictly to "ffs" and "msdos". */
	const char *t = fsb->f_fstypename;
	return (strcmp(t, "ffs") == 0) || (strcmp(t, "msdos") == 0);
}

/*
 * Overwrite a regular file with random data (extension -P).
 * Return 1 to proceed "successfully" (unlink will follow),
 * Return 0 to indicate a failure/ineffectiveness that should
 * contribute to a non-zero exit status.
 *
 * NOTE: Even on failure here, callers still unlink the file to preserve
 * POSIX rm semantics; scripts can detect the issue via exit status.
 */
int
rm_overwrite(char *file, struct stat *sbp)
{
	struct stat sb, sb2;
	struct statfs fsb;
	size_t bsize;
	int fd;
	off_t off;
	char *buf = NULL;

	fd = -1;
	if (sbp == NULL) {
		if (lstat(file, &sb))
			goto err;
		sbp = &sb;
	}
	if (!S_ISREG(sbp->st_mode))
		return (1);
	if (sbp->st_nlink > 1) {
		warnx("%s (inode %llu): not overwritten due to multiple links",
		    file, (unsigned long long)sbp->st_ino);
		eval = 1;
		return (0);
	}
	/* O_NONBLOCK is irrelevant on regular files; avoid it. */
	if ((fd = open(file, O_WRONLY|O_NOFOLLOW)) == -1)
		goto err;
	if (fstat(fd, &sb2))
		goto err;
	if (sb2.st_dev != sbp->st_dev || sb2.st_ino != sbp->st_ino || !S_ISREG(sb2.st_mode)) {
		errno = EPERM;
		goto err;
	}
	if (fstatfs(fd, &fsb) == -1)
		goto err;

	/* If -P is ineffective on this FS, warn once and mark failure. */
	if (!fs_supports_inplace_overwrite(&fsb)) {
		if (!warned_p_ineffective) {
			warnx("%s: -P is not effective on %s; overwrite skipped (command will fail)",
			    file, fsb.f_fstypename);
			warned_p_ineffective = 1;
		}
		close(fd);
		eval = 1;           /* ensure non-zero exit status */
		errno = ENOTSUP;    /* best-effort indicator */
		return (0);
	}

	bsize = MAXIMUM(fsb.f_iosize, 1024U);
	if ((buf = malloc(bsize)) == NULL)
		err(1, "%s: malloc", file);

	off = lseek(fd, 0, SEEK_SET);
	if (off == (off_t)-1)
		goto err;

	if (!pass(fd, sbp->st_size, buf, bsize))
		goto err;
	if (fsync(fd))
		goto err;
	close(fd);
	free(buf);
	return (1);

err:
	warn("%s", file);
	close(fd);
	eval = 1;
	free(buf);
	return (0);
}

/* Write random data across the file; tolerant to short writes and EINTR. */
int
pass(int fd, off_t len, char *buf, size_t bsize)
{
	size_t chunk;
	ssize_t wr;
	char *p;

	while (len > 0) {
		chunk = (size_t)(len < (off_t)bsize ? len : (off_t)bsize);
		arc4random_buf(buf, chunk);
		p = buf;
		while (chunk > 0) {
			wr = write(fd, p, chunk);
			if (wr < 0) {
				if (errno == EINTR)
					continue;
				return (0);
			}
			if (wr == 0) {
				errno = EIO; /* No forward progress. */
				return (0);
			}
			p    += (size_t)wr;
			chunk -= (size_t)wr;
			len  -= (off_t)wr;
		}
	}
	return (1);
}

/* Query the user if needed. Prompts go to stderr per POSIX. */
int
check(char *path, char *name, struct stat *sp)
{
	int ch, first;
	char modep[15];

	if (iflag)
		(void)fprintf(stderr, "remove %s? ", path);
	else {
		/* If not a symlink and unwritable and on a terminal, ask. */
		if (!stdin_ok || S_ISLNK(sp->st_mode) || !access(name, W_OK) || errno != EACCES)
			return (1);
		strmode(sp->st_mode, modep);
		(void)fprintf(stderr, "override %s%s%s/%s for %s? ",
		    modep + 1, modep[9] == ' ' ? "" : " ",
		    user_from_uid(sp->st_uid, 0),
		    group_from_gid(sp->st_gid, 0), path);
	}
	(void)fflush(stderr);

	first = ch = getchar();
	while (ch != '\n' && ch != EOF)
		ch = getchar();
	return (first == 'y' || first == 'Y');
}

/*
 * Reject "/" and the operands "." and ".." as required by POSIX.
 * Strip trailing slashes before basename inspection.
 */
#define ISDOT(a) ((a)[0] == '.' && (!(a)[1] || ((a)[1] == '.' && !(a)[2])))
void
checkdot(char **argv)
{
	char *p, **save, **t;
	int complained;
	struct stat sb, root;

	stat("/", &root);
	complained = 0;
	for (t = argv; *t;) {
		if (lstat(*t, &sb) == 0 && root.st_ino == sb.st_ino && root.st_dev == sb.st_dev) {
			if (!complained++)
				warnx("\"/\" may not be removed");
			goto skip;
		}
		/* Strip trailing slashes. */
		p = strrchr(*t, '\0');
		while (--p > *t && *p == '/')
			*p = '\0';

		/* Extract basename. */
		if ((p = strrchr(*t, '/')) != NULL)
			++p;
		else
			p = *t;

		if (ISDOT(p)) {
			if (!complained++)
				warnx("\".\" and \"..\" may not be removed");
skip:
			eval = 1;
			for (save = t; (t[0] = t[1]) != NULL; ++t)
				continue;
			t = save;
		} else
			++t;
	}
}

void
usage(void)
{
	/* POSIX synopsis: [-fiRr] file ... ; extensions shown but non-mandatory. */
	(void)fprintf(stderr, "usage: %s [-dfiPRrv] file ...\n", __progname);
	exit(1);
}
