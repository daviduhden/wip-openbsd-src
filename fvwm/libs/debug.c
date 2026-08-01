/* File:    debug.c
 *
 * Description:
 *      Implement some debugging/log functions that can be used generically
 *      by all of fvwm + modules.
 *
 * Created:
 *       6 Nov 1998 - Paul D. Smith <psmith@BayNetworks.com>
 */

#include <stdarg.h>
#include <stdio.h>

#include "config.h"
#include "fvwmlib.h"

int f_db_level = 0;

#ifdef DEBUG

struct f_db_info f_db_info;

void
f_db_print(const char *fmt, ...)
{
	va_list ap;

	fprintf(stderr, "%s:%ld: ", f_db_info.filenm, f_db_info.lineno);

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);

	fputc('\n', stderr);
}

#endif
