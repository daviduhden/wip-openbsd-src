/****************************************************************************
 * This is an all new program to set the root window to an Xpm pixmap.
 * Copyright 1993, Rob Nation 
 * You may use this file for anything you want, as long as the copyright
 * is kept intact. No guarantees of any sort are made in any way regarding
 * this program or anything related to it.
 ****************************************************************************/

#include "config.h"

#include "../libs/fvwmlib.h"
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xos.h>
#include <X11/xpm.h> /* Has to be after Intrinsic.h gets included */
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Display *dpy;
int screen;
Window root;
char *display_name = NULL;
static void SetRootWindow(char *tline, XWindowAttributes *root_attr,
                          Atom colors_atom);
static void FreePreviousResources(Atom pixmap_atom, Atom colors_atom,
                                  XWindowAttributes *root_attr);
Pixmap rootXpm;

int main(int argc, char **argv)
{
  Atom prop, color_prop;
  XWindowAttributes root_attr;

  if (argc != 2)
  {
    fprintf(stderr, "Xpmroot Version %s\n", VERSION);
    fprintf(stderr, "Usage: xpmroot xpmfile\n");
    fprintf(stderr, "Try Again\n");
    exit(1);
  }
  dpy = XOpenDisplay(display_name);
  if (!dpy)
  {
    fprintf(stderr, "Xpmroot:  unable to open display '%s'\n",
            XDisplayName(display_name));
    exit(2);
  }
  screen = DefaultScreen(dpy);
  root = RootWindow(dpy, screen);
  XGetWindowAttributes(dpy, root, &root_attr);

  prop = XInternAtom(dpy, "_XSETROOT_ID", False);
  color_prop = XInternAtom(dpy, "_XSETROOT_COLORS", False);

  FreePreviousResources(prop, color_prop, &root_attr);

  SetRootWindow(argv[1], &root_attr, color_prop);

  XChangeProperty(dpy, root, prop, XA_PIXMAP, 32, PropModeReplace,
                  (unsigned char *)&rootXpm, 1);
  XSetCloseDownMode(dpy, RetainPermanent);
  XCloseDisplay(dpy);
  return 0;
}

static void SetRootWindow(char *tline, XWindowAttributes *root_attr,
                          Atom colors_atom)
{
  XpmAttributes xpm_attributes;
  Pixmap shapeMask;
  int val;

  memset(&xpm_attributes, 0, sizeof(xpm_attributes));
  xpm_attributes.colormap = root_attr->colormap;
  xpm_attributes.valuemask =
    XpmSize | XpmReturnAllocPixels | XpmColormap;
  if ((val = XpmReadFileToPixmap(dpy, root, tline, &rootXpm, &shapeMask,
                                 &xpm_attributes)) != XpmSuccess)
  {
    if (val == XpmOpenFailed)
      fprintf(stderr, "Couldn't open pixmap file\n");
    else if (val == XpmColorFailed)
      fprintf(stderr, "Couldn't allocate required colors\n");
    else if (val == XpmFileInvalid)
      fprintf(stderr, "Invalid Format for an Xpm File\n");
    else if (val == XpmColorError)
      fprintf(stderr, "Invalid Color specified in Xpm FIle\n");
    else if (val == XpmNoMemory)
      fprintf(stderr, "Insufficient Memory\n");
    exit(1);
  }

  if (shapeMask != None)
    XFreePixmap(dpy, shapeMask);

  XSetWindowBackgroundPixmap(dpy, root, rootXpm);
  XClearWindow(dpy, root);

  if ((xpm_attributes.valuemask & XpmReturnAllocPixels) &&
      xpm_attributes.nalloc_pixels > 0 &&
      xpm_attributes.alloc_pixels != NULL)
  {
    XChangeProperty(dpy, root, colors_atom, XA_CARDINAL, 32,
                    PropModeReplace,
                    (unsigned char *)xpm_attributes.alloc_pixels,
                    (int)xpm_attributes.nalloc_pixels);
  }
  else
  {
    XDeleteProperty(dpy, root, colors_atom);
  }

  XpmFreeAttributes(&xpm_attributes);
}

static void FreePreviousResources(Atom pixmap_atom, Atom colors_atom,
                                  XWindowAttributes *root_attr)
{
  Atom type;
  int format;
  unsigned long length, after;
  unsigned char *data = NULL;
  int visual_class =
    (root_attr->visual != NULL) ? root_attr->visual->class : StaticGray;
  Bool can_free_colors =
    (visual_class == PseudoColor || visual_class == GrayScale ||
     visual_class == DirectColor);

  if (XGetWindowProperty(dpy, root, colors_atom, 0L, (~0L), True,
                         XA_CARDINAL, &type, &format, &length, &after,
                         &data) == Success)
  {
    if (can_free_colors && type == XA_CARDINAL && format == 32 &&
        length > 0 && data != NULL)
    {
      Pixel *pixels = (Pixel *)data;
      XFreeColors(dpy, root_attr->colormap, pixels, (int)length, 0);
    }
    if (data != NULL)
      XFree(data);
  }

  data = NULL;
  if (XGetWindowProperty(dpy, root, pixmap_atom, 0L, 1L, True,
                         AnyPropertyType, &type, &format, &length,
                         &after, &data) == Success)
  {
    if ((type == XA_PIXMAP) && (format == 32) && (length == 1) &&
        data != NULL)
      XKillClient(dpy, *((Pixmap *)data));
    if (data != NULL)
      XFree(data);
  }
}
