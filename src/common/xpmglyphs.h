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

#ifndef XPMGLYPHS_H
#define XPMGLYPHS_H

#include <stdlib.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>

#define MAX_LABEL_LENGTH 32	/* max length of labels */

G_BEGIN_DECLS

/* Definition of icons available */
typedef enum
{
  ICON_APPLY,
  ICON_AUDIO,
  ICON_BACK,
  ICON_BLANK,
  ICON_BROKEN,
  ICON_BULB,
  ICON_CAMERA,
  ICON_CANCEL,
  ICON_CANCEL_PRINT,
  ICON_CANCEL_SAVE,
  ICON_CHOOSER,
  ICON_CLOSE,
  ICON_CUT,
  ICON_DELETE,
  ICON_DIRS,
  ICON_DONE,
  ICON_DOWN,
  ICON_ECLIPSE,
  ICON_ERROR,
  ICON_EXEC,
  ICON_EXIT,
  ICON_EXPAND,
  ICON_FILE,
  ICON_FOLDER,
  ICON_FORWARD,
  ICON_GENESIS,
  ICON_HALT,
  ICON_HARDISK,
  ICON_HELP,
  ICON_HIDDEN,
  ICON_HOME,
  ICON_ICONS,
  ICON_IMAGE,
  ICON_INFO,
  ICON_ITEM,
  ICON_JAVA,
  ICON_LOCK,
  ICON_LOGO,
  ICON_LOGOUT,
  ICON_MAXIMIZE,
  ICON_MINIMIZE,
  ICON_MENU,
  ICON_OPEN,
  ICON_PACKAGE,
  ICON_PAINT,
  ICON_PAPER,
  ICON_PASTE,
  ICON_PDF,
  ICON_POSTSCRIPT,
  ICON_PRINT,
  ICON_PRINTER,
  ICON_QUESTION,
  ICON_QUIT,
  ICON_RDESKTOP,
  ICON_REBOOT,
  ICON_RECORD,
  ICON_REMOTE,
  ICON_RESIZE,
  ICON_RESTORE,
  ICON_SAVE,
  ICON_SAVE_AS,
  ICON_SETTING,
  ICON_SMILE,
  ICON_SNAPSHOT,
  ICON_START,
  ICON_SUSPEND,
  ICON_SYMLINK,
  ICON_THUMBNAIL,
  ICON_TITLE,
  ICON_UP,
  ICON_USBDRIVE,
  ICON_VIDEO,
  ICON_VISIBLE,
  ICON_VSPACER,
  ICON_WARNING,
  ICON_WELCOME,
  ICON_WALLPAPER,
  ICON_WORKSPACE
} IconIndex;

typedef struct {
  IconIndex   index;
  char      **data;
  GdkPixmap  *pixmap;
} IconCatalog;


/* Methods exported by the implementation */
GdkPixmap *xpm_icon (IconIndex index, GdkBitmap **mask, GdkColor *bg);

GdkPixbuf *xpm_pixbuf (IconIndex index, GdkColor *bg);
GdkPixbuf *xpm_pixbuf_scale (IconIndex index,
                             gint width, gint height,
                             GdkColor *bg);

GtkWidget *xpm_image (IconIndex index);
GtkWidget *xpm_image_scaled (IconIndex index, gint width, gint height);

GtkWidget *xpm_image_button (IconIndex index);
GtkWidget *xpm_image_button_scaled (IconIndex index, gint width, gint height);

GtkWidget *xpm_button (IconIndex index, const gchar *text);
GtkWidget *xpm_label (IconIndex index, const gchar *text);

G_END_DECLS

#endif /* </XPMGLYPHS_H> */
