# $OpenBSD: Makefile,v 1.9 2011/06/23 22:48:59 naddy Exp $

.include "../Makefile.inc"

PROG= fvwm
SRCS= add_window.c bindings.c borders.c \
	builtins.c colormaps.c colors.c complex.c decorations.c \
	events.c exec.c focus.c functions.c fvwm.c fvwmdebug.c icons.c \
	menus.c misc.c modconf.c module.c move.c \
	placement.c read.c resize.c style.c \
	virtual.c windows.c

CPPFLAGS+= -DFVWM_MODULEDIR=\"$(FVWMLIBDIR)\" \
	-DFVWMLIBDIR=\"$(FVWMLIBDIR)\" \
	-DFVWMRC=\".fvwmrc\" \
	-DFVWM_CONFIGDIR=\"$(FVWMLIBDIR)\"

LDADD+= -lXpm -lXt -lICE -lSM -lXext -lX11 -lxcb -lXdmcp -lXau -lutil

fvwm.1: fvwm2.1
	sed -e "s,__projectroot__,${X11BASE}," ${.CURDIR}/fvwm2.1 > fvwm.1

CLEANFILES= fvwm.1

SCRIPTS= fvwm_exec
SCRIPTSDIR= $(FVWMLIBDIR)

.all: fvwm_exec

fvwm_exec: fvwm_exec.c ${.CURDIR}/../config.h
	${CC} ${CFLAGS} ${CPPFLAGS} -o fvwm_exec fvwm_exec.c -lutil

.include <bsd.prog.mk>
.include <bsd.xorg.mk>
