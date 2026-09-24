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

#ifndef FVWM_XALLOC_H
#define FVWM_XALLOC_H

#include <err.h>
#include <stdckdint.h>
#include <stdlib.h>
#include <string.h>

[[nodiscard]] static inline void *
xmalloc(size_t size)
{
	void *ptr;

	if (size == 0)
		size = 1;
	ptr = malloc(size);
	if (ptr == NULL)
		err(1, "malloc");
	return ptr;
}

[[nodiscard]] static inline void *
xcalloc(size_t nmemb, size_t size)
{
	void *ptr;

	if (nmemb == 0 || size == 0) {
		nmemb = 1;
		size = 1;
	}
	ptr = calloc(nmemb, size);
	if (ptr == NULL)
		err(1, "calloc");
	return ptr;
}

[[nodiscard]] static inline void *
xrealloc(void *ptr, size_t size)
{
	void *newptr;

	if (size == 0)
		size = 1;
	newptr = realloc(ptr, size);
	if (newptr == NULL)
		err(1, "realloc");
	return newptr;
}

[[nodiscard]] static inline void *
xreallocarray(void *ptr, size_t nmemb, size_t size)
{
	size_t total;

	if (ckd_mul(&total, nmemb, size))
		err(1, "reallocarray");
	return xrealloc(ptr, total);
}

[[nodiscard]] static inline char *
xstrdup(const char *s)
{
	char *copy;

	copy = strdup(s);
	if (copy == NULL)
		err(1, "strdup");
	return copy;
}

[[nodiscard]] static inline char *
xstrndup(const char *s, size_t n)
{
	char *copy;

	copy = strndup(s, n);
	if (copy == NULL)
		err(1, "strndup");
	return copy;
}

#endif /* FVWM_XALLOC_H */
