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
#include "filechooser.h"
#include "module.h"

#ifndef get_current_dir_name
extern char *get_current_dir_name(void);
#endif

extern const char *Program, *Release;     /* see, gpanel.c */

/*
* Data structures used by this module.
*/
typedef struct _WallpaperSettings WallpaperSettings;

struct _WallpaperSettings {
  FileChooser *chooser;		/* file chooser instance */
  GlobalPanel *panel;

  GtkWidget *window;		/* main window: paned top, controls bottom */
  GtkWidget *browser;		/* file browser left side panel */
  GtkWidget *viewer;		/* preview image right side panel */
  GtkWidget *canvas;		/* image canvas drawing area */

  GtkWidget *backward;		/* backward one selection button */
  GtkWidget *forward;		/* forward one selection button */

  char pathname[MAX_PATHNAME];
  char resource[MAX_PATHNAME];	/* resource path ($HOME/.config/desktop) */
};

/* Forward prototype declarations. */
static void
setbg_refresh (GtkWidget *canvas, GdkEventExpose *ev, gpointer data);

static WallpaperSettings config_;  /* private global structure singleton */

/*
* (private) configsave
*/
static bool
configsave(WallpaperSettings *config)
{
  FILE *stream;
  char **buffer = NULL;
  char line[MAX_PATHNAME];
  int count = 0;
  int idx;

  /* Read config->resource in memory buffer. */
  if ((stream = fopen(config->resource, "r"))) {
    while (fgets(line, MAX_PATHNAME, stream)) ++count;

    /* Allocate memory space for config->resource */
    if ((buffer = calloc(count, sizeof (char *))) == NULL) {
      fclose(stream);
      return false;
    }
    fseek(stream, 0L, SEEK_SET);

    for (idx=0; idx < count; idx++) {
      if (fgets(line, MAX_PATHNAME, stream) <= 0) break;
      buffer[idx] = strdup(line);
    }
    fclose(stream);
  }

  /* [Re]open config->resource for writing. */
  if ((stream = fopen(config->resource, "w"))) {
    bool insert = true;
    gchar *pathname = filechooser_get_selected_name (config->chooser);

    for (idx=0; idx < count; idx++) {
      if (strncmp(buffer[idx], "BACKGROUND=", 11) == 0) {
        fprintf(stream, "BACKGROUND=\"%s\"\n", pathname);
        insert = false;
      }
      else {
        fputs(buffer[idx], stream);
      }
    }

    /* "BACKGROUND=" was not specified in config->resource */
    if (insert)
      fprintf(stream, "BACKGROUND=\"%s\"\n", pathname);

    fclose(stream);
  }

  /* Free the memory space allocated for config->resource */
  if (buffer) {
    for (idx=0; idx < count; idx++) free(buffer[idx]);
    free(buffer);
  }
  return true;
} /* </configsave> */

/*
* (private) set background callback
*/
static void
setbg (const gchar *pathname)
{
  GError *error = set_background (gdk_get_default_root_window(), pathname);
  if (error) g_printerr("%s: %s\n", Program, _(error->message));
} /* </setbg> */

/*
* setbg_settings_apply
* setbg_settings_cancel
* setbg_settings_close
*/
bool
setbg_settings_apply (Modulus *applet)
{
  WallpaperSettings *config = &config_;
  FileChooser *chooser  = config->chooser;
  const gchar *pathname = filechooser_get_selected_name (chooser);

  if (pathname) {	/* paranoid check */
    setbg (pathname);
    /* configsave (config); */
    strcpy(config->pathname, pathname);
  }
  return true;
} /* </setbg_settings_apply> */

bool
setbg_settings_cancel (Modulus *applet)
{
  WallpaperSettings *config = &config_;
  FileChooser *chooser = config->chooser;

  static gchar pathname[MAX_PATHNAME];
  const gchar *curdir = gtk_label_get_text (GTK_LABEL(chooser->path));
  gchar *scan;

  strcpy(pathname, config->pathname);
  scan = strrchr(pathname, '/');

  if (scan != NULL) {	/* paranoid check */
    *scan++ = (char)0;

    if (strcmp(curdir, pathname) != 0)
      filechooser_update (chooser, pathname, FALSE);

    filechooser_set_name (chooser, scan);
  }
  setbg_refresh (config->canvas, NULL, NULL);

  return true;
} /* </setbg_settings_cancel> */

bool
setbg_settings_close (Modulus *applet)
{
  GlobalPanel *panel = (GlobalPanel *)applet->data;

  settings_set_agents (panel->settings, NULL, NULL, NULL);
  configsave (&config_);

  return true;
} /* </setbg_settings_close> */

/*
* setbg_refresh view of the canvas area
*/
static void
setbg_refresh (GtkWidget *canvas, GdkEventExpose *ev, gpointer data)
{
  WallpaperSettings *config = &config_;
  gchar *name = filechooser_get_selected_name (config->chooser);

  if (name) {
    GError *error = NULL;
    GdkPixbuf *image = gdk_pixbuf_new_from_file (name, &error);

    if (image) {
      if (ev == NULL) {
        PanelSetting *settings = config->panel->settings;

        /* Register callbacks for apply and cancel. */
        settings_save_enable (settings, TRUE);
        settings_set_agents (settings,
                             (gpointer)setbg_settings_apply,
                             (gpointer)setbg_settings_cancel,
                             (gpointer)setbg_settings_close);
      }

      /* redraw_pixbuf() always clears the drawing area */
      redraw_pixbuf (canvas, image);
      g_object_unref (image);
    }
    else {
      g_printerr("%s: %s\n", Program, error->message);
      redraw_pixbuf (canvas, image = xpm_pixbuf(ICON_BROKEN, NULL));
      g_object_unref (image);
    }
  }
} /* </setbg_refresh> */

/*
* (private) setbg_selection callback function
*/
static bool
setbg_selection (FileChooserDatum *datum)
{
  const gchar *file = datum->file;
  struct stat info;

  if (lstat(file, &info) == 0 && S_ISREG(info.st_mode))
    setbg_refresh(config_.canvas, NULL, NULL);

  return true;
} /* </setbg_selection> */

/*
* (private) prevfile
* (private) nextfile
* (private) origfile
*/
static bool
prevfile(GtkWidget *button, FileChooser *chooser)
{
  int index = filechooser_get_cursor (chooser);

  if (index > 0) {
    filechooser_set_cursor (chooser, --index);
    setbg_refresh(config_.canvas, NULL, NULL);
  }
  return true;
} /* </prevfile> */

static bool
nextfile(GtkWidget *button, FileChooser *chooser)
{
  int index = filechooser_get_cursor (chooser);
  int count = filechooser_get_count (chooser);

  if (index < count) {
    filechooser_set_cursor (chooser, ++index);
    setbg_refresh(config_.canvas, NULL, NULL);
  }
  return true;
} /* </nextfile> */

static bool
origfile(GtkWidget *button, Modulus *applet)
{
  GlobalPanel *panel = applet->data;

  setbg_settings_cancel (applet);
  settings_save_enable (panel->settings, FALSE);

  return true;
} /* </origfile> */

/*
* setbg_initialize
*/
WallpaperSettings *
setbg_initialize (GlobalPanel *panel)
{
  static char path[MAX_PATHNAME];  /* dirname(1) of the BACKGROUND resource */

  WallpaperSettings *config = &config_;
  FileChooser *chooser;
  FILE *stream;

  char line[MAX_PATHNAME];
  char *resource, *scan;
  char *home = getenv("HOME");
  char *name = NULL;
  int error = 0;
  int mark;


  config->panel = panel;	/* save GlobalPanel *panel in singleton */

  /* $HOME/.config/desktop is the user specific resource file */
  sprintf(config->resource, "%s/.config/desktop", home);

  /* /etc/sysconfig/desktop is the system wide resource file */
  if (access(config->resource, R_OK) == 0) {
    resource = config->resource;
  }
  else {
    resource = "/etc/sysconfig/desktop";
    sprintf(line, "%s/.config", home);
    mkdir(line, 0755);
  }

   /* Consult the resource file. */
  if ((stream = fopen(resource, "r"))) {
    /* Search for "BACKGROUND=<file name>" in the resource file. */

    while (fgets(line, MAX_PATHNAME, stream)) {
      if (strncmp(line, "BACKGROUND=", 11) == 0) {
        if (line[11] == '"') {
          line[strlen(line) - 2] = (char)0;
          mark = 12;
        }
        else {
          line[strlen(line) - 1] = (char)0;
          mark = 11;
        }
        strcpy(path, &line[mark]);
	break;
      }
    }
    fclose(stream);
  }

  /* Fallback to the user home directory. */
  if (error || access(path, R_OK) != 0) {
    strcpy(path, home);
  }
  else if ( (scan = strrchr(path, '/')) ) {
    *scan = (char)0;
    name = ++scan;
  }

  /* Make sure we set (config->chooser)->agent */
  chooser = config->chooser = filechooser_new (path, NULL);
  filechooser_set_callback (chooser, (gpointer)setbg_selection, config);
  gtk_label_set_max_width_chars (GTK_LABEL(chooser->path), 32);
  chooser->clearname = TRUE;	/* clear file name on directory change */

  if (name) {
    filechooser_set_name (chooser, name);

    if (strcmp(path, "/") == 0)
      sprintf(config->pathname, "/%s", name);
    else
      sprintf(config->pathname, "%s/%s", path, name);
  }
  return config;
} /* </setbg_initialize> */

/*
* setbg_settings_new provides the configuration page
*/
GtkWidget*
setbg_settings_new (Modulus *applet, GlobalPanel *panel)
{
  WallpaperSettings *config = setbg_initialize (panel);
  FileChooser *chooser = config->chooser;

  GtkWidget *button, *field, *frame, *inset;
  GtkWidget *canvas, *pane, *scroll, *split;

  GtkWidget *area, *layout = gtk_vbox_new (FALSE, 0);
  gint width = 34 * gdk_screen_width() / 100;


  /* Split view: chooser on the left, preview on the right. */
  split = gtk_hbox_new(FALSE, 1);
  gtk_box_pack_start (GTK_BOX (layout), split, TRUE, TRUE, 0);
  gtk_widget_show (split);

  /* Assemble scrollable file chooser. */
  area = config->browser = gtk_vbox_new(FALSE, 1);
  gtk_box_pack_start (GTK_BOX (split), area, TRUE, TRUE, 0);
  gtk_widget_show (area);

  /* Assemble pane containing navigation and current directory. */
  pane = chooser->dirbox;
  gtk_box_pack_start (GTK_BOX (area), pane, FALSE, FALSE, 2);
  gtk_widget_show (pane);

  /* Assemble file selector. */
  scroll = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW(scroll),
                                  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_box_pack_start (GTK_BOX (area), scroll, TRUE, TRUE, 5);
  gtk_widget_show (scroll);

  area = chooser->viewer;
  gtk_container_add (GTK_CONTAINER(scroll), area);
  gtk_widget_show (area);

  /* Assemble canvas display area. */
  area = config->viewer = gtk_vbox_new(FALSE, 1);
  gtk_box_pack_start (GTK_BOX (split), area, TRUE, TRUE, 0);
  gtk_widget_show (area);

  /* Previous and next selection navigation. */
  pane = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (area), pane, FALSE, TRUE, 0);
  gtk_widget_show (pane);

  button = config->backward = xpm_button (ICON_BACK, NULL, 0, NULL);
  gtk_box_pack_start (GTK_BOX (pane), button, FALSE, TRUE, 0);
  gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
  g_signal_connect (G_OBJECT(button), "clicked", G_CALLBACK(prevfile), chooser);
  gtk_widget_show (button);

  button = config->forward = xpm_button (ICON_FORWARD, NULL, 0, NULL);
  gtk_box_pack_start (GTK_BOX (pane), button, FALSE, TRUE, 0);
  gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
  g_signal_connect (G_OBJECT(button), "clicked", G_CALLBACK(nextfile), chooser);
  gtk_widget_show (button);

  /* Image preview canvas drawing area. */
  frame = gtk_frame_new (NULL);
  gtk_container_add (GTK_CONTAINER(area), frame);
  gtk_widget_show (frame);

  canvas = config->canvas = gtk_drawing_area_new ();
  gtk_container_add (GTK_CONTAINER(frame), canvas);
  g_signal_connect (G_OBJECT (canvas), "expose_event",
                           G_CALLBACK (setbg_refresh), NULL);
  gtk_widget_show (canvas);

  /* Hide chooser->actuator used to open and close file browser. */
  gtk_widget_hide (chooser->actuator);

  /* Bottom layout area: display file name. */
  inset = gtk_hbox_new(FALSE, 2);
  gtk_box_pack_start (GTK_BOX(layout), inset, FALSE, TRUE, 0);
  gtk_widget_show (inset);

  button = gtk_label_new ("");
  gtk_box_pack_start (GTK_BOX(inset), button, FALSE, TRUE, 4);
  gtk_widget_show (button);

  button = xpm_button (ICON_FILE, NULL, 0, NULL);
  gtk_box_pack_start (GTK_BOX(inset), button, FALSE, TRUE, 0);
  gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
  g_signal_connect (G_OBJECT(button), "clicked", G_CALLBACK(origfile), applet);
  gtk_widget_show (button);

  area = chooser->namebox;
  gtk_box_pack_start (GTK_BOX(inset), area, FALSE, TRUE, 0);
  gtk_widget_show (area);

  /* Make chooser->name entry to display file name readonly. */
  field = chooser->name;
  gtk_entry_set_editable (GTK_ENTRY(field), FALSE);
  gtk_widget_set_usize (GTK_WIDGET(field), width, 0);

  return layout;
} /* </setbg_settings_new> */
