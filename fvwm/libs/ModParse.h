#ifndef MODPARSE_H
#define MODPARSE_H
/*
   File:		ModParse.h
   Date:		Wed Jan 29 21:13:36 1997
   Author:		Albrecht Kadlec (albrecht)

   Changed 08/30/98 by Dan Espen:
   - Copied from FvwmEvent/Parse.h.  Set up for general use by modules.
   Still to be done, try to integrate with Parse.c which is used by fvwm
   itself for parsing.
*/

#include <assert.h>
#include <stdio.h>
#include <stdlib.h> /* for free() */
#include <string.h>

char *PeekArgument(const char *pstr);
char *GetArgument(char **pstr);
int CmpArgument(const char *pstr, char *tok);
int MatchArgument(const char *pstr, char *tok);
#define NukeArgument(pstr) free(GetArgument(pstr))

/*
   function:		FindToken
   description:		find the entry of type 'struct_entry'
                        holding 'key' in 'table'
   returns:		pointer to the matching entry
                        NULL if not found

   table must be sorted in ascending order for FindToken.
*/

#define FindToken(key, table, struct_entry)                                    \
	(struct_entry *)bsearch(key, (char *)(table),                          \
	    sizeof(table) / sizeof(struct_entry), sizeof(struct_entry),        \
	    XCmpToken)

int XCmpToken(const void *s, const void *t);
/* needed by (L)FindToken */

#endif /* MODPARSE_H */
