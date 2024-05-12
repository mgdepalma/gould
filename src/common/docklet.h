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

#ifndef DOCKLET_H
#define DOCKLET_H

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define DOCKLET_TYPE (docklet_get_type ())
#define DOCKLET(obj) \
           (G_TYPE_CHECK_INSTANCE_CAST (obj, DOCKLET_TYPE, Docklet))

typedef struct _Docklet       Docklet;
typedef struct _DockletClass  DockletClass;
typedef struct _DockletDatum  DockletDatum;

struct _Docklet
{
  GtkObject parent;	/* Docklet inherits from GTK_TYPE_OBJECT */

  GtkFunction agent;    /* user defined callback */
  DockletDatum *datum;	/* information passed to the user defined callback */

  GtkWidget *window;	/* application window instance */
  GtkWidget *layout;	/* application window internal layout */
  GtkWidget *inside;	/* application window internals */
  GdkPixbuf *render;	/* off screen pixbuf contents */

  gulong buttonHandler;	/* tag identifing the button-press-event callback */
  gulong windowHandler;	/* tag identifing the configure-event callback */

  bool editable;	/* governs if editable (false => launch only) */
  bool moved;		/* used to flag position change */

  GtkOrientation place;	/* GTK_ORIENTATION of text */
  GtkVisibility visa;	/* governs using a frame layout */

  gint width, height;	/* application window dimensions */
  gint xpos, ypos;	/* application window position */

  const gchar *icon;    /* docklet icon image file */
  const gchar *text;    /* docklet text next to image */
  const gchar *font;    /* font description (ex: "Luxi Mono 12") */

  GdkColor *bg;		/* background color */
  GdkColor *fg;		/* foreground color */

  bool shadow;
};

struct _DockletClass
{
  GtkObjectClass parent; /* DockletClass inherits from GtkObjectClass */
};

struct _DockletDatum
{
  Docklet  *docklet;	 /* unique instance of a Docklet class */
  GdkEvent *event;	 /* union of events (see, gdk/gdkevents.h) */
  gpointer payload;	 /* payload data, ex. ConfigurationNode */
};

/* Methods exported in the implementation */
GType docklet_get_type (void);

Docklet *docklet_new (GdkWindowTypeHint hint,
                      gint width, gint height,
                      gint xpos, gint ypos,
                      GtkOrientation place,
                      GtkVisibility visa,
                      const gchar *icon,
                      const gchar *text,
                      const gchar *font,
                      GdkColor *bg,
                      GdkColor *fg,
                      bool shadow);

void docklet_update (Docklet *instance, const gchar *icon, const gchar *text);
void docklet_set_cursor (Docklet *instance, GdkCursorType cursor);

void docklet_set_callback (Docklet *instance,
                           GtkFunction callback,
                           gpointer data);

G_END_DECLS

#endif /* </DOCKLET_H */
