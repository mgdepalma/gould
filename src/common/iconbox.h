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

#ifndef ICON_BOX_H
#define ICON_BOX_H

#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <gtk/gtk.h>
#include <pango/pango.h>

#include "xpmglyphs.h"

G_BEGIN_DECLS

#define ICONBOX_TYPE (iconbox_get_type ())
#define ICONBOX(obj) \
                  (G_TYPE_CHECK_INSTANCE_CAST (obj, ICONBOX_TYPE, IconBox))
typedef enum {
  COLUMN_IMAGE,
  COLUMN_LABEL,
  COLUMN_DATA
} IconBoxColumn;

typedef struct _IconBox	     IconBox;
typedef struct _IconBoxClass IconBoxClass;
typedef struct _IconBoxItem  IconBoxItem;

struct _IconBox
{
  GtkObject parent;	/* IconBox inherits from GTK_TYPE_OBJECT */

  GdkCursor *cursor;	/* Cursor used in the view window */

  GtkListStore *store;	/* GTK_TREE_MODEL model used by default */
  GtkWidget *view;	/* GTK_ICON_VIEW instance */
};

struct _IconBoxClass
{
  GtkObjectClass parent;
};

struct _IconBoxItem
{
  IconBoxColumn column;
  gpointer data;
};

/* Methods exported by the implementation */
void     iconbox_append (IconBox *obj, GdkPixbuf *pixbuf,
                                             const gchar *label);
void     iconbox_clear (IconBox *obj);

gpointer iconbox_get_selected (IconBox *obj, IconBoxColumn col);

GType    iconbox_get_type (void);
IconBox *iconbox_new (GtkOrientation place);

void     iconbox_unselect (IconBox *obj);

G_END_DECLS

#endif /* ICON_BOX_H */
