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

#ifndef MODULE_H
#define MODULE_H

#include <ctype.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <gmodule.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

/* Module registry bits and mask. */
#define MODULI_SPACE_ALL      0x0fff
#define MODULI_SPACE_SERVICE  0x0001
#define MODULI_SPACE_DESKTOP  0x0010
#define MODULI_SPACE_TASKBAR  0x0100
#define MODULI_SPACE_ANCHOR   0x0300
#define MODULI_SPACE_START    0x0500

/* Data structures */
typedef struct _modulus        Modulus;
typedef enum   _modulus_place  ModulusPlace;

enum _modulus_place {
  PLACE_NONE,			/* do not place anywhere */
  PLACE_SCREEN,			/* (docklet) place on screen */
  PLACE_CONTAINER,		/* add to taskbar container */
  PLACE_START,			/* place by start menu */
  PLACE_END			/* place at the end */
};

struct _modulus
{
  GModule   *module;		/* struct _GModule from glib */

  GtkWidget *widget;		/* applet primary widget object */
  GtkWidget *settings;		/* applet settings widget object */

  ModulusPlace place;		/* [required]ModulusPlace positioning */
  guint space;			/* [required]moduli_space membership */

  const gchar *name;		/* unique name identifying the module */
  const gchar *icon;		/* icon image basename or full path */
  const gchar *label;		/* [optional] label used for description */
  const gchar *description;	/* [optional] narrative description */
  const gchar *release;		/* [optional] version release string */
  const gchar *authors;		/* [optional] comma separated authors */
  const gchar *website;		/* [optional] maintainer website URL */

  void (*module_init) (Modulus *self);  /* [required] */
  void (*module_open) (Modulus *self);  /* [optional] */
  void (*module_close)(Modulus *self);  /* [optional] */

  gboolean enable;		/* [internal] governs how to enable */
  gpointer data;		/* usually assigned to program global */
};

/* Methods exported by implementation */
Modulus *module_load(const gchar *file, guint space, gpointer data);
Modulus *module_search(GList *moduli, const gchar *name);

ModulusPlace module_place_convert_string (const char *string);
const char  *module_place_convert_enum (ModulusPlace idx);

GList *moduli_remove(GList *moduli, Modulus *applet);
GList *moduli_space(GList *moduli,
                    const gchar *path,
                    guint space,
                    gpointer data);

/* General purpose methods. */
void killproc (pid_t *proc, int sig);
void killwait (pid_t *proc, int sig);

G_END_DECLS

#endif /* </MODULE_H */
