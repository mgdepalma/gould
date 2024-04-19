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

#include "gould.h"      /* common package declarations */
#include "gpanel.h"
#include "tasklist.h"
#include "pager.h"

extern const char *Program, *Release;	/* see, gpanel.c */

/*
* Data structures used by this module.
*/
typedef struct _PagerConfig    PagerConfig;
typedef struct _TasklistConfig TasklistConfig;

struct _PagerConfig
{
  bool enable;				/* enable (or not) pager */

  guint workspaces;			/* number of virtual desktops */
  guint rows;				/* number of pager rows */

  ModulusPlace order;			/* left or right placement */
  GSList *stead;			/* left, right radio group */
};

struct _TasklistConfig
{
  bool enable;				/* enable (or not) tasklist */
  bool allspaces;			/* include allspaces (or not) */

  GtkFunction active_workspace_change;	/* active workspace change callback */
  gpointer active_workspace_data;

  TasklistGroupingType grouping;	/* tasklist grouping enum type */
};

/* Private data structures */
typedef struct _TaskbarPrivate TaskbarPrivate;

struct _TaskbarPrivate
{
  GtkWidget *pager_enable;	   /* pager enable (or not) check box */
  PagerConfig pager_cache;	   /* cache for configuration data, pager */
  PagerConfig *pager_config;	   /* applet configuration data, pager */

  GtkWidget *tasklist_enable;	   /* tasklist enable (or not) check box */
  TasklistConfig tasklist_cache;   /* cache for configuration data, tasklist */
  TasklistConfig *tasklist_config; /* applet configuration data, tasklist */
  Tasklist *tasklist;		   /* GREEN_TASKLIST container */

  GtkWidget *workspaces_scale;	   /* scale to adjust number of workspaces */
  GtkWidget *rows_scale;
};
static TaskbarPrivate local_;	   /* private structure singleton */

/*
* taskbar_config configuration of pager and tasklist
*/
void
taskbar_config (GlobalPanel *panel)
{
  static PagerConfig _pager_config;
  static TasklistConfig _tasklist_config;

  /* Initialize private data structure singleton. */
  memset(&_pager_config, 0, sizeof (PagerConfig));
  memset(&_tasklist_config, 0, sizeof (PagerConfig));
  memset(&local_, 0, sizeof (TaskbarPrivate));

  /* Initial fallback values for pager and tasklist. */
  _pager_config.rows = 1;

  _tasklist_config.allspaces = true;
  _tasklist_config.grouping  = TASKLIST_ALWAYS_GROUP;

  local_.tasklist_config = &_tasklist_config;
  local_.pager_config = &_pager_config;
} /* </taskbar_config> */

/*
* taskbar_initialize initialization of panel widgets
*/
void
taskbar_initialize (GlobalPanel *panel, GtkWidget *layout)
{
  GdkScreen *screen =  green_get_gdk_screen (panel->green);

  GtkWidget *actor;		/* quick launch icons box */
  GtkWidget *stack;		/* plugin module visuals */
  GtkWidget *tray;		/* systemtray icons */

  Modulus   *applet;		/* plugin module instance */
  Modulus   *pager = NULL;	/* pager module for delayed placement */
  GList     *iter;		/* plugin modules iterator */

  GtkWidget *vspacer;		/* vertical spacer */
  static const int vthickness = 2;
  int vheight, vwidth;


  /* Adjust for the orientation of taskbar. */
  if (panel->orientation == GTK_ORIENTATION_HORIZONTAL) {
    actor = gtk_hbox_new (FALSE, 1);
    stack = gtk_hbox_new (FALSE, 1);
    tray  = gtk_hbox_new (FALSE, 1);

    vheight = panel->thickness - 7;
    vwidth  = vthickness;
  }
  else {
    actor = gtk_vbox_new (FALSE, 1);
    stack = gtk_vbox_new (FALSE, 1);
    tray  = gtk_vbox_new (FALSE, 1);

    vheight = vthickness;
    vwidth  = panel->thickness - 7;
  }

  /* Prepend quick launch to layout. */
  panel->quicklaunch = actor;

  gtk_box_pack_start (GTK_BOX(layout), actor, FALSE, TRUE, 0);
  gtk_widget_show (actor);

  /* Connect systemtray with screen and tray widget. */
  systray_connect (panel->systray, screen, tray);

  gtk_box_pack_start (GTK_BOX(stack), tray, FALSE, FALSE, 0);
  gtk_widget_show (tray);

  /* Adjust packing position based on the applets placed at the end. */
  for (iter = panel->moduli; iter != NULL; iter = iter->next)
    if (((Modulus *)iter->data)->place == PLACE_END)
      panel->epos -= panel->icons->size;

  /* Activate all modules from panel->moduli */
  for (iter = panel->moduli; iter != NULL; iter = iter->next) {
    applet = (Modulus *)iter->data;

    if (applet->module_open)
      applet->module_open (applet);

    if (applet->widget) {
      if (applet->place == PLACE_CONTAINER)	/* tasklist module */
        gtk_box_pack_start (GTK_BOX(layout), applet->widget, TRUE, TRUE, 0);
      else if (applet->place == PLACE_START) {
        if (strcmp(applet->name, "pager") == 0)
          gtk_box_pack_end (GTK_BOX(actor), applet->widget, FALSE, FALSE, 0);
        else {
          gtk_box_pack_start (GTK_BOX(actor), applet->widget, FALSE, FALSE, 0);
          panel_update_pack_position (panel, applet);
        }
      }
      else if (applet->place == PLACE_END) {
        if (strcmp(applet->name, "pager") == 0)
          pager = applet;			/* postpone placement */
        else {
          gtk_box_pack_start (GTK_BOX(stack), applet->widget, FALSE, FALSE, 0);
          panel_update_pack_position (panel, applet);
        }
      }

      /* Proceed with placement, if applet->enable and (! PLACE_NONE) */
      if (applet->enable && applet->place != PLACE_NONE)
        gtk_widget_show (applet->widget);
    }
  }

  /* Place vertical spacer after quicklaunch. */
  vspacer = xpm_image_scaled (ICON_VSPACER, vwidth, vheight);
  gtk_box_pack_start (GTK_BOX(actor), vspacer, FALSE, FALSE, 8);
  gtk_widget_show (vspacer);

  if (pager) 	/* postponed pager (after tasklist) placement */
    gtk_box_pack_start (GTK_BOX(layout), pager->widget, FALSE, FALSE, 0);

  /* Place vertical spacer before systemtray. */
  vspacer = xpm_image_scaled (ICON_VSPACER, vwidth, vheight);
  gtk_box_pack_start (GTK_BOX(layout), vspacer, FALSE, FALSE, 8);
  gtk_widget_show (vspacer);

  /* Append plugin visuals to layout. */
  gtk_box_pack_end (GTK_BOX(layout), stack, FALSE, FALSE, 0);
  gtk_widget_show (stack);
} /* </taskbar_initialize> */

/*
* pager_configuration_read
*/
static void
pager_configuration_read (Modulus *applet, PagerConfig *config)
{
  GlobalPanel *panel = applet->data;

  ConfigurationNode *chain = configuration_find (panel->config, "modules");
  ConfigurationNode *item;

  const gchar *attrib;


  /* Configuration for the pager applet. */
  config->enable     = true;		/* fallback setting */
  config->order      = PLACE_END;
  config->workspaces = green_get_workspace_count (panel->green);

  while ((item = configuration_find (chain, "applet")) != NULL) {
    attrib = configuration_attrib (item, "name");

    if (strcmp(attrib, applet->name) == 0) {
      if ((attrib = configuration_attrib (item, "enable")) != NULL)
        if (strcmp(attrib, "off") == 0 || strcmp(attrib, "no") == 0)
          config->enable = false;

      if ((attrib = configuration_attrib (item, "icon")) != NULL)
        applet->icon = attrib;

      if ((attrib = configuration_attrib (item, "place")) != NULL) {
        if (strcmp(attrib, "START") == 0)
          config->order = PLACE_START;
      }

      if ((item = configuration_find (item, "settings")) != NULL) {
        if ((attrib = configuration_attrib (item, "workspaces")) != NULL)
          config->workspaces = atoi(attrib);

        if ((attrib = configuration_attrib (item, "rows")) != NULL)
          config->rows = atoi(attrib);
      }
      break;
    }
    chain = configuration_find_next (chain, "applet");
  }

  applet->enable = config->enable;	/* module overall enableness */
} /* </pager_configuration_read> */

/*
* pager_place_config
*/
static void
pager_place_config (PagerConfig *config)
{
  GSList *radio;
  radio = (config->order == PLACE_START) ? config->stead : config->stead->next;

  if (radio && radio->data) {
    GtkWidget *button = (GtkWidget *)radio->data;
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(button), TRUE);
  }
} /* </pager_place_config> */

/*
* pager_settings_apply
* pager_settings_cancel
*/
static void
pager_settings_apply (Modulus *applet)
{
  static char *spec =
"<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
"<applet name=\"%s\" icon=\"%s\"%s place=\"%s\">\n"
" <settings workspaces=\"%d\" rows=\"%d\" />\n"
"</applet>\n";

  GlobalPanel *panel = applet->data;
  PagerConfig *config = local_.pager_config;

  bool moved = (config->order != local_.pager_cache.order);
  static char data[MAX_PATHNAME];

  /* Save configuration settings from singleton cache settings. */
  memcpy(config, &local_.pager_cache, sizeof(PagerConfig));

  /* Save configuration settings in cache. */
  sprintf(data, spec, applet->name, applet->icon,
			((config->enable) ? "" : " enable=\"no\""),
			(config->order == PLACE_START) ? "START" : "END",
			config->workspaces, config->rows);
  vdebug (2, "\n%s\n", data);

  /* Replace configuration item. */
  configuration_replace (panel->config, data,
                         "modules", "applet", applet->name);

  /* See if any changes, read configuration. */
  pager_configuration_read (applet, config);

  /* Save configuration to singleton settings cache. */
  memcpy(&local_.pager_cache, config, sizeof(PagerConfig));

  /* Change workspace count and/or pager rows according to user selection. */
  green_change_workspace_count (panel->green, config->workspaces);
  pager_set_n_rows (GREEN_PAGER(applet->widget), config->rows);

  if (moved) {				/* position changed */
    applet->place = config->order;
  }
  panel_reconstruct (panel);		/* reconstruct taskbar */

  /* Show or hide pager widget according to user selection. */
  if (config->enable)
    gtk_widget_show (applet->widget);
  else
    gtk_widget_hide (applet->widget);
} /* </pager_settings_apply> */

static void
pager_settings_cancel (Modulus *applet)
{
  PagerConfig *config = local_.pager_config;

  /* Revert to previous settings. */
  memcpy(&local_.pager_cache, config, sizeof(PagerConfig));

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(local_.pager_enable),
                                config->enable);

  gtk_range_set_value (GTK_RANGE(local_.workspaces_scale), config->workspaces);
  gtk_range_set_value (GTK_RANGE(local_.rows_scale), config->rows);

  /* Set the radio button according to the configuration. */
  pager_place_config (config);

  /* Show or hide pager widget according to previous setting. */
  if (config->enable)
    gtk_widget_show (applet->widget);
  else
    gtk_widget_hide (applet->widget);
} /* </pager_settings_cancel> */

/*
* pager_enable
*/
static void
pager_enable (GtkWidget *button, Modulus *applet)
{
  gboolean state = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(button));

  if (state != local_.pager_cache.enable) {
    GlobalPanel *panel = applet->data;
    PanelSetting *settings = panel->settings;

    /* Register callbacks for apply and cancel. */
    settings_save_enable (settings, true);
    settings_set_agents (settings,
                         (gpointer)pager_settings_apply,
                         (gpointer)pager_settings_cancel,
                         NULL);

    local_.pager_cache.enable = state;
  }
} /* </pager_enable> */

/*
* pager_workspaces
* pager_rows
*/
static void
pager_workspaces (GtkRange *range, Modulus *applet)
{
  GlobalPanel *panel = applet->data;
  PanelSetting *settings = panel->settings;

  settings_save_enable (settings, true);
  settings_set_agents (settings,
                       (gpointer)pager_settings_apply,
                       (gpointer)pager_settings_cancel,
                       NULL);

  local_.pager_cache.workspaces = (int)gtk_range_get_value (range);
} /* </pager_workspaces> */

static void
pager_rows (GtkRange *range, Modulus *applet)
{
  GlobalPanel *panel = applet->data;
  PanelSetting *settings = panel->settings;

  settings_save_enable (settings, true);
  settings_set_agents (settings,
                       (gpointer)pager_settings_apply,
                       (gpointer)pager_settings_cancel,
                       NULL);

  local_.pager_cache.rows = (int)gtk_range_get_value (range);
} /* </pager_rows> */

/*
* pager_place_after
* pager_place_before
*/
static void
pager_place_after (GtkWidget *button, Modulus *applet)
{
  if (applet->settings) {
    GlobalPanel *panel = applet->data;
    PanelSetting *settings = panel->settings;

    settings_save_enable (settings, true);
    settings_set_agents (settings,
                         (gpointer)pager_settings_apply,
                         (gpointer)pager_settings_cancel,
                         NULL);
  }

  local_.pager_cache.order = PLACE_END;
} /* </pager_place_after> */

static void
pager_place_before (GtkWidget *button, Modulus *applet)
{
  if (applet->settings) {
    GlobalPanel *panel = applet->data;
    PanelSetting *settings = panel->settings;

    settings_save_enable (settings, true);
    settings_set_agents (settings,
                         (gpointer)pager_settings_apply,
                         (gpointer)pager_settings_cancel,
                         NULL);
  }

  local_.pager_cache.order = PLACE_START;
} /* </pager_place_before> */


/*
* pager_settings_new
*/
GtkWidget *
pager_settings_new (Modulus *applet, PagerConfig *config)
{
  GSList *radio = NULL;
  GtkWidget *area, *frame, *layer, *widget;
  GtkWidget *layout = gtk_vbox_new (FALSE, 2);

  static char caption[MAX_LABEL];
  sprintf(caption, "%s [%s]", _(applet->label), _("builtin"));

  /* Save configuration to singleton settings cache. */
  memcpy(&local_.pager_cache, config, sizeof(PagerConfig));

  /* Construct settings page. */
  frame = gtk_frame_new (caption);
  gtk_frame_set_shadow_type (GTK_FRAME(frame), GTK_SHADOW_ETCHED_IN);
  gtk_container_set_border_width (GTK_CONTAINER (frame), 10);
  gtk_container_add (GTK_CONTAINER(layout), frame);
  gtk_widget_show (frame);

  area = gtk_vbox_new (FALSE, 4);
  gtk_container_add (GTK_CONTAINER(frame), area);
  gtk_widget_show (area);

  widget = gtk_label_new ("");
  gtk_box_pack_start (GTK_BOX(area), widget, FALSE, TRUE, 6);
  gtk_widget_show (widget);

  /* Add option to enable or disable pager. */
  layer = gtk_hbox_new (FALSE, 6);
  gtk_box_pack_start (GTK_BOX(area), layer, FALSE, FALSE, 0);
  gtk_widget_show (layer);

  widget = gtk_label_new ("");
  gtk_box_pack_start (GTK_BOX(layer), widget, FALSE, TRUE, 6);
  gtk_widget_show (widget);

  widget = gtk_check_button_new_with_label (_("Enable"));
  gtk_box_pack_start (GTK_BOX(layer), widget, FALSE, TRUE, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(widget), config->enable);
  gtk_widget_show (local_.pager_enable = widget);

  g_signal_connect (G_OBJECT(widget), "toggled",
                    G_CALLBACK(pager_enable), applet);

  if (applet->icon != NULL) {
    GlobalPanel *panel = applet->data;
    const gchar *icon  = icon_path_finder (panel->icons, applet->icon);

    if (icon != NULL) {
      const guint size = 32;
      GtkWidget *image = image_new_from_file_scaled (icon, size, size);

      gtk_box_pack_end (GTK_BOX(layer), image, FALSE, FALSE, 12);
      gtk_widget_show (image);
    }
  }

  /* Add scale for number of workspaces. */
  layer = gtk_hbox_new (FALSE, 6);
  gtk_box_pack_start (GTK_BOX(area), layer, FALSE, FALSE, 0);
  gtk_widget_show (layer);

  sprintf(caption, "\t%s:", _("Number of workspaces"));
  widget = gtk_label_new (caption);
  gtk_box_pack_start (GTK_BOX(layer), widget, FALSE, TRUE, 2);
  gtk_widget_show (widget);

  widget = local_.workspaces_scale = gtk_hscale_new_with_range (1, 16, 1);
  gtk_box_pack_start (GTK_BOX(layer), widget, FALSE, TRUE, 0);
  gtk_range_set_value (GTK_RANGE(widget), config->workspaces);
  gtk_widget_set_size_request (GTK_WIDGET(widget), 100, 40);
  gtk_scale_set_digits (GTK_SCALE (widget), 0);
  gtk_widget_show (widget);

  g_signal_connect (G_OBJECT (widget), "value_changed",
                    G_CALLBACK (pager_workspaces), applet);

  /* Add scale for number of pager rows. */
  layer = gtk_hbox_new (FALSE, 6);
  gtk_box_pack_start (GTK_BOX(area), layer, FALSE, FALSE, 0);
  gtk_widget_show (layer);

  sprintf(caption, "\t%s:", _("Number of pager rows"));
  widget = gtk_label_new (caption);
  gtk_box_pack_start (GTK_BOX(layer), widget, FALSE, TRUE, 2);
  gtk_widget_show (widget);

  widget = local_.rows_scale = gtk_hscale_new_with_range (1, 4, 1);
  gtk_box_pack_start (GTK_BOX(layer), widget, FALSE, TRUE, 0);
  gtk_range_set_value (GTK_RANGE(widget), config->rows);
  gtk_widget_set_size_request (GTK_WIDGET(widget), 100, 40);
  gtk_scale_set_digits (GTK_SCALE (widget), 0);
  gtk_widget_show (widget);

  g_signal_connect (G_OBJECT (widget), "value_changed",
                    G_CALLBACK (pager_rows), applet);


  /* Add radio boxes for START or END placement. */
  layer = gtk_hbox_new (FALSE, 6);
  gtk_box_pack_start (GTK_BOX(area), layer, FALSE, FALSE, 6);
  gtk_widget_show (layer);

  sprintf(caption, "\t%s:", _("Placement"));
  widget = gtk_label_new (caption);
  gtk_box_pack_start (GTK_BOX(layer), widget, FALSE, TRUE, 2);
  gtk_widget_show (widget);

  widget = gtk_radio_button_new_with_label (radio, _("Ahead"));
  gtk_box_pack_start (GTK_BOX(layer), widget, FALSE, FALSE, 0);
  radio = g_slist_append (radio, widget);
  gtk_widget_show (widget);

  g_signal_connect (G_OBJECT(widget), "clicked",
                    G_CALLBACK(pager_place_before), applet);

  widget = gtk_radio_button_new_with_label (radio, _("After tasklist"));
  gtk_box_pack_start (GTK_BOX(layer), widget, FALSE, FALSE, 0);
  radio = g_slist_append (radio, widget);
  gtk_widget_show (widget);

  g_signal_connect (G_OBJECT(widget), "clicked",
                    G_CALLBACK(pager_place_after), applet);

  /* Store radio group list in both pager configurations. */
  config->stead = local_.pager_cache.stead = radio;
  pager_place_config (config);

  return layout;
} /* </pager_settings_new> */

/*
* pager_module_init
* pager_module_open
*/
void
pager_module_init (Modulus *applet, GlobalPanel *panel)
{
  PagerConfig *config = local_.pager_config;

  /* Modulus structure initialization. */
  applet->data    = panel;
  applet->name    = "pager";
  applet->icon    = "workspace.png";

  /* Read configuration data for the pager applet. */
  pager_configuration_read (applet, config);

  applet->place   = config->order;
  applet->space   = MODULI_SPACE_TASKBAR;
  applet->release = Release;
  applet->authors = Authors;

  /* Construct the settings pages. */
  applet->label       = "Pager";
  applet->description = _(
"Pager allows you to see and switch workspaces called virtual desktops."
);
  applet->settings = settings_notebook_new (applet, panel,
                        _("Settings"), pager_settings_new (applet, config),
                        _("About"), settings_manpage_new (applet, panel),
                        -1);
} /* </pager_module_init> */

void
pager_module_open (Modulus *applet)
{
  GlobalPanel *panel = applet->data;
  PagerConfig *config = local_.pager_config;
  Pager *pager = pager_new (panel->green);

  pager_set_n_rows (pager, config->rows);
  green_change_workspace_count (panel->green, config->workspaces);
  pager_set_orientation (pager, panel->orientation);
  pager_set_shadow_type (pager, GTK_SHADOW_IN);

  applet->widget = GTK_WIDGET (pager);
} /* </pager_module_open> */

/*
* tasklist_configuration_read
*/
static void
tasklist_configuration_read (Modulus *applet, TasklistConfig *config)
{
  GlobalPanel *panel = applet->data;

  ConfigurationNode *chain = configuration_find (panel->config, "modules");
  ConfigurationNode *item;

  const gchar *attrib;


  /* Configuration for the tasklist applet. */
  config->enable = true;		/* fallback setting */

  while ((item = configuration_find (chain, "applet")) != NULL) {
    attrib = configuration_attrib (item, "name");

    if (strcmp(attrib, applet->name) == 0) {
      if ((attrib = configuration_attrib (item, "icon")) != NULL)
        applet->icon = attrib;

      if ((attrib = configuration_attrib (item, "enable")) != NULL)
        if (strcmp(attrib, "off") == 0 || strcmp(attrib, "no") == 0)
          config->enable = false;

      if ((item = configuration_find (item, "settings")) != NULL) {
        if ((attrib = configuration_attrib (item, "allspaces")) != NULL)
          config->allspaces = atoi(attrib);

        if ((attrib = configuration_attrib (item, "grouping")) != NULL)
          config->grouping = atoi(attrib);
      }
      break;
    }
    chain = configuration_find_next (chain, "applet");
  }

  applet->enable = config->enable;	/* module overall enableness */
} /* </tasklist_configuration_read> */

/*
* tasklist_settings_apply
* tasklist_settings_cancel
*/
static void
tasklist_settings_apply (Modulus *applet)
{
  static char *spec =
"<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
"<applet name=\"%s\" icon=\"%s\"%s>\n"
" <settings allspaces=\"%d\" grouping=\"%d\">\n"
" </settings>\n"
"</applet>\n";

  GlobalPanel *panel = applet->data;
  TasklistConfig *config = local_.tasklist_config;
  Tasklist *tasklist = GREEN_TASKLIST (applet->widget);
  static char data[MAX_PATHNAME];

  /* Save configuration settings from singleton cache settings. */
  memcpy(config, &local_.tasklist_cache, sizeof(TasklistConfig));

  /* Save configuration settings in cache. */
  sprintf(data, spec, applet->name, applet->icon,
                          ((config->enable) ? "" : " enable=\"no\""),
                          config->allspaces, config->grouping);
  vdebug (2, "\n%s\n", data);

  /* Replace configuration item. */
  configuration_replace (panel->config, data,
                         "modules", "applet", applet->name);

  /* See if any changes, read configuration. */
  tasklist_configuration_read (applet, config);

  /* Save configuration to singleton settings cache. */
  memcpy(&local_.tasklist_cache, config, sizeof(TasklistConfig));

  /* Change allspaces and/or grouping according to user selection. */
  tasklist_set_include_all_workspaces (tasklist, config->allspaces);
  tasklist_set_grouping (tasklist, config->grouping);

  /* Show or hide pager widget according to user selection. */
  if (config->enable)
    gtk_widget_show (applet->widget);
  else
    gtk_widget_hide (applet->widget);
} /* </tasklist_settings_apply> */

static void
tasklist_settings_cancel (Modulus *applet)
{
  TasklistConfig *config = local_.tasklist_config;

  /* Revert to previous settings. */
  memcpy(&local_.tasklist_cache, config, sizeof(TasklistConfig));

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(local_.tasklist_enable),
                                config->enable);

  /* Show or hide pager widget according to previous setting. */
  if (config->enable)
    gtk_widget_show (applet->widget);
  else
    gtk_widget_hide (applet->widget);
} /* </tasklist_settings_cancel> */

/*
* tasklist_module_enable
*/
static void
tasklist_module_enable (GtkWidget *button, Modulus *applet)
{
  gboolean state = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(button));

  if (state != local_.tasklist_cache.enable) {
    GlobalPanel *panel = applet->data;
    PanelSetting *settings = panel->settings;

    /* Register callbacks for apply and cancel. */
    settings_save_enable (settings, true);
    settings_set_agents (settings,
                         (gpointer)tasklist_settings_apply,
                         (gpointer)tasklist_settings_cancel,
                         NULL);

    local_.tasklist_cache.enable = state;
  }
} /* </tasklist_enable> */

/*
* tasklist_module_allspaces
* tasklist_module_grouping
*/
static void
tasklist_module_allspaces (GtkWidget *button, Modulus *applet)
{
  gboolean state = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button));

  if (state != local_.tasklist_cache.allspaces) {
    GlobalPanel *panel = applet->data;
    PanelSetting *settings = panel->settings;

    /* Register callbacks for apply and cancel. */
    settings_save_enable (settings, true);
    settings_set_agents (settings,
                         (gpointer)tasklist_settings_apply,
                         (gpointer)tasklist_settings_cancel,
                         NULL);

    local_.tasklist_cache.allspaces = state;
  }
} /* </tasklist_module_allspaces> */

static void
tasklist_module_grouping (GtkComboBox *options, Modulus *applet)
{
  TasklistGroupingType grouping = gtk_combo_box_get_active (options);

  if (grouping != local_.tasklist_cache.grouping) {
    GlobalPanel *panel = applet->data;
    PanelSetting *settings = panel->settings;

    /* Register callbacks for apply and cancel. */
    settings_save_enable (settings, true);
    settings_set_agents (settings,
                         (gpointer)tasklist_settings_apply,
                         (gpointer)tasklist_settings_cancel,
                         NULL);

    local_.tasklist_cache.grouping = grouping;
  }
} /* </tasklist_module_grouping> */

GtkWidget *
tasklist_settings_new (Modulus *applet, TasklistConfig *config)
{
  GtkWidget *area, *frame, *layer, *widget;
  GtkWidget *layout = gtk_vbox_new (FALSE, 2);

  static char caption[MAX_LABEL];
  sprintf(caption, "%s [%s]", _(applet->label), _("builtin"));

   /* Save configuration to singleton settings cache. */
  memcpy(&local_.tasklist_cache, config, sizeof(TasklistConfig));

  /* Construct settings page. */
  frame = gtk_frame_new (caption);
  gtk_frame_set_shadow_type (GTK_FRAME(frame), GTK_SHADOW_ETCHED_IN);
  gtk_container_set_border_width (GTK_CONTAINER (frame), 10);
  gtk_container_add (GTK_CONTAINER(layout), frame);
  gtk_widget_show (frame);

  area = gtk_vbox_new (FALSE, 4);
  gtk_container_add (GTK_CONTAINER(frame), area);
  gtk_widget_show (area);

  widget = gtk_label_new ("");
  gtk_box_pack_start (GTK_BOX(area), widget, FALSE, TRUE, 6);
  gtk_widget_show (widget);

  /* Add option to enable or disable tasklist. */
  layer = gtk_hbox_new (FALSE, 6);
  gtk_box_pack_start (GTK_BOX(area), layer, FALSE, FALSE, 0);
  gtk_widget_show (layer);

  widget = gtk_label_new ("");
  gtk_box_pack_start (GTK_BOX(layer), widget, FALSE, TRUE, 6);
  gtk_widget_show (widget);

  widget = gtk_check_button_new_with_label (_("Enable"));
  gtk_box_pack_start (GTK_BOX(layer), widget, FALSE, TRUE, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(widget), config->enable);
  gtk_widget_show (local_.tasklist_enable = widget);
  gtk_widget_set_sensitive (widget, FALSE);	/* disallow tasklist disable */

  g_signal_connect (G_OBJECT(widget), "toggled",
                    G_CALLBACK(tasklist_module_enable), applet);

  if (applet->icon != NULL) {
    GlobalPanel *panel = applet->data;
    const gchar *icon  = icon_path_finder (panel->icons, applet->icon);

    if (icon != NULL) {
      const guint size = 32;
      GtkWidget *image = image_new_from_file_scaled (icon, size, size);

      gtk_box_pack_end (GTK_BOX(layer), image, FALSE, FALSE, 12);
      gtk_widget_show (image);
    }
  }

  /* Add checkbox for including all desktop spaces. */
  layer = gtk_hbox_new (FALSE, 6);
  gtk_box_pack_start (GTK_BOX(area), layer, FALSE, FALSE, 0);
  gtk_widget_show (layer);

  widget = gtk_label_new ("");
  gtk_box_pack_start (GTK_BOX(layer), widget, FALSE, TRUE, 6);
  gtk_widget_show (widget);

  widget = gtk_check_button_new_with_label (_("Include all workspaces"));
  gtk_box_pack_start (GTK_BOX(layer), widget, FALSE, TRUE, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(widget), config->allspaces);
  gtk_widget_show (widget);

  g_signal_connect (G_OBJECT(widget), "toggled",
                    G_CALLBACK(tasklist_module_allspaces), applet);

  /* Add selection list for grouping. */
  layer = gtk_hbox_new (FALSE, 6);
  gtk_box_pack_start (GTK_BOX(area), layer, FALSE, FALSE, 0);
  gtk_widget_show (layer);

  widget = gtk_label_new ("");
  gtk_box_pack_start (GTK_BOX(layer), widget, FALSE, TRUE, 6);
  gtk_widget_show (widget);

  widget = gtk_label_new (_("Grouping"));
  gtk_box_pack_start (GTK_BOX(layer), widget, FALSE, TRUE, 0);
  gtk_widget_show (widget);

  widget = gtk_combo_box_new_text ();
  gtk_combo_box_append_text(GTK_COMBO_BOX(widget), _("Never group"));
  gtk_combo_box_append_text(GTK_COMBO_BOX(widget), _("Always group"));
  gtk_combo_box_append_text(GTK_COMBO_BOX(widget), _("Auto group"));

  gtk_combo_box_set_active(GTK_COMBO_BOX(widget), config->grouping);
  gtk_box_pack_start (GTK_BOX(layer), widget, FALSE, TRUE, 0);
  gtk_widget_show (widget);

  g_signal_connect (G_OBJECT(widget), "changed",
                    G_CALLBACK(tasklist_module_grouping), applet);

  return layout;
} /* </tasklist_settings_new> */

/*
* tasklist_module_open
*/
void
tasklist_module_open (Modulus *applet)
{
  GlobalPanel *panel = applet->data;
  Tasklist *tasklist = local_.tasklist = tasklist_new (panel->green);
  TasklistConfig *config = local_.tasklist_config;

  tasklist->active_workspace_changed_cb = config->active_workspace_change;
  tasklist->callback_data = config->active_workspace_data;

  tasklist_set_grouping (tasklist, config->grouping);
  tasklist_set_include_all_workspaces (tasklist, config->allspaces);
  tasklist_set_orientation (tasklist, panel->orientation);

  applet->widget = GTK_WIDGET (tasklist);
} /* </tasklist_module_open> */

/*
* tasklist_module_init
*/
void
tasklist_module_init (Modulus *applet, GlobalPanel *panel)
{
  TasklistConfig *config = local_.tasklist_config;

  /* Modulus structure initialization. */
  applet->data    = panel;
  applet->name    = "tasklist";
  applet->icon    = "tasklist.png";
  applet->place   = PLACE_CONTAINER;
  applet->space   = MODULI_SPACE_TASKBAR;
  applet->release = Release;
  applet->authors = Authors;

  /* Read configuration data for the tasklist applet. */
  tasklist_configuration_read (applet, config);

  /* Construct the settings pages. */
  applet->label       = "Tasklist";
  applet->description = _(
"Tasklist shows and allows you to manage the active tasks on the desktop."
);
  applet->settings = settings_notebook_new (applet, panel,
                        _("Settings"), tasklist_settings_new (applet, config),
                        _("About"), settings_manpage_new (applet, panel),
                        -1);
} /* </tasklist_module_init> */

/*
* tasklist_module_set_active_cb - add callback for active-workspace-changed
*/
void
tasklist_module_set_active_cb(GtkFunction active_change_cb, gpointer data)
{
  TasklistConfig *config = local_.tasklist_config;
  config->active_workspace_change = active_change_cb;
  config->active_workspace_data = data;
} /* </tasklist_module_set_active_cb> */
