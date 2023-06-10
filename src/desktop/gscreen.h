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

#ifndef GCONTROL_H
#define GCONTROL_H

#include <math.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <gtk/gtk.h>

#include "dialog.h"
#include "gwindow.h"
#include "iconbox.h"
#include "xpmglyphs.h"

G_BEGIN_DECLS

/* Global program data structure */
typedef struct _GlobalControl GlobalControl;

struct _GlobalControl
{
  GtkWidget *window;	/* main window */
  IconBox   *iconbox;

  gint height;		/* preferred main window height */
  gint width;		/* preferred main window width */
};

G_END_DECLS

#endif /* </GCONTROL_H */
