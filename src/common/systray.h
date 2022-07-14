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

#ifndef SYSTRAY_H
#define SYSTRAY_H

#include <gtk/gtkwidget.h>
#include <gdk/gdkx.h>

G_BEGIN_DECLS

#define TYPE_SYSTRAY            (systray_get_type ())

#define SYSTRAY(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
                                 TYPE_SYSTRAY, Systray))

#define IS_SYSTRAY(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                                 TYPE_SYSTRAY))

#define SYSTRAY_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), \
                                 TYPE_SYSTRAY, SystrayClass))

#define IS_SYSTRAY_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), \
                                 TYPE_SYSTRAY))

#define SYSTRAY_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
                                 TYPE_SYSTRAY, SystrayClass))

/*
 * Systray data structures.
*/
typedef struct _Systray      Systray;
typedef struct _SystrayClass SystrayClass;
typedef struct _SystrayChild SystrayChild;

struct _Systray
{
  GObject instance;		/* parent instance */

  GList      *messages;
  GHashTable *socketable;

  GtkOrientation orientation;
  GtkWidget     *invisible;
  GdkScreen     *screen;

  Atom  selection;
  Atom  message;
  Atom  opcode;

  gulong append;
  gulong remove;
};

struct _SystrayClass
{
  GObjectClass klass;		/* parent class */

  void (* tray_icon_added)   (Systray *manager, SystrayChild *child);
  void (* tray_icon_removed) (Systray *manager, SystrayChild *child);

  void (* message_sent)      (Systray      *manager,
                              SystrayChild *child,
                              const gchar  *message,
                              glong         id,
                              glong         timeout);

  void (* message_cancelled) (Systray      *manager,
                              SystrayChild *child,
                              glong         id);

  void (* lost_selection)    (Systray *manager);
};

/**
 * Public methods exported by implementation.
 */
GType systray_get_type (void) G_GNUC_CONST;

Systray *systray_new (void);

gboolean systray_check_running_screen (GdkScreen *screen);

void systray_connect (Systray *manager, GdkScreen *screen, GtkWidget *tray);
void systray_disconnect (Systray *manager);

G_END_DECLS

#endif /* </SYSTRAY_H */
