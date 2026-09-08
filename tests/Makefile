# tests/Makefile -- host-side developer tests for wip-openbsd-src.
#
# These tests exercise the pure parsing/decoding logic that has no
# runtime X11 or kernel dependency.  They are NOT part of the
# OpenBSD build graph and are not covered by the production
# Makefile policies; they exist so the code can be exercised from
# Linux/macOS development machines with a C compiler and the X11
# headers installed (X11 headers are needed for type definitions
# only).
#
# Run with:
#   make            # build the test binaries
#   make run        # build and run them (ASan/UBSan enabled)
#   make clean
#
# Overrides:
#   CC=clang        # prefer clang for the sanitizer runtimes
#   X11INCS=...     # location of the X11 headers
#   SANITIZE=       # disable sanitizers

CC ?=		cc
CFLAGS ?=	-O2 -g
CFLAGS +=	-std=c17 -D_GNU_SOURCE -Wall -Wextra
CFLAGS +=	-Wno-unused-function -Wno-deprecated-declarations

# Nonexistent include directories are harmless; point X11INCS at
# the real one when it is not in the default list.
X11INCS ?=	-I/usr/X11R6/include -I/usr/local/include

SANITIZE ?=	-fsanitize=address,undefined

TESTS =		fvwm-parse-tests fvwm-exec-tests fvwm-pipebuf-tests \
		ext4fs-jbd2-tests ext4fs-inode-tests

all: $(TESTS)

fvwm-parse-tests: fvwm-parse-tests.c ../fvwm/libs/Parse.c \
    ../fvwm/libs/Strings.c ../fvwm/libs/System.c openbsd-shims.c
	$(CC) $(CFLAGS) $(SANITIZE) $(X11INCS) -I../fvwm -I../fvwm/libs \
	    -I../fvwm/fvwm -I. fvwm-parse-tests.c ../fvwm/libs/Strings.c \
	    ../fvwm/libs/System.c openbsd-shims.c -o $@

fvwm-exec-tests: fvwm-exec-tests.c ../fvwm/fvwm/fvwm_exec.c
	$(CC) $(CFLAGS) $(SANITIZE) -I. -I../fvwm -I../fvwm/fvwm \
	    fvwm-exec-tests.c -o $@

fvwm-pipebuf-tests: fvwm-pipebuf-tests.c ../fvwm/fvwm/pipebuf.c \
    ../fvwm/fvwm/pipebuf.h
	$(CC) $(CFLAGS) $(SANITIZE) -I. fvwm-pipebuf-tests.c -o $@

# The ext4fs kernel sources are included directly by the test
# translation units under the tests/openbsd stub include tree, so
# the exact production parsing/checksum code is exercised.
EXT4SRC =	../ext4fs/sys/ufs/ext4fs

ext4fs-jbd2-tests: ext4fs-jbd2-tests.c $(EXT4SRC)/ext4fs_journal.c \
    $(EXT4SRC)/ext4fs_crc32c.c $(EXT4SRC)/ext4fs_journal.h \
    $(EXT4SRC)/ext4fs.h $(EXT4SRC)/ext4fs_dinode.h
	$(CC) $(CFLAGS) $(SANITIZE) -I. -Iopenbsd -I../ext4fs/sys \
	    ext4fs-jbd2-tests.c -o $@

ext4fs-inode-tests: ext4fs-inode-tests.c $(EXT4SRC)/ext4fs_crc32c.c \
    $(EXT4SRC)/ext4fs.h $(EXT4SRC)/ext4fs_dinode.h
	$(CC) $(CFLAGS) $(SANITIZE) -I. -Iopenbsd -I../ext4fs/sys \
	    ext4fs-inode-tests.c -o $@

run: $(TESTS)
	ASAN_OPTIONS=detect_leaks=0 ./fvwm-parse-tests
	ASAN_OPTIONS=detect_leaks=0 ./fvwm-exec-tests
	ASAN_OPTIONS=detect_leaks=0 ./fvwm-pipebuf-tests
	ASAN_OPTIONS=detect_leaks=0 ./ext4fs-jbd2-tests
	ASAN_OPTIONS=detect_leaks=0 ./ext4fs-inode-tests

clean:
	rm -f $(TESTS) *.o

.PHONY: all run clean
