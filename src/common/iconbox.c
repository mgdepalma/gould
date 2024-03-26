/**
 * Copyright (C) Generations Linux <bugs@softcraft.org>
 * All Rights Reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <pango/pango.h>

#include "gould.h"
#include "iconbox.h"

/* (private) variables */
static GtkObjectClass *parent = NULL;


#if 0 /* unused code used for reference */
/*
* (private) append_names_list
* (private) get_names_list
*/
static gboolean
append_list_names (GtkTreeModel *model,
                   GtkTreePath  *path,
                   GtkTreeIter  *iter,
                   gpointer      data)
{
  gchar *label;
  GList *names = (GList *)data;

  gtk_tree_model_get (model, iter, COLUMN_LABEL, &label, -1);
  /* gchar *index = gtk_tree_path_to_string(path); */
  names = g_list_append(names, label);

  return FALSE; /* do not stop walking the store, call us with next row */
} /* </append_list_names> */

static GList *
get_names_list (IconBox *iconbox)
{
  GList *iter;
  GList *names = NULL;   /* new doubly linked list */

  gtk_tree_model_foreach(GTK_TREE_MODEL(iconbox->store),
                                append_list_names, names);

  /* Code to navigate the doubly linked list.
  for (iter = names; iter != NULL; iter = iter->next) {
    printf("%s\n", iter->data);
  }
  */

  /* At some point we need to free the memory allocated to names.
  g_list_foreach (names, (GFunc)g_free, NULL);
  g_list_free (names);
  */
  return names;
} /* </get_names_list> */
#endif

/*
* (private) active
*/
static void
active (GtkIconView *iconview, GtkTreePath *path, gpointer data)
{
  GtkTreeIter iterator;
  GtkTreeModel *model = gtk_icon_view_get_model (iconview);
  IconBoxItem *item = data;
  gpointer value;

  if (gtk_tree_model_get_iter (model, &iterator, path)) {
    gtk_tree_model_get (model, &iterator, item->column, &value, -1);
    item->data = value;
  }
} /* </active> */

/*
* (private) iconbox_class_init
*/
static void
iconbox_class_init (IconBoxClass *klass)
{
  parent = g_type_class_peek_parent (klass);
} /* </iconbox_class_init> */

/*
* (private) iconbox_init
*/
static void
iconbox_init (IconBox *iconbox)
{
  GtkWidget *view;
  GtkListStore *store;

  /* Construct the GtkIconView which shows directory contents. */
  view = gtk_icon_view_new ();
  gtk_icon_view_set_pixbuf_column (GTK_ICON_VIEW (view), COLUMN_IMAGE);
  gtk_icon_view_set_text_column (GTK_ICON_VIEW (view), COLUMN_LABEL);
  iconbox->view = view;

  store = gtk_list_store_new(3, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_POINTER);
  gtk_icon_view_set_model (GTK_ICON_VIEW (view), GTK_TREE_MODEL (store));
  iconbox->store = store;

  /* Set cursor for the view window */
  iconbox->cursor = gdk_cursor_new (GDK_HAND2);

  if (GDK_IS_WINDOW(view->window)) {
    gdk_window_set_cursor (view->window, iconbox->cursor);
    gdk_display_sync (gtk_widget_get_display (view));
  }
} /* </iconbox_init> */

/*
* iconbox_append
*/
void
iconbox_append (IconBox *iconbox, GdkPixbuf *pixbuf, const gchar *label)
{
  if (iconbox != NULL) {
    GtkTreeIter iterator;
    gtk_list_store_append (iconbox->store, &iterator);
    gtk_list_store_set (iconbox->store, &iterator,
                          COLUMN_IMAGE, pixbuf, COLUMN_LABEL, label, -1);
  }
  else {
    gould_error ("%s %s (iconbox => NULL)\n", timestamp(), __func__);
    vdebug(3, "%s: iconbox => NULL, pixbuf => 0x%lx, label => %s\n",
                __func__, pixbuf, label);
  }
} /* </iconbox_append> */

/*
* iconbox_clear
*/
void
iconbox_clear (IconBox *iconbox)
{
  if (iconbox != NULL) {
    gtk_list_store_clear (iconbox->store);
  }
  else {
    gould_error ("%s %s (iconbox => NULL)\n", timestamp(), __func__);
    vdebug(1, "%s: iconbox => NULL\n", __func__);
  }
} /* </iconbox_clear> */

/*
* iconbox_get_selected
*/
gpointer
iconbox_get_selected (IconBox *iconbox, IconBoxColumn column)
{
  IconBoxItem item = { column, NULL };
  GtkIconView *iconview = GTK_ICON_VIEW(iconbox->view);
  
  gtk_icon_view_selected_foreach (iconview, active, &item);

  return item.data;
} /* </iconbox_get_selected> */

/*
* iconbox_get_type
*/
GType
iconbox_get_type (void)
{
  static GType type = 0;

  if (type == 0) {
    static const GTypeInfo info =
    {
	sizeof (IconBoxClass),
	NULL,		/* base_init */
        NULL,		/* base_finalize */
	(GClassInitFunc)iconbox_class_init,
	NULL,		/* class_finalize */
	NULL,		/* class_data */
        sizeof (IconBox),
        0,		/* n_preallocs */
        (GInstanceInitFunc)iconbox_init
    };

    type = g_type_register_static (GTK_TYPE_OBJECT, "IconBox", &info, 0);
  }
  return type;
} /* </iconbox_get_type> */

/*
* iconbox_new
*/
IconBox *
iconbox_new (GtkOrientation place)
{
  IconBox *iconbox = gtk_type_new (ICONBOX_TYPE);

  gtk_icon_view_set_orientation (GTK_ICON_VIEW (iconbox->view), place);

  return iconbox;
} /* </iconbox_new> */

/*
* iconbox_unselect
*/
void
iconbox_unselect (IconBox *iconbox)
{
  gtk_icon_view_unselect_all (GTK_ICON_VIEW (iconbox->view));
}
