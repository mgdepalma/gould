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

#ifndef GRABBER_H
#define GRABBER_H

#include <string.h>
#include <stdbool.h>
#include <unistd.h>

#include <X11/cursorfont.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>

#define GRAB_SCREEN  0
#define GRAB_WINDOW  1
#define GRAB_REGION  2

typedef unsigned char byte;
typedef unsigned int  word;

G_BEGIN_DECLS

/* Methods exported in the implementation */
XImage *grab(Window drawable, XRectangle *xrect);
Window  grab_window(Display *display, Window root);
int     grab_rectangle(Display *display, Window root, XRectangle *xrect);

GdkPixbuf *grab_pixbuf(Window xid, XRectangle *xrect);
GdkPixbuf *capture(gint mode, bool decorations);

void grab_pointer_sleep (unsigned int seconds);

G_END_DECLS

#endif	/* GRABBER_H */
