/*
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

#include <X11/Xlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

extern Display *dpy;
extern int screen;
extern char *Module;

static void die(const char *fmt, const char *arg) {
  /* Uniform error reporting that includes the module name. */
  if (Module && *Module)
    fprintf(stderr, "%s: ", Module);
  else
    fprintf(stderr, "module: ");
  fprintf(stderr, fmt, arg ? arg : "(null)");
  fputc('\n', stderr);
  exit(1);
}

unsigned long GetColor(char *name)
{
  /* Defensive checks first. */
  if (dpy == NULL)
    die("cannot access X display while resolving color \"%s\"", name ? name : "");

  if (name == NULL || *name == '\0')
    die("invalid (empty) color name \"%s\"", name ? name : "");

  /* Resolve default colormap for the specified screen. */
  Screen *scr = ScreenOfDisplay(dpy, screen);
  if (scr == NULL)
    die("failed to resolve screen for color \"%s\"", name);

  Colormap cmap = DefaultColormapOfScreen(scr);

  /* First parse the textual color (so we can distinguish parse vs. alloc errors). */
  XColor xc;
  if (!XParseColor(dpy, cmap, name, &xc)) {
    die("unknown color name \"%s\"", name);
  }

  /* Now try to allocate that exact color in the colormap. */
  if (!XAllocColor(dpy, cmap, &xc)) {
    die("unable to allocate color \"%s\"", name);
  }

  return xc.pixel;
}