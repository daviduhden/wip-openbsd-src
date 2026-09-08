# $OpenBSD: Makefile,v 1.3 2007/04/09 18:59:57 matthieu Exp $

.include "../Makefile.inc"

PROG = FvwmSaveDesk
SRCS = FvwmSaveDesk.c

LDADD += -lXpm ${XLIB}
BINDIR = ${FVWMLIBDIR}

.include <bsd.prog.mk>
.include <bsd.xorg.mk>
