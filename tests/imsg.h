/*
 * stub-imsg.h -- minimal test-support declarations for the libutil
 * imsg(3) API used by the fvwm_exec decoder tests.
 *
 * NOT part of the production build.  On OpenBSD the real
 * /usr/include/imsg.h is used; this header exists only so the
 * pure decoding logic can be exercised on non-OpenBSD hosts.
 */
#ifndef STUB_IMSG_H
#define STUB_IMSG_H

#include <sys/types.h>
#include <stdint.h>

struct imsghdr {
	uint32_t type;
	uint16_t len;
	uint16_t flags;
	uint32_t peerid;
	uint32_t pid;
};

#define IMSG_HEADER_SIZE 16

struct imsg {
	struct imsghdr hdr;
	int fd;
	void *data;
};

struct imsgbuf {
	int fd;
};

int	 imsgbuf_init(struct imsgbuf *, int);
int	 imsgbuf_read(struct imsgbuf *);
int	 imsgbuf_flush(struct imsgbuf *);
void	 imsgbuf_clear(struct imsgbuf *);
int	 imsgbuf_get(struct imsgbuf *, struct imsg *);
int	 imsg_compose(struct imsgbuf *, uint32_t, uint32_t, pid_t, int,
    const void *, uint16_t);
void	 imsg_free(struct imsg *);

#endif
