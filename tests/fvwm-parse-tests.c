/*
 * fvwm-parse-tests.c -- host-testable unit tests for the pure FVWM
 * configuration parsing logic in libs/Parse.c.
 *
 * These tests only exercise the tokenizer and integer-argument
 * helpers, which have no X11 dependency at runtime.  They run on
 * any host with a C compiler and the X11 development headers
 * (for type definitions only).
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* strtonum lives in glibc (>= 2.38) but its declaration needs
 * _GNU_SOURCE; declare it here for the include below. */
long long strtonum(const char *, long long, long long, const char **);

#include "Parse.c"

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
test_basic_tokens(void)
{
	char  buf[] = "one two three";
	char *s = buf, *tok;

	s = GetNextToken(s, &tok);
	CHECK(s != NULL && tok != NULL && strcmp(tok, "one") == 0, "tok 1");
	free(tok);
	s = GetNextToken(s, &tok);
	CHECK(tok != NULL && strcmp(tok, "two") == 0, "tok 2");
	free(tok);
	s = GetNextToken(s, &tok);
	CHECK(tok != NULL && strcmp(tok, "three") == 0, "tok 3");
	free(tok);
	s = GetNextToken(s, &tok);
	CHECK(tok == NULL, "tok end");
}

static void
test_quoting(void)
{
	char  buf[] = "one \"two three\" four";
	char *s = buf, *tok;

	s = GetNextToken(s, &tok);
	CHECK(tok != NULL && strcmp(tok, "one") == 0, "q tok 1");
	free(tok);
	s = GetNextToken(s, &tok);
	CHECK(tok != NULL && strcmp(tok, "two three") == 0, "q tok 2");
	free(tok);
	s = GetNextToken(s, &tok);
	CHECK(tok != NULL && strcmp(tok, "four") == 0, "q tok 3");
	free(tok);
}

static void
test_escapes(void)
{
	char  buf[] = "a\\ b c";
	char *s = buf, *tok;

	s = GetNextToken(s, &tok);
	CHECK(tok != NULL && strcmp(tok, "a b") == 0, "escaped space");
	free(tok);
	s = GetNextToken(s, &tok);
	CHECK(tok != NULL && strcmp(tok, "c") == 0, "after escape");
	free(tok);
}

static void
test_empty_and_missing(void)
{
	char  buf[] = "  ";
	char *s = buf, *tok;

	s = GetNextToken(s, &tok);
	CHECK(tok == NULL && s != NULL, "empty input");
	s = GetNextToken(NULL, &tok);
	CHECK(tok == NULL && s == NULL, "NULL input");
}

static void
test_integers(void)
{
	char  buf[] = "3 4 -5";
	char *s = buf;
	int   ret[4];
	int   n;

	n = GetIntegerArguments(s, &s, ret, 4);
	CHECK(n == 3, "three integers");
	CHECK(ret[0] == 3 && ret[1] == 4 && ret[2] == -5, "integer values");

	/* Truncated input must never run past the buffer. */
	{
		char  bad[] = "1 2 3 4 5";
		char *t = bad;

		n = GetIntegerArguments(t, &t, ret, 4);
		CHECK(n == 4, "clamped to buffer size");
	}
}

static void
test_long_lines(void)
{
	char  big[8192];
	char *tok;

	memset(big, 'x', sizeof(big) - 2);
	big[sizeof(big) - 2] = '\0';
	GetNextToken(big, &tok);
	CHECK(tok != NULL && strlen(tok) == sizeof(big) - 2, "long token");
	free(tok);
}

/*
 * The FvwmParseInteger matrix: historical prefix semantics with
 * defined overflow.  Value checks plus errno classification.
 */
static void
check_int(const char *s, int want, const char *msg)
{
	int got;

	errno = 0;
	got = FvwmParseInteger(s);
	CHECK(got == want, msg);
}

static void
check_int_errno(const char *s, int want_errno, const char *msg)
{
	errno = 0;
	(void)FvwmParseInteger(s);
	CHECK(errno == want_errno, msg);
}

static void
test_fvwm_parse_integer(void)
{
	check_int("0", 0, "zero");
	check_int("5", 5, "positive");
	check_int("-5", -5, "negative");
	check_int("+7", 7, "explicit plus");
	check_int("  42", 42, "leading whitespace");
	check_int("42px", 42, "trailing syntax (prefix)");
	check_int("42 43", 42, "trailing token (prefix)");
	check_int("  -12 ", -12, "whitespace and sign");
	check_int("2147483647", 2147483647, "INT_MAX");
	check_int("-2147483648", (-2147483647 - 1), "INT_MIN");
	check_int("99999999999999999999", 0, "overflow clamps to 0");
	check_int_errno("99999999999999999999", ERANGE, "overflow sets ERANGE");
	check_int("-99999999999999999999", 0, "negative overflow clamps to 0");
	check_int_errno(
	    "-99999999999999999999", ERANGE, "negative overflow sets ERANGE");
	check_int("2147483648", 0, "INT_MAX + 1 clamps to 0");
	check_int_errno("2147483648", ERANGE, "INT_MAX + 1 sets ERANGE");
	check_int("-2147483649", 0, "INT_MIN - 1 clamps to 0");
	check_int_errno("-2147483649", ERANGE, "INT_MIN - 1 sets ERANGE");
	check_int("", 0, "empty input is 0");
	check_int_errno("", EINVAL, "empty input sets EINVAL");
	check_int("abc", 0, "non-numeric is 0");
	check_int_errno("abc", EINVAL, "non-numeric sets EINVAL");
	check_int("-", 0, "bare sign is 0");
	check_int_errno("-", EINVAL, "bare sign sets EINVAL");
	check_int("0x10", 0, "hex prefix reads 0");
	check_int("007", 7, "octal-looking input is decimal");
	{
		char big[300];

		memset(big, '9', sizeof(big) - 1);
		big[sizeof(big) - 1] = '\0';
		check_int(big, 0, "very long decimal clamps to 0");
		check_int_errno(big, ERANGE, "very long decimal sets ERANGE");
	}
}

int
main(void)
{
	test_basic_tokens();
	test_quoting();
	test_escapes();
	test_empty_and_missing();
	test_integers();
	test_long_lines();
	test_fvwm_parse_integer();

	if (failures != 0) {
		fprintf(stderr, "%d failure(s)\n", failures);
		return 1;
	}
	printf("fvwm parser tests: OK\n");
	return 0;
}

/* Link stub for strtonum(3) (not exercised by the tested paths). */
long long
strtonum(const char *n, long long lo, long long hi, const char **es)
{
	(void)n;
	(void)lo;
	(void)hi;
	(void)es;
	return 0;
}
