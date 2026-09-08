/*
 * sys/param.h -- host-test shim for the OpenBSD kernel headers.
 * NOT part of the production build.  Only what the ext4fs test
 * harness needs is defined here.
 */
#ifndef EXT4FS_TEST_PARAM_H
#define EXT4FS_TEST_PARAM_H

#include <sys/types.h>
#include <stddef.h>
#include <stdint.h>
#include <errno.h>
#include <endian.h>

typedef uint32_t ufsino_t;

#define DEV_BSIZE	512
#define MAXBSIZE	65536

/* Memory pool type referenced (extern only) by the ext4fs headers. */
struct pool {
	int	pr_dummy;
};

#define	howmany(x, y)	(((x) + ((y) - 1)) / (y))
#define	MIN(a, b)	(((a) < (b)) ? (a) : (b))
#define	MAX(a, b)	(((a) > (b)) ? (a) : (b))
#define	nitems(_a)	(sizeof((_a)) / sizeof((_a)[0]))
#define	roundup(x, y)	((((x) + ((y) - 1)) / (y)) * (y))

/* OpenBSD conversion names map onto the host endian.h functions. */
#define	betoh16(x)	be16toh((x))
#define	betoh32(x)	be32toh((x))
#define	letoh16(x)	le16toh((x))
#define	letoh32(x)	le32toh((x))
#define	letoh64(x)	le64toh((x))

#endif /* EXT4FS_TEST_PARAM_H */
