/*
 * *************************************************************************
 * This module is all original code
 * by Rob Nation
 * Copyright 1993, Robert Nation
 *     You may use this code for any purpose, as long as the original
 *     copyright remains in the source code and all documentation
 *
 * Changed 09/24/98 by Dan Espen:
 * - remove logic that processed and saved module configuration commands.
 * Its now in "modconf.c".
 *
 * PipeRead execution architecture (2026):
 * PipeRead commands are run by the privilege-separated fvwm_exec
 * helper, not forked here.  The helper was started before this
 * process locked its unveil(2) filesystem view, so PipeRead commands
 * can read and write files outside fvwm's own sandbox; the output is
 * streamed back over imsg(3) and processed with the same line
 * semantics as before.  See exec.c and fvwm_exec.c.
 * *************************************************************************
 */
#include <sys/types.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/wait.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "config.h"
#include "exec_imsg.h"
#include "fvwm.h"
#include "menus.h"
#include "misc.h"
#include "module.h"
#include "parse.h"
#include "pipebuf.h"
#include "screen.h"

extern Boolean debugging;

char *fvwm_file = NULL;

int numfilesread = 0;

#define MAX_NESTING_DEPTH 128

/*
 * The execution helper tracks one slot per concurrently running
 * PipeRead command.  A nesting limit above the slot count would let
 * the main process start a command the helper cannot track.
 */
static_assert(MAX_NESTING_DEPTH <= PIPEREAD_SLOTS,
    "MAX_NESTING_DEPTH must not exceed PIPEREAD_SLOTS");

/*
 * Bound for one assembled PipeRead command line.  The historical
 * implementation split lines at the 1023-byte fgets() buffer; the
 * limit here is far more generous while still bounding memory, so
 * every historical configuration behaves identically.
 */
#define MAX_PIPEREAD_LINE (64 * 1024)

/*
 * Bound on output buffered for an individual (possibly nested)
 * PipeRead frame whose output cannot be consumed right now.  When
 * it is reached the oldest complete lines are dropped, keeping
 * memory bounded while nested commands finish; the historical
 * direct-pipe implementation exerted backpressure instead, which
 * cannot be replicated across the imsg link without deadlocking
 * nested commands.
 */
#define PIPEREAD_BUF_MAX (256 * 1024)

static int last_read_failed = 0;

static const char *read_system_rc_cmd = "Read system" FVWMRC;

#define PIPE_READ_INTERVAL_SEC 1
#define PIPE_READ_MAX_IDLE_LOOPS 10

/*
 * One active PipeRead invocation.  Frames are linked into a stack:
 * nested PipeRead commands recurse on the C stack, and output
 * arriving for an outer frame while an inner frame runs is buffered
 * in that frame's pipebuf.
 */
struct piperead_frame {
	struct piperead_frame *next;
	u_int32_t	       id;
	struct pipebuf	       buf;
	char		      *lbuf; /* line assembly */
	size_t		       lbuf_len;
	size_t		       lbuf_cap;
	int		       prev_continued;
	int		       eof;
	int		       error;
	int		       timed_out;
	int		       got_data;
	int		       exit_status;
};

static struct piperead_frame *active_frames;
static u_int32_t	      piperead_next_id = 1;

/*
 * piperead_msg -- route one helper message: PipeRead payloads go to
 * the matching frame, everything else is handled as a normal Exec
 * response.
 */
static int
piperead_msg(struct imsg *imsg, void *)
{
	struct piperead_frame *frame;
	u_int32_t	       id;

	switch (imsg->hdr.type) {
	case IMSG_PIPEREAD_DATA:
		if (imsg->hdr.len < IMSG_HEADER_SIZE + sizeof(id))
			return 0;
		memcpy(&id, imsg->data, sizeof(id));
		for (frame = active_frames; frame != NULL;
		    frame = frame->next) {
			if (frame->id == id) {
				pipebuf_append(&frame->buf,
				    (char *)imsg->data + sizeof(id),
				    imsg->hdr.len - IMSG_HEADER_SIZE -
					sizeof(id));
				/*
				 * An outer frame whose output cannot
				 * be consumed right now (a nested
				 * PipeRead is running) must not grow
				 * without bound: keep the newest
				 * PIPEREAD_BUF_MAX bytes and drop
				 * the oldest complete lines.  The
				 * innermost frame consumes promptly
				 * and never hits this.
				 */
				if (frame != active_frames)
					pipebuf_trim(
					    &frame->buf, PIPEREAD_BUF_MAX);
				frame->got_data = 1;
				break;
			}
		}
		break;
	case IMSG_PIPEREAD_EOF:
		if (imsg->hdr.len < IMSG_HEADER_SIZE + sizeof(id) + sizeof(int))
			return 0;
		memcpy(&id, imsg->data, sizeof(id));
		for (frame = active_frames; frame != NULL;
		    frame = frame->next) {
			if (frame->id == id) {
				memcpy(&frame->exit_status,
				    (char *)imsg->data + sizeof(id),
				    sizeof(int));
				frame->eof = 1;
				break;
			}
		}
		break;
	case IMSG_PIPEREAD_ERROR:
		if (imsg->hdr.len < IMSG_HEADER_SIZE + sizeof(id))
			return 0;
		memcpy(&id, imsg->data, sizeof(id));
		for (frame = active_frames; frame != NULL;
		    frame = frame->next) {
			if (frame->id == id) {
				frame->error = 1;
				break;
			}
		}
		break;
	default:
		exec_helper_dispatch(imsg, NULL);
		break;
	}
	return 0;
}

/*
 * piperead_execute_line -- handle one physical line from the stream
 * (with the trailing newline included, or truncated at a NUL, or a
 * bounded fragment of an oversized line).  Backslash-newline at the
 * end of a line joins the next line into the same command, matching
 * the historical continuation behaviour.
 */
static void
piperead_execute_line(struct piperead_frame *frame, char *line, size_t len,
    XEvent *eventp, FvwmWindow *tmp_win, unsigned long context, int *Module)
{
	int    continued = 0;
	size_t need;
	char  *nbuf;

	if (len >= 2 && line[len - 2] == '\\' && line[len - 1] == '\n')
		continued = 1;

	if (frame->lbuf_len > 0 && frame->prev_continued) {
		/* Replace the previous line's backslash-newline. */
		frame->lbuf_len -= 2;
	}

	need = frame->lbuf_len + len + 1;
	if (need > frame->lbuf_cap) {
		size_t ncap = frame->lbuf_cap ? frame->lbuf_cap : 1024;

		while (ncap < need)
			ncap *= 2;
		/* Keep the original block on failure: realloc() leaves it
		 * allocated and the frame still owns it. */
		nbuf = realloc(frame->lbuf, ncap);
		if (nbuf == NULL) {
			/* Out of memory: drop the partial command. */
			frame->lbuf_len = 0;
			frame->lbuf_cap = 0;
			frame->prev_continued = 0;
			return;
		}
		frame->lbuf = nbuf;
		frame->lbuf_cap = ncap;
	}
	memcpy(frame->lbuf + frame->lbuf_len, line, len);
	frame->lbuf_len += len;
	frame->prev_continued = continued;

	if (continued && frame->lbuf_len < MAX_PIPEREAD_LINE)
		return; /* wait for the continuation line */

	frame->lbuf[frame->lbuf_len] = '\0';
	if (debugging) {
		fvwm_msg(
		    DBG, "ReadSubFunc", "about to exec: '%s'", frame->lbuf);
	}
	ExecuteFunction(frame->lbuf, tmp_win, eventp, context, *Module);
	frame->lbuf_len = 0;
	frame->prev_continued = 0;
}

/*
 * piperead_process_lines -- pull complete lines out of the frame's
 * buffer and execute them.  Returns 1 when a line was processed.
 */
static int
piperead_process_lines(struct piperead_frame *frame, XEvent *eventp,
    FvwmWindow *tmp_win, unsigned long context, int *Module)
{
	char  *line;
	size_t len;

	if (!pipebuf_next_line(&frame->buf, &line, &len, MAX_PIPEREAD_LINE))
		return 0;
	piperead_execute_line(
	    frame, line, len, eventp, tmp_win, context, Module);
	return 1;
}

/*
 * piperead_run -- execute one PipeRead command through the helper
 * and feed its output into the configuration parser.  This is the
 * synchronous replacement for the historical direct fork.  Returns
 * 1 when the command could not be started (helper unavailable),
 * 0 otherwise; last_read_failed records timeout/error outcomes.
 */
static int
piperead_run(const char *command, XEvent *eventp, FvwmWindow *tmp_win,
    unsigned long context, int *Module, const char *cmdname)
{
	struct piperead_frame frame;
	int		      idle_loops = 0;
	int		      helper_fd;
	int		      start_failed = 0;

	memset(&frame, 0, sizeof(frame));
	frame.id = piperead_next_id++;
	pipebuf_init(&frame.buf);
	frame.next = active_frames;
	active_frames = &frame;

	if (exec_helper_running() == 0 ||
	    exec_helper_piperead_start(frame.id, command) == -1) {
		frame.error = 1;
		start_failed = 1;
	}

	while (!frame.eof && !frame.error && !frame.timed_out) {
		fd_set	       readfds;
		struct timeval tv;
		int	       ready;

		/* Process whatever is already buffered. */
		while (piperead_process_lines(
		    &frame, eventp, tmp_win, context, Module))
			;
		if (frame.eof || frame.error)
			break;

		helper_fd = exec_helper_fd();
		FD_ZERO(&readfds);
		if (helper_fd >= 0)
			FD_SET(helper_fd, &readfds);
		tv.tv_sec = PIPE_READ_INTERVAL_SEC;
		tv.tv_usec = 0;

		ready = select((helper_fd >= 0 ? helper_fd : 0) + 1, &readfds,
		    NULL, NULL, &tv);
		if (ready < 0) {
			if (errno == EINTR)
				continue;
			frame.error = 1;
			break;
		}
		if (ready == 0) {
			if (++idle_loops >= PIPE_READ_MAX_IDLE_LOOPS) {
				(void)exec_helper_piperead_kill(frame.id);
				frame.timed_out = 1;
				break;
			}
			continue;
		}

		if (helper_fd >= 0 && FD_ISSET(helper_fd, &readfds)) {
			if (exec_helper_drain(piperead_msg, NULL) == -1) {
				/* The helper is gone. */
				frame.error = 1;
				break;
			}
		}

		if (frame.got_data) {
			idle_loops = 0;
			frame.got_data = 0;
		}
	}

	/*
	 * A partially assembled final line (EOF without newline) is
	 * executed, matching the historical behaviour.  On timeout or
	 * error the partial command is dropped.
	 */
	if (frame.eof && !frame.error && !frame.timed_out) {
		while (piperead_process_lines(
		    &frame, eventp, tmp_win, context, Module))
			;
		if (frame.lbuf_len > 0) {
			if (frame.prev_continued)
				frame.lbuf_len -= 2;
			frame.lbuf[frame.lbuf_len] = '\0';
			ExecuteFunction(
			    frame.lbuf, tmp_win, eventp, context, *Module);
		}
	}

	if (frame.timed_out)
		fvwm_msg(WARN, cmdname,
		    "command '%s' did not close pipe, terminating it", command);

	active_frames = frame.next;
	pipebuf_free(&frame.buf);
	free(frame.lbuf);

	if (frame.timed_out || frame.error)
		last_read_failed = 1;
	else
		last_read_failed = 0;

	return start_failed;
}

extern void StartupStuff(void);

/*
 * func to do actual read/piperead work
 * Arg 1 is file name to read.
 * Arg 2 (optional) "Quiet" to suppress message on missing file.
 */
static void
ReadSubFunc(XEvent *eventp, Window, FvwmWindow *tmp_win, unsigned long context,
    char *action, int *Module, int piperead)
{
	if (numfilesread >= MAX_NESTING_DEPTH) {
		fvwm_msg(ERR, piperead ? "PipeRead" : "Read",
		    "nesting depth exceeded (%d)", MAX_NESTING_DEPTH);
		return;
	}
	char  *filename = NULL, *Home, *home_file, *ofilename = NULL;
	char  *option; /* optional arg to read */
	char  *rest, *tline, line[1024];
	FILE  *stream = NULL;
	char   missing_quiet; /* missing file msg control */
	char  *cmdname;
	size_t len;

	/* domivogt (30-Dec-1998: I tried using conditional evaluation instead
	 * of the cmdname variable ( piperead?"PipeRead":"Read" ), but gcc seems
	 * to treat this expression as a pointer to a character pointer, not
	 * just as a character pointer, but it doesn't complain either. Or
	 * perhaps insure++ gets this wrong? */
	if (piperead)
		cmdname = "PipeRead";
	else
		cmdname = "Read";

	numfilesread++;

	/*  fvwm_msg(INFO,cmdname,"action == '%s'",action); */

	rest = GetNextToken(action, &ofilename); /* read file name arg */
	if (ofilename == NULL) {
		fvwm_msg(ERR, cmdname, "missing parameter");
		last_read_failed = 1;
		return;
	}
	missing_quiet = 'n';		    /* init */
	rest = GetNextToken(rest, &option); /* read optional arg */
	if (option != NULL) {		    /* if there is a second arg */
		if (strncasecmp(option, "Quiet", 5) ==
		    0) { /* is the arg "quiet"? */
			missing_quiet =
			    'y'; /* no missing file message wanted */
		} /* end quiet arg */
		free(option); /* arg not needed after this */
	} /* end there is a second arg */

	if (piperead) {
		int start_failed;

		/*
		 * Run the command in the execution helper, which was
		 * created before this process locked its unveil(2)
		 * filesystem view, and feed the output back into the
		 * parser.  If the helper is unavailable there is no
		 * way to run the command outside fvwm's sandbox, so
		 * the request fails instead of falling back to a
		 * sandboxed fork.
		 */
		start_failed = piperead_run(
		    ofilename, eventp, tmp_win, context, Module, cmdname);
		if (start_failed && missing_quiet == 'n') {
			fvwm_msg(ERR, cmdname,
			    "command '%s' not run "
			    "(fvwm_exec unavailable)",
			    ofilename);
		}
		free(ofilename);
		if (start_failed)
			last_read_failed = 1;
		return;
	}

	filename = ofilename;
	if (ofilename[0] != '/') {
		Home = getenv("HOME");
		if (Home != NULL) {
			len = strlen(Home) + strlen(ofilename) + 3;
			home_file = xmalloc(len);
			strlcpy(home_file, Home, len);
			strlcat(home_file, "/", len);
			strlcat(home_file, ofilename, len);
			filename = home_file;
			stream = fopen(filename, "r");
		} else {
			stream = NULL;
		}
		if (stream == NULL) {
			if ((filename != NULL) && (filename != ofilename))
				free(filename);
			Home = FVWM_CONFIGDIR;
			len = strlen(Home) + strlen(ofilename) + 3;
			home_file = xmalloc(len);
			strlcpy(home_file, Home, len);
			strlcat(home_file, "/", len);
			strlcat(home_file, ofilename, len);
			filename = home_file;
			stream = fopen(filename, "r");
		}
	} else {
		stream = fopen(filename, "r");
	}

	if (stream == NULL) {
		if (missing_quiet == 'n') {
			fvwm_msg(ERR, cmdname,
			    "file '%s' not found in $HOME "
			    "or " FVWM_CONFIGDIR,
			    ofilename);
		}
		if (filename && filename != ofilename)
			free(filename);
		free(ofilename);
		last_read_failed = 1;
		return;
	}

	if (filename != ofilename && ofilename != NULL) {
		free(ofilename);
		ofilename = NULL;
	}
	fcntl(fileno(stream), F_SETFD, 1);
	if (fvwm_file != NULL)
		free(fvwm_file);
	fvwm_file = filename;

	while (stream) {
		tline = fgets(line, (sizeof line) - 1, stream);
		if (tline == NULL)
			break;
		{
			int l;
			while ((l = strlen(line)) < (int)sizeof(line) &&
			    l >= 2 && line[l - 2] == '\\' &&
			    line[l - 1] == '\n') {
				char *cont = fgets(
				    line + l - 2, sizeof(line) - l + 1, stream);
				if (cont == NULL)
					break;
			}
		}
		tline = line;
		if (debugging) {
			fvwm_msg(
			    DBG, "ReadSubFunc", "about to exec: '%s'", tline);
		}
		ExecuteFunction(tline, tmp_win, eventp, context, *Module);
	}

	fclose(stream);
	last_read_failed = 0;
}

void
ReadFile(XEvent *eventp, Window junk, FvwmWindow *tmp_win,
    unsigned long context, char *action, int *Module)
{
	int this_read = numfilesread;

	if (debugging) {
		fvwm_msg(DBG, "ReadFile", "about to attempt '%s'", action);
	}

	ReadSubFunc(eventp, junk, tmp_win, context, action, Module, 0);

	if (last_read_failed && this_read == 0) {
		fvwm_msg(INFO, "Read", "trying to read system rc file");
		ExecuteFunction(
		    (char *)read_system_rc_cmd, NULL, &Event, C_ROOT, -1);
	}

	if (this_read == 0) {
		if (debugging) {
			fvwm_msg(
			    DBG, "ReadFile", "about to call startup functions");
		}
		StartupStuff();
	}
}

void
PipeRead(XEvent *eventp, Window junk, FvwmWindow *tmp_win,
    unsigned long context, char *action, int *Module)
{
	int this_read = numfilesread;

	if (debugging) {
		fvwm_msg(DBG, "PipeRead", "about to attempt '%s'", action);
	}

	ReadSubFunc(eventp, junk, tmp_win, context, action, Module, 1);

	if (last_read_failed && this_read == 0) {
		fvwm_msg(INFO, "PipeRead", "trying to read system rc file");
		ExecuteFunction(
		    (char *)read_system_rc_cmd, NULL, &Event, C_ROOT, -1);
	}

	if (this_read == 0) {
		if (debugging) {
			fvwm_msg(
			    DBG, "PipeRead", "about to call startup functions");
		}
		StartupStuff();
	}
}
