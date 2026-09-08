/*
 * cdefs-shim.h -- test-support definition of OpenBSD cdefs macros.
 * NOT part of the production build.
 */
#ifndef CDEFS_SHIM_H
#define CDEFS_SHIM_H
#ifndef __dead
#define __dead __attribute__((__noreturn__))
#endif
#endif
