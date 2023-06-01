/**
 * Copyright (C) Generations Linux <bugs@softcraft.org>
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "gould.h"
#include "xutil.h"
#include "window.h"

#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <gdk/gdkx.h>

/* X11 data types */
Atom _UTF8_STRING;
Atom _XROOTPMAP_ID;

/* old WM spec */
Atom _WM_STATE;
Atom _WM_CLASS;
Atom _WM_DELETE_WINDOW;
Atom _WM_PROTOCOLS;

Atom _NET_WM_ALLOWED_ACTIONS;
Atom _NET_WM_ACTION_MOVE;
Atom _NET_WM_ACTION_RESIZE;
Atom _NET_WM_ACTION_MINIMIZE;
Atom _NET_WM_ACTION_SHADE;
Atom _NET_WM_ACTION_STICK;
Atom _NET_WM_ACTION_MAXIMIZE_HORZ;
Atom _NET_WM_ACTION_MAXIMIZE_VERT;
Atom _NET_WM_ACTION_FULLSCREEN;
Atom _NET_WM_ACTION_CHANGE_DESKTOP;
Atom _NET_WM_ACTION_CLOSE;

/* new NET spec */
Atom _NET_ACTIVE_WINDOW;
Atom _NET_CLIENT_LIST;
Atom _NET_CLIENT_LIST_STACKING;
Atom _NET_CURRENT_DESKTOP;
Atom _NET_DESKTOP_NAMES;
Atom _NET_NUMBER_OF_DESKTOPS;
Atom _NET_CLOSE_WINDOW;
Atom _NET_SUPPORTED;
Atom _NET_WM_DESKTOP;
Atom _NET_WM_ICON;
Atom _NET_WM_ICON_NAME;
Atom _NET_WM_MOVERESIZE;
Atom _NET_WM_NAME;
Atom _NET_WM_STATE;
Atom _NET_WM_STATE_ABOVE;
Atom _NET_WM_STATE_BELOW;
Atom _NET_WM_STATE_DEMANDS_ATTENTION;
Atom _NET_WM_STATE_FULLSCREEN;
Atom _NET_WM_STATE_HIDDEN;
Atom _NET_WM_STATE_MAXIMIZED_HORZ;
Atom _NET_WM_STATE_MAXIMIZED_VERT;
Atom _NET_WM_STATE_MODAL;
Atom _NET_WM_STATE_SHADED;
Atom _NET_WM_STATE_SKIP_PAGER;
Atom _NET_WM_STATE_SKIP_TASKBAR;
Atom _NET_WM_STATE_STICKY;
Atom _NET_WM_STRUT;
Atom _NET_WM_STRUT_PARTIAL;
Atom _NET_WM_VISIBLE_NAME;
Atom _NET_WM_WINDOW_TYPE;
Atom _NET_WM_WINDOW_TYPE_DESKTOP;
Atom _NET_WM_WINDOW_TYPE_DIALOG;
Atom _NET_WM_WINDOW_TYPE_DOCK;
Atom _NET_WM_WINDOW_TYPE_MENU;
Atom _NET_WM_WINDOW_TYPE_NORMAL;
Atom _NET_WM_WINDOW_TYPE_SPLASH;
Atom _NET_WM_WINDOW_TYPE_TOOLBAR;
Atom _NET_WM_WINDOW_TYPE_UTILITY;
Atom _NET_WORKAREA;

static gboolean initialize_ = TRUE;	/* initialize once */
static GHashTable *atoms_   = NULL;

/*
 * initialize_properties needs to be called once to initialize X properties
 * xintern is a wrapper for XInternAtom
 */
static inline Atom
xintern (Display *display, const char *name)
{
  Atom atom = XInternAtom(display, name, False);
  g_hash_table_insert (atoms_, GUINT_TO_POINTER (atom), (gpointer)name);
  return atom;
} /* </xintern> */

static void
initialize_properties (Display *dpy)
{
  if (atoms_ == NULL)			/* sanity check */
    atoms_ = g_hash_table_new (NULL, NULL);

  _WM_CLASS                     = xintern(dpy, "WM_CLASS");

  _NET_WM_ALLOWED_ACTIONS       = xintern(dpy, "_NET_WM_ALLOWED_ACTIONS");
  _NET_WM_ACTION_MOVE           = xintern(dpy, "_NET_WM_ACTION_MOVE");
  _NET_WM_ACTION_RESIZE         = xintern(dpy, "_NET_WM_ACTION_RESIZE");
  _NET_WM_ACTION_MINIMIZE       = xintern(dpy, "_NET_WM_ACTION_MINIMIZE");
  _NET_WM_ACTION_SHADE          = xintern(dpy, "_NET_WM_ACTION_SHADE");
  _NET_WM_ACTION_STICK          = xintern(dpy, "_NET_WM_ACTION_STICK");
  _NET_WM_ACTION_MAXIMIZE_HORZ  = xintern(dpy, "_NET_WM_ACTION_MAXIMIZE_HORZ");
  _NET_WM_ACTION_MAXIMIZE_VERT  = xintern(dpy, "_NET_WM_ACTION_MAXIMIZE_VERT");
  _NET_WM_ACTION_FULLSCREEN     = xintern(dpy, "_NET_WM_ACTION_FULLSCREEN");
  _NET_WM_ACTION_CHANGE_DESKTOP = xintern(dpy, "_NET_WM_ACTION_CHANGE_DESKTOP");
  _NET_WM_ACTION_CLOSE          = xintern(dpy, "_NET_WM_ACTION_CLOSE");

  _NET_ACTIVE_WINDOW            = xintern(dpy, "_NET_ACTIVE_WINDOW");
  _NET_CLIENT_LIST              = xintern(dpy, "_NET_CLIENT_LIST");
  _NET_CLIENT_LIST_STACKING     = xintern(dpy, "_NET_CLIENT_LIST_STACKING");
  _NET_CURRENT_DESKTOP          = xintern(dpy, "_NET_CURRENT_DESKTOP");
  _NET_DESKTOP_NAMES            = xintern(dpy, "_NET_DESKTOP_NAMES");
  _NET_NUMBER_OF_DESKTOPS       = xintern(dpy, "_NET_NUMBER_OF_DESKTOPS");
  _NET_CLOSE_WINDOW             = xintern(dpy, "_NET_CLOSE_WINDOW");
  _NET_SUPPORTED                = xintern(dpy, "_NET_SUPPORTED");

  _NET_WM_DESKTOP               = xintern(dpy, "_NET_WM_DESKTOP");
  _NET_WM_ICON                  = xintern(dpy, "_NET_WM_ICON");
  _NET_WM_ICON_NAME             = xintern(dpy, "_NET_WM_ICON_NAME");
  _NET_WM_MOVERESIZE            = xintern(dpy, "_NET_WM_MOVERESIZE");
  _NET_WM_NAME                  = xintern(dpy, "_NET_WM_NAME");

  _NET_WM_STATE                 = xintern(dpy, "_NET_WM_STATE");
  _NET_WM_STATE_ABOVE           = xintern(dpy, "_NET_WM_STATE_ABOVE");
  _NET_WM_STATE_BELOW           = xintern(dpy, "_NET_WM_STATE_BELOW");
  _NET_WM_STATE_FULLSCREEN      = xintern(dpy, "_NET_WM_STATE_FULLSCREEN");
  _NET_WM_STATE_HIDDEN          = xintern(dpy, "_NET_WM_STATE_HIDDEN");
  _NET_WM_STATE_MAXIMIZED_HORZ  = xintern(dpy, "_NET_WM_STATE_MAXIMIZED_HORZ");
  _NET_WM_STATE_MAXIMIZED_VERT  = xintern(dpy, "_NET_WM_STATE_MAXIMIZED_VERT");
  _NET_WM_STATE_MODAL           = xintern(dpy, "_NET_WM_STATE_MODAL");
  _NET_WM_STATE_SHADED          = xintern(dpy, "_NET_WM_STATE_SHADED");
  _NET_WM_STATE_SKIP_PAGER      = xintern(dpy, "_NET_WM_STATE_SKIP_PAGER");
  _NET_WM_STATE_SKIP_TASKBAR    = xintern(dpy, "_NET_WM_STATE_SKIP_TASKBAR");
  _NET_WM_STATE_STICKY          = xintern(dpy, "_NET_WM_STATE_STICKY");

  _NET_WM_STRUT                 = xintern(dpy, "_NET_WM_STRUT");
  _NET_WM_STRUT_PARTIAL         = xintern(dpy, "_NET_WM_STRUT_PARTIAL");
  _NET_WM_VISIBLE_NAME          = xintern(dpy, "_NET_WM_VISIBLE_NAME");

  _NET_WM_WINDOW_TYPE           = xintern(dpy, "_NET_WM_WINDOW_TYPE");
  _NET_WM_WINDOW_TYPE_DESKTOP   = xintern(dpy, "_NET_WM_WINDOW_TYPE_DESKTOP");
  _NET_WM_WINDOW_TYPE_DIALOG    = xintern(dpy, "_NET_WM_WINDOW_TYPE_DIALOG");
  _NET_WM_WINDOW_TYPE_DOCK      = xintern(dpy, "_NET_WM_WINDOW_TYPE_DOCK");
  _NET_WM_WINDOW_TYPE_MENU      = xintern(dpy, "_NET_WM_WINDOW_TYPE_MENU");
  _NET_WM_WINDOW_TYPE_NORMAL    = xintern(dpy, "_NET_WM_WINDOW_TYPE_NORMAL");
  _NET_WM_WINDOW_TYPE_SPLASH    = xintern(dpy, "_NET_WM_WINDOW_TYPE_SPLASH");
  _NET_WM_WINDOW_TYPE_TOOLBAR   = xintern(dpy, "_NET_WM_WINDOW_TYPE_TOOLBAR");
  _NET_WM_WINDOW_TYPE_UTILITY   = xintern(dpy, "_NET_WM_WINDOW_TYPE_UTILITY");
  _NET_WORKAREA                 = xintern(dpy, "_NET_WORKAREA");

  _UTF8_STRING                  = xintern(dpy, "UTF8_STRING");
  _XROOTPMAP_ID                 = xintern(dpy, "XROOTPMAP_ID");
} /* </resolve_atoms> */


/*
 * argbdata_to_pixdata
 * free_pixels
 */
static guchar *
argbdata_to_pixdata (gulong *data, int len)
{
  guchar *value = NULL;
  guchar *pix = g_new (guchar, len * 4);

  if (pix) {
    guint32 argb;
    guint32 rgba;
    int idx;

    value = pix;	/* start of pixels data */

    for (idx = 0; idx < len; idx++) {
      argb = data[idx];
      rgba = (argb << 8) | (argb >> 24);

      *pix = rgba >> 24;          ++pix;
      *pix = (rgba >> 16) & 0xff; ++pix;
      *pix = (rgba >> 8) & 0xff;  ++pix;
      *pix = rgba & 0xff;         ++pix;
    }
  }

  return value;
} /* </argbdata_to_pixdata> */

static void
free_pixels (guchar *pixels, gpointer data)
{
  g_free (pixels);
}

/*
 * (private) get_text_property_to_utf8
 */
static gchar *
get_text_property_to_utf8 (const XTextProperty *prop)
{
  char **list = NULL;
  int  count = gdk_text_property_to_utf8_list (
                                        gdk_x11_xatom_to_atom (prop->encoding),
                                        prop->format,
                                        prop->value,
                                        prop->nitems,
                                        &list);
  gchar *value = NULL;

  if (count > 0) {
    value = list[0];
    list[0] = g_strdup ("");	/* something to free */
    g_strfreev (list);
  }

  return value;
} /* </get_text_property_to_utf8> */

/*
 * (private) get_utf8_property
 */
static void *
get_utf8_property (Window xid, Atom atom)
{
  unsigned char *data;
  unsigned long length, after;
  gchar *value = NULL;
  int error, result;
  int format;
  Atom cast;

  if (initialize_) {
    initialize_properties (gdk_display);
    initialize_ = FALSE;
  }

  gdk_error_trap_push ();
  result = XGetWindowProperty (gdk_display, xid, atom, 0, G_MAXLONG, False,
                               _UTF8_STRING, &cast, &format, &length, &after,
                               (void*)&data);
  error = _x_error_trap_pop ("get_utf8_property(xid=>%d)", xid);

  if (error == Success && result == Success)
    if (cast == _UTF8_STRING && format == 8 && length != 0 && data) {
      value = g_strndup ((gchar *)data, length);
      XFree (data);
    }

  return value;
} /* </get_utf8_property> */

/*
 * latin1_to_utf8
 */
static gchar *
latin1_to_utf8 (const gchar *latin1)
{
  GString *string = g_string_new (NULL);
  const gchar *scan = latin1;

  for (; *scan; scan++)
    g_string_append_unichar (string, (gunichar)*scan);

  return g_string_free (string, FALSE);
} /* </latin1_to_utf8> */

/*
 * WindowArrayCompare
 */
static int
WindowArrayCompare (const void *a, const void *b)
{
  const Window *aw = a;
  const Window *bw = b;
 
  if (*aw < *bw)
    return -1;
  else if (*aw > *bw)
    return 1;
  else
    return 0;
} /* </WindowArrayCompare> */

/*
 * WindowArraysEqual
 * WindowGListEqual
 */
gboolean
WindowArraysEqual (Window *a, int a_len, Window *b, int b_len)
{
  gboolean result = FALSE;

  if (a_len == b_len && a_len != 0 && b_len != 0) {
    Window *a_tmp = g_new (Window, a_len);
    Window *b_tmp = g_new (Window, b_len);

    memcpy (a_tmp, a, a_len * sizeof (Window));
    memcpy (b_tmp, b, b_len * sizeof (Window));

    qsort (a_tmp, a_len, sizeof (Window), WindowArrayCompare);
    qsort (b_tmp, b_len, sizeof (Window), WindowArrayCompare);

    result = memcmp (a_tmp, b_tmp, sizeof (Window) * a_len) == 0;

    g_free (a_tmp);
    g_free (b_tmp);
  }

  return result;
} /* </WindowArraysEqual> */

gboolean
WindowGListEqual (GList *alist, GList *blist)
{
  gboolean result = FALSE;

  int alen = g_list_length (alist);
  int blen = g_list_length (blist);

  if (alen == blen) {
    Window *awins = g_new (Window, alen);
    Window *bwins = g_new (Window, blen);

    GList *iter;
    int    idx;

    for (idx = 0, iter = alist; iter; iter = iter->next)
      awins[idx++] = (Window)iter->data;

    for (idx = 0, iter = blist; iter; iter = iter->next)
      bwins[idx++] = (Window)iter->data;

    result = memcmp (awins, bwins, sizeof (Window) * alen) == 0;

    g_free (awins);
    g_free (bwins);
  }

  return result;
} /* </WindowGListEqual> */

/*
 * WindowDesktopFilter
 */
gboolean
WindowDesktopFilter (Window xid, int desktop)
{
  gboolean result = FALSE;	     /* TRUE => reject */
  int space = get_window_desktop (xid);

  if (space >= 0 && desktop >= 0 && desktop != space)
    result = TRUE;
  else {
    XWindowState xws;
    XWindowType  xwt;

    if (get_window_state (xid, &xws) && (xws.skip_pager || xws.skip_taskbar))
      result = TRUE;

    if (get_window_type (xid, &xwt) && (xwt.desktop || xwt.dock || xwt.splash))
      result = TRUE;
  }

  return result;
} /* </WindowDesktopFilter> */

gboolean
WindowPagerFilter (Window xid, int desktop)
{
  gboolean result = FALSE;	     /* TRUE => reject */
  int space = get_window_desktop (xid);

  if (space >= 0 && desktop >= 0 && desktop != space)
    result = TRUE;
  else {
    XWindowState xws;
    XWindowType  xwt;

    if (get_window_state (xid, &xws) && xws.skip_pager)
      result = TRUE;

    if (get_window_type (xid, &xwt) && (xwt.desktop || xwt.dock || xwt.splash))
      result = TRUE;
  }

  return result;
} /* </WindowPagerFilter> */

gboolean
WindowTaskbarFilter (Window xid, int desktop)
{
  gboolean result = FALSE;	     /* TRUE => reject */
  int space = get_window_desktop (xid);

  if (space >= 0 && desktop >= 0 && desktop != space)
    result = TRUE;
  else {
    XWindowState xws;
    XWindowType  xwt;

    if (get_window_state (xid, &xws) && xws.skip_taskbar)
      result = TRUE;

    if (get_window_type (xid, &xwt) && (xwt.desktop || xwt.dock || xwt.splash))
      result = TRUE;
  }

  return result;
} /* </WindowTaskbarFilter> */

/*
 * get_atom_property
 */
Atom
get_atom_property (const char *name)
{
  Atom     atom;
  gpointer prop;

  if (initialize_) {
    initialize_properties (gdk_display);
    initialize_ = FALSE;
  }

  prop = g_hash_table_lookup (atoms_, name);
  atom = (prop != NULL) ? (Atom)prop : xintern (gdk_display, name);

  return atom;
} /* </get_atom_property> */

Atom *
get_atom_list (Window xid, Atom atom, int *count)
{
  Atom cast, *data, *list = NULL;
  unsigned long length, after;
  int error, result;
  int format;

  gdk_error_trap_push ();
  result = XGetWindowProperty (gdk_display, xid, atom, 0, G_MAXLONG, False,
                               XA_ATOM, &cast, &format, &length, &after,
                               (void*)&data);
  error = _x_error_trap_pop ("get_atom_list(xid=>%d)", xid);

  if (error == Success && result == Success) {
    if (cast == XA_ATOM) {
      list = g_new (Atom, length);
      memcpy (list, data, sizeof (Atom) * length);
      *count = length;
    }

    XFree (data);
  }

  return list;
} /* </get_atom_list> */

/*
 * get_active_window
 */
Window
get_active_window (Window xroot)
{
  Atom cast, prop = get_atom_property ("_NET_ACTIVE_WINDOW");
  unsigned long after, length;
  Window value = None;
  Window *data;

  int error, result;
  int format;
  
  gdk_error_trap_push ();
  result = XGetWindowProperty (gdk_display, xroot, prop, 0, G_MAXLONG,
                               False, XA_WINDOW, &cast, &format, &length,
                               &after, (void*)&data);
  error = _x_error_trap_pop ("get_active_window(xroot=%d)", xroot);

  if (error == Success && result == Success) {
    if (cast == XA_WINDOW)
      value = *data;

    XFree (data);
  }

  return value;
} /* </get_active_window> */

/*
 * get_window_client_list
 * get_window_client_list_stacking
 * get_window_list
 */
Window *
get_window_client_list (Window xroot, int desktop, int *count)
{
  Atom atom = get_atom_property ("_NET_CLIENT_LIST");

  /* The windows array is from "_NET_CLIENT_LIST" property value. */
  return get_window_list (xroot, atom, desktop, count);
} /* </get_window_client_list> */

Window *
get_window_client_list_stacking (Window xroot, int desktop, int *count)
{
  Atom atom = get_atom_property ("_NET_CLIENT_LIST_STACKING");

  /* The windows array is from "_NET_CLIENT_LIST_STACKING" property value. */
  return get_window_list (xroot, atom, desktop, count);
} /* </get_window_client_list_stacking> */

Window *
get_window_filter_list (WindowFilter filter,
                        Window xroot, Atom atom, int desktop, int *count)
{
  Window *clone, *wins =  get_window_list (xroot, atom, desktop, count);
  int idx, length = 0;

  for (idx = 0; idx < *count; idx++) {	/* determine new array length */
    if ((*filter)(wins[idx], -1))
      continue;

    ++length;
  }

  clone = g_new0 (Window, length);
  length = 0;

  for (idx = 0; idx < *count; idx++) {	/* populate new array */
    if ((*filter)(wins[idx], -1))
      continue;

    clone[length++] = wins[idx];
  }

  *count = length;
  g_free (wins);

  return clone;
} /* </get_window_filter_list> */

Window *
get_window_list (Window xroot, Atom atom, int desktop, int *count)
{
  Atom cast;
  Window *data;		/* XGetWindowProperty return value */
  Window *wins = NULL;

  unsigned long length, after;
  int error, result;
  int format;

  if (initialize_) {
    initialize_properties (gdk_display);
    initialize_ = FALSE;
  }

  gdk_error_trap_push ();
  result = XGetWindowProperty (gdk_display, xroot, atom, 0, G_MAXLONG,
                               False, XA_WINDOW, &cast, &format, &length,
                               &after, (void*)&data);
  error = _x_error_trap_pop ("get_window_list(xroot=%d)", xroot);

  if (error == Success && result == Success) {
    if (cast == XA_WINDOW) {
      wins = g_new0 (Window, length);
      memcpy (wins, data, sizeof (Window) * length);
      *count = length;
    }

    XFree (data);
  }

  return wins;
} /* </get_window_list> */

/*
 * get_current_desktop
 * get_window_desktop
 */
int
get_current_desktop (Screen *screen)
{
  Display *dpy = DisplayOfScreen(screen);
  return get_xprop_cardinal (DefaultRootWindow(dpy), "_NET_CURRENT_DESKTOP");
} /* </get_current_desktop> */

int
get_window_desktop (Window xid)
{
  return get_xprop_cardinal (xid, "_NET_WM_DESKTOP");
} /* </get_window_desktop> */

/*
 * get_desktop_names
 */
gchar **
get_desktop_names (Window xroot)
{
  unsigned char *data;
  unsigned long length, after;
  gchar **value = NULL;
  int error, result;
  int format;
  Atom cast;

  if (initialize_) {
    initialize_properties (gdk_display);
    initialize_ = FALSE;
  }

  gdk_error_trap_push ();
  result = XGetWindowProperty (gdk_display, xroot, _NET_DESKTOP_NAMES,
                               0, G_MAXLONG, False, _UTF8_STRING, &cast,
                               &format, &length, &after, &data);
  error = _x_error_trap_pop ("get_desktop_names(xroot=%d)", xroot);

  if (error == Success && result == Success)
    if (cast == _UTF8_STRING && format == 8 && length != 0 && data) {
      gchar *scan = (gchar *)data;
      int idx, count = 0;

      /* assertion: property is NULL separated */
      for (idx = 0; idx < length; idx++)
        if (data[idx] == '\0')
          ++count;

      if (data[length - 1] != '\0')
        ++count;

      value = g_new0 (gchar *, count + 1);	/* allocate value array */

      for (idx = 0; idx < count; idx++)
        if (g_utf8_validate (scan, -1, NULL)) {
          value[idx] = g_strdup (scan);
          scan = scan + strlen(scan) + 1;
        }
        else {
          g_strfreev (value);
          value = NULL;
          break;
        }

      XFree(data);
    }

  return value;
} /* </get_desktop_names> */

/*
 * get_window_pixmap
 */
Pixmap
get_window_pixmap (Window xid)
{
  Atom cast, prop = get_atom_property ("_XROOTPMAP_ID");
  unsigned long after, length;
  Pixmap pixmap = None;
  unsigned char *data;

  int error, result;
  int format;
  
  gdk_error_trap_push ();
  result = XGetWindowProperty (gdk_display, xid, prop, 0L, 1L,
                               False, XA_PIXMAP, &cast, &format, &length,
                               &after, (void*)&data);
  error = _x_error_trap_pop ("get_window_pixmap(xid=>%d)", xid);

  if (error == Success && result == Success) {
    if (cast == XA_PIXMAP && format == 32 && length == 1 && after == 0)
      pixmap = (Pixmap)(*(long *)data);

    XFree (data);
  }

  return pixmap;
} /* </get_window_pixmap> */

/*
 * get_window_class - obtain the given window application class group
 */
const gchar *
get_window_class (Window xid)
{
  XClassHint hint;
  const gchar *result = NULL;
  int error, status;

  gdk_error_trap_push ();
  status = XGetClassHint (gdk_display, xid, &hint);
  error = _x_error_trap_pop ("get_window_class(xid=>%d)", xid);

  if (error == Success && status) {
    if (hint.res_name) {
      XFree (hint.res_name);
    }

    if (hint.res_class) {
      result = latin1_to_utf8 (hint.res_class);
      XFree (hint.res_class);
    }
  }

  return result;
} /* </get_window_class> */

/*
 * get_window_geometry
 */
gboolean
get_window_geometry (Window xid, GdkRectangle *geometry)
{
  Display *display = gdk_display;
  XWindowAttributes xwa;
  Window ignore;

  int error, xpos, ypos;
  unsigned int xres, yres;

  gdk_error_trap_push ();

  if (XGetWindowAttributes (display, xid, &xwa)) {
    XTranslateCoordinates (display, xid, xwa.root,
                           -xwa.border_width, -xwa.border_width,
                           &xpos, &ypos, &ignore);
    xres = xwa.width;
    yres = xwa.height;
  }
  else {
    guint border, depth;

    if (!XGetGeometry (display, xid, &ignore,
                       &xpos, &ypos, &xres, &yres,
                       &border, &depth))
      xpos = ypos = xres = yres = 2;
  }

  error = _x_error_trap_pop ("get_window_geometry(xid=>%d)", xid);

  geometry->x = xpos;
  geometry->y = ypos;
  geometry->width = xres;
  geometry->height = yres;

  return TRUE;
} /* </get_window_geometry> */

/*
 * get_window_icon
 * get_window_icon_scaled
 * get_window_icon_name
 * get_window_name
 */
GdkPixbuf *
get_window_icon (Window xid)
{
  return get_window_icon_scaled (xid, -1, -1);
} /* </get_window_icon> */

GdkPixbuf *
get_window_icon_scaled (Window xid, int width, int height)
{
  int count;
  gulong *data;
  GdkPixbuf *value = NULL;

  if (initialize_) {
    initialize_properties (gdk_display);
    initialize_ = FALSE;
  }

  /* If possible retrieve the icon from the _NET_WM_ICON window property. */
  if ((data = get_xprop_atom (xid, _NET_WM_ICON, XA_CARDINAL, &count))) {
    if (count > 2*sizeof(guint32)) {
      int cols = data[0];
      int rows = data[1];
      guchar *pix = argbdata_to_pixdata (data + 2, cols * rows);

      if (pix) {
        GdkPixbuf *src = gdk_pixbuf_new_from_data (pix, GDK_COLORSPACE_RGB,TRUE,
                                                   8, cols, rows, cols * 4,
                                                   free_pixels, NULL);
        if (src && width > 0 && height > 0) {
          value = gdk_pixbuf_scale_simple (src, width, height,
                                           GDK_INTERP_BILINEAR);
          g_object_unref (src);
        }
        else {
          value = src;
        }
      }
    }

    XFree(data);
  }
  else {
    XWMHints *hints;

    gdk_error_trap_push ();
    hints = XGetWMHints (gdk_display, xid);
    _x_error_trap_pop ("get_window_icon_scaled(xid=>%d)", xid);

    /*
     * IconPixmapHint flag indicates that hints->icon_pixmap contains
     * valid data that is already in pixdata format, so we could process
     * it and turn it into a GTK image.
     */
    if (hints && (hints->flags & IconPixmapHint)) {
      GdkPixmap *pixmap = gdk_pixmap_foreign_new (hints->icon_pixmap);
      GdkColormap *colormap = colormap_from_pixmap (pixmap);
      GdkPixbuf *pixbuf = gdk_pixbuf_get_from_drawable (NULL, pixmap, colormap,
                                                        0, 0, 0, 0, -1, -1);

      if (pixbuf && width > 0 && height > 0) {
        value = gdk_pixbuf_scale_simple (pixbuf, width, height,
                                         GDK_INTERP_BILINEAR);
        g_object_unref (pixbuf);
      }
      else
        value = pixbuf;

      XFree (hints);
    }
  }

  return value;
} /* </get_window_icon_scaled> */

const gchar *
get_window_icon_name (Window xid)
{
  gchar *name;

  if (initialize_) {
    initialize_properties (gdk_display);
    initialize_ = FALSE;
  }

  if ((name = get_utf8_property (xid, _NET_WM_ICON_NAME)) == NULL)
    name = get_xprop_text (xid, XA_WM_ICON_NAME);

  return name;
} /* </get_window_icon_name> */

const gchar *
get_window_name (Window xid)
{
  gchar *name;

  if (initialize_) {
    initialize_properties (gdk_display);
    initialize_ = FALSE;
  }

  if ((name = get_utf8_property (xid, _NET_WM_NAME)) == NULL)
    name = get_xprop_text (xid, XA_WM_NAME);

  return name;
} /* </get_window_name> */


/*
 * get_window_state
 * get_window_type
 */
gboolean
get_window_state (Window xid, XWindowState *xws)
{
  Atom *state;		/* _NET_WM_STATE Atom list */
  int   mark;

  if (initialize_) {
    initialize_properties (gdk_display);
    initialize_ = FALSE;
  }

  /* [Re]initialize XWindowState *xws passed. */
  memset(xws, 0, sizeof(XWindowState));

  if ((state = get_atom_list (xid, _NET_WM_STATE, &mark))) {
    while (--mark >= 0)
      if (state[mark] == _NET_WM_STATE_MODAL)
        xws->modal = 1;
      else if (state[mark] == _NET_WM_STATE_HIDDEN)
        xws->hidden = 1;
      else if (state[mark] == _NET_WM_STATE_SHADED)
        xws->shaded = 1;
      else if (state[mark] == _NET_WM_STATE_STICKY)
        xws->sticky = 1;
      else if (state[mark] == _NET_WM_STATE_SKIP_TASKBAR)
        xws->skip_taskbar = 1;
      else if (state[mark] == _NET_WM_STATE_SKIP_PAGER)
        xws->skip_pager = 1;
      else if (state[mark] == _NET_WM_STATE_FULLSCREEN)
        xws->fullscreen = 1;
      else if (state[mark] == _NET_WM_STATE_MAXIMIZED_HORZ)
        xws->maximized_horz = 1;
      else if (state[mark] == _NET_WM_STATE_MAXIMIZED_VERT)
        xws->maximized_vert = 1;
      else if (state[mark] == _NET_WM_STATE_ABOVE)
        xws->above = 1;
      else if (state[mark] == _NET_WM_STATE_BELOW)
        xws->below = 1;
      else if (state[mark] == _NET_WM_STATE_DEMANDS_ATTENTION)
        xws->demands_attention = 1;

      g_free (state);
  }

  return (state) ? TRUE : FALSE;
} /* </get_window_state> */

gboolean
get_window_type (Window xid, XWindowType *xwt)
{
  Atom *type;
  int   mark;

  if (initialize_) {
    initialize_properties (gdk_display);
    initialize_ = FALSE;
  }

  if ((type = get_atom_list (xid, _NET_WM_WINDOW_TYPE, &mark))) {
    memset(xwt, 0, sizeof(XWindowType));

    while (--mark >= 0)
      if (type[mark] == _NET_WM_WINDOW_TYPE_DESKTOP)
        xwt->desktop = 1;
      else if (type[mark] == _NET_WM_WINDOW_TYPE_DIALOG)
        xwt->dialog = 1;
      else if (type[mark] == _NET_WM_WINDOW_TYPE_DOCK)
        xwt->dock = 1;
      else if (type[mark] == _NET_WM_WINDOW_TYPE_MENU)
        xwt->menu = 1;
      else if (type[mark] == _NET_WM_WINDOW_TYPE_NORMAL)
        xwt->normal = 1;
      else if (type[mark] == _NET_WM_WINDOW_TYPE_SPLASH)
        xwt->splash = 1;
      else if (type[mark] == _NET_WM_WINDOW_TYPE_TOOLBAR)
        xwt->toolbar = 1;
      else if (type[mark] == _NET_WM_WINDOW_TYPE_UTILITY)
        xwt->utility = 1;

    g_free (type);
  }

  return (type) ? TRUE : FALSE;
} /* </get_window_type> */

/*
 * get_xprop_atom - obtain the Atom value
 * get_xprop_name - obtain the Atom value by name
 * get_xprop_text - obtain the Atom text value
 */
void *
get_xprop_atom (Window xid, Atom prop, Atom type, int *count)
{
  unsigned char *data;
  unsigned char *value = NULL;
  unsigned long length, after;
  int error, result;
  int format;
  Atom cast;

  if (initialize_) {
    initialize_properties (gdk_display);
    initialize_ = FALSE;
  }

  gdk_error_trap_push ();
  result = XGetWindowProperty(gdk_display, xid, prop, 0, G_MAXLONG, False,
                              type, &cast, &format, &length, &after, &data);
  error = _x_error_trap_pop ("get_xprop_atom(xid=>%d)", xid);

  if (error == Success && result == Success) {
    value = data;

    if (count)
      *count = length;
  }

  return value;
} /* </get_xprop_atom> */

int
get_xprop_cardinal (Window xid, const char *name)
{
  int value = -1;
  unsigned char *data = get_xprop_name (xid, name);

  if (data) {
    value = *(unsigned long *)data;
    XFree(data);
  }

  return value;
} /* </get_xprop_cardinal> */

void *
get_xprop_name (Window xid, const char *name)
{
  unsigned char *data;
  unsigned char *value = NULL;
  unsigned long length, after;
  gpointer prop;

  int error, result;
  int format;

  Atom atom;		/* Atom property */
  Atom cast;		/* Atom type */

  if (initialize_) {
    initialize_properties (gdk_display);
    initialize_ = FALSE;
  }

  prop = g_hash_table_lookup (atoms_, name);
  atom = (prop != NULL) ? (Atom)prop : xintern (gdk_display, name);

  gdk_error_trap_push ();
  result = XGetWindowProperty (gdk_display, xid, atom, 0L, 1L, False,
                               AnyPropertyType, &cast, &format, &length,
                               &after, &data);
  error = _x_error_trap_pop ("get_xprop_name(xid=>%d)", xid);

  if (error == Success && result == Success)
    if ((cast == XA_CARDINAL) && (format == 32) && data)
      value = data;

  return value;
} /* </get_xprop_name> */

char *
get_xprop_text (Window xid, Atom atom)
{
  XTextProperty text;
  gchar *value = NULL;

  if (initialize_) {
    initialize_properties (gdk_display);
    initialize_ = FALSE;
  }

  gdk_error_trap_push ();
  text.nitems = 0;

  if (XGetTextProperty (gdk_display, xid, &text, atom)) {
    value = get_text_property_to_utf8 (&text);

    if (text.nitems > 0)
      XFree(text.value);
  }
  _x_error_trap_pop ("get_xprop_text(xid=>%d)", xid);

  return value;
} /* </get_xprop_text> */

/*
 * set_window_allowed_actions
 */
void
set_window_allowed_actions (Window xid, Atom *actions, int length)
{
  gdk_error_trap_push ();

  XChangeProperty (gdk_display, xid, _NET_WM_ALLOWED_ACTIONS, XA_ATOM, 32,
                   PropModeReplace, (unsigned char *)actions, length);

  _x_error_trap_pop (NULL);
} /* </set_window_allowed_actions> */
