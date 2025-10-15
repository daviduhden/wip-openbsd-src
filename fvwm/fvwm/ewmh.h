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

#ifndef FVWM_EWMH_H
#define FVWM_EWMH_H

#include <X11/Xlib.h>

struct FvwmWindow; /* forward */

/* Root/window EWMH atoms */
extern Atom _NET_SUPPORTED;
extern Atom _NET_SUPPORTING_WM_CHECK;
extern Atom _NET_CLIENT_LIST;
extern Atom _NET_CLIENT_LIST_STACKING;
extern Atom _NET_ACTIVE_WINDOW;
extern Atom _NET_NUMBER_OF_DESKTOPS;
extern Atom _NET_CURRENT_DESKTOP;
extern Atom _NET_DESKTOP_NAMES;
extern Atom _NET_DESKTOP_VIEWPORT;
extern Atom _NET_DESKTOP_GEOMETRY;
extern Atom _NET_WORKAREA;
extern Atom _NET_FRAME_EXTENTS;
extern Atom _NET_REQUEST_FRAME_EXTENTS;
extern Atom UTF8_STRING;
extern Atom _NET_CLOSE_WINDOW;
extern Atom _NET_WM_DESKTOP;
extern Atom _NET_WM_STATE;
extern Atom _NET_WM_STATE_STICKY;
extern Atom _NET_WM_STATE_ABOVE;
extern Atom _NET_WM_STATE_BELOW;
extern Atom _NET_WM_STATE_MAXIMIZED_VERT;
extern Atom _NET_WM_STATE_MAXIMIZED_HORZ;
extern Atom _NET_WM_STATE_FULLSCREEN;
extern Atom _NET_WM_ALLOWED_ACTIONS;
extern Atom _NET_WM_ACTION_CLOSE;
extern Atom _NET_WM_ACTION_MOVE;
extern Atom _NET_WM_ACTION_RESIZE;
extern Atom _NET_WM_ACTION_MINIMIZE;
extern Atom _NET_WM_ACTION_SHADE;
extern Atom _NET_WM_ACTION_STICK;
extern Atom _NET_WM_ACTION_MAXIMIZE_HORZ;
extern Atom _NET_WM_ACTION_MAXIMIZE_VERT;
extern Atom _NET_WM_ACTION_FULLSCREEN;
extern Atom _NET_WM_ACTION_CHANGE_DESKTOP;
extern Atom _NET_WM_ACTION_ABOVE;
extern Atom _NET_WM_ACTION_BELOW;

extern Atom _NET_WM_WINDOW_TYPE;
extern Atom _NET_WM_WINDOW_TYPE_NORMAL;
extern Atom _NET_WM_WINDOW_TYPE_DIALOG;
extern Atom _NET_WM_WINDOW_TYPE_UTILITY;
extern Atom _NET_WM_WINDOW_TYPE_TOOLBAR;
extern Atom _NET_WM_WINDOW_TYPE_DOCK;
extern Atom _NET_WM_WINDOW_TYPE_DESKTOP;
extern Atom _NET_WM_WINDOW_TYPE_SPLASH;
extern Atom _NET_WM_WINDOW_TYPE_MENU;
extern Atom _NET_WM_WINDOW_TYPE_DROPDOWN_MENU;
extern Atom _NET_WM_WINDOW_TYPE_POPUP_MENU;
extern Atom _NET_WM_WINDOW_TYPE_TOOLTIP;
extern Atom _NET_WM_WINDOW_TYPE_NOTIFICATION;
extern Atom _NET_WM_WINDOW_TYPE_COMBO;
extern Atom _NET_WM_WINDOW_TYPE_DND;
/* ICCCM/legacy hints consulted for type inference */
extern Atom WM_WINDOW_ROLE;

void EWMH_Init(void);
void EWMH_Shutdown(void);

/* Update root/window properties */
void EWMH_UpdateClientList(void);
void EWMH_UpdateClientListStacking(void);
void EWMH_UpdateActiveWindow(void);
void EWMH_UpdateNumberOfDesktops(int num_desktops);
void EWMH_UpdateCurrentDesktop(int desk);
void EWMH_UpdateDesktopGeometry(int width, int height);
void EWMH_UpdateDesktopViewport(int vx, int vy);
void EWMH_UpdateDesktopNames(int count);
void EWMH_UpdateWorkarea(int count);
void EWMH_RecalcDesktops(void);

/* Per-window updates */
void EWMH_SetWmDesktop(struct FvwmWindow *fw);
void EWMH_SetWmState(struct FvwmWindow *fw);
void EWMH_SetFrameExtents(struct FvwmWindow *fw);
void EWMH_SetAllowedActions(struct FvwmWindow *fw);
void EWMH_SetWindowType(struct FvwmWindow *fw);

/* Event handling: return True if handled */
int EWMH_HandleClientMessage(struct FvwmWindow *fw, XClientMessageEvent *xce);

#endif /* FVWM_EWMH_H */
