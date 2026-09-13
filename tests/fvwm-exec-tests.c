/*
 * fvwm-exec-tests.c -- host-testable unit tests for the fvwm_exec
 * imsg payload decoder (decode_exec_msg) in fvwm/fvwm/fvwm_exec.c.
 *
 * Exercises the trust boundary between fvwm and the execution
 * helper with hostile payloads: oversized counts, missing NUL
 * terminators, truncated messages, and exact-fit boundaries.
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "cdefs-shim.h"

/* strtonum lives in glibc (>= 2.38) but its declaration needs
 * _GNU_SOURCE; declare it here for the include below. */
long long strtonum(const char *, long long, long long, const char **);

/* fvwm_exec.c internals under test; its main() is renamed. */
#define main fvwm_exec_main
#include "../fvwm/fvwm/fvwm_exec.c"
#undef main

/* Link stubs for the sandbox calls referenced by fvwm_exec.c. */
int
pledge(const char *p, const char *q)
{
	(void)p;
	(void)q;
	return 0;
}

int
unveil(const char *p, const char *q)
{
	(void)p;
	(void)q;
	return 0;
}

static int failures;

#define CHECK(cond, msg) do {						\
	if (!(cond)) {							\
		fprintf(stderr, "FAIL: %s (%s:%d)\n", msg, __FILE__,	\
		    __LINE__);						\
		failures++;						\
	}								\
} while (0)

static void
run_decode(const char *payload, size_t len, int want_rc, int want_argc,
    int want_envc, const char *want_argv0)
{
	struct imsg imsg;
	char **av = NULL, **ev = NULL;
	char *buf;
	int rc;

	buf = malloc(len);
	memcpy(buf, payload, len);
	memset(&imsg, 0, sizeof(imsg));
	imsg.data = buf;
	imsg.hdr.len = IMSG_HEADER_SIZE + len;

	rc = decode_exec_msg(&imsg, &av, &ev);
	CHECK(rc == want_rc, "decode return value");
	if (rc == 0) {
		int argc = 0, envc = 0;

		while (av[argc] != NULL)
			argc++;
		while (ev[envc] != NULL)
			envc++;
		CHECK(argc == want_argc, "argc");
		CHECK(envc == want_envc, "envc");
		if (want_argv0 != NULL)
			CHECK(strcmp(av[0], want_argv0) == 0, "argv[0]");
		free(av);
		free(ev);
	}
	free(buf);
}

static size_t
mk_payload(char *buf, int cargc, int envc, const char **argv,
    const char **envp)
{
	char *p = buf;

	memcpy(p, &cargc, 4);
	p += 4;
	memcpy(p, &envc, 4);
	p += 4;
	for (int i = 0; i < cargc; i++) {
		memcpy(p, argv[i], strlen(argv[i]) + 1);
		p += strlen(argv[i]) + 1;
	}
	for (int i = 0; i < envc; i++) {
		memcpy(p, envp[i], strlen(envp[i]) + 1);
		p += strlen(envp[i]) + 1;
	}
	return (p - buf);
}

static void
test_valid_payload(void)
{
	char payload[512];
	const char *av[] = { "/bin/sh", "-c", "xterm" };
	const char *ev[] = { "HOME=/tmp", "PATH=/usr/bin" };
	size_t len;

	len = mk_payload(payload, 3, 2, av, ev);
	run_decode(payload, len, 0, 3, 2, "/bin/sh");
}

static void
test_short_header(void)
{
	char payload[8] = { 0 };

	run_decode(payload, 4, -1, 0, 0, NULL);
}

static void
test_negative_argc(void)
{
	char payload[64];
	int cargc = -1, envc = 0;

	memcpy(payload, &cargc, 4);
	memcpy(payload + 4, &envc, 4);
	payload[8] = '\0';
	run_decode(payload, 9, -1, 0, 0, NULL);
}

static void
test_huge_argc(void)
{
	char payload[64];
	int cargc = 0x7fffffff, envc = 0;

	memcpy(payload, &cargc, 4);
	memcpy(payload + 4, &envc, 4);
	run_decode(payload, 64, -1, 0, 0, NULL);
}

static void
test_missing_nul(void)
{
	char payload[64];
	int cargc = 1, envc = 0;
	int i;

	memcpy(payload, &cargc, 4);
	memcpy(payload + 4, &envc, 4);
	for (i = 8; i < 64; i++)
		payload[i] = 'A';
	run_decode(payload, 64, -1, 0, 0, NULL);
}

static void
test_argc_exceeding_payload(void)
{
	char payload[64];
	int cargc = 100, envc = 0;
	int i;

	memcpy(payload, &cargc, 4);
	memcpy(payload + 4, &envc, 4);
	for (i = 8; i < 64; i++)
		payload[i] = '\0';
	run_decode(payload, 64, -1, 0, 0, NULL);
}

static void
test_zero_argc(void)
{
	char payload[64];
	int cargc = 0, envc = 0;

	memcpy(payload, &cargc, 4);
	memcpy(payload + 4, &envc, 4);
	run_decode(payload, 8, -1, 0, 0, NULL);
}

static void
test_exact_boundary(void)
{
	/* One empty argv[0] exactly fills the payload: must decode. */
	char payload[64];
	int cargc = 1, envc = 0;

	memcpy(payload, &cargc, 4);
	memcpy(payload + 4, &envc, 4);
	payload[8] = '\0';
	run_decode(payload, 9, 0, 1, 0, "");
}

static void
run_piperead_decode(const char *payload, size_t len, int want_rc,
    const char *want_cmd)
{
	struct imsg imsg;
	char *cmd;

	memset(&imsg, 0, sizeof(imsg));
	imsg.data = (char *)payload;
	imsg.hdr.len = IMSG_HEADER_SIZE + len;

	cmd = decode_piperead_msg(&imsg);
	if (want_rc == -1) {
		CHECK(cmd == NULL, "piperead decode rejects payload");
	} else {
		CHECK(cmd != NULL, "piperead decode accepts payload");
		if (cmd != NULL) {
			CHECK(strcmp(cmd, want_cmd) == 0,
			    "piperead decode command matches");
			free(cmd);
		}
	}
}

static void
test_piperead_decode(void)
{
	char payload[256];
	u_int32_t id = 0x11223344;
	const char *cmd = "xterm -e /bin/sh";

	memcpy(payload, &id, 4);
	memcpy(payload + 4, cmd, strlen(cmd) + 1);
	run_piperead_decode(payload, 4 + strlen(cmd) + 1, 0, cmd);

	/* Truncated id. */
	run_piperead_decode(payload, 3, -1, NULL);

	/* Exactly the id and nothing else. */
	run_piperead_decode(payload, 4, -1, NULL);

	/* Id plus no NUL terminator for the command. */
	payload[4] = 'A';
	run_piperead_decode(payload, 5, -1, NULL);

	/* Empty command is valid. */
	payload[4] = '\0';
	run_piperead_decode(payload, 5, 0, "");

	/* Oversized command is caught by the caller-side length check
	 * in exec_helper_piperead_start(), not the decoder; the
	 * decoder itself must still reject non-terminated data at
	 * any length. */
	memset(payload + 4, 'B', sizeof(payload) - 4);
	run_piperead_decode(payload, sizeof(payload), -1, NULL);
}

int
main(void)
{
	test_valid_payload();
	test_short_header();
	test_negative_argc();
	test_huge_argc();
	test_missing_nul();
	test_argc_exceeding_payload();
	test_zero_argc();
	test_exact_boundary();
	test_piperead_decode();

	if (failures != 0) {
		fprintf(stderr, "%d failure(s)\n", failures);
		return 1;
	}
	printf("fvwm_exec decode tests: OK\n");
	return 0;
}

/* Link stubs for the libutil imsg API (not exercised by the tests). */
long long
strtonum(const char *n, long long lo, long long hi,
    const char **es)
{
	(void)n;
	(void)lo;
	(void)hi;
	(void)es;
	return 0;
}

int
imsgbuf_init(struct imsgbuf *b, int fd)
{
	(void)b;
	(void)fd;
	return 0;
}

int
imsgbuf_read(struct imsgbuf *b)
{
	(void)b;
	return 0;
}

int
imsgbuf_flush(struct imsgbuf *b)
{
	(void)b;
	return 0;
}

void
imsgbuf_clear(struct imsgbuf *b)
{
	(void)b;
}

int
imsgbuf_get(struct imsgbuf *b, struct imsg *m)
{
	(void)b;
	(void)m;
	return 0;
}

int
imsg_compose(struct imsgbuf *b, uint32_t t, uint32_t p, pid_t pid, int fd,
    const void *d, uint16_t l)
{
	(void)b;
	(void)t;
	(void)p;
	(void)pid;
	(void)fd;
	(void)d;
	(void)l;
	return 0;
}

void
imsg_free(struct imsg *m)
{
	(void)m;
}
