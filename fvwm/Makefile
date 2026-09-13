# $OpenBSD: Makefile,v 1.4 2016/10/10 13:34:43 matthieu Exp $
# Standalone fork build for OpenBSD 7.9 / 8.0-beta

X11BASE ?= /usr/X11R6

SUBDIR = libs fvwm sample.fvwmrc utils icons modules

.include <bsd.subdir.mk>
.include <bsd.xorg.mk>

# Debug the main window manager; modules have their own debug target.
debug:
	@echo "===> fvwm"
	@${MAKE} -C ${.CURDIR}/fvwm debug

.PHONY: debug
