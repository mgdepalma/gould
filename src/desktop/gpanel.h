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

#ifndef _GPANEL
#define _GPANEL "/tmp/gpanel"

#include "green.h"
#include "dialog.h"
#include "docklet.h"
#include "gwindow.h"
#include "xmlconfig.h"
#include "systray.h"
#include "module.h"
#include "util.h"

#include <signal.h>
#include <stdbool.h>
#include <sysexits.h>

#define SCHEMA_VERSION_CODE   SCHEMA_VERSION(1,2,0)
#define SCHEMA_VERSION_STRING "1.2"

G_BEGIN_DECLS

/* Desktop shortcut actions. */
typedef enum _desktop_actions DesktopAction;

enum _desktop_actions {
  DESKTOP_SHORTCUT_CREATE,
  DESKTOP_SHORTCUT_DELETE,
  DESKTOP_SHORTCUT_EDIT,
  DESKTOP_SHORTCUT_OPEN
};

/* Global program data structure */
typedef struct _GlobalShare GlobalShare;
typedef struct _GlobalPanel GlobalPanel;

typedef struct _PanelDesktop PanelDesktop;
typedef struct _PanelIcons   PanelIcons;
typedef struct _PanelLogout  PanelLogout;
typedef struct _PanelSetting PanelSetting;
typedef struct _PanelTaskbar PanelTaskbar;

struct _GlobalShare
{
  Display *display;
  Window  window;
  Atom    saver;		/* IPC with PanelSaver */
};

struct _GlobalPanel
{
  int session;			/* session stream socket */
  gchar *resource;		/* user configuration file */

  ConfigurationNode *config;	/* user configuration cache */
  ConfigurationNode *sysconfig;	/* system configuration cache */

  GlobalShare *shared;		/* interprocess communication */
  Green *green;			/* GREEN instance for workspaces */

  PanelDesktop *desktop;	/* create/edit desktop shortcut */
  PanelIcons   *icons;		/* icons configuration data */
  PanelLogout  *logout;		/* exit/logout dialog data */
  PanelSetting *settings;	/* configuration settings data */
  PanelTaskbar *taskbar;	/* menu/taskbar configuration data */

  Modulus *start;		/* start menu applet */
  Modulus *pager;		/* panel pager applet */
  Modulus *tasklist;		/* panel tasklist applet */
  Modulus *screensaver;		/* screen saver applet */

  GList *moduli;		/* plugin modules list */
  GList *notice;		/* alert notices list */
  GList *path;			/* PATH environment */

  Systray   *systray;		/* system tray manager */
  GtkWidget *backdrop;		/* ghost backdrop window */
  GtkWidget *quicklaunch;	/* quick launch icons box */
  GtkWidget *window;		/* main window widget */

  GtkOrientation orientation;	/* GTK_ORIENTATION_{HORIZONTAL, VERTICAL} */
  GtkPositionType place;	/* GTK_POS_{TOP, BOTTOM, LEFT, RIGHT} */

  bool visible;			/* controls if the panel is visible */

  guint thickness;		/* thickness of the panel bar */
  guint margin;			/* margin at each end */
  guint indent;			/* indent at each end */

  gint height, width;	 	/* panel bar main window dimensions */
  gint xpos, ypos;		/* (x,y) main window coordinates */
  gint spos, epos;		/* start and end pack positions */
};

struct _PanelDesktop
{
  gchar *folder;		/* usually $HOME/Desktop/ */
  const gchar *init;		/* initialization command */

  ConfigurationNode* node;	/* pertinent configuration node */
  GFileMonitor *monitor;	/* $HOME/Desktop/ directory monitor */

  DesktopAction action;		/* action to perform on desktop */
  Docklet *shortcut;		/* screen representation */

  FileChooser *chooser;		/* file chooser for selecting icon */
  GtkFunction chooser_apply;	/* optional additional apply callback */

  GtkWidget *menu;		/* builtin actions menu */
  GtkWidget *window;		/* popup widget window */
  GtkWidget *iconview;		/* icon display widget */
  GtkWidget *filer;		/* file chooser layout */

  GdkPixbuf *render;		/* pixbuf of the current icon */
  GtkWidget *canvas;		/* icon preview drawing area */
  GtkWidget *name;		/* name text entry field */
  GtkWidget *exec;		/* exec text entry field */
  GtkWidget *hint;		/* action hints */

  const gchar *icon;		/* default icon image pathname */
  bool active;			/* TRUE if desktop panel is active*/
  gint iconsize;		/* fixed icon width and height */

  gint xpos, ypos;		/* coordinates for new shortcut */
  gint step;			/* spacing between shortcuts */
};

struct _PanelIcons
{
  GList *path;			/* list of directory paths to search */
  const char *theme;		/* optional themes sub-directory */
  guint size;			/* width and height of each icon */
};

struct _PanelLogout {
  GtkWidget *window;		/* exit/logout dialog window */
  GtkWidget *shutdown;		/* shutdown button */
  GtkWidget *suspend;		/* suspend button */
  GtkWidget *reboot;		/* reboot button */
};

struct _PanelSetting
{
  GtkWidget   *window;		/* settings widget panel window */
  GtkNotebook *notebook;	/* main notebook (no tabs or border) */

  GtkFunction apply_cb;		/* additional apply callback */
  GtkFunction cancel_cb;	/* additional cancel callback */
  GtkFunction close_cb;		/* additional close callback */

  GtkWidget *apply;		/* apply button */
  GtkWidget *cancel;		/* cancel button */
  GtkWidget *close;		/* close button */
  GtkWidget *view;		/* selections */

  Modulus *applet;		/* current page data */
};

struct _PanelTaskbar
{
  GtkWidget *start;		/* main menu layout */
  GtkWidget *button;		/* main menu button */
  GtkWidget *options;		/* main menu options widget */
  guint iconsize;		/* width and height of each icon */
};

/* Methods exported by implementation. */
void taskbar_config (GlobalPanel *panel);
void taskbar_initialize (GlobalPanel *panel, GtkWidget *layout);

void desktop_config (GlobalPanel *panel, bool once);
void desktop_shortcut (Docklet *docklet, ConfigurationNode *node);
void desktop_settings (GlobalPanel *panel, DesktopAction act);

const char *icon_path_finder (PanelIcons *icons, const char *name);
GtkWidget *interface (GlobalPanel *panel);

GtkWidget *menu_config (Modulus *applet, GlobalPanel *panel);
GtkWidget *menu_options_config (ConfigurationNode *node,
				GlobalPanel *panel,
				gint iconsize);

GtkWidget *menu_submenu_config (ConfigurationNode *node,
				GlobalPanel *panel,
				gint iconsize);

Docklet *desktop_create (GlobalPanel *panel,
                         ConfigurationNode *node,
                         gint width, gint height,
                         gint xpos, gint ypos,
                         const gchar *icon,
                         const gchar *text,
                         const gchar *font,
                         GdkColor *fg,
                         GdkColor *bg);

int panel_loader (GlobalPanel *panel);
void panel_config_orientation (GlobalPanel *panel);
void panel_update_pack_position (GlobalPanel *panel, Modulus *applet);

void pager_module_init (Modulus *applet, GlobalPanel *panel);
void pager_module_open (Modulus *applet);
void pager_module_close (Modulus *applet);

void tasklist_module_init (Modulus *applet, GlobalPanel *panel);
void tasklist_module_open (Modulus *applet);
void tasklist_module_close (Modulus *applet);

bool screensaver_module_init (Modulus *applet, GlobalPanel *panel);
void screensaver_module_open (Modulus *applet);
void screensaver_module_close (Modulus *applet);

const char *screensaver_get_mode (void);
const int screensaver_get_selection (void);

GtkWidget *screensaver_settings (Modulus *applet, GlobalPanel *panel);
void screensaver_wake (GtkWidget *widget, GlobalPanel *panel);

GtkWidget *settings_manpage_new (Modulus *applet, GlobalPanel *panel);
GtkWidget *settings_missing_new (Modulus *applet, GlobalPanel *panel);
GtkWidget *settings_notebook_new (Modulus *applet, GlobalPanel *panel, ...);

GtkWidget *settings_menu_new (Modulus *applet);
void settings_new (GlobalPanel *panel);

void settings_activate (GlobalPanel *panel);
void settings_save_enable (PanelSetting *settings, bool state);
void settings_set_agents (PanelSetting *settings,
                          GtkFunction apply_cb,
                          GtkFunction cancel_cb,
                          GtkFunction close_cb);

GtkWidget *setbg_settings_new (Modulus *applet, GlobalPanel *panel);
GtkWidget *shutdown_dialog_new (GlobalPanel *panel);

void
startmenu (GtkMenu *menu, gint *x, gint *y, bool *pushin, gpointer data);

pid_t session_request (int stream, const char *command);
pid_t spawn_selected (ConfigurationNode *node, GlobalPanel *panel);
pid_t dispatch (int stream, const char *command);

void executer (GtkWidget *widget, ConfigurationNode *node);

void shutdown_panel (GlobalPanel *panel, int sig);
void restart (GlobalPanel *panel);

void reconstruct (GlobalPanel *panel);
int saveconfig (GlobalPanel *panel);

void gpanel_dialog(gint xpos, gint ypos, IconIndex icon, const gchar* fmt, ...);
gint spawn_dialog(gint xpos, gint ypos, IconIndex icon, const gchar* fmt, ...);
G_END_DECLS

#endif /* </GPANEL_H */
