/*
** XResource.c:
** These routines provide modules with an interface to parse all kinds of
** configuration options (X resources, command line options and configuration
** file lines) in the same way (Xrm database).
*/

#include <X11/Xlib.h>
#include <X11/Xresource.h>
#include <string.h>

#include "config.h"
#include "fvwmlib.h"

/* Default option table */
static XrmOptionDescRec default_opts[] = {
    {"-fg", "*Foreground", XrmoptionSepArg, NULL},
    {"-bg", "*Background", XrmoptionSepArg, NULL},
    {"-fn", "*Font", XrmoptionSepArg, NULL},
    {"-geometry", "*Geometry", XrmoptionSepArg, NULL},
    {"-title", "*Title", XrmoptionSepArg, NULL}
    /* Remember to update NUM_DEFAULT_OPTIONS if you change this list! */
};
#define NUM_DEFAULT_OPTS 5

/* internal function */
static void
DoMergeString(char *resource, XrmDatabase *ptarget, Bool override)
{
	XrmDatabase db;

	if (!resource)
		return;
	db = XrmGetStringDatabase(resource);
	XrmCombineDatabase(db, ptarget, override);
}

/***************************************************************************
 *
 * Merges all X resources for the display/screen into a Xrm database.
 * If the database does not exist (*pdb == NULL), a new database is created.
 * If override is True, existing entries of the same name are overwritten.
 *
 * Please remember to destroy the database with XrmDestroyDatabase(*pdb)
 * if you do not need it amymore.
 *
 ***************************************************************************/
void
MergeXResources(Display *dpy, XrmDatabase *pdb, Bool override)
{
	if (!*pdb)
		/* create new database */
		XrmPutStringResource(pdb, "", "");
	DoMergeString(XResourceManagerString(dpy), pdb, override);
	DoMergeString(
	    XScreenResourceString(DefaultScreenOfDisplay(dpy)), pdb, override);
}

/***************************************************************************
 *
 * Parses the command line given through pargc/argv and puts recognized
 * entries into the Xrm database *pdb (if *pdb is NULL a new database is
 * created). The caller may provide an option list in XrmOptionDescList
 * format (see XrmParseCommand manpage) and/or parse only standard options
 * (fg, bg, geometry, fn, title). User given options have precedence over
 * standard options which are disabled if fNoDefaults is True. Existing
 * values are overwritten.
 *
 * All recognised options are removed from the command line (*pargc and
 * argv are updated accordingly).
 *
 * Please remember to destroy the database with XrmDestroyDatabase(*pdb)
 * if you do not need it amymore.
 *
 ***************************************************************************/
void
MergeCmdLineResources(XrmDatabase *pdb, XrmOptionDescList opts, int num_opts,
    char *name, int *pargc, char **argv, Bool fNoDefaults)
{
	if (opts && num_opts > 0)
		XrmParseCommand(pdb, opts, num_opts, name, pargc, argv);
	if (!fNoDefaults)
		XrmParseCommand(
		    pdb, default_opts, NUM_DEFAULT_OPTS, name, pargc, argv);
}

/***************************************************************************
 *
 * Takes a line from a config file and puts a corresponding value into the
 * Xrm database *pdb (will be created if *pdb is NULL). 'prefix' is the
 * name of the module. A specific type of binding in the database must be
 * provided in bindstr (either "*" or "."). Leading unquoted whitespace are
 * stripped from value. Existing values in the database are overwritten.
 * True is returned if the line was indeed merged into the database (i.e. it
 * had the correct format) or False if not.
 *
 * Example: If prefix = "MyModule" and bindstr = "*", the line
 *
 *   *MyModuleGeometry   80x25+0+0
 *
 * will be put into the database as if you had this line in your .Xdefaults:
 *
 *   MyModule*Geometry:  80x25+0+0
 *
 * Please remember to destroy the database with XrmDestroyDatabase(*pdb)
 * if you do not need it amymore.
 *
 ***************************************************************************/
Bool
MergeConfigLineResource(
    XrmDatabase *pdb, char *line, char *prefix, char *bindstr)
{
	int len;
	char *end;
	char *value;
	char *myvalue;
	char *resource;
	size_t reslen;

	/* translate "*(prefix)(suffix)" to "(prefix)(binding)(suffix)",
	 * e.g. "*FvwmPagerGeometry" to "FvwmPager.Geometry" */
	if (!line || *line != '*')
		return False;

	line++;
	len = (prefix) ? strlen(prefix) : 0;
	if (!prefix || strncasecmp(line, prefix, len))
		return False;

	line += len;
	end = line;
	while (*end && !isspace(*end))
		end++;
	if (line == end)
		return False;
	value = end;
	while (*value && isspace(*value))
		value++;

	/* prefix*suffix: value */
	reslen = len + (end - line) + 2;
	resource = (char *)safemalloc(reslen);
	strlcpy(resource, prefix, reslen);
	strlcat(resource, bindstr, reslen);
	strncat(resource, line, end - line);

	len = strlen(value);
	myvalue = (char *)safemalloc(len + 1);
	strlcpy(myvalue, value, len + 1);
	for (len--; len >= 0 && isspace(myvalue[len]); len--)
		myvalue[len] = 0;

	/* merge string into database */
	XrmPutStringResource(pdb, resource, myvalue);

	free(resource);
	free(myvalue);
	return True;
}

/***************************************************************************
 *
 * Reads the string-value for the pair prefix/resource from the Xrm database
 * db and returns a pointer to it. The string may only be read and must not
 * be freed by the caller. 'prefix' is the class name (usually the name of
 * the module). If no value is found in the database, *val will be NULL.
 * True is returned if a value was found, False if not. If you are only
 * interested if there is a string, but not it's value, you can set val to
 * NULL.
 *
 * Example:
 *
 *   GetResourceString(db, "Geometry", "MyModule", &s)
 *
 * returns the string value of the "Geometry" resource for MyModule in s.
 *
 ***************************************************************************/
Bool
GetResourceString(
    XrmDatabase db, const char *resource, const char *prefix, char **val)
{
	XrmValue xval = {0, NULL};
	char *str_type;
	char *name;
	size_t len;

	len = strlen(resource) + strlen(prefix) + 2;
	name = (char *)safemalloc(len);
	strlcpy(name, prefix, len);
	strlcat(name, ".", len);
	strlcat(name, resource, len);

	if (!XrmGetResource(db, name, name, &str_type, &xval) ||
	    xval.addr == NULL) {
		free(name);
		if (val)
			*val = NULL;
		return False;
	}
	free(name);
	if (val)
		*val = xval.addr;

	return True;
}
