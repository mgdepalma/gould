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

#ifndef WINDOW_H
#define WINDOW_H

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>

#include <X11/Xlib.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

/*
* Public methods (gwindow.c) exported in the implementation.
*/
bool apply_gtk_theme (const char *confile);

GdkColormap *colormap_from_pixmap (GdkPixmap *pixmap);

GdkBitmap *create_mask_from_pixmap (GdkPixmap *pixmap, gint xsize, gint ysize);

GtkAllocation *get_label_size (GtkWidget *label);

GtkWidget *image_new_from_file_scaled (const gchar *file,
                                       gint width, gint height);

GtkWidget *image_new_from_path_scaled (GList *paths, const gchar *icon,
                                       guint width, guint height);

GdkPixbuf *pixbuf_new_from_file_scaled (const gchar *file,
                                        gint width, gint height);

GdkPixbuf *pixbuf_new_from_path_scaled (GList *paths, const gchar *file,
                                        guint width, guint height);

GdkPixbuf *pixbuf_scale (GdkPixbuf *pixbuf, gint width, gint height);

bool redraw_pixbuf (GtkWidget *canvas, GdkPixbuf *pixbuf);

GError *set_background_pixbuf (GdkWindow *window, GdkPixbuf *pixbuf);
GError *set_background (GdkWindow *window, const gchar *pathname);

void draw_pixbuf (GdkDrawable *target,
                  GdkPixbuf *pixbuf, GdkGC *gc,
                  gint xpos, gint ypos);

void draw_pixmap (GdkDrawable *target,
                  GdkPixmap *pixmap, GdkGC *gc,
                  gint xpos, gint ypos);

GtkWidget *sticky_window_new (GdkWindowTypeHint hint,
                              gint width, gint height,
                              gint xpos, gint ypos);

GtkWidget *backdrop_window_new (GdkWindowTypeHint hint,
                                gint width, gint height,
                                gint xpos, gint ypos,
                                const char *shape);

GtkWidget *inset_new (GtkWidget *box, guint spacing);
GtkWidget *inset_frame_new (GtkWidget *box, guint spacing, const gchar *tag);

GtkWidget *stock_button_new (const char *name, const char *stockid, int pad);

G_END_DECLS

#endif /* </WINDOW_H */
