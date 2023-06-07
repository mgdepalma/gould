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

#define BATTERY_FORMAT_MAX 80

extern const char *Program;	/* see, gpanel.c */
const char *Release = "1.2.1";

/*
* Data structures used by this module.
*/
typedef struct _BatteryConfig  BatteryConfig;
typedef struct _BatteryPrivate BatteryPrivate;
typedef struct _BatteryState   BatteryState;

struct _BatteryConfig
{
  guint interval;		/* update interval in seconds */
  guint warning;		/* warning threshold, percent */
  guint critical;		/* critical threshold, percent */
  gchar *command;               /* command to execute at critical */
};

struct _BatteryPrivate
{
  gboolean enable;		/* enable (or not) user selection */

  BatteryConfig  cache;		/* cache for configuration data */
  BatteryConfig *config;	/* applet configuration data */

  GtkWidget *enable_toggle;	/* enable (or not) check box */
  GtkWidget *warning_scale;
  GtkWidget *critical_scale;
  GtkWidget *interval_scale;

  GdkPixbuf *chargingIcon;	/* pixbuf for the charging icon */
  GdkPixbuf *dischargingIcon;	/* pixbuf for the discharing icon */

  GtkWidget *canvas;		/* drawing area shown on the panel */
  GtkWidget *layout;		/* applet->widget layout */

  GtkTooltips *tooltips;
};

struct _BatteryState
{
  gboolean discharging;
  long     remaining;
  long     capacity;
};

static BatteryPrivate local_;	/* private global structure singleton */

static char *SYSBAT = "/sys/class/power_supply/BAT0";


/*
 * battery_configuration_read
 */
static void
battery_configuration_read (Modulus *applet, BatteryConfig *config)
{
  GlobalPanel *panel = applet->data;
  ConfigurationNode *chain = configuration_find (panel->config, "modules");
  ConfigurationNode *item;

  const gchar *attrib;


  /* Configuration for the battery applet. */
  applet->enable = TRUE;
  config->interval = 20;	/* hardcoded check interval fallback */
  config->command = "";

  while ((item = configuration_find (chain, "applet")) != NULL) {
    attrib = configuration_attrib (item, "name");

    if (strcmp(attrib, applet->name) == 0) {
      if ((attrib = configuration_attrib (item, "icon")) != NULL)
        applet->icon = attrib;

      if ((attrib = configuration_attrib (item, "enable")) != NULL)
        if (strcmp(attrib, "off") == 0 || strcmp(attrib, "no") == 0)
          applet->enable = FALSE;

      if ((item = configuration_find (item, "settings")) != NULL) {
        if ((attrib = configuration_attrib (item, "interval")) != NULL)
          config->interval = atoi(attrib);

        if ((attrib = configuration_attrib (item, "command")) != NULL)
          config->command = (gchar *)attrib;

        if ((attrib = configuration_attrib (item, "critical")) != NULL)
          config->critical = atoi(attrib);

        if ((attrib = configuration_attrib (item, "warning")) != NULL)
          config->warning = atoi(attrib);
      }
      break;
    }
    chain = configuration_find_next (chain, "applet");
  }
} /* </battery_configuration_read> */

/*
 * battery_settings_apply
 * battery_settings_cancel
 */
static void
battery_settings_apply (Modulus *applet)
{
  GlobalPanel *panel = applet->data;
  BatteryConfig *config = local_.config;

  const char *spec;
  gchar *data;


  /* Save configuration settings from singleton cache settings. */
  memcpy(config, &local_.cache, sizeof(BatteryConfig));
  applet->enable = local_.enable;

  /* Save configuration settings in cache. */
  spec =
"<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
"<applet name=\"%s\" icon=\"%s\"%s>\n"
" <settings interval=\"%d\" command=\"%s\" critical=\"%d\" warning=\"%d\" />\n"
"</applet>\n";

  data = g_strdup_printf (spec, applet->name, applet->icon,
                          ((applet->enable) ? "" : " enable=\"no\""),
                          config->interval,
                          config->command, config->critical,
                          config->warning);
  vdebug (3, "\n%s\n", data);

  /* Replace configuration item. */
  configuration_replace (panel->config, data, "modules", "applet",applet->name);
  g_free (data);

  /* Memory was freed when original item was removed, read configuration. */
  battery_configuration_read (applet, config);

  /* Save configuration to singleton settings cache. */
  memcpy(&local_.cache, config, sizeof(BatteryConfig));

  /* Show or hide pager widget according to user selection. */
  if (applet->enable) {
    gtk_widget_show (applet->widget);
  }
  else {
    gtk_widget_hide (applet->widget);
  }
} /* </battery_settings_apply> */

static void
battery_settings_cancel (Modulus *applet)
{
  BatteryConfig *config = local_.config;	/* persistent settings */

  memcpy(&local_.cache, config, sizeof(BatteryConfig));
  local_.enable = applet->enable;

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(local_.enable_toggle),
                                applet->enable);

  gtk_range_set_value (GTK_RANGE(local_.interval_scale), config->interval);
  gtk_range_set_value (GTK_RANGE(local_.warning_scale),  config->warning);
  gtk_range_set_value (GTK_RANGE(local_.critical_scale), config->critical);
} /* </battery_settings_cancel> */

/*
 * battery_enable callback
 */
static void
battery_enable (GtkWidget *button, Modulus *applet)
{
  gboolean state = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(button));

  if (state != local_.enable) {
    GlobalPanel *panel = applet->data;
    PanelSetting *settings = panel->settings;

    /* Register callbacks for apply and cancel. */
    settings_save_enable (settings, TRUE);
    settings_set_agents (settings,
                         (gpointer)battery_settings_apply,
                         (gpointer)battery_settings_cancel,
                         NULL);

    local_.enable = state;
  }
} /* </battery_enable> */

/*
 * battery_interval
 * battery_warning
 * battery_critical
 */
static void
battery_interval (GtkRange *range, Modulus *applet)
{
  GlobalPanel *panel = applet->data;
  PanelSetting *settings = panel->settings;

  settings_save_enable (settings, TRUE);
  settings_set_agents (settings,
                       (gpointer)battery_settings_apply,
                       (gpointer)battery_settings_cancel,
                       NULL);

  local_.cache.interval = (int)gtk_range_get_value (range);
} /* </battery_interval> */

static void
battery_warning (GtkRange *range, Modulus *applet)
{
  GlobalPanel *panel = applet->data;
  PanelSetting *settings = panel->settings;

  settings_save_enable (settings, TRUE);
  settings_set_agents (settings,
                       (gpointer)battery_settings_apply,
                       (gpointer)battery_settings_cancel,
                       NULL);

  local_.cache.warning = (int)gtk_range_get_value (range);
} /* </battery_warning> */

static void
battery_critical (GtkRange *range, Modulus *applet)
{
  GlobalPanel *panel = applet->data;
  PanelSetting *settings = panel->settings;

  settings_save_enable (settings, TRUE);
  settings_set_agents (settings,
                       (gpointer)battery_settings_apply,
                       (gpointer)battery_settings_cancel,
                       NULL);

  local_.cache.critical = (int)gtk_range_get_value (range);
} /* </battery_critical> */

/*
 * (private) get_battery_state
 */
static gboolean
get_battery_state (BatteryState *battery)
{
  char line[FILENAME_MAX];
  char node[FILENAME_MAX];
  char *state = NULL;
  FILE *stream;

  /* Initialize BatteryState *battery using safe values. */
  battery->discharging = TRUE;
  battery->remaining   = 100;
  battery->capacity    = 100;

  sprintf(node, "%s/status", SYSBAT);

  if ((stream = fopen(node, "r")) != NULL) {
    if (fgets(line, FILENAME_MAX, stream)) {
      battery->discharging = (strncasecmp(line, "discharging", 11) == 0);
    }
    fclose(stream);

    sprintf(node, "%s/charge_now", SYSBAT);
    /* older kernel kernel versions used "energy_now" */
    if ((stream = fopen(node, "r")) == NULL)
      sprintf(node, "%s/energy_now", SYSBAT);

    if ((stream = fopen(node, "r")) != NULL) {
      if (fgets(line, FILENAME_MAX, stream)) {
        battery->remaining = atol(line);
      }
      fclose(stream);
    }

    sprintf(node, "%s/charge_full", SYSBAT);
    /* older kernel kernel versions used "energy_full" */
    if ((stream = fopen(node, "r")) == NULL)
      sprintf(node, "%s/energy_full", SYSBAT);

    if ((stream = fopen(node, "r")) != NULL) {
      if (fgets(line, FILENAME_MAX, stream)) {
        battery->capacity = atol(line);
      }
      fclose(stream);
    }
    state = line;  /* we are in a good state */
  }
  else if ((stream = fopen("/proc/acpi/battery/BAT0/state", "r")) != NULL) {
    while (fgets(line, FILENAME_MAX, stream)) {
      if (strstr(line, "charging state:")) {
        state = &line[strlen("charging state:")]; /* we are in a good state */

        while (*state == ' ') ++state;
        state[strlen(state) - 1] = (char)0;

        battery->discharging = (strncasecmp(state, "discharging", 11) == 0);
      }
      else if (strstr(line, "remaining capacity:")) {
        battery->remaining = atol(&line[strlen("remaining capacity:")]);
      }
    }
    fclose(stream);

    if ((stream = fopen("/proc/acpi/battery/BAT0/info", "r")) != NULL) {
      while (fgets(line, FILENAME_MAX, stream)) {
        if (strstr(line, "last full capacity:")) {
          battery->capacity = atol(&line[strlen("last full capacity:")]);
          break;
        }
      }
    }
    fclose(stream);
  }

  if (battery->capacity <= 0)   /* safeguard against divide by zero */
    battery->capacity = 100;

  if (battery->remaining <= 0)
    battery->remaining = 100;

  return (state != NULL);
} /* </get_battery_state> */

/*
 * battery_refresh - refresh view of the canvas area (and tooltips)
 */
static void
battery_refresh (GtkWidget *canvas, gpointer data)
{
  BatteryState battery;

  if (get_battery_state (&battery)) {
    char text[BATTERY_FORMAT_MAX];
    guint level = 100.0 * battery.remaining / battery.capacity;

    if (battery.discharging) {
      sprintf(text, "%d%c %s", level, '%', _("discharging"));
      redraw_pixbuf (canvas, local_.dischargingIcon);
    }
    else {
      sprintf(text, "%d%c %s", level, '%', _("charging"));
      redraw_pixbuf (canvas, local_.chargingIcon);
    }

    gtk_tooltips_set_tip (local_.tooltips, local_.layout, text, NULL);
    vdebug(2, "battery %s\n", text);
  }
} /* </battery_refresh> */

/*
 * (private) battery_command
 * (private) battery_monitor
 * (private) start_battery_monitor
 */
static gboolean
battery_command (gchar *command)
{
  g_spawn_command_line_sync(command, 0, 0, 0, 0);
  return FALSE;
}

static gboolean
battery_monitor (BatteryConfig *config)
{
  BatteryState battery;

  if (get_battery_state (&battery)) {
    if (battery.discharging) {
      guint level = 100.0 * battery.remaining / battery.capacity;

      if (level < config->critical) {
        if (strlen(config->command) > 0)
          g_timeout_add (1000, (GSourceFunc)battery_command, config->command);

        notice(NULL, ICON_ERROR, "%s: %s %d%c %s.", Program,
               _("battery level critical"), level, '%', _("charge left"));
      }
      else if (level < config->warning)
        notice(NULL, ICON_WARNING, "%s: %s %d%c %s.", Program,
               _("battery level low"), level, '%', _("charge left"));
    }
    battery_refresh (local_.canvas, NULL);
  }

  return (config->interval > 0);  /* FALSE => stop monitoring */
} /* </battery_monitor> */

static gboolean
start_battery_monitor (BatteryConfig *config)
{
  /* Set checking status every 'interval' minutes. */
  g_timeout_add (1000 * 60 * config->interval,
                 (GSourceFunc)battery_monitor,
                 config);

  return FALSE;
} /* </start_battery_monitor> */

/*
 * module_settings provides configuration pages
 */
GtkWidget *
module_settings (Modulus *applet, GlobalPanel *panel)
{
  BatteryConfig *config = local_.config;	/* persistent settings */

  GtkWidget *area, *frame, *layer, *widget;
  GtkWidget *layout = gtk_vbox_new (FALSE, 2);

  gchar *caption = g_strdup_printf ("%s [%s]", _(applet->label), _("plugin"));

  /* Initialize private data structure singleton settings cache. */
  memcpy(&local_.cache, config, sizeof(BatteryConfig));
  local_.enable = applet->enable;

  /* Construct settings page. */
  frame = gtk_frame_new (caption);
  gtk_frame_set_shadow_type (GTK_FRAME(frame), GTK_SHADOW_ETCHED_IN);
  gtk_container_set_border_width (GTK_CONTAINER (frame), 10);
  gtk_container_add (GTK_CONTAINER(layout), frame);
  gtk_widget_show (frame);
  g_free (caption);

  area = gtk_vbox_new (FALSE, 2);
  gtk_container_add (GTK_CONTAINER(frame), area);
  gtk_widget_show (area);

  /* Add option to enable or disable battery. */
  layer = gtk_hbox_new (FALSE, 6);
  gtk_box_pack_start (GTK_BOX(area), layer, FALSE, FALSE, 6);
  gtk_widget_show (layer);

  widget = gtk_label_new ("");
  gtk_box_pack_start (GTK_BOX(layer), widget, FALSE, TRUE, 6);
  gtk_widget_show (widget);

  widget = gtk_check_button_new_with_label (_("Enable"));
  gtk_box_pack_start (GTK_BOX(layer), widget, FALSE, TRUE, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(widget), applet->enable);
  gtk_widget_show (local_.enable_toggle = widget);

  g_signal_connect (G_OBJECT(widget), "toggled",
                    G_CALLBACK(battery_enable), applet);

  if (applet->icon != NULL) {
    const gchar *icon = icon_path_finder (panel->icons, applet->icon);

    if (icon != NULL) {
      const guint size = 32;
      GtkWidget *image = image_new_from_file_scaled (icon, size, size);

      gtk_box_pack_end (GTK_BOX(layer), image, FALSE, FALSE, 12);
      gtk_widget_show (image);
    }
  }

  /* Add slider for warning threshold. */
  layer = gtk_hbox_new (FALSE, 6);
  gtk_box_pack_start (GTK_BOX(area), layer, FALSE, FALSE, 0);
  gtk_widget_show (layer);

  caption = g_strdup_printf ("\t%s:", _("Warning threshold"));
  widget = gtk_label_new (caption);
  gtk_box_pack_start (GTK_BOX(layer), widget, FALSE, FALSE, 0);
  gtk_widget_show (widget);
  g_free (caption);

  widget = local_.warning_scale = gtk_hscale_new_with_range (10, 60, 1);
  gtk_box_pack_start (GTK_BOX(layer), widget, FALSE, TRUE, 0);
  gtk_range_set_value (GTK_RANGE(widget), config->warning);
  gtk_widget_set_size_request (GTK_WIDGET(widget), 100, 40);
  gtk_scale_set_digits (GTK_SCALE (widget), 0);
  gtk_widget_show (widget);

  g_signal_connect (G_OBJECT (widget), "value_changed",
                    G_CALLBACK (battery_warning), applet);

  caption = g_strdup_printf ("%s.", _("percent charge"));
  widget = gtk_label_new (caption);
  gtk_box_pack_start (GTK_BOX(layer), widget, FALSE, FALSE, 0);
  gtk_widget_show (widget);
  g_free (caption);

  /* Add slider for critical threshold. */
  layer = gtk_hbox_new (FALSE, 6);
  gtk_box_pack_start (GTK_BOX(area), layer, FALSE, FALSE, 0);
  gtk_widget_show (layer);

  caption = g_strdup_printf ("\t%s:", _("Critical threshold"));
  widget = gtk_label_new (caption);
  gtk_box_pack_start (GTK_BOX(layer), widget, FALSE, FALSE, 0);
  gtk_widget_show (widget);
  g_free (caption);

  widget = local_.critical_scale = gtk_hscale_new_with_range (5, 20, 1);
  gtk_box_pack_start (GTK_BOX(layer), widget, FALSE, TRUE, 0);
  gtk_range_set_value (GTK_RANGE(widget), config->critical);
  gtk_widget_set_size_request (GTK_WIDGET(widget), 100, 40);
  gtk_scale_set_digits (GTK_SCALE(widget), 0);
  gtk_widget_show (widget);

  g_signal_connect (G_OBJECT (widget), "value_changed",
                    G_CALLBACK (battery_critical), applet);

  caption = g_strdup_printf ("%s.", _("percent charge"));
  widget = gtk_label_new (caption);
  gtk_box_pack_start (GTK_BOX(layer), widget, FALSE, FALSE, 0);
  gtk_widget_show (widget);
  g_free (caption);

  /* Add slider for monitor interval. */
  layer = gtk_hbox_new (FALSE, 6);
  gtk_box_pack_start (GTK_BOX(area), layer, FALSE, FALSE, 0);
  gtk_widget_show (layer);

  caption = g_strdup_printf ("\t%s:", _("Monitor interval"));
  widget = gtk_label_new (caption);
  gtk_box_pack_start (GTK_BOX(layer), widget, FALSE, FALSE, 0);
  gtk_widget_show (widget);
  g_free (caption);

  widget = local_.interval_scale = gtk_hscale_new_with_range (1, 10, 1);
  gtk_box_pack_start (GTK_BOX(layer), widget, FALSE, TRUE, 0);
  gtk_range_set_value (GTK_RANGE(widget), config->interval);
  gtk_widget_set_size_request (GTK_WIDGET(widget), 100, 40);
  gtk_scale_set_digits (GTK_SCALE(widget), 0);
  gtk_widget_show (widget);

  g_signal_connect (G_OBJECT (widget), "value_changed",
                    G_CALLBACK (battery_interval), applet);

  caption = g_strdup_printf ("%s.", _("minutes"));
  widget = gtk_label_new (caption);
  gtk_box_pack_start (GTK_BOX(layer), widget, FALSE, FALSE, 0);
  gtk_widget_show (widget);
  g_free (caption);

  return layout;
} /* </module_settings> */

/*
 * Required methods
 */
void
module_init (Modulus *applet)
{
  GlobalPanel *panel = applet->data;
  BatteryConfig *config = g_new0 (BatteryConfig, 1);
  FILE *stream;

  /* Modulus structure initialization. */
  applet->name    = "battery";
  applet->icon    = "battery.png";
  applet->place   = PLACE_END;
  applet->space   = MODULI_SPACE_TASKBAR;
  applet->release = Release;
  applet->authors = Authors;

  /* Read configuration data for the battery applet. */
  battery_configuration_read (applet, config);

  /* Initialize private data structure singleton. */
  memset(&local_, 0, sizeof (BatteryPrivate));

  local_.tooltips = gtk_tooltips_new();
  local_.config = config;

  /* Construct the settings pages. */
  applet->label       = "Battery";
  applet->description = _(
"Shows the charged battery level and warns when levels reach save thresholds."
);
  applet->settings = settings_notebook_new (applet, panel,
                       _("Settings"), module_settings (applet, panel),
                       _("About"), settings_manpage_new (applet, panel),
                       -1);

  if ((stream = fopen("/sys/class/power_supply/BAT0/status", "r")) == NULL) {
    stream = fopen("/sys/class/power_supply/BAT1/status", "r");
    SYSBAT = "/sys/class/power_supply/BAT1";
  }

  if (stream == NULL) {
    stream = fopen("/proc/acpi/battery/BAT0", "r");
    SYSBAT = "//proc/acpi/battery/BAT0";
  }

  if (stream != NULL) {
    vdebug(1, "battery monitor activated\n");
    fclose(stream);
  }
  else {	/* if no battery, disengage applet by brute force */
    vdebug(1, "battery not detected\n");
    applet->space = 0;
  }
} /* </module_init> */

/*
 * Optional methods
 */
void
module_open (Modulus *applet)
{
  GtkWidget *canvas, *layout;
  BatteryConfig *config = local_.config;

  GlobalPanel *panel = applet->data;
  PanelIcons  *icons = panel->icons;

  guint size = icons->size;
  const gchar *icon;

  icon = icon_path_finder (icons, "power.png");
  local_.chargingIcon = pixbuf_new_from_file_scaled (icon, size, size);

  icon = icon_path_finder (icons, applet->icon);
  local_.dischargingIcon = pixbuf_new_from_file_scaled (icon, size, size);

  /* Construct the user interface. */
  layout = local_.layout = gtk_hbox_new (FALSE, 2);
  canvas = local_.canvas = gtk_drawing_area_new ();

  gtk_widget_set_size_request (GTK_WIDGET (canvas), size, size);
  gtk_box_pack_start (GTK_BOX(layout), canvas, FALSE, FALSE, 0);
  gtk_widget_show (canvas);

  /* Signals used to handle backing pixmap */
  g_signal_connect (G_OBJECT (canvas), "expose_event",
                    G_CALLBACK (battery_refresh), NULL);

  /* Start monitoring battery state. */
  g_timeout_add (1000, (GSourceFunc)start_battery_monitor, config);
  vdebug(2, "g_timeout_add 1000, start_battery_monitor\n");

  applet->widget = layout;
} /* module_open */

void
module_close (Modulus *applet)
{
  gtk_widget_destroy (applet->widget);
} /* module_close */
