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
#include "greenwindow.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <ctype.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xfuncs.h>
#include <X11/Xutil.h>
#include <X11/Xos.h>

#include <gtk/gtkmain.h>
#include <gdk/gdkx.h>

#define _NET_WM_MOVERESIZE_SIZE_TOPLEFT      0
#define _NET_WM_MOVERESIZE_SIZE_TOP          1
#define _NET_WM_MOVERESIZE_SIZE_TOPRIGHT     2
#define _NET_WM_MOVERESIZE_SIZE_RIGHT        3
#define _NET_WM_MOVERESIZE_SIZE_BOTTOMRIGHT  4
#define _NET_WM_MOVERESIZE_SIZE_BOTTOM       5
#define _NET_WM_MOVERESIZE_SIZE_BOTTOMLEFT   6
#define _NET_WM_MOVERESIZE_SIZE_LEFT         7
#define _NET_WM_MOVERESIZE_MOVE              8
#define _NET_WM_MOVERESIZE_SIZE_KEYBOARD     9
#define _NET_WM_MOVERESIZE_MOVE_KEYBOARD    10

#define _NET_WM_STATE_REMOVE        0    /* remove/unset property */
#define _NET_WM_STATE_ADD           1    /* add/set property */
#define _NET_WM_STATE_TOGGLE        2    /* toggle property  */

enum {					/* window state enum */
  ICON_CHANGED,
  NAME_CHANGED,
  STATE_CHANGED,
  GEOMETRY_CHANGED,
  WORKSPACE_CHANGED,
  LAST_SIGNAL
};

enum {					/* window property enum */
  NET_WM_ICON,
  NET_WM_NAME,
  NET_WM_STATE,
  NET_WM_DESKTOP,
  LAST_PROPERTY
};

struct _GreenWindowPrivate
{
  Window xid;			/* Xlib Window */

  XWindowState  state;		/* window state */
  GdkRectangle  geometry;	/* window geometry */

  const gchar  *name;		/* window name */
  const gchar  *iconame;	/* window iconame */
  const gchar  *wmclass;

  Green *green;			/* Green object instance */
  int  desktop;			/* window workspace number */

  bool	update[LAST_PROPERTY];	/* window property enum */
  guint agent;			/* update handler */
};

static gpointer parent_class_;		/* parent class instance */
static guint signals_[LAST_SIGNAL] = { 0 };


/*
* green_window_update_icon
* green_window_update_name
* green_window_update_state
* green_window_update_workspace
*/
static void
green_window_update_icon(GreenWindow *window)
{
  if (window->priv->update[NET_WM_ICON]) {
    GreenWindowPrivate *priv = window->priv;
    const gchar *name = get_window_icon_name (priv->xid);

    priv->update[NET_WM_ICON] = FALSE;

    if (priv->iconame && name && g_utf8_collate (priv->iconame, name)) {
      priv->iconame = name;

      g_signal_emit (G_OBJECT (window),
                     signals_[ICON_CHANGED],
                     0);
    }
  }
} /* </green_window_update_icon> */

static void
green_window_update_name(GreenWindow *window)
{
  if (window->priv->update[NET_WM_NAME]) {
    GreenWindowPrivate *priv = window->priv;
    const gchar *name = get_window_name (priv->xid);

    priv->update[NET_WM_NAME] = FALSE;

    if (priv->name && name && g_utf8_collate (priv->name, name)) {
      priv->name = name;

      g_signal_emit (G_OBJECT (window),
                     signals_[NAME_CHANGED],
                     0);
    }
  }
} /* </green_window_update_name> */

static void
green_window_update_state(GreenWindow *window)
{
  if (window->priv->update[NET_WM_STATE]) {
    GreenWindowPrivate *priv = window->priv;
    XWindowState state;

    priv->update[NET_WM_STATE] = FALSE;
    get_window_state (priv->xid, &state);

    if (memcmp(&priv->state, &state, sizeof (XWindowState)) != 0) {
      memcpy(&priv->state, &state, sizeof (XWindowState));

      g_signal_emit (G_OBJECT (window),
                     signals_[STATE_CHANGED],
                     0);
    }
  }
} /* </green_window_update_state> */

static void
green_window_update_workspace(GreenWindow *window)
{
  if (window->priv->update[NET_WM_DESKTOP]) {
    GreenWindowPrivate *priv = window->priv;
    int desktop = priv->desktop;	/* see if workspace changed */

    priv->update[NET_WM_DESKTOP] = FALSE;
    priv->desktop = get_window_desktop (priv->xid);

    if (desktop != priv->desktop)
      g_signal_emit (G_OBJECT (window),
                     signals_[WORKSPACE_CHANGED],
                     0);
  }
} /* </green_window_update_workspace> */

/*
* green_window_update
*/
static void
green_window_update(GreenWindow *window)
{
  green_window_update_icon (window);
  green_window_update_name (window);
  green_window_update_state (window);
  green_window_update_workspace (window);
} /* </green_window_update> */

/*
* green_window_idle_act
* green_window_idle_agent
* green_window_idle_cancel
*/
static gboolean
green_window_idle_act(GreenWindow *window)
{
  window->priv->agent = 0;	/* reentrancy guard */
  green_window_update (window);
  return FALSE;
} /* </green_window_idle_act> */

static void
green_window_idle_agent(GreenWindow *window)
{
  if (window->priv->agent == 0)
    window->priv->agent = g_idle_add ((GSourceFunc)green_window_idle_act,
                                      window);
} /* </green_window_idle_agent> */

static void
green_window_idle_cancel(GreenWindow *window)
{
  if (window->priv->agent != 0) {
    g_source_remove (window->priv->agent);
    window->priv->agent = 0;
  }
} /* </green_window_idle_cancel> */

/*
* GreenWindow and GreenWindowClass construction
*/
static void
green_window_init(GreenWindow *window)
{
  window->priv = g_new0 (GreenWindowPrivate, 1);
} /* </green_window_init> */

static void
green_window_finalize(GObject *object)
{
  GreenWindow *window = GREEN_WINDOW (object);

  green_window_idle_cancel (window);	/* inert g_idle_add() */

  g_free (window->priv);
  window->priv = NULL;

  G_OBJECT_CLASS (parent_class_)->finalize (object);
} /* <green_window_finalize> */

static void
green_window_class_init(GreenWindowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  parent_class_ = g_type_class_peek_parent (klass);

  object_class->finalize = green_window_finalize;

  signals_[ICON_CHANGED] =
    g_signal_new ("icon_changed",
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GreenWindowClass, icon_changed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  signals_[NAME_CHANGED] =
    g_signal_new ("name_changed",
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GreenWindowClass, name_changed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  signals_[STATE_CHANGED] =
    g_signal_new ("state_changed",
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GreenWindowClass, state_changed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  signals_[GEOMETRY_CHANGED] =
    g_signal_new ("geometry_changed",
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GreenWindowClass, geometry_changed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  signals_[WORKSPACE_CHANGED] =
    g_signal_new ("workspace_changed",
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GreenWindowClass, workspace_changed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
} /* </green_window_class_init> */

/*
 * Public methods
 */
GType
green_window_get_type(void)
{
  static GType object_type = 0;

  g_type_init ();			/* initialize the type system */

  if (!object_type)
    {
      const GTypeInfo object_info =
      {
        sizeof (GreenWindowClass),
        (GBaseInitFunc) NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) green_window_class_init,
        NULL,				/* class_finalize */
        NULL,				/* class_data */
        sizeof (GreenWindow),
        0,				/* n_preallocs */
        (GInstanceInitFunc) green_window_init,
        NULL				/* value_table */
      };

      object_type = g_type_register_static (G_TYPE_OBJECT, "GreenWindow",
                                            &object_info, 0);
    }

  return object_type;
} /* </green_get_type> */

/*
 * green_window_property_notify
 */
void
green_window_property_notify(GreenWindow *window, XEvent *xevent)
{
  Atom atom = xevent->xproperty.atom;
  bool *update_ = window->priv->update;

  if (atom == get_atom_property ("_NET_WM_ICON")) {
    update_[NET_WM_ICON] = true;
    green_window_idle_agent (window);
  }
  else if (atom == XA_WM_NAME ||
           atom == get_atom_property ("_NET_WM_NAME") ||
           atom == get_atom_property ("_NET_WM_VISIBLE_NAME")) {
    update_[NET_WM_NAME] = true;
    green_window_idle_agent (window);
  }
  else if (atom == get_atom_property ("_NET_WM_STATE")) {
    update_[NET_WM_STATE] = true;
    green_window_idle_agent (window);
  }
  else if (atom == get_atom_property ("_NET_WM_DESKTOP")) {
    update_[NET_WM_DESKTOP] = true;
    green_window_idle_agent (window);
  }
} /* </green_window_property_notify> */

/*
 * green_window_configure_notify
 */
void
green_window_configure_notify(GreenWindow *window, XEvent *xevent)
{
  GreenWindowPrivate *priv = window->priv;
  GdkRectangle geometry;

  get_window_geometry (priv->xid, &geometry);

  if (memcmp(&priv->geometry, &geometry, sizeof (GdkRectangle)) != 0) {
    memcpy(&priv->geometry, &geometry, sizeof (GdkRectangle));

    g_signal_emit (G_OBJECT (window),
                   signals_[GEOMETRY_CHANGED],
                   0);
  }
} /* </green_window_configure_notify> */

/*
 * green_window_activate
 * green_window_change_workspace
 * green_window_change_state
 */
void
green_window_activate(GreenWindow *window, Time stamp)
{
  Screen *screen = green_get_screen (window->priv->green);
  Window xwindow = green_window_get_xid (window);

  XEvent xev;			/* carefully populated XEvent structure */

  xev.xclient.type = ClientMessage;
  xev.xclient.serial = 0;
  xev.xclient.send_event = True;
  xev.xclient.display = gdk_display;
  xev.xclient.window = xwindow;
  xev.xclient.message_type = get_atom_property ("_NET_ACTIVE_WINDOW");
  xev.xclient.format = 32;

  xev.xclient.data.l[0] = 0;
  xev.xclient.data.l[1] = stamp;
  xev.xclient.data.l[2] = 0;
  xev.xclient.data.l[3] = 0;
  xev.xclient.data.l[4] = 0;

  XSendEvent (gdk_display,
              RootWindowOfScreen (screen),
              False,
              SubstructureRedirectMask | SubstructureNotifyMask,
              &xev);
} /* </green_window_activate> */

void
green_window_change_workspace(GreenWindow *window, int desktop)
{
  Screen *screen = green_get_screen (window->priv->green);
  Window xwindow = green_window_get_xid (window);

  XEvent xev;			/* carefully populated XEvent structure */

  xev.xclient.type = ClientMessage;
  xev.xclient.serial = 0;
  xev.xclient.send_event = True;
  xev.xclient.display = gdk_display;
  xev.xclient.window = xwindow;
  xev.xclient.message_type = get_atom_property ("_NET_WM_DESKTOP");
  xev.xclient.format = 32;

  xev.xclient.data.l[0] = desktop;
  xev.xclient.data.l[1] = 0;
  xev.xclient.data.l[2] = 0;
  xev.xclient.data.l[3] = 0;
  xev.xclient.data.l[4] = 0;

  XSendEvent (gdk_display,
              RootWindowOfScreen (screen),
              False,
              SubstructureRedirectMask | SubstructureNotifyMask,
              &xev);
} /* </green_window_change_workspace> */

void
green_window_change_state(GreenWindow *window, bool enable, const char *prop)
{
  Screen *screen = green_get_screen (window->priv->green);
  Window xwindow = green_window_get_xid (window);

  XEvent xev;			/* carefully populated XEvent structure */

  xev.xclient.type = ClientMessage;
  xev.xclient.serial = 0;
  xev.xclient.send_event = True;
  xev.xclient.display = gdk_display;
  xev.xclient.window = xwindow;
  xev.xclient.message_type = get_atom_property ("_NET_WM_STATE");
  xev.xclient.format = 32;

  xev.xclient.data.l[0] = enable ? _NET_WM_STATE_ADD : _NET_WM_STATE_REMOVE;
  xev.xclient.data.l[1] = get_atom_property (prop);
  xev.xclient.data.l[2] = 0;
  xev.xclient.data.l[3] = 0;
  xev.xclient.data.l[4] = 0;

  XSendEvent (gdk_display,
              RootWindowOfScreen (screen),
              False,
              SubstructureRedirectMask | SubstructureNotifyMask,
              &xev);
} /* </green_window_change_state> */

/*
 * green_window_close
 * green_window_minimize
 * green_window_maximize
 * green_window_move
 * green_window_resize
 * green_window_restore
 */
void
green_window_close(GreenWindow *window)
{
  Screen *screen = green_get_screen (window->priv->green);
  Window xwindow = green_window_get_xid (window);
  vdebug(2, "%s xid => 0x%x\n", __func__, xwindow);

  XEvent xev;			/* carefully populated XEvent structure */

  xev.xclient.type = ClientMessage;
  xev.xclient.serial = 0;
  xev.xclient.send_event = True;
  xev.xclient.display = gdk_display;
  xev.xclient.window = xwindow;
  xev.xclient.message_type = get_atom_property ("_NET_CLOSE_WINDOW");
  xev.xclient.format = 32;
  xev.xclient.data.l[0] = gtk_get_current_event_time ();
  xev.xclient.data.l[1] = 0;
  xev.xclient.data.l[2] = 0;
  xev.xclient.data.l[3] = 0;
  xev.xclient.data.l[4] = 0;

  XSendEvent (gdk_display,
              RootWindowOfScreen (screen),
              False,
              SubstructureRedirectMask | SubstructureNotifyMask,
              &xev);
} /* </green_window_close> */

void
green_window_minimize(GreenWindow *window)
{
  GreenWindowPrivate *priv = window->priv;

  if (priv->state.hidden == FALSE) {
    Screen *screen = green_get_screen (priv->green);
    int screenumber = XScreenNumberOfScreen (screen);

    gdk_error_trap_push ();
    XIconifyWindow (gdk_display, priv->xid, screenumber);
    _x_error_trap_pop ("green_window_minimize(xid => 0x%x)\n", priv->xid);
  }
} /* </green_window_minimize> */

void
green_window_maximize(GreenWindow *window)
{
  GreenWindowPrivate *priv = window->priv;
  GdkWindow *gdkwindow = gdk_window_foreign_new (priv->xid);

  gdk_window_maximize (gdkwindow);
  gdk_window_unref (gdkwindow);
} /* </green_window_maximize> */

void
green_window_move(GreenWindow *window)
{
  GreenWindowPrivate *priv = window->priv;
  Screen *screen = green_get_screen (priv->green);
  Window xwindow = priv->xid;

  XEvent xev;			/* carefully populated XEvent structure */

  xev.xclient.type = ClientMessage;
  xev.xclient.serial = 0;
  xev.xclient.send_event = True;
  xev.xclient.display = gdk_display;
  xev.xclient.window = xwindow;
  xev.xclient.message_type = get_atom_property ("_NET_WM_MOVERESIZE");
  xev.xclient.format = 32;
  xev.xclient.data.l[0] = priv->geometry.x;
  xev.xclient.data.l[1] = priv->geometry.y;
  xev.xclient.data.l[2] = _NET_WM_MOVERESIZE_MOVE_KEYBOARD;
  xev.xclient.data.l[3] = 0;
  xev.xclient.data.l[4] = gtk_get_current_event_time ();

  XSendEvent (gdk_display,
              RootWindowOfScreen (screen),
              False,
              SubstructureRedirectMask | SubstructureNotifyMask,
              &xev);
} /* </green_window_move> */

void
green_window_resize(GreenWindow *window)
{
  GreenWindowPrivate *priv = window->priv;
  Screen *screen = green_get_screen (priv->green);
  Window xwindow = priv->xid;

  XEvent xev;			/* carefully populated XEvent structure */

  xev.xclient.type = ClientMessage;
  xev.xclient.serial = 0;
  xev.xclient.send_event = True;
  xev.xclient.display = gdk_display;
  xev.xclient.window = xwindow;
  xev.xclient.message_type = get_atom_property ("_NET_WM_MOVERESIZE");
  xev.xclient.format = 32;
  xev.xclient.data.l[0] = priv->geometry.x + priv->geometry.width;
  xev.xclient.data.l[1] = priv->geometry.y + priv->geometry.height;
  xev.xclient.data.l[2] = _NET_WM_MOVERESIZE_SIZE_KEYBOARD;
  xev.xclient.data.l[3] = 0;
  xev.xclient.data.l[4] = 0;

  XSendEvent (gdk_display,
              RootWindowOfScreen (screen),
              False,
              SubstructureRedirectMask | SubstructureNotifyMask,
              &xev);
} /* </green_window_resize> */

void
green_window_restore(GreenWindow *window)
{
  GreenWindowPrivate *priv = window->priv;
  GdkWindow *gdkwindow = gdk_xid_table_lookup (priv->xid);

  if (priv->state.hidden) {
    /* We need special precautions, because GDK doesn't like XMapWindow()
     * called on its windows, need to use the GDK functions
     */
    if (gdkwindow)
      gdk_window_show (gdkwindow);
    else {
      gdk_error_trap_push ();
      XMapRaised (gdk_display, priv->xid);
      _x_error_trap_pop ("green_window_restore(xid => 0x%x)\n", priv->xid);
    }
  }

  if (priv->state.maximized_horz && priv->state.maximized_vert) {
    gdkwindow = gdk_window_foreign_new (priv->xid);
    gdk_window_unmaximize (gdkwindow);
    gdk_window_unref (gdkwindow);
  }
} /* </green_window_restore> */

/*
* green_window_get_xid
* green_window_get_name
* green_window_get_class
* green_window_get_icon
* green_window_get_geometry
* green_window_get_state
* green_window_get_green
* green_window_get_desktop
*/
Window
green_window_get_xid(GreenWindow *window)
{
  return (window) ? window->priv->xid : None;
} /* </green_window_get_xid> */

const gchar *
green_window_get_name(GreenWindow *window)
{
  return (window) ? window->priv->name : NULL;
} /* </green_window_get_name> */

const gchar *
green_window_get_class(GreenWindow *window)
{
  return (window) ? window->priv->wmclass : NULL;
} /* </green_window_get_class> */

GdkPixbuf *
green_window_get_icon(GreenWindow *window)
{
  return (window) ? get_window_icon (window->priv->xid) : NULL;
} /* </green_window_get_icon> */

GdkRectangle *
green_window_get_geometry(GreenWindow *window)
{
  return (window) ? &window->priv->geometry : NULL;
} /* </green_window_get_name> */

XWindowState *
green_window_get_state(GreenWindow *window)
{
  return (window) ? &window->priv->state : NULL;
} /* </green_window_get_state> */

Green *
green_window_get_green(GreenWindow *window)
{
  return (window) ? window->priv->green : NULL;
} /* </green_window_get_green> */

int
green_window_get_desktop(GreenWindow *window)
{
  return (window) ? window->priv->desktop : -1;
} /* </green_window_get_desktop> */

/*
 * green_window_new
 */
GreenWindow *
green_window_new(Window xid, Green *green)
{
  Display *display = green_get_display (green);
  GreenWindow *window = g_object_new (GREEN_TYPE_WINDOW, NULL);
  GreenWindowPrivate *priv = window->priv;
  int idx;

  for (idx = 0; idx < LAST_PROPERTY; idx++)
    priv->update[idx] = TRUE;

  priv->xid     = xid;
  priv->name    = get_window_name (xid);
  priv->iconame = get_window_icon_name (xid);
  priv->wmclass = get_window_class (xid);

  get_window_geometry (xid, &priv->geometry);
  get_window_state (xid, &priv->state);

  priv->desktop  = get_window_desktop (xid);
  priv->green    = green;

  green_select_input (xid, PropertyChangeMask | StructureNotifyMask);
  green_window_idle_agent (window);

  return window;
} /* </green_window_new> */

