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

#include "gould.h"
#include "gpanel.h"
#include "module.h"

extern const char *Program;	/* see, gpanel.c */
extern const char *Authors;	/* .. */

/*
* Data structures used by this module.
*/
typedef struct _MountdConfig  MountdConfig;
typedef struct _MountdPrivate MountdPrivate;

struct _MountdConfig
{
  gboolean automount;
  guint interval;		/* update interval in seconds */
};

struct _MountdPrivate
{
  gboolean enable;

  MountdConfig *config;		/* applet configuration data */
  gboolean automount;		/* local configuration data */
  guint interval;

  GtkWidget *enable_toggle;	/* settings panel widgets */
  GtkWidget *automount_toggle;
  GtkWidget *interval_scale;
  GtkWidget *menu;		/* mounted devices menu */

  GtkTooltips *tooltips;	/* applet tooltips widget */
  GList *internal;		/* internal disk partitions list */
  GList *mtab;			/* removable devices mount list */

  const gchar *icon_cdrom;	/* frequently used icons */
  const gchar *icon_usbdrive;
  const gchar *icon_removable;
};

static MountdPrivate local_;	/* private global structure singleton */


/*
 * mountd_configuration_read
 */
static void
mountd_configuration_read (Modulus *applet, MountdConfig *config)
{
  GlobalPanel *panel = applet->data;
  ConfigurationNode *chain = configuration_find (panel->config, "modules");
  ConfigurationNode *item;

  const gchar *attrib;


  /* Configuration for the mountd applet. */
  applet->enable = TRUE;
  config->interval = 2;		/* hardcoded check interval fallback */

  while ((item = configuration_find (chain, "applet")) != NULL) {
    attrib = configuration_attrib (item, "name");

    if (strcmp(attrib, applet->name) == 0) {
      if ((attrib = configuration_attrib (item, "icon")) != NULL)
        applet->icon = attrib;

      if ((attrib = configuration_attrib (item, "enable")) != NULL) {
        if (strcmp(attrib, "off") == 0 || strcmp(attrib, "no") == 0)
          applet->enable = FALSE;
      }

      if ((item = configuration_find (item, "settings")) != NULL) {
        if ((attrib = configuration_attrib (item, "automount")) != NULL)
          config->automount = (strcmp(attrib, "yes") == 0);

        if ((attrib = configuration_attrib (item, "interval")) != NULL)
          config->interval = atoi(attrib);
      }
      break;
    }
    chain = configuration_find_next (chain, "applet");
  }
} /* </mountd_configuration_read> */

/*
 *
 */
static gboolean
check_removable_mounts (GlobalPanel *panel)
{
  DeviceInfo *dev;
  GList *iter, *list;

  if (local_.menu) { 		/* check presence of removables menu */
    if (GTK_WIDGET_VISIBLE (local_.menu))
      return TRUE;

    gtk_widget_destroy (local_.menu); /* free previously made menu */
    local_.menu = NULL;
  }

  if (local_.mtab) {		/* free previously allocated mtab */
    for (iter = local_.mtab; iter; iter = iter->next) {
      dev = iter->data;
      free (dev->fsname);
      free (dev->mntdir);
      free (dev);
    }

    g_list_free (local_.mtab);
    local_.mtab = NULL;
  }

  /* get_mounted_devices (exclude => local_.internal) */
  list = get_mounted_devices (local_.internal);

  if (list) { 			/* mounted removable devices */
    GtkWidget *item, *image;
    PanelIcons  *icons = panel->icons;
    guint iconsize = panel->taskbar->iconsize;
    const gchar *icon;
    unsigned short word;

    /* Construct the menu of mounted removable devices. */
    local_.menu = gtk_menu_new();

    for (iter = list; iter; iter = iter->next) {
      dev  = iter->data;
      item = gtk_image_menu_item_new_with_label (dev->mntdir);
      word = dev->capability;

      if (word > 0 && (word & DEV_REMOVABLE)) {
        local_.mtab = g_list_append (local_.mtab, dev);

        if (word & DEV_DRIVERFS)
          icon = icon_path_finder (icons, local_.icon_cdrom);
        else
          icon = icon_path_finder (icons, local_.icon_usbdrive);

        image = image_new_from_file_scaled (icon, iconsize, iconsize);
        gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);

        gtk_menu_shell_append (GTK_MENU_SHELL(local_.menu), item);
        gtk_widget_show (item);
      }
      else {
        free (dev->fsname);  /* free elements that we do not use */
        free (dev->mntdir);
        free (dev);
      }
    }
    g_list_free (list);
  }

  return (local_.mtab != NULL);
} /* </check_removable_mounts> */

/*
 * (private) monitor_removable_devices
 */
static gboolean
monitor_removable_devices (Modulus *applet)
{
  if (local_.enable) {
    if (check_removable_mounts (applet->data)) /* TRUE => mounted removables */
      gtk_widget_show (applet->widget);
    else
      gtk_widget_hide (applet->widget);
  }

  return TRUE;
} /* </monitor_removable_devices> */

/*
 * (private) show_mounted
 */
static void
activate (GtkWidget *button, GdkEventButton *event, GlobalPanel *panel)
{
  if (event->button == 1)
    gtk_menu_popup (GTK_MENU (local_.menu), NULL, NULL, startmenu, panel,
                      event->button, event->time);
} /* </activate> */

/*
 * mountd_settings_apply
 * mountd_settings_cancel
 */
static void
mountd_settings_apply (Modulus *applet)
{
  gboolean state = local_.enable;

  GlobalPanel *panel = applet->data;
  MountdConfig *config = local_.config;

  const char *spec;
  gchar *data;


  /* Show or hide pager widget according to user selection. */
  if (state)
    gtk_widget_show (applet->widget);
  else
    gtk_widget_hide (applet->widget);

  /* Save configuration settings in cache. */
  spec =
"<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
"<applet name=\"%s\" icon=\"%s\"%s>\n"
" <settings automount=\"%s\" />\n"
"</applet>\n";

  data = g_strdup_printf (spec, applet->name, applet->icon,
                          (state) ? "" : " enable=\"no\"",
                          (local_.automount) ? "yes" : "no"
                         );
  vdebug (3, "\n%s\n", data);

  /* Replace configuration item. */
  configuration_replace (panel->config, data, "modules", "applet",applet->name);
  g_free (data);

  /* Memory was freed when original item was removed, read configuration. */
  mountd_configuration_read (applet, config);

  /* Save configuration cache data. */
  local_.enable = applet->enable;
  local_.automount = config->automount;
  local_.interval = config->interval;
} /* </mountd_settings_apply> */

static void
mountd_settings_cancel (Modulus *applet)
{
  gboolean state = applet->enable;
  MountdConfig *config = local_.config;

  /* Show or hide pager widget according to previous setting. */
  if (state)
    gtk_widget_show (applet->widget);
  else
    gtk_widget_hide (applet->widget);

  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(local_.enable_toggle), state);

  /* Restore previous settings. */
  local_.enable = applet->enable;
  local_.automount = config->automount;
  local_.interval = config->interval;
} /* </mountd_settings_cancel> */

/*
 * (private) mountd_enable
 */
static void
mountd_enable (GtkWidget *button, Modulus *applet)
{
  gboolean state = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(button));

  if (state != local_.enable) {
    GlobalPanel *panel = applet->data;
    PanelSetting *settings = panel->settings;

    /* Register callbacks for apply and cancel. */
    settings_save_enable (settings, TRUE);
    settings_set_agents (settings,
                         (gpointer)mountd_settings_apply,
                         (gpointer)mountd_settings_cancel,
                         NULL);

    local_.enable = state;
  }
} /* </mountd_enable> */

/*
 * (private) automount_enable
 */
static void
automount_enable (GtkWidget *button, Modulus *applet)
{
  gboolean state = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(button));

  if (state != local_.automount) {
    GlobalPanel *panel = applet->data;
    PanelSetting *settings = panel->settings;

    /* Register callbacks for apply and cancel. */
    settings_save_enable (settings, TRUE);
    settings_set_agents (settings,
                         (gpointer)mountd_settings_apply,
                         (gpointer)mountd_settings_cancel,
                         NULL);

    local_.automount = state;
  }
} /* </automount_enable> */

/*
 * module_settings provides configuration pages
 */
GtkWidget *
module_settings (Modulus *applet, GlobalPanel *panel)
{
  MountdConfig *config = local_.config;        /* persistent settings */
  GtkWidget    *frame, *area, *layer, *widget;
  GtkWidget    *layout = gtk_vbox_new (FALSE, 2);

  gchar *caption = g_strdup_printf ("%s [%s]", _(applet->label), _("plugin"));

  /* Initialize private data structure singleton. */
  local_.enable = applet->enable;

  /* Construct settings page. */
  frame = gtk_frame_new (caption);
  gtk_frame_set_shadow_type (GTK_FRAME(frame), GTK_SHADOW_ETCHED_IN);
  gtk_container_set_border_width (GTK_CONTAINER (frame), 10);
  gtk_container_add (GTK_CONTAINER(layout), frame);
  gtk_widget_show (frame);
  g_free (caption);

  area = gtk_vbox_new (FALSE, 4);
  gtk_container_add (GTK_CONTAINER(frame), area);
  gtk_widget_show (area);

  widget = gtk_label_new ("");
  gtk_box_pack_start (GTK_BOX(area), widget, FALSE, TRUE, 6);
  gtk_widget_show (widget);

  /* Add option to enable or disable mountd. */
  layer = gtk_hbox_new (FALSE, 6);
  gtk_box_pack_start (GTK_BOX(area), layer, FALSE, FALSE, 0);
  gtk_widget_show (layer);

  widget = gtk_label_new ("");
  gtk_box_pack_start (GTK_BOX(layer), widget, FALSE, TRUE, 6);
  gtk_widget_show (widget);

  widget = gtk_check_button_new_with_label (_("Enable"));
  gtk_box_pack_start (GTK_BOX(layer), widget, FALSE, TRUE, 0);
  gtk_widget_show (local_.enable_toggle = widget);

  g_signal_connect (G_OBJECT(widget), "toggled",
                    G_CALLBACK(mountd_enable), applet);

  /* Add checkbox to enable (or not) automount. */
  layer = gtk_hbox_new (FALSE, 6);
  gtk_box_pack_start (GTK_BOX(area), layer, FALSE, FALSE, 0);
  gtk_widget_show (layer);

  widget = gtk_check_button_new_with_label (_("Automount"));
  gtk_box_pack_start (GTK_BOX(layer), widget, FALSE, TRUE, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(widget), config->automount);
  gtk_widget_show (local_.automount_toggle = widget);

  g_signal_connect (G_OBJECT(widget), "toggled",
                    G_CALLBACK(automount_enable), applet);

  return layout;
} /* </module_settings> */

/*
 * Required methods
 */
void
module_init (Modulus *applet)
{
  GlobalPanel *panel = applet->data;
  MountdConfig *config = g_new0 (MountdConfig, 1);

  /* Modulus structure initialization. */
  applet->name    = "mountd";
  applet->icon    = "mountd.png";
  applet->place   = PLACE_END;
  applet->space   = MODULI_SPACE_TASKBAR;
  applet->release = "1.0";
  applet->authors = Authors;

  /* Read configuration data for the splash screen. */
  mountd_configuration_read (applet, config);

  /* Initialize private data structure singleton. */
  memset(&local_, 0, sizeof (MountdPrivate));

  local_.config = config;
  local_.enable = applet->enable;
  local_.automount = config->automount;
  local_.interval = config->interval;

  local_.internal = get_internal_partitions();
  local_.tooltips = gtk_tooltips_new();

  /* Initialize the frequently used icons. */
  local_.icon_cdrom = "cdrom.png";
  local_.icon_usbdrive = "usbdrive.png";
  local_.icon_removable = "removable.png";

  /* Construct the settings pages. */
  applet->label = "Removables";
  applet->description = _(
"Monitors when removable storage devices are mounted."
);
#if 0
  applet->settings = settings_notebook_new (applet, panel,
                       _("Settings"), module_settings (applet, panel),
                       _("About"), settings_manpage_new (applet, panel),
                       -1);
#endif
} /* </module_init> */

void
module_open (Modulus *applet)
{
  GtkWidget *image, *layout;
  GlobalPanel *panel = applet->data;
  PanelIcons  *icons = panel->icons;
  unsigned int iconsize = icons->size;
  const gchar *icon;

  /* Construct the user interface. */
  applet->widget = layout = gtk_toggle_button_new();
  gtk_button_set_relief (GTK_BUTTON(layout), GTK_RELIEF_NONE);

  icon = icon_path_finder (icons, applet->icon);
  image = image_new_from_file_scaled (icon, iconsize, iconsize);
  gtk_button_set_image (GTK_BUTTON(layout), image);

  g_signal_connect (G_OBJECT(layout), "button-press-event",
                    G_CALLBACK (activate), panel);

  /* Timer to monitor mounted removable devices every <interval> seconds. */
  applet->enable = check_removable_mounts (panel);

  g_timeout_add (local_.interval * 1000,
                 (GSourceFunc)monitor_removable_devices, applet);

#if 0
  /* Update local_.enable_toggle on the settings page. */
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(local_.enable_toggle),
                                                  applet->enable);
  settings_save_enable (panel->settings, FALSE);
#endif
} /* module_open */

void
module_close (Modulus *applet)
{
  gtk_widget_destroy (applet->widget);
} /* module_close */
