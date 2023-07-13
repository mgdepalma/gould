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

#ifndef XUTIL_H
#define XUTIL_H

#include "util.h"

#include <X11/Xlib.h>
#include <gdk/gdkx.h>
#include <gdk/gdk.h>

#define X_MAXBYTES 65536  // XGetWindowProperty long_length parameter

G_BEGIN_DECLS

/**
 * Data structures declaration.
 */
typedef bool (*WindowFilter)(Window xid, int desktop);

typedef struct _XWindowState XWindowState;
typedef struct _XWindowType  XWindowType;

struct _XWindowState {
    unsigned int modal : 1;
    unsigned int hidden : 1;
    unsigned int shaded : 1;
    unsigned int sticky : 1;
    unsigned int skip_taskbar : 1;
    unsigned int skip_pager : 1;
    unsigned int fullscreen : 1;
    unsigned int maximized_horz : 1;
    unsigned int maximized_vert : 1;
    unsigned int above : 1;
    unsigned int below : 1;
    unsigned int demands_attention : 1;
};

struct _XWindowType {
    unsigned int desktop : 1;
    unsigned int dialog : 1;
    unsigned int dock : 1;
    unsigned int menu : 1;
    unsigned int normal : 1;
    unsigned int splash : 1;
    unsigned int toolbar : 1;
    unsigned int utility : 1;
};

/*
* _x_error_trap_pop - gdk_error_trap_pop() wrapper.
*/
static inline int
_x_error_trap_pop (const char *format, ...)
{
  int error;

  XFlush (gdk_display);		/* flush the X queue to catch errors, now */
  error = gdk_error_trap_pop ();

  if (error && format) {
    gchar *message;
    va_list args;

    va_start (args, format);
    message = g_strdup_vprintf (format, args);
    vdebug (1, "Xlib error code %d at %s\n", error, message);
    g_free (message);
    va_end (args);
  }

  return error;
} /* </_x_error_trap_pop> */

/**
* Public methods (xutil.c) exported in the implementation.
*/
bool WindowValidate (Window xid);

bool WindowArraysEqual (Window *a, int a_len, Window *b, int b_len);
bool WindowGListEqual (GList *alist, GList *blist);

bool WindowDesktopFilter (Window xid, int desktop);
bool WindowTaskbarFilter (Window xid, int desktop);
bool WindowPagerFilter (Window xid, int desktop);

Atom *get_atom_list (Window xid, Atom prop, int *count);
Atom get_atom_property (const char *name);

Window get_active_window (Window xroot);

Window *get_window_client_list (Window xroot, int desktop, int *count);
Window *get_window_client_list_stacking (Window xroot, int desktop, int *count);

Window *get_window_filter_list (WindowFilter filter,
                        Window xroot, Atom atom, int desktop, int *count);

Window *get_window_list (Window xroot, Atom atom, int desktop, int *count);
pid_t get_window_pid (Window xid);

gchar **get_desktop_names (Window xroot);

int get_current_desktop (Screen *screen);
int get_window_desktop (Window xid);

const gchar *get_window_class (Window xid);

bool get_window_geometry (Window xid, GdkRectangle *geometry);

GdkPixbuf *get_window_icon (Window xid);
GdkPixbuf *get_window_icon_scaled (Window xid, int width, int height);

const gchar *get_window_icon_name (Window xid);
const gchar *get_window_name (Window xid);

Pixmap get_window_pixmap (Window xid);

bool get_window_state (Window xid, XWindowState *xws);
bool get_window_type (Window xid, XWindowType *xwt);

void *get_xprop_atom (Window xid, Atom prop, Atom type, int *count);
int   get_xprop_cardinal (Window xid, const char *name);
void *get_xprop_name (Window xid, const char *name);
char *get_xprop_text (Window xid, Atom atom);

void set_window_allowed_actions (Window xid, Atom *actions, int length);

G_END_DECLS

#endif /* </XUTIL_H */
