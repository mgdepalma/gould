/**
 * Copyright (C) Generations Linux <bugs@softcraft.org>
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; You may only use version 2 of the License,
 * you have no option to use any other version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
/*
* Code portions adopted from xgrabsc 2.41
*/

#include "gould.h"
#include "grabber.h"

/*
 * (private) Create a Pixmap from an XImage
 */
static void
CreatePixmapFromImage(Display *display, Drawable drawable,
		           XImage *ximage, Pixmap *pixmap)
{
    GC gc;
    XGCValues values;

    *pixmap = XCreatePixmap(display, drawable, ximage->width,
                               ximage->height, ximage->depth);

    /* set fg and bg in case we have an XYBitmap */
    values.foreground = 1;
    values.background = 0;
    gc = XCreateGC(display, *pixmap, GCForeground | GCBackground, &values);

    XPutImage(display, *pixmap, gc, ximage, 0, 0, 0, 0,
	      ximage->width, ximage->height);

    XFreeGC(display, gc);
} /* <CreatePixmapFromImage/> */

/*
 * grab general screen grab method
 */
XImage *
grab(Window drawable, XRectangle *xrect)
{
  Display *display = GDK_WINDOW_XDISPLAY( GDK_ROOT_PARENT() );
  XImage *ximage = XGetImage(display,
                             drawable,
                             xrect->x, xrect->y,
                             xrect->width, xrect->height,
                             AllPlanes,
                             ZPixmap);
  return ximage;
} /* </grab> */

/*
 * choose a window as in xwd
 */
Window
grab_window(Display *display, Window root)
{
  Cursor cursor = XCreateFontCursor(display, XC_target);
  Window result = None;
  XEvent event;
  int status;


  status = XGrabPointer(display, root, FALSE,
                        ButtonPressMask|ButtonReleaseMask, GrabModeSync,
                        GrabModeAsync, root, cursor, CurrentTime);

  if (status != GrabSuccess) {
    printf("can't grab mouse\n");
    return None;
  }

  while (TRUE) {
    XAllowEvents(display, SyncPointer, CurrentTime);
    XWindowEvent(display, root, ButtonPressMask|ButtonReleaseMask, &event);

    switch (event.type) {
      case ButtonRelease:
        result = event.xbutton.subwindow;
        if (result == None) result = root;
        XUngrabPointer(display, CurrentTime);      /* Done with pointer */
	return result;
        break;
    }
  }
} /* </grab_window> */

/*
 * Let the user stretch a rectangle on the screen and return its values.
 * It may be wise to grab the server before calling this routine.  If the
 * screen is allowed to change during XOR drawing video droppings may result.
 */
int
grab_rectangle(Display *display, Window root, XRectangle *xrect)
{
  Cursor cursor_start, cursor_finis;
  unsigned int x, y, rootx, rooty;
  int rx, ry, rw, rh;
  int drawn = False;
  XEvent event;
  GC gc;

  /* get some cursors for rectangle formation */
  cursor_start = XCreateFontCursor(display, XC_ul_angle);
  cursor_finis = XCreateFontCursor(display, XC_lr_angle);

  /* grab the pointer */
  if (GrabSuccess != XGrabPointer(display, root, False, ButtonPressMask,
        GrabModeAsync, GrabModeAsync, root, cursor_start, CurrentTime)) {
    return 1;  /* see GRAB_ERROR_1 */
  }

  /* create a graphics context to draw with */
  gc = XCreateGC(display, root, 0, NULL);
  if (!gc) {
    return 2; /* see GRAB_ERROR_2 */
  }
  XSetSubwindowMode(display, gc, IncludeInferiors);
  XSetForeground(display, gc, 255);
  XSetFunction(display, gc, GXxor);

  /* get a button-press and pull out the root location */
  XMaskEvent(display, ButtonPressMask, &event);
  rootx = rx = event.xbutton.x_root;
  rooty = ry = event.xbutton.y_root;

  /* get pointer motion events */
  XChangeActivePointerGrab(display, ButtonMotionMask | ButtonReleaseMask,
        cursor_finis, CurrentTime);

  /* MAKE_RECT converts the original root coordinates and the event root
   * coordinates into a rectangle in xrect */
#define MAKE_RECT(etype) \
  x = event.etype.x_root;       \
  y = event.etype.y_root;       \
  rw  = x - rootx;              \
  if (rw  < 0) rw  = -rw;       \
  rh  = y - rooty;              \
  if (rh  < 0) rh  = -rh;       \
  rx = x < rootx ? x : rootx;   \
  ry = y < rooty ? y : rooty

  /* loop to let the user drag a rectangle */
  while (TRUE) {
    XNextEvent(display, &event);
    switch(event.type) {
      case ButtonRelease:
        if (drawn) {
          XDrawRectangle(display, root, gc, rx, ry, rw, rh);
          drawn = False;
        }
        XFlush(display);
        /* record the final location */
        MAKE_RECT(xbutton);
        /* release resources */
        XFreeGC(display, gc);
        XFreeCursor(display, cursor_start);
        XFreeCursor(display, cursor_finis);
        xrect->x      = rx;
        xrect->y      = ry;
        xrect->width  = rw;
        xrect->height = rh;
        XUngrabPointer(display, CurrentTime);
	XSync(display, FALSE);
        return True;
      case MotionNotify:
        if (drawn) {
          XDrawRectangle(display, root, gc, rx, ry, rw, rh);
          drawn = False;
        }
        while (XCheckTypedEvent(display, MotionNotify, &event))
          {}
        MAKE_RECT(xmotion);
        XDrawRectangle(display, root, gc, rx, ry, rw, rh);
        drawn = True;
        break;
    }
  }

  return 0;
} /* </grab_rectagle> */

/*
 * window/screen screenshot returns pixbuf
 */
GdkPixbuf *
grab_pixbuf(Window xid, XRectangle *xrect)
{
  GdkWindow *window;
  gint x = 0, y = 0;
  gint width, height;
	
  if (xid == None) {
    window = gdk_get_default_root_window ();

    if (xrect != NULL) {
      x      = xrect->x;
      y      = xrect->y;
      width  = xrect->width;
      height = xrect->height; 
    }
    else {
      width  = gdk_screen_width ();
      height = gdk_screen_height ();
    }
  }
  else {
    gint xpos, ypos;
    window = gdk_window_foreign_new (xid);

    gdk_drawable_get_size (window, &width, &height);
    gdk_window_get_origin (window, &xpos, &ypos);

    if (xpos < 0) {
      x = - xpos;
      width = width + xpos;
      xpos = 0;
    }
    if (ypos < 0) {
      y = - ypos;
      height = height + ypos;
      ypos = 0;
    }

    if (xpos + width > gdk_screen_width ())
      width = gdk_screen_width () - xpos;

    if (ypos + height > gdk_screen_height ())
      height = gdk_screen_height () - ypos;
  }
  return gdk_pixbuf_get_from_drawable (NULL, window, NULL,
                                       x, y, 0, 0, width, height);
} /* </grab_pixbuf> */

/*
 * XGrabPointer and sleep specified time
 */
void
grab_pointer_sleep (unsigned int seconds)
{
  Display *display = GDK_WINDOW_XDISPLAY( GDK_ROOT_PARENT() );
  Cursor cursor = XCreateFontCursor(display, XC_watch);
  Window root = gdk_x11_get_default_root_xwindow();

  int status = XGrabPointer(display, root, FALSE,
                            ButtonPressMask|ButtonReleaseMask, GrabModeSync,
                            GrabModeAsync, root, cursor, CurrentTime);
  sleep (seconds);

  XUngrabPointer(display, CurrentTime);
} /* </grab_pointer_sleep> */

/*
 * capture method implementation
 */
GdkPixbuf *
capture(gint mode, gboolean decorations)
{
  Display *display = GDK_WINDOW_XDISPLAY( GDK_ROOT_PARENT() );
  Window root      = gdk_x11_get_default_root_xwindow();
  Window xwin      = None;

  XWindowAttributes xwa;
  XRectangle rect;

  GdkScreen *screen = gdk_screen_get_default();
  GdkPixbuf *pixbuf = NULL;


  /* initialize XRectangle with screen dimensions */
  rect.x      = 0;
  rect.y      = 0;
  rect.width  = gdk_screen_get_width(screen);
  rect.height = gdk_screen_get_height(screen);

  switch (mode) {
    case GRAB_SCREEN:
      pixbuf = grab_pixbuf(None, NULL);
      break;

    case GRAB_WINDOW:
      xwin = grab_window(display, root);
      
      if (XGetWindowAttributes(display, xwin, &xwa)) {
        int absx, absy;
        Window ign;

        rect.width = xwa.width;
        rect.height = xwa.height;

        if (decorations) {
          absx        -= xwa.border_width;
          absy        -= xwa.border_width;
          rect.width  += (2 * xwa.border_width);
          rect.height += (2 * xwa.border_width);
        }

        if (XTranslateCoordinates(display,xwin,root, 0,0, &absx,&absy,&ign)) {
          if (absx < 0) {
            rect.width += absx;
            absx = 0;
          }
          if (absy < 0) {
            rect.height += absy;
            absy = 0;
          }
          rect.x = absx;
          rect.y = absy;
        }
      }
      pixbuf = grab_pixbuf(xwin, &rect);
      break;

    case GRAB_REGION:
      grab_rectangle(display, root, &rect);
      pixbuf = grab_pixbuf(None, &rect);
      break;
  }
  return pixbuf;
} /* </capture> */
