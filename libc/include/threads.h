/*	$OpenBSD$	*/

/*-
 * Copyright (c) 2026 David Uhden Collado <david@uhden.dev>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * ISO/IEC 9899:2011 (C11) 7.26: Threads <threads.h>,
 * carried forward through C17 and C23.
 *
 * The C11 threads interface is a thin, standardised skin over POSIX
 * threads.  OpenBSD keeps its thread implementation inside libc (the
 * historical libpthread is only a set of manual pages), so the functions
 * are provided by libc and use the pthread_* entry points directly; no new
 * synchronisation primitive is invented and every object is a real pthread
 * object underneath.
 *
 * The object types are the corresponding pthread types (pthread_t is a
 * pointer in OpenBSD, so thrd_t, mtx_t and cnd_t are complete object
 * types).  mtx_plain/mtx_recursive/mtx_timed are bit flags so that
 * "mtx_recursive | mtx_timed" works as the standard allows.
 *
 * The return values are the standard thrd_* codes; the pthread functions
 * return the error number directly, so they are mapped without consulting
 * errno.
 */

#ifndef _THREADS_H_
#define _THREADS_H_

#include <sys/cdefs.h>

#include <pthread.h>
#include <time.h>

/*
 * The base clang currently predefines __STDC_NO_THREADS__ when targeting
 * OpenBSD, because the base system has historically lacked <threads.h>.
 * This header is part of libc and does provide the interface, so the
 * precondition is stale; it is withdrawn here so that a translation unit
 * that includes <threads.h> sees a consistent implementation.  The base
 * compiler's target description should be updated to match; until then,
 * code that decides whether to include <threads.h> by testing the macro
 * before including anything will still see the stale value.
 */
#undef __STDC_NO_THREADS__

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L
#define __STDC_VERSION_THREADS_H__ 202311L
#endif

typedef pthread_t thrd_t;
typedef int (*thrd_start_t)(void *);

typedef pthread_mutex_t mtx_t;
typedef pthread_cond_t cnd_t;
typedef pthread_key_t tss_t;
typedef void (*tss_dtor_t)(void *);
typedef pthread_once_t once_flag;

enum {
	thrd_success = 0,
	thrd_busy = 1,
	thrd_error = 2,
	thrd_nomem = 3,
	thrd_timedout = 4
};

/*
 * Mutex types.  The values are distinct bits so that the recursive and
 * timed properties can be requested together.
 */
#define mtx_plain 0
#define mtx_recursive 1
#define mtx_timed 2

#define ONCE_FLAG_INIT PTHREAD_ONCE_INIT
#define TSS_DTOR_ITERATIONS PTHREAD_DESTRUCTOR_ITERATIONS

/*
 * thread_local was a convenience macro for _Thread_local in C11; C23 made
 * thread_local a keyword and removed the macro.  It never existed in C++.
 */
#if !defined(__cplusplus) &&                                                   \
    !(defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L)
#define thread_local _Thread_local
#endif

__BEGIN_DECLS

int thrd_create(thrd_t *, thrd_start_t, void *);
int thrd_equal(thrd_t, thrd_t);
thrd_t thrd_current(void);
int thrd_sleep(const struct timespec *, struct timespec *);
void thrd_yield(void);
__dead void thrd_exit(int);
int thrd_detach(thrd_t);
int thrd_join(thrd_t, int *);

int mtx_init(mtx_t *, int);
int mtx_lock(mtx_t *);
int mtx_timedlock(mtx_t *__restrict, const struct timespec *__restrict);
int mtx_trylock(mtx_t *);
int mtx_unlock(mtx_t *);
void mtx_destroy(mtx_t *);

int cnd_init(cnd_t *);
int cnd_signal(cnd_t *);
int cnd_broadcast(cnd_t *);
int cnd_wait(cnd_t *, mtx_t *);
int cnd_timedwait(
    cnd_t *__restrict, mtx_t *__restrict, const struct timespec *__restrict);
void cnd_destroy(cnd_t *);

int tss_create(tss_t *, tss_dtor_t);
void tss_delete(tss_t);
void *tss_get(tss_t);
int tss_set(tss_t, void *);

void call_once(once_flag *, void (*)(void));

__END_DECLS

#endif /* !_THREADS_H_ */
