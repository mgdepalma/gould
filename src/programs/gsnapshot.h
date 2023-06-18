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

#ifndef GSNAPSHOT_H
#define GSNAPSHOT_H

#include <stdlib.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>

#include "filechooser.h"
#include "xpmglyphs.h"
#include "grabber.h"
#include "gwindow.h"
#include "dialog.h"

G_BEGIN_DECLS

/* Numeric constants */
#define BORDER_WIDTH  10
#define BOX_SPACING   20

#define VIEW_HEIGHT  150
#define VIEW_WIDTH   200


/* Global program data structure */
typedef struct _GlobalSnapshot GlobalSnapshot;

struct _GlobalSnapshot
{
  GtkWidget *window;      /* main interface window */
  GtkWidget *layout;      /* main interface layout */

  GtkWidget *browser;	  /* file browser on the left */
  GtkWidget *modes;	  /* capture mode GtkComboBox */
  GtkWidget *viewer;	  /* snapshot preview area */
  GdkPixbuf *image;       /* GdkPixbuf of the current snapshot */

  gboolean decorations;   /* include window decorations */
  gboolean hide;          /* hide application during a snapshot */

  unsigned int delay;     /* delay in milliseconds */

  PrintDialog *print;     /* print image dialog data structure */
  SaveDialog  *save;      /* save file dialog data structure */
};

G_END_DECLS

#endif /* </GSNAPSHOT_H */
