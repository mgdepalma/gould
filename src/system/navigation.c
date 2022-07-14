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

#include "interface.h"
#include "xpmglyphs.h"

extern GlobalData *global_;	/* imported from main program */

/*
 * navigation_frame 
 */
GtkWidget *
navigation_frame (GtkWidget *layout)
{
  GtkWidget *frame, *layer;
  gchar *description = g_strdup_printf ("%s %s", Program, Release);

  frame = gtk_frame_new (description);
  gtk_frame_set_shadow_type (GTK_FRAME(frame), GTK_SHADOW_ETCHED_IN);
  gtk_container_set_border_width (GTK_CONTAINER (frame), 4);
  gtk_container_add (GTK_CONTAINER(layout), frame);
  gtk_widget_show (frame);
  g_free (description);

  layer = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER(frame), layer);
  gtk_widget_show (layer);

  return layer;
} /* </navigation_frame> */

/*
 * navigation_buttons to go back and forward a step
 */
void
navigation_buttons (GtkWidget *layout, int back, int next)
{
  GtkWidget *button, *layer = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_end (GTK_BOX(layout), layer, FALSE, FALSE, 5);
  gtk_widget_show (layer);

  button = stock_button_new (_("Next"), GTK_STOCK_GO_FORWARD, 5);
  gtk_box_pack_end (GTK_BOX(layer), button, FALSE, FALSE, 5);
  gtk_widget_show (button);

  g_signal_connect (G_OBJECT(button), "clicked",
                    G_CALLBACK(navigation_page), GINT_TO_POINTER(next));

  button = stock_button_new (_("Back"), GTK_STOCK_GO_BACK, 5);
  gtk_box_pack_end (GTK_BOX(layer), button, FALSE, FALSE, 5);
  gtk_widget_show (button);

  g_signal_connect (G_OBJECT(button), "clicked",
                    G_CALLBACK(navigation_page), GINT_TO_POINTER(back));
} /* </navigation_buttons> */

/*
 * navigation_page
 */
void
navigation_page (GtkWidget *widget, gpointer data)
{
  gtk_notebook_set_current_page (global_->pages, GPOINTER_TO_INT(data));
} /* </navigation_page> */
