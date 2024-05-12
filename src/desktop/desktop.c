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
#include "gsession.h"
#include "docklet.h"

#define desktop_file_get_string(file, key) \
	g_key_file_get_string(file, G_KEY_FILE_DESKTOP_GROUP, key, NULL)

#define PREVIEW_VIEW_WIDTH  128
#define PREVIEW_VIEW_HEIGHT 128

extern const char *Program, *Release;	     /* see, gpanel.c */

static gchar *ActionHintDelete = "Press the apply button to delete.";
static gchar *ActionHintEdit   = "Click on the icon to change it.";

static gchar *DefaultIconHome = "home.png";  /* default icon for shortcut */
static gchar *DefaultIconExec = "exec.png";  /* default icon for programs */

static char *DefaultShortcutName = "Home";
static char *AppletName = "desktop";

/*
* Data structures used by this module.
*/
typedef struct _DesktopEntry DesktopEntry;
typedef struct _DesktopSettings DesktopSettings;

struct _DesktopEntry {
  gchar *name;
  gchar *icon;
  gchar *exec;
};

struct _DesktopSettings {
  GlobalPanel *panel;		/* GlobalPanel instance */
  GtkWidget *preview;		/* shortcut preview pane */
  const char *iconpath;		/* preview icon file */
  guint8 fontsel;		/* font array index */
  gint16 iconsize;		/* 24 => 24x24, ... */
};

const char *DesktopEntrySpec =
"<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
"<item name=\"%s\" icon=\"%s\">\n"
" <exec>%s</exec>\n"
" <xpos>%d</xpos>\n"
" <ypos>%d</ypos>\n"
"</item>\n";

const char *DesktopFileSpec =
"[Desktop Entry]\n\n"
"# The version of the desktop entry specification to which this file complies\n"
"Version=1.0\n\n"
"# The name of the application\n"
"Name=%s\n\n"
"# A comment which can/will be used as a tooltip\n"
"Comment=%s\n\n"
"# The name of the icon that will be used to display this entry\n"
"Icon=%s\n\n"
"# The executable of the application, possibly with arguments.\n"
"Exec=%s\n\n"
"# The path to the folder in which the executable is run\n"
"Path=/usr/bin\n\n"
"# Describes whether this application needs to be run in a terminal or not\n"
"Terminal=false\n\n"
"Type=Application\n\n"
"# Describes the categories in which this entry should be shown\n"
"Categories=Applications;Desktop;\n";

#define DesktopFontMax 8
const char *DesktopFont[DesktopFontMax] = {
  "Arial 10"
 ,"Arial 12"
 ,"Arial 14"
 ,"Times 9"
 ,"Times 12"
 ,"Times 14"
 ,"Sans 12"
 ,"Sans 14"
};

DesktopSettings settings_;  /* private DesktopSettings to this module */
static guint8 iconsize_maxsel = 5; // must match iconsize_selected[] array size
static gint iconsize_selected[5] = {24, 32, 48, 64, 96};

/*
* desktop_parse_file
*/
DesktopEntry *
desktop_file_parse(const char *filename)
{
  DesktopEntry *entry = NULL;
  GKeyFile *file = g_key_file_new ();

  if (g_key_file_load_from_file (file, filename, 0, NULL)) {
    static DesktopEntry _entry;

    _entry.exec = desktop_file_get_string (file, "Exec");
    _entry.icon = desktop_file_get_string (file, "Icon");
    _entry.name = desktop_file_get_string (file, "Name");

    entry = &_entry;
  }

  g_key_file_free (file);
  return entry;
} /* </desktop_file_parse> */

/*
* (private) desktop_move - change the position of a desktop shortcut
*/
static void
desktop_move(ConfigurationNode *node, gint16 xpos, gint16 ypos)
{
  ConfigurationNode *item;
  gchar *name = configuration_attrib(node, "name");

  if ((item = configuration_find(node, "xpos")) != NULL) {
    g_free (item->element);
    item->element = g_strdup_printf ("%d", ((xpos < 0) ? 0 : xpos));
  }
  if ((item = configuration_find(node, "ypos")) != NULL) {
    g_free (item->element);
    item->element = g_strdup_printf ("%d", ((ypos < 0) ? 0 : ypos));
  }
  vdebug (2, "%s moved to (%d,%d)\n", name, xpos, ypos);
} /* </desktop_move> */

/*
* (private) desktop_filer_apply
* (private) desktop_filer_cancel
*/
static bool
desktop_filer_apply(GtkWidget *button, GlobalPanel *panel)
{
  PanelDesktop *desktop = panel->desktop;

  gtk_widget_hide (desktop->filer);
  gtk_button_set_image (GTK_BUTTON(desktop->iconview),
			gtk_image_new_from_pixbuf (desktop->render));

  if (desktop->chooser_apply)	/* if specified invoke additional callback */
    (*desktop->chooser_apply) (panel);

  return true;
} /* </desktop_filer_apply> */

static bool
desktop_filer_cancel(GtkWidget *button, GlobalPanel *panel)
{
  PanelDesktop *desktop = panel->desktop;
  gtk_widget_hide (desktop->filer);
  return true;
} /* </desktop_filer_cancel> */

/*
* (private) desktop_filer_refresh
*/
bool
desktop_filer_refresh(GtkWidget *canvas,
                      GdkEventExpose *event,
                      GlobalPanel *panel)
{
  PanelDesktop *desktop = panel->desktop;
  redraw_pixbuf (canvas, desktop->render);
  return true;
} /* </desktop_filer_refresh> */

/*
* (private) desktop_filer_repaint
*/
static bool
desktop_filer_repaint(FileChooserDatum *datum)
{
  GlobalPanel *panel = datum->user;
  PanelDesktop *desktop = panel->desktop;
  const gchar *file = datum->file;
  struct stat info;

  if (lstat(file, &info) == 0 && S_ISREG(info.st_mode)) {
    gint16 iconsize = desktop->iconsize;

    desktop->render = pixbuf_new_from_file_scaled (file, iconsize, iconsize);
    desktop_filer_refresh (desktop->canvas, NULL, panel);
  }
  return true;
} /* </desktop_filer_repaint> */

/*
* (private) desktop_settings_apply
*/
static bool
desktop_settings_apply(GtkWidget *button, GlobalPanel *panel)
{
  static char chain_markend[MAX_STAMP];
  sprintf(chain_markend, "/%s", AppletName);

  PanelDesktop *desktop = panel->desktop;
  DesktopAction action = desktop->action;
  Docklet *docklet = desktop->shortcut;

  /* Dismiss the desktop->gwindow interface. */
  gtk_widget_hide (desktop->gwindow);
  desktop->active = false;

  /* Handle the user specified action. */
  if (action == DESKTOP_SAVECONFIG) {
    ConfigurationNode *node = configuration_find (panel->config, AppletName);

    if (node != NULL) {		// should never happen!
      ConfigurationAttrib *attrib = node->attrib;

      while (attrib != NULL) {
        if (strcmp(attrib->name, "font") == 0) {
          g_free(attrib->value);
          attrib->value = g_strdup(DesktopFont[settings_.fontsel]);
        }
        else if (strcmp(attrib->name, "iconsize") == 0) {
          g_free(attrib->value);
          attrib->value = g_strdup_printf("%d", settings_.iconsize);
        }
        vdebug(2, "%s attrib->name => %s, attrib->value => %s\n",
			  __func__, attrib->name, attrib->value);
        attrib = attrib->next;
      }
      saveconfig (panel);
      gpanel_restart (panel, SIGURG);	    /* signal program restart */
    }
  }
  else if (action == DESKTOP_SHORTCUT_DELETE) {
    configuration_remove (desktop->node);   /* remove from configuration */
    gtk_widget_destroy (docklet->window);   /* remove from screen display */
    g_object_ref_sink (docklet);	    /* clean-up memory */
    g_object_unref (docklet);
  }
  else {
    FileChooser *chooser = desktop->chooser;
    const gchar *iconname = gtk_entry_get_text (GTK_ENTRY(chooser->name));
    const gchar *iconpath = filechooser_get_selected_name (chooser);

    const gchar *text = gtk_entry_get_text (GTK_ENTRY(desktop->name));
    const gchar *run  = gtk_entry_get_text (GTK_ENTRY(desktop->exec));

    ConfigurationNode *config = configuration_find (panel->config, AppletName);
    gchar *font = configuration_attrib (config, "font");

    if (icon_path_finder (panel->icons, iconname) == NULL)
      iconname = iconpath;

    if (action == DESKTOP_SHORTCUT_EDIT) {
      ConfigurationNode *node = desktop->node;
      ConfigurationAttrib *attrib = node->attrib;
      ConfigurationNode *exec = configuration_find (node, "exec");

      /* Deallocate and reconstruct attrib link list. */
      configuration_attrib_remove (attrib);

      attrib = node->attrib = g_new0 (ConfigurationAttrib, 1);
      attrib->name  = g_strdup ("name");
      attrib->value = g_strdup (text);

      attrib = attrib->next = g_new0 (ConfigurationAttrib, 1);
      attrib->name  = g_strdup ("icon");
      attrib->value = g_strdup (iconname);

      /* Deallocate and reconstruct exec->element */
      g_free (exec->element);
      exec->element = g_strdup (run);

      docklet_update (docklet, iconpath, text);	/* update screen display */
    }
    else if (action == DESKTOP_SHORTCUT_CREATE) {
      ConfigurationNode *item;
      ConfigurationNode *mark;
      Docklet *docklet;

      char spec[MAX_PATHNAME];
      gint16 iconsize = desktop->iconsize;
      gint16 xpos;
      gint16 ypos;


      /* Calculate where to place and form the XML. */
      desktop_config (panel, false);

      xpos = desktop->xpos;
      ypos = desktop->ypos + desktop->step;

      sprintf(spec, DesktopEntrySpec, text, iconname, run, xpos, ypos);
      item = configuration_read (spec, NULL, true);
      mark = configuration_find (config, chain_markend);

      if (mark == NULL) {	/* <desktop ... /> case */
        if ( (mark = configuration_find (config, "/")) ) {
          g_free (mark->element);
          mark->element = g_strdup (chain_markend);
        }
        else {
          return false;
        }
      }

      configuration_insert (item, mark->back, mark->depth + 1);

      /* new screen visual (desktop shortcut) */
      docklet = desktop_shortcut_create (panel, item,
					 iconsize, iconsize,
					 xpos, ypos,
					 iconpath, text, font,
					 NULL, NULL, false);
      gtk_widget_show (docklet->window);
    }
  }
  return true;
} /* </desktop_settings_apply> */

/*
* (private) desktop_settings_cancel
*/
static bool
desktop_settings_cancel(GtkWidget *button, GlobalPanel *panel)
{
  PanelDesktop *desktop = panel->desktop;

  gtk_widget_hide (desktop->gwindow);
  desktop->active = false;
  return true;
} /* </desktop_settings_cancel> */

/*
* (private) desktop_settings_close - future use?
*/
static bool
desktop_settings_close(GtkWidget *button, GlobalPanel *panel)
{
  vdebug(3, "%s <unimplemented>\n", __func__);
  return true;
}

/*
* desktop_settings_enable - register callbacks for apply and cancel
*/
static void
desktop_settings_enable(PanelSetting *settings)
{
  settings_save_enable (settings, true);
  settings_set_agents (settings,
			(gpointer)desktop_settings_apply,
			(gpointer)desktop_settings_cancel,
			(gpointer)desktop_settings_close);
} /* </desktop_settings_enable> */

/*
* (private) desktop_setting_iconview
*/
static bool
desktop_setting_iconview(GtkWidget *button, GlobalPanel *panel)
{
  PanelDesktop *desktop = panel->desktop;
  gtk_widget_show (desktop->filer);
  return true;
} /* </desktop_setting_iconview> */

/*
* desktop_new - object for creating and editing desktop shortcuts
*/
PanelDesktop *
desktop_new(GlobalPanel *panel, gint16 iconsize)
{
  static char desktop_dir[UNIX_PATH_MAX];
  sprintf(desktop_dir, "%s/Desktop", getenv("HOME"));

  GtkWidget *button, *canvas, *entry, *frame, *label;
  GtkWidget *layer, *layout, *split, *window;

  ConfigurationNode *chain = configuration_find(panel->config, AppletName);
  ConfigurationNode *menu  = configuration_find(chain, "menu");

  PanelDesktop *desktop = g_new0 (PanelDesktop, 1);
  PanelIcons *icons = panel->icons;

  gchar *value = configuration_attrib(menu, "iconsize");
  gint16 menuiconsize = (value) ? atoi(value) : panel->taskbar->iconsize;

  FileChooser *chooser;
  gchar dirname[UNIX_PATH_MAX];
  gchar *scan;

  desktop->active = false;
  desktop->iconsize = iconsize;
  desktop->folder = desktop_dir;
  desktop->menu = menu_options_config (menu, panel, menuiconsize);

  /* Create a sticky window with a frame. */
  window = desktop->gwindow = sticky_window_new (GDK_WINDOW_TYPE_HINT_NORMAL,
                                                        280, 180, 100, 100);
  gtk_window_set_keep_above (GTK_WINDOW(window), TRUE);

  frame = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME(frame), GTK_SHADOW_OUT);
  gtk_container_add (GTK_CONTAINER(window), frame);
  gtk_widget_show (frame);

  /* Create the inside window layout. */
  layout = gtk_vbox_new (FALSE, 1);
  gtk_container_add (GTK_CONTAINER(frame), layout);
  gtk_widget_show (layout);

  label = gtk_label_new ("");
  gtk_box_pack_start (GTK_BOX(layout), label, FALSE, FALSE, 0);
  gtk_widget_show (label);

  /* Top portion has the icon and FileChooser */
  layer = gtk_hbox_new (FALSE, 1);
  gtk_container_add (GTK_CONTAINER(layout), layer);
  gtk_widget_show (layer);

  /* Brute force addition of horizontal spacing. */
  label = gtk_label_new ("");
  gtk_box_pack_start (GTK_BOX(layer), label, FALSE, FALSE, 5);
  gtk_widget_show (label);

  label = gtk_label_new ("");
  gtk_box_pack_end (GTK_BOX(layer), label, FALSE, FALSE, 5);
  gtk_widget_show (label);

  button = desktop->iconview = gtk_button_new ();
  gtk_box_pack_start (GTK_BOX(layer), button, FALSE, FALSE, 2);
  gtk_widget_show (button);

  label = desktop->hint = gtk_label_new (_(ActionHintEdit));
  gtk_box_pack_start (GTK_BOX(layer), label, FALSE, FALSE, 2);
  gtk_widget_show (label);

  g_signal_connect (G_OBJECT(button), "clicked",
                     G_CALLBACK(desktop_setting_iconview), panel);

  /* Middle portion has text entries for name and program. */
  label = gtk_label_new ("");
  gtk_box_pack_start (GTK_BOX(layout), label, FALSE, FALSE, 0);
  gtk_widget_show (label);

  layer = gtk_hbox_new (FALSE, 1);
  gtk_container_add (GTK_CONTAINER(layout), layer);
  gtk_widget_show (layer);

  label = gtk_label_new ("");
  gtk_box_pack_start (GTK_BOX(layer), label, TRUE, FALSE, 0);
  gtk_widget_show (label);

  label = gtk_label_new (_("Name:"));
  gtk_box_pack_start (GTK_BOX(layer), label, FALSE, FALSE, 2);
  gtk_widget_show (label);

  entry = desktop->name = gtk_entry_new ();
  gtk_box_pack_start (GTK_BOX(layer), entry, FALSE, FALSE, 2);
  gtk_widget_show (entry);

  label = gtk_label_new ("");
  gtk_box_pack_end (GTK_BOX(layer), label, FALSE, FALSE, 5);
  gtk_widget_show (label);

  /* Box for the exec item (program, url, ..) */
  layer = gtk_hbox_new (FALSE, 1);
  gtk_container_add (GTK_CONTAINER(layout), layer);
  gtk_widget_show (layer);

  label = gtk_label_new ("");
  gtk_box_pack_start (GTK_BOX(layer), label, TRUE, FALSE, 0);
  gtk_widget_show (label);

  label = gtk_label_new (_("Program:"));
  gtk_box_pack_start (GTK_BOX(layer), label, FALSE, FALSE, 2);
  gtk_widget_show (label);

  entry = desktop->exec = gtk_entry_new ();
  gtk_box_pack_start (GTK_BOX(layer), entry, FALSE, FALSE, 2);
  gtk_widget_show (entry);

  label = gtk_label_new ("");
  gtk_box_pack_end (GTK_BOX(layer), label, FALSE, FALSE, 5);
  gtk_widget_show (label);

  label = gtk_label_new ("");
  gtk_box_pack_start (GTK_BOX(layout), label, FALSE, FALSE, 0);
  gtk_widget_show (label);

  /* Bottom portion has buttons for cancel and apply. */
  layer = gtk_hbox_new (FALSE, 1);
  gtk_container_add (GTK_CONTAINER(layout), layer);
  gtk_widget_show (layer);

  button = xpm_button (ICON_APPLY, NULL, 0, _("Apply"));
  gtk_box_pack_end (GTK_BOX(layer), button, FALSE, FALSE, 0);
  gtk_button_set_relief (GTK_BUTTON(button), GTK_RELIEF_NONE);

  g_signal_connect (G_OBJECT(button), "clicked",
                     G_CALLBACK(desktop_settings_apply), panel);
  gtk_widget_show (button);

  button = xpm_button (ICON_CANCEL, NULL, 0, _("Cancel"));
  gtk_box_pack_end (GTK_BOX(layer), button, FALSE, FALSE, 0);
  gtk_button_set_relief (GTK_BUTTON(button), GTK_RELIEF_NONE);

  g_signal_connect (G_OBJECT(button), "clicked",
                     G_CALLBACK(desktop_settings_cancel), panel);
  gtk_widget_show (button);

  /* Save the initial default image path. */
  desktop->icon = g_strdup (icon_path_finder (icons, DefaultIconExec));
  gtk_entry_set_text (GTK_ENTRY(desktop->name), DefaultIconExec);

  /* Create popup window for choosing a different icon. */
  window = desktop->filer = gtk_window_new (GTK_WINDOW_POPUP);
  gtk_window_set_default_size (GTK_WINDOW(window),480, 300);
  gtk_widget_set_uposition (GTK_WIDGET(window), 60, 120);

  frame = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME(frame), GTK_SHADOW_OUT);
  gtk_container_add (GTK_CONTAINER(window), frame);
  gtk_widget_show (frame);

  /* Split window in two areas. */
  split = gtk_vbox_new (FALSE, 1);
  gtk_container_add (GTK_CONTAINER(frame), split);
  gtk_widget_show (split);

  layout = gtk_hbox_new (FALSE, 1);
  gtk_box_pack_start (GTK_BOX(split), layout, FALSE, FALSE, 0);
  gtk_widget_show (layout);

  /* Create an instance of FileChooser using desktop->icon dirname value. */
  desktop->render = pixbuf_new_from_file_scaled (desktop->icon,
                                                 iconsize, iconsize);
  if (desktop->icon) {
    strcpy(dirname, desktop->icon);
    scan = strrchr(dirname, '/');
    *scan = (gchar)0;
  }
  else {
    strcpy(dirname, "/usr/share/pixmaps");
  }

  chooser = desktop->chooser = filechooser_new (dirname, NULL);
  filechooser_set_callback (chooser, (gpointer)desktop_filer_repaint, panel);
  gtk_entry_set_text (GTK_ENTRY(chooser->name), DefaultIconExec);
  chooser->clearname  = TRUE;	/* clear file name on directory change */

  layer = filechooser_layout (chooser);
  gtk_widget_set_size_request (layer, 440, 5*iconsize);
  gtk_box_pack_start (GTK_BOX(layout), layer, TRUE, TRUE, 0);
  /* gtk_widget_hide (chooser->namebox); */
  gtk_widget_show (layer);

  canvas = desktop->canvas = gtk_drawing_area_new ();
  gtk_widget_set_size_request (canvas, iconsize, iconsize);
  gtk_box_pack_start (GTK_BOX(layout), canvas, FALSE, TRUE, 2*iconsize/3);
  gtk_widget_show (canvas);

  g_signal_connect (G_OBJECT (layer), "expose_event",
                    G_CALLBACK (desktop_filer_refresh), panel);

  /* Add the APPLY and CANCEL buttons. */
  layout = gtk_hbox_new (FALSE, 1);
  gtk_box_pack_start (GTK_BOX(split), layout, FALSE, FALSE, 0);
  gtk_widget_show (layout);

  button = xpm_button (ICON_APPLY, NULL, 0, _("Apply"));
  gtk_box_pack_end (GTK_BOX(layout), button, FALSE, FALSE, 0);
  gtk_button_set_relief (GTK_BUTTON(button), GTK_RELIEF_NONE);
  gtk_widget_show (button);

  g_signal_connect (G_OBJECT(button), "clicked",
                     G_CALLBACK(desktop_filer_apply), panel);

  button = xpm_button (ICON_CANCEL, NULL, 0, _("Cancel"));
  gtk_box_pack_end (GTK_BOX(layout), button, FALSE, FALSE, 0);
  gtk_button_set_relief (GTK_BUTTON(button), GTK_RELIEF_NONE);
  gtk_widget_show (button);

  g_signal_connect (G_OBJECT(button), "clicked",
                     G_CALLBACK(desktop_filer_cancel), panel);

  /* right margin space */
  label = gtk_label_new ("  ");
  gtk_box_pack_end (GTK_BOX(layout), label, TRUE, TRUE, 0);
  gtk_widget_show (label);

  return desktop;
} /* desktop_new */

/*
* (private) desktop_shortcut - present menu of desktop shortcut
*/
void
desktop_shortcut(Docklet *docklet, ConfigurationNode *node)
{
  GlobalPanel *panel = node->data;	 /* cannot be NULL */
  PanelDesktop *desktop = panel->desktop;

  GtkWidget *menu = desktop->menu;
  GList *list = gtk_container_get_children (GTK_CONTAINER(menu));
  GList *item;

  ConfigurationNode *exec = configuration_find (node, "exec");
  const gchar *icon = configuration_attrib (node, "icon");

  gchar *name = configuration_attrib (node, "name");
  const gchar *file = icon_path_finder (panel->icons, icon);
  gint16 iconsize = desktop->iconsize;

  vdebug(2, "[desktop]%s.icon %s %dx%d\n", name, file, iconsize, iconsize);

  desktop->render = pixbuf_new_from_file_scaled (file, iconsize, iconsize);
  gtk_button_set_image (GTK_BUTTON(desktop->iconview),
                        gtk_image_new_from_pixbuf (desktop->render));

  gtk_entry_set_text (GTK_ENTRY(desktop->name), name);
  gtk_entry_set_text (GTK_ENTRY(desktop->chooser->name), icon);
  gtk_entry_set_text (GTK_ENTRY(desktop->exec), exec->element);

  desktop->node = node;		/* configuration node data */
  desktop->shortcut = docklet;	/* screen representation */

  for (item = list->next; item != NULL; item = item->next) {
    gtk_widget_set_sensitive(GTK_WIDGET(item->data), docklet->editable);
  } 
  gtk_menu_popup (GTK_MENU(menu), NULL, NULL, NULL, NULL, 0, 0);
} /* </desktop_shortcut> */

/*
* (private) desktop_shortcut_callback - callback agent for Docklet events
*/
static bool
desktop_shortcut_callback(DockletDatum *datum)
{
  ConfigurationNode *node = (ConfigurationNode *)datum->payload;
  GdkEvent *event = datum->event;

  if (event->type == GDK_BUTTON_PRESS || event->type == GDK_2BUTTON_PRESS) {
    GdkEventButton *mouse = (GdkEventButton *)event;
    Docklet *docklet = datum->docklet;

    if (mouse->button == 1)
      executer (docklet->window, node);
    else if (mouse->button == 3)
      desktop_shortcut (docklet, node);
  }
  else if (event->type == GDK_CONFIGURE) {
    GdkEventConfigure *box = (GdkEventConfigure *)event;
    desktop_move (node, box->x, box->y);
  }
  return false;
} /* </desktop_shortcut_callback> */

/*
* (private) desktop_settings - change the settings of desktop shortcut
*/
void
desktop_settings(GlobalPanel *panel, DesktopAction action)
{
  PanelDesktop *desktop = panel->desktop;
  GtkWidget *iconview = desktop->iconview;

  if (desktop == NULL) {
    spawn_dialog(100, 100, ICON_ERROR, "desktop_settings: %s.",
				_("assert desktop failed!"));
    return;
  }
  
  if (desktop->active)	/* desktop settings already active */
    return;

  if (action == DESKTOP_SHORTCUT_CREATE) {
    const gchar *file = desktop->icon;
    gint16 iconsize = desktop->iconsize;

    desktop->render = pixbuf_new_from_file_scaled (file, iconsize, iconsize);
    gtk_button_set_image (GTK_BUTTON(iconview),
                          gtk_image_new_from_pixbuf (desktop->render));

    gtk_entry_set_text (GTK_ENTRY(desktop->name), "");
    gtk_entry_set_text (GTK_ENTRY(desktop->exec), "");
    gtk_entry_set_text (GTK_ENTRY(desktop->chooser->name), DefaultIconExec);

    desktop->shortcut = NULL;	/* screen representation */
  }
  else if (action == DESKTOP_SHORTCUT_DELETE) {
    gtk_button_set_relief (GTK_BUTTON(iconview), GTK_RELIEF_NONE);
    gtk_widget_set_sensitive (GTK_WIDGET(iconview), FALSE);

    gtk_entry_set_editable (GTK_ENTRY(desktop->name), FALSE);
    gtk_entry_set_editable (GTK_ENTRY(desktop->exec), FALSE);

    gtk_label_set_text (GTK_LABEL(desktop->hint), _(ActionHintDelete));
  }

  if (action == DESKTOP_SHORTCUT_CREATE || action == DESKTOP_SHORTCUT_EDIT) {
    gtk_button_set_relief (GTK_BUTTON(iconview), GTK_RELIEF_NORMAL);
    gtk_widget_set_sensitive (GTK_WIDGET(iconview), TRUE);

    gtk_entry_set_editable (GTK_ENTRY(desktop->name), TRUE);
    gtk_entry_set_editable (GTK_ENTRY(desktop->exec), TRUE);

    gtk_label_set_text (GTK_LABEL(desktop->hint), _(ActionHintEdit));
  }

  if (action == DESKTOP_SHORTCUT_OPEN) {
    ConfigurationNode *exec = configuration_find (desktop->node, "exec");
    spawn_selected (exec, panel);
  }
  else {
    gtk_widget_hide (panel->settings->window);  /* hide settings window */
    gtk_widget_show (desktop->gwindow);		/* show manager window */
    desktop->active = true;
  }

  desktop->action = action; /* action applied on shortcut */
} /* </desktop_settings> */

/*
* desktop_change - GFileMonitor callback for $HOME/Desktop changes
*/
static inline ConfigurationNode *
desktop_change_node(ConfigurationNode *config, const char *match)
{
  static char chain_markend[MAX_STAMP];
  sprintf(chain_markend, "/%s", AppletName);

  ConfigurationNode *chain = configuration_find(config, AppletName);
  ConfigurationNode *mark  = configuration_find (chain, chain_markend);
  ConfigurationNode *node  = chain;

  gchar *name;		/* expect $HOME/Desktop/<name>.desktop */

  for (; node != mark && node != NULL; node = node->next) {
    if (strcmp(node->element, "item") != 0)	/* not a desktop shortcut */
      continue;

    name = configuration_attrib (node, "name"); /* cannot be null */

    if (name && strncmp(match, name, strlen(name)) == 0)
      break;
  }
  return (node != mark) ? node : NULL;
}

static void
desktop_change(GFileMonitor *monitor, GFile *file, GFile *other,
	       GFileMonitorEvent event_type, GlobalPanel *panel)
{
  char *filename = g_file_get_path (file);
  const char *name = basename(filename);

  ConfigurationNode *chain = configuration_find(panel->config, AppletName);
  ConfigurationNode *node  = desktop_change_node(panel->config, name);

  DesktopEntry *entry = desktop_file_parse (filename);  /* may be NULL */

  switch (event_type) {
    case G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT:
      vdebug(1, "Name => %s, Icon => %s, Exec => %s\n",
		entry->name, entry->icon, entry->exec);
      break;

    case G_FILE_MONITOR_EVENT_CREATED:
      vdebug(1, "%s %s created\n", __func__, filename);
      configuration_write (chain, "<%s>\n", stdout);
      vdebug(1, "Name => %s, Icon => %s, Exec => %s\n",
		entry->name, entry->icon, entry->exec);
      break;

    case G_FILE_MONITOR_EVENT_DELETED:
      vdebug(1, "%s %s deleted\n", __func__, filename);
      configuration_write (chain, "<%s>\n", stdout);
      vdebug(1, ">>> %s will be deleted from $HOME/.config/panel\n", name);
      configuration_remove(node);

      panel->desktop->shortcut = node->widget;
      panel->desktop->action = DESKTOP_SHORTCUT_DELETE;

      desktop_settings_apply (NULL, panel);
      configuration_write (chain, "<%s>\n", stdout);
      break;

    default:
      break;
  }
  g_free (filename);
} /* </desktop_change> */

/*
* desktop_config - add gtk_event_box_new() to the interface layout
*/
void
desktop_config(GlobalPanel *panel, bool once)
{
  static char chain_markend[MAX_STAMP];
  sprintf(chain_markend, "/%s", AppletName);

  ConfigurationNode *node = configuration_find (panel->config, AppletName);
  ConfigurationNode *mark = configuration_find (node, chain_markend);

  PanelIcons *icons = panel->icons;
  guint depth = node->depth + 1;
  const gchar *icon;

  //gint16 width  = gdk_screen_width();
  gint16 height = gdk_screen_height();

  gint16 xpos = -1;	/* used to calculate position */
  gint16 ypos = -1;

  gint16 step = 70;	/* should be computed from font and icon */
  gint16 xorg = 5;	/* position for a new shortcut */
  gint16 yorg = 5;

  gchar *font, *name;
  gchar *init  = configuration_attrib(node, "init");
  gchar *value = configuration_attrib(node, "iconsize");

  /* Prefer the iconsize for <desktop> */
  guint iconsize = settings_.iconsize = (value) ? atoi(value) : icons->size;

  /* See if font attribute is set on <desktop> */
  if ((font = configuration_attrib(node, "font")) == NULL)
    font = "Times 12";

  for (; node != mark && node != NULL; node = node->next) {
    if (node->depth == depth && node->type != XML_READER_TYPE_END_ELEMENT) {
      unsigned short editable = 15;	/* priv | open | customize | remove */
      ConfigurationNode *item;

      if (strcmp(node->element, "item") != 0)	/* not a desktop shortcut */
        continue;

      /* Parse for shortcut entry. */
      name = configuration_attrib (node, "name");
      icon = NULL;

      if ((value = configuration_attrib (node, "icon")) != NULL)
        icon = icon_path_finder (icons, value);

      /* A shortcut must have a name or an icon. */
      if(icon == NULL && name == NULL) continue;

      /* Re-initialize coordinates. */
      xpos = ypos = -1;

      if ((item = configuration_find(node, "xpos")) != NULL)
        xpos = atoi(item->element);

      if ((item = configuration_find(node, "ypos")) != NULL)
        ypos = atoi(item->element);

      /* The initial position must be specified. */
      if(xpos < 0 || ypos < 0) continue;

      /* Adjust position for addition of a new shortcut. */
      if (ypos > yorg) {
        if (ypos < height - 2 * step) {
          yorg = ypos;
        }
        else {
          xorg += step;
          yorg = 5 - step;
        }
      }
      else if (xpos > xorg + iconsize) {
        xorg = xpos;
        yorg = ypos;
      }
    
      /* Substitute "blank.png" if icon == NULL */
      if(icon == NULL) icon = icon_path_finder (icons, "blank.png");

      /* A desktop item is a Docklet instance, create once. */
      if (once) {
        Docklet *docklet = desktop_shortcut_create (panel, node,
						    iconsize, iconsize,
						    xpos, ypos,
						    icon, name, font,
						    NULL, NULL, false);
        gtk_widget_show (docklet->window);

        if ((value = configuration_attrib (node, "editable")) != NULL)
          editable = atoi(value);

        docklet->editable = (editable & 3);
      }
    }
  }

  /* Construct panel->desktop for creating and editing desktop shortcuts. */
  if (once) {
    PanelDesktop *desktop = desktop_new (panel, iconsize);

    GFile *file = g_file_new_for_path (desktop->folder);
    GFileMonitorFlags flags = G_FILE_MONITOR_SEND_MOVED;

    if (init) {			/* init before g_file_monitor_directory */
      desktop->init = init;
      dispatch (panel->session, init);
    }

    desktop->monitor = g_file_monitor_directory (file, flags, NULL, NULL);
    g_signal_connect (desktop->monitor, "changed",
			G_CALLBACK(desktop_change), panel);
    g_object_unref (file);

    desktop->step  = step;	/* used to position next shortcut */
    panel->desktop = desktop;
  }

  panel->desktop->xpos = xorg;
  if(yorg > panel->desktop->ypos) panel->desktop->ypos = yorg;

  /* initialize settings_.fontsel */
  for (int idx = 0; idx < DesktopFontMax; idx++) {
    if (strcmp(font, DesktopFont[idx]) == 0) {
      settings_.fontsel = idx;
      break;
    }
  }
  vdebug(1, "%s font => %s, iconsize => %d\n", __func__,
			DesktopFont[settings_.fontsel], iconsize);
} /* </desktop_config> */

/*
* desktop_shortcut_create - instantiate a new desktop shortcut 
*/
Docklet *
desktop_shortcut_create(GlobalPanel *panel,
			ConfigurationNode *node,
			gint16 width, gint16 height,
			gint16 xpos, gint16 ypos,
			const gchar *icon,
			const gchar *text,
			const gchar *font,
			GdkColor *bg,
			GdkColor *fg,
                        bool shadow)
{
  Docklet *docklet = docklet_new (GDK_WINDOW_TYPE_HINT_NORMAL,
                                  width, height, xpos, ypos,
                                  GTK_ORIENTATION_VERTICAL,
                                  GTK_VISIBILITY_NONE,
                                  icon, text, font,
                                  bg, fg, shadow);
  node->widget = docklet;
  node->data   = panel;	/* set node->data madatory */

  docklet_set_callback (docklet, (gpointer)desktop_shortcut_callback, node);
  gtk_window_set_keep_below (GTK_WINDOW(docklet->window), TRUE);

  return docklet;
} /* </desktop_shortcut_create> */

/*
* desktop_shortcut_render - similiar to docklet_render(libgould.so)
*/
GdkPixbuf *
desktop_shortcut_render (GdkPixbuf *piximg, const char *label)
{
  const char *font = DesktopFont[settings_.fontsel];

  GtkWidget *canvas = settings_.preview;
  GdkWindow *gdkwindow = gtk_widget_get_window (canvas);
  GdkColormap *colormap = gdk_colormap_get_system ();
  GdkGC *gc = gdk_gc_new (gdkwindow);

  PangoFontDescription *fontdesc = pango_font_description_from_string (font);
  PangoLayout *layout = gtk_widget_create_pango_layout (canvas, label);

  GdkPixmap *pixmap;
  GdkPixbuf *render;	/* resulting pixbuf */

  GdkColor black, white;
  GdkColor *bg = NULL;  /* transparent */
  GdkColor *fg;

  gint16 iconsize = gdk_pixbuf_get_width (piximg);
  gint16 wrap = 3 * iconsize / 2;
  gint width, height;
  gint xsize, ysize;
  gint xpos, ypos;

  /* Set font according to the PangoFontDescription. */
  pango_layout_set_font_description (layout, fontdesc);
  pango_font_description_free (fontdesc);

  /* Obtain the pixel width and height of the text. */
  pango_layout_get_pixel_size (layout, &xsize, &ysize);

  pango_layout_set_width (layout, wrap * PANGO_SCALE);
  pango_layout_set_wrap (layout, PANGO_WRAP_WORD_CHAR);

  xpos = (xsize > iconsize) ? (xsize-iconsize) / 2 : -1;
  width = (xsize > iconsize) ? xsize : iconsize;
  height = iconsize + ysize;

  /* Create a pixmap for the drawing area. */
  render = xpm_pixbuf_scale (ICON_BLANK, width, height, NULL);
  gdk_pixbuf_render_pixmap_and_mask (render, &pixmap, NULL, 255);

  /* Draw the image pixbuf passed. */
  gdk_draw_pixbuf (pixmap, gc, piximg, 0, 0,
		   xpos, 0, -1, -1, GDK_RGB_DITHER_NORMAL, 0 , 0);

  /* Draw the label text passed. */
  gdk_color_black (colormap, &black);
  gdk_color_white (colormap, &white);
  fg = &white;

  xpos = (iconsize > xsize) ? (iconsize-xsize) / 2 : -1;
  ypos = iconsize;

  gdk_draw_layout_with_colors (pixmap, gc, xpos, ypos, layout, fg, bg);
  //gdk_draw_layout (pixmap, gc, xpos, ypos, layout);

  /* Convert drawable to a pixbuf. */
  render = gdk_pixbuf_get_from_drawable(NULL, pixmap, colormap,
                                        0, 0, 0, 0, -1, -1);
  g_object_unref (pixmap);
  g_object_unref (gc);

  return render;
} /* </desktop_shortcut_render> */

/*
* desktop_shortcut_preview
*/
void
desktop_shortcut_preview(GtkWidget *canvas, GlobalPanel *panel)
{
  if(GDK_IS_DRAWABLE(canvas->window) == FALSE) return;

  GdkPixbuf *piximg = pixbuf_new_from_file_scaled (settings_.iconpath,
						   settings_.iconsize,
						   settings_.iconsize);
  if (piximg) {
    GdkPixbuf *pixbuf = desktop_shortcut_render(piximg, _(DefaultShortcutName));
    /* redraw_pixbuf() always clears the drawing area */
    redraw_pixbuf (canvas, pixbuf);
    g_object_unref (pixbuf);
  }
} /* </desktop_shortcut_preview> */

/*
* desktop_icon_size
*/
static void
desktop_icon_size(GtkWidget *button, gpointer scale)
{
  gint16 iconsize = GPOINTER_TO_INT (scale);

  if (settings_.iconsize != iconsize) {
    GlobalPanel *panel = settings_.panel;
    PanelDesktop *desktop = panel->desktop;

    desktop->action = DESKTOP_SAVECONFIG;
    settings_.iconsize = iconsize;

    vdebug(1, "%s iconfile => %s, iconsize => %d\n", __func__,
			basename(settings_.iconpath), iconsize);

    desktop_shortcut_preview (settings_.preview, panel);
    desktop_settings_enable (panel->settings);  // register callbacks
  }
} /* </desktop_icon_size> */

/*
* dialog_iconsize_radio_button
*/
GtkWidget *
dialog_iconsize_radio_button(GSList *radiogroup, gint iconsize)
{
  static char label[MAX_LABEL];
  sprintf(label, "%dx%d", iconsize, iconsize);
  GtkWidget *button = gtk_radio_button_new_with_label (radiogroup, label);

  g_signal_connect (G_OBJECT(button), "clicked",
                    G_CALLBACK(desktop_icon_size),
		    GINT_TO_POINTER (iconsize));
  return button;
} /* </dialog_iconsize_radio_button> */

/*
* desktop_icon_font - set icon font according to selection
*/
static void
desktop_icon_font(GtkComboBox *combo, GlobalPanel *panel)
{
  gint selection = gtk_combo_box_get_active (combo);

  if (settings_.fontsel != selection) {
    PanelDesktop *desktop = panel->desktop;

    desktop->action = DESKTOP_SAVECONFIG;
    settings_.fontsel = selection;

    vdebug(1, "%s DesktopFont[%d] => %s\n", __func__,
				selection, DesktopFont[selection]);

    desktop_shortcut_preview (settings_.preview, panel);
    desktop_settings_enable (panel->settings);  // register callbacks
  }
} /* </desktop_icon_font> */

/*
* desktop_settings_new
*/
Modulus *
desktop_settings_new(GlobalPanel *panel)
{
  GSList *radiogroup = NULL;
  GtkWidget *area, *button, *canvas, *combo;
  GtkWidget *frame, *glue, *label, *layer, *radio;
  GtkWidget *layout = gtk_vbox_new (FALSE, 2);
  PanelDesktop *desktop = panel->desktop;

  static Modulus _applet;
  static char iconpath[UNIX_PATH_MAX];
  Modulus *applet = &_applet;
  int idx;

  /* Modulus structure initialization. */
  applet->data  = panel;
  applet->name  = AppletName;
  applet->label = _("Desktop");
  applet->description = _("Desktop settings module.");
  applet->release  = Release;
  applet->authors  = Authors;
  applet->settings = layout;

  /* DesktopSettings initialization. */
  // .fontsel and .iconsize initialized in desktop_config()
  sprintf(iconpath, "%s", icon_path_finder (panel->icons, DefaultIconHome));
  settings_.iconpath = iconpath;
  settings_.panel = panel;

  static char caption[MAX_BUFFER_SIZE];
  sprintf(caption, "%s [builtin]", applet->label);

  frame = gtk_frame_new (caption);
  gtk_frame_set_shadow_type (GTK_FRAME(frame), GTK_SHADOW_ETCHED_IN);
  gtk_container_set_border_width (GTK_CONTAINER (frame), 4);
  gtk_container_add (GTK_CONTAINER(layout), frame);
  gtk_widget_show (frame);

  /* Area inside the frame. */
  area = gtk_vbox_new (TRUE, 4);
  gtk_container_add (GTK_CONTAINER(frame), area);
  gtk_widget_show (area);

  /* Add selection widget for iconsize. */
  layer = gtk_hbox_new (FALSE, 8);
  gtk_box_pack_start (GTK_BOX(area), layer, FALSE, FALSE, 0);
  gtk_widget_show (layer);

  /* Add spacing on the left side. */
  glue = gtk_label_new ("\t ");
  gtk_box_pack_start (GTK_BOX(layer), glue, FALSE, FALSE, 0);
  gtk_widget_show (glue);

  /* Add radio buttons for iconsize. */
  radio = gtk_vbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX(layer), radio, FALSE, FALSE, 0);
  gtk_widget_show (radio);

  frame = gtk_frame_new (_("Icon Size"));
  gtk_container_set_border_width (GTK_CONTAINER (frame), 4);
  gtk_container_add (GTK_CONTAINER(radio), frame);
  gtk_widget_show (frame);

  for (idx = 0; idx < iconsize_maxsel; idx++) {
    button = dialog_iconsize_radio_button (radiogroup, iconsize_selected[idx]);
    gtk_box_pack_start (GTK_BOX(radio), button, FALSE, FALSE, 0);
    radiogroup = g_slist_append (radiogroup, button);
    gtk_widget_show (button);
  }
  desktop->icongroup = radiogroup;	// make availabe as protected

  /* Add font selection combo box. */
  glue = gtk_vbox_new (FALSE, 8);
  gtk_box_pack_start (GTK_BOX(layer), glue, FALSE, FALSE, 0);
  gtk_widget_show (glue);

  area = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX(glue), area, FALSE, FALSE, 0);
  gtk_widget_show (area);

  label = gtk_label_new (_("Font: "));
  gtk_box_pack_start (GTK_BOX(area), label, FALSE, FALSE, 0);
  gtk_widget_show (label);

  combo = gtk_combo_box_new_text ();
  for (idx = 0; idx < DesktopFontMax; idx++) {
    gtk_combo_box_append_text(GTK_COMBO_BOX(combo), DesktopFont[idx]);
  }
  gtk_combo_box_set_active (GTK_COMBO_BOX(combo), settings_.fontsel);
  gtk_box_pack_start (GTK_BOX(area), combo, FALSE, FALSE, 0);
  gtk_widget_show (combo);

  g_signal_connect (G_OBJECT(combo), "changed",
                      G_CALLBACK (desktop_icon_font), panel);

  label = gtk_label_new ("\t");
  gtk_box_pack_start (GTK_BOX(area), label, FALSE, FALSE, 0);
  gtk_widget_show (label);

  /* Add canvas for preview area. */
  canvas = settings_.preview = gtk_drawing_area_new ();
  gtk_box_pack_start (GTK_BOX(layer), canvas, FALSE, FALSE, 0);
  gtk_widget_set_size_request (canvas, PREVIEW_VIEW_WIDTH, PREVIEW_VIEW_HEIGHT);
  gtk_widget_show (canvas);

  /* Signals used to handle backing pixmap. */
  g_signal_connect (G_OBJECT (canvas), "expose_event",
                    G_CALLBACK (desktop_shortcut_preview), panel);
  return applet;
} /* </desktop_settings_new> */

/*
* desktop_default_iconsize - (public) set iconsize according to configuration
*/
void
desktop_default_iconsize(GlobalPanel *panel)
{
  PanelDesktop *desktop = panel->desktop;
  GSList *iter = desktop->icongroup;

  for (int idx = 0; idx < iconsize_maxsel; idx++) {
    if (iconsize_selected[idx] == settings_.iconsize) {
      GtkWidget *button = (GtkWidget *)iter->data;
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(button), TRUE);
      settings_save_enable (panel->settings, false);
      break;
    }
    iter = iter->next;
  }
} /* </desktop_default_iconsize> */
