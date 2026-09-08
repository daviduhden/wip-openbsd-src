# -*- makefile -*-
# $OpenBSD: Makefile.inc,v 1.9 2012/09/30 10:12:51 espie Exp $
# Standalone fork build for OpenBSD 7.9 / 8.0-beta

.ifndef FVWM_MAKEFILE_INC
FVWM_MAKEFILE_INC = done

X11BASE ?=	/usr/X11R6

CFLAGS +=	-std=c17 -I${X11BASE}/include -I${.CURDIR}/../.. \
		-I${.CURDIR}/../../libs
LDADD +=	-L${X11BASE}/lib

XLIB =		-lX11 -lxcb -lXau -lXdmcp

FVWMLIBDIR =	${X11BASE}/lib/X11/fvwm
MANDIR =	${X11BASE}/man/man

# Standalone builds are in-place; never create objdirs.
NOOBJ =		Yes

LDADD +=	-L${.CURDIR}/../../libs -lfvwm
.endif
