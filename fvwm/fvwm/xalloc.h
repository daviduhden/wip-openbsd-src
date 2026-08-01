#ifndef FVWM_XALLOC_H
#define FVWM_XALLOC_H

#include <err.h>
#include <stdlib.h>
#include <string.h>

static inline void *
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

static inline void *
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

static inline void *
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

static inline void *
xreallocarray(void *ptr, size_t nmemb, size_t size)
{
	return xrealloc(ptr, nmemb * size);
}

static inline char *
xstrdup(const char *s)
{
	char *copy;

	copy = strdup(s);
	if (copy == NULL)
		err(1, "strdup");
	return copy;
}

static inline char *
xstrndup(const char *s, size_t n)
{
	char *copy;

	copy = strndup(s, n);
	if (copy == NULL)
		err(1, "strndup");
	return copy;
}

#endif /* FVWM_XALLOC_H */
