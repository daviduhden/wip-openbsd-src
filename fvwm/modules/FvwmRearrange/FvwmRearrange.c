/*
 * FvwmRearrange: fvwm module to tile or cascade windows in a region.
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

#include <X11/Xlib.h>

#include <ctype.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "fvwmlib.h"
#include "../../fvwm/fvwm.h"
#include "../../fvwm/module.h"
#include "../../libs/secure.h"

/* --------------------------- Types & Globals --------------------------- */

typedef struct {
  Window frame;            /* Window frame id (fvwm frame)              */
  int title_h;             /* Title-bar height in pixels                 */
  int border_w;            /* Border width in pixels                     */
  unsigned long w, h;      /* Client (frame) width/height in pixels      */
} WinInfo;

typedef struct {
  WinInfo *data;
  int size;
  int cap;
} WinVec;

/* X / module I/O */
static Display *g_dpy = NULL;
static int g_screen_w = 0, g_screen_h = 0;
static int g_pipe[2] = {-1, -1};
static int g_fd_width = 0; /* for select() */
static FILE *g_console = NULL;

/* Options / switches (default values chosen to mirror the classic behavior) */
static int opt_ofs_x = 0, opt_ofs_y = 0; /* origin for tile/cascade */
static int opt_max_w = 0, opt_max_h = 0; /* cascade: max client size if resizing */
static int opt_lim_x = 0, opt_lim_y = 0; /* tile: max X/Y (region bottom-right)  */

static int opt_include_untitled = 0;
static int opt_include_transients = 0;
static int opt_include_maximized = 0;
static int opt_include_sticky = 0;
static int opt_all = 0;
static int opt_desk_anywhere = 0;

static int opt_reverse_order = 0;
static int opt_raise = 1;

static int opt_resize = 0;      /* request Resize */
static int opt_no_stretch = 0;  /* never expand beyond current size */
static int opt_flat_x = 0;      /* cascade: do not add border to X step */
static int opt_flat_y = 0;      /* cascade: do not add title+border to Y step */
static int opt_inc_x = 0;       /* cascade: additional X increment */
static int opt_inc_y = 0;       /* cascade: additional Y increment */
static int opt_horizontal = 0;  /* tile: fill by rows first (-h) */
static int opt_max_per_line = 0;/* tile: max per row/col (depending on -h) */

static char g_is_tile = 0;      /* behaves like “FvwmTile” */
static char g_is_cascade = 0;   /* behaves like “FvwmCascade” */

static const char *g_prog = "RearrangeX";

/* ----------------------------- Utilities ------------------------------ */

static void fatal(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  fputc('\n', stderr);
  exit(1);
}

static void log_console(const char *fmt, ...) {
  if (!g_console) return;
  va_list ap;
  va_start(ap, fmt);
  vfprintf(g_console, fmt, ap);
  va_end(ap);
  fputc('\n', g_console);
}

/* Simple vector of WinInfo */
static void wv_init(WinVec *v) {
  v->data = NULL; v->size = 0; v->cap = 0;
}
static void wv_free(WinVec *v) {
  free(v->data); v->data = NULL; v->size = v->cap = 0;
}
static void wv_push(WinVec *v, const WinInfo *w) {
  if (v->size == v->cap) {
    int ncap = v->cap ? v->cap * 2 : 16;
    v->data = (WinInfo *)saferealloc(v->data, ncap * sizeof(WinInfo));
    v->cap = ncap;
  }
  v->data[v->size++] = *w;
}

/* Percent-or-pixels parser.
 * Convention preserved from the classic module:
 * - If the last char is alphabetic (e.g. "300p" or "300x"), parse the number as *pixels*.
 * - Otherwise interpret as a percentage of "full".
 */
static int parse_dim_token(const char *s, unsigned long full) {
  int len = (int)strlen(s);
  if (len < 1) return 0;
  if (isalpha((unsigned char)s[len - 1])) {
    char buf[32];
    if (len >= (int)sizeof(buf)) len = (int)sizeof(buf) - 1;
    memcpy(buf, s, len - 1);
    buf[len - 1] = '\0';
    return atoi(buf);
  }
  return (int)((atoi(s) * (long)full) / 100L);
}

/* Read until we receive a M_CONFIGURE_WINDOW for a specific frame.
 * This ensures synchronous sequencing after Move/Resize.
 */
static void wait_for_configure(Window frame) {
  unsigned long hdr[HEADER_SIZE], *body = NULL;
  fd_set rfds;
  FD_ZERO(&rfds);
  FD_SET(g_pipe[1], &rfds);
  (void)select(g_fd_width, &rfds, NULL, NULL, NULL);

  for (;;) {
    int n = ReadFvwmPacket(g_pipe[1], hdr, &body);
    if (n > 0) {
      if (hdr[1] == M_CONFIGURE_WINDOW && (Window)body[1] == frame) {
        free(body);
        return;
      }
      free(body);
    }
  }
}

/* ---------------------------- Filtering ------------------------------- */

static int window_suitable(const unsigned long *b) {
  (void)fvwm_pledge("stdio", NULL);
  /* Body layout is defined by fvwm. The positions used below match fvwm's
     M_CONFIGURE_WINDOW format. */
  unsigned long flags = b[8];

  if ((flags & WINDOWLISTSKIP) && !opt_all) return 0;
  if ((flags & MAXIMIZED) && !opt_include_maximized) return 0;
  if ((flags & STICKY) && !opt_include_sticky) return 0;
  if (!(flags & MAPPED)) return 0;
  if (flags & ICONIFIED) return 0;

  /* Must be viewable in X (mapped and not unmapped/withdrawn). */
  XWindowAttributes xwa;
  if (!XGetWindowAttributes(g_dpy, (Window)b[1], &xwa)) return 0;
  if (xwa.map_state != IsViewable) return 0;

  /* If not scanning the entire desk, require at least partial on-screen. */
  if (!opt_desk_anywhere) {
    int x = (int)b[3], y = (int)b[4];
    int w = (int)b[5], h = (int)b[6];
    if (!(x < g_screen_w && y < g_screen_h && x + w > 0 && y + h > 0))
      return 0;
  }

  if (!(flags & TITLE) && !opt_include_untitled) return 0;
  if ((flags & TRANSIENT) && !opt_include_transients) return 0;

  return 1;
}

/* ------------------------- Module I/O: intake ------------------------- */

static void collect_windows(WinVec *out) {
  /* Ask fvwm/fvwm2 for the current window list. */
  SendInfo(g_pipe, "Send_WindowList", 0);

  /* Read packets until M_END_WINDOWLIST. Push suitable windows. */
  for (;;) {
    unsigned long hdr[HEADER_SIZE], *body = NULL;
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(g_pipe[1], &rfds);
    (void)select(g_fd_width, &rfds, NULL, NULL, NULL);

    int n = ReadFvwmPacket(g_pipe[1], hdr, &body);
    if (n <= 0) continue;

    if (hdr[1] == M_CONFIGURE_WINDOW) {
      if (window_suitable(body)) {
        WinInfo w;
        w.frame   = (Window)body[1];
        w.title_h = (int)body[9];
        w.border_w= (int)body[10];
        w.w       = body[5];
        w.h       = body[6];
        /* We append in *arrival order*. Later we iterate forward/backward
           to emulate classic ordering semantics. */
        wv_push(out, &w);
      }
    } else if (hdr[1] == M_END_WINDOWLIST) {
      free(body);
      break;
    }
    free(body);
  }
}

/* ------------------------------ Actions ------------------------------- */

static void do_tile(const WinVec *wv) {
  char buf[128];
  if (!opt_lim_x) opt_lim_x = g_screen_w;
  if (!opt_lim_y) opt_lim_y = g_screen_h;

  const int n = wv->size;
  if (n <= 0) return;

  /* Determine rows/cols from -h and -mn.
   * - If horizontal: limit per column is maxnum; compute columns from n.
   * - Otherwise: limit per row is maxnum; compute rows from n.
   */
  int lines = 1, per_line = n;
  if (opt_max_per_line > 0 && opt_max_per_line < n) {
    per_line = opt_max_per_line;
    lines = (n + per_line - 1) / per_line; /* ceil */
  } else {
    per_line = n;
    lines = 1;
  }

  const int available_w = (opt_lim_x - opt_ofs_x + 1);
  const int available_h = (opt_lim_y - opt_ofs_y + 1);

  int cols, rows;
  if (opt_horizontal) {
    rows = per_line;
    cols = lines;
  } else {
    cols = per_line;
    rows = lines;
  }

  if (cols < 1) cols = 1;
  if (rows < 1) rows = 1;

  const int cell_w = available_w / cols;
  const int cell_h = available_h / rows;

  /* Iterate in the same effective order as the classic module:
     - default (not reversed): process newest first.
     - reversed: process oldest first. */
  int index = opt_reverse_order ? 0 : (n - 1);
  const int step = opt_reverse_order ? +1 : -1;

  int cur_col = 0, cur_row = 0;

  for (int processed = 0; processed < n; ++processed, index += step) {
    const WinInfo *w = &wv->data[index];

    const int x = opt_ofs_x + cur_col * cell_w;
    const int y = opt_ofs_y + cur_row * cell_h;

    /* Frame area available to the client after decorations. */
    int target_w = cell_w - 2 * w->border_w;
    int target_h = cell_h - (2 * w->border_w + w->title_h);

    if (opt_resize) {
      if (opt_no_stretch) {
        if (target_w > (int)w->w) target_w = (int)w->w;
        if (target_h > (int)w->h) target_h = (int)w->h;
      }
      snprintf(buf, sizeof(buf), "Resize %lup %lup",
               (target_w > 0) ? (unsigned long)target_w : w->w,
               (target_h > 0) ? (unsigned long)target_h : w->h);
      SendInfo(g_pipe, buf, w->frame);
    }

    snprintf(buf, sizeof(buf), "Move %up %up", x, y);
    SendInfo(g_pipe, buf, w->frame);

    if (opt_raise) SendInfo(g_pipe, "Raise", w->frame);

    wait_for_configure(w->frame);

    /* Advance grid position. */
    if (opt_horizontal) {
      /* Fill downwards first (rows), then advance column. */
      cur_row++;
      if (cur_row == rows) { cur_row = 0; cur_col++; }
    } else {
      /* Fill across first (cols), then advance row. */
      cur_col++;
      if (cur_col == cols) { cur_col = 0; cur_row++; }
    }
  }
}

static void do_cascade(const WinVec *wv) {
  char buf[128];
  const int n = wv->size;
  if (n <= 0) return;

  int x = opt_ofs_x;
  int y = opt_ofs_y;

  /* Iterate with the same perceived order as the classic module. */
  int index = opt_reverse_order ? 0 : (n - 1);
  const int step = opt_reverse_order ? +1 : -1;

  for (int processed = 0; processed < n; ++processed, index += step) {
    const WinInfo *w = &wv->data[index];

    if (opt_raise) SendInfo(g_pipe, "Raise", w->frame);

    snprintf(buf, sizeof(buf), "Move %up %up", x, y);
    SendInfo(g_pipe, buf, w->frame);

    if (opt_resize) {
      unsigned long new_w = 0, new_h = 0;
      if (opt_no_stretch) {
        if (opt_max_w && (w->w > (unsigned long)opt_max_w)) new_w = opt_max_w;
        if (opt_max_h && (w->h > (unsigned long)opt_max_h)) new_h = opt_max_h;
      } else {
        new_w = (opt_max_w > 0) ? (unsigned long)opt_max_w : 0;
        new_h = (opt_max_h > 0) ? (unsigned long)opt_max_h : 0;
      }
      if (new_w || new_h) {
        snprintf(buf, sizeof(buf), "Resize %lup %lup",
                 new_w ? new_w : w->w,
                 new_h ? new_h : w->h);
        SendInfo(g_pipe, buf, w->frame);
      }
    }

    wait_for_configure(w->frame);

    /* Increment for the next window:
       - If not "flat", include decoration thickness, then add user increments. */
    if (!opt_flat_x) x += w->border_w;
    x += opt_inc_x;

    if (!opt_flat_y) y += w->border_w + w->title_h;
    y += opt_inc_y;
  }
}

/* ----------------------------- Arg parsing ---------------------------- */

static void parse_args(const char *src, int argc, char **argv, int start_idx) {
  int positional = 0;
  for (int i = start_idx; i < argc; ++i) {
    const char *a = argv[i];

    if (!strcmp(a, "-tile") || !strcmp(a, "-cascade")) {
      /* ignored: mode is decided earlier */
    } else if (!strcmp(a, "-u")) {
      opt_include_untitled = 1;
    } else if (!strcmp(a, "-t")) {
      opt_include_transients = 1;
    } else if (!strcmp(a, "-a")) {
      opt_all = 1;
      opt_include_untitled = 1;
      opt_include_transients = 1;
      opt_include_maximized = 1;
      if (g_is_cascade) opt_include_sticky = 1;
    } else if (!strcmp(a, "-r")) {
      opt_reverse_order = 1;
    } else if (!strcmp(a, "-noraise")) {
      opt_raise = 0;
    } else if (!strcmp(a, "-noresize")) {
      opt_resize = 0;
    } else if (!strcmp(a, "-resize")) {
      opt_resize = 1;
    } else if (!strcmp(a, "-nostretch")) {
      opt_no_stretch = 1;
    } else if (!strcmp(a, "-desk")) {
      opt_desk_anywhere = 1;
    } else if (!strcmp(a, "-flatx")) {
      opt_flat_x = 1;
    } else if (!strcmp(a, "-flaty")) {
      opt_flat_y = 1;
    } else if (!strcmp(a, "-h")) {
      opt_horizontal = 1;
    } else if (!strcmp(a, "-m")) {
      opt_include_maximized = 1;
    } else if (!strcmp(a, "-s")) {
      opt_include_sticky = 1;
    } else if (!strcmp(a, "-mn") && i + 1 < argc) {
      opt_max_per_line = atoi(argv[++i]);
    } else if (!strcmp(a, "-incx") && i + 1 < argc) {
      opt_inc_x = parse_dim_token(argv[++i], (unsigned long)g_screen_w);
    } else if (!strcmp(a, "-incy") && i + 1 < argc) {
      opt_inc_y = parse_dim_token(argv[++i], (unsigned long)g_screen_h);
    } else {
      /* Positional args:
       *   Tile mode:   ofsx, ofsy, limX, limY
       *   Cascade:     ofsx, ofsy, maxW, maxH
       * Values accept "NN" (percent) or "NNx" (pixels, last char alphabetic).
       */
      ++positional;
      if (positional == 1) {
        opt_ofs_x = parse_dim_token(a, (unsigned long)g_screen_w);
      } else if (positional == 2) {
        opt_ofs_y = parse_dim_token(a, (unsigned long)g_screen_h);
      } else if (positional == 3) {
        if (g_is_cascade) {
          opt_max_w = parse_dim_token(a, (unsigned long)g_screen_w);
        } else {
          opt_lim_x = parse_dim_token(a, (unsigned long)g_screen_w);
        }
      } else if (positional == 4) {
        if (g_is_cascade) {
          opt_max_h = parse_dim_token(a, (unsigned long)g_screen_h);
        } else {
          opt_lim_y = parse_dim_token(a, (unsigned long)g_screen_h);
        }
      } else {
        log_console("%s: %s: ignoring unknown arg %s", g_prog, src, a);
      }
    }
  }
}

/* ------------------------------- Main --------------------------------- */

static void on_sigpipe(int sig) { (void)sig; exit(0); }

int main(int argc, char **argv) {
  g_console = fopen("/dev/console", "w");
  if (!g_console) g_console = stderr;

  /* Program name (used to decide mode by convention) */
  const char *slash = strrchr(argv[0], '/');
  g_prog = slash ? slash + 1 : argv[0];

  if (argc < 6) {
#ifdef FVWM1
    fatal("%s: module should be executed by fvwm only", g_prog);
#else
    fatal("%s: module should be executed by fvwm2 only", g_prog);
#endif
  }

  g_pipe[0] = atoi(argv[1]);
  g_pipe[1] = atoi(argv[2]);

  g_dpy = XOpenDisplay(NULL);
  if (!g_dpy) {
    fatal("%s: couldn't open display %s", g_prog, XDisplayName(NULL));
  }
  signal(SIGPIPE, on_sigpipe);

  {
    int s = DefaultScreen(g_dpy);
    g_screen_w = DisplayWidth(g_dpy, s);
    g_screen_h = DisplayHeight(g_dpy, s);
  }
  g_fd_width = GetFdWidth();

  /* Decide mode: mimic classic behavior based on binary name or -tile. */
  if (strcmp(g_prog, "FvwmCascade") &&
      (!strcmp(g_prog, "FvwmTile") || (argc >= 7 && !strcmp(argv[6], "-tile")))) {
    g_is_tile = 1; g_is_cascade = 0; opt_resize = 1;
  } else {
    g_is_tile = 0; g_is_cascade = 1; opt_resize = 0;
  }

  /* Parse module arguments starting at index 6 (fvwm passes fds and more). */
  parse_args("module args", argc, argv, 6);

  /* Select messages we care about. */
#ifdef FVWM1
  {
    char msg[128];
    snprintf(msg, sizeof(msg), "SET_MASK %lu\n", (unsigned long)(
             M_CONFIGURE_WINDOW | M_END_WINDOWLIST));
    SendInfo(g_pipe, msg, 0);

#ifdef FVWM1_MOVENULL
    /* Avoid interactive placement in fvwm version 1 */
    if (!opt_ofs_x) ++opt_ofs_x;
    if (!opt_ofs_y) ++opt_ofs_y;
#endif
  }
#else
  SetMessageMask(g_pipe, M_CONFIGURE_WINDOW | M_END_WINDOWLIST);
#endif

  /* Collect candidates and perform arrangement. */
  WinVec windows;
  wv_init(&windows);
  collect_windows(&windows);

  if (windows.size > 0) {
    if (g_is_cascade)
      do_cascade(&windows);
    else
      do_tile(&windows);
  }

  wv_free(&windows);
  if (g_console && g_console != stderr) fclose(g_console);
  return 0;
}