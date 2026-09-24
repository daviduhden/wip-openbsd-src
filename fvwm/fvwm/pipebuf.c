/*
 * pipebuf.c -- see pipebuf.h.
 *
 * The buffer keeps a read offset and only compacts (moving memory)
 * on append, so line pointers handed to the caller remain valid
 * until the next pipebuf_append().
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

#include <err.h>
#include <stdlib.h>
#include <string.h>

#include "pipebuf.h"

#define PIPEBUF_CHUNK 4096

void
pipebuf_init(struct pipebuf *pb)
{
	pb->data = NULL;
	pb->len = 0;
	pb->off = 0;
	pb->cap = 0;
	pb->discarding = 0;
}

void
pipebuf_free(struct pipebuf *pb)
{
	free(pb->data);
	pb->data = NULL;
	pb->len = 0;
	pb->off = 0;
	pb->cap = 0;
	pb->discarding = 0;
}

void
pipebuf_append(struct pipebuf *pb, const void *data, size_t len)
{
	size_t need, ncap;

	if (len == 0)
		return;

	/* Compact consumed bytes before growing; this invalidates
	 * previously returned line pointers, per the contract. */
	if (pb->off > 0) {
		memmove(pb->data, pb->data + pb->off, pb->len - pb->off);
		pb->len -= pb->off;
		pb->off = 0;
	}

	need = pb->len + len;
	if (need > pb->cap) {
		ncap = pb->cap ? pb->cap : PIPEBUF_CHUNK;
		while (ncap < need)
			ncap *= 2;
		pb->data = realloc(pb->data, ncap);
		if (pb->data == NULL)
			err(1, "realloc");
		pb->cap = ncap;
	}
	memcpy(pb->data + pb->len, data, len);
	pb->len = need;
}

void
pipebuf_trim(struct pipebuf *pb, size_t limit)
{
	size_t i;

	if (pb->discarding) {
		/* Discard-region bytes have no value; drop them all. */
		pb->off = pb->len;
		pb->discarding = 0;
	}

	while (pb->len - pb->off > limit) {
		for (i = pb->off; i < pb->len; i++) {
			if (pb->data[i] == '\n') {
				pb->off = i + 1;
				break;
			}
		}
		if (i == pb->len) {
			/* No complete line: drop what is buffered. */
			pb->off = pb->len;
			break;
		}
	}
}

int
pipebuf_next_line(
    struct pipebuf *pb, char **linep, size_t *linelen, size_t maxline)
{
	size_t i, start;

	if (maxline == 0 || pb->off >= pb->len)
		return 0;

	if (pb->discarding) {
		/* Skipping to the end of a physical line whose
		 * leading part was truncated at a NUL. */
		for (i = pb->off; i < pb->len; i++) {
			if (pb->data[i] == '\n') {
				pb->off = i + 1;
				pb->discarding = 0;
				break;
			}
		}
		if (pb->discarding) {
			/* The buffered bytes are all still part of
			 * the discard region; drop them and keep
			 * discarding. */
			pb->off = pb->len;
			return 0;
		}
		if (pb->off >= pb->len)
			return 0;
	}

	start = pb->off;
	for (i = start; i < pb->len; i++) {
		if (pb->data[i] == '\n') {
			*linep = pb->data + start;
			*linelen = i - start + 1;
			pb->off = i + 1;
			return 1;
		}
		if (pb->data[i] == '\0') {
			/*
			 * A NUL terminates the effective command;
			 * discard the rest of the physical line, as
			 * the historical C-string processing did.
			 */
			*linep = pb->data + start;
			*linelen = i - start;
			pb->off = i + 1;
			pb->discarding = 1;
			return 1;
		}
		if (i - start + 1 >= maxline) {
			/* Line exceeds the bound: return a bounded
			 * fragment and leave the rest queued. */
			*linep = pb->data + start;
			*linelen = maxline;
			pb->off = start + maxline;
			return 1;
		}
	}

	return 0; /* need more data */
}
