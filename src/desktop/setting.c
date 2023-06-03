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

#include <X11/Xlib.h>
#include <X11/Xatom.h>

#include <gdk/gdkkeysyms.h>

const GdkColor ColorWhite = { 0, 65535, 65535, 65535 };

extern const char *Program, *Release, *Description;  /* see, gpanel.c */
extern const char *ConfigurationHeader, *Schema;     /* .. */
extern debug_t debug;				     /* .. */

static void
settings_menu_config (GlobalPanel *panel, ConfigurationNode *menu,
                      GtkTreeStore *store, GtkTreeIter *root);

static gboolean
settings_menu_select_iter (ConfigurationNode *node, GtkTreeIter *iter,
                           GtkTreeIter *parent);


/*
* Data structures used in implementation.
*/
typedef struct _GeneralSettings GeneralSettings;

typedef struct _MenuEditor      MenuEditor;
typedef struct _MenuEntry       MenuEntry;

struct _GeneralSettings
{
  GlobalPanel *panel;		/* reference to global data */

  GtkPositionType place;	/* user selected panel place */
  GSList *stead;		/* panel place radio buttons group */

  GtkWidget *thickness_scale;
  GtkWidget *margin_scale;
  GtkWidget *indent_scale;

  guint thickness;		/* thickness of the panel bar */
  guint margin;			/* margin at each end */
  guint indent;			/* indent at each end */
};

struct _MenuEntry
{
  gchar *name;			/* menu entry name */
  GdkPixbuf *icon;		/* menu entry icon */
  ConfigurationNode *node;	/* menu entry node */
  GtkTreeIter cell;		/* menu entry view */
};

struct _MenuEditor
{
  ConfigurationNode *cache;	/* menu options working copy */

  GtkTreeIter  root;		/* start of menu items */
  GtkTreeStore *store;		/* storage of menu items */
  GtkTreeView  *view;		/* access to menu items */

  MenuEntry entry;		/* pending menu item edit */
  MenuEntry paste;		/* pending menu item paste */
  MenuEntry *item;		/* selected menu item entry */
  guint iconsize;		/* icon size: width and height */

  GtkWidget *cut_button;	/* cut and paste buttons */
  GtkWidget *paste_button;
  GtkWidget *paste_frame;
  GtkWidget *paste_view;

  GtkWidget *insert_button;	/* insert and remove buttons */
  GtkWidget *remove_button;

  GtkWidget *icon_hbox;		/* menu item icon entry */
  GtkWidget *icon_button;

  GtkWidget *name_entry;	/* menu item name entry */
  GtkWidget *type_combo;	/* menu item type selection */

  GtkWidget *exec_hbox;		/* menu item exec entry */
  GtkWidget *exec_entry;
  GtkWidget *exec_method;	/* check box for internal program */

  gboolean change_icon;		/* pending menu item icon change */
  gboolean change_name;		/* pending menu item name change */
  gboolean change_exec;		/* pending menu item exec change */
  gboolean change_type;		/* pending menu item type change */

  gboolean change_menu;		/* pending main menu change */
};

/* Enumeration type for settings pages. */
enum {
  PAGE_COLUMN,
  NAME_COLUMN,
  DATA_COLUMN,
  N_COLUMNS
};

/* Enumeration type for menu editing. */
enum {
  MENU_ICON,
  MENU_NAME,
  MENU_DATA,
  MENU_COUNT
};

/* Enumeration for menu item type. */
enum {
  MENU_ITEM,
  MENU_SEPARATOR,
  MENU_SUBMENU,
  MENU_HEADER
};

static GeneralSettings general_;	/* singleton for general settings */
static GtkWidget *menuicondialog_;	/* panel->desktop->filer safe access */
static MenuEditor menueditor_;		/* singleton for menu editor data */


/*
* activate popup window for editing configuration settings
*/
void
settings_activate (GlobalPanel *panel)
{
  PanelDesktop *desktop = panel->desktop;
  gtk_widget_show (panel->settings->window);
  gtk_widget_hide (desktop->window);	 /* hide desktop panel */
  desktop->active = FALSE;
} /* </settings_activate> */

/*
* settings_apply
* settings_cancel
* settings_close
* settings_dismiss
* settings_key_press
*/
static void
settings_apply (GtkWidget *button, GlobalPanel *panel)
{
  PanelSetting *settings = panel->settings;

  if (settings->apply_cb != NULL)
    (*settings->apply_cb) (settings->applet);

  settings_save_enable (settings, FALSE);
} /* </settings_apply> */

static void
settings_cancel (GtkWidget *button, GlobalPanel *panel)
{
  PanelSetting *settings = panel->settings;

  if (settings->cancel_cb != NULL)
    (*settings->cancel_cb) (settings->applet);

  settings_save_enable (settings, FALSE);
} /* </settings_cancel> */

static void
settings_close (GtkWidget *button, GlobalPanel *panel)
{
  PanelDesktop *desktop  = panel->desktop;
  PanelSetting *settings = panel->settings;

  saveconfig (panel);	/* check for changes and save configuration */

  if (settings->close_cb != NULL)
    (*settings->close_cb) (settings->applet);

  gtk_widget_hide (desktop->filer);
  gtk_widget_hide (settings->window);
} /* </settings_close> */

/* static */void
settings_dismiss (GtkWidget *widget, GlobalPanel *panel)
{
  gtk_widget_hide (menuicondialog_);
  gtk_widget_hide (widget);
}

#if __key_press_event_
static void
settings_key_press (GtkWidget *window, GdkEventKey *event, GlobalPanel *panel)
{
  if (event->keyval == GDK_Escape)
    gtk_widget_hide (window);
} /* </settings_key_press> */
#endif

/*
* settings_save_enable
* settings_set_agents
*/
void
settings_save_enable (PanelSetting *settings, gboolean state)
{
  gtk_widget_set_sensitive (settings->apply, state);
  gtk_widget_set_sensitive (settings->cancel, state);
  gtk_widget_set_sensitive (settings->close, !state);
  gtk_widget_set_sensitive (settings->view, !state);
} /* </settings_save_enable> */

void
settings_set_agents (PanelSetting *settings,
                     GtkFunction apply_cb,
                     GtkFunction cancel_cb,
                     GtkFunction close_cb)
{
  settings->apply_cb  = apply_cb;
  settings->cancel_cb = cancel_cb;
  settings->close_cb  = close_cb;
} /* </settings_set_agents> */


/*
* settings_selection change callback
*/
static void
settings_selection (GtkTreeSelection *selection, GlobalPanel *panel)
{
  GtkTreeIter   cell;
  GtkTreeModel *model;

  if (gtk_tree_selection_get_selected (selection, &model, &cell)) {
    gint page;
    Modulus *applet;

    gtk_tree_model_get (model, &cell,
                        PAGE_COLUMN, &page,
                        DATA_COLUMN, &applet,
                        -1);

    if (page >= 0) {		/* set notebook current page */
      PanelSetting *settings = panel->settings;

      /* When changing applet invoke applicable close_cb. */
      if (applet != settings->applet && settings->close_cb)
        (*settings->close_cb) (settings->applet);

      gtk_notebook_set_current_page (settings->notebook, page);
      settings->applet = applet;
    }
  }
} /* </settings_selection> */

/*
* settings_about_new provides the information page
*/
Modulus *
settings_about_new ()
{
  GtkWidget *frame, *info;
  GtkWidget *layout = gtk_vbox_new (FALSE, 2);
  Modulus *applet = g_new0 (Modulus, 1);
  gchar *description;

  description = g_strdup_printf ("%s %s", Program, Release);
  frame = gtk_frame_new (description);
  gtk_frame_set_shadow_type (GTK_FRAME(frame), GTK_SHADOW_ETCHED_IN);
  gtk_container_set_border_width (GTK_CONTAINER (frame), 4);
  gtk_container_add (GTK_CONTAINER(layout), frame);
  gtk_widget_show (frame);
  g_free (description);

  description = g_strdup_printf (
                  "<span font_desc=\"Arial 14\">%s %s</span>",
                                      Program, Description);
  info = gtk_label_new (description);
  gtk_label_set_use_markup (GTK_LABEL (info), TRUE);
  gtk_misc_set_alignment (GTK_MISC(info), 0.06, 0.5);
  gtk_container_add (GTK_CONTAINER(frame), info);
  gtk_widget_show (info);
  g_free (description);

  applet->name = _("About");
  applet->settings = layout;

  return applet;
} /* </settings_about_new> */

/*
* panel_place_config
*/
static void
panel_place_config (GtkPositionType place)
{
  GtkWidget *button;
  GSList *iter = general_.stead;

  switch (place) {
     case GTK_POS_TOP:
       break;
     case GTK_POS_BOTTOM:
       iter = iter->next;
       break;
     case GTK_POS_LEFT:
       iter = iter->next->next;
       break;
     case GTK_POS_RIGHT:
       iter = iter->next->next->next;
       break;
  }

  button = (GtkWidget *)iter->data;
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(button), TRUE);
} /* </panel_place_config> */

/*
* panel_settings_apply
* panel_settings_cancel
*/
static void
panel_settings_apply (Modulus *applet)
{
  GlobalPanel *panel = applet->data;
  GtkPositionType place = general_.place; 

  ConfigurationNode *config = panel->config;
  ConfigurationNode *item;

  unsigned short changes = 0;
  const gchar *value = NULL;

  if (panel->place != place) {	/* see if panel place changed */
    item = configuration_find (config, "place");

    switch (place) {
      case GTK_POS_TOP:
        value = "TOP";
        break;
      case GTK_POS_BOTTOM:
        value = "BOTTOM";
        break;
      case GTK_POS_LEFT:
        value = "LEFT";
        break;
      case GTK_POS_RIGHT:
        value = "RIGHT";
        break;
    }

    g_free (item->element);
    item->element = g_strdup (value);

    ++changes;
  }
  if (panel->thickness != general_.thickness) {
    item = configuration_find (config, "thickness");

    g_free (item->element);
    item->element = g_strdup_printf ("%d", general_.thickness);

    ++changes;
  }
  if (panel->margin != general_.margin) {
    item = configuration_find (config, "margin");

    g_free (item->element);
    item->element = g_strdup_printf ("%d", general_.margin);

    ++changes;
  }
  if (panel->indent != general_.indent) {
    item = configuration_find (config, "indent");

    g_free (item->element);
    item->element = g_strdup_printf ("%d", general_.indent);

    ++changes;
  }

  /* Save user selected configuration settings. */
  panel->place     = general_.place;
  panel->thickness = general_.thickness;
  panel->margin    = general_.margin;
  panel->indent    = general_.indent;

  if (changes > 0) {
    reconstruct (panel);	/* reconstruct panel interface */
  }
} /* </panel_settings_apply> */

static void
panel_settings_cancel (Modulus *applet)
{
  GlobalPanel *panel = applet->data;

  panel_place_config (panel->place);
  gtk_range_set_value (GTK_RANGE(general_.thickness_scale), panel->thickness);
  gtk_range_set_value (GTK_RANGE(general_.margin_scale), panel->margin);
  gtk_range_set_value (GTK_RANGE(general_.indent_scale), panel->indent);

  /* Revert to previous settings. */
  general_.place     = panel->place;
  general_.thickness = panel->thickness;
  general_.margin    = panel->margin;
  general_.indent    = panel->indent;
} /* </panel_settings_cancel> */

/*
* panel_place callback
*/
static void
panel_place (GtkWidget *button, gpointer value)
{
  GlobalPanel *panel = general_.panel;
  PanelSetting *settings = panel->settings;

  if (settings->apply && settings->cancel && settings->close) {
    GtkPositionType place = GPOINTER_TO_INT(value); 

    /* Register callbacks for apply and cancel. */
    if (general_.place != place) {
      settings_save_enable (settings, TRUE);
      settings_set_agents (settings,
                           (gpointer)panel_settings_apply,
                           (gpointer)panel_settings_cancel,
                         NULL);

      general_.place = place;
    }
  }
} /* </panel_place> */

/*
* panel_thickness
* panel_margin
* panel_indent
*/
static void
panel_thickness (GtkRange *range, GlobalPanel *panel)
{
  PanelSetting *settings = panel->settings;

  settings_save_enable (settings, TRUE);
  settings_set_agents (settings,
                       (gpointer)panel_settings_apply,
                       (gpointer)panel_settings_cancel,
                       NULL);

  general_.thickness = (int)gtk_range_get_value (range);
} /* </panel_thickness> */

static void
panel_margin (GtkRange *range, GlobalPanel *panel)
{
  PanelSetting *settings = panel->settings;

  settings_save_enable (settings, TRUE);
  settings_set_agents (settings,
                       (gpointer)panel_settings_apply,
                       (gpointer)panel_settings_cancel,
                       NULL);

  general_.margin = (int)gtk_range_get_value (range);
} /* </panel_margin> */

static void
panel_indent (GtkRange *range, GlobalPanel *panel)
{
  PanelSetting *settings = panel->settings;

  settings_save_enable (settings, TRUE);
  settings_set_agents (settings,
                       (gpointer)panel_settings_apply,
                       (gpointer)panel_settings_cancel,
                       NULL);

  general_.indent = (int)gtk_range_get_value (range);
} /* </panel_indent> */

/*
* settings_general_new provides an under construction page
*/
Modulus *
settings_general_new (GlobalPanel *panel)
{
  GSList *radio = NULL;
  GtkWidget *box, *button, *frame, *layer, *view, *widget;
  GtkWidget *layout = gtk_hbox_new (FALSE, 4);

  Modulus *applet = g_new0 (Modulus, 1);
  Modulus *saver  = panel->xlock;	/* [special] screen saver module */

  gchar *value;


  /* Initialize applet data. */
  applet->name = _("General");
  applet->icon = "settings.png";
  applet->release = Release;
  applet->data = panel;

  /* Construct general settings layout. */
  /* view = inset_frame_new (layout, 5, _("Panel")); */
  view = inset_new (layout, 4);

  /* Add setting for panel position. */
  box = gtk_vbox_new (FALSE, 1);
  gtk_box_pack_start (GTK_BOX(view), box, FALSE, FALSE, 5);
  gtk_widget_show (box);

  frame = gtk_frame_new (_("Position"));
  gtk_container_add (GTK_CONTAINER(box), frame);
  gtk_widget_show (frame);

  layer = gtk_hbox_new (FALSE, 4);
  gtk_container_add (GTK_CONTAINER(frame), layer);
  gtk_widget_show (layer);

  widget = gtk_label_new (NULL);
  gtk_box_pack_start (GTK_BOX(layer), widget, FALSE, FALSE, 5);
  gtk_widget_show (widget);

  button = gtk_radio_button_new_with_label (radio, _("Top"));
  gtk_box_pack_start (GTK_BOX(layer), button, FALSE, FALSE, 0);
  radio = g_slist_append (radio, button);
  gtk_widget_show (button);

  g_signal_connect (G_OBJECT(button), "clicked",
                    G_CALLBACK(panel_place), GINT_TO_POINTER (GTK_POS_TOP));

  button = gtk_radio_button_new_with_label (radio, _("Bottom"));
  gtk_box_pack_start (GTK_BOX(layer), button, FALSE, FALSE, 0);
  radio = g_slist_append (radio, button);
  gtk_widget_show (button);

  g_signal_connect (G_OBJECT(button), "clicked",
                    G_CALLBACK(panel_place), GINT_TO_POINTER (GTK_POS_BOTTOM));

  button = gtk_radio_button_new_with_label (radio, _("Left"));
  gtk_box_pack_start (GTK_BOX(layer), button, FALSE, FALSE, 0);
  radio = g_slist_append (radio, button);
  gtk_widget_show (button);

  g_signal_connect (G_OBJECT(button), "clicked",
                    G_CALLBACK(panel_place), GINT_TO_POINTER (GTK_POS_LEFT));

  button = gtk_radio_button_new_with_label (radio, _("Right"));
  gtk_box_pack_start (GTK_BOX(layer), button, FALSE, FALSE, 0);
  radio = g_slist_append (radio, button);
  gtk_widget_show (button);

  g_signal_connect (G_OBJECT(button), "clicked",
                    G_CALLBACK(panel_place), GINT_TO_POINTER (GTK_POS_RIGHT));

  /* Set the active radio button according to panel->place */
  general_.panel = panel;
  general_.stead = radio;

  panel_place_config (panel->place);

  /* Initialize remaining saved settings. */
  general_.place     = panel->place;
  general_.thickness = panel->thickness;
  general_.margin    = panel->margin;
  general_.indent    = panel->indent;

  /* Add box for thickness, margin and indentation. */
  box = gtk_vbox_new (FALSE, 1);
  gtk_box_pack_start (GTK_BOX(view), box, FALSE, FALSE, 5);
  gtk_widget_show (box);

  frame = gtk_frame_new (NULL);
  gtk_container_add (GTK_CONTAINER(box), frame);
  gtk_widget_show (frame);

  layer = gtk_hbox_new (FALSE, 4);
  gtk_container_add (GTK_CONTAINER(frame), layer);
  gtk_widget_show (layer);

  view = gtk_vbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX(layer), view, FALSE, FALSE, 0);
  gtk_widget_show (view);

  /* Add setting for panel thickness. */
  layer = gtk_hbox_new (FALSE, 4);
  gtk_box_pack_start (GTK_BOX(view), layer, FALSE, FALSE, 0);
  gtk_widget_show (layer);

  value = g_strdup_printf ("%s:", _("Thickness"));
  widget = gtk_label_new (value);
  gtk_box_pack_start (GTK_BOX(layer), widget, FALSE, FALSE, 
                             32 - strlen(value));
  gtk_widget_show (widget);
  g_free (value);

  widget = general_.thickness_scale = gtk_hscale_new_with_range (30, 100, 1);
  gtk_box_pack_start (GTK_BOX(layer), widget, FALSE, TRUE, 0);
  gtk_range_set_value (GTK_RANGE(widget), panel->thickness);
  gtk_widget_set_size_request (GTK_WIDGET(widget), 100, 40);
  gtk_scale_set_digits (GTK_SCALE (widget), 0);
  gtk_widget_show (widget);

  g_signal_connect (G_OBJECT (widget), "value_changed",
                    G_CALLBACK (panel_thickness), panel);

  value = g_strdup_printf ("(%s)", _("height or width"));
  widget = gtk_label_new (value);
  gtk_box_pack_start (GTK_BOX(layer), widget, FALSE, FALSE, 4);
  gtk_widget_show (widget);
  g_free (value);

  /* Add setting for panel margins. */
  layer = gtk_hbox_new (FALSE, 4);
  gtk_box_pack_start (GTK_BOX(view), layer, FALSE, FALSE, 0);
  gtk_widget_show (layer);

  value = g_strdup_printf ("%s:", _("Margin"));
  widget = gtk_label_new (value);
  gtk_box_pack_start (GTK_BOX(layer), widget, FALSE, FALSE, 
                             32 - strlen(value));
  gtk_widget_show (widget);
  g_free (value);

  widget = general_.margin_scale = gtk_hscale_new_with_range (0, 200, 1);
  gtk_box_pack_start (GTK_BOX(layer), widget, FALSE, TRUE, 0);
  gtk_range_set_value (GTK_RANGE(widget), panel->margin);
  gtk_widget_set_size_request (GTK_WIDGET(widget), 100, 40);
  gtk_scale_set_digits (GTK_SCALE (widget), 0);
  gtk_widget_show (widget);

  g_signal_connect (G_OBJECT (widget), "value_changed",
                    G_CALLBACK (panel_margin), panel);

  value = g_strdup_printf ("(%s)", _("berth at start and end"));
  widget = gtk_label_new (value);
  gtk_box_pack_start (GTK_BOX(layer), widget, FALSE, FALSE, 4);
  gtk_widget_show (widget);
  g_free (value);

  /* Add setting for panel indentation. */
  layer = gtk_hbox_new (FALSE, 4);
  gtk_box_pack_start (GTK_BOX(view), layer, FALSE, FALSE, 0);
  gtk_widget_show (layer);

  value = g_strdup_printf ("%s:", _("Indentation"));
  widget = gtk_label_new (value);
  gtk_box_pack_start (GTK_BOX(layer), widget, FALSE, FALSE, 
                             32 - strlen(value));
  gtk_widget_show (widget);
  g_free (value);

  widget = general_.indent_scale = gtk_hscale_new_with_range (0, 30, 1);
  gtk_box_pack_start (GTK_BOX(layer), widget, FALSE, TRUE, 0);
  gtk_range_set_value (GTK_RANGE(widget), panel->indent);
  gtk_widget_set_size_request (GTK_WIDGET(widget), 100, 40);
  gtk_scale_set_digits (GTK_SCALE (widget), 0);
  gtk_widget_show (widget);

  g_signal_connect (G_OBJECT (widget), "value_changed",
                    G_CALLBACK (panel_indent), panel);

  value = g_strdup_printf ("(%s)", _("lead at start and end"));
  widget = gtk_label_new (value);
  gtk_box_pack_start (GTK_BOX(layer), widget, FALSE, FALSE, 4);
  gtk_widget_show (widget);
  g_free (value);

  /* Make notebook with: layout, screen saver and wallpaper. */
  if (saver && saver->settings)
    applet->settings = settings_notebook_new (applet, panel,
                         _("Settings"), layout,
                         _(saver->label), saver->settings,
                         _("Wallpaper"), setbg_settings_new(applet, panel),
                         -1);
  else
    applet->settings = settings_notebook_new (applet, panel,
                         _("Settings"), layout,
                         _("Wallpaper"), setbg_settings_new(applet, panel),
                         -1);

  return applet;
} /* </settings_general_new> */

/*
* settings_manpage_new provides layout man-page style
*/
GtkWidget *
settings_manpage_new (Modulus *applet, GlobalPanel *panel)
{
  gchar *label = (applet->label != NULL)
               ? g_strdup_printf ("%s (%s)", applet->label, applet->name)
               : g_strdup (applet->name);

  gchar *content = g_strdup_printf (
"<span font_desc=\"Sans 12\">\n"
"<b>%s</b>\n"
"\t\t%s\n\n"
"<b>%s</b>\n"
"\t\t%s\n\n"
"<b>%s</b>\n"
"\t\t%s\n\n"
"<b>%s</b>\n"
"\t\t%s\n"
"</span>",
          _("NAME"), label,
          _("DESCRIPTION"), applet->description,
          _("AUTHOR"), applet->authors,
          _("RELEASE"), applet->release);

  /* Build page layout. */
  GtkWidget *layout = gtk_label_new (content);
  gtk_label_set_line_wrap (GTK_LABEL (layout), TRUE);
  gtk_label_set_use_markup (GTK_LABEL (layout), TRUE);
  gtk_misc_set_alignment (GTK_MISC(layout), 0.01, 0.5);
  gtk_widget_show (layout);

  g_free (content);
  g_free (label);

  return layout;
} /* </settings_manpage_new> */

/*
* settings_missing_new builds an applet->settings missing page
*/
GtkWidget *
settings_missing_new (Modulus *applet, GlobalPanel *panel)
{
  gchar *text;

  GtkWidget *label;
  GtkWidget *layout = gtk_vbox_new (FALSE, 4);

  if (applet->icon != NULL) {
    const gchar *icon = icon_path_finder (panel->icons, applet->icon);

    if (icon != NULL) {
      const guint size = 32;
      GtkWidget *image = image_new_from_file_scaled (icon, size, size);
      gtk_box_pack_start (GTK_BOX(layout), image, FALSE, FALSE, 4);
      gtk_misc_set_alignment (GTK_MISC(image), 1, 0.5);
      gtk_widget_show (image);
    }
  }

  text = g_strdup_printf (
           "<span font_desc=\"Sans 12\">%s %s [%s]\n\n\t<b>(%s)</b></span>",
                 applet->name, applet->release,
                 (applet->module != NULL) ? "plugin" : "builtin",
                 _("configuration pages not implemented"));

  label = gtk_label_new (text);
  gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
  gtk_box_pack_start (GTK_BOX(layout), label, FALSE, FALSE, 0);
  gtk_misc_set_alignment (GTK_MISC(label), 0.1, 0.5);
  gtk_widget_show (label);
  g_free (text);

  return layout;
} /* </settings_missing_new> */

/*
* settings_notebook_new build an initial notebook layout
*/
GtkWidget *
settings_notebook_new (Modulus *applet, GlobalPanel *panel, ...)
{
  GtkWidget *label;
  GtkWidget *layout;
  GtkWidget *notebook;

  va_list ap;	/* variable arguments list pointer */
  void *arg;

  /* Create a new notebook, place the position of the tabs. */
  notebook = gtk_notebook_new ();
  gtk_notebook_set_tab_pos (GTK_NOTEBOOK (notebook), GTK_POS_TOP);

  /* Iterate arguments, finishing with -1 */
  va_start (ap, panel);
  arg = va_arg (ap, void *);

  while ((int)arg != -1) {
    label = gtk_label_new (arg);
    gtk_widget_show (layout = va_arg (ap, GtkWidget *));
    gtk_notebook_append_page (GTK_NOTEBOOK(notebook), layout, label);
    arg = va_arg (ap, void *);
  }
  va_end (ap);

  return notebook;
} /* </settings_notebook_new> */

/*
* settings_page_new build a page for the main notebook
*/
gint
settings_page_new (GtkTreeStore *store, GtkTreeIter *node,
                   GtkNotebook *notebook, Modulus *applet,
                   GlobalPanel *panel)
{
  gint page;
  const gchar *label = (applet->label != NULL) ? applet->label : applet->name;

  GtkWidget *book;
  GtkWidget *name;

  name = gtk_label_new (label);
  page = g_list_length (notebook->children);
  book = (applet->settings != NULL) ? applet->settings
                                    : settings_missing_new(applet, panel);

  gtk_notebook_append_page (notebook, book, name);
  gtk_widget_show (book);

  gtk_tree_store_set (store, node,
                      PAGE_COLUMN, page,
                      NAME_COLUMN, label,
                      DATA_COLUMN, applet,
                      -1);
  return page;
} /* </settings_page_new> */

#ifdef __settings_page_switch_
/*
* settings_page_switch handler for settings page change
*/
static void
settings_page_switch (GtkNotebook *book, GtkNotebookPage *page,
                      gint index, GlobalPanel *panel)
{
  g_return_if_fail (book != NULL);
  g_return_if_fail (page != NULL);
} /* </settings_page_switch> */
#endif

/*
* getSettingsPanelWidth - width of settings panel adjusted by resolution
*/
static unsigned short
getSettingsPanelWidth ()
{
  gint xres = green_screen_width ();
  unsigned short width;

  if (xres < 640)
    width = 95 * xres / 100;
  else if (xres < 800)
    width = 92 * xres / 100;
  else if (xres < 1024)
    width = 76 * xres / 100;
  else if (xres < 1280)
    width = 69 * xres / 100;
  else
    width = 64 * xres / 100;

  return width;
} /* </getSettingsPanelWidth> */

/*
* settings_new - popup window for editing configuration settings
*/
void
settings_new (GlobalPanel *panel)
{
  GtkWidget         *view;
  GtkTreeViewColumn *column;
  GtkCellRenderer   *renderer;
  GtkTreeSelection  *selection;
  GtkTreeStore      *store;
  GtkTreePath       *path;
  GtkTreeIter       child;
  GtkTreeIter       cell;

  GtkWidget *window, *frame, *splitview, *scrolled;
  GtkWidget *button, *layer, *layout, *widget;
  GtkNotebook *notebook;

  PanelSetting *settings = panel->settings = g_new0(PanelSetting, 1);
  gboolean screen_expand = TRUE;
  gboolean panel_expand  = TRUE;

  Modulus *applet;              /* plugin modules */
  GList *iter;

  gint width, height;		/* settings window preferred dimentions */
  gint xorg, yorg;		/* ... (x,y) origin coordinates */

  gint xres = green_screen_width();

  /* Adjust width and height according to screen resolution. */
  if (xres < 800)
    xorg = yorg = 5;
  else if (xres < 1024)
    xorg = yorg = 65;
  else if (xres < 1280)
    xorg = yorg = 100;
  else
    xorg = yorg = 150;

  /*
  width  = getSettingsPanelWidth();
  height = 50 * width / 100;
  */
  width = height = -1;

  /* Create a popup window with a frame (GDK_WINDOW_TYPE_HINT_MENU). */
  window = settings->window = sticky_window_new (GDK_WINDOW_TYPE_HINT_MENU,
                                                  width, height, xorg, yorg);
#ifndef BORDER_WIDTH
#define BORDER_WIDTH 6
#endif
  //gtk_container_set_border_width (GTK_CONTAINER (window), BORDER_WIDTH);

  gtk_window_set_decorated (GTK_WINDOW(window), TRUE);
  gtk_window_set_deletable (GTK_WINDOW(window), FALSE);
  gtk_window_set_keep_above (GTK_WINDOW(window), TRUE);
  gtk_window_set_resizable (GTK_WINDOW(window), TRUE);
  gtk_window_set_position (GTK_WINDOW (window), GTK_WIN_POS_CENTER);
  gtk_window_set_title (GTK_WINDOW(window), _("Control Panel"));

  g_signal_connect(G_OBJECT (window), "destroy",
                   G_CALLBACK (settings_dismiss), panel);

  g_signal_connect(G_OBJECT (window), "delete_event",
                   G_CALLBACK (settings_dismiss), panel);

#if __key_press_event_
  g_signal_connect(G_OBJECT (window), "key_press_event",
                   G_CALLBACK (settings_key_press), panel);
#endif

  frame = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME(frame), GTK_SHADOW_OUT);
  gtk_container_add (GTK_CONTAINER(window), frame);
  gtk_widget_show (frame);

  /* Create the container window layout. */
  splitview = gtk_hbox_new (FALSE, 2);
  gtk_container_add (GTK_CONTAINER(frame), splitview);
  gtk_widget_show (splitview);

  /* Add a scrolled window for the selections. */
  scrolled = gtk_scrolled_window_new (NULL, NULL);
  gtk_box_pack_start (GTK_BOX(splitview), scrolled, FALSE, FALSE, 0);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW(scrolled),
                                  GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_widget_show (scrolled);

  /* [selections]Left side of splitview. */
  store = gtk_tree_store_new (N_COLUMNS,
                              G_TYPE_INT,
                              G_TYPE_STRING,
                              G_TYPE_POINTER);

  view = settings->view = gtk_tree_view_new_with_model (GTK_TREE_MODEL(store));
  gtk_container_add (GTK_CONTAINER(scrolled), view);
  gtk_widget_show (view);

  column = gtk_tree_view_column_new();
  gtk_tree_view_column_set_title (column, _("Configuration"));
  gtk_tree_view_append_column (GTK_TREE_VIEW(view), column);

  renderer = gtk_cell_renderer_text_new();
  gtk_tree_view_column_pack_start (column, renderer, TRUE);
  gtk_tree_view_column_add_attribute (column, renderer, "text", NAME_COLUMN);

  /* [settings]Right side of splitview. */
  layout = gtk_vbox_new (FALSE, 1);
  gtk_box_pack_start (GTK_BOX(splitview), layout, TRUE, TRUE, 5);
  gtk_widget_show (layout);

  widget = gtk_notebook_new();
  notebook = settings->notebook = GTK_NOTEBOOK(widget);

  gtk_notebook_set_show_tabs (notebook, FALSE);
  gtk_notebook_set_show_border (notebook, FALSE);
  gtk_box_pack_start (GTK_BOX(layout), widget, TRUE, TRUE, 0);
  gtk_widget_show (widget);

  /* [0]First entry is for general settings. */
  gtk_tree_store_append (store, &cell, NULL);
  settings->applet = settings_general_new (panel);
  settings_page_new (store, &cell, notebook, settings->applet, panel);

  /* [1]Second entry are SCREEN applets. */
  gtk_tree_store_append (store, &cell, NULL);
  gtk_tree_store_set (store, &cell,
                      PAGE_COLUMN, -1,
                      NAME_COLUMN, _("Screen"),
                      -1);

  for (iter = panel->moduli; iter != NULL; iter = iter->next) {
    applet = (Modulus *)iter->data;

    if (applet->settings && applet->place == PLACE_SCREEN) {
      gtk_tree_store_append(store, &child, &cell);
      settings_page_new (store, &child, notebook, applet, panel);
    }
  }

  /* [2]Third entry is the menu editor. */
  gtk_tree_store_append (store, &cell, NULL);
  settings_page_new (store, &cell, notebook, panel->start, panel);

  /* [3]Fourth entry is the taskbar. */
  gtk_tree_store_append (store, &cell, NULL);
  gtk_tree_store_set (store, &cell,
                      PAGE_COLUMN, -1,
                      NAME_COLUMN, _("Modules"),
                      -1);

  for (iter = panel->moduli; iter != NULL; iter = iter->next) {
    applet = (Modulus *)iter->data;

    if (applet != panel->start && applet != panel->xlock)
      if (applet->place != PLACE_SCREEN && applet->settings) {
        gtk_tree_store_append (store, &child, &cell);
        settings_page_new (store, &child, notebook, applet, panel);
      }
  }

  /* [4]Last entry is for general information. */
  gtk_tree_store_append (store, &cell, NULL);
  settings_page_new (store, &cell, notebook, settings_about_new(), panel);

  /* Expand selections based on variable settings. */
  if (screen_expand) {
    path = gtk_tree_path_new_from_string ("1");
    gtk_tree_view_expand_row (GTK_TREE_VIEW(view), path, TRUE);
    gtk_tree_path_free (path);
  }
  if (panel_expand) {
    path = gtk_tree_path_new_from_string ("3");
    gtk_tree_view_expand_row (GTK_TREE_VIEW(view), path, TRUE);
    gtk_tree_path_free (path);
  }

  /* Add callback for selection change. */
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW(view));
  gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);

  g_signal_connect (selection, "changed",
                    G_CALLBACK(settings_selection), panel);

  /* Add apply, cancel and dismiss buttons. */
  layer = gtk_hbox_new (FALSE, 1);
  gtk_box_pack_start (GTK_BOX(layout), layer, FALSE, FALSE, 2);
  gtk_widget_show (layer);

  button = settings->close = xpm_button(ICON_CLOSE, _("Close"));
  gtk_box_pack_end (GTK_BOX(layer), button, FALSE, FALSE, 0);
  GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
  gtk_widget_grab_default (button);
  gtk_widget_show (button);

  g_signal_connect (G_OBJECT(button), "clicked",
                    G_CALLBACK(settings_close), panel);

  button = settings->apply = xpm_button(ICON_APPLY, _("Apply"));
  gtk_box_pack_end (GTK_BOX(layer), button, FALSE, FALSE, 0);
  gtk_widget_set_sensitive (button, FALSE);
  gtk_widget_show (button);

  g_signal_connect (G_OBJECT(button), "clicked",
                    G_CALLBACK(settings_apply), panel);

  button = settings->cancel = xpm_button(ICON_CANCEL, _("Cancel"));
  gtk_box_pack_end (GTK_BOX(layer), button, FALSE, FALSE, 0);
  gtk_widget_set_sensitive (button, FALSE);
  gtk_widget_show (button);

  g_signal_connect (G_OBJECT(button), "clicked",
                    G_CALLBACK(settings_cancel), panel);

  /* Set the initial notebook page. */
  gtk_notebook_set_current_page (notebook, 0);

#ifdef __SETTINGS_PAGE_SWITCH_
  g_signal_connect (G_OBJECT(notebook), "switch_page",
                    G_CALLBACK(settings_page_switch), panel);
#endif

  menuicondialog_ = panel->desktop->filer;
} /* </settings_new> */

/*
* settings_menu_apply_changes
*/
void
settings_menu_apply_changes (MenuEntry *item, GlobalPanel *panel)
{
  GtkTreeStore *store = menueditor_.store;
  ConfigurationAttrib *attrib;
  const gchar *value;

  if (menueditor_.change_icon) {
    PanelDesktop *desktop = panel->desktop;
    gboolean iconattr = FALSE;

    for (attrib = item->node->attrib; attrib != NULL; attrib = attrib->next)
      if (strcmp(attrib->name, "icon") == 0) {
        iconattr = TRUE;
        break;
      }

    if (iconattr == FALSE && strcmp(item->node->element, "header") == 0) {
      for (attrib = item->node->attrib; ; attrib = attrib->next)
        if (attrib->next == NULL) {
          attrib->next = g_new0(ConfigurationAttrib, 1);
          attrib->next->name = g_strdup ("icon");
          attrib->next->value = g_strdup (".");	/* fake attribute value */
          attrib = attrib->next;
          break;
        }

      iconattr = TRUE;
    }

    if (iconattr) {		/* change icon attribute as necessary */
      FileChooser *chooser = desktop->chooser;

      value = gtk_entry_get_text (GTK_ENTRY(chooser->name));

      if (strcmp(attrib->value, value) != 0) { 
        gint iconsize = menueditor_.iconsize;
        const gchar *file;
        GdkPixbuf *pixbuf;

        g_free (attrib->value);
        attrib->value = g_strdup (value);

        file = icon_path_finder (panel->icons, value);
        pixbuf = pixbuf_new_from_file_scaled (file, iconsize, iconsize);

        gtk_tree_store_set (store, &item->cell,
                            MENU_ICON, pixbuf,
                            -1);
      }
    }

    desktop->chooser_apply = NULL;
    menueditor_.change_icon = FALSE;
  }

  if (menueditor_.change_name) {
    for (attrib = item->node->attrib; attrib != NULL; attrib = attrib->next)
      if (strcmp(attrib->name, "name") == 0) {
        g_free (attrib->value);
        value = gtk_entry_get_text (GTK_ENTRY(menueditor_.name_entry));
        attrib->value = g_strdup (value);

        gtk_tree_store_set (store, &item->cell,
                            MENU_NAME, _(value),
                            -1);
        break;
      }

    menueditor_.change_name = FALSE;
  }

  if (menueditor_.change_exec) {
    ConfigurationNode *exec = configuration_find (item->node, "exec");

    if (exec != NULL) {
      const gchar *type = configuration_attrib (exec->back, "type");
      gboolean active = gtk_toggle_button_get_active
                                 (GTK_TOGGLE_BUTTON (menueditor_.exec_method));
      gboolean internal = FALSE;

      value = gtk_entry_get_text (GTK_ENTRY(menueditor_.exec_entry));

      if (strcmp(value, exec->element) != 0) {
        g_free (exec->element);
        exec->element = g_strdup (value);
      }

      if (type != NULL && strcmp(type, "method") == 0)
        internal = TRUE;

      if (active != internal) {
        ConfigurationNode *node = exec->back;
        configuration_attrib_remove (node->attrib);
        attrib = NULL;

        if (active) {
          attrib = g_new0(ConfigurationAttrib, 1);
          attrib->name = g_strdup ("type");
          attrib->value = g_strdup ("method");
        }

        node->attrib = attrib;
      }
    }

    menueditor_.change_exec = FALSE;
  }
} /* </settings_menu_apply_changes> */

/*
* settings_menu_reconstruct - reconstruct start menu view.
*/
static void
settings_menu_reconstruct (GlobalPanel *panel, ConfigurationNode *menu,
                           ConfigurationNode *node)
{
  GtkTreeIter      *root  = &menueditor_.root;
  GtkTreeStore     *store = menueditor_.store;
  GtkTreeView      *view  = menueditor_.view;
  GtkTreeSelection *seen  = gtk_tree_view_get_selection (view);

  /* Reconstruct start menu view interface. */
  gtk_tree_store_remove (store, root);
  settings_menu_config (panel, menu, store, root);
  gtk_tree_view_expand_all (view);

  if (node) {
    GtkTreeIter cell;
    settings_menu_select_iter (node, &cell, NULL);
  }
  else {
    gtk_tree_selection_select_iter (seen, root);
  }
} /* </settings_menu_reconstruct> */

/*
* settings_menu_apply
* settings_menu_cancel
*/
static void
settings_menu_apply (Modulus *applet)
{
  GlobalPanel  *panel = applet->data;
  PanelTaskbar *taskbar = panel->taskbar;

  ConfigurationNode *menu = configuration_find (panel->config, "menu");
  ConfigurationNode *mark = menu->back;
  ConfigurationNode *cache;

  gboolean point = menueditor_.change_icon ||
                   menueditor_.change_name ||
                   menueditor_.change_exec ||
                   menueditor_.change_menu;

  /* Hide cut and paste preview area. */
  if (menueditor_.paste_view != NULL) {
    GtkWidget *frame = menueditor_.paste_frame;
    gtk_container_remove (GTK_CONTAINER(frame), menueditor_.paste_view);
    menueditor_.paste_view = NULL;
    gtk_widget_hide (frame);
  }

  /* Disable menu item type selections. */
  gtk_widget_set_sensitive (menueditor_.type_combo, FALSE);
  menueditor_.change_type = FALSE;

  /* See if icon and/or name and/or exec were changed. */
  settings_menu_apply_changes (&menueditor_.entry, panel);
  menueditor_.change_menu = FALSE;

  /* Replace start menu options with menueditor_.cache */
  configuration_remove (menu);
  cache = configuration_clone (menueditor_.cache);
  configuration_insert (cache, mark, 0);

  if (point) { 		/* point to the menu item changed */
    const gchar *name = gtk_entry_get_text (GTK_ENTRY(menueditor_.name_entry));
    ConfigurationNode *node = configuration_find (menueditor_.cache, name);
    settings_menu_reconstruct (panel, menueditor_.cache, node);

    if (menueditor_.entry.node == menueditor_.cache)
      menu_config (panel->start, panel); 
  }
  else
    settings_menu_reconstruct (panel, menueditor_.cache, NULL);

  /* We need to reconstruct the start menu options. */
  taskbar->options = menu_options_config (menueditor_.cache, panel,
						taskbar->iconsize);
  if (debug > 1)
    configuration_write (panel->config, ConfigurationHeader, stdout);

  gtk_widget_set_sensitive (GTK_WIDGET(menueditor_.view), TRUE);
} /* </settings_menu_apply> */

static void
settings_menu_cancel (Modulus *applet)
{
  GlobalPanel *panel = applet->data;
  ConfigurationNode *menu = configuration_find (panel->config, "menu");
  ConfigurationNode *cache = configuration_clone (menu);

  /* Hide cut and paste preview area. */
  if (menueditor_.paste_view != NULL) {
    GtkWidget *frame = menueditor_.paste_frame;
    gtk_container_remove (GTK_CONTAINER(frame), menueditor_.paste_view);
    menueditor_.paste_view = NULL;
    gtk_widget_hide (frame);
  }

  /* Disable menu item type selections. */
  gtk_widget_set_sensitive (menueditor_.type_combo, FALSE);
  menueditor_.change_type = FALSE;

  /* Restore menueditor_.cache from start menu options. */
  configuration_remove (menueditor_.cache);
  menueditor_.cache = cache;

  /* Reconstruct start menu view. */
  settings_menu_reconstruct (panel, cache, NULL);

  gtk_widget_set_sensitive (GTK_WIDGET(menueditor_.view), TRUE);
} /* </settings_menu_cancel> */

/*
* settings_menu_enable
*/
static void
settings_menu_enable (GlobalPanel *panel)
{
  PanelSetting *settings = panel->settings;

  /* Hide cut and paste preview area. */
  if (menueditor_.paste_view != NULL) {
    GtkWidget *frame = menueditor_.paste_frame;
    gtk_container_remove (GTK_CONTAINER(frame), menueditor_.paste_view);
    menueditor_.paste_view = NULL;
    gtk_widget_hide (frame);
  }

  /* Enable save and cancel buttons. */
  settings_save_enable (settings, TRUE);
  settings_set_agents (settings,
                       (gpointer)settings_menu_apply,
                       (gpointer)settings_menu_cancel,
                       NULL);
} /* </settings_menu_enable> */

/*
* settings_menu_insert
*/
static void
settings_menu_insert (GtkWidget *button, GdkEventButton *ev, GlobalPanel *panel)
{
  MenuEntry *item = menueditor_.item;

  if (item != NULL) {
    gchar *stub =
"<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
"<item name=\"%s\" icon=\"%s\">\n"
" <exec>%s</exec>\n"
"</item>\n";

    gchar *spec = g_strdup_printf (stub, "New Item", "item.png", "gpanel");
    ConfigurationNode *node = configuration_read (spec, NULL, TRUE);
    ConfigurationNode *site = item->node;

    /* Reflect changes in menueditor_.cache */
    gint adjust = site->depth + 1;

    if (strcmp(site->element, "item") == 0) {
      adjust = site->depth;
      site   = configuration_find_end (item->node);
    }
    else if (strcmp(site->element, "header") == 0 ||
             strcmp(site->element, "separator") == 0) {
      adjust = site->depth;
      site   = site->next;
    }
    else if (strcmp(item->node->element, "header") == 0 ||
             strcmp(item->node->element, "separator") == 0) {
      adjust = site->depth;
    }

    configuration_insert (node, site, adjust);
    g_free (spec);

    /* Reflect changes in the interface. */
    settings_menu_reconstruct (panel, menueditor_.cache, node);

    /* Enable one time change menu item type selection. */
    gtk_widget_set_sensitive (menueditor_.type_combo, TRUE);
    gtk_widget_set_sensitive (GTK_WIDGET(menueditor_.view), FALSE);

    gtk_combo_box_set_active (GTK_COMBO_BOX(menueditor_.type_combo), MENU_ITEM);
    menueditor_.change_type = TRUE;
    settings_menu_enable (panel);	/* enable apply and cancel buttons */
  }
} /* </settings_menu_insert> */

/*
* settings_menu_cut
* settings_menu_paste
*/
static void
settings_menu_cut (GtkWidget *button, GdkEventButton *ev, GlobalPanel *panel)
{
  MenuEntry *item = menueditor_.item;

  if (item != NULL && item->node != menueditor_.cache) {
    PanelSetting *settings = panel->settings;

    GtkWidget *frame = menueditor_.paste_frame;
    GtkWidget *paste = menueditor_.paste_view;
    GtkWidget *widget;

    settings_save_enable (settings, TRUE);
    gtk_widget_set_sensitive (settings->apply, FALSE);
    settings_set_agents (settings,
                         (gpointer)settings_menu_apply,
                         (gpointer)settings_menu_cancel,
                         NULL);

    /* Update cut and paste preview area. */
    if (paste != NULL) {
      gtk_container_remove (GTK_CONTAINER(frame), paste);
    }
    gtk_widget_show (frame);

    paste = menueditor_.paste_view = gtk_hbox_new (FALSE, 4);
    gtk_container_add (GTK_CONTAINER(frame), paste);
    gtk_widget_show (paste);

    widget = gtk_image_new_from_pixbuf (item->icon);
    gtk_box_pack_start (GTK_BOX(paste), widget, FALSE, FALSE, 0);
    gtk_widget_show (widget);

    widget = gtk_label_new (item->name);
    gtk_box_pack_start (GTK_BOX(paste), widget, FALSE, FALSE, 0);
    gtk_widget_show (widget);

    /* Save the menu item selection. */
    memcpy(&menueditor_.paste, item, sizeof(MenuEntry));
  }
} /* </settings_menu_cut> */

static void
settings_menu_paste (GtkWidget *button, GdkEventButton *ev, GlobalPanel *panel)
{
  MenuEntry *item = menueditor_.item;

  if (item != NULL)
    if (menueditor_.paste_view != NULL) {
      ConfigurationNode *node = menueditor_.paste.node;
      ConfigurationNode *site = item->node;

      if (node == site)		/* avoid unnecessary work.. */
        return;

      /* Reflect changes in menueditor_.cache */
      gint adjust = 0;

      if (strcmp(site->element, "item") == 0)
        site = configuration_find_end (item->node);
      else if (strcmp(site->element, "submenu") == 0)
        adjust = 1;
      else if (site == menueditor_.cache)
        adjust = 1;

      configuration_move (node, site);

      if (adjust != 0) {		/* adjust nesting depth, if needed */
        ConfigurationNode *iter = node;
        ConfigurationNode *mark = configuration_find_end (node);

        for (mark = mark->next; iter != mark; iter = iter->next)
          iter->depth += adjust;
      }

      /* Reflect changes in the interface. */
      settings_menu_reconstruct (panel, menueditor_.cache, node);
      settings_menu_enable (panel);	/* enable apply and cancel buttons */
    }
} /* </settings_menu_paste> */

/*
* settings_menu_remove
*/
static void
settings_menu_remove (GtkWidget *button, GdkEventButton *ev, GlobalPanel *panel)
{
  MenuEntry *item = menueditor_.item;

  if (item != NULL && item->node != menueditor_.cache) {
    ConfigurationNode *node;
    GtkTreeIter cell;

    /* Reflect changes in menueditor_.cache */
    if ((node = configuration_remove (item->node)) == NULL)
      node = menueditor_.cache;

    /* Reflect changes in the interface. */
    gtk_tree_store_remove (menueditor_.store, &item->cell);
    settings_menu_select_iter (node, &cell, NULL);
    settings_menu_enable (panel);	/* enable apply and cancel buttons */
  }
} /* </settings_menu_remove> */

/*
* settings_menu_item change callback
*/
static void
settings_menu_item (GtkTreeSelection *selection, GlobalPanel *panel)
{
  static MenuEntry item;	/* current menu item selected */
  GtkTreeModel *model;

  if (gtk_tree_selection_get_selected (selection, &model, &item.cell)) {
    ConfigurationNode *node;
    GtkWidget *image;

    GtkWidget *exec_check = menueditor_.exec_method;
    GtkWidget *exec_entry = menueditor_.exec_entry;
    GtkWidget *name_entry = menueditor_.name_entry;
    GtkWidget *type_combo = menueditor_.type_combo;

    const gchar *icon = NULL;
    const gchar *name = "";
    const gchar *exec = "";

    gboolean enable = TRUE;	/* enable (or not) menu editing */
    gboolean internal = FALSE;
    gboolean isitem = FALSE;

    /* Disable menu item type selections. */
    gtk_widget_set_sensitive (type_combo, FALSE);
    menueditor_.change_type = FALSE;

    /* Store name, icon and configuration node. */
    gtk_tree_model_get (model, &item.cell,
                        MENU_ICON, &item.icon,
                        MENU_NAME, &item.name,
                        MENU_DATA, &item.node,
                        -1);

    menueditor_.item = &item;	/* internal access to selected menu item */
    node = item.node;

    if (node->next && strcmp(node->next->element, "header") == 0)
      enable = FALSE;

    if (strcmp(node->element, "separator") == 0) {
      gtk_combo_box_set_active (GTK_COMBO_BOX(type_combo), MENU_SEPARATOR);
      gtk_widget_set_sensitive (name_entry, FALSE);
    }
    else {
      if (strcmp(node->element, "header") == 0)
        gtk_combo_box_set_active (GTK_COMBO_BOX(type_combo), MENU_HEADER);
      else if (strcmp(node->element, "submenu") == 0)
        gtk_combo_box_set_active (GTK_COMBO_BOX(type_combo), MENU_SUBMENU);
      else {
        ConfigurationNode *item;
        const gchar       *type;

        item = configuration_find (node, "exec");
        type = configuration_attrib (item->back, "type");
        exec = item->element;

        if (type != NULL && strcmp(type, "method") == 0)
          internal = TRUE;

        gtk_combo_box_set_active (GTK_COMBO_BOX(type_combo), MENU_ITEM);
        isitem = TRUE;
      }

      name = configuration_attrib (node, "name");
      icon = configuration_attrib (node, "icon");

      if (icon == NULL && strcmp(node->element, "header") == 0)
        icon = (getuid() == 0) ? "administrator.png" : "user.png";

      gtk_widget_set_sensitive (name_entry, TRUE);
    }

    if (icon) {
      gtk_widget_set_sensitive (menueditor_.icon_hbox, TRUE);
      image = image_new_from_path_scaled (panel->icons->path, icon, 22, 22);
    }
    else {
      gtk_widget_set_sensitive (menueditor_.icon_hbox, FALSE);
      image = xpm_image_scaled (ICON_BLANK, 22, 22);
    }

    gtk_button_set_image (GTK_BUTTON(menueditor_.icon_button), image);
    gtk_widget_set_sensitive (menueditor_.exec_hbox, isitem);

    gtk_entry_set_text (GTK_ENTRY(name_entry), name);
    gtk_entry_set_text (GTK_ENTRY(exec_entry), exec);

    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(exec_check), internal);
    gtk_widget_set_sensitive (exec_check, isitem);

    if (strcmp(node->element, "header") == 0) {
      gtk_widget_set_sensitive (menueditor_.cut_button, FALSE);
      gtk_widget_set_sensitive (menueditor_.paste_button, FALSE);
      gtk_widget_set_sensitive (menueditor_.remove_button, FALSE);
    }
    else {
      gtk_widget_set_sensitive (menueditor_.cut_button, enable);
      gtk_widget_set_sensitive (menueditor_.paste_button, enable);
      gtk_widget_set_sensitive (menueditor_.remove_button, enable);
    }

    gtk_widget_set_sensitive (menueditor_.insert_button, enable);
  }
  else {
    menueditor_.item = NULL;
  }
} /* </settings_menu_item> */

/*
* change menu item type callback
*/
static void
settings_menu_itemtype (GtkComboBox *combo, GlobalPanel *panel)
{
  int selection = gtk_combo_box_get_active (combo);

  if (menueditor_.change_type && selection >= 0) {
    MenuEntry *item = menueditor_.item;

    ConfigurationNode *node = item->node;
    ConfigurationNode *site = node->back;
    ConfigurationNode *exec;

    gchar *stub;
    gchar *spec = NULL;
    gint  nest;

    switch (selection) {
      case MENU_HEADER:
        stub = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n%s\n";
        spec = g_strdup_printf (stub, "<header name=\"%u\" />");

        gtk_widget_set_sensitive (menueditor_.icon_hbox, FALSE);
        gtk_widget_set_sensitive (menueditor_.name_entry, FALSE);
        gtk_widget_set_sensitive (menueditor_.exec_hbox, FALSE);
        break;

      case MENU_SEPARATOR:
        stub = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n%s\n";
        spec = g_strdup_printf (stub, "<separator />");

        gtk_widget_set_sensitive (menueditor_.icon_hbox, FALSE);
        gtk_widget_set_sensitive (menueditor_.name_entry, FALSE);
        gtk_widget_set_sensitive (menueditor_.exec_hbox, FALSE);
        break;

      case MENU_ITEM:
        exec = configuration_find (node, "exec");

        stub =
"<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
"<item name=\"%s\" icon=\"%s\">\n"
" <exec>%s</exec>\n"
"</item>\n";
        spec = g_strdup_printf (stub, "New Item", "item.png", exec->element);
        break;

      case MENU_SUBMENU:
        stub =
"<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
"<submenu name=\"%s\" icon=\"%s\">\n"
"</submenu>\n";
        spec = g_strdup_printf (stub, "New Menu", "folder.png");
        gtk_widget_set_sensitive (menueditor_.exec_hbox, FALSE);
        break;
    }

    /* Disable menu item type selections. */
    gtk_widget_set_sensitive (menueditor_.type_combo, FALSE);
    menueditor_.change_type = FALSE;

    /* Reflect changes in menueditor_.cache */
    if (site == menueditor_.cache || selection == MENU_SUBMENU)
      nest = site->depth + 1;
    else
      nest = site->depth;

    configuration_remove (node);
    node = configuration_read (spec, NULL, TRUE);
    configuration_insert (node, site, nest);

    /* Reflect changes in the interface. */
    settings_menu_reconstruct (panel, menueditor_.cache, node);
    settings_menu_enable (panel);	/* enable apply and cancel buttons */
    MFREE(spec);
  }
} /* </settings_menu_itemtype> */

static void
settings_menu_itemexec (GtkToggleButton *button, GlobalPanel *panel)
{
  ConfigurationNode *node = menueditor_.item->node;
  ConfigurationNode *item = configuration_find (node, "exec");;
  const gchar       *type = configuration_attrib (item->back, "type");

  gboolean active = gtk_toggle_button_get_active (button);
  gboolean internal = FALSE;

  if (type != NULL && strcmp(type, "method") == 0)
    internal = TRUE;

  if (active != internal) {
    memcpy(&menueditor_.entry, menueditor_.item, sizeof(MenuEntry));
    gtk_widget_set_sensitive (GTK_WIDGET(menueditor_.view), FALSE);
    menueditor_.change_exec = TRUE;
    settings_menu_enable (panel);	/* enable apply and cancel buttons */
  }
} /* </settings_menu_itemexec> */

/*
* restore system configuration callback
*/
void
settings_menu_restore (GtkWidget *button,GdkEventButton *ev, GlobalPanel *panel)
{
  ConfigurationNode *menu = configuration_find (panel->sysconfig, "menu");
  ConfigurationNode *cache = configuration_clone (menu);

  /* Restore the system wide configuration. */
  configuration_remove (menueditor_.cache);
  menueditor_.cache = cache;

  /* Reflect changes in the interface. */
  settings_menu_reconstruct (panel, cache, NULL);

  /* Just in case the main menu button was changed. */
  memcpy(&menueditor_.entry, menueditor_.item, sizeof(MenuEntry));
  menueditor_.change_menu = TRUE;
  settings_menu_enable (panel);		/* enable apply and cancel buttons */
} /* </settings_menu_restore> */

/*
* settings_menu_icon_change
*/
static void
settings_menu_icon_change (GlobalPanel *panel)
{
  PanelDesktop *desktop = panel->desktop;
  FileChooser  *chooser = desktop->chooser;
  const gchar *icon = gtk_entry_get_text (GTK_ENTRY(chooser->name));
  GtkWidget *image;

  image = image_new_from_path_scaled (panel->icons->path, icon, 22, 22);
  gtk_button_set_image (GTK_BUTTON(menueditor_.icon_button), image);
  gtk_widget_set_sensitive (GTK_WIDGET(menueditor_.view), FALSE);
  menueditor_.change_icon = TRUE;
  settings_menu_enable (panel);		/* enable apply and cancel buttons */
} /* </settings_menu_icon_change> */

/*
* (private) settings_menu_icon
* (private) settings_menu_name
* (private) settings_menu_exec
*/
void
settings_menu_icon (GtkWidget *button, GlobalPanel *panel)
{
  MenuEntry *item = menueditor_.item;
  PanelDesktop *desktop = panel->desktop;
  FileChooser  *chooser = desktop->chooser;
  const gchar *icon = configuration_attrib (item->node, "icon");

  if (icon == NULL && strcmp(item->node->element, "header") == 0)
    icon = (getuid() == 0) ? "administrator.png" : "user.png";

  memcpy(&menueditor_.entry, item, sizeof(MenuEntry));
  gtk_entry_set_text (GTK_ENTRY(chooser->name), icon);
  desktop->chooser_apply = (gpointer)settings_menu_icon_change;
  desktop->render = item->icon;

  gtk_widget_show (desktop->filer);
} /* </settings_menu_icon> */

static gboolean
settings_menu_name (GtkEntry *entry, GdkEventKey *event, GlobalPanel *panel)
{
  MenuEntry *item = menueditor_.item;
  const gchar *name = gtk_entry_get_text (entry);
  
  memcpy(&menueditor_.entry, item, sizeof(MenuEntry));
  menueditor_.change_name = strcmp(item->name, name);

  if (menueditor_.change_name) {
    gtk_widget_set_sensitive (GTK_WIDGET(menueditor_.view), FALSE);
    settings_menu_enable (panel);	/* enable apply and cancel buttons */
  }
  return FALSE;
} /* </settings_menu_name> */

static gboolean
settings_menu_exec (GtkEntry *entry, GdkEventKey *event, GlobalPanel *panel)
{
  MenuEntry *item = menueditor_.item;
  ConfigurationNode *exec = configuration_find (item->node, "exec");
  const gchar *name = gtk_entry_get_text (entry);
  
  memcpy(&menueditor_.entry, item, sizeof(MenuEntry));
  menueditor_.change_exec = strcmp(exec->element, name);

  if (menueditor_.change_exec) {
    gtk_widget_set_sensitive (GTK_WIDGET(menueditor_.view), FALSE);
    settings_menu_enable (panel);	/* enable apply and cancel buttons */
  }
  return FALSE;
} /* </settings_menu_exec> */

/*
* settings_menu_item_new
*/
static void
settings_menu_item_new (GlobalPanel *panel,
                        ConfigurationNode *chain,
                        GtkTreeStore *store,
                        GtkTreeIter *parent,
                        guint iconsize)
{
  const gchar *name = NULL;
  GdkPixbuf *pixbuf = NULL;
  GtkTreeIter cell;


  /* Handle {item, submenu, separator} menu item types. */
  if (strcmp(chain->element, "header") == 0 ||
      strcmp(chain->element, "submenu") == 0 ||
      strcmp(chain->element, "item") == 0 ) {

    const gchar *icon;
    const gchar *file;

    /* when element = "header" parse the name attribute */
    icon = configuration_attrib (chain, "icon");
    name = configuration_attrib (chain, "name");

    if (strcmp(chain->element, "header") == 0) {
      if (icon == NULL)
        icon = (getuid() == 0) ? "administrator.png" : "user.png";

      /* "%u" => user name, %r => real name */
      if (name && (strcmp(name, "%u") == 0 || strcmp(name, "%r") == 0))
        name = get_username (strcmp(name, "%r") == 0);
    }

    if (icon && (file = icon_path_finder (panel->icons, icon)) != NULL)
      pixbuf = pixbuf_new_from_file_scaled (file, iconsize, iconsize);
  }
  else if (strcmp(chain->element, "separator") == 0) {
    name = "---------------";
  }

  if (name == NULL)	/* unknown configuration menu item type */
    return;

  gtk_tree_store_append (store, &cell, parent);
  gtk_tree_store_set (store, &cell,
                      MENU_ICON, pixbuf,
                      MENU_NAME, _(name),
                      MENU_DATA, chain,
                      -1);

  /* Account for all items in the submenu. */
  if (strcmp(chain->element, "submenu") == 0) {
    ConfigurationNode *node;
    guint depth = chain->depth + 1;

    for (node = chain->next; node != NULL; node = node->next)
      if (node->depth == depth && node->type != XML_READER_TYPE_END_ELEMENT)
        settings_menu_item_new (panel, node, store, &cell, iconsize);
      else if (node->depth == chain->depth)  /* same depth.. move on */
        break;
  }
} /* </settings_menu_item_new> */

/*
* settings_menu_new
*/
GtkWidget *
settings_menu_new (Modulus *applet)
{
  static MenuEntry item;
  GlobalPanel *panel = applet->data;

  ConfigurationNode *menu = configuration_find (panel->config, "menu");
  ConfigurationNode *cache = configuration_clone (menu);
  PanelIcons *icons = panel->icons;

  const gchar *name = configuration_attrib (menu, "name");
  const gchar *value = configuration_attrib (menu, "iconsize");
  guint iconsize = (value) ? atoi(value) : icons->size;

  GtkWidget *button, *entry, *image, *label, *layer, *layout;
  GtkWidget *box, *hbox, *frame, *scrolled, *splitview, *vbox, *view;
  GtkTooltips *tooltips;

  GtkCellRenderer   *renderer;
  GtkTreeSelection  *selection;
  GtkTreeViewColumn *column;
  GtkTreeStore      *store;
  GtkTreeModel      *model;
  GtkTreePath       *path;

  unsigned short width = getSettingsPanelWidth() * 33 / 100;


  /* Construct panel layout. */
  layout = gtk_vbox_new (FALSE, 2);

  /* Add selections controls. */
  layer = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX(layout), layer, FALSE, FALSE, 0);
  gtk_widget_show (layer);

  /* Button for new item. */
  button = xpm_image_button_scaled (ICON_ITEM, iconsize, iconsize);
  gtk_box_pack_start (GTK_BOX(layer), button, FALSE, FALSE, 0);
  gtk_widget_show (button);

  g_signal_connect (G_OBJECT(button), "button-press-event",
                    G_CALLBACK(settings_menu_insert), panel);

  tooltips = gtk_tooltips_new ();
  gtk_tooltips_set_tip (tooltips, button, _("Insert"), NULL);
  menueditor_.insert_button = button;

  /* Button for cut item. */
  button = xpm_image_button_scaled (ICON_CUT, iconsize, iconsize);
  gtk_box_pack_start (GTK_BOX(layer), button, FALSE, FALSE, 0);
  gtk_widget_show (button);

  g_signal_connect (G_OBJECT(button), "button-press-event",
                    G_CALLBACK(settings_menu_cut), panel);

  tooltips = gtk_tooltips_new();
  gtk_tooltips_set_tip (tooltips, button, _("Cut"), NULL);
  menueditor_.cut_button = button;

  /* Button for paste item. */
  button = xpm_image_button_scaled (ICON_PASTE, iconsize, iconsize);
  gtk_box_pack_start (GTK_BOX(layer), button, FALSE, FALSE, 0);
  gtk_widget_show (button);

  g_signal_connect (G_OBJECT(button), "button-press-event",
                    G_CALLBACK(settings_menu_paste), panel);

  tooltips = gtk_tooltips_new();
  gtk_tooltips_set_tip (tooltips, button, _("Paste"), NULL);
  menueditor_.paste_button = button;

  /* Button for remove item. */
  button = xpm_image_button_scaled (ICON_DELETE, iconsize, iconsize);
  gtk_box_pack_start (GTK_BOX(layer), button, FALSE, FALSE, 0);
  gtk_widget_show (button);

  g_signal_connect (G_OBJECT(button), "button-press-event",
                    G_CALLBACK(settings_menu_remove), panel);

  tooltips = gtk_tooltips_new();
  gtk_tooltips_set_tip (tooltips, button, _("Remove"), NULL);
  menueditor_.remove_button = button;

  /* Cut and paster preview area. */
  box = gtk_label_new ("");
  gtk_box_pack_start (GTK_BOX(layer), box, FALSE, FALSE, 12);
  gtk_widget_show (box);

  box = gtk_hbox_new (FALSE, 5);
  gtk_box_pack_start (GTK_BOX(layer), box, FALSE, FALSE, 8);
  gtk_widget_show (box);

  menueditor_.paste_view = NULL;
  frame = menueditor_.paste_frame = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME(frame), GTK_SHADOW_IN);
  gtk_container_add (GTK_CONTAINER(box), frame);

  /* Construct splitview: left => selections, right => view/edit. */
  splitview = gtk_hbox_new (FALSE, 2);
  gtk_box_pack_start (GTK_BOX(layout), splitview, TRUE, TRUE, 0);
  gtk_widget_show (splitview);

  /* Add a scrolled window for the selections. */
  scrolled = gtk_scrolled_window_new (NULL, NULL);
  gtk_box_pack_start (GTK_BOX(splitview), scrolled, FALSE, FALSE, 0);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW(scrolled),
                                  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_widget_show (scrolled);

  /* [selections]Left side of splitview. */
  store = gtk_tree_store_new (MENU_COUNT,
                              GDK_TYPE_PIXBUF,
                              G_TYPE_STRING,
                              G_TYPE_POINTER);

  view = gtk_tree_view_new_with_model (GTK_TREE_MODEL(store));
  gtk_container_add (GTK_CONTAINER(scrolled), view);
  /* gtk_widget_set_usize (scrolled, 200, 0); FIXME: adjust by resolution */
  gtk_widget_set_usize (scrolled, width, 0);
  gtk_widget_show (view);

  /* Construct a single column. */
  column = gtk_tree_view_column_new();
  gtk_tree_view_append_column (GTK_TREE_VIEW(view), column);

  /* Set column header. */
  label = gtk_label_new ("");
  gtk_tree_view_column_set_widget (column, label);
  gtk_widget_set_usize (label, 100, 1);
  gtk_widget_show (label);

  /* Set column attributes only for the icon and text. */
  renderer = gtk_cell_renderer_pixbuf_new();
  gtk_tree_view_column_pack_start (column, renderer, FALSE);
  gtk_tree_view_column_set_attributes (column, renderer,
                                        "pixbuf", MENU_ICON,
                                        NULL);

  renderer = gtk_cell_renderer_text_new();
  gtk_tree_view_column_pack_start (column, renderer, TRUE);
  gtk_tree_view_column_set_attributes (column, renderer,
                                       "text", MENU_NAME,
                                       NULL);

  /* Initialize menueditor_{iconsize,cache,store,view} */
  menueditor_.iconsize = iconsize;
  menueditor_.cache    = cache;
  menueditor_.store    = store;
  menueditor_.view     = GTK_TREE_VIEW(view);

  /* Populate view with the menu items according to configuration. */
  settings_menu_config (panel, menueditor_.cache, store, &menueditor_.root);

  /* Expand top level menu row. */
  path = gtk_tree_path_new_from_string ("0");
  gtk_tree_view_expand_row (GTK_TREE_VIEW(view), path, TRUE);
  gtk_tree_path_free (path);

  /* Add callback for selection change. */
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW(view));
  gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);
  gtk_tree_selection_select_iter (selection, &menueditor_.root);

  /* Store name, icon and configuration node. */
  gtk_tree_selection_get_selected (selection, &model, &item.cell);

  gtk_tree_model_get (model, &item.cell,
                      MENU_ICON, &item.icon,
                      MENU_NAME, &item.name,
                      MENU_DATA, &item.node,
                      -1);

  menueditor_.item = &item;
  g_signal_connect (selection, "changed",
                    G_CALLBACK(settings_menu_item), panel);

  /* disable editing at menu start when <header> element present */
  if (menu->next && strcmp(menu->next->element, "header") == 0) {
    gtk_widget_set_sensitive (menueditor_.cut_button, FALSE);
    gtk_widget_set_sensitive (menueditor_.insert_button, FALSE);
    gtk_widget_set_sensitive (menueditor_.paste_button, FALSE);
    gtk_widget_set_sensitive (menueditor_.remove_button, FALSE);
  }

  /* [settings]Right side of splitview. */
  vbox = gtk_vbox_new (FALSE, 2);
  gtk_box_pack_start (GTK_BOX(splitview), vbox, FALSE, FALSE, 5);
  gtk_widget_show (vbox);

  /* Box for the icon, type, name and program. */
  box = gtk_vbox_new (FALSE, 1);
  gtk_box_pack_start (GTK_BOX(vbox), box, FALSE, FALSE, 8);
  /* gtk_widget_set_usize (box, 220, 0);   1024x768 resolution */
  gtk_widget_set_usize (box, width+20, 0);
  gtk_widget_show (box);

  frame = gtk_frame_new (NULL);
  gtk_container_add (GTK_CONTAINER(box), frame);
  gtk_widget_show (frame);

  /* Space item details view inside hbox. */
  hbox = gtk_hbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER(frame), hbox);
  gtk_widget_show (hbox);

  label = gtk_label_new ("");
  gtk_box_pack_start (GTK_BOX(hbox), label, FALSE, FALSE, 5);
  gtk_widget_show (label);

  view = gtk_vbox_new (FALSE, 1);
  gtk_box_pack_start (GTK_BOX(hbox), view, FALSE, FALSE, 0);
  gtk_widget_show (view);

  label = gtk_label_new ("");
  gtk_box_pack_start (GTK_BOX(hbox), label, FALSE, FALSE, 5);
  gtk_widget_show (label);

  /* The icon image of the item. */
  layer = menueditor_.icon_hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX(view), layer, FALSE, FALSE, 0);
  gtk_widget_show (layer);

  button = menueditor_.icon_button = gtk_button_new ();
  gtk_button_set_relief (GTK_BUTTON(button), GTK_RELIEF_NONE);
  gtk_box_pack_start (GTK_BOX(layer), button, FALSE, FALSE, 2);
  gtk_widget_show (button);

  value = configuration_attrib (menu, "icon");
  image = image_new_from_path_scaled (icons->path, value, 22, 22);
  gtk_container_add (GTK_CONTAINER (button), image);
  gtk_widget_show (image);

  label = gtk_label_new (_("Click on the icon to change it."));
  gtk_box_pack_start (GTK_BOX(layer), label, FALSE, FALSE, 0);
  gtk_widget_show (label);

  g_signal_connect (G_OBJECT(button), "clicked",
                    G_CALLBACK(settings_menu_icon), panel);

  /* The name of the item. */
  layer = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX(view), layer, FALSE, FALSE, 0);
  gtk_widget_show (layer);

  label = gtk_label_new (_("Name:"));
  gtk_box_pack_start (GTK_BOX(layer), label, FALSE, FALSE, 2);
  gtk_widget_show (label);

  entry = menueditor_.name_entry = gtk_entry_new ();
  gtk_box_pack_end (GTK_BOX(layer), entry, FALSE, FALSE, 2);
  gtk_entry_set_text(GTK_ENTRY(entry), name);
  gtk_widget_set_usize (entry, 120, 20);
  gtk_widget_show (entry);

  g_signal_connect (G_OBJECT(entry), "key-press-event",
                    G_CALLBACK(settings_menu_name), panel);

  /* The program of the item. */
  layer = menueditor_.exec_hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX(view), layer, FALSE, FALSE, 0);
  gtk_widget_set_sensitive (layer, FALSE);
  gtk_widget_show (layer);

  label = gtk_label_new (_("Program:"));
  gtk_box_pack_start (GTK_BOX(layer), label, FALSE, FALSE, 2);
  gtk_widget_show (label);

  entry = menueditor_.exec_entry = gtk_entry_new ();
  gtk_box_pack_end (GTK_BOX(layer), entry, FALSE, FALSE, 2);
  gtk_widget_set_usize (entry, 120, 20);
  gtk_widget_show (entry);

  g_signal_connect (G_OBJECT(entry), "key-press-event",
                    G_CALLBACK(settings_menu_exec), panel);

  /* The type of item {item, menu, separator} */
  layer = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX(view), layer, FALSE, FALSE, 2);
  gtk_widget_show (layer);

  label = gtk_label_new (_("Type:"));
  gtk_box_pack_start (GTK_BOX(layer), label, FALSE, FALSE, 2);
  gtk_widget_show (label);

  entry = menueditor_.type_combo = gtk_combo_box_new_text ();
  gtk_box_pack_end  (GTK_BOX(layer), entry, FALSE, FALSE, 2);
  gtk_widget_set_usize (entry, 120, 22);
  gtk_widget_show (entry);

  gtk_combo_box_append_text (GTK_COMBO_BOX(entry), _("menu item"));
  gtk_combo_box_append_text (GTK_COMBO_BOX(entry), _("separator"));
  gtk_combo_box_append_text (GTK_COMBO_BOX(entry), _("submenu"));

  gtk_combo_box_set_active (GTK_COMBO_BOX(entry), MENU_SUBMENU);
  gtk_widget_set_sensitive (entry, FALSE);

  g_signal_connect (G_OBJECT(entry), "changed",
                    G_CALLBACK (settings_menu_itemtype), panel);

  /* Check button for internal (unchecked => external) program. */
  button = gtk_check_button_new_with_label (_("Internal program method"));
  gtk_box_pack_start (GTK_BOX(view), button, FALSE, FALSE, 2);
  gtk_widget_set_sensitive (button, FALSE);
  menueditor_.exec_method = button;

  g_signal_connect (G_OBJECT (button), "toggled",
                          G_CALLBACK (settings_menu_itemexec), panel);
  gtk_widget_show(button);

  /* Last box.. revert to default menu. */
  layer = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX(vbox), layer, FALSE, FALSE, 5);
  gtk_widget_show (layer);

  button = xpm_image_button_scaled (ICON_REBOOT, 30, 30);
  gtk_box_pack_start (GTK_BOX(layer), button, FALSE, FALSE, 2);
  gtk_widget_show (button);

  g_signal_connect (G_OBJECT(button), "button-press-event",
                    G_CALLBACK(settings_menu_restore), panel);

  label = gtk_label_new (_("Restore default menu."));
  gtk_box_pack_start (GTK_BOX(layer), label, FALSE, FALSE, 2);
  gtk_widget_show (label);

  /* Initial values for changing icon, name, program and type. */
  menueditor_.change_icon = FALSE;
  menueditor_.change_name = FALSE;
  menueditor_.change_exec = FALSE;
  menueditor_.change_type = FALSE;

  return layout;
} /* </settings_menu_new> */

/*
* settings_menu_config
*/
static void
settings_menu_config (GlobalPanel *panel, ConfigurationNode *menu,
                      GtkTreeStore *store, GtkTreeIter *root)
{
  const gchar *name = configuration_attrib (menu, "name");
  const gchar *icon = configuration_attrib (menu, "icon");

  guint iconsize = menueditor_.iconsize;


  /* Add the top level menu item. */
  GdkPixbuf *pixbuf = pixbuf_new_from_path_scaled (panel->icons->path,
                                                   icon, iconsize, iconsize);
  gtk_tree_store_append (store, root, NULL);

  gtk_tree_store_set (store, root,
                      MENU_ICON, pixbuf,
                      MENU_NAME, _(name),
                      MENU_DATA, menu,
                      -1);

  /* Add the menu items from the configuration. */
  if (menu != NULL) {
    ConfigurationNode *node = menu->next;
    ConfigurationNode *mark = configuration_find_end (menu);

    guint depth = menu->depth + 1;

    for (; node != mark && node != NULL; node = node->next)
      if (node->depth == depth && node->type != XML_READER_TYPE_END_ELEMENT)
        settings_menu_item_new (panel, node, store, root, iconsize);
  }
} /* </settings_menu_config> */

/*
* settings_menu_select_iter
*/
static gboolean
settings_menu_select_iter (ConfigurationNode *node, GtkTreeIter *iter,
                           GtkTreeIter *parent)
{
  GtkTreeIter  cell;
  GtkTreeIter  *root;
  GtkTreeModel *model = GTK_TREE_MODEL(menueditor_.store);
  GtkTreeSelection *seen = gtk_tree_view_get_selection (menueditor_.view);

  ConfigurationNode *match;
  gint count, idx;
  gboolean valid;

  if (parent != NULL) {
    root  = parent;
    valid = TRUE;
  }
  else {
    root  = &menueditor_.root;
    valid = gtk_tree_model_get_iter_first (model, iter);

    gtk_tree_model_get (model, iter, 
                        MENU_DATA, &match,
                        -1);

    if (match == node) {	/* found our match.. */
      gtk_tree_selection_select_iter (seen, iter);
      return TRUE;
    }
  }

  /* Walk through the list, reading each row */
  count = gtk_tree_model_iter_n_children (model, root);

  for (idx = 0; idx < count; idx++) {
    valid = gtk_tree_model_iter_nth_child (model, iter, root, idx);

    gtk_tree_model_get (model, iter, 
                        MENU_DATA, &match,
                        -1);

    if (match == node) {	/* found our match.. */
      gtk_tree_selection_select_iter (seen, iter);
      return TRUE;
    }

    if (gtk_tree_model_iter_has_child (model, iter)) {
      valid = settings_menu_select_iter (node, &cell, iter);
    }
  }

  return valid;
} /* </settings_menu_select_iter> */
