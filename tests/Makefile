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
#   make run        # build and run them (ASan+UBSan, or ubsan_minimal on OpenBSD)
#   make debug      # clean, rebuild one test as <test>_debug with
#                   # -O0 -g and debug it (TEST=..., default
#                   # fvwm-parse-tests)
#   make test       # alias for run
#   make clean
#
# Overrides:
#   CC=...          # C compiler (default clang)
#   DEBUGGER=...    # debugger (default lldb)
#   TEST=...        # test to debug
#   X11INCS=...     # location of the X11 headers
#   SANITIZE=       # disable sanitizers

CC =		clang
DEBUGGER =	lldb
CFLAGS =	-O2 -pipe -std=c17 -D_GNU_SOURCE -Wall -Wextra -Wpedantic
DEBUG_CFLAGS =	-O0 -g -std=c17 -D_GNU_SOURCE -Wall -Wextra -Wpedantic
SUFFIX =

# Nonexistent include directories are harmless; point X11INCS at
# the real one when it is not in the default list.
X11INCS ?=	-I/usr/X11R6/include -I/usr/local/include

# On OpenBSD ASan is unsupported and only the ubsan_minimal runtime is
# shipped (clang-local(1)); elsewhere use the ASan+UBSan runtimes.
SANITIZE != if [ "`uname -s`" = OpenBSD ]; then echo "-fsanitize=undefined -fsanitize-minimal-runtime -Wl,--no-execute-only"; else echo "-fsanitize=address,undefined"; fi
SANITIZE ?=	-fsanitize=address,undefined

TESTS =		fvwm-parse-tests fvwm-exec-tests fvwm-pipebuf-tests \
		ext4fs-jbd2-tests ext4fs-inode-tests

all: $(TESTS)

fvwm-parse-tests: fvwm-parse-tests.c ../fvwm/libs/Parse.c \
    ../fvwm/libs/Strings.c ../fvwm/libs/System.c openbsd-shims.c
	$(CC) $(CFLAGS) $(SANITIZE) $(X11INCS) -I../fvwm -I../fvwm/libs \
	    -I../fvwm/fvwm -I. fvwm-parse-tests.c ../fvwm/libs/Strings.c \
	    ../fvwm/libs/System.c openbsd-shims.c -o $@$(SUFFIX)

fvwm-exec-tests: fvwm-exec-tests.c ../fvwm/fvwm/fvwm_exec.c
	$(CC) $(CFLAGS) $(SANITIZE) -I. -I../fvwm -I../fvwm/fvwm \
	    fvwm-exec-tests.c -o $@$(SUFFIX)

fvwm-pipebuf-tests: fvwm-pipebuf-tests.c ../fvwm/fvwm/pipebuf.c \
    ../fvwm/fvwm/pipebuf.h
	$(CC) $(CFLAGS) $(SANITIZE) -I. fvwm-pipebuf-tests.c -o $@$(SUFFIX)

# The ext4fs kernel sources are included directly by the test
# translation units under the tests/openbsd stub include tree, so
# the exact production parsing/checksum code is exercised.
EXT4SRC =	../ext4fs/sys/ufs/ext4fs

ext4fs-jbd2-tests: ext4fs-jbd2-tests.c $(EXT4SRC)/ext4fs_journal.c \
    $(EXT4SRC)/ext4fs_crc32c.c $(EXT4SRC)/ext4fs_journal.h \
    $(EXT4SRC)/ext4fs.h $(EXT4SRC)/ext4fs_dinode.h
	$(CC) $(CFLAGS) $(SANITIZE) -I. -Iopenbsd -I../ext4fs/sys \
	    ext4fs-jbd2-tests.c -o $@$(SUFFIX)

ext4fs-inode-tests: ext4fs-inode-tests.c $(EXT4SRC)/ext4fs_crc32c.c \
    $(EXT4SRC)/ext4fs.h $(EXT4SRC)/ext4fs_dinode.h
	$(CC) $(CFLAGS) $(SANITIZE) -I. -Iopenbsd -I../ext4fs/sys \
	    ext4fs-inode-tests.c -o $@$(SUFFIX)

run: $(TESTS)
	ASAN_OPTIONS=detect_leaks=0 ./fvwm-parse-tests
	ASAN_OPTIONS=detect_leaks=0 ./fvwm-exec-tests
	ASAN_OPTIONS=detect_leaks=0 ./fvwm-pipebuf-tests
	ASAN_OPTIONS=detect_leaks=0 ./ext4fs-jbd2-tests
	ASAN_OPTIONS=detect_leaks=0 ./ext4fs-inode-tests

test: run

TEST ?=		fvwm-parse-tests

debug:
	$(MAKE) clean
	$(MAKE) CFLAGS="$(DEBUG_CFLAGS)" SUFFIX=_debug $(TEST)
	$(DEBUGGER) ./$(TEST)_debug

rebuild:
	$(MAKE) clean
	$(MAKE) all

clean:
	rm -f $(TESTS) *_debug *.o

.PHONY: all run test debug rebuild clean
