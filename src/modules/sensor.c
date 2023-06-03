/**
 * Copyright (C) Generations Linux <bugs@softcraft.org>
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; You may only use version 3 of the License,
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

#include "gould.h"
#include "gpanel.h"
#include "module.h"

extern const char *Authors;	/* see, gpanel.c */

/*
* Data structures used by this module.
*/
typedef struct _SensorConfig  SensorConfig;
typedef struct _SensorPrivate SensorPrivate;

struct _SensorConfig
{
  GtkWidget *display;		/* display drawing area */
};

struct _SensorPrivate
{
  SensorConfig *sensor;		/* applet configuration data */
  GtkTooltips *tooltips;
  gboolean enable;
};

static SensorPrivate local_;	/* private global structure singleton */


/*
 * sensor_configuration_read
 */
static void
sensor_configuration_read (Modulus *applet, SensorConfig *sensor)
{
  GlobalPanel *panel = applet->data;
  ConfigurationNode *chain = configuration_find (panel->config, "modules");
  ConfigurationNode *item;

  const gchar *attrib;


  /* Configuration for the clock applet. */
  applet->enable  = TRUE;

  while ((item = configuration_find (chain, "applet")) != NULL) {
    attrib = configuration_attrib (item, "name");

    if (strcmp(attrib, applet->name) == 0) {
      if ((attrib = configuration_attrib (item, "icon")) != NULL)
        applet->icon = attrib;

      if ((attrib = configuration_attrib (item, "enable")) != NULL)
        if (strcmp(attrib, "off") == 0 || strcmp(attrib, "no") == 0)
          applet->enable = FALSE;

      if ((item = configuration_find (item, "settings")) != NULL) {
      }
      break;
    }
    chain = configuration_find_next (chain, "applet");
  }
} /* </sensor_configuration_read> */

/*
 * module_settings provides configuration pages
 */
GtkWidget *
module_settings (Modulus *applet, GlobalPanel *panel)
{
  GtkWidget *layout = NULL;
  gchar *caption = g_strdup_printf ("%s [%s]", _(applet->label), _("plugin"));
  return layout;
} /* </module_settings> */

/*
 * Required methods
 */
void
module_init (Modulus *applet)
{
  GlobalPanel *panel = applet->data;
  SensorConfig *sensor = g_new0 (SensorConfig, 1);

  /* Modulus structure initialization. */
  applet->name    = "sensor";
  applet->icon    = "sensor.png";
  applet->place   = PLACE_END;
  applet->space   = MODULI_SPACE_ANCHOR;
  applet->release = "0.1";
  applet->authors = Authors;

  /* Read configuration data for the splash screen. */
  sensor_configuration_read (applet, sensor);

  /* Initialize private data structure singleton. */
  memset(&local_, 0, sizeof (SensorPrivate));

  local_.sensor   = sensor;
  local_.tooltips = gtk_tooltips_new();

  /* Construct the settings pages. */
  applet->label       = "Sensor";
  applet->description = _(
"Sensors for temperature, etc."
);
  applet->settings = settings_notebook_new (applet, panel,
                       _("Settings"), module_settings (applet, panel),
                       _("About"), settings_manpage_new (applet, panel),
                       -1);
} /* </module_init> */

void
module_open (Modulus *applet)
{
  GlobalPanel *panel = applet->data;

  /* Construct the user interface. */
  applet->widget = NULL;
} /* module_open */

void
module_close (Modulus *applet)
{
  /* gtk_widget_destroy (applet->widget); */
} /* module_close */
