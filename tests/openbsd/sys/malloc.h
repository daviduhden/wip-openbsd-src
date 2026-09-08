/*
 * sys/malloc.h -- host-test shim.  NOT part of the production build.
 */
#ifndef EXT4FS_TEST_MALLOC_H
#define EXT4FS_TEST_MALLOC_H

#include <stdlib.h>

#define	M_TEMP		0
#define	M_WAITOK	0x0000
#define	M_ZERO		0x0020

/* Map the three-argument kernel allocation API onto libc. */
#define	mallocarray(nmemb, size, type, flags)	calloc((nmemb), (size))
#define	free(addr, type, freedsize)		free((addr))

#endif /* EXT4FS_TEST_MALLOC_H */
