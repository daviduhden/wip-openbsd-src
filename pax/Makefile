#	$OpenBSD: $
#	Standalone fork build for OpenBSD 7.9 / 8.0-beta

BINDIR ?=	/bin
MANDIR ?=	/usr/share/man/man
CC =		clang
DEBUGGER =	lldb
CFLAGS_COMMON =	-std=c17 -Wall -Wextra -Wpedantic
CFLAGS =	-O2 -pipe ${CFLAGS_COMMON}
CFLAGS_DEBUG =	-O0 -g ${CFLAGS_COMMON}
PROG =		pax
SRCS =		ar_io.c ar_subs.c buf_subs.c cpio.c file_subs.c ftree.c \
		gen_subs.c getoldopt.c options.c pat_rep.c pax.c sel_subs.c \
		tables.c tar.c tty_subs.c
MAN =		pax.1 tar.1 cpio.1
LINKS =		${BINDIR}/pax ${BINDIR}/tar ${BINDIR}/pax ${BINDIR}/cpio

.include <bsd.prog.mk>

# Build pax_debug with -O0 -g, then start it under the debugger.
debug: clean
	${MAKE} CFLAGS="${CFLAGS_DEBUG}" PROG="${PROG}_debug" all
	${DEBUGGER} ./${PROG}_debug

# Clean and rebuild the program from scratch.
rebuild: clean .WAIT all

.PHONY: debug rebuild
