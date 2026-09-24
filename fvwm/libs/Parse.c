/*
** Parse.c: routines for parsing in fvwm & modules
*/

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "fvwmlib.h"

/*
 * FvwmParseInteger -- historical atoi(3) configuration parsing made
 * explicit and safe.
 *
 * Accepts optional leading whitespace, one optional sign and decimal
 * digits; parsing stops at the first non-digit (prefix semantics,
 * exactly what atoi(3) did with trailing text).  Returns the parsed
 * value, or 0 when nothing numeric is present (atoi() returned 0 for
 * non-numeric input).  Overflow, which was undefined behaviour for
 * atoi(3), is defined here: 0 is returned.
 *
 * errno reports why a value could not be produced: EINVAL when no
 * digit was parsed, ERANGE on overflow.  Callers that do not care
 * can ignore errno and treat the result as atoi()'s value.
 *
 * fvwm's configuration language intentionally accepted numeric
 * prefixes followed by other syntax; nothing in this function is
 * stricter than atoi(3) except the defined overflow result.
 */
int
FvwmParseInteger(const char *string)
{
	const char   *s = string;
	int	      negative = 0;
	unsigned long value = 0;
	int	      digits = 0;

	if (s == NULL)
		return 0;

	while (isspace((unsigned char)*s))
		s++;
	if (*s == '+' || *s == '-') {
		negative = (*s == '-');
		s++;
	}
	while (isdigit((unsigned char)*s)) {
		unsigned long nv;

		digits = 1;
		nv = value * 10 + (unsigned long)(*s - '0');
		if (nv < value ||
		    nv > (negative ? (unsigned long)LONG_MAX + 1 :
				     (unsigned long)LONG_MAX)) {
			/* Historical atoi() overflow was undefined;
			 * report it instead of returning garbage. */
			errno = ERANGE;
			return 0;
		}
		value = nv;
		s++;
	}
	if (digits == 0) {
		errno = EINVAL;
		return 0;
	}

	if (negative) {
		if (value > (unsigned long)INT_MAX + 1) {
			errno = ERANGE;
			return 0;
		}
		if (value == (unsigned long)INT_MAX + 1)
			return INT_MIN;
		return -(int)value;
	}
	if (value > (unsigned long)INT_MAX) {
		errno = ERANGE;
		return 0;
	}
	return (int)value;
}

/*
 * FvwmParseFd -- module-protocol file descriptor argument (argv[1]
 * and argv[2] of every module, supplied by fvwm).  Strict
 * non-negative integer; a module cannot operate without valid
 * descriptors, so a malformed argument is fatal.
 */
int
FvwmParseFd(const char *arg)
{
	const char *errstr;
	int	    fd;

	fd = (int)strtonum(arg, 0, INT_MAX, &errstr);
	if (errstr != NULL) {
		fprintf(stderr,
		    "fvwm module: invalid descriptor "
		    "argument '%s': %s\n",
		    arg, errstr);
		exit(1);
	}
	return fd;
}

/* If the string s begins with a quote chracter SkipQuote returns a pointer
 * to the first unquoted character or to the final '\0'. If it does not, a
 * pointer to the next character in the string is returned.
 * There are three possible types of quoting: a backslash quotes the next
 * character only. Long quotes like " " or ' ' quoting everything in
 * between and quote pairs like ( ) or { }.
 *
 * precedence:
 *
 * 1) Backslashes are honoured always, even inside long or pair quotes.
 * 2) long quotes do quote quoting pair characters but not simple quotes. All
 *    long quotes can quote all other types of long quotes).
 * 3) pair quotes none of the above. Text between a pair of quotes is treated
 *    as a single token.
 *
 * qlong   - string of long quoted (defaults to "'` )
 * qstart  - string of pair quote start characters (defaults to empty string)
 * qend    - string of pair quote end characters (defaults to empty string)
 *
 * The defaults are used if NULL is passed for the corresponding string.
 */
char *
SkipQuote(char *s, const char *qlong, const char *qstart, const char *qend)
{
	char *t;

	if (s == NULL || *s == 0)
		return s;
	if (!qlong)
		qlong = "\"'`";
	if (!qstart)
		qstart = "";
	if (!qend)
		qend = "";

	if (*s == '\\' && s[1] != 0)
		return s + 2;
	else if (*qlong && (t = strchr(qlong, *s))) {
		char c = *t;

		s++;
		while (*s && *s != c) {
			/* Skip over escaped text, ie \quote */
			if (*s == '\\' && *(s + 1) != 0)
				s++;
			s++;
		}
		if (*s == c)
			s++;
		return s;
	} else if (*qstart && (t = strchr(qstart, *s))) {
		char c = *((t - qstart) + qend);

		while (*s && *s != c)
			s = SkipQuote(s, qlong, "", "");
		return (*s == *t) ? ++s : s;
	} else
		return ++s;
}

/* Returns a string up to the first character from the string delims in a
 * malloc'd area just like GetNextToken. Quotes are not removed from the
 * returned string. The returned string is stored in *sout, the return value
 * of this call is a pointer to the first character after the delimiter or
 * to the terminating '\0'. Quoting is handled like in SkipQuote. */
char *
GetQuotedString(char *sin, char **sout, const char *delims, const char *qlong,
    const char *qstart, const char *qend)
{
	char	    *t = sin;
	unsigned int len;

	if (!sout || !sin)
		return NULL;

	while (*t && !strchr(delims, *t))
		t = SkipQuote(t, qlong, qstart, qend);
	len = t - sin;
	*sout = (char *)xmalloc(len + 1);
	memcpy(*sout, sin, len);
	(*sout)[len] = 0;
	if (*t)
		t++;

	return t;
}

/*
** PeekToken: returns next token from string, leaving string intact
**            (you should free returned string later)
**
** notes - quotes and brakets denote blocks, which are returned MINUS
**         the start & end delimiters.  Delims within block may be escaped
**         with a backslash \ (which is removed when returned).  Backslash
**         must be escaped too, if you want one... (\\).  Tokens may be up
**         to MAX_TOKEN_LENGTH in size.
*/
char *
PeekToken(const char *pstr)
{
	char	   *tok = NULL;
	const char *p;
	char	    bc = 0, be = 0, tmptok[MAX_TOKEN_LENGTH];
	int	    len = 0;

	if (!pstr)
		return NULL;

	p = pstr;
	EatWS(p); /* skip leading space */
	if (*p) {
		if (IsQuote(*p) ||
		    IsBlockStart(*p)) { /* quoted string or block start? */
			bc = *p;	/* save block start char */
			p++;
		}
		/* find end of token */
		while (*p && len < MAX_TOKEN_LENGTH) {
			/* first, check stop conditions based on block or normal
			 * token */
			if (bc) {
				if ((IsQuote(*p) && bc == *p) ||
				    IsBlockEnd(*p, bc)) {
					be = *p;
					break;
				}
			} else /* normal token */ {
				if (isspace((unsigned char)*p) || *p == ',')
					break;
			}

			if (*p == '\\' &&
			    *(p + 1)) /* if \, copy next char verbatim */
				p++;
			tmptok[len] = *p;
			len++;
			p++;
		}

		/* sanity checks: */
		if (bc && !be) { /* did we have block start, but not end? */
			/* should yell about this */
			return NULL;
		}

		if (len) {
			tok = (char *)malloc(len + 1);
			strncpy(tok, tmptok, len);
			tok[len] = '\0';
		}
	}

	return tok;
}

/*
** CmpToken: does case-insensitive compare on next token in string, leaving
**           string intact (return code like strcmp)
*/
int
CmpToken(const char *pstr, char *tok)
{
	int   rc = 0;
	char *ntok = PeekToken(pstr);
	if (ntok) {
		rc = strcasecmp(tok, ntok);
		free(ntok);
	}
	return rc;
}

/*
** MatchToken: does case-insensitive compare on next token in string, leaving
**             string intact (returns true if matches, false otherwise)
*/
int
MatchToken(const char *pstr, char *tok)
{
	int   rc = 0;
	char *ntok = PeekToken(pstr);
	if (ntok) {
		rc = (strcasecmp(tok, ntok) == 0);
		free(ntok);
	}
	return rc;
}

/*
** NukeToken: removes next token from string
*/
void
NukeToken(char **pstr)
{
	char *tok;
	char *next;
	char *temp = NULL;

	next = GetNextToken(*pstr, &tok);
	if (next != NULL)
		temp = strdup(next);
	if (pstr && *pstr)
		free(*pstr);
	*pstr = temp;
	if (tok)
		free(tok);
}

/****************************************************************************
 *
 * Gets the next "word" of input from string indata.
 * "word" is a string with no spaces, or a qouted string.
 * Return value is ptr to indata,updated to point to text after the word
 * which is extracted.
 * token is the extracted word, which is copied into a malloced
 * space, and must be freed after use. DoGetNextToken *never* returns an
 * empty string or token. If the token consists only of whitespace or
 * delimiters, the returned token is NULL instead. If out_delim is given,
 * the character ending the string is returned therein.
 *
 * spaces = string of characters to treat as spaces
 * delims = string of characters delimiting token
 *
 * Use "spaces" and "delims" to define additional space/delimiter
 * characters (spaces are skipped before a token, delimiters are not).
 *
 **************************************************************************/
char *
DoGetNextToken(
    char *indata, char **token, char *spaces, char *delims, char *out_delim)
{
	char *t, *start, *end, *text;
	int   snum;
	int   dnum;

	snum = (spaces) ? strlen(spaces) : 0;
	dnum = (delims) ? strlen(delims) : 0;
	if (indata == NULL) {
		if (out_delim)
			*out_delim = '\0';
		*token = NULL;
		return NULL;
	}
	t = indata;
	while ((*t != 0) &&
	    (isspace((unsigned char)*t) || (snum && strchr(spaces, *t))))
		t++;
	start = t;
	while ((*t != 0) &&
	    !(isspace((unsigned char)*t) || (snum && strchr(spaces, *t)) ||
		(dnum && strchr(delims, *t)))) {
		/* Check for qouted text */
		if (IsQuote(*t)) {
			char c = *t;

			t++;
			while ((*t != c) && (*t != 0)) {
				/* Skip over escaped text, ie \quote */
				if ((*t == '\\') && (*(t + 1) != 0))
					t++;
				t++;
			}
			if (*t == c)
				t++;
		} else {
			/* Skip over escaped text, ie \" or \space */
			if ((*t == '\\') && (*(t + 1) != 0))
				t++;
			t++;
		}
	}
	end = t;
	if (out_delim)
		*out_delim = *end;

	text = xmalloc(end - start + 1);
	*token = text;

	/* copy token */
	while (start < end) {
		/* Check for qouted text */
		if (IsQuote(*start)) {
			char c = *start;
			start++;
			while ((*start != c) && (*start != 0)) {
				/* Skip over escaped text, ie \" or \space */
				if ((*start == '\\') && (*(start + 1) != 0))
					start++;
				*text++ = *start++;
			}
			if (*start == c)
				start++;
		} else {
			/* Skip over escaped text, ie \" or \space */
			if ((*start == '\\') && (*(start + 1) != 0))
				start++;
			*text++ = *start++;
		}
	}
	*text = 0;
	if (*end != 0)
		end++;

	if (**token == 0) {
		free(*token);
		*token = NULL;
	}
	return end;
}

char *
GetNextToken(char *indata, char **token)
{
	return DoGetNextToken(indata, token, NULL, NULL, NULL);
}

char *
GetNextOption(char *indata, char **token)
{
	return DoGetNextToken(indata, token, ",", NULL, NULL);
}

char *
SkipNTokens(char *indata, unsigned int n)
{
	char *tmp;
	char *junk;

	tmp = indata;
	for (; n > 0; n--) {
		tmp = GetNextToken(tmp, &junk);
		if (junk)
			free(junk);
	}
	return tmp;
}

/****************************************************************************
 *
 * Works like GetNextToken, but with the following differences:
 *
 * If *indata begins with a "*" followed by the string module_name,
 * it returns the string following directly after module_name as the
 * new token. Otherwise NULL is returned.
 * e.g. GetModuleResource("*FvwmPagerGeometry", &token, "FvwmPager")
 * returns "Geometry" in token.
 *
 **************************************************************************/
char *
GetModuleResource(char *indata, char **resource, char *module_name)
{
	char *tmp;
	char *next;

	if (!module_name) {
		*resource = NULL;
		return indata;
	}
	next = GetNextToken(indata, &tmp);
	if (!tmp)
		return next;

	if (tmp[0] != '*' ||
	    strncasecmp(tmp + 1, module_name, strlen(module_name))) {
		*resource = NULL;
		return indata;
	}
	CopyString(resource, tmp + 1 + strlen(module_name));
	free(tmp);
	return next;
}

/****************************************************************************
 *
 * This function uses GetNextToken to parse action for up to num integer
 * arguments. The number of values actually found is returned.
 * If ret_action is non-NULL, a pointer to the next token is returned there.
 *
 **************************************************************************/
int
GetIntegerArguments(char *action, char **ret_action, int retvals[], int num)
{
	int   i;
	char *token;

	for (i = 0; i < num && action; i++) {
		action = GetNextToken(action, &token);
		if (token == NULL)
			break;
		errno = 0;
		retvals[i] = FvwmParseInteger(token);
		free(token);
		token = NULL;
		if (errno == EINVAL)
			break;
	}
	if (token)
		free(token);
	if (ret_action != NULL)
		*ret_action = action;

	return i;
}

/***************************************************************************
 *
 * This function tries to match a token with a list of strings and returns
 * the position of token in the array or -1 if no match is found. The last
 * entry in the list must be NULL.
 *
 * len = 0 : only exact matches
 * len < 0 : match, if token begins with one of the strings in list
 * len > 0 : match, if the first len characters do match
 *
 * if next is non-NULL, *next will be set to point to the first character
 * in token after the match.
 *
 **************************************************************************/
int
GetTokenIndex(char *token, char *list[], int len, char **next)
{
	int i;
	int l;
	int k;

	if (!token || !list) {
		if (next)
			*next = NULL;
		return -1;
	}
	l = (len) ? len : strlen(token);
	for (i = 0; list[i] != NULL; i++) {
		k = strlen(list[i]);
		if (len < 0)
			l = k;
		if (len == 0 && k != l)
			continue;
		if (!strncasecmp(token, list[i], l))
			break;
	}
	if (next) {
		*next = (list[i]) ? token + l : token;
	}

	return (list[i]) ? i : -1;
}

/***************************************************************************
 *
 * This function does roughly the same as GetTokenIndex but reads the
 * token from string action with GetNextToken. The index is returned
 * in *index. The return value is a pointer to the character after the
 * token (just like the return value of GetNextToken).
 *
 **************************************************************************/
char *
GetNextTokenIndex(char *action, char *list[], int len, int *index)
{
	char *token;
	char *next;

	if (!index)
		return action;

	next = GetNextToken(action, &token);
	if (!token) {
		*index = -1;
		return action;
	}
	*index = GetTokenIndex(token, list, len, NULL);
	free(token);

	return (*index == -1) ? action : next;
}

int
GetRectangleArguments(char *action, int *width, int *height)
{
	char	   *token;
	const char *s;
	int	    w, h;

	GetNextToken(action, &token);
	if (!token)
		return 0;
	/* MxN style number, specifically for DeskTopSize: a number,
	 * one separator character, another number.  The historical
	 * sscanf("%d%*c%d") accepted any separator. */
	errno = 0;
	w = FvwmParseInteger(token);
	if (errno != 0)
		goto bad;
	s = token;
	while (*s != '\0' && !isdigit((unsigned char)*s))
		s++;
	while (isdigit((unsigned char)*s))
		s++;
	if (*s == '\0')
		goto bad;
	s++;
	errno = 0;
	h = FvwmParseInteger(s);
	if (errno != 0)
		goto bad;

	free(token);
	*width = w;
	*height = h;
	return 2;

bad:
	free(token);
	return 0;
}

/* unit_io is input as well as output. If action has a postfix 'p' or 'P',
 * *unit_io is set to 100, otherwise it is left untouched. */
int
GetOnePercentArgument(char *action, int *value, int *unit_io)
{
	unsigned int len;
	char	    *token;
	int	     n;

	*value = 0;
	if (!action)
		return 0;
	GetNextToken(action, &token);
	if (!token)
		return 0;

	len = strlen(token);
	/* token never contains an empty string, so this is ok */
	if (token[len - 1] == 'p' || token[len - 1] == 'P') {
		*unit_io = 100;
		token[len - 1] = '\0';
	}
	errno = 0;
	*value = FvwmParseInteger(token);
	n = (errno == EINVAL) ? 0 : 1;

	free(token);
	return n;
}

int
GetTwoPercentArguments(
    char *action, int *val1, int *val2, int *val1_unit, int *val2_unit)
{
	char *tok1, *tok2;
	int   n = 0;

	*val1 = 0;
	*val2 = 0;

	action = GetNextToken(action, &tok1);
	if (!tok1)
		return 0;
	GetNextToken(action, &tok2);
	if (GetOnePercentArgument(tok2, val2, val2_unit) == 1 &&
	    GetOnePercentArgument(tok1, val1, val1_unit) == 1) {
		free(tok1);
		free(tok2);
		return 2;
	}

	/* now try MxN style number, specifically for DeskTopSize: */
	n = GetRectangleArguments(tok1, val1, val2);
	free(tok1);
	if (tok2)
		free(tok2);
	return n;
}
