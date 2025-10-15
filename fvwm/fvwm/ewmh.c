/*
 * Minimal EWMH (NETWM) support for fvwm 2.x
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

#include <string.h>
#include <stdlib.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>

#include "fvwm.h"
#include "screen.h"
#include "misc.h"
#include "ewmh.h"

Atom _NET_SUPPORTED;
Atom _NET_SUPPORTING_WM_CHECK;
Atom _NET_CLIENT_LIST;
Atom _NET_CLIENT_LIST_STACKING;
Atom _NET_ACTIVE_WINDOW;
Atom _NET_NUMBER_OF_DESKTOPS;
Atom _NET_CURRENT_DESKTOP;
Atom _NET_DESKTOP_NAMES;
Atom _NET_DESKTOP_VIEWPORT;
Atom _NET_DESKTOP_GEOMETRY;
Atom _NET_WORKAREA;
Atom _NET_CLOSE_WINDOW;
Atom _NET_WM_DESKTOP;
Atom _NET_WM_STATE;
Atom _NET_WM_STATE_STICKY;
Atom _NET_WM_STATE_ABOVE;
Atom _NET_WM_STATE_BELOW;
Atom _NET_WM_STATE_MAXIMIZED_VERT;
Atom _NET_WM_STATE_MAXIMIZED_HORZ;
Atom _NET_WM_STATE_FULLSCREEN;
Atom _NET_FRAME_EXTENTS;
Atom _NET_REQUEST_FRAME_EXTENTS;
Atom UTF8_STRING;
Atom _NET_WM_ALLOWED_ACTIONS;
Atom _NET_WM_ACTION_CLOSE;
Atom _NET_WM_ACTION_MOVE;
Atom _NET_WM_ACTION_RESIZE;
Atom _NET_WM_ACTION_MINIMIZE;
Atom _NET_WM_ACTION_SHADE;
Atom _NET_WM_ACTION_STICK;
Atom _NET_WM_ACTION_MAXIMIZE_HORZ;
Atom _NET_WM_ACTION_MAXIMIZE_VERT;
Atom _NET_WM_ACTION_FULLSCREEN;
Atom _NET_WM_ACTION_CHANGE_DESKTOP;
Atom _NET_WM_ACTION_ABOVE;
Atom _NET_WM_ACTION_BELOW;

Atom _NET_WM_WINDOW_TYPE;
Atom _NET_WM_WINDOW_TYPE_NORMAL;
Atom _NET_WM_WINDOW_TYPE_DIALOG;
Atom _NET_WM_WINDOW_TYPE_UTILITY;
Atom _NET_WM_WINDOW_TYPE_TOOLBAR;
Atom _NET_WM_WINDOW_TYPE_DOCK;
Atom _NET_WM_WINDOW_TYPE_DESKTOP;
Atom _NET_WM_WINDOW_TYPE_SPLASH;
Atom _NET_WM_WINDOW_TYPE_MENU;
Atom _NET_WM_WINDOW_TYPE_DROPDOWN_MENU;
Atom _NET_WM_WINDOW_TYPE_POPUP_MENU;
Atom _NET_WM_WINDOW_TYPE_TOOLTIP;
Atom _NET_WM_WINDOW_TYPE_NOTIFICATION;
Atom _NET_WM_WINDOW_TYPE_COMBO;
Atom _NET_WM_WINDOW_TYPE_DND;
Atom WM_WINDOW_ROLE;

static Window ewmh_wm_window = None; /* the _NET_SUPPORTING_WM_CHECK window */

static void set_cardinal_list(Window w, Atom prop, unsigned long *vals, int n)
{
  XChangeProperty(dpy, w, prop, XA_CARDINAL, 32, PropModeReplace,
                  (unsigned char *)vals, n);
}

static void set_window(Window w, Atom prop, Window val)
{
  XChangeProperty(dpy, w, prop, XA_WINDOW, 32, PropModeReplace,
                  (unsigned char *)&val, 1);
}

static void ensure_wm_check_window(void)
{
  if (ewmh_wm_window != None)
    return;
  XSetWindowAttributes attr;
  attr.override_redirect = True;
  attr.event_mask = 0;
  ewmh_wm_window = XCreateWindow(dpy, Scr.Root, -10, -10, 1, 1, 0,
                                 CopyFromParent, InputOnly, CopyFromParent,
                                 CWOverrideRedirect | CWEventMask, &attr);
  XMapWindow(dpy, ewmh_wm_window);
  set_window(ewmh_wm_window, _NET_SUPPORTING_WM_CHECK, ewmh_wm_window);
  set_window(Scr.Root, _NET_SUPPORTING_WM_CHECK, ewmh_wm_window);
}

void EWMH_Init(void)
{
  _NET_SUPPORTED                = XInternAtom(dpy, "_NET_SUPPORTED", False);
  _NET_SUPPORTING_WM_CHECK      = XInternAtom(dpy, "_NET_SUPPORTING_WM_CHECK", False);
  _NET_CLIENT_LIST              = XInternAtom(dpy, "_NET_CLIENT_LIST", False);
  _NET_CLIENT_LIST_STACKING     = XInternAtom(dpy, "_NET_CLIENT_LIST_STACKING", False);
  _NET_ACTIVE_WINDOW            = XInternAtom(dpy, "_NET_ACTIVE_WINDOW", False);
  _NET_NUMBER_OF_DESKTOPS       = XInternAtom(dpy, "_NET_NUMBER_OF_DESKTOPS", False);
  _NET_CURRENT_DESKTOP          = XInternAtom(dpy, "_NET_CURRENT_DESKTOP", False);
  _NET_DESKTOP_NAMES            = XInternAtom(dpy, "_NET_DESKTOP_NAMES", False);
  _NET_DESKTOP_VIEWPORT         = XInternAtom(dpy, "_NET_DESKTOP_VIEWPORT", False);
  _NET_DESKTOP_GEOMETRY         = XInternAtom(dpy, "_NET_DESKTOP_GEOMETRY", False);
  _NET_WORKAREA                 = XInternAtom(dpy, "_NET_WORKAREA", False);
  _NET_CLOSE_WINDOW             = XInternAtom(dpy, "_NET_CLOSE_WINDOW", False);
  _NET_WM_DESKTOP               = XInternAtom(dpy, "_NET_WM_DESKTOP", False);
  _NET_WM_STATE                 = XInternAtom(dpy, "_NET_WM_STATE", False);
  _NET_WM_STATE_STICKY          = XInternAtom(dpy, "_NET_WM_STATE_STICKY", False);
  _NET_WM_STATE_ABOVE           = XInternAtom(dpy, "_NET_WM_STATE_ABOVE", False);
  _NET_WM_STATE_BELOW           = XInternAtom(dpy, "_NET_WM_STATE_BELOW", False);
  _NET_WM_STATE_MAXIMIZED_VERT  = XInternAtom(dpy, "_NET_WM_STATE_MAXIMIZED_VERT", False);
  _NET_WM_STATE_MAXIMIZED_HORZ  = XInternAtom(dpy, "_NET_WM_STATE_MAXIMIZED_HORZ", False);
  _NET_WM_STATE_FULLSCREEN      = XInternAtom(dpy, "_NET_WM_STATE_FULLSCREEN", False);
  _NET_FRAME_EXTENTS            = XInternAtom(dpy, "_NET_FRAME_EXTENTS", False);
  _NET_REQUEST_FRAME_EXTENTS    = XInternAtom(dpy, "_NET_REQUEST_FRAME_EXTENTS", False);
  UTF8_STRING                   = XInternAtom(dpy, "UTF8_STRING", False);
  _NET_WM_ALLOWED_ACTIONS       = XInternAtom(dpy, "_NET_WM_ALLOWED_ACTIONS", False);
  _NET_WM_ACTION_CLOSE          = XInternAtom(dpy, "_NET_WM_ACTION_CLOSE", False);
  _NET_WM_ACTION_MOVE           = XInternAtom(dpy, "_NET_WM_ACTION_MOVE", False);
  _NET_WM_ACTION_RESIZE         = XInternAtom(dpy, "_NET_WM_ACTION_RESIZE", False);
  _NET_WM_ACTION_MINIMIZE       = XInternAtom(dpy, "_NET_WM_ACTION_MINIMIZE", False);
  _NET_WM_ACTION_SHADE          = XInternAtom(dpy, "_NET_WM_ACTION_SHADE", False);
  _NET_WM_ACTION_STICK          = XInternAtom(dpy, "_NET_WM_ACTION_STICK", False);
  _NET_WM_ACTION_MAXIMIZE_HORZ  = XInternAtom(dpy, "_NET_WM_ACTION_MAXIMIZE_HORZ", False);
  _NET_WM_ACTION_MAXIMIZE_VERT  = XInternAtom(dpy, "_NET_WM_ACTION_MAXIMIZE_VERT", False);
  _NET_WM_ACTION_FULLSCREEN     = XInternAtom(dpy, "_NET_WM_ACTION_FULLSCREEN", False);
  _NET_WM_ACTION_CHANGE_DESKTOP = XInternAtom(dpy, "_NET_WM_ACTION_CHANGE_DESKTOP", False);
  _NET_WM_ACTION_ABOVE          = XInternAtom(dpy, "_NET_WM_ACTION_ABOVE", False);
  _NET_WM_ACTION_BELOW          = XInternAtom(dpy, "_NET_WM_ACTION_BELOW", False);

  _NET_WM_WINDOW_TYPE           = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
  _NET_WM_WINDOW_TYPE_NORMAL    = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_NORMAL", False);
  _NET_WM_WINDOW_TYPE_DIALOG    = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DIALOG", False);
  _NET_WM_WINDOW_TYPE_UTILITY   = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_UTILITY", False);
  _NET_WM_WINDOW_TYPE_TOOLBAR   = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_TOOLBAR", False);
  _NET_WM_WINDOW_TYPE_DOCK      = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DOCK", False);
  _NET_WM_WINDOW_TYPE_DESKTOP   = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DESKTOP", False);
  _NET_WM_WINDOW_TYPE_SPLASH    = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_SPLASH", False);
  _NET_WM_WINDOW_TYPE_MENU      = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_MENU", False);
  _NET_WM_WINDOW_TYPE_DROPDOWN_MENU = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DROPDOWN_MENU", False);
  _NET_WM_WINDOW_TYPE_POPUP_MENU    = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_POPUP_MENU", False);
  _NET_WM_WINDOW_TYPE_TOOLTIP       = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_TOOLTIP", False);
  _NET_WM_WINDOW_TYPE_NOTIFICATION  = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_NOTIFICATION", False);
  _NET_WM_WINDOW_TYPE_COMBO         = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_COMBO", False);
  _NET_WM_WINDOW_TYPE_DND           = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DND", False);
  WM_WINDOW_ROLE                = XInternAtom(dpy, "WM_WINDOW_ROLE", False);

  ensure_wm_check_window();

  /* Advertise supported hints */
  Atom supported[] = {
    _NET_SUPPORTED,
    _NET_SUPPORTING_WM_CHECK,
    _NET_CLIENT_LIST,
    _NET_CLIENT_LIST_STACKING,
    _NET_ACTIVE_WINDOW,
    _NET_NUMBER_OF_DESKTOPS,
    _NET_CURRENT_DESKTOP,
    _NET_DESKTOP_VIEWPORT,
    _NET_DESKTOP_GEOMETRY,
  _NET_WORKAREA,
    _NET_CLOSE_WINDOW,
    _NET_WM_DESKTOP,
    _NET_WM_STATE,
    _NET_WM_STATE_STICKY,
    _NET_WM_STATE_ABOVE,
    _NET_WM_STATE_BELOW,
    _NET_WM_STATE_MAXIMIZED_VERT,
    _NET_WM_STATE_MAXIMIZED_HORZ,
    _NET_WM_STATE_FULLSCREEN,
    _NET_FRAME_EXTENTS,
    _NET_REQUEST_FRAME_EXTENTS,
    _NET_WM_ALLOWED_ACTIONS,
    _NET_WM_WINDOW_TYPE,
    _NET_WM_WINDOW_TYPE_NORMAL,
    _NET_WM_WINDOW_TYPE_DIALOG,
    _NET_WM_WINDOW_TYPE_UTILITY,
    _NET_WM_WINDOW_TYPE_TOOLBAR,
    _NET_WM_WINDOW_TYPE_DOCK,
    _NET_WM_WINDOW_TYPE_DESKTOP,
    _NET_WM_WINDOW_TYPE_SPLASH,
    _NET_WM_WINDOW_TYPE_MENU,
    _NET_WM_WINDOW_TYPE_DROPDOWN_MENU,
    _NET_WM_WINDOW_TYPE_POPUP_MENU,
    _NET_WM_WINDOW_TYPE_TOOLTIP,
    _NET_WM_WINDOW_TYPE_NOTIFICATION,
    _NET_WM_WINDOW_TYPE_COMBO,
    _NET_WM_WINDOW_TYPE_DND
  };
  XChangeProperty(dpy, Scr.Root, _NET_SUPPORTED, XA_ATOM, 32,
                  PropModeReplace, (unsigned char *)supported,
                  sizeof(supported)/sizeof(supported[0]));

  /* Initialize static properties */
  EWMH_UpdateDesktopGeometry(Scr.MyDisplayWidth, Scr.MyDisplayHeight);
  EWMH_UpdateDesktopViewport(Scr.Vx, Scr.Vy);
  EWMH_UpdateCurrentDesktop(Scr.CurrentDesk);
  EWMH_RecalcDesktops();
  EWMH_UpdateClientList();
  EWMH_UpdateClientListStacking();
  EWMH_UpdateActiveWindow();
}

void EWMH_Shutdown(void)
{
  if (ewmh_wm_window != None)
  {
    XDestroyWindow(dpy, ewmh_wm_window);
    ewmh_wm_window = None;
  }
}

void EWMH_UpdateClientList(void)
{
  int count = 0;
  struct FvwmWindow *t;
  for (t = Scr.FvwmRoot.next; t != NULL; t = t->next)
    count++;
  if (count == 0)
  {
    XDeleteProperty(dpy, Scr.Root, _NET_CLIENT_LIST);
    return;
  }
  Window *list = (Window *)safemalloc(sizeof(Window) * count);
  int i = 0;
  for (t = Scr.FvwmRoot.next; t != NULL; t = t->next)
    list[i++] = t->w;
  XChangeProperty(dpy, Scr.Root, _NET_CLIENT_LIST, XA_WINDOW, 32,
                  PropModeReplace, (unsigned char *)list, count);
  free(list);
}

void EWMH_UpdateClientListStacking(void)
{
  int count = 0;
  struct FvwmWindow *t;
  for (t = Scr.FvwmRoot.stack_next; t != &Scr.FvwmRoot; t = t->stack_next)
    count++;
  if (count == 0)
  {
    XDeleteProperty(dpy, Scr.Root, _NET_CLIENT_LIST_STACKING);
    return;
  }
  Window *list = (Window *)safemalloc(sizeof(Window) * count);
  int i = 0;
  for (t = Scr.FvwmRoot.stack_next; t != &Scr.FvwmRoot; t = t->stack_next)
    list[i++] = t->w;
  XChangeProperty(dpy, Scr.Root, _NET_CLIENT_LIST_STACKING, XA_WINDOW, 32,
                  PropModeReplace, (unsigned char *)list, count);
  free(list);
}

void EWMH_UpdateActiveWindow(void)
{
  Window w = None;
  if (Scr.Focus)
    w = Scr.Focus->w;
  set_window(Scr.Root, _NET_ACTIVE_WINDOW, w);
}

void EWMH_UpdateNumberOfDesktops(int num_desktops)
{
  unsigned long v = (num_desktops < 1) ? 1 : (unsigned long)num_desktops;
  set_cardinal_list(Scr.Root, _NET_NUMBER_OF_DESKTOPS, &v, 1);
}

void EWMH_UpdateCurrentDesktop(int desk)
{
  unsigned long v = (unsigned long)desk;
  set_cardinal_list(Scr.Root, _NET_CURRENT_DESKTOP, &v, 1);
}

void EWMH_UpdateDesktopGeometry(int width, int height)
{
  unsigned long vals[2];
  vals[0] = (unsigned long)width;
  vals[1] = (unsigned long)height;
  set_cardinal_list(Scr.Root, _NET_DESKTOP_GEOMETRY, vals, 2);
}

void EWMH_UpdateDesktopViewport(int vx, int vy)
{
  unsigned long vals[2];
  vals[0] = (unsigned long)vx;
  vals[1] = (unsigned long)vy;
  set_cardinal_list(Scr.Root, _NET_DESKTOP_VIEWPORT, vals, 2);
}

void EWMH_UpdateDesktopNames(int count)
{
  if (count < 1) count = 1;
  /* Build a null-separated UTF-8 string list: "Desk 0\0Desk 1\0..." */
  char buf[16];
  int total = 0;
  /* First pass to compute size */
  for (int i = 0; i < count; i++)
  {
    int n = snprintf(buf, sizeof(buf), "Desk %d", i);
    total += n + 1; /* +1 for NUL */
  }
  unsigned char *data = (unsigned char *)safemalloc(total);
  int off = 0;
  for (int i = 0; i < count; i++)
  {
    int n = snprintf((char *)data + off, total - off, "Desk %d", i);
    off += n + 1;
  }
  XChangeProperty(dpy, Scr.Root, _NET_DESKTOP_NAMES, UTF8_STRING, 8,
                  PropModeReplace, data, total);
  free(data);
}

void EWMH_UpdateWorkarea(int count)
{
  if (count < 1) count = 1;
  /* x, y, width, height per desktop */
  unsigned long *vals = (unsigned long *)safemalloc(sizeof(unsigned long) * 4 * count);
  for (int i = 0; i < count; i++)
  {
    vals[i*4 + 0] = 0;
    vals[i*4 + 1] = 0;
    vals[i*4 + 2] = (unsigned long)Scr.MyDisplayWidth;
    vals[i*4 + 3] = (unsigned long)Scr.MyDisplayHeight;
  }
  set_cardinal_list(Scr.Root, _NET_WORKAREA, vals, 4 * count);
  free(vals);
}

void EWMH_RecalcDesktops(void)
{
  /* Choose N = max_desk + 1; EWMH indices are 0..N-1. */
  int max_desk = (Scr.CurrentDesk > 0) ? Scr.CurrentDesk : 0;
  struct FvwmWindow *t;
  for (t = Scr.FvwmRoot.next; t != NULL; t = t->next)
  {
    if (t->Desk > max_desk) max_desk = t->Desk;
  }
  int n = max_desk + 1;
  if (n < 1) n = 1;
  EWMH_UpdateNumberOfDesktops(n);
  EWMH_UpdateDesktopNames(n);
  EWMH_UpdateWorkarea(n);
}

void EWMH_SetWmDesktop(struct FvwmWindow *fw)
{
  if (!fw) return;
  unsigned long v = (unsigned long)fw->Desk;
  XChangeProperty(dpy, fw->w, _NET_WM_DESKTOP, XA_CARDINAL, 32,
                  PropModeReplace, (unsigned char *)&v, 1);
}

void EWMH_SetWmState(struct FvwmWindow *fw)
{
  if (!fw) return;
  /* Build list of state atoms. Keep it small for now. */
  Atom atoms[6];
  int n = 0;
  if (fw->flags & STICKY) atoms[n++] = _NET_WM_STATE_STICKY;
  if (fw->flags & ONTOP) atoms[n++] = _NET_WM_STATE_ABOVE;
  if (fw->flags & MAXIMIZED) {
    atoms[n++] = _NET_WM_STATE_MAXIMIZED_VERT;
    atoms[n++] = _NET_WM_STATE_MAXIMIZED_HORZ;
  }
  /* fvwm has no explicit BELOW flag in this vintage; skip unless available */
  if (n == 0)
  {
    XDeleteProperty(dpy, fw->w, _NET_WM_STATE);
    return;
  }
  XChangeProperty(dpy, fw->w, _NET_WM_STATE, XA_ATOM, 32, PropModeReplace,
                  (unsigned char *)atoms, n);
}

void EWMH_SetFrameExtents(struct FvwmWindow *fw)
{
  if (!fw) return;
  unsigned long extents[4]; /* left, right, top, bottom */
  unsigned long bw = (unsigned long)((fw->boundary_width < 0) ? 0 : fw->boundary_width);
  unsigned long top = bw;
  if (fw->flags & TITLE)
    top += (unsigned long)((fw->title_height < 0) ? 0 : fw->title_height);
  extents[0] = bw;            /* left */
  extents[1] = bw;            /* right */
  extents[2] = top;           /* top */
  extents[3] = bw;            /* bottom */
  set_cardinal_list(fw->w, _NET_FRAME_EXTENTS, extents, 4);
}

void EWMH_SetAllowedActions(struct FvwmWindow *fw)
{
  if (!fw) return;
  Atom acts[16];
  int n = 0;
  acts[n++] = _NET_WM_ACTION_CLOSE;
  acts[n++] = _NET_WM_ACTION_MOVE;
  acts[n++] = _NET_WM_ACTION_RESIZE;
  acts[n++] = _NET_WM_ACTION_MINIMIZE;
  acts[n++] = _NET_WM_ACTION_STICK;
  acts[n++] = _NET_WM_ACTION_MAXIMIZE_HORZ;
  acts[n++] = _NET_WM_ACTION_MAXIMIZE_VERT;
  acts[n++] = _NET_WM_ACTION_FULLSCREEN;
  acts[n++] = _NET_WM_ACTION_CHANGE_DESKTOP;
  acts[n++] = _NET_WM_ACTION_ABOVE;
  acts[n++] = _NET_WM_ACTION_BELOW;
#ifdef WINDOWSHADE
  acts[n++] = _NET_WM_ACTION_SHADE;
#endif
  XChangeProperty(dpy, fw->w, _NET_WM_ALLOWED_ACTIONS, XA_ATOM, 32,
                  PropModeReplace, (unsigned char *)acts, n);
}

void EWMH_SetWindowType(struct FvwmWindow *fw)
{
  if (!fw) return;
  /* If client already set _NET_WM_WINDOW_TYPE, don't override. */
  Atom actual_type;
  int actual_format;
  unsigned long nitems, bytes_after;
  unsigned char *prop = NULL;
  if (XGetWindowProperty(dpy, fw->w, _NET_WM_WINDOW_TYPE, 0, 32, False,
                         XA_ATOM, &actual_type, &actual_format,
                         &nitems, &bytes_after, &prop) == Success && prop)
  {
    XFree(prop);
    return;
  }

  /* Infer from hints: transient/dialog; toolbar/utility by role/class; dock/desktop by override-redirect and class. */
  Atom inferred = None;

  /* 1) Transient windows are usually dialogs. */
  if ((fw->flags & TRANSIENT) || (fw->transientfor && fw->transientfor != None && fw->transientfor != Scr.Root))
  {
    inferred = _NET_WM_WINDOW_TYPE_DIALOG;
  }

  /* 2) Role or class patterns indicating splash/menus/toolbars/utilities. */
  if (inferred == None)
  {
    /* Try WM_WINDOW_ROLE text */
    unsigned char *role = NULL;
    if (XGetWindowProperty(dpy, fw->w, WM_WINDOW_ROLE, 0, 32, False,
                           XA_STRING, &actual_type, &actual_format,
                           &nitems, &bytes_after, &role) == Success && role)
    {
      /* Heuristic: roles like "toolbox", "toolbar", "utility" */
      char *r = (char *)role;
      if (r && (*r))
      {
        /* simple string checks (case variants covered) */
        if (strstr(r, "toolbar") || strstr(r, "ToolBar") || strstr(r, "TOOLBAR"))
          inferred = _NET_WM_WINDOW_TYPE_TOOLBAR;
        else if (strstr(r, "utility") || strstr(r, "Utility") || strstr(r, "UTILITY") || strstr(r, "toolbox"))
          inferred = _NET_WM_WINDOW_TYPE_UTILITY;
        else if (strstr(r, "menu") || strstr(r, "Menu"))
          inferred = _NET_WM_WINDOW_TYPE_MENU;
        else if (strstr(r, "splash") || strstr(r, "Splash"))
          inferred = _NET_WM_WINDOW_TYPE_SPLASH;
      }
      XFree(role);
    }
  }

  /* 3) Class/resource name heuristics */
  if (inferred == None && fw->class.res_class)
  {
    const char *rc = fw->class.res_class;
    if (strstr(rc, "Dock") || strstr(rc, "dock") || strstr(rc, "Panel") || strstr(rc, "panel"))
      inferred = _NET_WM_WINDOW_TYPE_DOCK;
    else if (strstr(rc, "Toolbar") || strstr(rc, "toolbar"))
      inferred = _NET_WM_WINDOW_TYPE_TOOLBAR;
    else if (strstr(rc, "Utility") || strstr(rc, "utility") || strstr(rc, "Toolbox") || strstr(rc, "toolbox"))
      inferred = _NET_WM_WINDOW_TYPE_UTILITY;
    else if (strstr(rc, "Menu") || strstr(rc, "menu"))
      inferred = _NET_WM_WINDOW_TYPE_MENU;
    else if (strstr(rc, "Splash") || strstr(rc, "splash"))
      inferred = _NET_WM_WINDOW_TYPE_SPLASH;
  }
  if (inferred == None && fw->class.res_name)
  {
    const char *rn = fw->class.res_name;
    if (strstr(rn, "dock") || strstr(rn, "panel"))
      inferred = _NET_WM_WINDOW_TYPE_DOCK;
    else if (strstr(rn, "toolbar"))
      inferred = _NET_WM_WINDOW_TYPE_TOOLBAR;
    else if (strstr(rn, "utility") || strstr(rn, "toolbox"))
      inferred = _NET_WM_WINDOW_TYPE_UTILITY;
    else if (strstr(rn, "menu"))
      inferred = _NET_WM_WINDOW_TYPE_MENU;
    else if (strstr(rn, "splash"))
      inferred = _NET_WM_WINDOW_TYPE_SPLASH;
  }

  /* 4) Override-redirect full-screen background often indicates a desktop. */
  if (inferred == None && fw->attr.override_redirect)
  {
    /* If it covers the root and is not a menu, treat as dock/desktop */
    if (fw->frame_width >= Scr.MyDisplayWidth && fw->frame_height >= Scr.MyDisplayHeight)
      inferred = _NET_WM_WINDOW_TYPE_DESKTOP;
    else
      inferred = _NET_WM_WINDOW_TYPE_DOCK; /* struts/panels often o-r */
  }

  if (inferred == None)
    inferred = _NET_WM_WINDOW_TYPE_NORMAL;

  XChangeProperty(dpy, fw->w, _NET_WM_WINDOW_TYPE, XA_ATOM, 32,
                  PropModeReplace, (unsigned char *)&inferred, 1);
}

/* Handle a small subset of client messages: _NET_CLOSE_WINDOW */
static void ewmh_call_function(struct FvwmWindow *fw, const char *cmd)
{
  /* Helper to invoke an fvwm built-in from here */
  XEvent ev;
  memset(&ev, 0, sizeof(ev));
  ev.type = ClientMessage;
  ExecuteFunction((char *)cmd, fw, &ev, C_FRAME, -1);
}

static void ewmh_apply_state_action(struct FvwmWindow *fw, Atom atom, long action)
{
  /* action: 0 remove, 1 add, 2 toggle */
  if (!fw || !atom) return;

  if (atom == _NET_WM_STATE_STICKY)
  {
    if (action == 2 || (action == 1 && !(fw->flags & STICKY)) ||
        (action == 0 && (fw->flags & STICKY)))
    {
      ewmh_call_function(fw, "Stick");
    }
    EWMH_SetWmState(fw);
    return;
  }

  if (atom == _NET_WM_STATE_ABOVE)
  {
    if (action == 2)
      fw->flags ^= ONTOP;
    else if (action == 1)
      fw->flags |= ONTOP;
    else if (action == 0)
      fw->flags &= ~ONTOP;
    KeepOnTop();
    EWMH_SetWmState(fw);
    EWMH_UpdateClientListStacking();
    return;
  }

  if (atom == _NET_WM_STATE_BELOW)
  {
    /* fvwm 2.x has no persistent BELOW flag; approximate via lowering */
    if (action == 1 || action == 2)
    {
      fw->flags &= ~ONTOP;
      LowerWindow(fw);
      EWMH_UpdateClientListStacking();
    }
    EWMH_SetWmState(fw);
    return;
  }

  if (atom == _NET_WM_STATE_MAXIMIZED_VERT)
  {
    if (action == 2)
    {
      ewmh_call_function(fw, (fw->flags & MAXIMIZED) ? "Maximize" : "Maximize 0 100");
    }
    else if (action == 1)
    {
      if (!(fw->flags & MAXIMIZED)) ewmh_call_function(fw, "Maximize 0 100");
    }
    else if (action == 0)
    {
      if (fw->flags & MAXIMIZED) ewmh_call_function(fw, "Maximize");
    }
    EWMH_SetWmState(fw);
    return;
  }

  if (atom == _NET_WM_STATE_MAXIMIZED_HORZ)
  {
    if (action == 2)
    {
      ewmh_call_function(fw, (fw->flags & MAXIMIZED) ? "Maximize" : "Maximize 100 0");
    }
    else if (action == 1)
    {
      if (!(fw->flags & MAXIMIZED)) ewmh_call_function(fw, "Maximize 100 0");
    }
    else if (action == 0)
    {
      if (fw->flags & MAXIMIZED) ewmh_call_function(fw, "Maximize");
    }
    EWMH_SetWmState(fw);
    return;
  }

  if (atom == _NET_WM_STATE_FULLSCREEN)
  {
    if (action == 2)
    {
      ewmh_call_function(fw, (fw->flags & MAXIMIZED) ? "Maximize" : "Maximize 0 0");
    }
    else if (action == 1)
    {
      if (!(fw->flags & MAXIMIZED)) ewmh_call_function(fw, "Maximize 0 0");
    }
    else if (action == 0)
    {
      if (fw->flags & MAXIMIZED) ewmh_call_function(fw, "Maximize");
    }
    EWMH_SetWmState(fw);
    return;
  }
}

int EWMH_HandleClientMessage(struct FvwmWindow *fw, XClientMessageEvent *xce)
{
  if (!xce) return 0;
  if (xce->message_type == _NET_CLOSE_WINDOW)
  {
    /* Honor timestamp if provided in data.l[0]; not needed here. */
    if (fw && (fw->flags & DoesWmDeleteWindow))
    {
      send_clientmessage(dpy, fw->w, _XA_WM_DELETE_WINDOW, CurrentTime);
    }
    else if (fw)
    {
      XKillClient(dpy, fw->w);
    }
    return 1;
  }
  else if (xce->message_type == _NET_WM_STATE)
  {
    long action = xce->data.l[0];
    Atom a1 = (Atom)xce->data.l[1];
    Atom a2 = (Atom)xce->data.l[2];
    if (action == 0 || action == 1 || action == 2)
    {
      if (a1) ewmh_apply_state_action(fw, a1, action);
      if (a2) ewmh_apply_state_action(fw, a2, action);
      return 1;
    }
  }
  else if (xce->message_type == _NET_REQUEST_FRAME_EXTENTS)
  {
    if (fw)
    {
      EWMH_SetFrameExtents(fw);
      return 1;
    }
  }
  else if (xce->message_type == _NET_ACTIVE_WINDOW)
  {
    /* Request to activate (focus) a window. Basic implementation:
     * raise and focus the target window. Honor timestamp in data.l[1] if needed.
     */
    if (fw)
    {
      /* Raise first so focus isn't immediately obscured */
      RaiseWindow(fw);
      KeepOnTop();
      SetFocus(fw->w, fw, 0);
      EWMH_UpdateActiveWindow();
      EWMH_UpdateClientListStacking();
    }
    return 1;
  }
  return 0;
}
