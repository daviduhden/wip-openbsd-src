/*
 * fvwm-exec-tests.c -- host-testable unit tests for the fvwm_exec
 * imsg payload decoder (decode_exec_msg) in fvwm/fvwm/fvwm_exec.c.
 *
 * Exercises the trust boundary between fvwm and the execution
 * helper with hostile payloads: oversized counts, missing NUL
 * terminators, truncated messages, and exact-fit boundaries.
 */
// clang-format off
#include <assert.h>
#include <stdio.h>
#include <string.h>
// clang-format on

/* Declare the local strtonum stub before including the helper, whose
 * entry point refers to it even though these tests call the decoder. */
long long strtonum(const char *, long long, long long, const char **);

/* fvwm_exec.c internals under test; its main() is renamed. */
#define main fvwm_exec_main
// clang-format off
#include "../fvwm/fvwm/fvwm_exec.c"
// clang-format on
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

#define CHECK(cond, msg)                                                       \
	do {                                                                   \
		if (!(cond)) {                                                 \
			fprintf(stderr, "FAIL: %s (%s:%d)\n", msg, __FILE__,   \
			    __LINE__);                                         \
			failures++;                                            \
		}                                                              \
	} while (0)

static void
run_decode(const char *payload, size_t len, int want_rc, int want_argc,
    int want_envc, const char *want_argv0)
{
	struct imsg imsg;
	char	  **av = NULL, **ev = NULL;
	char	   *buf;
	int	    rc;

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
mk_payload(char *buf, int cargc, int envc, const char **argv, const char **envp)
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
	char	    payload[512];
	const char *av[] = {"/bin/sh", "-c", "xterm"};
	const char *ev[] = {"HOME=/tmp", "PATH=/usr/bin"};
	size_t	    len;

	len = mk_payload(payload, 3, 2, av, ev);
	run_decode(payload, len, 0, 3, 2, "/bin/sh");
}

static void
test_short_header(void)
{
	char payload[8] = {0};

	run_decode(payload, 4, -1, 0, 0, NULL);
}

static void
test_negative_argc(void)
{
	char payload[64];
	int  cargc = -1, envc = 0;

	memcpy(payload, &cargc, 4);
	memcpy(payload + 4, &envc, 4);
	payload[8] = '\0';
	run_decode(payload, 9, -1, 0, 0, NULL);
}

static void
test_huge_argc(void)
{
	char payload[64];
	int  cargc = 0x7fffffff, envc = 0;

	memcpy(payload, &cargc, 4);
	memcpy(payload + 4, &envc, 4);
	run_decode(payload, 64, -1, 0, 0, NULL);
}

static void
test_missing_nul(void)
{
	char payload[64];
	int  cargc = 1, envc = 0;
	int  i;

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
	int  cargc = 100, envc = 0;
	int  i;

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
	int  cargc = 0, envc = 0;

	memcpy(payload, &cargc, 4);
	memcpy(payload + 4, &envc, 4);
	run_decode(payload, 8, -1, 0, 0, NULL);
}

static void
test_exact_boundary(void)
{
	/* One empty argv[0] exactly fills the payload: must decode. */
	char payload[64];
	int  cargc = 1, envc = 0;

	memcpy(payload, &cargc, 4);
	memcpy(payload + 4, &envc, 4);
	payload[8] = '\0';
	run_decode(payload, 9, 0, 1, 0, "");
}

static void
test_piperead_decode(void)
{
	char payload[256], *command;
	const char expected[] = "xterm -e /bin/sh";
	struct imsg imsg = {0};
	u_int32_t id = 0x11223344, decoded_id;

	memcpy(payload, &id, sizeof(id));
	memcpy(payload + sizeof(id), expected, sizeof(expected));
	imsg.data = payload;
	imsg.hdr.len = IMSG_HEADER_SIZE + sizeof(id) + sizeof(expected);
	command = decode_piperead_msg(&imsg, &decoded_id);
	CHECK(command != NULL && strcmp(command, expected) == 0,
	    "PipeRead decode accepts command");
	CHECK(decoded_id == id, "PipeRead frame ID");
	imsg.hdr.len--;
	CHECK(decode_piperead_msg(&imsg, &decoded_id) == NULL,
	    "PipeRead rejects unterminated command");
	imsg.hdr.len = IMSG_HEADER_SIZE + sizeof(id) - 1;
	CHECK(decode_piperead_msg(&imsg, &decoded_id) == NULL,
	    "PipeRead rejects truncated ID");
}

static void
test_piperead_environment(void)
{
	const char var[] = "B=two";
	struct piperead_env env = {0};
	char **envp, *entry;
	size_t len = MAX_EXEC_PAYLOAD + 16;

	env.limit = len + sizeof(var);
	entry = malloc(len);
	if (entry == NULL)
		err(1, "malloc");
	memset(entry, 'x', len);
	memcpy(entry, "A=", 2);
	entry[len - 1] = '\0';

	piperead_env_append(&env, NULL, 0);
	piperead_env_append(&env, entry, MAX_EXEC_PAYLOAD);
	CHECK(piperead_env_vector(&env) == NULL,
	    "PipeRead rejects incomplete environment");
	piperead_env_append(&env, entry + MAX_EXEC_PAYLOAD,
	    len - MAX_EXEC_PAYLOAD);
	piperead_env_append(&env, var, sizeof(var));
	envp = piperead_env_vector(&env);
	CHECK(envp != NULL, "PipeRead accepts environment over one frame");
	if (envp != NULL) {
		CHECK(strcmp(envp[0], entry) == 0 && strcmp(envp[1], var) == 0 &&
		    envp[2] == NULL, "PipeRead joins split environment strings");
		free(envp);
	}
	piperead_env_append(&env, var, sizeof(var));
	CHECK(piperead_env_vector(&env) == NULL,
	    "PipeRead rejects aggregate environment over the limit");
	piperead_env_append(&env, NULL, 0);
	envp = piperead_env_vector(&env);
	CHECK(envp != NULL && envp[0] == NULL,
	    "PipeRead resets failed staging before the next request");
	free(envp);
	free(env.data);
	free(entry);
}

static void
test_piperead_child(void)
{
	struct imsgbuf ibuf = {0};
	struct piperead_slot *slot = &piperead_slots[0];
	char *envp[] = {"FVWM_TEST_VAR=value", NULL};
	char command[] = "printf '%s' \"$FVWM_TEST_VAR\"; exit 3";
	char output[sizeof("value")];
	sigset_t set, oldset;
	struct sigaction sa = {0}, oldsa;
	pid_t pid;
	ssize_t len;
	int signo;

	sa.sa_handler = sigchld_handler;
	sigemptyset(&sa.sa_mask);
	if (sigaction(SIGCHLD, &sa, &oldsa) == -1)
		err(1, "sigaction");
	sigemptyset(&set);
	sigaddset(&set, SIGCHLD);
	if (sigprocmask(SIG_BLOCK, &set, &oldset) == -1)
		err(1, "sigprocmask");

	alarm(10);
	if (piperead_start(&ibuf, 1, command, envp) == -1)
		errx(1, "piperead_start");
	pid = slot->pid;
	if (sigwait(&set, &signo) != 0)
		errx(1, "sigwait");
	reap_children(&ibuf);
	len = read(slot->fd, output, sizeof(output));
	CHECK(len == 5 && memcmp(output, "value", 5) == 0,
	    "PipeRead child received environment");
	CHECK(slot->pid == 0 && slot->status_known &&
	    WIFEXITED(slot->status) && WEXITSTATUS(slot->status) == 3,
	    "active PipeRead retained wait status");
	CHECK(waitpid(pid, NULL, WNOHANG) == -1 && errno == ECHILD,
	    "active PipeRead child reaped");
	piperead_finish(&ibuf, slot, 0);

	pid = fork();
	if (pid == -1)
		err(1, "fork");
	if (pid == 0)
		_exit(0);
	if (sigwait(&set, &signo) != 0)
		errx(1, "sigwait");
	reap_children(&ibuf);
	CHECK(waitpid(pid, NULL, WNOHANG) == -1 && errno == ECHILD,
	    "child without reporting slot reaped");
	alarm(0);
	if (sigprocmask(SIG_SETMASK, &oldset, NULL) == -1)
		err(1, "sigprocmask");
	if (sigaction(SIGCHLD, &oldsa, NULL) == -1)
		err(1, "sigaction");
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
	test_piperead_environment();
	test_piperead_child();

	if (failures != 0) {
		fprintf(stderr, "%d failure(s)\n", failures);
		return 1;
	}
	printf("fvwm_exec tests: OK\n");
	return 0;
}

/* Link stubs for the helper entry point. The tests bypass transport. */
long long
strtonum(const char *n, long long lo, long long hi, const char **es)
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
