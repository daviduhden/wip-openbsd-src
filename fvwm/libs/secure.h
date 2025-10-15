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

#ifndef FVWM_SECURE_H
#define FVWM_SECURE_H

#ifdef __cplusplus
extern "C" {
#endif

/* 
 * Request the kernel to restrict process capabilities using pledge.
 * - promises: space-separated promise groups (same semantics as OpenBSD pledge)
 * - execpromises: promises to allow after exec(2) (may be NULL)
 * Returns 0 on success, or -1 on error (same convention as pledge).
 * On non-OpenBSD systems this function is a no-op that returns 0.
 */
int fvwm_pledge(const char *promises, const char *execpromises);

/*
 * Restrict filesystem visibility using unveil.
 * - path: filesystem path to reveal (NULL to remove all unveils)
 * - permissions: string of permission letters (e.g. "r", "rw", "x")
 * Returns 0 on success, or -1 on error (same convention as unveil).
 * On non-OpenBSD systems this function is a no-op that returns 0.
 */
int fvwm_unveil(const char *path, const char *permissions);

/*
 * Permanently lock the unveil configuration so no further unveil calls
 * can change the process's view of the filesystem.
 * Returns 0 on success, or -1 on error.
 * On non-OpenBSD systems this function is a no-op that returns 0.
 */
int fvwm_unveil_lock(void);

#ifdef __cplusplus
}
#endif

#endif /* FVWM_SECURE_H */
