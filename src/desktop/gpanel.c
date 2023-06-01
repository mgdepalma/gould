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

<<<<<<< HEAD
#include "gould.h"		/* common package declarations */
=======
#include "gould.h"	/* common package declarations */
>>>>>>> 1c7ba7d252389ff48c813c34f56bde04273b373c
#include "gpanel.h"
#include "gsession.h"

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <sys/prctl.h>
#include <execinfo.h>

const gchar *Authors = "Mauro Gianni DePalma";  /* <bugs@softcraft.org> */
static GlobalPanel *global = NULL; /* (protected) encapsulated program data */

const char *Program = "gpanel";	   /* (public) published program name    */
const char *Release = "1.6.6";	   /* (public) published program version */

const char *Information =
"is a gtk-based simple desktop panel.\n"
"\n"
"The program is developed for Generations Linux and distributed\n"
"under the terms and condition of the GNU Public License.\n"
"\n\t\thttp://www.softcraft.org\n";

const char *Usage =
"usage: %s [-h | -v | -c | -l | -n | -r | -s]\n"
"\n"
"\t-h print help usage\n"
"\t-v print version information\n"
"\t-c display contrl panel\n"
"\t-l display logout dialog\n"
"\t-n create new desktop shortcut\n"
"\n"
"without options start program or display the configuration control\n"
"panel if the program is already running. The -l and -n options only\n"
"come into effect if the program is already running.\n"
"\n";

const char *Schema = "panel";	/* (public) XML configuration schema */
const char *ConfigurationHeader =
"<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
"<%s>\n"
"\n"
" <!-- Automatically generated ~/.config/panel, please DO NOT edit. Manual\n"
"   changes made will be lost when the program using this file exits.  -->\n"
"\n";

debug_t debug = 0;  /* debug verbosity (0 => none) {must be declared} */
int _stream = -1;   /* stream socket descriptor */
<<<<<<< HEAD
=======

const char *gpanelLock = "/tmp/gpanel.lock";
>>>>>>> 1c7ba7d252389ff48c813c34f56bde04273b373c


/*
* configuration_new creates new configuration data structure and file
*/
ConfigurationNode *
configuration_new (const char *resource)
{
  ConfigurationNode *config = NULL;
  char *reference;

  /* Locate the system-wide resource configuration file. */
  if (access("/usr/local/share/gould/panel", R_OK) == 0)
    reference = "/usr/local/share/gould/panel";
  else
    reference = "/usr/share/gould/panel";

  if (access(reference, R_OK) == 0) {
    if ((config = configuration_read (reference, Schema, FALSE))) {
      if (resource != NULL) {
        FILE *stream = fopen(resource, "w");

        if (stream) {
          configuration_write (config, ConfigurationHeader, stream);
          fclose(stream);
        }
      }
    }
  }
  return config;
} /* </configuration_new> */

/*
* check_configuration_version
*/
static void
check_configuration_version (GlobalPanel *panel, SchemaVersion *version)
{
  ConfigurationNode *config = panel->config;
  ConfigurationNode *chain;
  ConfigurationNode *clone;
  ConfigurationNode *match;
  ConfigurationNode *place;

  if (version && version->major < 1) {	/* keep only desktop shortcuts */
    match = configuration_find (config, "desktop");

    fprintf(stderr, "**Warning* Configuration schema version too old.\n");
    panel->config = configuration_new (panel->resource);

    if (match != NULL) {
      clone = configuration_clone (match);
      chain = configuration_find (panel->config, "desktop");
      place = chain->back;

      configuration_remove (chain);	/* note: chain is on free list */
      configuration_insert (clone, place, 0);
    }

    configuration_remove (config);	/* free original configuration */
  }

  if (configuration_find (config, "quicklaunch") == NULL) {
    const char *spec =
"<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
"<quicklaunch>\n"
" <item name=\"Web Browser\" icon=\"web.png\">\n"
"  <exec>xwww</exec>\n"
" </item>\n"
" <item name=\"Mail\" icon=\"mail.png\">\n"
"  <exec>xmail</exec>\n"
" </item>\n"
"</quicklaunch>\n";

    clone = configuration_read (spec, NULL, TRUE);
    chain = configuration_find (panel->config, "modules");

    configuration_insert (clone, chain->back, 1);
  }

  if(debug) configuration_write (panel->config,
                                 ConfigurationHeader,
                                 stdout);
} /* </check_configuration_version> */

/*
 * icon_path_finder wrapper for path_finder()
 */
const char *
icon_path_finder (PanelIcons *icons, const char *name)
{
  return path_finder (icons->path, name);
} /* </icon_path_finder> */

/*
* (protected) start_menu_open
*/
static void
start_menu_open (Modulus *applet)
{
  applet->widget = menu_config (applet, applet->data);
} /* </start_menu_open> */

/*
* (private) applets_builtin - setup internal applets
*/
static GList *
applets_builtin (GlobalPanel *panel)
{
  GList   *iter;		/* modules list iterator */
  GList   *list = NULL;		/* builtin modules list */
  Modulus *applet;		/* module instance */


  /* Configuration of pager and tasklist. */
  taskbar_config (panel);

  /* Add applications menu. */
  applet = panel->start = g_new0 (Modulus, 1);
  applet->module_open = start_menu_open;
  applet->data = panel;
  applet->name = "menu";
  applet->icon = "startx.png";
  applet->place = PLACE_START;
  applet->space = MODULI_SPACE_TASKBAR;
  applet->release = Release;
  applet->authors = Authors;
  applet->label = _("Start Menu");
  applet->settings = settings_menu_new (applet);
  list = g_list_append (list, applet);

  /* Add pager applet. */
  applet = panel->pager = g_new0 (Modulus, 1);
  applet->module_open = pager_open;
  pager_init (applet, panel);
  list = g_list_append (list, applet);

  /* Add tasklist applet. */
  applet = panel->tasklist = g_new0 (Modulus, 1);
  applet->module_open = tasklist_open;
  tasklist_init (applet, panel);
  list = g_list_append (list, applet);

  /* Set fallback/initial defaults. */
  for (iter = list; iter != NULL; iter = iter->next) {
    applet = (Modulus *)iter->data;

    if (applet->place == 0 && (applet->space & MODULI_SPACE_TASKBAR))
      applet->place = PLACE_START;

    applet->enable = TRUE;
  }

  return list;
} /* </applets_builtin> */

/*
* panel_config_orientation
*/
void
panel_config_orientation (GlobalPanel *panel)
{
  gint height = gdk_screen_height ();
  gint width  = green_screen_width ();

  if (panel->place == GTK_POS_BOTTOM || panel->place == GTK_POS_TOP) {
    panel->orientation = GTK_ORIENTATION_HORIZONTAL;

    panel->width  = width - 2 * panel->margin;
    panel->height = panel->thickness;
    panel->xpos   = panel->margin;
    panel->epos   = panel->width;

    if (panel->place == GTK_POS_BOTTOM)
      panel->ypos = height - panel->thickness - 1;
    else
      panel->ypos = 1;
  }
  else {
    panel->orientation = GTK_ORIENTATION_VERTICAL;

    panel->width  = panel->thickness;
    panel->height = height - 2 * panel->margin;
    panel->ypos   = panel->margin;
    panel->epos   = panel->height;

    if (panel->place == GTK_POS_RIGHT)
      panel->xpos = width - panel->thickness - 1;
    else
      panel->xpos = 1;
  }

  /* Adjust start and end packing positions. */
  panel->spos  = panel->indent + panel->margin;
  panel->epos -= panel->indent - panel->margin;
} /* </panel_config_orientation> */

/*
* (private) panel_config_settings
*/
static void
panel_config_settings (GlobalPanel *panel)
{
  ConfigurationNode *item;
  ConfigurationNode *chain;
  ConfigurationNode *config = panel->config;

  Display *display = GDK_WINDOW_XDISPLAY( GDK_ROOT_PARENT() );
  GlobalShare *shared = g_new0 (GlobalShare, 1);

  PanelIcons *icons = panel->icons = g_new0 (PanelIcons, 1);

  gchar *attrib = g_strdup_printf ("%s:%s", Program, XLOCK);
  /* gchar **path = g_strsplit (getenv("PATH"), ":", MAX_PATHNAME); */

  /* initialize panel->shared for interprocess communication */
  shared->display = display;
  shared->window  = DefaultRootWindow (display);
  shared->saver   = XInternAtom (display, attrib, False);
  panel->shared   = shared;

  XSetSelectionOwner(display, shared->saver, shared->window, CurrentTime);
  g_free (attrib);

  /* configuration for general settings  */
  panel->notice = NULL;		/* initialize alert notices */
  panel->path = NULL;		/* initialize exec search path */
  /* g_strfreev (path); */

  /* Configuration for the icons. */
  icons->path  = NULL;
  icons->size  = 22;

  if ((chain = configuration_find (config, "icons")) != NULL) {
    gchar *value;

    if ((item = configuration_find (chain, "size")) != NULL)
      icons->size = atoi(item->element);

    if ((value = configuration_path (chain)) != NULL) {
      icons->path = g_list_append (icons->path, value);

      /* FIXME: danger of an infinite loop */
      while (value != NULL) {
        item = configuration_find (chain, "path");  /* next <path> */

        if (item->next && (item->next)->next) {
          chain = (item->next)->next;

          if ((value = configuration_path (chain)) != NULL)
            icons->path = g_list_append (icons->path, value);
        }
      }
    }
  }

  if (icons->path == NULL) {	/* fallback icons path */
    icons->path = g_list_append (icons->path, "/usr/local/share/gould/pixmaps");
    icons->path = g_list_append (icons->path, "/usr/share/gould/pixmaps");
    icons->path = g_list_append (icons->path, "/usr/share/pixmaps");
  }

  /* Configuration of remaining settings. */
  panel->indent = 0;			 /* default indents at each end */
  panel->margin = 0;			 /* default margins at each end */
  panel->thickness = 32;		 /* default panel bar thickness */
  panel->place = GTK_POS_TOP;	         /* default place & orientation */

  if ((item = configuration_find (config, "indent")) != NULL)
    panel->indent = atoi(item->element);

  if ((item = configuration_find (config, "margin")) != NULL)
    panel->margin = atoi(item->element);

  if ((item = configuration_find (config, "thickness")) != NULL)
    panel->thickness = atoi(item->element);

  if ((item = configuration_find (config, "place")) != NULL) {
    if (strcmp(item->element, "TOP") == 0)
      panel->place = GTK_POS_TOP;
    else if (strcmp(item->element, "BOTTOM") == 0)
      panel->place = GTK_POS_BOTTOM;
    else if (strcmp(item->element, "RIGHT") == 0)
      panel->place = GTK_POS_RIGHT;
    else if (strcmp(item->element, "LEFT") == 0)
      panel->place = GTK_POS_LEFT;
  }

  panel_config_orientation (panel);  /* need to config orientation */
} /* </panel_config_settings> */

/*
* (private) panel_quicklaunch
* (private) panel_quicklaunch_open
*/
static void
panel_quicklaunch (GtkWidget *widget, Modulus *applet)
{
  GlobalPanel *panel = applet->data;
  const gchar *program = path_finder(panel->path, applet->label);

  if (program)
    dispatch (program, panel->session);
  else
     notice_at(100, 100, ICON_WARNING, "[%s]%s: %s.",
               Program, applet->label, _("command not found"));

} /* </panel_quicklaunch> */

static void
panel_quicklaunch_open (Modulus *applet)
{
  GtkWidget   *button, *image;
  GtkTooltips *tooltips;

  GlobalPanel *panel = applet->data;
  PanelIcons  *icons = panel->icons;
  const gchar *icon;

  /* Construct the user interface. */
  icon  = icon_path_finder (icons, applet->icon);
  image = image_new_from_file_scaled (icon, icons->size, icons->size);

  button = gtk_button_new();
  gtk_button_set_relief (GTK_BUTTON(button), GTK_RELIEF_NONE);
  gtk_button_set_image (GTK_BUTTON(button), image);

  g_signal_connect (G_OBJECT(button), "clicked",
                      G_CALLBACK(panel_quicklaunch), applet);

  /* Construct tooltips for the widget. */
  tooltips = gtk_tooltips_new();
  gtk_tooltips_set_tip (tooltips, button, applet->name, NULL);

  applet->widget = button;
} /* </panel_quicklaunch_open> */

/*
* (private) panel_config_moduli
*/
static void
panel_config_moduli (GlobalPanel *panel, GList *builtin, GList *plugins)
{
  ConfigurationNode *config = panel->config;
  ConfigurationNode *chain = configuration_find (config, "quicklaunch");
  ConfigurationNode *mark = configuration_find_end (chain);
  ConfigurationNode *exec;
  const gchar *value;

  GList   *iter, *list;		/* modules list iterator */
  Modulus *applet;		/* applet instance */


  /* Concatenate builtin + plugins to form complete list. */
  list = panel->moduli = g_list_concat (builtin, plugins);

  /* Screen saver applet. */
  applet = panel->xlock = g_new0 (Modulus, 1);
  applet->module_close = saver_close;
  applet->module_open = saver_open;
  applet->enable = TRUE;

  if (saver_init (applet, panel)) 	/* sanity check.. */
    list = g_list_append (list, applet);

  /* Add quicklaunch shortcuts to the list. */
  while (chain && chain != mark) {
    if (strcmp(chain->element, "item") == 0) {
      value = configuration_attrib (chain, "name");

      for (iter = list; iter != NULL; iter = iter->next)
        if (strcmp(((Modulus *)iter->data)->name, value) == 0)
          break;

      if (iter == NULL) {		/* not in the list */
        if ((exec = configuration_find (chain, "exec")) != NULL) {
          applet = g_new0 (Modulus, 1);
          applet->data = panel;
          applet->name = value;
          applet->icon = configuration_attrib (chain, "icon");
          applet->place = PLACE_START;
          applet->space = MODULI_SPACE_TASKBAR;
          applet->module_open = panel_quicklaunch_open;
          applet->label = exec->element;
          applet->enable = TRUE;

          list = g_list_append (list, applet);
        }
      }
    }
    chain = chain->next;
  }

  /* The anchor applet (ex: clock) will be last on the panel. */
  for (iter = list; iter != NULL; iter = iter->next) {
    applet = (Modulus *)iter->data;

    if (applet->space == MODULI_SPACE_ANCHOR) {
      list = g_list_remove (list, applet);  /* move to the end of the list */
      list = g_list_append (list, applet);
      break;
    }
  }

  if (debug) {
    for (iter = list; iter != NULL; iter = iter->next) {
      applet = (Modulus *)iter->data;
      printf ("applet name => %s, place => %d, space => 0x%.4x\n",
               applet->name, applet->place, applet->space);
    }
  }
} /* </panel_config_moduli> */

/*
* (protected) panel_update_pack_position
*
* This is an interim implementaion approach to account for the position
* of each applet so that applets which display protruding gadgets, such
* a volume control slider, etc. can display the gadget positioned near
* the applet->widget window. A better approach would be to obtaint the
* the screen coordinates of the applet->widget window.
*/
void
panel_update_pack_position (GlobalPanel *panel, Modulus *applet)
{
  GtkRequisition requisition;

  /* Obtain the dimensions of the applet->widget */
  gtk_widget_size_request (applet->widget, &requisition);
  
  if (applet->place == PLACE_START)
    panel->spos += requisition.width - 4;
  else
    panel->epos += requisition.width - 4;

  vdebug (2, "panel spos => %d, epos => %d, applet => %s\n",
              panel->spos, panel->epos, applet->name);
} /* </panel_update_pack_position> */

/*
* (private) applets_loadable
*/
static GList *
applets_loadable (GlobalPanel *panel, guint space)
{
  GList *iter, *list = NULL;
  GList *moduli = NULL;

  ConfigurationNode *config = panel->config;
  ConfigurationNode *chain = configuration_find (config, "modules");
  ConfigurationNode *mark = configuration_find_end (chain);
  ConfigurationNode *item;

  Modulus *applet;
  gchar *path;


  /* Add plugin modules to the list at each <path ..> */
  if (chain && (path = configuration_path (chain)) != NULL) {
    list = moduli_space (list, path, space, panel);

    while (path != NULL) {
      item = configuration_find (chain, "path");  /* next <path ..> */

      if (item->next && item->next->next) {
        chain = item->next->next;

        if ((path = configuration_path (chain)) != NULL)
          list = moduli_space (list, path, space, panel);
      }
    }

    /* Construct moduli list in the order of configuration. */
    chain = configuration_find (config, "modules") -> next;

    while (chain && chain != mark) {
      if (strcmp(chain->element, "applet") == 0) {
        const gchar *name = configuration_attrib (chain, "name");

        for (iter = list; iter != NULL; iter = iter->next)
          if (strcmp(((Modulus *)iter->data)->name, name) == 0)
            moduli = g_list_append (moduli, iter->data);

        chain = chain->next;
      }
      chain = chain->next;
    }
  }


  /* Set fallback/initial defaults. */
  for (iter = moduli; iter != NULL; iter = iter->next) {
    applet = (Modulus *)iter->data;

    if (applet->place == 0 && (applet->space & MODULI_SPACE_TASKBAR))
      applet->place = PLACE_END;

    applet->enable = TRUE;
  }

  return moduli;
} /* </applets_loadable> */

/*
* settings_initialize post modules load initialization
*/
static void
settings_initialize (GlobalPanel *panel)
{
  PanelIcons   *icons   = panel->icons;
  PanelLogout  *logout  = panel->logout  = g_new0 (PanelLogout, 1);
  PanelTaskbar *taskbar = panel->taskbar = g_new0 (PanelTaskbar, 1);


  ConfigurationNode *node = configuration_find (panel->config, "menu");
  const gchar *value = configuration_attrib (node, "iconsize");

  desktop_config (panel, TRUE);	/* PanelDesktop initialization */
  settings_new (panel);		/* PanelSetting initialization */

  /* Initialize structures for start menu. */
  taskbar->iconsize = (value) ? atoi(value) : icons->size;
  taskbar->options  = menu_options_config (node, panel, taskbar->iconsize);

  /* Create the shutdown dialog and ghost backdrop windows. */
  panel->backdrop = backdrop_window_new (GDK_WINDOW_TYPE_HINT_NORMAL,
                                        gdk_screen_width(),
                                        gdk_screen_height(),
                                        0, 0, "00ff");

  /* Must be created after panel->backdrop */
  logout->window = shutdown_dialog_new (panel);
} /* </settings_initialize> */

/*
* interface - construct user interface
*/
GtkWidget *
interface (GlobalPanel *panel)
{
  static gboolean once = TRUE;	/* one time initialization */

  GtkWidget *layout;		/* user interface layout */
  GtkWidget *space;		/* margin space */


  /* Adjust orientation of taskbar. */
  if (panel->orientation == GTK_ORIENTATION_HORIZONTAL)
    layout = gtk_hbox_new (FALSE, 1);
  else
    layout = gtk_vbox_new (FALSE, 1);

  /* Spacing at both ends of the layout box. */
  if (panel->indent > 0) {
    space = gtk_label_new ("");
    gtk_box_pack_start (GTK_BOX(layout), space, FALSE, FALSE, panel->indent);
    gtk_widget_show (space);

    space = gtk_label_new ("");
    gtk_box_pack_end (GTK_BOX(layout), space, FALSE, FALSE, panel->indent);
    gtk_widget_show (space);
  }

  if (once) {			/* one time initialization */
    settings_initialize (panel);
    once = FALSE;
  }

  /* Activate all modules from panel->moduli */
  taskbar_initialize (panel, layout);

  return layout;
} /* </interface> */

/*
* selfexclude - WindowFilter to exclude self from GREEN window lists
*/
static gboolean
selfexclude (Window xid, int desktop)
{
  const gchar *wname = get_window_name (xid);
  return (wname) ? strcmp(wname, (char *)Program) == 0 : TRUE;
} /* </selfexclude> */

/*
* open session stream socket
*/
static inline int
session_open(const char *pathway)
{
  int stream = socket(PF_UNIX, SOCK_STREAM, 0);

  if (stream < 0)
    perror("opening stream socket"); 
  else {
    struct sockaddr_un address;
    memset(&address, 0, sizeof(struct sockaddr_un));

    address.sun_family = AF_UNIX;
    snprintf(address.sun_path, UNIX_PATH_MAX, pathway);

    if (connect(stream, (struct sockaddr *)&address, sizeof(struct sockaddr_un))) {
      perror("connect stream socket");
      stream = -1;
    }
  }
  return stream;
} /* session_open */

/*
<<<<<<< HEAD
* signal_systemtray_process
*/
pid_t
signal_systemtray_process(int sig)
{
  static char answer[MAX_STRING];
  gchar *command = g_strdup_printf ("pidof %s", Program);
  FILE *stream = popen(command, "r");
  pid_t ppid = 0;			/* process running systemtray */

  if (stream) {
    const char delim[2] = " ";
    const char *master, *token;

    fgets(answer, MAX_STRING, stream);   /* answer maybe a list */
    master = token = strtok(answer, delim);

    while ( token != NULL ) {
      token = strtok(NULL, delim);
      if(token != NULL) master = token;  /* pid is last on the list */
    }
    ppid = strtoul(master, NULL, 10);
    /* kill(ppid, sig); */
    pclose(stream);
  }
  g_free (command);

  return ppid;
} /* </signal_systemtray_process> */

/*
* gpanel_initialize
=======
* get_pid_from_lockfile
*/
pid_t
get_pid_from_lockfile(const char *lockfile)
{
  pid_t pid = 0;
  FILE *stream = fopen(lockfile, "r");

  if (stream) {
    fscanf (stream, "%d", &pid);
    fclose(stream);
  }
  return pid;
} /* </get_pid_from_lockfile> */

/*
* initialize
>>>>>>> 1c7ba7d252389ff48c813c34f56bde04273b373c
*/
void
gpanel_initialize (GlobalPanel *panel)
{
  GList *builtin, *plugins;
  SchemaVersion *version = NULL;
  const char *abend = "another program is already using system tray";

  char dirname[MAX_PATHNAME];
  char *home = getenv("HOME");
  char *scan;

  /* initialize configuration variables */
  panel->config   = NULL;
  panel->settings = NULL;
  panel->resource = g_strdup_printf("%s/.config/panel", home);
  panel->green    = green_filter_new (selfexclude, DefaultScreen(gdk_display));

  if (systray_check_running_screen (green_get_gdk_screen (panel->green))) {
<<<<<<< HEAD
    pid_t pid = signal_systemtray_process (SIGUSR1);
    g_printerr("%s: %s (pid => %d)\n", Program, abend, pid);
    if(pid > 0) kill(pid, SIGUSR1);
    _exit(_RUNNING);
  }
  panel->systray  = systray_new ();
#ifndef NEVER
  panel->session  = session_open(_GSESSION);
#else
  printf("%s: not calling session_open() until it stops hanging\n", __func__);
#endif
=======
    pid_t pid = get_pid_from_lockfile (gpanelLock);
    g_printerr("%s: another instance is already running (pid => %d)\n",
                    Program, pid);
    _exit(RUNNING_);
  }
>>>>>>> 1c7ba7d252389ff48c813c34f56bde04273b373c

  panel->systray  = systray_new ();
  strcpy(dirname, panel->resource);	/* obtain parent directory path */
  scan = strrchr(dirname, '/');

  if (scan != NULL) {
    *scan = (char)0;
    /* create parent directory as needed */
    if(access(dirname, F_OK) != 0) mkdir(dirname, 0755);
  }

  /* read and cache system wide configuration file */
  panel->sysconfig = configuration_new (NULL);

  /* read and cache user configuration file */
  if (access(panel->resource, R_OK) == 0)
    panel->config = configuration_read (panel->resource, Schema, FALSE);

  /* check for missing or corrupt configuration file */
  if (panel->config != NULL)
    version = configuration_schema_version (panel->config);
  else
    panel->config = configuration_new (panel->resource); /* new configuration */

  /* check configuration schema version compatibility */
  check_configuration_version (panel, version);
  panel_config_settings (panel);	     /* initial settings from config */

  builtin = applets_builtin (panel);		        /* builtin modules */
  plugins = applets_loadable (panel, MODULI_SPACE_ALL); /* loadable modules */

  /* arrange plugin modules according to configuration settings */
  panel_config_moduli (panel, builtin, plugins);
} /* </gpanel_initialize> */

/*
* application constructor
*/
void
constructor (GlobalPanel *panel)
{
  GtkWidget *frame;
  GtkWidget *layout;
  GtkWidget *widget;
  GList     *iter;

  /* Create a new top level window and set attributes. */
  widget = panel->window = sticky_window_new (GDK_WINDOW_TYPE_HINT_DOCK,
                                              panel->width, panel->height,
                                              panel->xpos, panel->ypos);

  gtk_window_set_keep_below (GTK_WINDOW(widget), TRUE);
  if (panel->visible) gtk_widget_show (widget);

  /* Give the layout box a nicer look. */
  frame = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_OUT);
  gtk_container_add (GTK_CONTAINER(widget), frame);
  gtk_widget_show (frame);

  /* Contruct the user interface. */
  layout = interface (panel);
  gtk_container_add (GTK_CONTAINER(frame), layout);
  gtk_widget_show (layout);

  /* Iterate the alert notices list. */
  for (iter = panel->notice; iter != NULL; iter = iter->next)
    notice_at(100, 100, ICON_WARNING, "%s: %s.", Program, (gchar*)iter->data);

  g_list_free (panel->notice);
  panel->notice = NULL;
} /* </constructor> */

/*
* reload data structures and reconstruct as needed
*/
int
panel_loader (GlobalPanel *panel)
{
  int status = EX_CONFIG;	/* i.e. config corrupted or missing */

  if (panel->config == NULL)
    gpanel_initialize (panel);	/* initialize GlobalPanel data structure */

  if (panel->config) {
    const char *value = configuration_attrib (panel->config, "visible");
    panel->visible = TRUE;	/* unless otherwise, assume visible */

    if (value)
      if (strcmp(value, "false") == 0 || strcmp(value, "no") == 0)
        panel->visible = FALSE;

    constructor (panel);
    status = EX_OK;
  }
  return status;
} /* </panel_loader> */

/*
* check running status
*/
/* inline */ pid_t
gpanel_getpid(const char *lockfile)
{
  FILE *stream;			/* file descriptor for lockfile */
  pid_t pid = 0;

  if ((stream = fopen(lockfile, "r"))) {  /* lockfile exists */
    const int bytes = sizeof(double);

    char line[bytes];
    fgets(line, bytes, stream);
    fclose(stream);

    if (strlen(line) > 0) {
      char *procpid;

      line[strlen(line) - 1] = (char)0;		/* chomp newline */
      procpid = g_strdup_printf("/proc/%s", line);

      if(access(procpid, F_OK) == 0) pid = atoi(line);
      g_free (procpid);
    }
  }
  return pid;
} /* </gpanel_getpid> */

/*
* application unique instance
*/
pid_t
gpanel_instance (GlobalPanel *panel)
{
<<<<<<< HEAD
  pid_t instance = 0;                   /* implies all went wrong */
  memset(panel, 0, sizeof(GlobalPanel));

  if (panel_loader(panel) == EX_OK) {
    instance = getpid();
    global = panel;		/* save GlobalPanel data structure */
  }
  else {			/* configuration file disappeared */
    if (global)
      notice_at(50, 50, ICON_ERROR,"%s: %s.",
			Program, _("cannot find configuration file"));
    else
      printf("%s: %s.", Program, _("cannot find configuration file"));

    exit(EX_CONFIG);
=======
  pid_t instance = 0;
  memset(panel, 0, sizeof(GlobalPanel));

  if (panel_loader(panel) == EX_OK) {
    FILE *stream = fopen(gpanelLock, "w");

    if (stream) {
      instance = getpid();
      fprintf(stream, "%d\n", instance);
      panel->lockfile = (char *)gpanelLock;
      fclose(stream);
    }
    global = panel;             /* save GlobalPanel data structure */
  }
  else {                        /* configuration file disappeared */
    if (global)
      notice_at(50, 50, ICON_ERROR,"%s: %s.",
                        Program, _("cannot find configuration file"));
    else
      printf("%s: %s.", Program, _("cannot find configuration file"));

    _exit(EX_CONFIG);
>>>>>>> 1c7ba7d252389ff48c813c34f56bde04273b373c
  }
  return instance;
} /* </gpanel_instance> */

/*
* responder - signal handler
*/
void
responder (int sig)
{
<<<<<<< HEAD
  const static debug_t BACKTRACE_SIZE = 69;
=======
  const static debug_t BACKTRACE_LINES = 69;
>>>>>>> 1c7ba7d252389ff48c813c34f56bde04273b373c

  switch (sig) {
    case SIGHUP:
    case SIGCONT:
    case SIGTERM:
      shutdown_panel (global, sig);
      break;

    case SIGUSR1:
      settings_activate (global);
      break;

    case SIGUSR2:
      desktop_settings (global, DESKTOP_SHORTCUT_CREATE);
      break;

    default:
<<<<<<< HEAD
#ifdef LOCKFILE
      remove (LOCKFILE);
#endif
      if (sig |= SIGINT && sig != SIGKILL) {
        void *trace[BACKTRACE_SIZE];
        int nptrs = backtrace(trace, BACKTRACE_SIZE);
=======
      remove (gpanelLock);

      if (sig |= SIGINT && sig != SIGKILL) {
        void *trace[BACKTRACE_LINES];
        int nptrs = backtrace(trace, BACKTRACE_LINES);
>>>>>>> 1c7ba7d252389ff48c813c34f56bde04273b373c

        printf("%s, exiting on signal: %d\n", Program, sig);
        backtrace_symbols_fd(trace, nptrs, STDOUT_FILENO);

        notice_at(50, 50, ICON_ERROR, "%s: %s.", Program,
<<<<<<< HEAD
				_("internal program error"));
=======
                             _("internal program error"));
>>>>>>> 1c7ba7d252389ff48c813c34f56bde04273b373c
      }
      gtk_main_quit ();
      exit (sig);
      break;
  }
} /* </responder> */

/*
* apply_signal_responder
*/
static inline void
apply_signal_responder(void)
{
  signal(SIGHUP,  responder);	/* reload */
  signal(SIGTERM, responder);	/* logout */
  signal(SIGCONT, responder);	/* cancel logout */
  signal(SIGUSR1, responder);	/* show control panel */
  signal(SIGUSR2, responder);	/* new desktop shortcut */
  signal(SIGABRT, responder);   /* internal program error */
  signal(SIGALRM, responder);   /* .. */
  signal(SIGBUS,  responder);   /* .. */
  signal(SIGILL,  responder);   /* .. */
  signal(SIGINT,  responder);   /* .. */
  signal(SIGIOT,  responder);   /* .. */
  signal(SIGKILL, responder);   /* .. */
  signal(SIGQUIT, responder);   /* .. */
  signal(SIGSEGV, responder);	/* .. */
  signal(SIGCHLD, SIG_IGN);	/* do not want to wait() for SIGCHLD */
} /* </apply_signal_responder> */

/*
* gpanel_lockfile_pid
*/
pid_t
gpanel_lockfile_pid(const char *lockfile)
{
  FILE *stream;                           /* lockfile descriptor */
  pid_t pid = 0;

  if ((stream = fopen(lockfile, "r"))) {  /* open lockfile readonly */
    const int bytes = sizeof(double);

    char line[bytes];
    fgets(line, bytes, stream);
    fclose(stream);

    if (strlen(line) > 0) {
      line[strlen(line) - 1] = (char)0;   /* chomp newline */
      pid = atoi(line);
    }
  }
  return pid;
} /* </gpanel_lockfile_pid> */

/*
* gpanel_lockfile_check
*/
int
gpanel_lockfile_check(const char *lockfile)
{
  int status = MISSING_;

  if (access(lockfile, F_OK) == 0) {
    char procfile[MAX_STRING];
    sprintf(procfile, "/proc/%d", gpanel_lockfile_pid(lockfile));

    if (access(procfile, F_OK) == 0) {
      FILE *stream;
      strcat(procfile, "/cmdline");

      if ( (stream = fopen(procfile, "r")) ) {
        char command[MAX_LABEL], *match, *program;

        fgets(command, MAX_LABEL, stream);
        program = strrchr(command, '/');
        match = (program != NULL) ? program : command;
        status = (strcmp(match, Program) == 0) ? VALID_ : INVALID_;

        fclose(stream);
      }
    }
  }
  return status;
} /* </gpanel_lockfile_check> */

/*
* main - gpanel program main
*/
int
main(int argc, char *argv[])
{
  GlobalPanel memory;		/* master data structure (gpanel.h) */
  char *alias = NULL;		/* used to changer process name */

  int sig = SIGUSR1;		/* show control panel */
  int status = EX_OK;
  int opt;

  /* disable invalid option messages */
  opterr = 0;
     
  while ((opt = getopt (argc, argv, "d:hvlnprs")) != -1) {
  /* while ((opt = getopt_long (argc, argv, opts, longopts, NULL)) != -1) */
    switch (opt) {
      case 'd':
        debug = atoi(optarg);
        break;

      case 'h':
        printf(Usage, Program);
        exit(0);

      case 'v':
        printf("\n%s %s %s\n", Program, Release, Information);
        exit(0);

      case 'c':			/* show control panel */
        sig = SIGUSR1;
        break;

      case 'l':			/* show logout panel */
        sig = SIGTERM;
        break;

      case 'n':			/* create new shortcut */
        sig = SIGUSR2;
        break;

      case 'p':			/* process name */
        alias = optarg;
        break;

      case 'r':
        sig = SIGCONT;
        break;

      case 's':
        sig = SIGHUP;
        break;

      default:
        if (optopt == 'd')		/* debug default => 1 */
          debug = 1;
        else {
          printf("%s: invalid option, use -h for help usage.\n", Program);
          exit(1);
        }
    }
  }
  apply_signal_responder();

#ifdef GETTEXT_PACKAGE
  bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);
  gtk_disable_setlocale();
#endif
<<<<<<< HEAD
  if(alias) prctl(PR_SET_NAME, (unsigned long)alias, 0, 0, 0);

  gtk_init (&argc, &argv);	/* initialization of the GTK */
  gtk_set_locale ();

  apply_signal_responder();
  gpanel_instance (&memory);	/* return pid_t ignored */

=======
  prctl(PR_SET_NAME, (unsigned long)gpanelProcess, 0, 0, 0);

  if (gpanel_lockfile_check (gpanelLock) != VALID_)
    instance = gpanel_instance (&memory);  /* gpanel_instance() initialize */
  else {
    instance = gpanel_lockfile_pid (gpanelLock);
    status = kill(instance, sig);
    return status;
  }

  gtk_init (&argc, &argv);	/* initialization of the GTK */
>>>>>>> 1c7ba7d252389ff48c813c34f56bde04273b373c
  apply_gtk_theme (CONFIG_FILE);
  gtk_set_locale ();
  gtk_main ();			/* main event loop */

  return status;
} /* </main> */
