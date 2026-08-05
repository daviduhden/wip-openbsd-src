/*
 * fvwm_exec.c -- privilege-separated execution helper for fvwm(1)
 *
 * Runs as a separate process that receives exec requests via imsg(3)
 * from the main fvwm process, executes them, and reports results.
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

#include <sys/socket.h>

#include <imsg.h>

#include <err.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

#include "config.h"

/*
 * imsg_exec -- structured IPC message types for execution requests.
 *
 * Message flow:
 *   fvwm -> helper:  IMSG_EXEC_RUN       (path + argv + envp)
 *   helper -> fvwm:  IMSG_EXEC_OK        (pid of launched child)
 *                    IMSG_EXEC_ERROR     (errno + message)
 *                    IMSG_EXEC_EXIT      (pid + status, sent on child exit)
 */

enum imsg_exec_type {
	IMSG_EXEC_RUN = 0,
	IMSG_EXEC_OK,
	IMSG_EXEC_ERROR,
	IMSG_EXEC_EXIT,
};

__dead static void
usage(void)
{
	extern char *__progname;

	fprintf(stderr, "usage: %s\n", __progname);
	exit(1);
}

static void
exec_child(int argc, char **argv, char **envp)
{
	(void)unveil("/usr/bin", "rx");
	(void)unveil("/usr/local/bin", "rx");
	(void)unveil("/usr/X11R6/bin", "rx");
	(void)unveil("/bin", "rx");
	(void)unveil("/sbin", "rx");
	(void)unveil("/usr/sbin", "rx");
	if (unveil(NULL, NULL) == -1)
		err(1, "unveil");

	if (pledge("stdio exec", NULL) == -1)
		err(1, "pledge");

	closefrom(3);

	if (envp)
		execve(argv[0], argv, envp);
	else
		execv(argv[0], argv);

	err(1, "execv: %s", argv[0]);
}

int
main(int argc, char **argv)
{
	struct imsgbuf ibuf;
	struct imsg imsg;
	ssize_t n;
	int s;

	if (argc != 1)
		usage();

	if (getenv("FVWM_EXEC_FD") == NULL)
		errx(1, "FVWM_EXEC_FD not set");

	s = (int)strtonum(getenv("FVWM_EXEC_FD"), 0, INT_MAX, NULL);

	signal(SIGPIPE, SIG_IGN);

	if (imsgbuf_init(&ibuf, s) == -1)
		err(1, "imsgbuf_init");

	if (pledge("stdio proc exec", NULL) == -1)
		err(1, "pledge");

	for (;;) {
		if ((n = imsgbuf_read(&ibuf)) == -1 && errno != EAGAIN)
			err(1, "imsgbuf_read");
		if (n == 0)
			break;

		while ((n = imsg_get(&ibuf, &imsg)) != -1) {
			if (n == 0)
				break;

			switch (imsg.hdr.type) {
			case IMSG_EXEC_RUN: {
				pid_t pid;
				char **child_argv;
				char **child_envp;
				int cargc, envc;
				char *data = imsg.data;

				if (imsg.hdr.len < sizeof(int) * 2) {
					warnx("short IMSG_EXEC_RUN");
					break;
				}
				memcpy(&cargc, data, sizeof(int));
				memcpy(&envc, data + sizeof(int), sizeof(int));
				data += sizeof(int) * 2;

				/* Reconstruct argv */
				child_argv = malloc(
				    (cargc + 1) * sizeof(char *));
				if (child_argv == NULL)
					err(1, "malloc");
				for (int i = 0; i < cargc; i++) {
					size_t len = strlen(data);
					child_argv[i] = data;
					data += len + 1;
				}
				child_argv[cargc] = NULL;

				/* Reconstruct envp */
				child_envp = malloc(
				    (envc + 1) * sizeof(char *));
				if (child_envp == NULL)
					err(1, "malloc");
				for (int i = 0; i < envc; i++) {
					size_t len = strlen(data);
					child_envp[i] = data;
					data += len + 1;
				}
				child_envp[envc] = NULL;

				pid = fork();
				if (pid == -1) {
					warn("fork");
					imsg_compose(&ibuf, IMSG_EXEC_ERROR,
					    0, 0, -1, &errno, sizeof(int));
					free(child_argv);
					free(child_envp);
					break;
				}
				if (pid == 0)
					exec_child(cargc, child_argv,
					    child_envp);

				free(child_argv);
				free(child_envp);

				imsg_compose(&ibuf, IMSG_EXEC_OK,
				    0, 0, -1, &pid, sizeof(pid_t));
				imsgbuf_flush(&ibuf);
				break;
			}
			default:
				warnx("unknown imsg type %d",
				    imsg.hdr.type);
				break;
			}
			imsg_free(&imsg);
		}
	}

	imsgbuf_clear(&ibuf);
	close(s);
	return 0;
}
