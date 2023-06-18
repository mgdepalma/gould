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

#ifndef GRDESKTOP_H
#define GRDESKTOP_H

#include <stdlib.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>

#include "dialog.h"
#include "gwindow.h"
#include "xpmglyphs.h"

G_BEGIN_DECLS

/* Numeric constants */
#define BORDER_WIDTH 10

/* Global program data structure */
typedef struct _GlobalRemote GlobalRemote;

struct _GlobalRemote
{
  GtkWidget *window;	/* main interface window */
  GtkWidget *layout;	/* main interface layout */

  GtkWidget *username;	/* authentication user name */
  GtkWidget *password;	/* authentication password */
  GtkWidget *server;	/* server to log on */
  GtkWidget *domain;	/* server domain */

  GtkWidget *resolution;
  gboolean fullscreen;
};

G_END_DECLS

#endif /* </GRDESKTOP_H */
