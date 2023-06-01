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

#include "util.h"
#include "gould.h"
#include "green.h"
#include "greenwindow.h"

#include <string.h>
#include <stdlib.h>
#include <gdk/gdkx.h>


/* Private data structures. */
enum {
  ACTIVE_WINDOW_CHANGED,
  ACTIVE_WORKSPACE_CHANGED,
  BACKGROUND_CHANGED,
  CLASS_GROUP_OPENED,
  CLASS_GROUP_CLOSED,
  SHOWING_DESKTOP_CHANGED,
  VIEWPORTS_CHANGED,
  WINDOW_OPENED,
  WINDOW_CLOSED,
  WINDOW_STACKING_CHANGED,
  WORKSPACE_CREATED,
  WORKSPACE_DESTROYED,
  LAST_SIGNAL
};

enum {
  NET_ACTIVE_WINDOW,
  NET_CLIENT_LIST,
  NET_CURRENT_DESKTOP,
  NET_DESKTOP_GEOMETRY,
  NET_DESKTOP_LAYOUT,
  NET_DESKTOP_NAMES,
  NET_DESKTOP_VIEWPORT,
  NET_NUMBER_OF_DESKTOPS,
  XROOTPMAP_ID,
  LAST_PROPERTY
};

struct _GreenPrivate
{
  Window  xroot;		/* Xlib root window */
  Screen *screen;		/* Xlib Screen of workspaces */

  Pixmap backdrop;		/* Xlib background pixmap */

  GHashTable *reshash;		/* persistent Xlib resource class hash */
  GHashTable *winhash;		/* persistent GREEN_TYPE_WINDOW hash */
  GList *winlist;		/* open windows list */

  WindowFilter filter;		/* [optional] Xlib window list filter */

  Window *mapping;		/* _NET_CLIENT_LIST */
  Window *stacking;		/* _NET_CLIENT_LIST_STACKING */

  int clients, stacks;		/* respective number of elements */

  gboolean update[LAST_PROPERTY];
  guint agent;			/* update handler */
};

static gpointer parent_class_;	/* parent class instance */
static guint signals_[LAST_SIGNAL] = { 0 };

static Green **screen_ = NULL;	/* singleton GREEN array */

/*
 * green_hash_insert
 * green_hash_remove
 */
static void
green_hash_insert (Window xid, GreenWindow *window, Green *green)
{
  GreenPrivate *priv = green->priv;
  const gchar *wmclass = green_window_get_class (window);

  if (wmclass) {
    GList *list = g_hash_table_lookup (priv->reshash, wmclass);

    list = g_list_append (list, window);  /* either a new or existing list */
    g_hash_table_replace (priv->reshash, (gpointer)wmclass, list);
  }

  g_hash_table_insert (priv->winhash, GUINT_TO_POINTER (xid), window);
} /* </green_hash_insert> */

static void
green_hash_remove (Window xid, GreenWindow* window, Green *green)
{
  GreenPrivate *priv = green->priv;
  const gchar *wmclass = green_window_get_class (window);

  if (wmclass) {
    GList *list = g_hash_table_lookup (priv->reshash, wmclass);

    if (list) {
      list = g_list_remove (list, window);
      g_hash_table_replace (priv->reshash, (gpointer)wmclass, list);
    }
  }

  g_hash_table_remove (priv->winhash, GUINT_TO_POINTER (xid));
} /* </green_hash_remove> */

/*
 * green_hash_list - update hash table and return windows linked list
 */
GList *
green_hash_list (Green *green, Window *wins, int count)
{
  GreenPrivate *priv = green->priv;
  GreenWindow  *window;

  GList *list = NULL;
  int idx;

  for (idx = 0; idx < count; idx++) {
    gpointer key = GUINT_TO_POINTER (wins[idx]);
    window = g_hash_table_lookup (priv->winhash, key);

    if (window == NULL) {
      window = green_window_new (wins[idx], green);
      green_hash_insert (wins[idx], window, green);
    }

    list = g_list_append (list, key); 
  }

  return list;
} /* </green_hash_list> */

GList *
green_window_list (Green *green, WindowFilter filter, int desktop,
                   Window *wins, int count, const char *banner)
{
  GList *list = NULL;
  GreenPrivate *priv = green->priv;
  GreenWindow *window;
  int idx;

  for (idx = 0; idx < count; idx++) {
    Window xid = wins[idx];

    if (filter && (*filter)(xid, desktop)) /* filter by xid and desktop */
      continue;

    if ((window = g_hash_table_lookup (priv->winhash, GUINT_TO_POINTER (xid))))
      list = g_list_append (list, window);
  }

  return list;
} /* </green_window_list> */

/*
 * green_get_windows
 * green_get_windows_stacking
 * green_get_group_list
 */
GList *
green_get_windows (Green *green, WindowFilter filter, int desktop)
{
  GreenPrivate *priv = green->priv;

  return green_window_list (green, filter, desktop,
                            priv->mapping, priv->clients,
                            "green_get_windows");
} /* </green_get_windows> */

GList *
green_get_windows_stacking (Green *green, WindowFilter filter, int desktop)
{
  GreenPrivate *priv = green->priv;

  return green_window_list (green, filter, desktop,
                            priv->stacking, priv->stacks,
                            "green_get_windows_stacking");
} /* </green_get_windows_stacking> */

GList *
green_get_group_list (Green *green, gpointer object)
{
  GList *list = NULL;
  const gchar *wmclass = green_window_get_class ((GreenWindow *)object);

  if (wmclass)
    list = g_hash_table_lookup (green->priv->reshash, wmclass);

  return list;
} /* </green_get_group_list> */

/*
 * green_update_active_window
 * green_update_active_workspace
 * green_update_background_pixmap
 * green_update_client_list
 * green_update_workspace_count
 * green_update_workspace_names
 * green_update_workspace_layout
 * green_update_workspace_viewport
 */
static void
green_update_active_window (Green *green)
{
  static Window current = None;

  if (green->priv->update[NET_ACTIVE_WINDOW]) {
    Window active = get_active_window (green->priv->xroot);

    green->priv->update[NET_ACTIVE_WINDOW] = FALSE;

    if (active != current) {
      g_signal_emit (G_OBJECT (green),
                     signals_[ACTIVE_WINDOW_CHANGED],
                     0);

      current = active;
    }
  }
} /* </green_update_active_window> */

static void
green_update_active_workspace (Green *green)
{
  static int desktop = -1;

  if (green->priv->update[NET_CURRENT_DESKTOP]) {
    int active = green_get_active_workspace (green);

    green->priv->update[NET_CURRENT_DESKTOP] = FALSE;

    if (active != desktop) {
      g_signal_emit (G_OBJECT (green),
                     signals_[ACTIVE_WORKSPACE_CHANGED],
                     0);

      desktop = active;
    }
  }
} /* </green_update_active_workspace> */

static void
green_update_background_pixmap (Green *green)
{
  if (green->priv->update[XROOTPMAP_ID]) {
    green->priv->update[XROOTPMAP_ID] = FALSE;
  }
} /* </green_update_background_pixmap> */

static void
green_update_client_list (Green *green)
{
  if (green->priv->update[NET_CLIENT_LIST]) {
    GreenPrivate *priv = green->priv;
    GreenWindow  *window = NULL;
    GList *iter, *list = NULL;	/* windows stacking order, new list */

    Window *mapping;		/* windows initial mapping order */
    Window *stacking;		/* windows stacking order */

    gboolean stackeq;		/* check for stacking order changes */
    int clients, stacks;	/* respective number of elements */
    int idx;

    priv->update[NET_CLIENT_LIST] = FALSE;

    if (priv->filter) {
      mapping = get_window_filter_list (priv->filter, priv->xroot,
                                        get_atom_property ("_NET_CLIENT_LIST"),
                                        -1, &clients);

      stacking = get_window_filter_list (priv->filter, priv->xroot,
                               get_atom_property ("_NET_CLIENT_LIST_STACKING"),
                                         -1, &stacks);
    }
    else {
      mapping  = get_window_client_list (priv->xroot, -1, &clients);
      stacking = get_window_client_list_stacking (priv->xroot, -1, &stacks);
    }

    if (stacks != priv->stacks)
      stackeq = FALSE;
    else		/* same number of elements, memory compare */
      stackeq = memcmp(stacking, priv->stacking, sizeof(Window) * stacks) == 0;

    /* Client mapping and stacking orders mismatch, or no stacking changes. */
    if (!WindowArraysEqual (stacking, stacks, mapping, clients) || stackeq) {
      g_free (stacking);
      g_free (mapping);
      return;
    }

    /* Check for windows that were recently opened. */
    for (idx = 0; idx < clients; idx++) {
      gpointer key = GUINT_TO_POINTER (mapping[idx]);

      if ((window = g_hash_table_lookup (priv->winhash, key)) == NULL) {
        window = green_window_new (mapping[idx], green);
        green_hash_insert (mapping[idx], window, green);

        g_signal_emit (G_OBJECT (green),
                       signals_[WINDOW_OPENED],
                       0, window);
      }

      list = g_list_append (list, key);  /* add to new list in order */
    }

    /* Walk through the priv->winlist for closed windows.  */
    for (iter = priv->winlist; iter != NULL; iter = iter->next) {
      if (g_list_find (list, iter->data) == NULL) {   /* not in new list */
        window = g_hash_table_lookup (priv->winhash, iter->data);
        green_hash_remove ((Window)iter->data, window, green);

        g_signal_emit (G_OBJECT (green),
                       signals_[WINDOW_CLOSED],
                       0, window);
      }
    }

    if (stackeq == FALSE)	/* windows stacking order changed */
      g_signal_emit (G_OBJECT (green),
                     signals_[WINDOW_STACKING_CHANGED],
                     0);

    g_list_free (priv->winlist);
    priv->winlist = list;

    g_free (priv->stacking);
    priv->stacking = stacking;
    priv->stacks = stacks;

    g_free (priv->mapping);
    priv->mapping = mapping;
    priv->clients = clients;
  }
} /* </green_update_client_list> */

static void
green_update_workspace_count (Green *green)
{
  if (green->priv->update[NET_NUMBER_OF_DESKTOPS]) {
    green->priv->update[NET_NUMBER_OF_DESKTOPS] = FALSE;
  }
} /* </green_update_workspace_count> */

static void
green_update_workspace_layout (Green *green)
{
  if (green->priv->update[NET_DESKTOP_LAYOUT]) {
    green->priv->update[NET_DESKTOP_LAYOUT] = FALSE;
  }
} /* </green_update_workspace_layout> */

static void
green_update_workspace_names (Green *green)
{
  if (green->priv->update[NET_DESKTOP_NAMES]) {
    green->priv->update[NET_DESKTOP_NAMES] = FALSE;
  }
} /* </green_update_workspace_names> */

static void
green_update_workspace_viewport (Green *green)
{
  if (green->priv->update[NET_DESKTOP_VIEWPORT]) {
    green->priv->update[NET_DESKTOP_VIEWPORT] = FALSE;
  }
} /* </green_update_workspace_viewport> */

/*
 * green_update
 */
static void
green_update (Green *green)
{
  GreenPrivate *priv = green->priv;

  if (priv->agent) {
    g_source_remove (priv->agent);
    priv->agent = 0;
  }

  /* when the number of workspaces changes we need to update each space */
  if (priv->update[NET_NUMBER_OF_DESKTOPS]) {
    priv->update[NET_DESKTOP_VIEWPORT] = TRUE;
    priv->update[NET_DESKTOP_NAMES] = TRUE;
  }

  /* Big-picture state in order, first. */
  green_update_workspace_count (green);
  green_update_client_list (green);

  /* Next, note any smaller scale changes. */
  green_update_active_workspace (green);
  green_update_active_window (green);
  green_update_workspace_layout (green);
  green_update_workspace_names (green);
  green_update_workspace_viewport (green);

  green_update_background_pixmap (green);
} /* green_update */

/*
 * green_idle_act
 * green_idle_agent
 * green_idle_cancel
 */
static gboolean
green_idle_act (Green *green)
{
  green->priv->agent = 0;	/* reentrancy guard */
  green_update (green);

  return FALSE;
} /* </green_idle_act> */

static void
green_idle_agent (Green *green)
{
  if (green->priv->agent == 0)
    green->priv->agent = g_idle_add ((GSourceFunc)green_idle_act, green);
} /* </green_idle_agent> */

static void
green_idle_cancel (Green *green)
{
  if (green->priv->agent != 0) {
    g_source_remove (green->priv->agent);
    green->priv->agent = 0;
  }
} /* </green_idle_cancel> */

/*
 * green_property_notify
 */
static void
green_property_notify (Green *green, XEvent *xevent)
{
  Atom atom = xevent->xproperty.atom;
  gboolean *update_ = green->priv->update;

  if (atom == get_atom_property ("_NET_ACTIVE_WINDOW")) {
    update_[NET_ACTIVE_WINDOW] = TRUE;
    green_idle_agent (green);
  }
  else if (atom == get_atom_property ("_NET_CURRENT_DESKTOP")) {
    update_[NET_CURRENT_DESKTOP] = TRUE;
    green_idle_agent (green);
  }
  else if (atom == get_atom_property ("_NET_CLIENT_LIST") || 
           atom == get_atom_property ("_NET_CLIENT_LIST_STACKING")) {
    update_[NET_CLIENT_LIST] = TRUE;
    green_idle_agent (green);
  }
  else if (atom == get_atom_property ("_NET_DESKTOP_VIEWPORT")) {
    update_[NET_DESKTOP_VIEWPORT] = TRUE;
    green_idle_agent (green);
  }
  else if (atom == get_atom_property ("_NET_DESKTOP_GEOMETRY")) {
    update_[NET_DESKTOP_GEOMETRY] = TRUE;
    green_idle_agent (green);
  }
  else if (atom == get_atom_property ("_NET_NUMBER_OF_DESKTOPS")) {
    update_[NET_NUMBER_OF_DESKTOPS] = TRUE;
    green_idle_agent (green);
  }
  else if (atom == get_atom_property ("_NET_DESKTOP_LAYOUT")) {
    update_[NET_DESKTOP_LAYOUT] = TRUE;
    green_idle_agent (green);
  }
  else if (atom == get_atom_property ("_NET_DESKTOP_NAMES")) {
    update_[NET_DESKTOP_NAMES] = TRUE;
    green_idle_agent (green);
  }
  else if (atom == get_atom_property ("_XROOTPMAP_ID")) {
    update_[XROOTPMAP_ID] = TRUE;
    green_idle_agent (green);
  }
} /* </green_property_notify> */

/*
 * green_get_for_xroot
 */
Green *
green_get_for_xroot (gulong xroot)
{
  int count = ScreenCount (gdk_display), idx;

  for (idx = 0; idx < count; idx++)
    if (screen_[idx] && (screen_[idx]->priv->xroot == xroot))
      break;

  return ((idx < count) ? screen_[idx] : NULL);
} /* </green_get_for_xroot> */

/*
 * green_event_filter
 * green_event_filter_init
 */
static GdkFilterReturn
green_event_filter (XEvent *xevent, GdkEvent *event, gpointer data)
{
  Green *green;
  GreenWindow *window;

  switch (xevent->type) {
    case PropertyNotify:
      if ((green = green_get_for_xroot (xevent->xany.window)) != NULL)
        green_property_notify (green, xevent);
      else if ((window = green_find_window (xevent->xany.window)) != NULL)
        green_window_property_notify (window, xevent);
      break;

    case ConfigureNotify:
      if ((window = green_find_window (xevent->xany.window)) != NULL)
        green_window_configure_notify (window, xevent);
      break;

#if 0 /* CONSIDER */
    case SelectionClear:
      _green_desktop_layout_manager_process_event (xevent);
      break;
#endif

    case ClientMessage:
      break;
    }
  
  return GDK_FILTER_CONTINUE;
} /* </green_event_filter> */

static void
green_event_filter_init (gpointer data)
{
  static gboolean once = TRUE;

  if (once) {
    gdk_window_add_filter (NULL, (GdkFilterFunc)green_event_filter, data);
    once = FALSE;
  }
} /* </green_event_filter_init> */

/*
 * green_winhash_unref - only use immediately prior to g_hash_table_destroy()
 */
static void
green_winhash_unref (Window key, GdkWindow *window, Green *green)
{
  g_object_unref (window);	/* green_window_finalize() */
} /* </green_winhash_unref> */

/**
 * Green and GreenClass construction
 */
static void
green_init (Green *green)
{
  green->priv = g_new0 (GreenPrivate, 1);
} /* </green_init> */

static void
green_finalize (GObject *object)
{
  Green *green = GREEN (object);
  GreenPrivate *priv = green->priv;

  green_idle_cancel (green);	/* inert g_idle_add() */

  g_hash_table_destroy (priv->reshash);
  priv->reshash = NULL;

  g_hash_table_foreach (priv->winhash,
                        (GHFunc)green_winhash_unref, green);

  g_hash_table_destroy (priv->winhash);
  priv->winhash = NULL;

  g_list_free (priv->winlist);
  priv->winlist = NULL;

  g_free (green->priv);
  green->priv = NULL;

  G_OBJECT_CLASS (parent_class_)->finalize (object);
} /* <green_finalize> */

static void
green_class_init (GreenClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  parent_class_ = g_type_class_peek_parent (klass);

  object_class->finalize = green_finalize;

  signals_[ACTIVE_WINDOW_CHANGED] =
    g_signal_new ("active_window_changed",
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GreenClass, active_window_changed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  signals_[ACTIVE_WORKSPACE_CHANGED] =
    g_signal_new ("active_workspace_changed",
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GreenClass, active_workspace_changed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
                  //G_TYPE_NONE, 1, GREEN_TYPE_WORKSPACE);

  signals_[BACKGROUND_CHANGED] =
    g_signal_new ("background_changed",
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GreenClass, background_changed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  signals_[CLASS_GROUP_OPENED] =
    g_signal_new ("class_group_opened",
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GreenClass, class_group_opened),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 0);
                  //G_TYPE_NONE, 1, GREEN_TYPE_CLASS_GROUP);

  signals_[CLASS_GROUP_CLOSED] =
    g_signal_new ("class_group_closed",
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GreenClass, class_group_closed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 0);
                  //G_TYPE_NONE, 1, GREEN_TYPE_CLASS_GROUP);

  signals_[SHOWING_DESKTOP_CHANGED] =
    g_signal_new ("showing_desktop_changed",
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GreenClass, showing_desktop_changed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  signals_[VIEWPORTS_CHANGED] =
    g_signal_new ("viewports_changed",
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GreenClass, viewports_changed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  signals_[WINDOW_STACKING_CHANGED] =
    g_signal_new ("window_stacking_changed",
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GreenClass, window_stacking_changed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  signals_[WINDOW_OPENED] =
    g_signal_new ("window_opened",
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GreenClass, window_opened),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1, GREEN_TYPE_WINDOW);

  signals_[WINDOW_CLOSED] =
    g_signal_new ("window_closed",
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GreenClass, window_closed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1, GREEN_TYPE_WINDOW);

  signals_[WORKSPACE_CREATED] =
    g_signal_new ("workspace_created",
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GreenClass, workspace_created),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 0);
                  //G_TYPE_NONE, 1, GREEN_TYPE_WORKSPACE);

  signals_[WORKSPACE_DESTROYED] =
    g_signal_new ("workspace_destroyed",
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GreenClass, workspace_destroyed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 0);
                  //G_TYPE_NONE, 1, GREEN_TYPE_WORKSPACE);
} /* </green_class_init> */

/*
 * Public methods
 */
GType
green_get_type (void)
{
  static GType object_type = 0;

  g_type_init ();			/* initialize the type system */

  if (!object_type)
    {
      const GTypeInfo object_info =
      {
        sizeof (GreenClass),
        (GBaseInitFunc) NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) green_class_init,
        NULL,				/* class_finalize */
        NULL,				/* class_data */
        sizeof (Green),
        0,				/* n_preallocs */
        (GInstanceInitFunc) green_init,
        NULL				/* value_table */
      };

      object_type = g_type_register_static (G_TYPE_OBJECT, "Green",
                                            &object_info, 0);
    }

  return object_type;
} /* </green_get_type> */

/*
 * green_get_gdk_screen
 * green_get_screen
 */
GdkScreen *
green_get_gdk_screen (Green *green)
{
  int screenumber = XScreenNumberOfScreen (green->priv->screen);
  return gdk_display_get_screen (gdk_display_get_default(), screenumber);
} /* </green_get_gdk_screen> */

Screen *
green_get_screen (Green *green)
{
  return green->priv->screen;
} /* </green_get_screen> */

/*
 * green_get_root_window
 */
Window
green_get_root_window (Green *green)
{
  return green->priv->xroot;
} /* </green_get_root_window> */

/*
 * green_get_background_pixmap
 */
Pixmap
green_get_background_pixmap (Green *green)
{
  GreenPrivate *priv = green->priv;

#if 0 /* should not set workspace background to root window pixmap */
  if (priv->backdrop == None)
    priv->backdrop = get_window_pixmap (priv->xroot);
#endif

  return priv->backdrop;
} /* </green_get_background_pixmap> */

/*
 * green_get_active_workspace
 * green_get_window_workspace
 */
int
green_get_active_workspace (Green *green)
{
  return get_current_desktop (green->priv->screen);
} /* </green_get_active_workspace> */

/*
 * green_get_workspace_count
 * green_get_workspace_name
 */
int
green_get_workspace_count (Green *green)
{
  return get_xprop_cardinal (green->priv->xroot, "_NET_NUMBER_OF_DESKTOPS");
} /* </green_get_workspace_count> */

const gchar *
green_get_workspace_name (Green *green, int desktop)
{
  static gchar **name = NULL;

  if (name != NULL)		/* free previously allocated array */
    g_strfreev (name);

  name = get_desktop_names (green->priv->xroot);

  return (name) ? name[desktop] : NULL;
} /* </green_get_workspace_name> */

/*
 * green_change_workspace
 */
void
green_change_workspace (Green *green, int desktop, Time stamp)
{
  Screen *screen = green->priv->screen;
  Window xwindow = RootWindowOfScreen (screen);

  XEvent xev;			/* carefully populated XEvent structure */

  xev.xclient.type = ClientMessage;
  xev.xclient.serial = 0;
  xev.xclient.send_event = True;
  xev.xclient.display = gdk_display;
  xev.xclient.window = xwindow;
  xev.xclient.message_type = get_atom_property ("_NET_CURRENT_DESKTOP");
  xev.xclient.format = 32;

  xev.xclient.data.l[0] = desktop;
  xev.xclient.data.l[1] = stamp;
  xev.xclient.data.l[2] = 0;
  xev.xclient.data.l[3] = 0;
  xev.xclient.data.l[4] = 0;

  XSendEvent (gdk_display,
              xwindow,
              False,
              SubstructureRedirectMask | SubstructureNotifyMask,
              &xev);
} /* </green_change_workspace> */

/*
 * green_change_workspace_count
 */
void
green_change_workspace_count (Green *green, int count)
{
  Screen  *screen  = green_get_screen (green);
  Display *display = DisplayOfScreen (screen);

  XEvent xev;			/* carefully populated XEvent structure */

  xev.xclient.type = ClientMessage;
  xev.xclient.serial = 0;
  xev.xclient.window = green->priv->xroot;
  xev.xclient.send_event = True;
  xev.xclient.display = display;
  xev.xclient.message_type = get_atom_property ("_NET_NUMBER_OF_DESKTOPS");
  xev.xclient.format = 32;
  xev.xclient.data.l[0] = count;
 
  XSendEvent (display,
              green->priv->xroot,
              False,
              SubstructureRedirectMask | SubstructureNotifyMask,
              &xev);
} /* </green_change_workspace_count> */

/*
 * green_release_workspace_layout
 * green_request_workspace_layout
 */
void
green_release_workspace_layout (Green *green, int token)
{
  /* UNFINISHED */
} /* </green_release_workspace_layout> */

int
green_request_workspace_layout (Green *green, int token, int rows, int cols)
{
  /* UNFINISHED */
  return 0;
} /* </green_request_workspace_layout> */

/*
 * green_remove_window - draconian Xlib window removal
 */
void
green_remove_window (Green *green, Window xid)
{
  GreenPrivate *priv = green->priv;
  GreenWindow *window = g_hash_table_lookup (priv->winhash,
                                             GUINT_TO_POINTER (xid));

  if (window) {
    Window *mapping = g_new (Window, priv->clients - 1);
    Window *stacking = g_new (Window, priv->stacks - 1);
    int count, idx;

    for (count = 0, idx = 0; idx < priv->clients; idx++)
      if (priv->mapping[idx] != xid)
        mapping[count++] = priv->mapping[idx];

    g_free (priv->mapping);
    priv->mapping = mapping;
    priv->clients -= 1;

    for (count = 0, idx = 0; idx < priv->stacks; idx++)
      if (priv->stacking[idx] != xid)
        stacking[count++] = priv->stacking[idx];

    g_free (priv->stacking);
    priv->stacking = stacking;
    priv->stacks -= 1;

    green_hash_remove (xid, window, green);
  }
} /* </green_remove_window> */

/*
 * green_set_window_filter
 */
void
green_set_window_filter (Green *green, WindowFilter filter)
{
  GreenPrivate *priv = green->priv;

  if (filter && priv->filter != filter) {
    priv->filter = filter;

    priv->mapping = get_window_filter_list (priv->filter, priv->xroot,
                                        get_atom_property ("_NET_CLIENT_LIST"),
                                            -1, &priv->clients);

    priv->stacking = get_window_filter_list (priv->filter, priv->xroot,
                               get_atom_property ("_NET_CLIENT_LIST_STACKING"),
                                             -1, &priv->stacks);

    g_hash_table_remove_all (priv->reshash);
    g_hash_table_remove_all (priv->winhash);

    g_list_free (priv->winlist);
    priv->winlist = green_hash_list (green, priv->mapping, priv->clients);
  }
} /* </green_set_window_filter> */

/*
 * green_get_default
 * green_new
 */
Green *
green_get_default (void)	/* singleton GREEN object instance */
{
  return green_new (DefaultScreen (gdk_display));
} /* </green_get_default> */

Green *
green_filter_new (WindowFilter filter, int number)
{
  g_return_val_if_fail (number < ScreenCount (gdk_display), NULL);

  if (screen_ == NULL) {
    screen_ = g_new0 (Green*, ScreenCount (gdk_display));
    green_event_filter_init (screen_);
  }

  if (screen_[number] == NULL) {
    Green *object = g_object_new (GREEN_TYPE, NULL);
    GreenPrivate *priv = object->priv;
    int idx;

    for (idx = 0; idx < LAST_PROPERTY; idx++)
      priv->update[idx] = TRUE;

    priv->xroot    = RootWindow (gdk_display, number);
    priv->screen   = ScreenOfDisplay (gdk_display, number);;
    priv->filter   = filter;
    priv->backdrop = None;

    if (priv->filter) {
      priv->mapping = get_window_filter_list (priv->filter, priv->xroot,
                                        get_atom_property ("_NET_CLIENT_LIST"),
                                              -1, &priv->clients);

      priv->stacking = get_window_filter_list (priv->filter, priv->xroot,
                               get_atom_property ("_NET_CLIENT_LIST_STACKING"),
                                               -1, &priv->stacks);
    }
    else {
      priv->mapping  = get_window_client_list (priv->xroot, -1, &priv->clients);
      priv->stacking = get_window_client_list_stacking (priv->xroot, -1,
                                                        &priv->stacks);
    }

    priv->reshash  = g_hash_table_new (g_str_hash, g_str_equal);
    priv->winhash  = g_hash_table_new (g_direct_hash, g_direct_equal);
    priv->winlist  = green_hash_list (object, priv->mapping, priv->clients);

    green_select_input (priv->xroot, PropertyChangeMask);
    green_idle_agent (screen_[number] = object);
  }

  return screen_[number];
} /* </green_filter_new> */

Green *
green_new (int number)
{
  return green_filter_new (NULL, number);
} /* </green_new> */

/*
 * green_find_window - search all screen_[]->priv->winhash for Window
 */
gpointer
green_find_window (Window xid)
{
  GreenWindow *window = NULL;
  int idx, count = ScreenCount (gdk_display);

  for (idx = 0; idx < count; idx++)
    if (screen_[idx]) {
      window = g_hash_table_lookup (screen_[idx]->priv->winhash,
                                    GUINT_TO_POINTER (xid));
      if (window != NULL)
        break;
    }
    else
      break;

  return window;
} /* </green_find_window> */

/*
 * green_screen_width
 */
gint
green_screen_width (void)
{
  gint width = gdk_screen_width ();
  GdkScreen *screen = gdk_screen_get_default();

  if (screen && GDK_IS_SCREEN(screen)) {
    GdkRectangle rect;
    /* use resolution of monitor[0] in case there are multiple */
    gdk_screen_get_monitor_geometry(screen, 0, &rect);
    width = rect.width;
  }
  return width;
} /* </green_screen_width> */

/*
 * green_select_input
 */
void
green_select_input (Window xid, int mask)
{
  if (gdk_xid_table_lookup (xid)) {
    XWindowAttributes attrs;
    XGetWindowAttributes (gdk_display, xid, &attrs);
    mask |= attrs.your_event_mask;
  }

  XSelectInput (gdk_display, xid, mask);
} /* </green_select_input> */
