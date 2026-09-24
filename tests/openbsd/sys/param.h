/*
 * sys/param.h -- host-test shim for the OpenBSD kernel headers.
 * NOT part of the production build.  Only what the ext4fs test
 * harness needs is defined here.
 */
#ifndef EXT4FS_TEST_PARAM_H
#define EXT4FS_TEST_PARAM_H

#include <sys/types.h>

#include <endian.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>

typedef uint32_t ufsino_t;

#define DEV_BSIZE 512
#define MAXBSIZE 65536

/* Memory pool type referenced (extern only) by the ext4fs headers. */
struct pool {
	int pr_dummy;
};

#define howmany(x, y) (((x) + ((y) - 1)) / (y))
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#define nitems(_a) (sizeof((_a)) / sizeof((_a)[0]))
#define roundup(x, y) ((((x) + ((y) - 1)) / (y)) * (y))

/* OpenBSD conversion names map onto the host endian.h functions.
 * On OpenBSD they already exist in <sys/endian.h>, so only define
 * the ones the host headers do not provide. */
#ifndef betoh16
#define betoh16(x) be16toh((x))
#endif
#ifndef betoh32
#define betoh32(x) be32toh((x))
#endif
#ifndef letoh16
#define letoh16(x) le16toh((x))
#endif
#ifndef letoh32
#define letoh32(x) le32toh((x))
#endif
#ifndef letoh64
#define letoh64(x) le64toh((x))
#endif

#endif /* EXT4FS_TEST_PARAM_H */
