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

#ifndef DIALOG_H
#define DIALOG_H

#include <errno.h>
#include <string.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <unistd.h>

#include <gtk/gtk.h>
#include <gdk/gdkx.h>

#include "filechooser.h"
#include "xpmglyphs.h"
#include "print.h"

G_BEGIN_DECLS

/* Definitions for image file type */
typedef enum {
  PNG,
  JPEG,
  BMP,
  PostScript
} FileType;

typedef struct {
  FileType type;
  gchar    *extension;
} FileBinding;

/* Data structure definition for generic gtk_dialog */
typedef struct _dialogue Dialogue;

struct _dialogue {
  GtkWidget *window;
  GtkWidget *layout;
  GtkWidget *cancel;
  GtkWidget *apply;
};

/* Data structure definition for print image dialog */
typedef struct _print_dialog PrintDialog;

struct _print_dialog {
  FileChooser *chooser;		/* file chooser instance */
  GtkFunction callback;		/* callback on save      */

  GtkWidget *parent;		/* parent window         */
  GtkWidget *dialog;		/* main dialog window    */
  GtkWidget *filer;		/* file chooser area     */

  gint width, height;		/* dialog minimal size   */

  GtkWidget *command;		/* print command entry   */
  GtkWidget *outfile;		/* print to file switch  */

  FileType imagetype;		/* image file type       */
  bool landscape;		/* landscape orientation */
  int device;			/* print device          */
  int paper;			/* PaperSize index       */
};

/* Data structure definition for save file dialog */
typedef struct _save_dialog SaveDialog;

struct _save_dialog {
  GtkWidget *parent;		/* parent window         */
  GtkWidget *dialog;		/* main dialog window    */
  GtkWidget *filer;		/* file chooser area     */

  GtkFunction callback;		/* callback on save      */
  GtkWidget   *options;		/* image type menu       */
  FileChooser *chooser;		/* file chooser instance */
  FileType    imgtype;		/* image file type       */

  GtkWidget *cancel;		/* cancel button         */
  GtkWidget *apply;		/* confirm button        */
};


/* Methods exported by implementation */
const gchar *get_file_extension(FileType index);

GtkWidget *add_button(GtkWidget *dialog, IconIndex index,
                      gchar *label, GtkResponseType handle);

void remove_button(GtkWidget *dialog, GtkWidget* button);

GtkWidget *add_filetypes(GtkWidget **list);
void set_file_type(GtkWidget *dialog, gpointer value);

GtkWidget *add_papersize(PaperSize *list);
void set_papersize(GtkWidget *widget, gpointer value);

GtkWidget *add_printers(GList *list);
void set_printer(GtkWidget *widget, gpointer value);

void add_horizontal_separator(GtkWidget* layout);
void add_horizontal_space(GtkWidget* layout, gint space);
void add_vertical_space(GtkWidget* layout, gint space);

GtkWidget *make_menu_item(gchar *name,GCallback callback,gpointer data);

PrintDialog *printdialog_new (GtkWidget *parent, GtkFunction callback);
SaveDialog  *savedialog_new  (GtkWidget *parent, GtkFunction callback);

gint notice(GtkWidget *parent, IconIndex icon,
	const char *fontname, const int fontsize, const gchar* fmt, ...);

gint notice_at(gint xpos, gint ypos, IconIndex icon,
	const char *fontname, const int fontsize, const gchar* fmt, ...);

gint overwrite(GtkWidget *parent, const gchar* filename);
void unimplemented(GtkWidget *parent, const gchar* feature);

G_END_DECLS

#endif /* </DIALOG_H> */
