/*
 * Copyright (c) 2025 David Uhden Collado <david@uhden.dev>
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/*
 * safemalloc.c
 *
 * Small wrappers around the standard allocation functions that:
 * - avoid requesting zero bytes (some implementations may return NULL)
 * - check for allocation failure and abort the program with an error message
 * - provide a checked calloc and a safe strdup implementation
 */

/*
 * safemalloc - allocate length bytes, treating a request for 0 bytes as 1.
 * On allocation failure this function prints an error and exits the process.
 * Returns a non-NULL pointer to allocated memory.
 */
char *safemalloc(size_t length)
{
  char *ptr;

  /* Treat a 0-byte request as 1 to avoid implementations that return NULL */
  if (length == 0)
    length = 1;

  ptr = (char *)malloc(length);
  if (ptr == (char *)0)
    {
      fprintf(stderr, "malloc of %lu bytes failed. Exiting\n",
              (unsigned long)length);
      exit(1);
    }
  return ptr;
}

/*
 * saferealloc - reallocate memory to the specified length.
 * A length of 0 is treated as 1 to preserve the non-NULL API contract.
 * On failure the function prints an error and exits the process.
 * Returns a non-NULL pointer to the reallocated memory.
 */
void *saferealloc(void *ptr, size_t length)
{
  void *nptr;

  /* Normalize zero-size requests to 1 so the function never returns NULL */
  if (length == 0)
    length = 1;

  nptr = realloc(ptr, length);
  if (nptr == (void *)0)
  {
    fprintf(stderr, "realloc of %lu bytes failed. Exiting\n",
            (unsigned long)length);
    exit(1);
  }
  return nptr;
}

/*
 * safecalloc - allocate an array of nmemb elements each of size bytes,
 * with zero-initialization. Performs an overflow check on nmemb * size
 * before calling calloc. Zero counts are normalized to 1 to avoid NULL
 * returns from some calloc implementations. On failure the function
 * prints an error and exits the process.
 */
void *safecalloc(size_t nmemb, size_t size)
{
  /* Normalize zero counts to 1 to maintain a non-NULL return contract */
  if (nmemb == 0 || size == 0)
  {
    nmemb = 1;
    size = 1;
  }

  /* Check for multiplication overflow: nmemb * size must fit in size_t */
  if (nmemb > 0 && size > ((size_t)-1) / nmemb)
  {
    fprintf(stderr, "calloc overflow for %lu elements of %lu bytes. Exiting\n",
            (unsigned long)nmemb, (unsigned long)size);
    exit(1);
  }

  void *ptr = calloc(nmemb, size);
  if (ptr == (void *)0)
  {
    fprintf(stderr, "calloc of %lu bytes failed. Exiting\n",
            (unsigned long)(nmemb * size));
    exit(1);
  }
  return ptr;
}

/*
 * safestrdup - duplicate a C string using safemalloc.
 * If s is NULL, returns NULL. Otherwise allocates strlen(s)+1 bytes
 * and copies the contents including the terminating NUL.
 */
char *safestrdup(const char *s)
{
  if (!s)
  return NULL;
  size_t len = strlen(s) + 1;
  char *d = safemalloc(len);
  memcpy(d, s, len);
  return d;
}


