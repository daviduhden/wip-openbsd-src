# -*- makefile -*-
# $OpenBSD: Makefile.inc,v 1.9 2012/09/30 10:12:51 espie Exp $
# Standalone fork build for OpenBSD 7.9 / 8.0-beta

.ifndef FVWM_MAKEFILE_INC
FVWM_MAKEFILE_INC = done

X11BASE ?=	/usr/X11R6
MANDIR ?=	${X11BASE}/man/man

CC =		clang
DEBUGGER =	lldb

CFLAGS +=	-std=c17 -I${X11BASE}/include -I${.CURDIR} -I${.CURDIR}/.. \
		-I${.CURDIR}/../libs
LDADD +=	-L${X11BASE}/lib

FVWMLIBDIR =	${X11BASE}/lib/X11/fvwm

# Standalone builds are in-place; never create objdirs.
NOOBJ =		Yes

# Fallbacks used by the data-only subdirectories when the Xenocara
# bsd.xorg.mk is unavailable.  The Xenocara definitions win.
INSTALL_DATA ?=	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} \
		-m ${SHAREMODE}
INCSDIR ?=	${X11BASE}/include

LDADD +=	-L${.CURDIR}/../libs -lfvwm

# Build the directory's program, then start it under the debugger.
debug: all
	@if [ -z "${PROG}" ]; then \
		echo "make debug: no PROG in ${.CURDIR}" >&2; \
		exit 1; \
	fi; \
	exec ${DEBUGGER} ${PROG}

.PHONY: debug
.endif
