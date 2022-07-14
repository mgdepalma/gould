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

#include "xutil.h"
#include "systray.h"

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <gtk/gtk.h>


/*
 * Systray private data structures.
*/
#define SYSTEM_TRAY_REQUEST_DOCK    0
#define SYSTEM_TRAY_BEGIN_MESSAGE   1
#define SYSTEM_TRAY_CANCEL_MESSAGE  2

#define SYSTEM_TRAY_ORIENTATION_HORZ 0
#define SYSTEM_TRAY_ORIENTATION_VERT 1

enum {
  PROP_START,
  PROP_ORIENTATION
};

enum
{
  TRAY_ICON_ADDED,
  TRAY_ICON_REMOVED,
  TRAY_MESSAGE_SENT,
  TRAY_MESSAGE_CANCELLED,
  TRAY_LOST_SELECTION,
  TRAY_MAX_SIGNAL
};

typedef struct
{
  Window window;

  long id;
  long len;
  long remaining_len;
  long timeout;

  char *str;
} PendingMessage;

static gpointer parent_class_;			/* parent class instance */
static guint signals_[TRAY_MAX_SIGNAL];		/* systemtray signals */

/* Method implementation prototypes forward declarations. */
static gboolean systray_manage (Systray *manager, GdkScreen *screen);

static void systray_unmanage (Systray *manager);

static void systray_set_orientation_property (Systray *manager);
static void systray_set_orientation (Systray *manager, GtkOrientation orien);

/*
 * systray_marshal_VOID:OBJECT,OBJECT
 * systray_marshal_VOID:OBJECT,STRING,LONG,LONG
 * systray_marshal_VOID:OBJECT,LONG
 */
void
systray_marshal_VOID__OBJECT_OBJECT (GClosure     *closure,
                                     GValue       *return_value,
                                     guint         n_param_values,
                                     const GValue *param_values,
                                     gpointer      invocation_hint,
                                     gpointer      data)
{
  typedef void (*GMarshalFunc_VOID__OBJECT_OBJECT) (gpointer data1,
                                                    gpointer arg_1,
                                                    gpointer arg_2,
                                                    gpointer data2);
  register GMarshalFunc_VOID__OBJECT_OBJECT callback;
  register GCClosure *cc = (GCClosure*) closure;
  register gpointer data1, data2;

  g_return_if_fail (n_param_values == 3);

  if (G_CCLOSURE_SWAP_DATA (closure)) {
    data1 = closure->data;
    data2 = g_value_peek_pointer (param_values + 0);
  }
  else {
    data1 = g_value_peek_pointer (param_values + 0);
    data2 = closure->data;
  }

  callback = (GMarshalFunc_VOID__OBJECT_OBJECT) (data ? data : cc->callback);
  callback (data1,
            g_value_get_object (param_values + 1),
            g_value_get_object (param_values + 2),
            data2);
} /* </systray_marshal_VOID__OBJECT_OBJECT> */

void
systray_marshal_VOID__OBJECT_STRING_LONG_LONG (GClosure     *closure,
                                            GValue       *return_value,
                                            guint         n_param_values,
                                            const GValue *param_values,
                                            gpointer      invocation_hint,
                                            gpointer      data)
{
  typedef void (*GMarshalFunc_VOID__OBJECT_STRING_LONG_LONG) (gpointer data1,
                                                              gpointer arg_1,
                                                              gpointer arg_2,
                                                              glong    arg_3,
                                                              glong    arg_4,
                                                              gpointer data2);
  register GMarshalFunc_VOID__OBJECT_STRING_LONG_LONG callback;
  register GCClosure *cc = (GCClosure*) closure;
  register gpointer data1, data2;

  g_return_if_fail (n_param_values == 5);

  if (G_CCLOSURE_SWAP_DATA (closure)) {
    data1 = closure->data;
    data2 = g_value_peek_pointer (param_values + 0);
  }
  else {
    data1 = g_value_peek_pointer (param_values + 0);
    data2 = closure->data;
  }

  callback = (GMarshalFunc_VOID__OBJECT_STRING_LONG_LONG) (data ? data : cc->callback);
  callback (data1,
            g_value_get_object (param_values + 1),
            (char *)g_value_get_string (param_values + 2),
            g_value_get_long (param_values + 3),
            g_value_get_long (param_values + 4),
            data2);
} /* </systray_marshal_VOID__OBJECT_STRING_LONG_LONG> */

void
systray_marshal_VOID__OBJECT_LONG (GClosure     *closure,
                                   GValue       *return_value,
                                   guint         n_param_values,
                                   const GValue *param_values,
                                   gpointer      invocation_hint,
                                   gpointer      data)
{
  typedef void (*GMarshalFunc_VOID__OBJECT_LONG) (gpointer data1,
                                                  gpointer arg_1,
                                                  glong    arg_2,
                                                  gpointer data2);
  register GMarshalFunc_VOID__OBJECT_LONG callback;
  register GCClosure *cc = (GCClosure*) closure;
  register gpointer data1, data2;

  g_return_if_fail (n_param_values == 3);

  if (G_CCLOSURE_SWAP_DATA (closure)) {
    data1 = closure->data;
    data2 = g_value_peek_pointer (param_values + 0);
  }
  else {
    data1 = g_value_peek_pointer (param_values + 0);
    data2 = closure->data;
  }

  callback = (GMarshalFunc_VOID__OBJECT_LONG) (data ? data : cc->callback);
  callback (data1,
            g_value_get_object (param_values + 1),
            g_value_get_long (param_values + 2),
            data2);
} /* </systray_marshal_VOID__OBJECT_LONG> */

/*
 * SYSTRAY implementation.
*/
G_DEFINE_TYPE (Systray, systray, G_TYPE_OBJECT)

static void
systray_init (Systray *manager)
{
  manager->invisible = NULL;
  manager->socketable = g_hash_table_new (NULL, NULL);
} /* </systray_init> */

static void
systray_finalize (GObject *object)
{
  Systray *manager;

  manager = SYSTRAY (object);
  systray_unmanage (manager);

  g_list_free (manager->messages);
  g_hash_table_destroy (manager->socketable);

  G_OBJECT_CLASS (parent_class_)->finalize (object);
} /* </systray_finalize> */

/*
 * systray_set_property
 * systray_get_property
 */
static void
systray_set_property (GObject      *object,
                      guint         prop_id,
                      const GValue *value,
                      GParamSpec   *pspec)
{
  Systray *manager = SYSTRAY (object);

  switch (prop_id) {
    case PROP_ORIENTATION:
      systray_set_orientation (manager, g_value_get_enum (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
} /* </systray_set_property> */

static void
systray_get_property (GObject    *object,
                      guint       prop_id,
                      GValue     *value,
                      GParamSpec *pspec)
{
  Systray *manager = SYSTRAY (object);

  switch (prop_id) {
    case PROP_ORIENTATION:
      g_value_set_enum (value, manager->orientation);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
} /* </systray_get_property> */

static void
systray_class_init (SystrayClass *klass)
{
  GObjectClass *instance = G_OBJECT_CLASS (klass);

  parent_class_ = g_type_class_peek_parent (klass);

  instance->finalize     = systray_finalize;
  instance->set_property = systray_set_property;
  instance->get_property = systray_get_property;

  g_object_class_install_property (
                                  instance,
                                  PROP_ORIENTATION,
                                  g_param_spec_enum ("orientation",
                                                     "orientation",
                                                     "orientation",
                                                     GTK_TYPE_ORIENTATION,
                                                     GTK_ORIENTATION_HORIZONTAL,
                                                     G_PARAM_READWRITE |
                                                     G_PARAM_CONSTRUCT |
                                                     G_PARAM_STATIC_NAME |
                                                     G_PARAM_STATIC_NICK |
                                                     G_PARAM_STATIC_BLURB)
                                  );

  signals_[TRAY_ICON_ADDED] =
    g_signal_new ("tray_icon_added",
          G_OBJECT_CLASS_TYPE (klass),
          G_SIGNAL_RUN_LAST,
          G_STRUCT_OFFSET (SystrayClass, tray_icon_added),
          NULL, NULL,
          g_cclosure_marshal_VOID__OBJECT,
          G_TYPE_NONE, 1,
          GTK_TYPE_SOCKET);

  signals_[TRAY_ICON_REMOVED] =
    g_signal_new ("tray_icon_removed",
          G_OBJECT_CLASS_TYPE (klass),
          G_SIGNAL_RUN_LAST,
          G_STRUCT_OFFSET (SystrayClass, tray_icon_removed),
          NULL, NULL,
          g_cclosure_marshal_VOID__OBJECT,
          G_TYPE_NONE, 1,
          GTK_TYPE_SOCKET);

  signals_[TRAY_MESSAGE_SENT] =
    g_signal_new ("message_sent",
          G_OBJECT_CLASS_TYPE (klass),
          G_SIGNAL_RUN_LAST,
          G_STRUCT_OFFSET (SystrayClass, message_sent),
          NULL, NULL,
          systray_marshal_VOID__OBJECT_STRING_LONG_LONG,
          G_TYPE_NONE, 4,
          GTK_TYPE_SOCKET,
          G_TYPE_STRING,
          G_TYPE_LONG,
          G_TYPE_LONG);

  signals_[TRAY_MESSAGE_CANCELLED] =
    g_signal_new ("message_cancelled",
          G_OBJECT_CLASS_TYPE (klass),
          G_SIGNAL_RUN_LAST,
          G_STRUCT_OFFSET (SystrayClass, message_cancelled),
          NULL, NULL,
          systray_marshal_VOID__OBJECT_LONG,
          G_TYPE_NONE, 2,
          GTK_TYPE_SOCKET,
          G_TYPE_LONG);

  signals_[TRAY_LOST_SELECTION] =
    g_signal_new ("lost_selection",
          G_OBJECT_CLASS_TYPE (klass),
          G_SIGNAL_RUN_LAST,
          G_STRUCT_OFFSET (SystrayClass, lost_selection),
          NULL, NULL,
          g_cclosure_marshal_VOID__VOID,
          G_TYPE_NONE, 0);
} /* </systray_class_init> */

/*
 * Method implementation internals.
*/
static inline void
pending_message_free (PendingMessage *message)
{
  g_free (message->str);
  g_free (message);
} /* </pending_message_free> */

/*
 * systray_plug_removed
 */
static gboolean
systray_plug_removed (GtkSocket *socket, Systray *manager)
{
  Window *window;
  window = g_object_get_data (G_OBJECT (socket), "systray-child-window");

  g_hash_table_remove (manager->socketable, GINT_TO_POINTER (*window));
  g_object_set_data (G_OBJECT (socket), "systray-child-window", NULL);

  g_signal_emit (manager, signals_[TRAY_ICON_REMOVED], 0, socket);
  vdebug (5, "system tray signal => ICON_REMOVED\n");

  return FALSE;		    /* return FALSE which destroys the socket */
} /* </systray_plug_removed> */

/*
 * systray_plug_added
 */
static void
systray_plug_added (Systray *manager, XClientMessageEvent *xevent)
{
  GtkRequisition req;
  GtkWidget     *socket;
  GHashTable    *hashtable = manager->socketable;
  Window        *window;

  if (g_hash_table_lookup (hashtable, GINT_TO_POINTER (xevent->data.l[2])))
    return;	/* we already got this notification earlier.. */

  socket = gtk_socket_new ();

  /* We need to set the child window here so that the client can call
   * _get functions in the signal handler
   */
  window = g_new (Window, 1);
  *window = xevent->data.l[2];

  g_object_set_data_full (G_OBJECT (socket),
                          "systray-child-window",
                          window, g_free);

  g_signal_emit (manager, signals_[TRAY_ICON_ADDED], 0, socket);
  vdebug (5, "systray signal => ICON_ADDED\n");

  /* Add the socket only if it's been attached */
  if (GTK_IS_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (socket)))) {
    g_signal_connect (socket, "plug_removed",
                      G_CALLBACK (systray_plug_removed), manager);

    gtk_socket_add_id (GTK_SOCKET (socket), *window);
    g_hash_table_insert (hashtable, GINT_TO_POINTER(*window), socket);

    /* Make sure the icons have a meaningfull size ... */
    req.width = req.height = 1;
    gtk_widget_size_request (socket, &req);
    gtk_widget_show (socket);
  }
  else {
    gtk_widget_destroy (socket);
  }
} /* </systray_plug_added> */

/*
 * systray_handle_message_data
 */
static GdkFilterReturn
systray_handle_message_data (Systray *manager, XClientMessageEvent *xevent)
{
  GList *iter;
  int    len;

  /* See if we can find the pending message in the list. */
  for (iter = manager->messages; iter; iter = iter->next) {
    PendingMessage *msg = iter->data;

    if (xevent->window == msg->window) {	/* append the message */
      len = MIN (msg->remaining_len, 20);

      memcpy ((msg->str + msg->len - msg->remaining_len), &xevent->data, len);
      msg->remaining_len -= len;

      if (msg->remaining_len == 0) {
        GtkSocket *socket; 
        socket = g_hash_table_lookup (manager->socketable,
                                      GINT_TO_POINTER (msg->window));

        if (socket)
          g_signal_emit (manager, signals_[TRAY_MESSAGE_SENT], 0,
                         socket, msg->str, msg->id, msg->timeout);

        manager->messages = g_list_remove_link (manager->messages, iter);
        pending_message_free (msg);
      }
      break;
    }
  }

  return GDK_FILTER_REMOVE;
} /* </systray_handle_message_data> */

static void
systray_handle_begin_message (Systray *manager, XClientMessageEvent *xevent)
{
  GList          *iter;
  PendingMessage *msg;

  /* See if the same message is already in the queue and remove it if so. */
  for (iter = manager->messages; iter; iter = iter->next) {
    PendingMessage *msg = iter->data;

    if (xevent->window == msg->window && xevent->data.l[4] == msg->id) {
      manager->messages = g_list_remove_link (manager->messages, iter);
      pending_message_free (msg);	/* we we found it, now remove it */
      break;
    }
  }

  /* Add the new message to the queue */
  msg = g_new0 (PendingMessage, 1);
  msg->window = xevent->window;
  msg->timeout = xevent->data.l[2];
  msg->len = xevent->data.l[3];
  msg->id = xevent->data.l[4];
  msg->remaining_len = msg->len;
  msg->str = g_malloc (msg->len + 1);
  msg->str[msg->len] = '\0';
  manager->messages = g_list_prepend (manager->messages, msg);
} /* </systray_handle_begin_message> */

/*
 * systray_handle_cancel_message
 */
static void
systray_handle_cancel_message (Systray *manager, XClientMessageEvent *xevent)
{
  GtkSocket *socket;

  socket = g_hash_table_lookup (manager->socketable,
                                GINT_TO_POINTER (xevent->window));
  if (socket) {
    g_signal_emit (manager, signals_[TRAY_MESSAGE_CANCELLED], 0,
                   socket, xevent->data.l[2]);
  }
} /* </systray_handle_cancel_message> */

/*
 * systray_handle_event
 */
static GdkFilterReturn
systray_handle_event (Systray *manager, XClientMessageEvent *xevent)
{
  GdkFilterReturn value = GDK_FILTER_REMOVE;

  switch (xevent->data.l[1]) {
    case SYSTEM_TRAY_REQUEST_DOCK:
      systray_plug_added (manager, xevent);
      vdebug (5, "systray event => REQUEST_DOCK\n");
      break;

    case SYSTEM_TRAY_BEGIN_MESSAGE:
      systray_handle_begin_message (manager, xevent);
      vdebug (5, "systray event => BEGIN_MESSAGE\n");
      break;

    case SYSTEM_TRAY_CANCEL_MESSAGE:
      systray_handle_cancel_message (manager, xevent);
      vdebug (5, "systray event => CANCEL_MESSAGE\n");
      break;

    default:
      vdebug (5, "systray event => %d\n, xevent->data.l[1]");
      value = GDK_FILTER_CONTINUE;
  }

  return value;
} /* </systray_handle_event> */

/*
 * systray_window_filter
 */
static GdkFilterReturn
systray_window_filter (GdkXEvent *xev, GdkEvent *event, gpointer data)
{
  XEvent  *xevent  = (GdkXEvent *)xev;
  Systray *manager = data;

  if (xevent->type == ClientMessage) {
    XClientMessageEvent *cme = (XClientMessageEvent *)xevent;

    if (xevent->xclient.message_type == manager->opcode)
      return systray_handle_event (manager, cme);
    else if (xevent->xclient.message_type == manager->message) {
      return systray_handle_message_data (manager, cme);
    }
  }
  else if (xevent->type == SelectionClear) {
    g_signal_emit (manager, signals_[TRAY_LOST_SELECTION], 0);
    vdebug (1, "systray signal => LOST_SELECTION\n");
    systray_unmanage (manager);
  }

  return GDK_FILTER_CONTINUE;
} /* </systray_window_filter> */

/*
 * systray_set_orientation
 * systray_set_orientation_property
 */
static void
systray_set_orientation (Systray *manager, GtkOrientation orientation)
{
  g_return_if_fail (IS_SYSTRAY (manager));

  if (manager->orientation != orientation) {
    manager->orientation = orientation;

    systray_set_orientation_property (manager);

    g_object_notify (G_OBJECT (manager), "orientation");
  }
} /* </systray_set_orientation> */

static void
systray_set_orientation_property (Systray *manager)
{
  Atom        orientation;
  GdkDisplay *display;
  gulong      data[1];

  if (!manager->invisible || !manager->invisible->window)
    return;

  display = gtk_widget_get_display (manager->invisible);
  orientation = get_atom_property ("_NET_SYSTEM_TRAY_ORIENTATION");

  data[0] = manager->orientation == GTK_ORIENTATION_HORIZONTAL
          ? SYSTEM_TRAY_ORIENTATION_HORZ : SYSTEM_TRAY_ORIENTATION_VERT;

  XChangeProperty (GDK_DISPLAY_XDISPLAY (display),
                   GDK_WINDOW_XWINDOW (manager->invisible->window),
                   orientation,
                   XA_CARDINAL, 32,
                   PropModeReplace,
                   (guchar *) &data, 1);
} /* </systray_set_orientation_property> */

/*
 * systray_manage - manage system tray
 */
static gboolean
systray_manage (Systray *manager, GdkScreen *screen)
{
  Display    *display;
  GtkWidget  *invisible;
  Screen     *xscreen;
  Window      xwindow;
  char       *selection_atom_name;
  guint32     timestamp;

  g_return_val_if_fail (IS_SYSTRAY (manager), FALSE);
  g_return_val_if_fail (manager->screen == NULL, FALSE);

  invisible = gtk_invisible_new_for_screen (screen);
  gtk_widget_realize (invisible);

  gtk_widget_add_events (invisible,
                         GDK_PROPERTY_CHANGE_MASK | GDK_STRUCTURE_MASK);

  xwindow = GDK_WINDOW_XWINDOW (invisible->window);
  xscreen = GDK_SCREEN_XSCREEN (screen);
  display = DisplayOfScreen (xscreen);

  selection_atom_name = g_strdup_printf ("_NET_SYSTEM_TRAY_S%d",
                                         gdk_screen_get_number (screen));

  manager->selection = get_atom_property (selection_atom_name);
  g_free (selection_atom_name);

  gdk_error_trap_push ();
  timestamp = gdk_x11_get_server_time (invisible->window);
  XSetSelectionOwner (display, manager->selection, xwindow, timestamp);

  /* Check if we could set the selection owner successfully. */
  if (XGetSelectionOwner (display, manager->selection) == xwindow) {
    XClientMessageEvent xev;

    xev.type = ClientMessage;
    xev.window = RootWindowOfScreen (xscreen);
    xev.message_type = get_atom_property ("MANAGER");
    xev.format = 32;

    xev.data.l[0] = timestamp;
    xev.data.l[1] = manager->selection;
    xev.data.l[2] = xwindow;
    xev.data.l[3] = 0;    /* manager specific data */
    xev.data.l[4] = 0;    /* manager specific data */

    XSendEvent (display,
                RootWindowOfScreen (xscreen),
                False, StructureNotifyMask, (XEvent *)&xev);

    manager->invisible = invisible;
    g_object_ref (manager->invisible);

    manager->opcode = get_atom_property ("_NET_SYSTEM_TRAY_OPCODE");
    manager->message = get_atom_property ("_NET_SYSTEM_TRAY_MESSAGE_DATA");

    systray_set_orientation_property (manager);
    gdk_window_add_filter (invisible->window, systray_window_filter, manager);

    _x_error_trap_pop ("systray_manage\n", xwindow);
    return TRUE;
  }
  else {
    gtk_widget_destroy (invisible);
    _x_error_trap_pop ("systray_manage\n", xwindow);
    return FALSE;
  }
} /* </systray_manage> */

/*
 * systray_unmanage - unmanage system tray.
 */
static void
systray_unmanage (Systray *manager)
{
  Display    *display;
  GtkWidget  *invisible = manager->invisible;
  Window      window;
  guint32     timestamp;

  if (invisible == NULL)
    return;

  g_assert (GTK_IS_INVISIBLE (invisible));
  g_assert (GTK_WIDGET_REALIZED (invisible));
  g_assert (GDK_IS_WINDOW (invisible->window));
  gdk_error_trap_push ();

  display = GDK_WINDOW_XDISPLAY (invisible->window);
  window  = XGetSelectionOwner (display, manager->selection);

  if (window == GDK_WINDOW_XWINDOW (invisible->window)) {
    timestamp = gdk_x11_get_server_time (invisible->window);
    XSetSelectionOwner (display, manager->selection, None, timestamp);
  }

  _x_error_trap_pop ("systray_unmanage");
  gdk_window_remove_filter (invisible->window, systray_window_filter, manager);
  manager->invisible = NULL; /* prior to destroy for reentrancy paranoia */

  gtk_widget_destroy (invisible);
  g_object_unref (invisible);
} /* </systray_unmanage> */

/*
 * systray_icon_added - callback for adding "tray_icon_added" event.
 */
static void
systray_icon_added (Systray *manager, GtkWidget *icon, GtkWidget *tray)
{
  gtk_box_pack_end (GTK_BOX(tray), icon, FALSE, FALSE, 0);
  gtk_widget_show (icon);

  gdk_display_sync (gtk_widget_get_display (icon));
} /* </systray_icon_added> */

/*
 * systray_icon_removed - callback for adding "tray_icon_removed" event.
 */
static void
systray_icon_removed (Systray *manager, GtkWidget *icon, GtkWidget *tray)
{
  /* nothing else */
} /* </systray_icon_removed> */

/*
 * Public methods.
*/
Systray *
systray_new (void)
{
  return g_object_new (TYPE_SYSTRAY, NULL);
} /* </systray_new> */

/*
 * systray_check_running_screen - see if another client has systemtray.
 */
gboolean
systray_check_running_screen (GdkScreen *screen)
{
  GdkDisplay *display = gdk_screen_get_display (screen);
  char *name = g_strdup_printf ("_NET_SYSTEM_TRAY_S%d",
                                gdk_screen_get_number (screen));
  int error, result;

  Atom xatom = get_atom_property (name);
  g_free (name);

  gdk_error_trap_push ();
  result = XGetSelectionOwner (GDK_DISPLAY_XDISPLAY (display), xatom);
  error = _x_error_trap_pop ("systray_check_running_screen");

  return (result != None) ? TRUE : FALSE;
} /* </systray_check_running_screen> */

/*
 * systray_connect
 * systray_disconnect
 * systray_new
 */
void
systray_connect (Systray *manager, GdkScreen *screen, GtkWidget *tray)
{
  g_return_if_fail (IS_SYSTRAY (manager));

  if (systray_manage (manager, screen)) {
    manager->append = g_signal_connect (manager, "tray_icon_added",
                                        G_CALLBACK (systray_icon_added), tray);

    manager->remove = g_signal_connect (manager, "tray_icon_removed",
                                        G_CALLBACK (systray_icon_removed),tray);
  }
  else {
    g_printerr("systray_connect: %s\n",
               _("cannot manage systemtray on this screen."));
  }
} /* </systray_connect> */

void
systray_disconnect (Systray *manager)
{
  g_return_if_fail (IS_SYSTRAY (manager));

  if (manager->append > 0 && manager->remove > 0) {
    g_signal_handler_disconnect (manager, manager->remove);
    g_signal_handler_disconnect (manager, manager->append);

    systray_unmanage (manager);
  }
} /* </systray_disconnect> */

