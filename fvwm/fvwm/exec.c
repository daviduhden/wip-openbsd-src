/*
 * exec.c -- interface to the privilege-separated execution helper.
 *
 * The main fvwm process communicates with fvwm_exec via imsg(3)
 * over a socketpair(2).  The helper executes external commands
 * without inheriting the X11 connection.
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
#include <sys/wait.h>

#include <imsg.h>

#include <err.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "config.h"
#include "fvwm.h"
#include "module.h"
#include "misc.h"

enum imsg_exec_type {
	IMSG_EXEC_RUN = 0,
	IMSG_EXEC_OK,
	IMSG_EXEC_ERROR,
	IMSG_EXEC_EXIT,
};

static struct imsgbuf	*exec_ibuf;
static int		 exec_fd = -1;
static pid_t		 exec_pid = -1;

/*
 * exec_helper_start -- fork and exec the execution helper.
 * The helper receives one end of a socketpair for imsg communication.
 */
void
exec_helper_start(void)
{
	int sv[2];

	if (socketpair(AF_UNIX, SOCK_STREAM, PF_UNSPEC, sv) == -1)
		err(1, "socketpair");

	exec_pid = fork();
	if (exec_pid == -1)
		err(1, "fork");

	if (exec_pid == 0) {
		char fdstr[32];

		close(sv[0]);
		snprintf(fdstr, sizeof(fdstr), "%d", sv[1]);
		setenv("FVWM_EXEC_FD", fdstr, 1);

		if (pledge("stdio proc exec", NULL) == -1)
			err(1, "pledge");

		execl(FVWMLIBDIR "/fvwm_exec", "fvwm_exec", NULL);
		err(1, "execl %s/fvwm_exec", FVWMLIBDIR);
	}

	close(sv[1]);
	exec_fd = sv[0];

	exec_ibuf = malloc(sizeof(struct imsgbuf));
	if (exec_ibuf == NULL)
		err(1, "malloc");
	if (imsgbuf_init(exec_ibuf, exec_fd) == -1)
		err(1, "imsgbuf_init");
}

/*
 * exec_helper_stop -- request helper shutdown and reap.
 */
void
exec_helper_stop(void)
{
	if (exec_ibuf == NULL)
		return;

	imsgbuf_clear(exec_ibuf);
	close(exec_fd);
	free(exec_ibuf);
	exec_ibuf = NULL;
	exec_fd = -1;

	if (exec_pid > 0) {
		kill(exec_pid, SIGTERM);
		waitpid(exec_pid, NULL, 0);
		exec_pid = -1;
	}
}

/*
 * exec_helper_handle -- process imsg responses from the helper.
 * Called from the event loop when exec_fd is readable.
 */
void
exec_helper_handle(void)
{
	struct imsg imsg;
	ssize_t n;

	if (exec_ibuf == NULL)
		return;

	if ((n = imsgbuf_read(exec_ibuf)) == -1 && errno != EAGAIN)
		warn("imsgbuf_read");
	if (n == 0) {
		warnx("exec helper disconnected");
		exec_helper_stop();
		return;
	}

	while ((n = imsg_get(exec_ibuf, &imsg)) != -1) {
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_EXEC_OK: {
			pid_t pid;

			if (imsg.hdr.len < (IMSG_HEADER_SIZE + sizeof(pid_t)))
				break;
			memcpy(&pid, imsg.data, sizeof(pid_t));
			break;
		}
		case IMSG_EXEC_ERROR: {
			int errnum;

			if (imsg.hdr.len < (IMSG_HEADER_SIZE + sizeof(int)))
				break;
			memcpy(&errnum, imsg.data, sizeof(int));
			warnc(errnum, "exec helper reported error");
			break;
		}
		case IMSG_EXEC_EXIT: {
			/* Module/command exit; handled by signal watching */
			break;
		}
		default:
			break;
		}
		imsg_free(&imsg);
	}
}

/*
 * exec_helper_launch -- request the helper to execute a command.
 * On success, returns 0. On failure (fork error in helper), returns -1.
 */
int
exec_helper_launch(int argc, char **argv, char **envp)
{
	struct ibuf *buf;
	size_t datalen, total;
	int cargc = argc - 1; /* skip argv[0] which is the path */
	int envc = 0;
	int i, ret = -1;
	int fd;

	if (exec_ibuf == NULL)
		return -1;

	/* Calculate total payload size: sizeof(int)*2 + all strings */
	datalen = sizeof(int) * 2;
	for (i = 1; i < argc; i++)
		datalen += strlen(argv[i]) + 1;
	if (envp) {
		for (i = 0; envp[i] != NULL; i++) {
			datalen += strlen(envp[i]) + 1;
			envc++;
		}
	}

	if (datalen > MAX_BODY_SIZE * sizeof(unsigned long)) {
		warnx("exec argument too large");
		return -1;
	}

	/* Compose and send the request */
	buf = imsg_create(exec_ibuf, IMSG_EXEC_RUN, 0, 0, datalen);
	if (buf == NULL)
		return -1;

	/* argc */
	buf->wpos += imsg_add(buf, &cargc, sizeof(int));
	/* envc */
	buf->wpos += imsg_add(buf, &envc, sizeof(int));
	/* strings */
	for (i = 1; i < argc; i++) {
		buf->wpos += imsg_add(buf, argv[i], strlen(argv[i]) + 1);
	}
	if (envp) {
		for (i = 0; envp[i] != NULL; i++) {
			buf->wpos += imsg_add(buf, envp[i],
			    strlen(envp[i]) + 1);
		}
	}
	imsg_close(exec_ibuf, buf);
	imsgbuf_flush(exec_ibuf);

	return 0;
}
