# $OpenBSD: Makefile,v 1.4 2011/03/08 20:48:59 matthieu Exp $
# Standalone fork build for OpenBSD 7.9 / 8.0-beta

.include "../Makefile.inc"

PROG = xpmroot
BINDIR = ${X11BASE}/bin
SRCS = xpmroot.c

LDADD += -lXpm -lXt -lICE -lSM -lXext -lX11 -lxcb -lXdmcp -lXau

.include <bsd.prog.mk>
.include <bsd.xorg.mk>
