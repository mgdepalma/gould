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

#include "gdisplay.h"

#ifndef get_current_dir_name
extern char *get_current_dir_name(void);
#endif

const char *Program = "gdisplay"; /* (public) published program name */
const char *Release = "1.2";	  /* (public) published program version */

const char *Description =
"is a gtk-based simple image viewing utility.\n"
"\n"
"  The image selected can be used to set the screen background and save\n"
"  the setting in $HOME/.config/desktop (used by Generations Linux).\n"
"\n"
"  The program is developed for Generations Linux and distributed\n"
"  under the terms and condition of the GNU Public License. It is\n"
"  part of gould (http://www.softcraft.org/gould).";

const char *Usage =
"usage: %s [-v | -h]\n"
"\n"
"\t-v print version information\n"
"\t-h print help usage (what you are reading)\n"
"\n";

static GlobalDisplay *global;	  /* (protected) encapsulated program data */
gboolean debug = FALSE;		  /* (protected) must be present */

/*
* (private) about
*/
static gboolean
about (GtkWidget *instance, GtkWidget *parent)
{
  notice(parent, ICON_SNAPSHOT, NULL, 0, "\n%s %s %s", Program, Release, Description);
  return FALSE;
} /* </about> */

/*
* clean application exit
*/
static void
finis (GtkWidget *instance, gpointer data)
{
  gtk_main_quit();
} /* </finis> */

/*
* refresh view of the canvas area
*/
static void
refresh (GtkWidget *canvas, gpointer data)
{
  GError    *error = NULL;
  GdkPixbuf *image = NULL;
  gchar     *name;

  if ( (name = filechooser_get_selected_name (global->chooser)) )
    image = gdk_pixbuf_new_from_file (name, &error);

  if (image) {
    /* redraw_pixbuf() always clears the drawing area */
    redraw_pixbuf (canvas, image);
    g_object_unref (image);
  }
  else {
    if (name) {
      redraw_pixbuf (canvas, image = xpm_pixbuf(ICON_BROKEN, NULL));
      g_object_unref (image);
    }
    if(error) g_printerr("%s: %s\n", Program, error->message);
  }
} /* </refresh> */

/*
* (private) agent callback function
*/
static gboolean
agent (FileChooserDatum *datum)
{
  const gchar *file = datum->file;
  struct stat info;

  if (lstat(file, &info) == 0 && S_ISREG(info.st_mode))
    refresh(global->canvas, NULL);

  return TRUE;
} /* </agent> */

/*
* (private) changemode callback function
*/
static gboolean
changemode(GtkWidget *button, gpointer data)
{
  if (global->filer) {		/* Toggle with and without global->browser */
    GError    *error = NULL;
    GdkPixbuf *image = NULL;
    gchar     *name;

    if ( (name = filechooser_get_selected_name (global->chooser)) )
      image = gdk_pixbuf_new_from_file (name, &error);

    if (image) {
      int width  = 640;		/* proportional width and height */
      int height = 560;

      g_object_unref (image);	/* image out of scope */

      gtk_button_set_image (GTK_BUTTON(button), xpm_image(ICON_FOLDER));
      gtk_widget_set_usize (global->window, width, height);
      gtk_widget_hide (global->browser);

      global->filer = FALSE;
    }
  }
  else {
    gtk_button_set_image (GTK_BUTTON(button), xpm_image(ICON_CHOOSER));
    gtk_widget_set_usize (global->window, global->width, global->height);
    gtk_widget_show (global->browser);

    global->filer = TRUE;
  }
  return TRUE;
} /* </changemode> */

/*
* (private) prevfile
* (private) nextfile
*/
static gboolean
prevfile(GtkWidget *button, FileChooser *chooser)
{
  int index = filechooser_get_cursor (chooser);

  if (index > 0) {
    filechooser_set_cursor (chooser, --index);
    refresh(global->canvas, NULL);
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
    refresh(global->canvas, NULL);
  }
  return TRUE;
} /* </nextfile> */

/*
* (private) configsave
*/
static gboolean
configsave(GtkWidget *widget, gpointer data)
{
  FILE *stream;
  char **buffer = NULL;
  char line[FILENAME_MAX];
  int count = 0;
  int idx;

  /* Read global->resource in memory buffer. */
  if ((stream = fopen(global->resource, "r"))) {
    while (fgets(line, FILENAME_MAX, stream)) ++count;

    /* Allocate memory space for global->resource */
    buffer = malloc(count * sizeof (char *));
    fseek(stream, 0L, SEEK_SET);

    for (idx=0; idx < count; idx++) {
      if (fgets(line, FILENAME_MAX, stream) <= 0) break;
      buffer[idx] = strdup(line);
    }
    fclose(stream);
  }

  /* [Re]open global->resource for writing. */
  if ((stream = fopen(global->resource, "w"))) {
    gboolean insert = TRUE;
    gchar *pathname = filechooser_get_selected_name (global->chooser);

    for (idx=0; idx < count; idx++) {
      if (strncmp(buffer[idx], "BACKGROUND=", 11) == 0) {
        fprintf(stream, "BACKGROUND=\"%s\"\n", pathname);
        insert = FALSE;
      }
      else {
        fputs(buffer[idx], stream);
      }
    }
    /* "BACKGROUND=" was not specified in global->resource */
    if (insert) fprintf(stream, "BACKGROUND=\"%s\"\n", pathname);
    fclose(stream);
  }

  /* Free the memory space allocated for global->resource */
  if (buffer) {
    for (idx=0; idx < count; idx++) free(buffer[idx]);
    free(buffer);
  }
  return TRUE;
} /* </configsave> */

/*
* (private) set background callback
*/
static gboolean
setbg (GtkWidget *widget, gpointer data)
{
  gchar *pathname = filechooser_get_selected_name (global->chooser);
  gboolean result = FALSE;

  if (pathname) {
    GError *error = set_background (gdk_get_default_root_window(), pathname);

    if (error)
      g_printerr("%s: %s\n", Program, _(error->message));
    else
      result = configsave(widget, data);
  }
  return result;
} /* </setbg> */

/*
* (private) update_chooser
*/
static gboolean
update_chooser (GlobalDisplay *display)
{
  FileChooser *chooser = display->chooser;
  const gchar *curdir = gtk_label_get_text (GTK_LABEL(chooser->path));

  filechooser_update (chooser, curdir, FALSE);

  return TRUE;
} /* </update_chooser> */

/*
* interface - construct user interface
*/
static GtkWidget*
interface (GtkWidget *window)
{
  FileChooser *chooser = global->chooser;

  GtkWidget *layout = gtk_vbox_new (FALSE, 0);
  GtkWidget *area, *box, *button, *field, *inset;
  GtkWidget *canvas, *frame, *scroll, *split;


  /* Split view: file chooser on the left and preview on the right. */
  split = gtk_hbox_new(FALSE, 1);
  gtk_box_pack_start(GTK_BOX (layout), split, TRUE, TRUE, 0);
  gtk_widget_show(split);

  /* Assemble scrollable file chooser. */
  area = global->browser = gtk_vbox_new(FALSE, 1);
  gtk_box_pack_start(GTK_BOX (split), area, TRUE, TRUE, 0);
  gtk_widget_show(area);

  /* Assemble box containing navigation and current directory. */
  box = chooser->dirbox;
  gtk_box_pack_start(GTK_BOX (area), box, FALSE, FALSE, 2);
  gtk_widget_show(box);

  /* Assemble file selector. */
  scroll = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
                                 GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_box_pack_start(GTK_BOX (area), scroll, TRUE, TRUE, 5);
  gtk_widget_show(scroll);

  area = chooser->viewer;
  gtk_container_add (GTK_CONTAINER(scroll), area);
  gtk_widget_show(area);

  /* Neutral zone between global->browser and image viewer */
  inset = gtk_label_new ("");
  gtk_box_pack_start(GTK_BOX (split), inset, FALSE, FALSE, 4);
  gtk_widget_show(inset);

  /* Assemble display canvas area. */
  area = global->viewer = gtk_vbox_new(FALSE, 1);
  gtk_box_pack_start(GTK_BOX (split), area, TRUE, TRUE, 0);
  gtk_widget_show(area);

  /* Previous and next selection navigation. */
  box = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX (area), box, FALSE, TRUE, 0);
  gtk_widget_show(box);

  button = global->backward = xpm_button(ICON_BACK, NULL, 0, NULL);
  gtk_box_pack_start(GTK_BOX (box), button, FALSE, TRUE, 0);
  gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
  g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(prevfile), chooser);
  gtk_widget_show(button);

  button = global->forward = xpm_button(ICON_FORWARD, NULL, 0, NULL);
  gtk_box_pack_start(GTK_BOX (box), button, FALSE, TRUE, 0);
  gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
  g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(nextfile), chooser);
  gtk_widget_show(button);

  /* Program information (about) button. */
  button = xpm_button(ICON_HELP, NULL, 0, _("Information"));
  gtk_box_pack_end (GTK_BOX(box), button, FALSE, TRUE, 2);
  gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
  g_signal_connect (G_OBJECT(button), "clicked", G_CALLBACK(about), window);
  gtk_widget_show (button);

  /* Image preview canvas drawing area. */
  frame = gtk_frame_new (NULL);
  gtk_container_add (GTK_CONTAINER(area), frame);
  gtk_widget_show (frame);

  canvas = global->canvas = gtk_drawing_area_new ();
  gtk_container_add (GTK_CONTAINER(frame), canvas);
  /* gtk_box_pack_start (GTK_BOX(area), canvas, TRUE, TRUE, 5); */

  g_signal_connect (G_OBJECT (canvas), "expose_event",
                           G_CALLBACK (refresh), NULL);
  gtk_widget_show (canvas);

  /* Bottom layout area: display file name and control buttons */
  area = chooser->namebox;
  gtk_box_pack_start(GTK_BOX(layout), area, FALSE, TRUE, 0);
  gtk_widget_show(area);

  /* Neutral zone between global->browser and global->window right border */
  inset = gtk_label_new ("");
  gtk_box_pack_start(GTK_BOX (split), inset, FALSE, FALSE, 4);
  gtk_widget_show(inset);

  /* (global->chooser)->actuator to open and close global->browser */
  button = chooser->actuator;
  gtk_button_set_image (GTK_BUTTON(button), xpm_image(ICON_CHOOSER));
  g_signal_connect(G_OBJECT (button), "clicked", G_CALLBACK(changemode), NULL);

  /* (global->chooser)->name readonly entry to display file name */
  field = chooser->name;
  gtk_entry_set_editable (GTK_ENTRY(field), FALSE);
  gtk_widget_set_usize(GTK_WIDGET(field), global->width/3, 0);

  /* command bar */
  box = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_end(GTK_BOX (area), box, FALSE, FALSE, 0);
  gtk_widget_show(box);

  /* Set as background button. */
  button = xpm_button(ICON_WALLPAPER, NULL, 0, _("Set as background"));
  g_signal_connect (G_OBJECT(button), "clicked", G_CALLBACK(setbg), NULL);
  gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
  gtk_box_pack_start (GTK_BOX(box), button, FALSE, TRUE, 20);
  gtk_widget_show(button);

  /* CLOSE button */
  button = xpm_button(ICON_CLOSE, NULL, 0, _("Close"));
  g_signal_connect (G_OBJECT(button), "clicked", G_CALLBACK(finis), NULL);
  gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
  gtk_box_pack_start(GTK_BOX (box), button, FALSE, FALSE, 0);
  gtk_widget_show(button);

  return layout;
} /* </interface> */

/*
* initialize
*/
static void
initialize (GlobalDisplay *display, char *pathname)
{
  static char path[FILENAME_MAX];  /* dirname(1) of the BACKGROUND resource */

  FileChooser *chooser;
  FILE *stream;

  char line[FILENAME_MAX];
  char *resource, *scan;
  char *home = getenv("HOME");
  char *name = NULL;
  int error = 0;
  int mark;


  display->filer = TRUE;	/* controls showing the file chooser */

  display->height = (3 * gdk_screen_height ()) / 8;
  display->width  = (4 * green_screen_width ())  / 9;

  /* $HOME/.config/desktop is the user specific resource file */
  sprintf(display->resource, "%s/.config/desktop", home);

  /* /etc/sysconfig/desktop is the system wide resource file */
  if (access(display->resource, R_OK) == 0) {
    resource = display->resource;
  }
  else {
    resource = "/etc/sysconfig/desktop";
    sprintf(line, "%s/.config", home);
    mkdir(line, 0755);
  }

  /* Program called with a directory or an image file name, or
   * only options in which case we consult the resource file.
   */
  if (pathname) {
    if (access(pathname, R_OK) != 0) {
      fprintf(stderr, "%s: no such file or directory\n", pathname);
      error = ENOENT;
    }
    else {
      if (pathname[0] == '/')
        sprintf(path, "%s", pathname);
      else
        sprintf(path, "%s/%s", get_current_dir_name(), pathname);
    }
  }
  else if ((stream = fopen(resource, "r"))) {
    /* Search for "BACKGROUND=<file name>" in the resource file. */

    while (fgets(line, FILENAME_MAX, stream)) {
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

  /* fallback to the user home directory */
  if (error || access(path, R_OK) != 0) {
    strcpy(path, home);
  }
  else if ( (scan = strrchr(path, '/')) ) {
    *scan = (char)0;
    name = ++scan;
  }

  /* Make sure we set (display->chooser)->agent */
  chooser = display->chooser = filechooser_new (path);
  filechooser_set_callback (chooser, (gpointer)agent, display);
  gtk_label_set_max_width_chars (GTK_LABEL(chooser->path), 32);

  if (name) filechooser_set_name (chooser, name);
  chooser->clearname = TRUE;	/* clear file name on directory change */

  global = display;  /* global program data structure */
} /* </initialize> */

/*
* main - gdisplay main program
*/
int
main(int argc, char *argv[])
{
  struct _GlobalDisplay memory;	/* see, gdisplay.h */

  GtkWidget *window;		/* main window     */
  GtkWidget *layout;		/* user interface  */

  int opt;
  /* disable invalid option messages */
  opterr = 0;

  while ((opt = getopt (argc, argv, "hv")) != -1) {
    switch (opt) {
      case 'h':
        printf(Usage, Program);
        return EX_OK;
        break;

      case 'v':
        printf("<!-- %s %s %s\n -->\n", Program, Release, Description);
        return EX_OK;

      default:
        printf("%s: invalid option, use -h for help usage.\n", Program);
        return EX_USAGE;
    }
  }

#ifdef GETTEXT_PACKAGE
  bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);
  gtk_disable_setlocale();
#endif

  gtk_init (&argc, &argv);	/* initialization of the GTK */
  gtk_set_locale ();

  /* Initialize program data structure */
  initialize (&memory, (argc > 1) ? argv[1] : NULL);

  /* Create a new top level window */
  window = global->window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

  /* Set title and border width of the main window */
  gtk_window_set_title (GTK_WINDOW (window), Program);
  gtk_window_set_icon (GTK_WINDOW (window), xpm_pixbuf(ICON_CHOOSER, NULL));
  gtk_window_set_position (GTK_WINDOW (window), GTK_WIN_POS_CENTER);
  gtk_widget_set_usize (window, global->width, global->height);

  //gtk_window_set_decorated (GTK_WINDOW (window), FALSE);
  gtk_widget_show (window);
  //gdk_window_set_decorations(window->window, GDK_DECOR_BORDER);

  /* Construct the user interface */
  layout = interface (window);
  gtk_container_add (GTK_CONTAINER(window), layout);
  gtk_widget_show (layout);

  /* Set event handlers for window close/destroy*/
  g_signal_connect (G_OBJECT(window), "destroy", G_CALLBACK(finis), NULL);
  g_signal_connect (G_OBJECT(window), "delete_event", G_CALLBACK(finis), NULL);

  /* See if we were called with a file to display */
  if (argc > 1 && access(argv[1], R_OK) == 0)
    changemode ((global->chooser)->actuator, NULL);

  /* Update global->chooser every 20 seconds */
  g_timeout_add (1000 * 20, (gpointer)update_chooser, global);

  /* Main event loop */
  gtk_main ();
    
  return Success;
} /* </gdisplay> */
