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

#ifndef GREEN_WINDOW_H
#define GREEN_WINDOW_H

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "green.h"

G_BEGIN_DECLS

/* Screen */
#define GREEN_TYPE_WINDOW            (green_window_get_type ())
#define GREEN_WINDOW(object)         (G_TYPE_CHECK_INSTANCE_CAST ((object), \
                                      GREEN_TYPE_WINDOW, GreenWindow))

#define GREEN_WINDOW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), \
                                      GREEN_TYPE_WINDOW, GreenWindowClass))

#define IS_GREEN_WINDOW(object)      (G_TYPE_CHECK_INSTANCE_TYPE((object), \
                                      GREEN_TYPE_WINDOW))
#define IS_GREEN_WINDOW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), \
                                      GREEN_TYPE_WINDOW))

#define GREEN_WINDOW_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
                                      GREEN_TYPE_WINDOW, GreenClass))

typedef struct _GreenWindow        GreenWindow;
typedef struct _GreenWindowClass   GreenWindowClass;
typedef struct _GreenWindowPrivate GreenWindowPrivate;

struct _GreenWindow
{
  GObject instance;		/* parent instance */

  GreenWindowPrivate *priv;	/* private data */
};

struct _GreenWindowClass
{
  GObjectClass parent;		/* parent class */

  void (* icon_changed)      (GreenWindow *object);
  void (* name_changed)      (GreenWindow *object);
  void (* state_changed)     (GreenWindow *object);
  void (* geometry_changed)  (GreenWindow *object);
  void (* workspace_changed) (GreenWindow *object);
};

/**
 * Public methods declaration.
 */
GType green_window_get_type (void) G_GNUC_CONST;

void green_window_property_notify (GreenWindow *object, XEvent *xevent);
void green_window_configure_notify (GreenWindow *object, XEvent *xevent);

void green_window_activate (GreenWindow *window, Time stamp);
void green_window_change_workspace (GreenWindow *window, int desktop);

void green_window_change_state (GreenWindow *window,
                                bool enable, const char *prop);

void green_window_close (GreenWindow *window);
void green_window_minimize (GreenWindow *window);
void green_window_maximize (GreenWindow *window);
void green_window_move (GreenWindow *window);
void green_window_resize (GreenWindow *window);
void green_window_restore (GreenWindow *window);

Window        green_window_get_xid (GreenWindow *window);
GdkPixbuf    *green_window_get_icon (GreenWindow *window);
const gchar  *green_window_get_name (GreenWindow *window);
const gchar  *green_window_get_class (GreenWindow *window);
GdkRectangle *green_window_get_geometry (GreenWindow *window);
XWindowState *green_window_get_state (GreenWindow *window);
int           green_window_get_desktop (GreenWindow *window);

GreenWindow *green_window_new (Window xid, Green *green);

G_END_DECLS

#endif /* </GREEN_WINDOW_H> */
