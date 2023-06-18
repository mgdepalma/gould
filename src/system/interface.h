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

#ifndef INTERFACE_H
#define INTERFACE_H

#include "genesis.h"
#include "gwindow.h"
#include "xutil.h"

#include <ctype.h>
#include <X11/Xatom.h>
#include <gtk/gtk.h>

#define GENESIS  "genesis"
#define GPACKAGE "gpackage"

extern const char *Program;	/* published by main program */
extern const char *Release;	/* ... */
extern const char *License;	/* ... */
extern const char *Terms;	/* ... */

G_BEGIN_DECLS

/*
 * Public data structures.
*/
enum
{
  INSTALL,
  UPDATES,
  BOOKEND
};

enum
{
  BUSY_CURSOR,
  NORMAL_CURSOR,
  LAST_CURSOR
};

typedef struct _GlobalData  GlobalData;
typedef struct _GlobalDepot GlobalDepot;

struct _GlobalData
{
  GtkNotebook *pages;		 /* program master notebook */
  GtkNotebook *packages;	 /* packages installed and updates notebook */

  GlobalDepot *depot[BOOKEND];	 /* installed and updates software depots */
  GtkWidget   *dirent[BOOKEND];	 /* packages installed and updates path */
  GHashTable  *hashing[BOOKEND]; /* hash tables of packages by group */
  const char  *path[BOOKEND];	 /* cache of directory paths */

  GdkCursor *cursor[LAST_CURSOR];
  GtkTreeSelection *selection;	 /* one or multiple selections */

  GtkWidget *erase;	   	 /* erase (uninstall) packages button */
  GtkWidget *install;	   	 /* install/update packages button */
  GtkWidget *remove;		 /* remove RPM files button */
  GtkWidget *verify;		 /* verify packages button */
};

struct _GlobalDepot
{
  GtkTreeStore  *store;
  GtkTextBuffer *describe;
  GtkTextBuffer *filelist;
  GtkWidget     *viewer;
};

/*
 * Methods exported in the implementation.
*/
GtkWidget *navigation_frame (GtkWidget *layout);

void navigation_buttons (GtkWidget *layout, int back, int next);
void navigation_page (GtkWidget *widget, gpointer data);

G_END_DECLS

#endif /* </INTERFACE_H> */
