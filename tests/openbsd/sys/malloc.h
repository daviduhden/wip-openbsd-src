/*
 * sys/malloc.h -- host-test shim.  NOT part of the production build.
 */
#ifndef EXT4FS_TEST_MALLOC_H
#define EXT4FS_TEST_MALLOC_H

// clang-format off
#include <stdlib.h>
// clang-format on

#define M_TEMP 0
#define M_WAITOK 0x0000
#define M_ZERO 0x0020
#define M_CANFAIL 0x0040

/* Supply zeroed storage for tests without modeling kernel allocation flags. */
#define mallocarray(nmemb, size, type, flags) calloc((nmemb), (size))
#define free(addr, type, freedsize) free((addr))

#endif /* EXT4FS_TEST_MALLOC_H */
