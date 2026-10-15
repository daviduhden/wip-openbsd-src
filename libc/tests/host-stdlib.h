/*	$OpenBSD$	*/

/*
 * Host-only <stdlib.h> replacement for the threads wrapper build.
 *
 * glibc's real <stdlib.h> declares the C23 once_flag/call_once when
 * __STDC_VERSION__ >= 202311L, which collides with the tree's <threads.h>
 * (OpenBSD's <stdlib.h> does not).  thrd.c only needs malloc() and free()
 * from this header, so a minimal declaration lets the wrapper be built
 * against glibc in C23 mode.  This file is used only by libc/tests; it is
 * not part of the OpenBSD build.
 */

#ifndef _HOST_STDLIB_H_
#define _HOST_STDLIB_H_

#include <stddef.h>

void *malloc(size_t);
void free(void *);

#endif /* !_HOST_STDLIB_H_ */
