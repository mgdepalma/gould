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
#include "docklet.h"

#define desktop_file_get_string(file, key) \
	g_key_file_get_string(file, G_KEY_FILE_DESKTOP_GROUP, key, NULL)

extern const char *Release, *Information;	/* see, gpanel.c */

static gchar *ActionHintDelete = "Press the apply button to delete.";
static gchar *ActionHintEdit   = "Click on the icon to change it.";

static gchar *DefaultIconExec = "exec.png";	/* default icon for programs */
// static gchar *DefaultIconURL = "earth.png";	/* default icon for URLs */

typedef struct {
  gchar *name;
  gchar *icon;
  gchar *exec;
} DesktopEntry;

const char *DesktopEntryComplete =
"<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
"<item name=\"%s\" icon=\"%s\">\n"
" <exec>%s</exec>\n"
" <xpos>%d</xpos>\n"
" <ypos>%d</ypos>\n"
"</item>\n";

const char *DesktopEntryMinimal =
"<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
"<item file=\"%s\">\n"
" <xpos>%d</xpos>\n"
" <ypos>%d</ypos>\n"
"</item>\n";

/*
* desktop_parse_file
*/
DesktopEntry *
desktop_file_parse (const char *filename)
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
}

/*
* (private) desktop_move - change the position of a desktop shortcut
*/
static void
desktop_move (ConfigurationNode *node, gint xpos, gint ypos)
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
static gboolean
desktop_filer_apply (GtkWidget *button, GlobalPanel *panel)
{
  PanelDesktop *desktop = panel->desktop;

  gtk_widget_hide (desktop->filer);
  gtk_button_set_image (GTK_BUTTON(desktop->iconview),
			gtk_image_new_from_pixbuf (desktop->render));

  if (desktop->chooser_apply)	/* if specified invoke additional callback */
    (*desktop->chooser_apply) (panel);

  return TRUE;
} /* </desktop_filer_apply> */

static gboolean
desktop_filer_cancel (GtkWidget *button, GlobalPanel *panel)
{
  PanelDesktop *desktop = panel->desktop;
  gtk_widget_hide (desktop->filer);
  return TRUE;
} /* </desktop_filer_cancel> */

/*
* (private) desktop_filer_refresh
* (private) desktop_filer_repaint
*/
gboolean
desktop_filer_refresh (GtkWidget *canvas,
                       GdkEventExpose *event,
                       GlobalPanel *panel)
{
  PanelDesktop *desktop = panel->desktop;
  redraw_pixbuf (canvas, desktop->render);
  return TRUE;
} /* </desktop_filer_refresh> */

static gboolean
desktop_filer_repaint (FileChooserDatum *datum)
{
  GlobalPanel *panel = datum->user;
  PanelDesktop *desktop = panel->desktop;
  const gchar *file = datum->file;
  struct stat info;

  if (lstat(file, &info) == 0 && S_ISREG(info.st_mode)) {
    gint iconsize = desktop->iconsize;

    desktop->render = pixbuf_new_from_file_scaled (file, iconsize, iconsize);
    desktop_filer_refresh (desktop->canvas, NULL, panel);
  }

  return TRUE;
} /* </desktop_filer_repaint> */

/*
* (private) desktop_setting_apply
* (private) desktop_setting_cancel
*/
static gboolean
desktop_setting_apply (GtkWidget *button, GlobalPanel *panel)
{
  PanelDesktop *desktop = panel->desktop;
  DesktopAction action = desktop->action;
  Docklet *docklet = desktop->shortcut;

  /* Dismiss the desktop->window interface. */
  gtk_widget_hide (desktop->window);
  desktop->active = FALSE;

  /* Handle the user specified action. */
  if (action == DESKTOP_SHORTCUT_DELETE) {
    configuration_remove (desktop->node);   /* remove from configuration */
    gtk_widget_destroy (docklet->window);   /* remove from screen display */
    g_object_ref_sink (docklet);	    /* clean-up memory */
    g_object_unref (docklet);
  }
  else {
    FileChooser *chooser = desktop->chooser;
    const gchar *iconame = gtk_entry_get_text (GTK_ENTRY(chooser->name));
    const gchar *icopath = filechooser_get_selected_name (chooser);

    const gchar *text = gtk_entry_get_text (GTK_ENTRY(desktop->name));
    const gchar *run  = gtk_entry_get_text (GTK_ENTRY(desktop->exec));

    ConfigurationNode *config = configuration_find (panel->config, "desktop");
    gchar *font = configuration_attrib (config, "font");

    if (icon_path_finder (panel->icons, iconame) == NULL)
      iconame = icopath;

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
      attrib->value = g_strdup (iconame);

      /* Deallocate and reconstruct exec->element */
      g_free (exec->element);
      exec->element = g_strdup (run);

      docklet_update (docklet, icopath, text);	/* update screen display */
    }
    else if (action == DESKTOP_SHORTCUT_CREATE) {
      ConfigurationNode *item;
      ConfigurationNode *mark;
      Docklet *docklet;

      gchar *stub = (gchar *)DesktopEntryComplete;
      gchar *spec;

      gint iconsize = desktop->iconsize;
      gint xpos;
      gint ypos;


      /* Calculate where to place and form the XML. */
      desktop_config (panel, FALSE);

      xpos = desktop->xpos;
      ypos = desktop->ypos + desktop->step;

      spec = g_strdup_printf (stub, text, iconame, run, xpos, ypos);
      item = configuration_read (spec, NULL, TRUE);
      mark = configuration_find (config, "/desktop");

      if (mark == NULL) {			/* <desktop ... /> case */
        if ( (mark = configuration_find (config, "/")) ) {
          g_free (mark->element);
          mark->element = g_strdup ("/desktop");
        }
        else {
          g_free (spec);
          return FALSE;
        }
      }

      configuration_insert (item, mark->back, mark->depth + 1);
      g_free (spec);

      docklet = desktop_create (panel, item,	/* new screen visual */
                                iconsize, iconsize,
                                xpos, ypos,
                                icopath, text, font,
                                NULL, NULL);

      gtk_widget_show (docklet->window);
    }
  }
  return TRUE;
} /* </desktop_setting_apply> */

static gboolean
desktop_setting_cancel (GtkWidget *button, GlobalPanel *panel)
{
  PanelDesktop *desktop = panel->desktop;

  gtk_widget_hide (desktop->window);
  desktop->active = FALSE;

  return TRUE;
} /* </desktop_setting_cancel> */

/*
* (private) desktop_setting_iconview
*/
static gboolean
desktop_setting_iconview (GtkWidget *button, GlobalPanel *panel)
{
  PanelDesktop *desktop = panel->desktop;
  gtk_widget_show (desktop->filer);
  return TRUE;
} /* </desktop_setting_iconview> */

/*
* desktop_new - object for creating and editing desktop shortcuts
*/
PanelDesktop *
desktop_new (GlobalPanel *panel, gint iconsize)
{
  GtkWidget *window, *frame, *layout, *layer;
  GtkWidget *button, *entry, *label, *split;

  ConfigurationNode *chain = configuration_find(panel->config, "desktop");
  ConfigurationNode *menu  = configuration_find(chain, "menu");

  PanelDesktop *desktop = g_new0 (PanelDesktop, 1);
  PanelIcons *icons = panel->icons;

  gchar *value = configuration_attrib(menu, "iconsize");
  gint menuiconsize = (value) ? atoi(value) : panel->taskbar->iconsize;

  FileChooser *chooser;
  gchar dirname[FILENAME_MAX];
  gchar *scan;

  desktop->active = FALSE;
  desktop->iconsize = iconsize;
  desktop->folder = g_strdup_printf ("%s/Desktop", getenv("HOME"));
  desktop->menu = menu_options_config (menu, panel, menuiconsize);

  /* Create a sticky window with a frame. */
  window = desktop->window = sticky_window_new (GDK_WINDOW_TYPE_HINT_NORMAL,
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

  button = xpm_button(ICON_APPLY, _("Apply"));
  gtk_box_pack_end (GTK_BOX(layer), button, FALSE, FALSE, 0);
  gtk_button_set_relief (GTK_BUTTON(button), GTK_RELIEF_NONE);

  g_signal_connect (G_OBJECT(button), "clicked",
                     G_CALLBACK(desktop_setting_apply), panel);
  gtk_widget_show (button);

  button = xpm_button(ICON_CANCEL, _("Cancel"));
  gtk_box_pack_end (GTK_BOX(layer), button, FALSE, FALSE, 0);
  gtk_button_set_relief (GTK_BUTTON(button), GTK_RELIEF_NONE);

  g_signal_connect (G_OBJECT(button), "clicked",
                     G_CALLBACK(desktop_setting_cancel), panel);
  gtk_widget_show (button);

  /* Save the initial default image path. */
  desktop->icon = g_strdup (icon_path_finder (icons, DefaultIconExec));
  gtk_entry_set_text (GTK_ENTRY(desktop->name), DefaultIconExec);

  /* Create popup window for choosing a different icon. */
  window = desktop->filer = gtk_window_new (GTK_WINDOW_POPUP);
  gtk_widget_set_uposition (GTK_WIDGET(window), 120, 120);

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

  chooser = desktop->chooser = filechooser_new (dirname);
  filechooser_set_callback (chooser, (gpointer)desktop_filer_repaint, panel);
  gtk_entry_set_text (GTK_ENTRY(chooser->name), DefaultIconExec);
  chooser->clearname  = TRUE;	/* clear file name on directory change */

  layer = filechooser_layout (chooser);
  gtk_widget_set_size_request (layer, 380, 4*iconsize);
  gtk_box_pack_start (GTK_BOX(layout), layer, TRUE, TRUE, 5);
  /* gtk_widget_hide (chooser->namebox); */
  gtk_widget_show (layer);

  layer = desktop->canvas = gtk_drawing_area_new ();
  gtk_widget_set_size_request (layer, iconsize, iconsize);
  gtk_box_pack_start (GTK_BOX(layout), layer, TRUE, TRUE, 5);
  gtk_widget_show (layer);

  g_signal_connect (G_OBJECT (layer), "expose_event",
                    G_CALLBACK (desktop_filer_refresh), panel);

  /* Add the APPLY and CANCEL buttons. */
  layout = gtk_hbox_new (FALSE, 1);
  gtk_box_pack_start (GTK_BOX(split), layout, FALSE, FALSE, 0);
  gtk_widget_show (layout);

  button = xpm_button(ICON_APPLY, _("Apply"));
  gtk_box_pack_end (GTK_BOX(layout), button, FALSE, FALSE, 0);
  gtk_button_set_relief (GTK_BUTTON(button), GTK_RELIEF_NONE);

  g_signal_connect (G_OBJECT(button), "clicked",
                     G_CALLBACK(desktop_filer_apply), panel);
  gtk_widget_show (button);

  button = xpm_button(ICON_CANCEL, _("Cancel"));
  gtk_box_pack_end (GTK_BOX(layout), button, FALSE, FALSE, 0);
  gtk_button_set_relief (GTK_BUTTON(button), GTK_RELIEF_NONE);
  gtk_widget_show (button);

  g_signal_connect (G_OBJECT(button), "clicked",
                     G_CALLBACK(desktop_filer_cancel), panel);

  return desktop;
} /* desktop_new */

/*
* (private) desktop_shortcut - present menu of desktop shortcut
*/
void
desktop_shortcut (Docklet *docklet, ConfigurationNode *node)
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
  gint iconsize = desktop->iconsize;

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
static gboolean
desktop_shortcut_callback (DockletDatum *datum)
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
  return FALSE;
} /* </desktop_shortcut_callback> */

/*
* (private) desktop_settings - change the settings of desktop shortcut
*/
void
desktop_settings (GlobalPanel *panel, DesktopAction act)
{
  PanelDesktop *desktop = panel->desktop;
  GtkWidget *iconview = desktop->iconview;

  if (desktop == NULL) {
    notice_at(100, 100, ICON_ERROR, "desktop_settings: %s.",
				_("assert desktop failed!"));
    return;
  }
  
  if (desktop->active)	/* desktop settings already active */
    return;

  if (act == DESKTOP_SHORTCUT_CREATE) {
    const gchar *file = desktop->icon;
    gint iconsize = desktop->iconsize;

    desktop->render = pixbuf_new_from_file_scaled (file, iconsize, iconsize);
    gtk_button_set_image (GTK_BUTTON(iconview),
                          gtk_image_new_from_pixbuf (desktop->render));

    gtk_entry_set_text (GTK_ENTRY(desktop->name), "");
    gtk_entry_set_text (GTK_ENTRY(desktop->exec), "");
    gtk_entry_set_text (GTK_ENTRY(desktop->chooser->name), DefaultIconExec);

    desktop->shortcut = NULL;	/* screen representation */
  }
  else if (act == DESKTOP_SHORTCUT_DELETE) {
    gtk_button_set_relief (GTK_BUTTON(iconview), GTK_RELIEF_NONE);
    gtk_widget_set_sensitive (GTK_WIDGET(iconview), FALSE);

    gtk_entry_set_editable (GTK_ENTRY(desktop->name), FALSE);
    gtk_entry_set_editable (GTK_ENTRY(desktop->exec), FALSE);

    gtk_label_set_text (GTK_LABEL(desktop->hint), _(ActionHintDelete));
  }

  if (act == DESKTOP_SHORTCUT_CREATE || act == DESKTOP_SHORTCUT_EDIT) {
    gtk_button_set_relief (GTK_BUTTON(iconview), GTK_RELIEF_NORMAL);
    gtk_widget_set_sensitive (GTK_WIDGET(iconview), TRUE);

    gtk_entry_set_editable (GTK_ENTRY(desktop->name), TRUE);
    gtk_entry_set_editable (GTK_ENTRY(desktop->exec), TRUE);

    gtk_label_set_text (GTK_LABEL(desktop->hint), _(ActionHintEdit));
  }

  if (act == DESKTOP_SHORTCUT_OPEN) {
    ConfigurationNode *exec = configuration_find (desktop->node, "exec");
    spawn_selected (exec, panel);
  }
  else {
    gtk_widget_hide (panel->settings->window);  /* hide settings window */
    gtk_widget_show (desktop->window); 		/* show manager window */
    desktop->active = TRUE;
  }

  desktop->action = act; /* action applied on shortcut */
} /* </desktop_settings> */

/*
* desktop_change - GFileMonitor callback for $HOME/Desktop changes
*/
static inline ConfigurationNode *
desktop_change_node (ConfigurationNode *config, const char *match)
{
  ConfigurationNode *chain = configuration_find(config, "desktop");
  ConfigurationNode *mark  = configuration_find (chain, "/desktop");
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
desktop_change (GFileMonitor *monitor, GFile *file, GFile *other,
		GFileMonitorEvent event_type, GlobalPanel *panel)
{
  char *filename = g_file_get_path (file);
  const char *name = basename(filename);

  ConfigurationNode *chain = configuration_find(panel->config, "desktop");
  ConfigurationNode *node  = desktop_change_node(panel->config, name);

  DesktopEntry *entry = desktop_file_parse (filename);  /* may be NULL */

  switch (event_type) {
    case G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT:
      vdebug(1, "Name => %s, Icon => %s, Exec => %s\n",
		entry->name, entry->icon, entry->exec);
      break;

    case G_FILE_MONITOR_EVENT_CREATED:
vdebug(1, "desktop_change %s created\n", filename);
configuration_write (chain, "<%s>\n", stdout);
      vdebug(1, "Name => %s, Icon => %s, Exec => %s\n",
		entry->name, entry->icon, entry->exec);
      break;

    case G_FILE_MONITOR_EVENT_DELETED:
vdebug(1, "desktop_change %s deleted\n", filename);
configuration_write (chain, "<%s>\n", stdout);
      vdebug(1, ">>>>>>> %s will be deleted from $CONFIG/panel\n", name);
      configuration_remove(node);

      panel->desktop->shortcut = node->widget;
      panel->desktop->action = DESKTOP_SHORTCUT_DELETE;
      desktop_setting_apply (NULL, panel);

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
desktop_config (GlobalPanel *panel, gboolean once)
{
  const gchar *icon;

  ConfigurationNode *node = configuration_find (panel->config, "desktop");
  ConfigurationNode *mark = configuration_find (node, "/desktop");

  PanelIcons *icons = panel->icons;
  guint depth = node->depth + 1;

  /* gint width  = gdk_screen_width(); */
  gint height = gdk_screen_height();

  gint xpos = -1;	/* used to calculate position */
  gint ypos = -1;

  gint step = 70;	/* should be computed from font and icon */
  gint xorg = 5;	/* position for a new shortcut */
  gint yorg = 5;

  gchar *font, *name;
  gchar *init  = configuration_attrib(node, "init");
  gchar *value = configuration_attrib(node, "iconsize");

  /* Prefer the iconsize for <desktop> */
  guint iconsize = (value) ? atoi(value) : icons->size;

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

      /* See if the Docklet has its own font, otherwise use the default. */
      if((value = configuration_attrib(node, "font")) == NULL) value = font;

      /* A desktop item is a Docklet instance, create once. */
      if (once) {
        Docklet *docklet = desktop_create (panel, node,
                                           iconsize, iconsize,
                                           xpos, ypos,
                                           icon, name, value,
                                           NULL, NULL);
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
      session_request (panel->session, init);
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
} /* </desktop_config> */

/*
* desktop_create - instantiate a new desktop launcher 
*/
Docklet *
desktop_create (GlobalPanel *panel,
                ConfigurationNode *node,
                gint width, gint height,
                gint xpos, gint ypos,
                const gchar *icon,
                const gchar *text,
                const gchar *font,
                GdkColor *fg,
                GdkColor *bg)
{
  Docklet *docklet = docklet_new (GDK_WINDOW_TYPE_HINT_NORMAL,
                                  width, height, xpos, ypos,
                                  GTK_ORIENTATION_VERTICAL,
                                  GTK_VISIBILITY_NONE,
                                  icon, text, font,
                                  fg, bg);
  node->widget = docklet;
  node->data   = panel;	/* set node->data madatory */

  docklet_set_callback (docklet, (gpointer)desktop_shortcut_callback, node);
  gtk_window_set_keep_below (GTK_WINDOW(docklet->window), TRUE);

  return docklet;
} /* </desktop_create> */
