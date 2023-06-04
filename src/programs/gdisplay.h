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

#ifndef GDISPLAY_H
#define GDISPLAY_H

#include "gould.h"	/* common package declarations */
#include "green.h"
#include "dialog.h"
#include "filechooser.h"
#include "xpmglyphs.h"
#include "window.h"
#include "util.h"

#include <math.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

/* Global program data structure */
typedef struct _GlobalDisplay GlobalDisplay;

struct _GlobalDisplay
{
  FileChooser *chooser;	  /* file chooser instance */

  GtkWidget *window;	  /* main window: paned top, controls bottom */
  GtkWidget *browser; 	  /* file browser left side panel */
  GtkWidget *viewer;	  /* preview image right side panel */
  GtkWidget *canvas;	  /* image canvas drawing area */

  GtkWidget *backward;    /* backward one selection button */
  GtkWidget *forward;     /* forward one selection button */

  char resource[MAX_PATHNAME]; /* resource path ($HOME/.config/desktop) */

  gboolean filer;	  /* file brower active: TRUE or FALSE */

  gint height;		  /* preferred main window height */
  gint width;		  /* preferred main window width */
};

G_END_DECLS

#endif /* </GDISPLAY_H */
