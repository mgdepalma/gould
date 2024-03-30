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

#ifndef FILECHOOSER_H
#define FILECHOOSER_H

#include <dirent.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <gtk/gtk.h>

#include "iconbox.h"

G_BEGIN_DECLS

#define FILECHOOSER_TYPE (filechooser_get_type ())
#define FILECHOOSER(obj) \
           (G_TYPE_CHECK_INSTANCE_CAST (obj, FILECHOOSER_TYPE, FileChooser))

/* Global program data structure */
typedef struct _FileChooser      FileChooser;
typedef struct _FileChooserClass FileChooserClass;
typedef struct _FileChooserTypes FileChooserTypes;
typedef struct _FileChooserDatum FileChooserDatum;

typedef struct _IconboxDatum {
    gchar *name;		/* name displayed or filename */
    char label[MAX_PATHNAME];	/* screensaver title */
} IconboxDatum;

typedef bool (*IconboxFilter)(IconboxDatum *);

struct _FileChooser
{
  GtkObject parent;		/* FileChooser inherits from GTK_TYPE_OBJECT */

  GtkFunction agent;		/* callback function invoked on selection */
  FileChooserDatum *datum;	/* user defined data passed to callback */
  IconboxFilter iconboxfilter;	/* optional iconbox callback filter */
  IconBox *iconbox;		/* IconBox container object */

  GtkWidget *split; 		/* Split view layout */
  GtkWidget *viewer;		/* iconbox->view */

  GtkWidget *dirbox;		/* hbox for directory navigation */
  GtkWidget *dirup;		/* up one directory level button */
  GtkWidget *home;		/* home directory button */
  GtkWidget *path;		/* current directory */

  GtkWidget *namebox;   	/* hbox for file name */
  GtkWidget *actuator;		/* icon button on the left */
  GtkWidget *name;		/* current name displayed */

  bool clearname;		/* clear name on directory change */
  bool showhidden;		/* show hidden files (TRUE | FALSE) */
  bool showthumbs;		/* show thumbnails (TRUE | FALSE) */

  unsigned short thumbsize;	/* 32 => 32x32, 48 => 48x48, ... */

  GList    *_names;		/* list of directory entries */
  int      _cursor;		/* present position in names */
  int      _count;		/* number of directory entries */
};

struct _FileChooserClass
{
  GtkObjectClass parent;
};

struct _FileChooserTypes
{
  const char *pattern;
  IconIndex   index;
};

struct _FileChooserDatum
{
  FileChooser *self;
  const gchar *file;
  gpointer    user;
};

/* Constants exported by implementation */
#define FILECHOOSER_CWD  "{CWD}"
#define FILECHOOSER_HOME "{HOME}"
#define FILECHOOSER_ROOT "{ROOT}"

/* Methods exported by implementation */
GType        filechooser_get_type (void);
FileChooser *filechooser_new (const gchar *dirname, ...);

GtkWidget   *filechooser_layout (FileChooser *obj);

gchar       *filechooser_get_selected_name (FileChooser *obj);

int          filechooser_get_index (FileChooser *obj, const gchar *name);
int          filechooser_get_count (FileChooser *obj);

int          filechooser_get_cursor (FileChooser *obj);
void         filechooser_set_cursor (FileChooser *obj, int index);

void         filechooser_set_name (FileChooser *self, const gchar *name);

bool         filechooser_update (FileChooser *obj,
                                 const gchar *path,
                                 bool clearname);

void         filechooser_set_callback (FileChooser *obj,
                                       GtkFunction callback,
                                       gpointer data);
G_END_DECLS

#endif /* </FILECHOOSER_H */
