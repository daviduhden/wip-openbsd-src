/*
** Strings.c: various routines for dealing with strings
*/

// clang-format off
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fvwmlib.h"
// clang-format on

/************************************************************************
 *
 * Concatentates 3 strings
 *
 *************************************************************************/
char CatS[256];

char *
CatString3(char *a, char *b, char *c)
{
	int len = 0;

	if (a != nullptr)
		len += strlen(a);
	if (b != nullptr)
		len += strlen(b);
	if (c != nullptr)
		len += strlen(c);

	if (len > 255)
		return nullptr;

	if (a == nullptr)
		CatS[0] = 0;
	else
		strlcpy(CatS, a, sizeof(CatS));
	if (b != nullptr)
		strlcat(CatS, b, sizeof(CatS));
	if (c != nullptr)
		strlcat(CatS, c, sizeof(CatS));
	return CatS;
}

/***************************************************************************
 * A simple routine to copy a string, stripping spaces and mallocing
 * space for the new string
 ***************************************************************************/
void
CopyString(char **dest, char *source)
{
	int   len;
	char *start;

	if (source == nullptr) {
		*dest = nullptr;
		return;
	}
	while (((isspace((unsigned char)*source)) && (*source != '\n')) &&
	    (*source != 0)) {
		source++;
	}
	len = 0;
	start = source;
	while ((*source != '\n') && (*source != 0)) {
		len++;
		source++;
	}

	source--;
	while (
	    (len > 0) && (isspace((unsigned char)*source)) && (*source != 0)) {
		len--;
		source--;
	}
	*dest = xmalloc(len + 1);
	strncpy(*dest, start, len);
	(*dest)[len] = 0;
}

/****************************************************************************
 *
 * Copies a string into a new, malloc'ed string
 * Strips leading spaces and trailing spaces and new lines
 *
 ****************************************************************************/
char *
stripcpy(char *source)
{
	char *tmp, *ptr;
	int   len;

	if (source == nullptr)
		return nullptr;

	while (isspace((unsigned char)*source))
		source++;
	len = strlen(source);
	if (len == 0) {
		ptr = xmalloc(1);
		ptr[0] = 0;
		return ptr;
	}
	tmp = source + len - 1;
	while ((tmp >= source) &&
	    ((isspace((unsigned char)*tmp)) || (*tmp == '\n'))) {
		tmp--;
		len--;
	}
	ptr = xmalloc(len + 1);
	strncpy(ptr, source, len);
	ptr[len] = 0;
	return ptr;
}

int
StrEquals(char *s1, char *s2)
{
	if (!s1 && !s2)
		return 1;
	if (!s1 || !s2)
		return 0;
	return (strcasecmp(s1, s2) == 0);
}
