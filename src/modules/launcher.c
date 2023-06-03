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

#include "gould.h"
#include "gpanel.h"
#include "module.h"

#include "greenwindow.h"
#include "xutil.h"

extern const char *Authors;	/* see, gpanel.c */

/*
* Data structures used by this module.
*/
typedef struct _Launcher Launcher;

struct _Launcher
{
  const char *command;
};

static Launcher local_;     /* private structure singleton */

/*
 * module_settings provides configuration pages
 */
GtkWidget *
module_settings (Modulus *applet, GlobalPanel *panel)
{
  return settings_missing_new (applet, panel);
} /* </module_settings> */

/*
 * Required methods
 */
void
module_init (Modulus *applet)
{
  /* GlobalPanel *panel = applet->data; */

  /* Modulus structure initialization. */
  applet->name    = "launcher";
  applet->place   = PLACE_START;
  applet->space   = MODULI_SPACE_TASKBAR;
  applet->release = "1.0";
  applet->authors = Authors;

  /* Initialize private data structure singleton. */
  memset(&local_, 0, sizeof (Launcher));

  /* Construct the settings pages.
  applet->label       = "Program Launcher";
  applet->description = _(
"Launcher spawns a program."
);
  applet->settings = settings_notebook_new (applet, panel,
                       _("Settings"), module_settings (applet, panel),
                       _("About"), settings_manpage_new (applet, panel),
                       -1);
  */
} /* </module_init> */

void
module_open (Modulus *applet)
{
} /* module_open */

void
module_close (Modulus *applet)
{
} /* module_close */
