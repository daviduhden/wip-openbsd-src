/*	$OpenBSD: gen_subs.c,v 1.34 2024/04/27 19:49:42 florian Exp $	*/
/*	$NetBSD: gen_subs.c,v 1.5 1995/03/21 09:07:26 cgd Exp $	*/

/*-
 * Copyright (c) 1992 Keith Muller.
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Keith Muller of the University of California, San Diego.
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
#include <ctype.h>
#include <grp.h>
#include <libgen.h>
#include <limits.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <utmp.h>
#include <vis.h>

#include "pax.h"
#include "extern.h"

/*
 * a collection of general purpose subroutines used by pax
 */

/* Accumulated format string for -o listopt=. */
static char *listopt_format;
static size_t listopt_len;

/* Scratch context tracking dynamically duplicated strings. */
struct listopt_ctx {
	char **allocated;
	size_t count;
	size_t cap;
};

/* Parsed elements for a single custom listopt conversion. */
struct listopt_spec {
	char flags[16];
	char width[16];
	char precision[16];
	char length[8];
	char keyword[128];
	char subfmt[128];
	char conv;
};

static void listopt_output(ARCHD *, FILE *);

static void
listopt_ctx_init(struct listopt_ctx *ctx)
{
	ctx->allocated = NULL;
	ctx->count = ctx->cap = 0;
}

static void
listopt_ctx_free(struct listopt_ctx *ctx)
{
	size_t i;

	if (ctx == NULL)
		return;
	for (i = 0; i < ctx->count; i++)
		free(ctx->allocated[i]);
	free(ctx->allocated);
	ctx->allocated = NULL;
	ctx->count = ctx->cap = 0;
}

static const char *
listopt_store(struct listopt_ctx *ctx, const char *str)
{
	char *dup;
	char **tmp;

	if (str == NULL)
		str = "";
	dup = strdup(str);
	if (dup == NULL)
		return "";
	if (ctx->count == ctx->cap) {
		size_t newcap = ctx->cap ? ctx->cap * 2 : 8;
		tmp = reallocarray(ctx->allocated, newcap, sizeof(*tmp));
		if (tmp == NULL) {
			free(dup);
			return "";
		}
		ctx->allocated = tmp;
		ctx->cap = newcap;
	}
	ctx->allocated[ctx->count++] = dup;
	return dup;
}

/* Break down a single % conversion, recording printf modifiers and keyword. */
static int
listopt_parse_spec(const char *fmt, struct listopt_spec *spec,
    const char **endp)
{
	const char *p = fmt;
	enum { PARSE_FLAGS, PARSE_WIDTH, PARSE_PRECISION, PARSE_LENGTH } state;

	if (*p != '%')
		return 0;
	memset(spec, 0, sizeof(*spec));
	p++;
	state = PARSE_FLAGS;
	while (*p != '\0') {
		if (*p == '(') {
			const char *start = ++p;
			size_t len;

			while (*p != '\0' && *p != ')')
				p++;
			len = p - start;
			if (len >= sizeof(spec->keyword))
				len = sizeof(spec->keyword) - 1;
			memcpy(spec->keyword, start, len);
			spec->keyword[len] = '\0';
			if (*p == ')')
				p++;
			state = PARSE_FLAGS;
			continue;
		}
		switch (state) {
		case PARSE_FLAGS:
			if (strchr("-+ #0'", *p) != NULL) {
				size_t fl = strlen(spec->flags);
				if (fl + 1 < sizeof(spec->flags)) {
					spec->flags[fl] = *p;
					spec->flags[fl + 1] = '\0';
				}
				p++;
				continue;
			}
			state = PARSE_WIDTH;
			continue;
		case PARSE_WIDTH:
			if (isdigit((unsigned char)*p)) {
				size_t wl = strlen(spec->width);
				if (wl + 1 < sizeof(spec->width)) {
					spec->width[wl] = *p;
					spec->width[wl + 1] = '\0';
				}
				p++;
				continue;
			}
			if (*p == '.') {
				size_t pl = strlen(spec->precision);
				if (pl + 1 < sizeof(spec->precision)) {
					spec->precision[pl] = '.';
					spec->precision[pl + 1] = '\0';
				}
				p++;
				state = PARSE_PRECISION;
				continue;
			}
			state = PARSE_LENGTH;
			continue;
		case PARSE_PRECISION:
			if (isdigit((unsigned char)*p)) {
				size_t pl = strlen(spec->precision);
				if (pl + 1 < sizeof(spec->precision)) {
					spec->precision[pl] = *p;
					spec->precision[pl + 1] = '\0';
				}
				p++;
				continue;
			}
			state = PARSE_LENGTH;
			continue;
		case PARSE_LENGTH:
			if (strchr("hljztL", *p) != NULL) {
				size_t ll = strlen(spec->length);
				if (ll + 1 < sizeof(spec->length)) {
					spec->length[ll] = *p;
					spec->length[ll + 1] = '\0';
				}
				p++;
				/* Support double h/l modifiers */
				if ((spec->length[0] == 'h' || spec->length[0] == 'l') &&
				    spec->length[1] == '\0' && (*p == spec->length[0])) {
					if (strlen(spec->length) + 1 < sizeof(spec->length)) {
						size_t l2 = strlen(spec->length);
						spec->length[l2] = *p;
						spec->length[l2 + 1] = '\0';
					}
					p++;
				}
				continue;
			}
			spec->conv = *p++;
			goto done;
		}
	}
	done:
	if (spec->conv == '\0')
		return -1;
	if (spec->keyword[0] != '\0' && spec->conv == 'T') {
		char *eq = strchr(spec->keyword, '=');
		if (eq != NULL) {
			strlcpy(spec->subfmt, eq + 1, sizeof(spec->subfmt));
			*eq = '\0';
		}
	}
	*endp = p;
	return 1;
}

/* Retrieve a keyword value as a string, allocating stable storage as needed. */
static const char *
listopt_keyword_string(struct listopt_ctx *ctx, ARCHD *arcn,
    const char *keyword)
{
	const char *val;
	char *dup;

	if (keyword == NULL || *keyword == '\0' ||
	    strcmp(keyword, "path") == 0)
		return arcn->name;
	if (strcmp(keyword, "linkpath") == 0)
		return arcn->ln_name;
	if (strcmp(keyword, "uname") == 0) {
		val = user_from_uid(arcn->sb.st_uid, 0);
		return val ? val : "";
	}
	if (strcmp(keyword, "gname") == 0) {
		val = group_from_gid(arcn->sb.st_gid, 0);
		return val ? val : "";
	}
	if (strcmp(keyword, "name") == 0) {
		if ((dup = strdup(arcn->name)) == NULL)
			return "";
		val = listopt_store(ctx, basename(dup));
		free(dup);
		return val;
	}
	if (strcmp(keyword, "dirname") == 0) {
		if ((dup = strdup(arcn->name)) == NULL)
			return "";
		val = listopt_store(ctx, dirname(dup));
		free(dup);
		return val;
	}
	val = pax_kv_lookup(arcn, keyword);
	return val ? val : "";
}

/* Interpret a keyword as a timespec, falling back to archive defaults. */
static int
listopt_keyword_time(struct listopt_ctx *ctx, ARCHD *arcn,
    const char *keyword, struct timespec *ts)
{
	const char *val;
	char *end;

	if (keyword == NULL || *keyword == '\0' ||
	    strcmp(keyword, "mtime") == 0) {
		*ts = arcn->sb.st_mtim;
		return 0;
	}
	if (strcmp(keyword, "atime") == 0) {
		*ts = arcn->sb.st_atim;
		return 0;
	}
	if (strcmp(keyword, "ctime") == 0) {
		*ts = arcn->sb.st_ctim;
		return 0;
	}
	val = pax_kv_lookup(arcn, keyword);
	if (val == NULL || *val == '\0')
		return -1;
	ts->tv_sec = strtoll(val, &end, 10);
	ts->tv_nsec = 0;
	if (end == val)
		return -1;
	if (*end == '.') {
		long nsec = 0;
		int digits = 0;
		for (end++; *end && isdigit((unsigned char)*end) && digits < 9;
		    end++, digits++)
			nsec = nsec * 10 + (*end - '0');
		for (; digits < 9; digits++)
			nsec *= 10;
		ts->tv_nsec = nsec;
	}
	return 0;
}

/* Parse signed numeric keywords, allowing overrides from extended headers. */
static int
listopt_keyword_sll(ARCHD *arcn, const char *keyword, long long *out)
{
	const char *val;
	char *end;

	if (keyword == NULL)
		return -1;
	if (strcmp(keyword, "uid") == 0) {
		*out = arcn->sb.st_uid;
		return 0;
	}
	if (strcmp(keyword, "gid") == 0) {
		*out = arcn->sb.st_gid;
		return 0;
	}
	if (strcmp(keyword, "nlink") == 0) {
		*out = arcn->sb.st_nlink;
		return 0;
	}
	if (strcmp(keyword, "mode") == 0) {
		*out = arcn->sb.st_mode;
		return 0;
	}
	val = pax_kv_lookup(arcn, keyword);
	if (val == NULL)
		return -1;
	*out = strtoll(val, &end, 10);
	if (end == val)
		return -1;
	return 0;
}

/* Parse unsigned numeric keywords, falling back to header values. */
static int
listopt_keyword_ull(ARCHD *arcn, const char *keyword,
    unsigned long long *out)
{
	const char *val;
	char *end;

	if (keyword == NULL)
		return -1;
	if (strcmp(keyword, "size") == 0) {
		*out = arcn->sb.st_size;
		return 0;
	}
	if (strcmp(keyword, "devmajor") == 0) {
		*out = MAJOR(arcn->sb.st_rdev);
		return 0;
	}
	if (strcmp(keyword, "devminor") == 0) {
		*out = MINOR(arcn->sb.st_rdev);
		return 0;
	}
	val = pax_kv_lookup(arcn, keyword);
	if (val == NULL)
		return -1;
	*out = strtoull(val, &end, 10);
	if (end == val)
		return -1;
	return 0;
}

/*
 * constants used by ls_list() when printing out archive members
 */
#define MODELEN 20
#define DATELEN 64
#define SECSPERDAY	(24 * 60 * 60)
#define SIXMONTHS	(SECSPERDAY * 365 / 2)
#define CURFRMT		"%b %e %H:%M"
#define OLDFRMT		"%b %e  %Y"
#define NAME_WIDTH	8
#define	TIMEFMT(t, now) \
	(((t) + SIXMONTHS <= (now) || (t) > (now)) ? OLDFRMT : CURFRMT)

/*
 * ls_list()
 *	list the members of an archive in ls format
 */

void
ls_list(ARCHD *arcn, time_t now, FILE *fp)
{
	struct stat *sbp;
	struct tm *tm;
	char f_mode[MODELEN];
	char f_date[DATELEN];
	int term;

	term = zeroflag ? '\0' : '\n';	/* path termination character */

	if (vflag && listopt_get() != NULL) {
		listopt_output(arcn, fp);
		(void)fputc(term, fp);
		(void)fflush(fp);
		return;
	}

	/*
	 * if not verbose, just print the file name
	 */
	if (!vflag) {
		if (zeroflag)
			(void)fputs(arcn->name, fp);
		else
			safe_print(arcn->name, fp);
		(void)putc(term, fp);
		(void)fflush(fp);
		return;
	}

	/*
	 * user wants long mode
	 */
	sbp = &(arcn->sb);
	strmode(sbp->st_mode, f_mode);

	/*
	 * print file mode, link count, uid, gid and time
	 */
	if ((tm = localtime(&(sbp->st_mtime))) == NULL)
		f_date[0] = '\0';
	else if (strftime(f_date, sizeof(f_date), TIMEFMT(sbp->st_mtime, now),
	    tm) == 0)
		f_date[0] = '\0';
	(void)fprintf(fp, "%s%2u %-*.*s %-*.*s ", f_mode, sbp->st_nlink,
		NAME_WIDTH, UT_NAMESIZE, user_from_uid(sbp->st_uid, 0),
		NAME_WIDTH, UT_NAMESIZE, group_from_gid(sbp->st_gid, 0));

	/*
	 * print device id's for devices, or sizes for other nodes
	 */
	if ((arcn->type == PAX_CHR) || (arcn->type == PAX_BLK))
		(void)fprintf(fp, "%4lu, %4lu ",
		    (unsigned long)MAJOR(sbp->st_rdev),
		    (unsigned long)MINOR(sbp->st_rdev));
	else {
		(void)fprintf(fp, "%9llu ", sbp->st_size);
	}

	/*
	 * print name and link info for hard and soft links
	 */
	(void)fputs(f_date, fp);
	(void)putc(' ', fp);
	safe_print(arcn->name, fp);
	if (PAX_IS_HARDLINK(arcn->type)) {
		fputs(" == ", fp);
		safe_print(arcn->ln_name, fp);
	} else if (arcn->type == PAX_SLK) {
		fputs(" -> ", fp);
		safe_print(arcn->ln_name, fp);
	}
	(void)putc(term, fp);
	(void)fflush(fp);
}

/*
 * tty_ls()
 *	print a short summary of file to tty.
 */

void
ls_tty(ARCHD *arcn)
{
	struct tm *tm;
	char f_date[DATELEN];
	char f_mode[MODELEN];
	time_t now = time(NULL);

	/*
	 * convert time to string, and print
	 */
	if ((tm = localtime(&(arcn->sb.st_mtime))) == NULL)
		f_date[0] = '\0';
	else if (strftime(f_date, DATELEN, TIMEFMT(arcn->sb.st_mtime, now),
	    tm) == 0)
		f_date[0] = '\0';
	strmode(arcn->sb.st_mode, f_mode);
	tty_prnt("%s%s %s\n", f_mode, f_date, arcn->name);
}

void
safe_print(const char *str, FILE *fp)
{
	char visbuf[5];
	const char *cp;

	/*
	 * if printing to a tty, use vis(3) to print special characters.
	 */
	if (isatty(fileno(fp))) {
		for (cp = str; *cp; cp++) {
			(void)vis(visbuf, cp[0], VIS_CSTYLE, cp[1]);
			(void)fputs(visbuf, fp);
		}
	} else {
		(void)fputs(str, fp);
	}
}

/* Append a new fragment to the aggregated custom listopt format string. */
int
listopt_append(const char *chunk)
{
	char *tmp;
	size_t add;

	if (chunk == NULL)
		return 0;
	add = strlen(chunk);
	if (add == 0)
		return 0;
	if (SIZE_MAX - listopt_len <= add)
		return -1;
	tmp = realloc(listopt_format, listopt_len + add + 1);
	if (tmp == NULL)
		return -1;
	listopt_format = tmp;
	memcpy(listopt_format + listopt_len, chunk, add);
	listopt_len += add;
	listopt_format[listopt_len] = '\0';
	return 0;
}

const char *
listopt_get(void)
{
	return listopt_format;
}

/* Reset cached list formatting between separate pax invocations. */
void
listopt_reset(void)
{
	free(listopt_format);
	listopt_format = NULL;
	listopt_len = 0;
}

/* Emit a single verbose listing line obeying the custom listopt format. */
static void
listopt_output(ARCHD *arcn, FILE *fp)
{
	const char *fmt = listopt_get();
	struct listopt_ctx ctx;
	struct listopt_spec spec;
	const char *next;
	char fmtbuf[64];
	char outbuf[PATH_MAX * 2];

	if (fmt == NULL || *fmt == '\0')
		return;
	listopt_ctx_init(&ctx);
	while (*fmt != '\0') {
		if (*fmt != '%') {
			(void)fputc(*fmt++, fp);
			continue;
		}
		if (fmt[1] == '%') {
			fmt += 2;
			(void)fputc('%', fp);
			continue;
		}
		if (listopt_parse_spec(fmt, &spec, &next) <= 0) {
			(void)fputc(*fmt++, fp);
			continue;
		}
		fmt = next;
		switch (spec.conv) {
		case 's':
		{
			const char *str = listopt_keyword_string(&ctx, arcn,
			    spec.keyword[0] ? spec.keyword : "path");
			snprintf(fmtbuf, sizeof(fmtbuf), "%%%s%s%s%s",
			    spec.flags, spec.width, spec.precision, "s");
			(void)fprintf(fp, fmtbuf, str);
			break;
		}
		case 'c':
		{
			const char *str = listopt_keyword_string(&ctx, arcn,
			    spec.keyword[0] ? spec.keyword : "path");
			char ch = (str && *str) ? *str : ' ';
			snprintf(fmtbuf, sizeof(fmtbuf), "%%%s%s%s%c",
			    spec.flags, spec.width, spec.precision, 'c');
			(void)fprintf(fp, fmtbuf, ch);
			break;
		}
		case 'd':
		case 'i':
		{
			long long val = 0;
			if (listopt_keyword_sll(arcn, spec.keyword, &val) != 0)
				val = 0;
			const char *length = "ll";
			snprintf(fmtbuf, sizeof(fmtbuf), "%%%s%s%s%s%c",
			    spec.flags, spec.width, spec.precision, length, spec.conv);
			(void)fprintf(fp, fmtbuf, val);
			break;
		}
		case 'o':
		case 'u':
		case 'x':
		case 'X':
		{
			unsigned long long val = 0;
			if (listopt_keyword_ull(arcn, spec.keyword, &val) != 0)
				val = 0;
			const char *length = "ll";
			snprintf(fmtbuf, sizeof(fmtbuf), "%%%s%s%s%s%c",
			    spec.flags, spec.width, spec.precision, length, spec.conv);
			(void)fprintf(fp, fmtbuf, val);
			break;
		}
		case 'T':
		{
			struct timespec ts;
			struct tm tm;
			const char *key = spec.keyword[0] ? spec.keyword : "mtime";
			const char *tfmt = spec.subfmt[0] ? spec.subfmt :
			    "%b %e %H:%M %Y";
			if (listopt_keyword_time(&ctx, arcn, key, &ts) == 0 &&
			    localtime_r(&ts.tv_sec, &tm) != NULL &&
			    strftime(outbuf, sizeof(outbuf), tfmt, &tm) > 0) {
				snprintf(fmtbuf, sizeof(fmtbuf), "%%%s%s%s%s",
				    spec.flags, spec.width, spec.precision, "s");
				(void)fprintf(fp, fmtbuf, outbuf);
			}
			break;
		}
		case 'M':
		{
			char modebuf[12];
			strmode(arcn->sb.st_mode, modebuf);
			snprintf(fmtbuf, sizeof(fmtbuf), "%%%s%s%s%s",
			    spec.flags, spec.width, spec.precision, "s");
			(void)fprintf(fp, fmtbuf, modebuf);
			break;
		}
		case 'D':
		{
			const char *use = NULL;
			if (S_ISCHR(arcn->sb.st_mode) || S_ISBLK(arcn->sb.st_mode)) {
				snprintf(outbuf, sizeof(outbuf), "%lu,%lu",
				    (u_long)MAJOR(arcn->sb.st_rdev),
				    (u_long)MINOR(arcn->sb.st_rdev));
				use = outbuf;
			} else if (spec.keyword[0]) {
				unsigned long long val = 0;
				if (listopt_keyword_ull(arcn, spec.keyword, &val) == 0) {
					snprintf(outbuf, sizeof(outbuf), "%llu", val);
					use = outbuf;
				}
			}
			if (use == NULL)
				use = "";
			snprintf(fmtbuf, sizeof(fmtbuf), "%%%s%s%s%s",
			    spec.flags, spec.width, spec.precision, "s");
			(void)fprintf(fp, fmtbuf, use);
			break;
		}
		case 'F':
		{
			const char *out = NULL;
			if (!spec.keyword[0])
				out = arcn->name;
			else {
				char *tmp = strdup(spec.keyword);
				char *save = tmp;
				outbuf[0] = '\0';
				if (tmp != NULL) {
					char *token;
					int first = 1;
					while ((token = strsep(&tmp, ",")) != NULL) {
						const char *part =
						    listopt_keyword_string(&ctx, arcn, token);
						if (!first)
							strlcat(outbuf, "/",
							    sizeof(outbuf));
						strlcat(outbuf, part, sizeof(outbuf));
						first = 0;
					}
					free(save);
					out = outbuf;
				}
			}
			snprintf(fmtbuf, sizeof(fmtbuf), "%%%s%s%s%s",
			    spec.flags, spec.width, spec.precision, "s");
			(void)fprintf(fp, fmtbuf, out ? out : "");
			break;
		}
		case 'L':
		{
			if (arcn->type == PAX_SLK)
				snprintf(outbuf, sizeof(outbuf), "%s -> %s",
				    arcn->name, arcn->ln_name);
			else
				strlcpy(outbuf, arcn->name, sizeof(outbuf));
			snprintf(fmtbuf, sizeof(fmtbuf), "%%%s%s%s%s",
			    spec.flags, spec.width, spec.precision, "s");
			(void)fprintf(fp, fmtbuf, outbuf);
			break;
		}
		default:
			(void)fputc(spec.conv, fp);
			break;
		}
	}
	listopt_ctx_free(&ctx);
}

void
pax_kv_free(PAXKEY **head)
{
	PAXKEY *cur;

	if (head == NULL)
		return;
	while ((cur = *head) != NULL) {
		*head = cur->next;
		free(cur->name);
		free(cur->value);
		free(cur);
	}
}

const char *
pax_kv_lookup(const ARCHD *arcn, const char *key)
{
	const PAXKEY *kv;

	if (arcn == NULL || key == NULL)
		return NULL;
	for (kv = arcn->xattr; kv != NULL; kv = kv->next)
		if (strcmp(kv->name, key) == 0)
			return kv->value;
	for (kv = arcn->gattr; kv != NULL; kv = kv->next)
		if (strcmp(kv->name, key) == 0)
			return kv->value;
	return NULL;
}

/*
 * asc_ul()
 *	convert hex/octal character string into a u_long. We do not have to
 *	check for overflow! (the headers in all supported formats are not large
 *	enough to create an overflow).
 *	NOTE: strings passed to us are NOT TERMINATED.
 * Return:
 *	unsigned long value
 */

u_long
asc_ul(char *str, int len, int base)
{
	char *stop;
	u_long tval = 0;

	stop = str + len;

	/*
	 * skip over leading blanks and zeros
	 */
	while ((str < stop) && ((*str == ' ') || (*str == '0')))
		++str;

	/*
	 * for each valid digit, shift running value (tval) over to next digit
	 * and add next digit
	 */
	if (base == HEX) {
		while (str < stop) {
			if ((*str >= '0') && (*str <= '9'))
				tval = (tval << 4) + (*str++ - '0');
			else if ((*str >= 'A') && (*str <= 'F'))
				tval = (tval << 4) + 10 + (*str++ - 'A');
			else if ((*str >= 'a') && (*str <= 'f'))
				tval = (tval << 4) + 10 + (*str++ - 'a');
			else
				break;
		}
	} else {
		while ((str < stop) && (*str >= '0') && (*str <= '7'))
			tval = (tval << 3) + (*str++ - '0');
	}
	return(tval);
}

/*
 * ul_asc()
 *	convert an unsigned long into an hex/oct ascii string. pads with LEADING
 *	ascii 0's to fill string completely
 *	NOTE: the string created is NOT TERMINATED.
 */

int
ul_asc(u_long val, char *str, int len, int base)
{
	char *pt;
	u_long digit;

	/*
	 * WARNING str is not '\0' terminated by this routine
	 */
	pt = str + len - 1;

	/*
	 * do a tailwise conversion (start at right most end of string to place
	 * least significant digit). Keep shifting until conversion value goes
	 * to zero (all digits were converted)
	 */
	if (base == HEX) {
		while (pt >= str) {
			if ((digit = (val & 0xf)) < 10)
				*pt-- = '0' + (char)digit;
			else
				*pt-- = 'a' + (char)(digit - 10);
			val >>= 4;
			if (val == 0)
				break;
		}
	} else {
		while (pt >= str) {
			*pt-- = '0' + (char)(val & 0x7);
			val >>= 3;
			if (val == 0)
				break;
		}
	}

	/*
	 * pad with leading ascii ZEROS. We return -1 if we ran out of space.
	 */
	while (pt >= str)
		*pt-- = '0';
	if (val != 0)
		return(-1);
	return(0);
}

/*
 * asc_ull()
 *	Convert hex/octal character string into a unsigned long long.
 *	We do not have to check for overflow!  (The headers in all
 *	supported formats are not large enough to create an overflow).
 *	NOTE: strings passed to us are NOT TERMINATED.
 * Return:
 *	unsigned long long value
 */

unsigned long long
asc_ull(char *str, int len, int base)
{
	char *stop;
	unsigned long long tval = 0;

	stop = str + len;

	/*
	 * skip over leading blanks and zeros
	 */
	while ((str < stop) && ((*str == ' ') || (*str == '0')))
		++str;

	/*
	 * for each valid digit, shift running value (tval) over to next digit
	 * and add next digit
	 */
	if (base == HEX) {
		while (str < stop) {
			if ((*str >= '0') && (*str <= '9'))
				tval = (tval << 4) + (*str++ - '0');
			else if ((*str >= 'A') && (*str <= 'F'))
				tval = (tval << 4) + 10 + (*str++ - 'A');
			else if ((*str >= 'a') && (*str <= 'f'))
				tval = (tval << 4) + 10 + (*str++ - 'a');
			else
				break;
		}
	} else {
		while ((str < stop) && (*str >= '0') && (*str <= '7'))
			tval = (tval << 3) + (*str++ - '0');
	}
	return(tval);
}

/*
 * ull_asc()
 *	Convert an unsigned long long into a hex/oct ascii string.
 *	Pads with LEADING ascii 0's to fill string completely
 *	NOTE: the string created is NOT TERMINATED.
 */

int
ull_asc(unsigned long long val, char *str, int len, int base)
{
	char *pt;
	unsigned long long digit;

	/*
	 * WARNING str is not '\0' terminated by this routine
	 */
	pt = str + len - 1;

	/*
	 * do a tailwise conversion (start at right most end of string to place
	 * least significant digit). Keep shifting until conversion value goes
	 * to zero (all digits were converted)
	 */
	if (base == HEX) {
		while (pt >= str) {
			if ((digit = (val & 0xf)) < 10)
				*pt-- = '0' + (char)digit;
			else
				*pt-- = 'a' + (char)(digit - 10);
			val >>= 4;
			if (val == 0)
				break;
		}
	} else {
		while (pt >= str) {
			*pt-- = '0' + (char)(val & 0x7);
			val >>= 3;
			if (val == 0)
				break;
		}
	}

	/*
	 * pad with leading ascii ZEROS. We return -1 if we ran out of space.
	 */
	while (pt >= str)
		*pt-- = '0';
	if (val != 0)
		return(-1);
	return(0);
}

/*
 * Copy at max min(bufz, fieldsz) chars from field to buf, stopping
 * at the first NUL char. NUL terminate buf if there is room left.
 */
size_t
fieldcpy(char *buf, size_t bufsz, const char *field, size_t fieldsz)
{
	char *p = buf;
	const char *q = field;
	size_t i = 0;

	if (fieldsz > bufsz)
		fieldsz = bufsz;
	while (i < fieldsz && *q != '\0') {
		*p++ = *q++;
		i++;
	}
	if (i < bufsz)
		*p = '\0';
	return(i);
}
