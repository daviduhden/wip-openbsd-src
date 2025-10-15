/* 
 * Portable wrappers for OpenBSD pledge(2) and unveil(2).
 * On non-OpenBSD systems, these are no-ops that return 0.
 *
 * Copyright (c) 2025 David Uhden Collado <david@uhden.dev>
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

#include "config.h"

#include <errno.h>
#include <string.h>

/* Only include OpenBSD headers when building on OpenBSD. */
#ifdef __OpenBSD__
#include <unistd.h>
#endif

/*
 * fvwm_pledge:
 *  A thin portability wrapper around OpenBSD's pledge(2).
 *
 *  On OpenBSD:
 *    - Calls pledge(promises, execpromises) and returns its result.
 *
 *  On other systems:
 *    - The function is a no-op: the parameters are explicitly unused
 *      and 0 is returned to indicate success.
 *
 *  Parameters:
 *    - promises: A string describing the allowed pledge promises.
 *    - execpromises: Promises for the executed program, or NULL.
 *
 *  Returns:
 *    - The result of pledge(2) on OpenBSD, or 0 on non-OpenBSD systems.
 */
int fvwm_pledge(const char *promises, const char *execpromises)
{
#ifdef __OpenBSD__
    return pledge(promises, execpromises);
#else
    (void)promises; (void)execpromises; /* silence unused parameter warnings */
    return 0;
#endif
}

/*
 * fvwm_unveil:
 *  A thin portability wrapper around OpenBSD's unveil(2).
 *
 *  On OpenBSD:
 *    - Calls unveil(path, permissions) and returns its result.
 *
 *  On other systems:
 *    - The function is a no-op: the parameters are explicitly unused
 *      and 0 is returned to indicate success.
 *
 *  Parameters:
 *    - path: Filesystem path to unveil, or NULL to remove restrictions.
 *    - permissions: Permission string such as "r", "rw", or NULL.
 *
 *  Returns:
 *    - The result of unveil(2) on OpenBSD, or 0 on non-OpenBSD systems.
 */
int fvwm_unveil(const char *path, const char *permissions)
{
#ifdef __OpenBSD__
    return unveil(path, permissions);
#else
    (void)path; (void)permissions; /* silence unused parameter warnings */
    return 0;
#endif
}

/*
 * fvwm_unveil_lock:
 *  Wrapper to lock the unveil(2) state (prevent further changes).
 *
 *  On OpenBSD:
 *    - Calling unveil(NULL, NULL) locks the path/permission table.
 *
 *  On other systems:
 *    - No-op and return success (0).
 *
 *  Returns:
 *    - The result of unveil(NULL, NULL) on OpenBSD, or 0 otherwise.
 */
int fvwm_unveil_lock(void)
{
#ifdef __OpenBSD__
    return unveil(NULL, NULL);
#else
    return 0;
#endif
}
