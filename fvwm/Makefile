# $OpenBSD: Makefile,v 1.4 2016/10/10 13:34:43 matthieu Exp $
# Standalone fork build for OpenBSD 7.9 / 8.0-beta

X11BASE ?= /usr/X11R6
BINDIR ?= /usr/local/bin
MANDIR ?= /usr/local/man

SUBDIR = libs fvwm sample.fvwmrc utils icons modules

update:

	.include <bsd.subdir.mk>
.include <bsd.xorg.mk>