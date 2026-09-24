/*
 * fvwm-pipebuf-tests.c -- host-testable unit tests for the
 * PipeRead line-assembly buffer (fvwm/fvwm/pipebuf.c), which turns
 * arbitrary stream chunks into bounded command lines.
 *
 * Covers partial reads, multiple lines per chunk, split lines,
 * EOF without final newline, NUL handling, oversized lines, and
 * the backpressure-related boundedness of the buffer.
 */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../fvwm/fvwm/pipebuf.c"

static int failures;

#define CHECK(cond, msg) do {						\
	if (!(cond)) {							\
		fprintf(stderr, "FAIL: %s (%s:%d)\n", msg, __FILE__,	\
		    __LINE__);						\
		failures++;						\
	}								\
} while (0)

static int
next(struct pipebuf *pb, char **linep, size_t *lenp, size_t maxline)
{
	return pipebuf_next_line(pb, linep, lenp, maxline);
}

static void
test_split_lines(void)
{
	/* "one\n" then "tw" then "o\nthree\n" -> three lines. */
	struct pipebuf pb;
	char *line;
	size_t len;

	pipebuf_init(&pb);
	pipebuf_append(&pb, "one\n", 4);
	CHECK(next(&pb, &line, &len, 64) == 1, "first line ready");
	CHECK(len == 4 && memcmp(line, "one\n", 4) == 0, "first line bytes");

	pipebuf_append(&pb, "tw", 2);
	CHECK(next(&pb, &line, &len, 64) == 0, "partial line waits");
	pipebuf_append(&pb, "o\nthree\n", 8);
	CHECK(next(&pb, &line, &len, 64) == 1, "second line ready");
	CHECK(len == 4 && memcmp(line, "two\n", 4) == 0, "second line bytes");
	CHECK(next(&pb, &line, &len, 64) == 1, "third line ready");
	CHECK(len == 6 && memcmp(line, "three\n", 6) == 0, "third line bytes");
	CHECK(next(&pb, &line, &len, 64) == 0, "buffer drained");

	/* EOF without final newline is flushed by the caller. */
	pipebuf_append(&pb, "tail", 4);
	CHECK(next(&pb, &line, &len, 64) == 0, "no newline waits");
	pipebuf_free(&pb);
}

static void
test_multiple_lines_per_chunk(void)
{
	struct pipebuf pb;
	char *line;
	size_t len;

	pipebuf_init(&pb);
	pipebuf_append(&pb, "a\nb\nc\nd\n", 8);
	for (int i = 0; i < 4; i++) {
		CHECK(next(&pb, &line, &len, 64) == 1, "line available");
		CHECK(len == 2, "line length");
		CHECK(line[0] == 'a' + i, "line content");
		CHECK(line[1] == '\n', "line newline");
	}
	CHECK(next(&pb, &line, &len, 64) == 0, "buffer drained");
	pipebuf_free(&pb);
}

static void
test_long_line(void)
{
	struct pipebuf pb;
	char chunk[4096];
	char *line;
	size_t len, total = 0, want;

	/* A 12288-byte line without newline is returned in bounded
	 * 64-byte fragments, never all at once. */
	pipebuf_init(&pb);
	memset(chunk, 'x', sizeof(chunk));
	for (int i = 0; i < 3; i++)
		pipebuf_append(&pb, chunk, sizeof(chunk));
	pipebuf_append(&pb, "\n", 1);
	want = 3 * sizeof(chunk);

	while (next(&pb, &line, &len, 64) == 1) {
		if (len == 1 && line[0] == '\n')
			break;
		CHECK(len == 64, "fragment bounded at maxline");
		CHECK(line[0] == 'x', "fragment content");
		total += len;
	}
	CHECK(total == want, "all fragments accounted for");
	CHECK(next(&pb, &line, &len, 64) == 0, "buffer drained");
	pipebuf_free(&pb);
}

static void
test_nul_handling(void)
{
	struct pipebuf pb;
	char *line;
	size_t len;

	/* "Exec foo\0garbage\nExec bar\n": the effective first command
	 * is "Exec foo"; the rest of the physical line is discarded. */
	pipebuf_init(&pb);
	pipebuf_append(&pb, "Exec foo\0garbage\nExec bar\n", 26);
	CHECK(next(&pb, &line, &len, 64) == 1, "nul line ready");
	CHECK(len == 8 && memcmp(line, "Exec foo", 8) == 0,
	    "nul truncates command");
	CHECK(next(&pb, &line, &len, 64) == 1, "next line ready");
	CHECK(len == 9 && memcmp(line, "Exec bar\n", 9) == 0,
	    "line after discard intact");
	pipebuf_free(&pb);

	/* NUL discard region larger than one chunk. */
	pipebuf_init(&pb);
	pipebuf_append(&pb, "A\0", 2);
	pipebuf_append(&pb, "0123456789", 10);
	pipebuf_append(&pb, "0123456789\nB\n", 13);
	CHECK(next(&pb, &line, &len, 64) == 1, "nul line ready 2");
	CHECK(len == 1 && line[0] == 'A', "command prefix");
	CHECK(next(&pb, &line, &len, 64) == 1, "post-discard line ready");
	CHECK(len == 2 && memcmp(line, "B\n", 2) == 0,
	    "post-discard line content");
	pipebuf_free(&pb);
}

static void
test_continuation_visible(void)
{
	/* The buffer itself hands the caller the physical lines; the
	 * backslash-newline continuation is the caller's job.  Just
	 * make sure lines with trailing backslash come back intact. */
	struct pipebuf pb;
	char *line;
	size_t len;

	pipebuf_init(&pb);
	pipebuf_append(&pb, "cmd \\\narg\n", 10);
	CHECK(next(&pb, &line, &len, 64) == 1, "continued line ready");
	CHECK(len == 6 && memcmp(line, "cmd \\\n", 6) == 0,
	    "backslash newline preserved");
	CHECK(next(&pb, &line, &len, 64) == 1, "continuation line ready");
	CHECK(len == 4 && memcmp(line, "arg\n", 4) == 0,
	    "continuation content");
	pipebuf_free(&pb);
}

static void
test_empty_and_edges(void)
{
	struct pipebuf pb;
	char *line;
	size_t len;

	pipebuf_init(&pb);
	CHECK(next(&pb, &line, &len, 64) == 0, "empty buffer waits");
	pipebuf_append(&pb, "", 0);
	CHECK(next(&pb, &line, &len, 64) == 0, "zero-length append noop");

	/* Empty line. */
	pipebuf_append(&pb, "\n", 1);
	CHECK(next(&pb, &line, &len, 64) == 1, "empty line ready");
	CHECK(len == 1 && line[0] == '\n', "empty line bytes");

	/* maxline 0 is invalid input, must not crash. */
	CHECK(next(&pb, &line, &len, 0) == 0, "maxline 0 waits");
	pipebuf_free(&pb);
}

int
main(void)
{
	test_split_lines();
	test_multiple_lines_per_chunk();
	test_long_line();
	test_nul_handling();
	test_continuation_visible();
	test_empty_and_edges();

	if (failures != 0) {
		fprintf(stderr, "fvwm-pipebuf-tests: %d failure(s)\n",
		    failures);
		return 1;
	}
	printf("fvwm-pipebuf-tests: all tests passed\n");
	return 0;
}
