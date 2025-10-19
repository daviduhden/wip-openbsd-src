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

extern Display *dpy;
extern int screen;
extern char *Module;

unsigned long
GetColor(char *name)
{
  Colormap cmap = DefaultColormap(dpy, screen);
  XColor spec = {0};

  if (!XParseColor(dpy, cmap, name, &spec)) {
    fprintf(stderr, "%s: unknown color \"%s\"\n", Module, name);
    exit(1);
  }

  if (!XAllocColor(dpy, cmap, &spec)) {
    fprintf(stderr, "%s: unable to allocate color for \"%s\"\n",
            Module, name);
    exit(1);
  }

  return spec.pixel;
}
