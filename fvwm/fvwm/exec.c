/*
 * exec.c -- interface to the privilege-separated execution helper.
 *
 * The main fvwm process communicates with fvwm_exec via imsg(3)
 * over a socketpair(2).  The helper executes external commands
 * (Exec) and PipeRead commands without inheriting the X11 connection
 * and, crucially, without being restricted by the filesystem view
 * this process later locks down with unveil(2).
 *
 * The helper is intentionally NOT sandboxed with pledge(2) or
 * unveil(2): both are inherited across execve(2) and would cripple
 * the arbitrary programs fvwm launches.  See fvwm_exec.c.  The
 * helper is started before fvwm applies its own unveil(2) policy,
 * which is what keeps PipeRead and Exec working for programs that
 * live outside the main process's filesystem sandbox.
 *
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

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <imsg.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "config.h"
#include "exec_imsg.h"
#include "fvwm.h"
#include "misc.h"
#include "module.h"

static struct imsgbuf *exec_ibuf;
static int	       exec_fd = -1;
static pid_t	       exec_pid = -1;

/*
 * exec_helper_start -- fork and exec the execution helper.
 * The helper receives one end of a socketpair for imsg communication.
 *
 * Must be called before fvwm locks its own unveil(2) state: the
 * helper and everything it later runs need the unrestricted
 * filesystem view.
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

		/*
		 * No pledge here: it would be inherited by every
		 * program the helper launches.  See fvwm_exec.c.
		 */
		execl(FVWMLIBDIR "/fvwm_exec", "fvwm_exec", NULL);
		err(1, "execl %s/fvwm_exec", FVWMLIBDIR);
	}

	close(sv[1]);
	exec_fd = sv[0];
	/*
	 * Nonblocking so the PipeRead pump (and the event loop) never
	 * stall on a partial imsg; incomplete messages stay buffered
	 * inside the imsgbuf.
	 */
	if (fcntl(exec_fd, F_SETFL, O_NONBLOCK) == -1)
		err(1, "fcntl");

	exec_ibuf = malloc(sizeof(struct imsgbuf));
	if (exec_ibuf == NULL)
		err(1, "malloc");
	if (imsgbuf_init(exec_ibuf, exec_fd) == -1)
		err(1, "imsgbuf_init");
}

/*
 * exec_helper_fd -- return the helper's imsg descriptor, or -1 when
 * the helper is not running.  Used by the event loop and the
 * PipeRead pump.
 */
int
exec_helper_fd(void)
{
	return exec_fd;
}

/*
 * exec_helper_running -- nonzero while the helper is available.
 */
int
exec_helper_running(void)
{
	return exec_ibuf != NULL;
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
 * exec_helper_dispatch -- handle one already-dequeued helper
 * message of the Exec family.  PipeRead messages are routed by the
 * PipeRead pump in read.c instead.
 */
int
exec_helper_dispatch(struct imsg *imsg, void *)
{
	switch (imsg->hdr.type) {
	case IMSG_EXEC_OK: {
		pid_t pid;

		if (imsg->hdr.len < (IMSG_HEADER_SIZE + sizeof(pid_t)))
			break;
		memcpy(&pid, imsg->data, sizeof(pid_t));
		break;
	}
	case IMSG_EXEC_ERROR: {
		int errnum;

		if (imsg->hdr.len < (IMSG_HEADER_SIZE + sizeof(int)))
			break;
		memcpy(&errnum, imsg->data, sizeof(int));
		warnc(errnum, "exec helper reported error");
		break;
	}
	case IMSG_EXEC_EXIT:
		/* Launched program exited; helper reaped it. */
		break;
	default:
		break;
	}
	return 0;
}

/*
 * exec_helper_drain -- read whatever imsg data is currently
 * available from the helper and invoke cb() for each complete
 * message.  Returns 1 when messages were processed, 0 when the
 * socket would block, and -1 when the helper disconnected.
 */
int
exec_helper_drain(int (*cb)(struct imsg *, void *), void *arg)
{
	struct imsg imsg;
	ssize_t	    n;
	int	    processed = 0;

	if (exec_ibuf == NULL)
		return -1;

	n = imsgbuf_read(exec_ibuf);
	if (n == -1) {
		if (errno == EAGAIN || errno == EINTR)
			return 0;
		warn("imsgbuf_read");
		return -1;
	}
	if (n == 0) {
		warnx("exec helper disconnected");
		exec_helper_stop();
		return -1;
	}

	while ((n = imsgbuf_get(exec_ibuf, &imsg)) != -1) {
		if (n == 0)
			break;
		cb(&imsg, arg);
		imsg_free(&imsg);
		processed = 1;
	}

	return processed;
}

/*
 * exec_helper_handle -- process imsg responses from the helper.
 * Called from the event loop when exec_fd is readable.
 */
void
exec_helper_handle(void)
{
	exec_helper_drain(exec_helper_dispatch, NULL);
}

/*
 * exec_helper_piperead_start -- request a PipeRead command from the
 * helper.  id is fvwm-chosen and echoed back in every response so
 * nested PipeRead invocations can be told apart.
 */
int
exec_helper_piperead_start(u_int32_t id, const char *command)
{
	struct ibuf *buf;
	size_t	     clen;

	if (exec_ibuf == NULL)
		return -1;

	clen = strlen(command) + 1;
	if (clen > MAX_PIPEREAD_COMMAND) {
		warnx("PipeRead command too large");
		return -1;
	}

	buf =
	    imsg_create(exec_ibuf, IMSG_PIPEREAD_RUN, 0, 0, sizeof(id) + clen);
	if (buf == NULL)
		return -1;
	if (imsg_add(buf, &id, sizeof(id)) == -1)
		return -1;
	if (imsg_add(buf, command, clen) == -1)
		return -1;
	imsg_close(exec_ibuf, buf);
	imsgbuf_flush(exec_ibuf);

	return 0;
}

/*
 * exec_helper_piperead_kill -- abort a running PipeRead command.
 */
int
exec_helper_piperead_kill(u_int32_t id)
{
	struct ibuf *buf;

	if (exec_ibuf == NULL)
		return -1;

	buf = imsg_create(exec_ibuf, IMSG_PIPEREAD_KILL, 0, 0, sizeof(id));
	if (buf == NULL)
		return -1;
	if (imsg_add(buf, &id, sizeof(id)) == -1)
		return -1;
	imsg_close(exec_ibuf, buf);
	imsgbuf_flush(exec_ibuf);

	return 0;
}

/*
 * exec_helper_launch -- request the helper to execute a command.
 * argv is the full argument vector (argv[0] is the program);
 * envp may be NULL to inherit the helper's environment (which is
 * fvwm's environment).  Returns 0 on success, -1 on failure.
 */
int
exec_helper_launch(int argc, char **argv, char **envp)
{
	struct ibuf *buf;
	size_t	     datalen;
	int	     cargc = argc; /* include argv[0], the program path */
	int	     envc = 0;
	int	     i;

	if (exec_ibuf == NULL)
		return -1;

	/* Calculate total payload size: sizeof(int)*2 + all strings */
	datalen = sizeof(int) * 2;
	for (i = 0; i < argc; i++)
		datalen += strlen(argv[i]) + 1;
	if (envp) {
		for (i = 0; envp[i] != NULL; i++) {
			datalen += strlen(envp[i]) + 1;
			envc++;
		}
	}

	if (datalen > MAX_EXEC_PAYLOAD) {
		warnx("exec argument too large");
		return -1;
	}

	/* Compose and send the request.  imsg_add advances the buffer
	 * itself and frees it on failure; check every call. */
	buf = imsg_create(exec_ibuf, IMSG_EXEC_RUN, 0, 0, datalen);
	if (buf == NULL)
		return -1;

	/* argc */
	if (imsg_add(buf, &cargc, sizeof(int)) == -1)
		return -1;
	/* envc */
	if (imsg_add(buf, &envc, sizeof(int)) == -1)
		return -1;
	/* strings (argv[0..argc-1], matching decode_exec_msg()) */
	for (i = 0; i < argc; i++) {
		if (imsg_add(buf, argv[i], strlen(argv[i]) + 1) == -1)
			return -1;
	}
	if (envp) {
		for (i = 0; envp[i] != NULL; i++) {
			if (imsg_add(buf, envp[i], strlen(envp[i]) + 1) == -1)
				return -1;
		}
	}
	imsg_close(exec_ibuf, buf);
	imsgbuf_flush(exec_ibuf);

	return 0;
}
