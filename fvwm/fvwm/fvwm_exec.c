/*
 * fvwm_exec.c -- same-user execution helper for fvwm(1)
 *
 * Runs as a separate process that receives execution requests via
 * imsg(3) from the main fvwm process, executes them, and reports
 * results.  Two request classes are handled:
 *
 *   IMSG_EXEC_RUN      launch a program detached from fvwm (Exec)
 *   IMSG_PIPEREAD_RUN  run a command with stdout streamed back to
 *                      fvwm (PipeRead)
 *
 * The controller starts the helper before limiting its own
 * filesystem view with unveil(2).  This lets configured commands
 * use the invoking user's filesystem access while the X11 event
 * loop runs under the controller's restricted view.
 *
 * The helper runs under the same UID as the controller, with no
 * pledge(2) or unveil(2) policy of its own.  It bounds individual
 * messages and closes inherited descriptors before running commands.
 * The controller trusts it to execute commands with the user's access.
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

// clang-format off
#include <sys/socket.h>
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <imsg.h>
#include <limits.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "config.h"
#include "exec_imsg.h"
// clang-format on

static volatile sig_atomic_t got_sigchld;

/*
 * One active PipeRead command.
 */
struct piperead_slot {
	int	  active;
	u_int32_t id;
	pid_t	  pid;
	int	  fd;
	int	  status; /* wait status once known */
	int	  status_known;
	int	  kill_pending;
	time_t	  kill_deadline;
};

static struct piperead_slot piperead_slots[PIPEREAD_SLOTS];

/* Pids of detached (Exec) children, for exit reporting. */
#define MAX_EXEC_CHILDREN (512)
static pid_t  exec_children[MAX_EXEC_CHILDREN];
static int    exec_children_count;

__attribute__((__noreturn__)) static void
usage(void)
{
	extern char *__progname;

	fprintf(stderr, "usage: %s\n", __progname);
	exit(1);
}

static void
sigchld_handler(int signo)
{
	(void)signo;
	got_sigchld = 1;
}

/*
 * exec_child -- final stage before exec: drop the helper's
 * descriptors (including the imsg socket and, in particular, any X
 * connection that reached the helper) and run the target.  Runs
 * unrestricted: the launched program needs the user's full
 * privileges.
 */
__attribute__((__noreturn__)) static void
exec_child(char **argv, char **envp)
{
	closefrom(3);

	if (envp)
		execve(argv[0], argv, envp);
	else
		execv(argv[0], argv);

	err(1, "execv: %s", argv[0]);
}

/*
 * decode_exec_msg -- validate an IMSG_EXEC_RUN payload and build
 * argv/envp arrays pointing into it.  The caller owns both arrays
 * on success and must keep the message alive while using the strings.
 * A zero environment count produces an empty vector.
 */
static int
decode_exec_msg(struct imsg *imsg, char ***argvp, char ***envpp)
{
	char **child_argv, **child_envp;
	char  *data = imsg->data;
	size_t remaining;
	int    cargc, envc;
	size_t i;

	if (imsg->hdr.len < IMSG_HEADER_SIZE + 2 * sizeof(int))
		return -1;
	remaining = imsg->hdr.len - IMSG_HEADER_SIZE - 2 * sizeof(int);

	memcpy(&cargc, data, sizeof(int));
	memcpy(&envc, data + sizeof(int), sizeof(int));
	data += 2 * sizeof(int);

	/*
	 * The shortest possible encoding of argc arguments is argc
	 * empty strings (one NUL each), so argc must not exceed
	 * remaining.  Same for envc.
	 */
	if (cargc < 1 || envc < 0 || (size_t)cargc > remaining ||
	    (size_t)envc > remaining)
		return -1;

	if ((child_argv = calloc(cargc + 1, sizeof(char *))) == NULL)
		err(1, "calloc");
	for (i = 0; i < (size_t)cargc; i++) {
		size_t len;

		len = strnlen(data, remaining);
		if (len == remaining) {
			/* No terminating NUL inside the message. */
			free(child_argv);
			return -1;
		}
		child_argv[i] = data;
		data += len + 1;
		remaining -= len + 1;
	}
	child_argv[cargc] = NULL;

	if ((child_envp = calloc(envc + 1, sizeof(char *))) == NULL)
		err(1, "calloc");
	for (i = 0; i < (size_t)envc; i++) {
		size_t len;

		len = strnlen(data, remaining);
		if (len == remaining) {
			free(child_argv);
			free(child_envp);
			return -1;
		}
		child_envp[i] = data;
		data += len + 1;
		remaining -= len + 1;
	}
	child_envp[envc] = NULL;

	*argvp = child_argv;
	*envpp = child_envp;
	return 0;
}

/*
 * Environment staging for the next PipeRead request. A prepare/command
 * sequence is queued in order by the single-threaded controller.
 */
struct piperead_env {
	char *data;
	size_t len;
	size_t cap;
	size_t limit;
	int error;
	int staged;
};

static void
piperead_env_append(struct piperead_env *env, const void *data, size_t len)
{
	char *next;
	size_t cap;

	if (len == 0) {
		env->len = 0;
		env->error = 0;
		env->staged = 1;
		return;
	}
	if (!env->staged || env->error != 0)
		return;
	if (len > env->limit - env->len) {
		env->error = E2BIG;
		return;
	}
	if (env->len + len > env->cap) {
		cap = env->cap > env->limit / 2 ? env->limit : env->cap * 2;
		if (cap < env->len + len)
			cap = env->len + len;
		next = realloc(env->data, cap);
		if (next == NULL) {
			env->error = errno;
			return;
		}
		env->data = next;
		env->cap = cap;
	}
	memcpy(env->data + env->len, data, len);
	env->len += len;
}

static char **
piperead_env_vector(struct piperead_env *env)
{
	char **vec;
	size_t i, n = 0;

	if (env->error != 0) {
		errno = env->error;
		return NULL;
	}
	if (env->len != 0 && env->data[env->len - 1] != '\0') {
		errno = EINVAL;
		return NULL;
	}
	for (i = 0; i < env->len; i++)
		n += env->data[i] == '\0';
	vec = calloc(n + 1, sizeof(*vec));
	if (vec == NULL)
		return NULL;
	n = 0;
	for (i = 0; i < env->len; i += strlen(env->data + i) + 1)
		vec[n++] = env->data + i;
	return vec;
}

/* Borrow the validated command string until the caller frees the imsg. */
static char *
decode_piperead_msg(struct imsg *imsg, u_int32_t *id)
{
	char *command;
	size_t len;

	if (imsg->hdr.len < IMSG_HEADER_SIZE + sizeof(*id) + 1)
		return NULL;
	memcpy(id, imsg->data, sizeof(*id));
	command = (char *)imsg->data + sizeof(*id);
	len = imsg->hdr.len - IMSG_HEADER_SIZE - sizeof(*id);
	if (strnlen(command, len) == len)
		return NULL;
	return command;
}

/*
 * reap_children -- collect exited detached children and report them
 * to fvwm; also record exit statuses of PipeRead children so the
 * EOF message can carry them. Reap every child regardless of whether
 * its reporting slot or output pipe is still active.
 */
static void
reap_children(struct imsgbuf *ibuf)
{
	pid_t pid;
	int status, i;

	got_sigchld = 0;
	for (;;) {
		pid = waitpid(-1, &status, WNOHANG);
		if (pid == -1 && errno == EINTR)
			continue;
		if (pid <= 0)
			break;

		for (i = 0; i < PIPEREAD_SLOTS; i++) {
			struct piperead_slot *slot = &piperead_slots[i];

			if (!slot->active || slot->pid != pid)
				continue;
			slot->status = status;
			slot->status_known = 1;
			slot->pid = 0;
			break;
		}
		if (i != PIPEREAD_SLOTS)
			continue;

		/* Untracked children still get reaped; only reports are bounded. */
		for (i = 0; i < exec_children_count; i++) {
			struct {
				pid_t pid;
				int   status;
			} msg;

			if (exec_children[i] != pid)
				continue;
			msg.pid = pid;
			msg.status = status;
			imsg_compose(ibuf, IMSG_EXEC_EXIT, 0, 0, -1, &msg,
			    sizeof(msg));
			exec_children[i] = exec_children[--exec_children_count];
			break;
		}
	}
	imsgbuf_flush(ibuf);
}

/*
 * piperead_finish -- close one PipeRead pipe, send EOF or ERROR,
 * and free the slot.
 */
static void
piperead_finish(struct imsgbuf *ibuf, struct piperead_slot *slot, int errnum)
{
	struct {
		u_int32_t id;
		int	  status;
	} msg;

	if (slot->fd >= 0) {
		close(slot->fd);
		slot->fd = -1;
	}
	if (slot->pid > 0 && !slot->status_known) {
		int status;

		if (waitpid(slot->pid, &status, WNOHANG) == slot->pid) {
			slot->status = status;
			slot->status_known = 1;
		}
	}
	if (errnum != 0) {
		struct {
			u_int32_t id;
			int	  errnum;
		} errmsg;

		errmsg.id = slot->id;
		errmsg.errnum = errnum;
		imsg_compose(ibuf, IMSG_PIPEREAD_ERROR, 0, 0, -1, &errmsg,
		    sizeof(errmsg));
	} else {
		msg.id = slot->id;
		msg.status = slot->status_known ? slot->status : 0;
		imsg_compose(
		    ibuf, IMSG_PIPEREAD_EOF, 0, 0, -1, &msg, sizeof(msg));
	}
	imsgbuf_flush(ibuf);
	slot->active = 0;
	slot->pid = 0;
}

/*
 * piperead_start -- fork /bin/sh -c command with stdout on a pipe
 * and register the slot.  The child runs unrestricted (no pledge or
 * unveil was ever applied to the helper).
 */
static int
piperead_start(struct imsgbuf *ibuf, u_int32_t id, char *command,
    char **envp)
{
	struct piperead_slot *slot;
	int		      pipe_fd[2];
	pid_t		      pid;
	int		      i;

	for (i = 0; i < PIPEREAD_SLOTS; i++) {
		if (!piperead_slots[i].active)
			break;
	}
	if (i == PIPEREAD_SLOTS) {
		struct {
			u_int32_t id;
			int	  errnum;
		} errmsg;

		errmsg.id = id;
		errmsg.errnum = EBUSY;
		imsg_compose(ibuf, IMSG_PIPEREAD_ERROR, 0, 0, -1, &errmsg,
		    sizeof(errmsg));
		imsgbuf_flush(ibuf);
		return -1;
	}
	slot = &piperead_slots[i];

	if (pipe(pipe_fd) < 0) {
		struct {
			u_int32_t id;
			int	  errnum;
		} errmsg;

		errmsg.id = id;
		errmsg.errnum = errno;
		imsg_compose(ibuf, IMSG_PIPEREAD_ERROR, 0, 0, -1, &errmsg,
		    sizeof(errmsg));
		imsgbuf_flush(ibuf);
		return -1;
	}

	pid = fork();
	if (pid < 0) {
		struct {
			u_int32_t id;
			int	  errnum;
		} errmsg;

		close(pipe_fd[0]);
		close(pipe_fd[1]);
		errmsg.id = id;
		errmsg.errnum = errno;
		imsg_compose(ibuf, IMSG_PIPEREAD_ERROR, 0, 0, -1, &errmsg,
		    sizeof(errmsg));
		imsgbuf_flush(ibuf);
		return -1;
	}

	if (pid == 0) {
		char *argv[] = {"sh", "-c", command, NULL};

		close(pipe_fd[0]);
		if (dup2(pipe_fd[1], STDOUT_FILENO) == -1)
			_exit(127);
		close(pipe_fd[1]);
		closefrom(3);
		execve("/bin/sh", argv, envp);
		_exit(127);
	}

	close(pipe_fd[1]);
	slot->active = 1;
	slot->id = id;
	slot->pid = pid;
	slot->fd = pipe_fd[0];
	slot->status = 0;
	slot->status_known = 0;
	slot->kill_pending = 0;
	slot->kill_deadline = 0;
	return 0;
}

/*
 * piperead_kill -- terminate a PipeRead command.  The historical
 * fvwm semantics are SIGTERM followed by SIGKILL after a grace
 * period; the deadline is enforced in the main loop.
 */
static void
piperead_kill(struct imsgbuf *ibuf, u_int32_t id)
{
	int i;

	(void)ibuf;

	for (i = 0; i < PIPEREAD_SLOTS; i++) {
		struct piperead_slot *slot = &piperead_slots[i];

		if (!slot->active || slot->id != id || slot->pid <= 0)
			continue;
		if (kill(slot->pid, SIGTERM) == -1 && errno == ESRCH) {
			/* Already gone; drain and report EOF. */
			continue;
		}
		slot->kill_pending = 1;
		slot->kill_deadline = time(NULL) + 1;
		return;
	}
}

int
main(int argc, char **argv)
{
	extern char **environ;
	struct imsgbuf ibuf;
	struct imsg    imsg;
	struct piperead_env env = {0};
	long	       arg_max;
	const char    *errstr = NULL;
	ssize_t	       n;
	int	       s;

	(void)argv;
	arg_max = sysconf(_SC_ARG_MAX);
	env.limit = arg_max > 0 ? (size_t)arg_max : MAX_EXEC_PAYLOAD;
	if (argc != 1)
		usage();

	if (getenv("FVWM_EXEC_FD") == NULL)
		errx(1, "FVWM_EXEC_FD not set");

	s = (int)strtonum(getenv("FVWM_EXEC_FD"), 0, INT_MAX, &errstr);
	if (errstr != NULL)
		errx(1, "FVWM_EXEC_FD is %s: %s", errstr,
		    getenv("FVWM_EXEC_FD"));

	signal(SIGPIPE, SIG_IGN);

	/*
	 * SIGCHLD wakes the main loop so exited children are reaped
	 * and reported; the handler itself only sets a flag (async
	 * signal safe).
	 */
	{
		struct sigaction sa;

		memset(&sa, 0, sizeof(sa));
		sa.sa_handler = sigchld_handler;
		if (sigaction(SIGCHLD, &sa, NULL) == -1)
			err(1, "sigaction");
	}

	if (imsgbuf_init(&ibuf, s) == -1)
		err(1, "imsgbuf_init");

	/*
	 * Keep the imsg socket and close higher-numbered
	 * descriptors here.  Children close all descriptors
	 * above stderr immediately before exec.
	 */
	closefrom(s + 1);

	for (;;) {
		struct pollfd	pfd[1 + PIPEREAD_SLOTS];
		int		slot_of_pfd[PIPEREAD_SLOTS];
		nfds_t		npfd = 0;
		int		i, ready;
		time_t		now;
		static u_int8_t chunk_data[PIPEREAD_CHUNK];

		pfd[npfd].fd = s;
		pfd[npfd].events = POLLIN;
		npfd++;
		for (i = 0; i < PIPEREAD_SLOTS; i++) {
			if (!piperead_slots[i].active)
				continue;
			pfd[npfd].fd = piperead_slots[i].fd;
			pfd[npfd].events = POLLIN;
			slot_of_pfd[npfd - 1] = i;
			npfd++;
		}

		ready = poll(pfd, npfd, 1000);
		if (ready == -1) {
			if (errno == EINTR) {
				reap_children(&ibuf);
				continue;
			}
			err(1, "poll");
		}

		now = time(NULL);
		if (got_sigchld)
			reap_children(&ibuf);

		/* Enforce SIGKILL deadlines for aborted commands. */
		for (i = 0; i < PIPEREAD_SLOTS; i++) {
			struct piperead_slot *slot = &piperead_slots[i];

			if (!slot->active || !slot->kill_pending)
				continue;
			if (now < slot->kill_deadline)
				continue;
			slot->kill_deadline = now + 1;
			if (slot->pid > 0)
				kill(slot->pid, SIGKILL);
		}

		/* Forward available PipeRead output. */
		for (i = 0; i < (int)npfd - 1; i++) {
			struct piperead_slot *slot =
			    &piperead_slots[slot_of_pfd[i]];
			ssize_t nr;

			if (!slot->active)
				continue;
			if (!(pfd[1 + i].revents & (POLLIN | POLLHUP)))
				continue;

			nr = read(slot->fd, chunk_data, sizeof(chunk_data));
			if (nr > 0) {
				struct {
					u_int32_t id;
					u_int8_t  data[];
				} *chunk;

				chunk = malloc(sizeof(u_int32_t) + (size_t)nr);
				if (chunk == NULL)
					err(1, "malloc");
				chunk->id = slot->id;
				memcpy(chunk->data, chunk_data, (size_t)nr);
				if (imsg_compose(&ibuf, IMSG_PIPEREAD_DATA, 0,
				    0, -1, chunk,
				    sizeof(u_int32_t) + (size_t)nr) == -1)
					err(1, "imsg_compose PipeRead data");
				free(chunk);
				if (imsgbuf_flush(&ibuf) == -1)
					err(1, "imsgbuf_flush PipeRead data");
			} else if (nr == 0) {
				/* All write ends closed, even if the child is alive. */
				piperead_finish(&ibuf, slot, 0);
			}
			/* EAGAIN: nothing to forward after all. */
		}

		if (!(pfd[0].revents & POLLIN)) {
			if (pfd[0].revents & (POLLHUP | POLLERR | POLLNVAL))
				break;
			continue;
		}

		if ((n = imsgbuf_read(&ibuf)) == -1) {
			if (errno == EINTR && got_sigchld) {
				reap_children(&ibuf);
				continue;
			}
			if (errno == EAGAIN)
				continue;
			err(1, "imsgbuf_read");
		}
		if (n == 0)
			break;

		while ((n = imsgbuf_get(&ibuf, &imsg)) != -1) {
			if (n == 0)
				break;

			switch (imsg.hdr.type) {
			case IMSG_EXEC_RUN: {
				pid_t  pid;
				char **child_argv;
				char **child_envp;

				if (imsg.hdr.len >
				    IMSG_HEADER_SIZE + MAX_EXEC_PAYLOAD) {
					warnx("oversized IMSG_EXEC_RUN");
					break;
				}
				if (decode_exec_msg(&imsg, &child_argv,
					&child_envp) == -1) {
					warnx("malformed IMSG_EXEC_RUN");
					break;
				}

				pid = fork();
				if (pid == -1) {
					warn("fork");
					imsg_compose(&ibuf, IMSG_EXEC_ERROR, 0,
					    0, -1, &errno, sizeof(int));
					free(child_argv);
					free(child_envp);
					break;
				}
				if (pid == 0)
					exec_child(child_argv, child_envp);

				free(child_argv);
				free(child_envp);

				if (exec_children_count < MAX_EXEC_CHILDREN)
					exec_children[exec_children_count++] =
					    pid;

				imsg_compose(&ibuf, IMSG_EXEC_OK, 0, 0, -1,
				    &pid, sizeof(pid_t));
				imsgbuf_flush(&ibuf);
				break;
			}
			case IMSG_PIPEREAD_ENV:
				piperead_env_append(&env, imsg.data,
				    imsg.hdr.len - IMSG_HEADER_SIZE);
				break;
			case IMSG_PIPEREAD_RUN: {
				u_int32_t id;
				char *command, **envp;

				if (imsg.hdr.len >
				    IMSG_HEADER_SIZE + MAX_EXEC_PAYLOAD ||
				    (command = decode_piperead_msg(&imsg, &id)) ==
				    NULL) {
					warnx("malformed IMSG_PIPEREAD_RUN");
					env.staged = 0;
					break;
				}
				envp = env.staged ? piperead_env_vector(&env) : environ;
				if (envp == NULL) {
					struct {
						u_int32_t id;
						int errnum;
					} msg = {id, errno};

					imsg_compose(&ibuf, IMSG_PIPEREAD_ERROR,
					    0, 0, -1, &msg, sizeof(msg));
					imsgbuf_flush(&ibuf);
				} else {
					piperead_start(&ibuf, id, command, envp);
					if (env.staged)
						free(envp);
				}
				env.staged = 0;
				break;
			}
			case IMSG_PIPEREAD_KILL: {
				u_int32_t id;

				if (imsg.hdr.len <
				    IMSG_HEADER_SIZE + sizeof(id))
					break;
				memcpy(&id, imsg.data, sizeof(id));
				piperead_kill(&ibuf, id);
				break;
			}
			default:
				warnx("unknown imsg type %d", imsg.hdr.type);
				break;
			}
			imsg_free(&imsg);
		}

		if (got_sigchld)
			reap_children(&ibuf);
	}

	free(env.data);
	imsgbuf_clear(&ibuf);
	close(s);
	return 0;
}
