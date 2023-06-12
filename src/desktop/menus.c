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
#include "tasklist.h"
#include "pager.h"
#include "xutil.h"

#include <X11/Xatom.h>

extern const char *Program;				/* see, gpanel.c */
extern const char *Release;				/* ... */
extern const char *Description;				/* ... */
extern const char *ConfigurationHeader, *Schema;        /* ... */

/* Needed forward function declarations. */
static void exit_cancel (GtkWidget *button, GlobalPanel *panel);

#ifdef NEVER
/*
* (private) shortcut_cb
*/
static void
shortcut_cb (GtkWidget *widget, gpointer drag_context, gint x, gint y,
              time_t stamp, gpointer data)
{
  printf("[shortcut_cb]hit!\n");
} /* </shortcut_cb> */
#endif

/*
 * (private) about
 */
static void
about (GlobalPanel *panel)
{
  gpanel_dialog(100, 100, ICON_SNAPSHOT,
               "\n%s %s %s", Program, Release, Description);
} /* </about> */

/*
* save configuration file, when needed
*/
int
saveconfig (GlobalPanel *panel)
{
  gchar *resource  = panel->resource;
  gchar *oldconfig = g_strdup_printf ("%s~", resource);
  gchar *newconfig = g_strdup_printf ("%s#", resource);

  struct stat info;
  int status = 0;

  /* Save configuration changes. */
  if (lstat(resource, &info) == 0) {
    int bytes = info.st_size;	       /* original configuration byte count */
    FILE *stream = fopen(newconfig, "w");

    if (stream) {
      configuration_write (panel->config, ConfigurationHeader, stream);
      fclose(stream);

      if (lstat(newconfig, &info) == 0 && info.st_size != 0) {
        if (configuration_read (newconfig, Schema, FALSE) != NULL) {
          gboolean delta = (bytes != info.st_size) ? TRUE : FALSE;

          if (bytes == info.st_size) { /* compare old and new configurations */
            char *oldbytes = malloc(bytes);
            char *newbytes = malloc(bytes);

            if (oldbytes != NULL && newbytes != NULL) {
              if ( (stream = fopen(resource, "r")) ) {
                fread(oldbytes, bytes, 1, stream);
                fclose(stream);

                if ( (stream = fopen(newconfig, "r")) ) {
                  fread(newbytes, bytes, 1, stream);
                  fclose(stream);

                  if (memcmp(oldbytes, newbytes, bytes) != 0)
                    delta = TRUE;
                }
              }
            }

            MFREE(newbytes)
            MFREE(oldbytes)
          }

          if (delta) {
            if (bytes == info.st_size)
              vdebug (1, "[%s]configuration changes detected!\n", __func__);
            else
              vdebug (1, "[%s]old config %d bytes, new config %d bytes\n",
                                __func__, bytes, (int)info.st_size);

            if (rename(resource, oldconfig) == 0)
              rename(newconfig, resource);
          }
          else {
            vdebug (1, "[%s]no configuration changes detected!\n", __func__);
            remove(newconfig);
          }
        }
        else {
          configuration_write (panel->config, ConfigurationHeader, stderr);
          fprintf(stderr, "[%s]new configuration is corrupted.\n", __func__);
          status = 1;
        }
      } /* </lstat newconfig> */
      else {
        fprintf(stderr, "[%s]error saving new configuration.\n", __func__);
        status = 1;
      }
    }
  } /* </lstat oldconfig> */

  g_free (oldconfig);
  g_free (newconfig);

  return status;
} /* </saveconfig> */

/*
 * (private) finis
 */
static void
finis (GlobalPanel *panel, gboolean logout, gboolean quit)
{
  GList   *iter;		/* plugin modules iterator */
  Modulus *applet;		/* plugin module instance */
  int      status = Success;

  /* Invoke module_close() on every module. */
  for (iter = panel->moduli; iter != NULL; iter = iter->next) {
    applet = (Modulus *)iter->data;

    if (applet->module_close)
      applet->module_close (applet);
  }
  saveconfig (panel);		/* check for changes and save configuration */

  if (logout) {	
    if (panel->session >= 0) {	/* send SIGTERM to gsession */
      killall(gsessionProcess, SIGTERM);
    }
    else {			/* signal {WINDOWMANAGER} */
      Display *display = panel->shared->display;
      Window window = DefaultRootWindow (display);
      unsigned char *data = get_xprop_name (window, "_OPENBOX_PID");

      /* if not openbox, try blackbox/fluxbox */
      if(data == NULL) data = get_xprop_name (window, "_BLACKBOX_PID");

      if (data != NULL) {
        pid_t pid = *(unsigned long *)data;

        if (kill(pid, SIGTERM) == 0) {
          sleep(2);
          if(kill(pid, 0) == 0) kill(pid, SIGKILL); /* signal windowmanager */
        }
      }
      else {
        status = BadAtom;
        gpanel_dialog(100, 100, ICON_ERROR, "[%s]%s.",
                  Program, _("Sorry, cannot get window manager process ID"));
      }
    }
  }

  if (quit && status == Success) {
    gtk_main_quit ();		/* all done.. */
  }
} /* </finis> */

/*
* end session callback
*/
static void
end_session (GtkWidget *button, GlobalPanel *panel)
{
  gtk_widget_hide (panel->backdrop);
  gtk_widget_hide (panel->logout->window);
  gdk_display_sync (gtk_widget_get_display (panel->window));

  finis (panel, TRUE, TRUE);
} /* </end_session> */

/*
* shutdown halt callback
*/
static void
end_session_halt (GtkWidget *button, GlobalPanel *panel)
{
  finis (panel, FALSE, FALSE);

  vdebug (1, "sudo /sbin/shutdown -h now\n");
  system("sudo /sbin/shutdown -h now >/dev/null 2>&1");

  gtk_main_quit ();	/* all done */
} /* </end_session_halt> */

/*
* shutdown reboot callback
*/
static void
end_session_reboot (GtkWidget *button, GlobalPanel *panel)
{
  finis (panel, FALSE, FALSE);

  vdebug (1, "sudo /sbin/shutdown -r now\n");
  system("sudo /sbin/shutdown -r now >/dev/null 2>&1");

  gtk_main_quit ();	  /* all done */
} /* </end_session_reboot> */

/*
* exit cancel callback
*/
static void
exit_cancel (GtkWidget *button, GlobalPanel *panel)
{
  gtk_widget_hide (panel->backdrop);
  gtk_widget_hide (panel->logout->window);
  if (panel->visible) gtk_widget_show (panel->window);
} /* </exit_cancel> */

/*
* suspend callback
*/
static void
exit_suspend (GtkWidget *button, GlobalPanel *panel)
{
  exit_cancel(button, panel);
  vdebug (1, "sudo /sbin/suspend\n");
  system("sudo /sbin/suspend >/dev/null 2>&1");
} /* </exit_suspend> */

/*
* screenlock
*/
static void
screenlock (GtkWidget *button, GlobalPanel *panel)
{
  char command[128];
  const char *mode = saver_getmode();

  if (mode)
    sprintf(command, "%s -mode %s", _XLOCK_COMMAND, mode);
  else
    strcpy(command, _XLOCK_COMMAND);

  vdebug (1, "[%s]%s\n", __func__, command);
  exit_cancel(button, panel);

  system (command);
} /* </screenlock> */

/*
* (protected) startmenu - position menu consistently
*/
void
startmenu (GtkMenu *menu, gint *x, gint *y, gboolean *pushin, gpointer data)
{
  GlobalPanel *panel = data;
  GtkRequisition requisition;

  gint width  = gdk_screen_width ();
  gint height = gdk_screen_height ();
  gint offset = panel->thickness + 1;

  gint xsize;		/* width of the menu */
  gint ysize;		/* height of the menu */

  gtk_widget_size_request (GTK_WIDGET(menu), &requisition);
  xsize = requisition.width;
  ysize = requisition.height;

  switch (panel->place) {
    case GTK_POS_TOP:
    case GTK_POS_BOTTOM:
      if (GTK_WIDGET(menu) == panel->taskbar->options)
        *x = panel->margin;

      *y = (panel->place == GTK_POS_TOP) ? offset : height - ysize - offset;
      break;

    case GTK_POS_LEFT:
    case GTK_POS_RIGHT:
      *x = (panel->place == GTK_POS_LEFT) ? offset : width - xsize - offset;

      if (GTK_WIDGET(menu) == panel->taskbar->options)
        *y = panel->margin;
      break;
  }
} /* </startmenu> */

/*
* (private) activate - popup the main menu
*/
static void
activate (GtkWidget *button, GdkEventButton *event, GlobalPanel *panel)
{
  PanelTaskbar *menu = panel->taskbar;

  if (event->button == 1)
    gtk_menu_popup (GTK_MENU (menu->options), NULL, NULL, startmenu, panel,
                      event->button, event->time);
} /* </activate> */

/*
* item_check_access
*/
const char * 
item_check_access (ConfigurationNode *node, GlobalPanel *panel, gchar *attr)
{
  static char file[FILENAME_MAX];
  static char args[FILENAME_MAX];

  char *value  = (attr) ? configuration_attrib (node, attr) : node->element;
  char *result = NULL;
  
  if (value != NULL) {		/* check access based on given attribute */
    gchar *scan;

    strcpy(file, value);	/* scan for the first space character */
    strcpy(args, "");

    if ((scan = strchr(file, ' '))) {
      strcpy(args, (scan+1));
      *scan = (char)0;
    }

    if (file[0] == '/') {	/* absolute path specified */
      if (access(file, X_OK) == 0) {
        strcpy(file, value);
        result = file;
      }
    }
    else {
      const gchar *pathname = path_finder (panel->path, file);

      if (pathname != NULL) {
        sprintf(file, "%s %s", pathname, args);
        result = file;
      }
    }
  }
  else {			/* pass, attribute was not specified */
    result = node->element;
  }
  return result;
} /* </item_check_access> */

/**
* (protected) spawn_selected forks child process for selected application
*/
pid_t
spawn_selected (ConfigurationNode *node, GlobalPanel *panel)
{
  pid_t pid = -1;
  const char *cmdline = item_check_access (node, panel, NULL);

  if (cmdline != NULL) {
    vdebug(2, "%s: cmdline => %s\n", __func__, cmdline);
    pid = dispatch (panel->session, cmdline);
  }
  else
    gpanel_dialog(100, 100, ICON_WARNING, "[%s]%s: %s.",
               Program, node->element, _("command not found"));
  return pid;
} /* </spawn_selected> */

/*
* (protected) executer
*/
void
executer (GtkWidget *widget, ConfigurationNode *node)
{
  GlobalPanel *panel = (GlobalPanel *)node->data;
  ConfigurationNode *exec = configuration_find (node, "exec");

  const gchar *type = configuration_attrib (exec->back, "type");
  const gchar *item = exec->element;

  vdebug (2, "[%s]%s.exec: %s (type=%s)\n", __func__,
          configuration_attrib (node, "name"), item,
          (type != NULL) ? type : "application");

  if (type != NULL && strcmp(type, "method") == 0) {
    if (strcmp(item, "about") == 0)
      about (panel);
    else if (strcmp(item, "exit") == 0)
      shutdown_panel (panel, SIGTERM);
    else if (strcmp(item, "logout") == 0)
      finis (panel, TRUE, TRUE);
    else if (strcmp(item, "quit") == 0)
      finis (panel, FALSE, TRUE);
    else if (strcmp(item, "restart") == 0)
      restart (panel);
    else if (strcmp(item, "settings") == 0)
      settings_activate (panel);
    else if (strcmp(item, "shortcut:create") == 0)
      desktop_settings (panel, DESKTOP_SHORTCUT_CREATE);
    else if (strcmp(item, "shortcut:delete") == 0)
      desktop_settings (panel, DESKTOP_SHORTCUT_DELETE);
    else if (strcmp(item, "shortcut:edit") == 0)
      desktop_settings (panel, DESKTOP_SHORTCUT_EDIT);
    else if (strcmp(item, "shortcut:open") == 0)
      desktop_settings (panel, DESKTOP_SHORTCUT_OPEN);
    else
      gpanel_dialog(100, 100, ICON_ERROR, "[%s]%s: %s.",
                 Program, item, _("unimplemented method"));
  }
  else {
    spawn_selected (exec, panel);
  }
} /* </executer> */

/*
* shutdown_panel - display shutdown dialog
*/
void
shutdown_panel (GlobalPanel *panel, int sig)
{
  PanelLogout *logout = panel->logout;
  gboolean allowed = sudoallowed("/sbin/shutdown");

  gtk_widget_set_sensitive (logout->shutdown, allowed);
  gtk_widget_set_sensitive (logout->reboot, allowed);

  if (sig == SIGCONT) {
    gtk_widget_hide (panel->backdrop);
    gtk_widget_hide (logout->window);
    if (panel->visible) gtk_widget_show (panel->window);
  }
  else {
    gtk_widget_show (panel->backdrop);
    if(sig == SIGTERM) gtk_widget_show (logout->window);
    gtk_widget_hide (panel->settings->window);
    gtk_widget_hide (panel->window);
  }
} /* </shutdown_panel> */

/*
* (protected) reconstruct
* (protected) restart
*/
void
reconstruct (GlobalPanel *panel)
{
  GList    *iter;             /* plugin modules iterator */
  Modulus  *applet;           /* plugin module instance */
  Modulus  *splash = NULL;    /* splash plugin module */
  gboolean enable;            /* splash plugin enable */

  /* See if the splash screen plugin is available. */
  for (iter = panel->moduli; iter != NULL; iter = iter->next) {
    applet = (Modulus *)iter->data;

    if (strcmp(applet->name, "splash") == 0) {
      splash = applet;
      enable = splash->enable;	/* save splash plugin enable */
      splash->enable = FALSE;
      break;
    }
  }

  restart (panel);		/* reconstruct panel interface */

  if (splash != NULL)		/* restore splash plugin enable */
    splash->enable = enable;
} /* </reconstruct> */

void
restart (GlobalPanel *panel)
{
  GList   *iter;		/* modules list iterator */
  Modulus *applet;		/* module instance */

  /* Invoke module_close() on every plugin module. */
  for (iter = panel->moduli; iter != NULL; iter = iter->next) {
    applet = (Modulus *)iter->data;

    if (applet->module && applet->module_close)
      applet->module_close (applet);
  }

  panel->taskbar->button = NULL;	/* must invalidate main menu */
  systray_disconnect (panel->systray);	/* disconnect from system tray */

  green_remove_window (panel->green, GDK_WINDOW_XID (panel->window->window));
  gtk_widget_destroy (panel->window);	/* destroy panel main window */

  panel_config_orientation (panel);	/* orientation may have changed */
  panel_loader (panel);
} /* </restart> */

/*
* shutdown_dialog_new - create shutdown dialog window
*/
GtkWidget *
shutdown_dialog_new (GlobalPanel *panel)
{
  FILE *stream;
  gchar *caption;
  gchar text[FILENAME_MAX];
  gboolean resume = FALSE;

  GtkWidget *dialog, *frame, *layout, *layer, *label, *image, *button;
  const gchar *icon = icon_path_finder (panel->icons, "shutdown.png");
  PanelLogout *logout = panel->logout;
  const guint iconsize = 48;

  gint width  = gdk_screen_width();
  gint height = gdk_screen_height();

  gint xsize = -1; /* (width < 1280) ? 460 : 640; */
  gint ysize = 200;

  /* Create the shutdown dialog window. */
  dialog = sticky_window_new (GDK_WINDOW_TYPE_HINT_NORMAL, xsize, ysize, 0, 0);
  gtk_window_set_keep_above (GTK_WINDOW(dialog), TRUE);
  gtk_window_set_modal (GTK_WINDOW(dialog), TRUE);

  /* Make sure it is on top of panel->backdrop */
  gtk_window_set_transient_for (GTK_WINDOW(dialog),
                                GTK_WINDOW(panel->backdrop));

  frame = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME(frame), GTK_SHADOW_OUT);
  gtk_container_add (GTK_CONTAINER(dialog), frame);
  gtk_widget_show (frame);

  /* Create the inside dialog layout. */
  layout = gtk_vbox_new (FALSE, 1);
  gtk_container_add (GTK_CONTAINER(frame), layout);
  gtk_widget_show (layout);

  /* Caption on the top of the dialog window. */
  layer = gtk_hbox_new (FALSE, 1);
  gtk_box_pack_start (GTK_BOX(layout), layer, FALSE, FALSE, 2);
  gtk_widget_show (layer);

  if ( (stream = popen("hostname --long", "r")) ) {
    fgets(text, FILENAME_MAX, stream);
    caption = g_strdup_printf (
      "\t<span foreground=\"#6699CC\" font_desc=\"San Bold 14\">%s</span>",
                                text);

    label = gtk_label_new (caption);
    gtk_misc_set_alignment(GTK_MISC(label), 0.5, 0.5);
    gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
    gtk_box_pack_start (GTK_BOX(layer), label, FALSE, FALSE, 2);
    gtk_widget_show (label);

    g_free (caption);
    pclose(stream);
  }

  /* Right justify `shutdown icon`(fallback: ICON_ECLIPSE) */
  if (icon != NULL)
    image = image_new_from_file_scaled (icon, iconsize, iconsize);
  else
    image = xpm_image_scaled (ICON_ECLIPSE, iconsize, iconsize);

  gtk_box_pack_end (GTK_BOX(layer), image, FALSE, FALSE, 2);
  gtk_widget_show (image);

  /* Instructions for the user. */
  caption = g_strdup_printf (
      "\t<span foreground=\"#FFFFFF\" font_desc=\"San Bold 12\">%s</span>",
                             _("What do you want the computer to do?"));

  layer = gtk_hbox_new (FALSE, 1);
  gtk_box_pack_start (GTK_BOX(layout), layer, FALSE, FALSE, 2);
  gtk_widget_show (layer);

  label = gtk_label_new (caption);
  //gtk_misc_set_alignment(GTK_MISC(label), 0.5, 0.5);
  gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
  gtk_box_pack_start (GTK_BOX(layer), label, FALSE, FALSE, 2);
  gtk_widget_show (label);
  g_free (caption);

  /* Selections: {End Session, Suspend, Shutdown, Reboot} */
  layer = gtk_hbox_new (FALSE, 1);
  gtk_box_pack_start (GTK_BOX(layout), layer, FALSE, FALSE, 2);
  gtk_widget_show (layer);

  label = gtk_label_new ("\t");
  gtk_box_pack_start (GTK_BOX(layer), label, FALSE, FALSE, 2);
  gtk_widget_show (label);

  button = xpm_button(ICON_LOGOUT, _("Logout"));
  g_signal_connect (G_OBJECT(button), "clicked",
                    G_CALLBACK(end_session), panel);

  gtk_box_pack_start (GTK_BOX(layer), button, FALSE, FALSE, 2);
  gtk_button_set_relief (GTK_BUTTON(button), GTK_RELIEF_NONE);
  gtk_widget_show (button);

  /* Suspend */
  button = logout->suspend = xpm_button(ICON_SUSPEND, _("Suspend"));

  gtk_box_pack_start (GTK_BOX(layer), button, FALSE, FALSE, 2);
  gtk_button_set_relief (GTK_BUTTON(button), GTK_RELIEF_NONE);
  gtk_widget_show (button);

  if ( (stream = fopen("/proc/cmdline", "r")) ) {
    char line[MAX_PATHNAME];
    fgets(line, MAX_PATHNAME, stream);
    resume = strstr(line, "resume=") != NULL;
    if (resume == FALSE) resume = strstr(line, "restore=") != NULL;
    fclose(stream);
  }

  if (resume && sudoallowed("/sbin/suspend"))
    g_signal_connect (G_OBJECT(button), "clicked",
                      G_CALLBACK(exit_suspend), panel);
  else
    gtk_widget_set_sensitive(button, FALSE);

  /* Shutdown */
  button = logout->shutdown = xpm_button(ICON_HALT, _("Shutdown"));
  g_signal_connect (G_OBJECT(button), "clicked",
                    G_CALLBACK(end_session_halt), panel);

  gtk_box_pack_start (GTK_BOX(layer), button, FALSE, FALSE, 2);
  gtk_button_set_relief (GTK_BUTTON(button), GTK_RELIEF_NONE);
  gtk_widget_show (button);

  button = logout->reboot = xpm_button(ICON_REBOOT, _("Restart"));
  g_signal_connect (G_OBJECT(button), "clicked",
                    G_CALLBACK(end_session_reboot), panel);

  gtk_box_pack_start (GTK_BOX(layer), button, FALSE, FALSE, 2);
  gtk_button_set_relief (GTK_BUTTON(button), GTK_RELIEF_NONE);
  gtk_widget_show (button);

  label = gtk_label_new ("\t");
  gtk_box_pack_end (GTK_BOX(layer), label, FALSE, FALSE, 2);
  gtk_widget_show (label);

  /* Lock Screen */
  button = logout->shutdown = xpm_button(ICON_LOCK, _("Lock Screen"));
  g_signal_connect (G_OBJECT(button), "clicked",
                    G_CALLBACK(screenlock), panel);

  gtk_box_pack_start (GTK_BOX(layer), button, FALSE, FALSE, 2);
  gtk_button_set_relief (GTK_BUTTON(button), GTK_RELIEF_NONE);
  gtk_widget_show (button);


  /* CANCEL button: bottom horizontal layer. */
  layer = gtk_hbox_new (FALSE, 1);
  gtk_box_pack_start (GTK_BOX(layout), layer, FALSE, FALSE, 2);
  gtk_widget_show (layer);

  label = gtk_label_new ("\t");
  gtk_box_pack_end (GTK_BOX(layer), label, FALSE, FALSE, 2);
  gtk_widget_show (label);

  button = xpm_button(ICON_CANCEL, _("Cancel"));
  gtk_box_pack_end (GTK_BOX(layer), button, FALSE, FALSE, 2);
  gtk_widget_show (button);

  g_signal_connect (G_OBJECT(button), "clicked",
                    G_CALLBACK(exit_cancel), panel);

  /* Center the shutdown dialog window. */
  gtk_window_get_size (GTK_WINDOW(dialog), &xsize, &ysize);
  gtk_window_move (GTK_WINDOW(dialog), (width-xsize) / 2, (height-ysize) / 2);

  return dialog;
} /* shutdown_dialog_new */

/*
* menu_header_config
*/
GtkWidget *
menu_header_config (ConfigurationNode *config, GlobalPanel *panel)
{
  GtkWidget *item = NULL;
  ConfigurationNode *node = configuration_find (config, "header");

  if (node != NULL) {
    PanelIcons *icons = panel->icons;

    const gchar *font = configuration_attrib (node, "font");
    const gchar *name = configuration_attrib (node, "name");
    const gchar *icon = configuration_attrib (node, "icon");
    const gchar *size = configuration_attrib (node, "iconsize");

    guint iconsize = (size) ? atoi(size) : 24;   /* default 24x24 icon size */

    /* defaul to font = if not specified */
    if (font == NULL)
      font = "Sans bold 14";

#ifdef NEVER /* may need a NEW menu class to have custom font menuitem labels */
    PangoFontDescription *fontdesc;
    PangoLayout *layout;

    item = gtk_image_menu_item_new ();

    /* setup font for text */
    fontdesc = pango_font_description_from_string (font);
    layout = gtk_widget_create_pango_layout (item, "alien");
    pango_layout_set_font_description (layout, fontdesc);
    pango_font_description_free (fontdesc);
#endif

    /* "%u" => user name, %r => real name */
    if (strcmp(name, "%u") == 0 || strcmp(name, "%r") == 0)
      item = gtk_image_menu_item_new_with_label (
                     get_username (strcmp(name, "%r") == 0)
                                                );
    else
      item = gtk_image_menu_item_new_with_label (_(name));

    /* Built-in default for missing icon attribute. */
    if (icon == NULL)
      icon = (getuid() == 0) ? "administrator.png" : "user.png";

    if (icon != NULL)
      if ((name = icon_path_finder (icons, icon)) != NULL) {
        GtkWidget *image = image_new_from_file_scaled (name, iconsize,iconsize);
        gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
      }
  }

  return item;
} /* </menu_header_config> */

/*
* menu_item_config
*/
GtkWidget *
menu_item_config (ConfigurationNode *config, GlobalPanel *panel, gint iconsize)
{
  GtkWidget *image, *item;
  PanelIcons *icons = panel->icons;

  const gchar *name = configuration_attrib (config, "name");
  const gchar *icon = configuration_attrib (config, "icon");

  if (name == NULL)	/* shouldn't happen.. */
    return NULL;

  if (item_check_access (config, panel, "check") == NULL)
    return NULL;

  item = gtk_image_menu_item_new_with_label (_(name));
  g_signal_connect (item, "activate", G_CALLBACK (executer), config);
  config->data = panel;	/* pass global program attributes */

  if (icon != NULL)
    if ((name = icon_path_finder (icons, icon)) != NULL) {
      image = image_new_from_file_scaled (name, iconsize, iconsize);
      gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
    }

  return item;
} /* </menu_item_config> */

/*
* menu_element_config
*
* parse a menu element which can be:
*   <item>, <separator>, or <submenu>
*/
GtkWidget *
menu_element_config (ConfigurationNode *node, GlobalPanel *panel, gint iconsize)
{
  GtkWidget *image, *submenu;
  GtkWidget *item = NULL;

  PanelIcons *icons = panel->icons;

  const gchar *name;
  const gchar *icon;

  if (item_check_access (node, panel, "check") == NULL)
    return NULL;

  if (strcmp(node->element, "item") == 0) {
    item = menu_item_config (node, panel, iconsize);
  }
  else if (strcmp(node->element, "separator") == 0) {
    item = gtk_separator_menu_item_new();
  }
  else if (strcmp(node->element, "submenu") == 0) {
    name = configuration_attrib (node, "name");
    item = gtk_image_menu_item_new_with_label (_(name));

    if ((icon = configuration_attrib (node, "icon")) != NULL)
      if ((name = icon_path_finder (icons, icon)) != NULL) {
        image = image_new_from_file_scaled (name, iconsize, iconsize);
        gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
      }

    submenu = menu_submenu_config (node, panel, iconsize);
    gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), submenu);
  }
  return item;
} /* </menu_element_config> */

/*
* menu_submenu_config
*/
GtkWidget *
menu_submenu_config (ConfigurationNode *chain, GlobalPanel *panel, gint iconsize)
{
  ConfigurationNode *node;
  GtkWidget *item, *options = gtk_menu_new ();
  guint depth = chain->depth + 1;

  for (node = chain->next; node != NULL; node = node->next) {
    if (node->depth == depth && node->type != XML_READER_TYPE_END_ELEMENT) {
      if ((item = menu_element_config (node, panel, iconsize)) != NULL) {
        gtk_menu_shell_append (GTK_MENU_SHELL(options), item);
        gtk_widget_show (item);
      }
    }
    else if (node->depth == chain->depth)  /* same depth.. move on */
      break;
  }
  return options;
} /* </menu_submenu_config> */

/*
* menu_options_config
*/
GtkWidget *
menu_options_config (ConfigurationNode *menu, GlobalPanel *panel, gint iconsize)
{
  GtkWidget *item;
  GtkWidget *options = gtk_menu_new();

  ConfigurationNode *node = menu->next;
  ConfigurationNode *mark = configuration_find_end (menu);

  guint depth = menu->depth + 1;


  /* Configure the menu header, then walk the menu configuration. */
  if ((item = menu_header_config (menu, panel)) != NULL) {
    gtk_menu_shell_append (GTK_MENU_SHELL(options), item);
    gtk_widget_show (item);
  }

  for (; node != mark && node != NULL; node = node->next) {
    if (node->depth == depth && node->type != XML_READER_TYPE_END_ELEMENT)
      if ((item = menu_element_config (node, panel, iconsize)) != NULL) {
        gtk_menu_shell_append (GTK_MENU_SHELL(options), item);
        gtk_widget_show (item);
      }
  }
  return options;
} /* </menu_options_config> */

/*
* menu_config
*/
GtkWidget *
menu_config (Modulus *applet, GlobalPanel *panel)
{
  GtkWidget *button, *image, *layout;
  ConfigurationNode *node = configuration_find (panel->config, "menu");

  PanelIcons   *icons = panel->icons;
  PanelTaskbar *menu  = panel->taskbar;

  guint iconsize = menu->iconsize;
  const gchar *name;

  if (node == NULL)	/* <menu ..> not in the configuration */
    return NULL;

  /* Create the main widget visible on the panel bar. */
  if (panel->orientation == GTK_ORIENTATION_HORIZONTAL)
    layout = gtk_hbox_new (FALSE, 0);
  else
    layout = gtk_vbox_new (FALSE, 0);

  gtk_container_set_border_width (GTK_CONTAINER (layout), 2);
  gtk_widget_show (layout);

  /* See if an icon is specified in the configuration. */
  if ((applet->icon = configuration_attrib (node, "icon")) != NULL) {
    const gchar *file = icon_path_finder (icons, applet->icon);

    if (file != NULL) {
      image = image_new_from_file_scaled (file, iconsize, iconsize);
      gtk_box_pack_start (GTK_BOX (layout), image, FALSE, FALSE, 3);
      gtk_widget_show (image);
    }
  }

  /* Use the name attribute in the configuration. */
  if ((name = configuration_attrib (node, "name")) != NULL) {
    GtkWidget *label = gtk_label_new (_(name));
    gtk_box_pack_start (GTK_BOX (layout), label, FALSE, TRUE, 3);
    gtk_widget_show (label);
  }
  else if (applet->icon == NULL) {
    image = xpm_image_scaled (ICON_START, iconsize, iconsize);
    gtk_box_pack_start (GTK_BOX (layout), image, FALSE, FALSE, 3);
    gtk_widget_show (image);
  }

  /* See if icon and/or name was changed. */
  if (menu->button && menu->start) {
    gtk_container_remove (GTK_CONTAINER(menu->button), menu->start);
    button = menu->button;
  }
  else {
    button = menu->button = gtk_toggle_button_new ();

    g_signal_connect (G_OBJECT (button), "button-press-event",
                      G_CALLBACK (activate), panel); 
  }

  /* Main widget for the top level menu. */
  gtk_container_add (GTK_CONTAINER(button), layout);
  gtk_button_set_relief (GTK_BUTTON(button), GTK_RELIEF_NORMAL);
  menu->start = layout;

  return button;
} /* </menu_config> */
