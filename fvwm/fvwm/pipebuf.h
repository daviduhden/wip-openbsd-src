/*
 * pipebuf.h -- bounded byte-buffer with line extraction for
 * PipeRead output processing.
 *
 * PipeRead output arrives in arbitrary-sized chunks (it is a pipe
 * stream): a read may return part of a line, several lines, or
 * several lines plus part of one.  This module accumulates bytes
 * and hands complete lines back to the caller, enforcing a bounded
 * maximum line length so hostile command output can never grow
 * memory without limit.
 *
 * Embedded NUL bytes end the effective command at that point (fvwm
 * commands are C strings): the NUL and the rest of the physical
 * line up to and including the next newline are discarded, which
 * matches the historical fgets()/strlen() behaviour exactly.
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

#ifndef FVWM_PIPEBUF_H
#define FVWM_PIPEBUF_H

#include <stddef.h>

struct pipebuf {
	char  *data;
	size_t len; /* valid bytes from data[0] */
	size_t off; /* consumed bytes */
	size_t cap;
	int    discarding; /* past a NUL, skipping to newline */
};

void pipebuf_init(struct pipebuf *);
void pipebuf_free(struct pipebuf *);
void pipebuf_append(struct pipebuf *, const void *, size_t);

/*
 * Extract the next line from the buffer.
 *
 * On success stores a pointer to the line (including the trailing
 * newline when present) in *linep, sets *linelen, removes it from
 * the buffer, and returns 1.  A line longer than maxline bytes
 * (newline not seen within the bound) is returned split at maxline.
 * Returns 0 when no complete line is available yet.  The returned
 * pointer is only valid until the next pipebuf_append() call.
 */
int pipebuf_next_line(
    struct pipebuf *, char **linep, size_t *linelen, size_t maxline);

/*
 * Drop the oldest buffered complete lines until at most limit bytes
 * remain.  Used to bound memory when a nested PipeRead command
 * cannot consume an outer frame's output for a while; incomplete
 * trailing data is preserved.
 */
void pipebuf_trim(struct pipebuf *, size_t limit);

#endif /* FVWM_PIPEBUF_H */
