#include <sys/utsname.h>
#include <unistd.h>

#include "config.h"

#ifndef FD_SETSIZE
#define FD_SETSIZE 2048
#endif

int
GetFdWidth(void)
{
	return min(sysconf(_SC_OPEN_MAX), FD_SETSIZE);
}

int
getostype(char *buf, int max)
{
	struct utsname sysname;

	if (uname(&sysname) >= 0) {
		strlcpy(buf, sysname.sysname, max);
		return 0;
	}
	strlcpy(buf, "", max);
	return -1;
}
