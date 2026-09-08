# $OpenBSD: Makefile,v 1.2 2006/11/27 19:56:35 matthieu Exp $
# Standalone fork build for OpenBSD 7.9 / 8.0-beta

.include "../Makefile.inc"

LIB = fvwm
SRCS = ClientMsg.c GetFont.c Grab.c ModParse.c ModParse.h \
	Module.c Parse.c Picture.c Strings.c System.c \
	envvar.c fvwmlib.h wild.c \
	lang-strings.h XResource.c debug.c ColorUtils.c

NOPROFILE = yes
NOPIC = yes

# libfvwm.a is internal to the window manager; never installed.
install:

.include <bsd.lib.mk>
.include <bsd.xorg.mk>
