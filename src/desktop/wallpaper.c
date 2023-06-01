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

extern const char *Authors;	/* see, gpanel.c */
extern const char *Program;     /* see, gpanel.c */

/*
* Data structures used by this module.
*/
typedef struct _DesktopPanel DesktopPanel;

struct _DesktopPanel {
  FileChooser *chooser;	  /* file chooser instance */
  GlobalPanel *panel;

  GtkWidget *window;	  /* main window: paned top, controls bottom */
  GtkWidget *browser; 	  /* file browser left side panel */
  GtkWidget *viewer;	  /* preview image right side panel */
  GtkWidget *canvas;	  /* image canvas drawing area */

  GtkWidget *backward;    /* backward one selection button */
  GtkWidget *forward;     /* forward one selection button */

  char resource[MAX_PATHNAME]; /* resource path ($HOME/.config/desktop) */
  char pathname[MAX_PATHNAME];
};

/* Forward prototype declarations. */
static void
setbg_refresh (GtkWidget *canvas, GdkEventExpose *ev, gpointer data);

static DesktopPanel desktop_;	/* private global structure singleton */

/*
* (private) configsave
*/
static gboolean
configsave(DesktopPanel *desktop)
{
  FILE *stream;
  char **buffer = NULL;
  char line[MAX_PATHNAME];
  int count = 0;
  int idx;

  /* Read desktop->resource in memory buffer. */
  if ((stream = fopen(desktop->resource, "r"))) {
    while (fgets(line, MAX_PATHNAME, stream)) ++count;

    /* Allocate memory space for desktop->resource */
    if ((buffer = calloc(count, sizeof (char *))) == NULL) {
      fclose(stream);
      return FALSE;
    }
    fseek(stream, 0L, SEEK_SET);

    for (idx=0; idx < count; idx++) {
      if (fgets(line, MAX_PATHNAME, stream) <= 0) break;
      buffer[idx] = strdup(line);
    }
    fclose(stream);
  }

  /* [Re]open desktop->resource for writing. */
  if ((stream = fopen(desktop->resource, "w"))) {
    gboolean insert = TRUE;
    gchar *pathname = filechooser_get_selected_name (desktop->chooser);

    for (idx=0; idx < count; idx++) {
      if (strncmp(buffer[idx], "BACKGROUND=", 11) == 0) {
        fprintf(stream, "BACKGROUND=\"%s\"\n", pathname);
        insert = FALSE;
      }
      else {
        fputs(buffer[idx], stream);
      }
    }

    /* "BACKGROUND=" was not specified in desktop->resource */
    if (insert)
      fprintf(stream, "BACKGROUND=\"%s\"\n", pathname);

    fclose(stream);
  }

  /* Free the memory space allocated for desktop->resource */
  if (buffer) {
    for (idx=0; idx < count; idx++) free(buffer[idx]);
    free(buffer);
  }

  return TRUE;
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
gboolean
setbg_settings_apply (Modulus *applet)
{
  DesktopPanel *desktop = &desktop_;
  FileChooser *chooser  = desktop->chooser;
  const gchar *pathname = filechooser_get_selected_name (chooser);

  if (pathname) {	/* paranoid check */
    setbg (pathname);
    /* configsave (desktop); */
    strcpy(desktop->pathname, pathname);
  }
  return TRUE;
} /* </setbg_settings_apply> */

gboolean
setbg_settings_cancel (Modulus *applet)
{
  DesktopPanel *desktop = &desktop_;
  FileChooser *chooser = desktop->chooser;

  static gchar pathname[MAX_PATHNAME];
  const gchar *curdir = gtk_label_get_text (GTK_LABEL(chooser->path));
  gchar *scan;

  strcpy(pathname, desktop->pathname);
  scan = strrchr(pathname, '/');

  if (scan != NULL) {	/* paranoid check */
    *scan++ = (char)0;

    if (strcmp(curdir, pathname) != 0)
      filechooser_update (chooser, pathname, FALSE);

    filechooser_set_name (chooser, scan);
  }

  setbg_refresh (desktop->canvas, NULL, NULL);

  return TRUE;
} /* </setbg_settings_cancel> */

gboolean
setbg_settings_close (Modulus *applet)
{
  GlobalPanel *panel = (GlobalPanel *)applet->data;

  settings_set_agents (panel->settings, NULL, NULL, NULL);
  configsave (&desktop_);

  return TRUE;
} /* </setbg_settings_close> */

/*
* setbg_refresh view of the canvas area
*/
static void
setbg_refresh (GtkWidget *canvas, GdkEventExpose *ev, gpointer data)
{
  DesktopPanel *desktop = &desktop_;
  gchar *name = filechooser_get_selected_name (desktop->chooser);

  if (name) {
    GError *error = NULL;
    GdkPixbuf *image = gdk_pixbuf_new_from_file (name, &error);

    if (image) {
      if (ev == NULL) {
        PanelSetting *settings = desktop->panel->settings;

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
static gboolean
setbg_selection (FileChooserDatum *datum)
{
  const gchar *file = datum->file;
  struct stat info;

  if (lstat(file, &info) == 0 && S_ISREG(info.st_mode))
    setbg_refresh(desktop_.canvas, NULL, NULL);

  return TRUE;
} /* </setbg_selection> */

/*
* (private) prevfile
* (private) nextfile
* (private) origfile
*/
static gboolean
prevfile(GtkWidget *button, FileChooser *chooser)
{
  int index = filechooser_get_cursor (chooser);

  if (index > 0) {
    filechooser_set_cursor (chooser, --index);
    setbg_refresh(desktop_.canvas, NULL, NULL);
  }
  return TRUE;
} /* </prevfile> */

static gboolean
nextfile(GtkWidget *button, FileChooser *chooser)
{
  int index = filechooser_get_cursor (chooser);
  int count = filechooser_get_count (chooser);

  if (index < count) {
    filechooser_set_cursor (chooser, ++index);
    setbg_refresh(desktop_.canvas, NULL, NULL);
  }
  return TRUE;
} /* </nextfile> */

static gboolean
origfile(GtkWidget *button, Modulus *applet)
{
  GlobalPanel *panel = applet->data;

  setbg_settings_cancel (applet);
  settings_save_enable (panel->settings, FALSE);

  return TRUE;
} /* </origfile> */

/*
* setbg_initialize
*/
DesktopPanel *
setbg_initialize (GlobalPanel *panel)
{
  static char path[MAX_PATHNAME];  /* dirname(1) of the BACKGROUND resource */

  DesktopPanel *desktop = &desktop_;
  FileChooser *chooser;
  FILE *stream;

  char line[MAX_PATHNAME];
  char *resource, *scan;
  char *home = getenv("HOME");
  char *name = NULL;
  int error = 0;
  int mark;


  desktop->panel = panel;	/* save GlobalPanel *panel in singleton */

  /* $HOME/.config/desktop is the user specific resource file */
  sprintf(desktop->resource, "%s/.config/desktop", home);

  /* /etc/sysconfig/desktop is the system wide resource file */
  if (access(desktop->resource, R_OK) == 0) {
    resource = desktop->resource;
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

  /* Make sure we set (desktop->chooser)->agent */
  chooser = desktop->chooser = filechooser_new (path);
  filechooser_set_callback (chooser, (gpointer)setbg_selection, desktop);
  gtk_label_set_max_width_chars (GTK_LABEL(chooser->path), 32);
  chooser->clearname = TRUE;	/* clear file name on directory change */

  if (name) {
    filechooser_set_name (chooser, name);

    if (strcmp(path, "/") == 0)
      sprintf(desktop->pathname, "/%s", name);
    else
      sprintf(desktop->pathname, "%s/%s", path, name);
  }

  return desktop;
} /* </setbg_initialize> */

/*
* setbg_settings_new provides the configuration page
*/
GtkWidget*
setbg_settings_new (Modulus *applet, GlobalPanel *panel)
{
  DesktopPanel *desktop = setbg_initialize (panel);
  FileChooser *chooser = desktop->chooser;

  GtkWidget *layout = gtk_vbox_new (FALSE, 0);
  GtkWidget *area, *box, *button, *field, *inset;
  GtkWidget *canvas, *frame, *scroll, *split;

  gint width  = 34 * gdk_screen_width() / 100;


  /* Split view: preview on the left, file chooser on the right. */
  split = gtk_hbox_new(FALSE, 1);
  gtk_box_pack_start (GTK_BOX (layout), split, TRUE, TRUE, 0);
  gtk_widget_show (split);

  /* Assemble scrollable file chooser. */
  area = desktop->browser = gtk_vbox_new(FALSE, 1);
  gtk_box_pack_start (GTK_BOX (split), area, TRUE, TRUE, 0);
  gtk_widget_show (area);

  /* Assemble box containing navigation and current directory. */
  box = chooser->dirbox;
  gtk_box_pack_start (GTK_BOX (area), box, FALSE, FALSE, 2);
  gtk_widget_show (box);

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
  area = desktop->viewer = gtk_vbox_new(FALSE, 1);
  gtk_box_pack_start (GTK_BOX (split), area, TRUE, TRUE, 0);
  gtk_widget_show (area);

  /* Previous and next selection navigation. */
  box = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (area), box, FALSE, TRUE, 0);
  gtk_widget_show (box);

  button = desktop->backward = xpm_button(ICON_BACK, NULL);
  gtk_box_pack_start (GTK_BOX (box), button, FALSE, TRUE, 0);
  gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
  g_signal_connect (G_OBJECT(button), "clicked", G_CALLBACK(prevfile), chooser);
  gtk_widget_show (button);

  button = desktop->forward = xpm_button(ICON_FORWARD, NULL);
  gtk_box_pack_start (GTK_BOX (box), button, FALSE, TRUE, 0);
  gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
  g_signal_connect (G_OBJECT(button), "clicked", G_CALLBACK(nextfile), chooser);
  gtk_widget_show (button);

  /* Image preview canvas drawing area. */
  frame = gtk_frame_new (NULL);
  gtk_container_add (GTK_CONTAINER(area), frame);
  gtk_widget_show (frame);

  canvas = desktop->canvas = gtk_drawing_area_new ();
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

  button = xpm_button(ICON_FILE, NULL);
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
