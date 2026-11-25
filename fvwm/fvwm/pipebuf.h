/*
 * pipebuf.h -- bounded byte-buffer with line extraction for
 * PipeRead output processing.
 *
 * PipeRead output arrives in arbitrary-sized chunks (it is a pipe
 * stream): a read may return part of a line, several lines, or
 * several lines plus part of one.  This module accumulates bytes
 * and hands lines or bounded fragments back to the caller.
 * The caller controls how much unread output accumulates between
 * extractions and can trim output for a suspended reader.
 *
 * Embedded NUL bytes end the effective command at that point (fvwm
 * commands are C strings): the NUL and the rest of the physical
 * line up to and including the next newline are discarded during
 * extraction, even when the newline arrives in a later append.
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

// clang-format off
#include <stddef.h>
// clang-format on

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
 * the unread region, and returns 1.  If no terminator occurs
 * in the first maxline bytes, those bytes form a fragment.
 * At EOF, also return a final unterminated line if one remains.
 * Returns 0 when no line or fragment is ready.  The result is a
 * borrowed byte span, which may lack a NUL terminator.  Appending
 * or freeing the buffer invalidates it.
 */
int pipebuf_next_line(
    struct pipebuf *, char **linep, size_t *linelen, size_t maxline, int eof);

/*
 * Drop the oldest buffered complete lines until at most limit bytes
 * remain.  Drop an incomplete tail if it alone exceeds the
 * limit.  Trimming also clears any pending NUL discard state
 * and retains the allocation for reuse.
 */
void pipebuf_trim(struct pipebuf *, size_t limit);

#endif /* FVWM_PIPEBUF_H */
