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
#include "sha1.h"

#include <libgen.h>	/* basename(3),dirname(3) declarations */

#define desktop_file_get_string(file, key) \
	g_key_file_get_string(file, G_KEY_FILE_DESKTOP_GROUP, key, NULL)

#define PREVIEW_VIEW_WIDTH  128
#define PREVIEW_VIEW_HEIGHT 128

extern const char *Program, *Release;	     /* see, gpanel.c */
extern debug_t debug;			     /* .. */

static gchar *ActionHintDelete = "Press the apply button to delete.";
static gchar *ActionHintEdit   = "Click on the icon to change it.";

static gchar *DefaultIconHome = "home.png";  /* default icon for shortcut */
static gchar *DefaultIconExec = "exec.png";  /* default icon for programs */

static char *DefaultShortcutName = "Home";   /* default label for shortcut */
static char *DesktopExtension = ".desktop";  /* ~/Desktop/{file} extension */
static char *AppletName = "desktop";

static guint8 iconsize_maxsel = 5; // must match iconsize_selected[] array size
static gint iconsize_selected[5] = {24, 32, 48, 64, 96};

/*
* Data structures used by this module.
*/
typedef struct _DesktopItem DesktopItem;
typedef struct _DesktopSettings DesktopSettings;

struct _DesktopItem {
  gchar *sha1;			/* sha1sum( {corename}.desktop ) */
  gchar *name;			/* {corename}.desktop Name={name} */
  gchar *comment;		/* {corename}.desktop Comment={comment} */
  gchar *icon;			/* {corename}.desktop Icon={icon} */
  gchar *exec;			/* {corename}.desktop Exec={exec} */
  gint16 xpos;			/* saved desktop x-coordinate */
  gint16 ypos;			/* saved desktop y-coordinate */
};

struct _DesktopSettings {
  GlobalPanel *panel;		/* GlobalPanel instance */
  GHashTable *filehash;		/* GKeyFile(s) hash table */
  GtkWidget *preview;		/* shortcut preview pane */
  const char *filepath;		/* g_file_monitor_directory(file, */
  const char *iconpath;		/* preview icon file */
  gint16 iconsize;		/* 24 => 24x24, ... */
  guint8 fontsel;		/* font array index */
  guint monitor;		/* g_signal_connect(desktop->monitor, */
};

const char *DesktopFileSpec =
"[Desktop Entry]\n"
"Version=1.0\n"
"Name=%s\n"
"Comment=%s\n"
"Icon=%s\n"
"Exec=%s\n"
"Terminal=false\n"
"Type=Application\n"
"Categories=;\n";

const char *DesktopItemSpec =
"<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
"<item name=\"%s\" icon=\"%s\">\n"
" <sha1>%s</sha1>\n"
" <comment>%s</comment>\n"
" <exec>%s</exec>\n"
" <xpos>%d</xpos>\n"
" <ypos>%d</ypos>\n"
"</item>\n";

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

DesktopSettings settings_;	   /* private DesktopSettings to this module */

/* Forward prototype declarations. */
static void desktop_change_cb(GFileMonitor *monitor, GFile *file, GFile *other,
			      GFileMonitorEvent event_type, GlobalPanel *panel);

bool desktop_shortcut_create(GlobalPanel *panel, const char *ident);
void desktop_shortcut_remove(ConfigurationNode *node);

Docklet *desktop_shortcut_new(GlobalPanel *panel,
			      ConfigurationNode *node,
			      gint16 width, gint16 height,
			      gint16 xpos, gint16 ypos,
			      const gchar *icon,
			      const gchar *text,
			      const gchar *font,
			      GdkColor *bg,
			      GdkColor *fg,
			      bool shadow);

/*
* (private) desktop_parse_file - {name}.desktop parser
*/
static DesktopItem *
desktop_file_parse(const char *filepath)
{
  DesktopItem *item = NULL;
  g_autoptr(GKeyFile) file = g_key_file_new ();
  //GKeyFile *file = g_key_file_new ();

  if (g_key_file_load_from_file (file, filepath, 0, NULL)) {
    static DesktopItem entry;

    entry.sha1 = (char *)sha1sum (filepath);
    entry.name = desktop_file_get_string (file, "Name");
    entry.comment = desktop_file_get_string (file, "Comment");
    entry.icon = desktop_file_get_string (file, "Icon");
    entry.exec = desktop_file_get_string (file, "Exec");

    item = &entry;
  }
  //g_key_file_free (file);
  return item;
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
  static char endmark[MAX_STAMP];
  sprintf(endmark, "/%s", AppletName);

  PanelDesktop *desktop = panel->desktop;
  DesktopAction action = desktop->action;
  Docklet *docklet = desktop->shortcut;

  /* Dismiss the desktop->gwindow interface. */
  gtk_widget_hide (desktop->gwindow);
  desktop->active = false;

  /* Handle the user specified action. */
  if (action == DESKTOP_SAVECONFIG) {
    ConfigurationNode *node = configuration_find (panel->config, AppletName);

    if (node != NULL) {			    /* should never happen! */
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
      gpanel_restart (panel, SIGURG);	     /* signal program restart */
    }
  }
  else if (action == DESKTOP_SHORTCUT_DELETE) {
    vdebug(1, "%s desktop->node => 0x%lx [widget => 0x%lx], docklet => 0x%lx\n",
		__func__, desktop->node, desktop->node->widget, docklet);

    desktop_shortcut_remove (desktop->node); /* remove from configuration */
    if(docklet == NULL) docklet = desktop->node->widget; /* rough hack */

    gtk_widget_destroy (docklet->window);    /* remove from screen display */
    g_object_ref_sink (docklet);	     /* clean-up memory */
    g_object_unref (docklet);

    saveconfig (panel);  // coup d'�tat
  }
  else {
    FileChooser *chooser = desktop->chooser;
    const gchar *iconname = gtk_entry_get_text (GTK_ENTRY(chooser->name));
    const gchar *iconpath = filechooser_get_selected_name (chooser);

    const gchar *name = gtk_entry_get_text (GTK_ENTRY(desktop->name));
    //const gchar *comment = gtk_entry_get_text (GTK_ENTRY(desktop->comment));
    const gchar *program  = gtk_entry_get_text (GTK_ENTRY(desktop->exec));

    const char *ident;  /* sha1sum identifier */

    if (icon_path_finder (panel->icons, iconname) == NULL)
      iconname = iconpath;

    if (action == DESKTOP_SHORTCUT_CREATE) {
      const char separator = ' ';
      char comment[MAX_LABEL];
      char *scan = strchr(program, separator);

      if(scan != NULL) *scan = (char)0;
      strcpy(comment, program);	// program without command line arguments
      if(scan != NULL) *scan = separator;

      if (settings_.filepath && access(settings_.filepath, R_OK) == 0)
        ident = sha1sum (settings_.filepath);
      else {
        static char filepath[UNIX_PATH_MAX];
        sprintf(filepath, "%s/Desktop/%s.desktop", getenv("HOME"), program);
        settings_.filepath = filepath;

        if (access(filepath, R_OK) == 0)  // avoid duplicate
          ident = sha1sum (filepath);
        else {
          FILE *stream = fopen(filepath, "w");
          g_signal_handler_disconnect (G_OBJECT (desktop->monitor),
					     settings_.monitor);
          if (stream) {
            const char separator = '.';
            scan = strrchr(iconname, separator);

            if(scan) *scan = (char)0;
            fprintf(stream, DesktopFileSpec, name, comment, iconname, program);
            if(scan) *scan = separator;
            fclose(stream);
          }
          sleep(1);  // prevent g_file_monitor_directory acting on filepath
          settings_.monitor = g_signal_connect (desktop->monitor, "changed",
				    G_CALLBACK(desktop_change_cb), panel);

          ident = (stream) ? sha1sum (filepath) : "0";
        }
      }

      if (g_hash_table_lookup (settings_.filehash, ident) == NULL) {
        desktop_shortcut_create (panel, ident);
      }
    }
    else if (action == DESKTOP_SHORTCUT_EDIT) {
      ConfigurationNode *node = desktop->node;
      ConfigurationAttrib *attrib = node->attrib;
      //ConfigurationNode *sha1 = configuration_find (node, "sha1");
      ConfigurationNode *exec = configuration_find (node, "exec");

      /* Deallocate and reconstruct attrib link list. */
      configuration_attrib_remove (attrib);

      attrib = node->attrib = g_new0 (ConfigurationAttrib, 1);
      attrib->name  = "name";
      //attrib->name  = g_strdup ("name");
      attrib->value = g_strdup (name);

      attrib = attrib->next = g_new0 (ConfigurationAttrib, 1);
      attrib->name  = "icon";
      //attrib->name  = g_strdup ("icon");
      attrib->value = g_strdup (iconname);

      /* Deallocate and reconstruct sha1->element
      g_free (sha1->element);
      sha1->element = g_strdup (sha1sum);
      */

      /* Deallocate and reconstruct exec->element */
      g_free (exec->element);
      exec->element = g_strdup (program);

      docklet_update (docklet, iconpath, name);	/* update screen display */
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
* desktop_panel_new - object for creating and editing desktop shortcuts
*/
PanelDesktop *
desktop_panel_new(GlobalPanel *panel, gint16 iconsize)
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
} /* desktop_panel_new */

/*
* desktop_shortcut_menu - present menu of desktop shortcut
*/
void
desktop_shortcut_menu(Docklet *docklet, ConfigurationNode *node)
{
  GlobalPanel *panel = node->data;	 /* cannot be NULL */
  PanelDesktop *desktop = panel->desktop;

  GtkWidget *menu = desktop->menu;
  GList *list = gtk_container_get_children (GTK_CONTAINER(menu));

  ConfigurationNode *exec = configuration_find (node, "exec");
  gchar *name = configuration_attrib (node, "name");

  const gchar *icon = configuration_attrib (node, "icon");
  const gchar *file = icon_path_finder (panel->icons, icon);
  gint16 iconsize = desktop->iconsize;

  vdebug(2, "%s node => 0x%lx, docklet => 0x%lx\n", __func__, node, docklet);
  vdebug(2, "%s icon => %s [%dx%d], name => %s, exec => %s\n",
		__func__, file, iconsize, iconsize, name, exec->element);

  desktop->render = pixbuf_new_from_file_scaled (file, iconsize, iconsize);
  gtk_button_set_image (GTK_BUTTON(desktop->iconview),
                        gtk_image_new_from_pixbuf (desktop->render));

  gtk_entry_set_text (GTK_ENTRY(desktop->name), name);
  gtk_entry_set_text (GTK_ENTRY(desktop->chooser->name), icon);
  gtk_entry_set_text (GTK_ENTRY(desktop->exec), exec->element);

  desktop->node = node;		/* configuration node data */
  desktop->shortcut = docklet;	/* screen representation */

  for (GList *item = list->next; item != NULL; item = item->next) {
    gtk_widget_set_sensitive(GTK_WIDGET(item->data), docklet->editable);
  } 
  gtk_menu_popup (GTK_MENU(menu), NULL, NULL, NULL, NULL, 0, 0);
} /* </desktop_shortcut_menu> */

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
      desktop_shortcut_menu (docklet, node);
  }
  else if (event->type == GDK_CONFIGURE) {
    GdkEventConfigure *box = (GdkEventConfigure *)event;
    desktop_move (node, box->x, box->y);
  }
  return false;
} /* </desktop_shortcut_callback> */

/*
* desktop_settings_agent - change the settings of desktop shortcut
*/
void
desktop_settings_agent(GlobalPanel *panel, DesktopAction action)
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

  desktop->action = action;   /* action applied on shortcut */
} /* </desktop_settings_agent> */

/*
* desktop_find_deleted_node - locate deleted node by sha1sum
*/
static ConfigurationNode *
desktop_find_deleted_node(const char *desktopdir, const char *filepath)
{
  struct dirent **names;
  char desktopfile[UNIX_PATH_MAX];
  int count = scandir(desktopdir, &names, NULL, alphasort);
  size_t bytes = strlen(desktopdir) + 2;  /* '/' and null characters */
  const char *sha1;
  char *name;

  ConfigurationNode *node = NULL;
  GHashTable *filehash = g_hash_table_new (g_str_hash, g_str_equal);
  GHashTableIter iter;
  gpointer key, value;


  /* iterate settings_.filehash to populate local filehash */
  g_hash_table_iter_init (&iter, settings_.filehash);

  while (g_hash_table_iter_next (&iter, &key, &value)) {
    vdebug(3, "%s key => %s, value => 0x%lx\n", __func__, key, value);
    g_hash_table_insert (filehash, key, value);
  }

  for (int idx = 0; idx < count; idx++) {
    name = names[idx]->d_name;
    if(name[0] == '.' || strstr(name, DesktopExtension) == NULL) continue;

    snprintf(desktopfile, bytes+strlen(name), "%s/%s", desktopdir, name);
    sha1 = sha1sum (desktopfile);

    /* iterate filehash to find missing node */
    g_hash_table_iter_init (&iter, filehash);

    while (g_hash_table_iter_next (&iter, &key, &value)) {
      if (strcmp(sha1, key) == 0) {
        vdebug(2, "%s %s %s [node => 0x%lx]\n", __func__, sha1, name, value);
        g_hash_table_remove (filehash, key);
        break;
      }
    }
  }

  /* only filepath entry left in filehash */
  g_hash_table_iter_init (&iter, filehash);
  g_hash_table_iter_next (&iter, &key, &value);

  if (key != NULL) {
    vdebug(2, "%s key => %s, node => 0x%lx\n", __func__, key, value);
    node = (ConfigurationNode *)value;

    g_free (key);  // free memory allocated for (sha1)key
    g_hash_table_destroy (filehash);

    if (access(filepath, R_OK) == 0) {
      if (unlink(filepath) == 0)
        vdebug(2, "%s %s: deleted.\n", __func__, filepath);
      else
        vdebug(2, "%s %s: not found.\n", __func__, filepath);
    }
  }
  return node;
} /* </desktop_find_deleted_node> */

/*
* desktop_change_agent - workhorse of adding/changing/deleting a .desktop file
*/
void
desktop_change_agent(const char *filepath, GFileMonitorEvent event_type,
						GlobalPanel *panel)
{
  static char iconame[MAX_LABEL];

  ConfigurationNode *chain = configuration_find(panel->config, AppletName);
  DesktopItem *entry = desktop_file_parse (filepath);

  PanelDesktop *desktop = panel->desktop;
  FileChooser  *chooser = desktop->chooser;

  settings_.filepath = filepath;	// global within module


  switch (event_type) {
    case G_FILE_MONITOR_EVENT_ATTRIBUTE_CHANGED:
      break;

    case G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT:
      vdebug(2, "%s G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT %s\n",
				__func__, filepath);
      break;

    case G_FILE_MONITOR_EVENT_CREATED:
      if(debug > 1) configuration_write (chain, "<%s>\n", stdout);
      vdebug(1, "%s filepath => %s\n", __func__, filepath);

      strcpy(iconame, basename(entry->icon));
      if (strrchr(iconame, '.') == NULL)   // .png file default
        sprintf(iconame, "%s.png", basename(entry->icon));

      gtk_entry_set_text(GTK_ENTRY(chooser->name), iconame);
      gtk_entry_set_text(GTK_ENTRY(desktop->name), entry->name);
      gtk_entry_set_text(GTK_ENTRY(desktop->exec), entry->exec);

      desktop->action = DESKTOP_SHORTCUT_CREATE;
      desktop_settings_apply (NULL, panel);

      if(debug > 1) configuration_write (chain, "<%s>\n", stdout);
      saveconfig (panel);  // coup d'�tat
      break;

    case G_FILE_MONITOR_EVENT_DELETED:
      if(debug > 1) configuration_write (chain, "<%s>\n", stdout);
      vdebug(1, "%s %s deleted\n", __func__, filepath);

      /* locate deleted node by sha1sum */
      desktop->node = desktop_find_deleted_node (desktop->folder, filepath);

      if (desktop->node != NULL) {
        desktop->action = DESKTOP_SHORTCUT_DELETE;
        desktop_settings_apply (NULL, panel);

        if(debug > 1) configuration_write (chain, "<%s>\n", stdout);
        saveconfig (panel);  // coup d'�tat
      }
      break;

    default:
      break;
  }
} /* </desktop_change_agent> */

/*
* desktop_change_cb - GFileMonitor callback for $HOME/Desktop changes
*/
static void
desktop_change_cb(GFileMonitor *monitor, GFile *file, GFile *other,
		  GFileMonitorEvent event_type, GlobalPanel *panel)
{
  static char endmark[MAX_STAMP];
  sprintf(endmark, "/%s", AppletName);

  static char filepath[UNIX_PATH_MAX];
  char *scan = g_file_get_path (file);
  strcpy(filepath, scan);
  g_free (scan);

  /* only monitor $HOME/Desktop/{name}.desktop files */
  scan = strrchr(basename(filepath), '.');

  if (scan && strcmp(scan, DesktopExtension) == 0) {
    vdebug(2, "%s filepath => %s, event_type => %d\n",
			__func__, filepath, event_type);
    desktop_change_agent (filepath, event_type, panel);
  }
  else
    return;
} /* </desktop_change_cb> */

/*
* desktop_config - add gtk_event_box_new() to the interface layout
*/
void
desktop_config(GlobalPanel *panel, bool once)
{
  static char endmark[MAX_STAMP];
  sprintf(endmark, "/%s", AppletName);

  ConfigurationNode *node = configuration_find (panel->config, AppletName);
  ConfigurationNode *mark = configuration_find (node, endmark);
  ConfigurationNode *item;

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

  gchar *font;
  gchar *name;
  gchar *sha1;

  gchar *init  = configuration_attrib(node, "init");
  gchar *value = configuration_attrib(node, "iconsize");

  /* Prefer the iconsize for <desktop> */
  guint iconsize = settings_.iconsize = (value) ? atoi(value) : icons->size;

  /* See if font attribute is set on <desktop> */
  if ((font = configuration_attrib(node, "font")) == NULL)
    font = "Times 12";

  if (once) {
    settings_.filehash = g_hash_table_new (g_str_hash, g_str_equal);
    vdebug(2, "%s g_hash_table_new => 0x%lx\n", __func__, settings_.filehash); 
  }

  for (; node != mark && node != NULL; node = node->next) {
    if (node->depth == depth && node->type != XML_READER_TYPE_END_ELEMENT) {
      unsigned short editable = 15;	/* priv | open | customize | remove */

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
        vdebug(1, "%s name => %s, icon => %s, xpos => %d, ypos => %d\n",
		__func__, name, icon, xpos, ypos);

        Docklet *docklet = desktop_shortcut_new (panel, node,
						 iconsize, iconsize,
						 xpos, ypos,
						 icon, name, font,
						 NULL, NULL, false);
        gtk_widget_show (docklet->window);

        if ((value = configuration_attrib (node, "editable")) != NULL)
          editable = atoi(value);

        //docklet->editable = (editable & 3);	/* mask accordingly */
        docklet->editable = editable;		/* mask accordingly */

        /* Add [sha1, ConfigurationNode*] to settings_.filehash */
        if ((item = configuration_find(node, "sha1")) != NULL) {
          sha1 = item->element;
          g_hash_table_insert (settings_.filehash, sha1, node);
          vdebug(1, "%s sha1 => %s, node => 0x%lx\n", __func__, sha1, node);
        }
      }
    }
  }

  /* Construct panel->desktop for creating and editing desktop shortcuts. */
  if (once) {
    PanelDesktop *desktop = desktop_panel_new (panel, iconsize);

    GFile *file = g_file_new_for_path (desktop->folder);
    GFileMonitorFlags flags = G_FILE_MONITOR_SEND_MOVED;

    if (init) {			/* init before g_file_monitor_directory */
      desktop->init = init;
      dispatch (panel->session, init);
    }

    desktop->monitor = g_file_monitor_directory (file, flags, NULL, NULL);
    settings_.monitor = g_signal_connect (desktop->monitor, "changed",
				G_CALLBACK(desktop_change_cb), panel);
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
* desktop_shortcut_new - new desktop shortcut docklet
*/
Docklet *
desktop_shortcut_new(GlobalPanel *panel,
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

  node->data   = panel;		/* set node->data madatory */
  node->widget = docklet;

  docklet_set_callback (docklet, (gpointer)desktop_shortcut_callback, node);
  gtk_window_set_keep_below (GTK_WINDOW(docklet->window), TRUE);

  return docklet;
} /* </desktop_shortcut_new> */

/*
* desktop_shortcut_create - instantiate a new desktop shortcut 
*/
bool
desktop_shortcut_create(GlobalPanel *panel, const char *ident)
{
  static char endmark[MAX_STAMP];
  sprintf(endmark, "/%s", AppletName);

  PanelDesktop *desktop = panel->desktop;
  FileChooser *chooser = desktop->chooser;

  ConfigurationNode *chain = configuration_find (panel->config, AppletName);
  ConfigurationNode *mark;
  ConfigurationNode *item;

  const gchar *iconname = gtk_entry_get_text (GTK_ENTRY(chooser->name));
  const gchar *iconpath = filechooser_get_selected_name (chooser);
  const gchar *font = configuration_attrib (chain, "font");

  const gchar *name = gtk_entry_get_text (GTK_ENTRY(desktop->name));
  //const gchar *program  = gtk_entry_get_text (GTK_ENTRY(desktop->comment));
  const gchar *program  = gtk_entry_get_text (GTK_ENTRY(desktop->exec));

  const char separator = ' ';
  char comment[MAX_LABEL];
  char *scan = strchr(program, separator);

  if(scan != NULL) *scan = (char)0;
  strcpy(comment, program);	// program without command line arguments
  if(scan != NULL) *scan = separator;

  char spec[MAX_PATHNAME];

  gint16 xpos;
  gint16 ypos;



  /* Calculate where to place and form the XML. */
  desktop_config (panel, false);

  xpos = desktop->xpos;
  ypos = desktop->ypos + desktop->step;

  sprintf(spec, DesktopItemSpec, name, iconname,
		ident, comment, program, xpos, ypos);

  item = configuration_read (spec, NULL, true);
  mark = configuration_find (chain, endmark);

  vdebug(1, "%s %s 0x%lx\n", __func__, ident, item);

  if (mark == NULL) {	/* <desktop ... /> case */
    if ( (mark = configuration_find (chain, "/")) ) {
      g_free (mark->element);
      mark->element = g_strdup (endmark);
    }
    else {
      return false;
    }
  }
  configuration_insert (item, mark->back, mark->depth + 1);
  g_hash_table_insert (settings_.filehash, g_strdup(ident), item);

  /* new screen visual (desktop shortcut) */
  gint16 iconsize = desktop->iconsize;
  Docklet *docklet = desktop_shortcut_new (panel, item,
					   iconsize, iconsize,
					   xpos, ypos,
					   iconpath, name, font,
					   NULL, NULL, false);
  gtk_widget_show (docklet->window);

  return true;
} /* </desktop_shortcut_create> */

/*
* desktop_shortcut_remove
*/
void
desktop_shortcut_remove(ConfigurationNode *node)
{
  ConfigurationNode *sha1 = configuration_find (node, "sha1");
  vdebug(1, "%s sha1 => %s, node => 0x%lx\n", __func__, sha1->element, node);

  g_hash_table_remove (settings_.filehash, sha1->element);
  configuration_remove (node);  /* remove from configuration */
} /* </desktop_shortcut_remove> */

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
		basename((char *)settings_.iconpath), iconsize);

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
