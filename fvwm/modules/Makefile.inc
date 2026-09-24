# -*- makefile -*-
# $OpenBSD: Makefile.inc,v 1.9 2012/09/30 10:12:51 espie Exp $
# Standalone fork build for OpenBSD 7.9 / 8.0-beta

.ifndef FVWM_MAKEFILE_INC
FVWM_MAKEFILE_INC = done

X11BASE ?=	/usr/X11R6

CC =		clang
DEBUGGER =	lldb

CFLAGS_COMMON =	-std=c23 -Wall -Wextra -Wpedantic \
		-isystem ${.CURDIR}/../../../compat \
		-I${X11BASE}/include \
		-I${.CURDIR}/../.. -I${.CURDIR}/../../libs
CFLAGS =	-O2 -pipe ${CFLAGS_COMMON}
CFLAGS_DEBUG =	-O0 -g ${CFLAGS_COMMON}
LDADD +=	-L${X11BASE}/lib

XLIB =		-lX11 -lxcb -lXau -lXdmcp

FVWMLIBDIR =	${X11BASE}/lib/X11/fvwm
MANDIR =	${X11BASE}/man/man

# Standalone builds are in-place; never create objdirs.
NOOBJ =		Yes

LDADD +=	-L${.CURDIR}/../../libs -lfvwm

# Reject directories without a program before cleaning anything.
debug-check:
	@if [ -z "${PROG}" ]; then \
		echo "make debug: no PROG in ${.CURDIR}" >&2; \
		exit 1; \
	fi

# Build <module>_debug with -O0 -g, then start it under the debugger.
debug: debug-check .WAIT clean
	${MAKE} CFLAGS="${CFLAGS_DEBUG}" PROG="${PROG}_debug" all
	${DEBUGGER} ./${PROG}_debug

# Clean and rebuild the module from scratch.
rebuild: clean .WAIT all

.PHONY: debug debug-check rebuild
.endif
