# libc/tests/Makefile -- host-side developer tests for the imported libc.
#
# These tests are NOT part of the OpenBSD build graph.  They exist so that
# the six new headers and the threads wrappers can be compiled and executed
# on a development machine (Linux/macOS), with the host C library standing
# in for the parts that only OpenBSD provides.
#
# The headers under test are exposed through a shim directory that contains
# nothing but the header being tested, so that <limits.h>, <stdbool.h>,
# <pthread.h> and friends come from the host C library while <stdbit.h>,
# <stdatomic.h>, <threads.h>, ... come from the OpenBSD tree.  Each test
# also asserts that the tree's include guard is in effect, proving that the
# header selected is the one under libc/include and not a compiler or host
# copy.
#
# This Makefile uses BSD make syntax.  Run it with OpenBSD make(1) or,
# on other hosts, bmake; GNU make is not supported.
#
# Run with:
#   bmake           # build the tests
#   bmake run       # build, run and run the compile matrix
#   bmake test      # alias for run
#   bmake clean
#
# Overrides:
#   CC=...          # C compiler (default clang)

CC =		clang
CXX =		clang++
CFLAGS =	-O2 -std=c99 -Wall -Wextra -Werror -pedantic-errors
# Tests that declare arbitrary atomic types exercise a C11 language API.
# Keep atomics and C23 BitInt test language requirements explicit.
# The remaining host runtime tests use C99.
ATOMIC_STD ?= c11
ATOMIC_CFLAGS = ${CFLAGS:N-std=*} -std=${ATOMIC_STD}
STDBIT_CFLAGS = ${CFLAGS:N-std=*} -std=c23
SAN ?=

LIBCDIR =	${.CURDIR}/..
SHIM =		${.CURDIR}/.shim
HDRS =		stdalign.h stdnoreturn.h stdatomic.h stdbit.h \
		stdckdint.h threads.h
THRD_C =	${LIBCDIR}/lib/librthread/thrd.c
ATOMICFB_C =	${LIBCDIR}/lib/libc/thread/atomic_fallback.c
STDBIT_C =	${LIBCDIR}/lib/libc/gen/stdbit.c
ATOMIC_C =	${LIBCDIR}/lib/libc/thread/atomic.c

# The host C library is glibc: _POSIX_C_SOURCE exposes PTHREAD_MUTEX_NORMAL
# and the timedlock/settype entry points without enabling glibc's C23
# <stdlib.h> additions (once_flag/call_once), which would collide with the
# tree's <threads.h>.  DEF_WEAK and __dead are libc-internal/OpenBSD macros
# that have no host counterpart.
HOSTDEFS =	-D_POSIX_C_SOURCE=200809L -D__dead= \
		-D'DEF_WEAK(x)=typedef int __stdc_host_dummy_\#\#x' \
		-D'DEF_STRONG(x)=typedef int __stdc_host_strong_\#\#x'

TESTS =		stdbit-tests stdatomic-tests stdatomic-fallback-tests \
		threads-tests stdckdint-tests stdckdint-oracle-tests

all: ${TESTS}

shim:
	@mkdir -p ${SHIM}
.for h in ${HDRS}
	@ln -sf ${LIBCDIR}/include/${h} ${SHIM}/${h}
.endfor
	@ln -sf ${.CURDIR}/host-stdlib.h ${SHIM}/stdlib.h

stdbit-tests: ${STDBIT_C} shim
	${CC} ${STDBIT_CFLAGS} ${SAN} ${HOSTDEFS} -I${SHIM} -I${.CURDIR} \
	    ${STDBIT_C} stdbit-link-helper.c stdbit-tests.c -o $@

stdatomic-tests: ${ATOMIC_C} shim
	${CC} ${ATOMIC_CFLAGS} ${SAN} ${HOSTDEFS} -I${SHIM} -I${.CURDIR} \
	    ${ATOMIC_C} stdatomic-tests.c -pthread -o $@

stdatomic-fallback-tests: ${ATOMICFB_C} shim
	${CC} ${ATOMIC_CFLAGS} ${SAN} ${HOSTDEFS} -I${SHIM} -I${.CURDIR} \
	    ${ATOMICFB_C} stdatomic-fallback-tests.c -pthread -o $@

threads-tests: ${THRD_C} shim
	${CC} ${CFLAGS} ${SAN} ${HOSTDEFS} -I${SHIM} -I${.CURDIR} \
	    ${THRD_C} threads-tests.c -pthread -o $@

stdckdint-tests: shim
	${CC} ${CFLAGS} ${SAN} -I${SHIM} -I${.CURDIR} stdckdint-tests.c -o $@

stdckdint-oracle-tests: shim
	${CC} ${CFLAGS} ${SAN} -I${SHIM} -I${.CURDIR} \
	    stdckdint-oracle-tests.c -o $@

run: ${TESTS}
	./stdbit-tests
	./stdatomic-tests
	./stdatomic-fallback-tests
	./threads-tests
	./stdckdint-tests
	./stdckdint-oracle-tests
	CC=${CC} sh ${.CURDIR}/stdbit-fallback.sh
	CC="${CC}" CXX="${CXX}" sh ${.CURDIR}/headers-compile.sh

test: run

clean:
	rm -f ${TESTS}
	rm -rf ${SHIM}

.PHONY: all shim run test clean
