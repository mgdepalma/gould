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

#ifndef GREEN_H
#define GREEN_H

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <glib-object.h>
#include <gdk/gdkpixbuf.h>
#include <gdk/gdkscreen.h>

#include "xutil.h"

G_BEGIN_DECLS

/* Screen */
#define GREEN_TYPE            (green_get_type ())
#define GREEN(object)         (G_TYPE_CHECK_INSTANCE_CAST ((object), \
                               GREEN_TYPE, Green))

#define GREEN_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), \
                               GREEN_TYPE, GreenClass))

#define IS_GREEN(object)      (G_TYPE_CHECK_INSTANCE_TYPE((object), GREEN_TYPE))
#define IS_GREEN_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GREEN_TYPE))

#define GREEN_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
                               GREEN_TYPE, GreenClass))

#define GREEN_ACTIVATE_TIMEOUT 1000	/* g_timeout_add() timer default */

/* GREEN management tokens */
#define GREEN_NO_TOKEN          0

typedef struct _Green        Green;
typedef struct _GreenClass   GreenClass;
typedef struct _GreenPrivate GreenPrivate;

struct _Green
{
  GObject instance;		/* parent instance */

  GreenPrivate *priv;		/* private data */
};

struct _GreenClass
{
  GObjectClass parent;		/* parent class */

  void (* active_window_changed)    (Green *green);
  void (* active_workspace_changed) (Green *green);
  void (* application_opened)       (Green *green);
  void (* application_closed)       (Green *green);
  void (* background_changed)       (Green *green);
  void (* class_group_opened)       (Green *green);
  void (* class_group_closed)       (Green *green);
  void (* showing_desktop_changed)  (Green *green);
  void (* viewports_changed)        (Green *green);
  void (* window_opened)            (Green *green);
  void (* window_closed)            (Green *green);
  void (* window_stacking_changed)  (Green *green);
  void (* workspace_created)        (Green *green);
  void (* workspace_destroyed)      (Green *green);
};

typedef enum
{
  MOTION_UP = -1,
  MOTION_DOWN = -2,
  MOTION_LEFT = -3,
  MOTION_RIGHT = -4
} MotionDirection;

/**
 * Public methods declaration.
 */
GType green_get_type (void) G_GNUC_CONST;

GdkScreen *green_get_gdk_screen (Green *green);
Screen    *green_get_screen  (Green *green);

Window green_get_root_window (Green *green);
Pixmap green_get_background_pixmap (Green *green);

int  green_get_active_workspace (Green *green);
int  green_get_workspace_count (Green *green);

const gchar *green_get_workspace_name (Green *green, int desktop);

GList *green_get_windows (Green *green, WindowFilter filter, int desktop);
GList *green_get_windows_stacking(Green *green,WindowFilter filter,int desktop);

GList *green_get_group_list (Green *green, gpointer window);

void green_change_workspace (Green *green, int desktop, Time stamp);
void green_change_workspace_count (Green *green, int count);

void green_release_workspace_layout (Green *green, int token);
int  green_request_workspace_layout (Green *green, int token,int rows,int cols);

void green_remove_window (Green *green, Window xid);

void green_set_window_filter (Green *green, WindowFilter filter);

Green *green_get_default (void);
Green *green_filter_new (WindowFilter filter, int number);
Green *green_new (int number);

gpointer green_find_window (Window xid);

gint green_screen_width(void);

void green_select_input (Window xid, int mask);

G_END_DECLS

#endif /* </GREEN_H> */
