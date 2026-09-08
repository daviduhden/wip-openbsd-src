/*
 * exec_imsg.h -- shared imsg protocol between fvwm(1) and its
 * privilege-separated execution helper fvwm_exec.
 *
 * Both sides include this header so the message types and payload
 * layouts cannot drift apart.  Every payload is validated by the
 * receiver against the message length before use.
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

#ifndef FVWM_EXEC_IMSG_H
#define FVWM_EXEC_IMSG_H

#include <imsg.h>

enum imsg_exec_type {
	/* fvwm -> helper: launch a program (plain Exec).
	 * Payload: int cargc, int envc, NUL-terminated strings. */
	IMSG_EXEC_RUN = 0,
	/* helper -> fvwm: child forked.  Payload: pid_t pid. */
	IMSG_EXEC_OK,
	/* helper -> fvwm: launch failed.  Payload: int errno. */
	IMSG_EXEC_ERROR,
	/* helper -> fvwm: child exited.  Payload: pid_t pid, int status. */
	IMSG_EXEC_EXIT,

	/* fvwm -> helper: run a command, streaming its stdout back
	 * (PipeRead).  Payload: u_int32_t id, NUL-terminated command. */
	IMSG_PIPEREAD_RUN,
	/* helper -> fvwm: chunk of command output.
	 * Payload: u_int32_t id, bytes. */
	IMSG_PIPEREAD_DATA,
	/* helper -> fvwm: command finished.
	 * Payload: u_int32_t id, int status. */
	IMSG_PIPEREAD_EOF,
	/* helper -> fvwm: command could not be started.
	 * Payload: u_int32_t id, int errno. */
	IMSG_PIPEREAD_ERROR,
	/* fvwm -> helper: abort a running PipeRead command.
	 * Payload: u_int32_t id. */
	IMSG_PIPEREAD_KILL,
};

/*
 * Maximum payload for IMSG_EXEC_RUN: two ints (argc/envc) plus the
 * argv and envp strings.  Fits the 16-bit imsg length field with
 * room to spare and covers any realistic command line plus a full
 * environment.
 */
#define MAX_EXEC_PAYLOAD (60 * 1024)

/*
 * Single PipeRead command accepted by the helper; bounded so one
 * malicious/hostile fvwm message cannot consume unbounded memory.
 */
#define MAX_PIPEREAD_COMMAND (60 * 1024)

/* Max bytes of PipeRead output forwarded per IMSG_PIPEREAD_DATA. */
#define PIPEREAD_CHUNK (32 * 1024)

/* Maximum number of concurrent PipeRead commands the helper tracks;
 * fvwm caps command nesting at MAX_NESTING_DEPTH, which must stay
 * below this. */
#define PIPEREAD_SLOTS 128

#endif /* FVWM_EXEC_IMSG_H */
