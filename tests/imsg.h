/*
 * imsg.h -- test declarations for the imsg(3) fields used by the
 * fvwm_exec decoder. The test constructs messages in memory.
 *
 * The host test include path selects this header on every platform.
 * The production build uses the OpenBSD libutil header and transport.
 * The shim makes no general ABI compatibility guarantee.
 */
#ifndef STUB_IMSG_H
#define STUB_IMSG_H

// clang-format off
#include <sys/types.h>

#include <stdint.h>
// clang-format on

struct imsghdr {
	uint32_t type;
	uint16_t len;
	uint16_t flags;
	uint32_t peerid;
	uint32_t pid;
};

#define MAX_IMSGSIZE 16384
#define IMSG_HEADER_SIZE 16

struct imsg {
	struct imsghdr hdr;
	int fd;
	void *data;
};

struct imsgbuf {
	int fd;
};

int imsgbuf_init(struct imsgbuf *, int);
int imsgbuf_read(struct imsgbuf *);
int imsgbuf_flush(struct imsgbuf *);
void imsgbuf_clear(struct imsgbuf *);
int imsgbuf_get(struct imsgbuf *, struct imsg *);
int imsg_compose(
    struct imsgbuf *, uint32_t, uint32_t, pid_t, int, const void *, uint16_t);
void imsg_free(struct imsg *);

#endif
