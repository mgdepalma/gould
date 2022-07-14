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

#include "util.h"
#include "gsnapshot.h"

static GlobalSnapshot *global;	/* (protected) encapsulated program data */
unsigned short debug = 0;	/* (protected) must be present */

const char *Program;		/* (public) published program name */
const char *Release;		/* (public) published program version */
const char *Information =
"is a gtk-based utility which captures screen contents.\n"
"One can select to capture the entire screen, a window or a rectangle.\n"
"The snapshot can be saved to a file and/or printed.\n"
"\n"
"The program is developed for Generations Linux and distributed\n"
"under the terms and condition of the GNU Public License.\n";


/*
 * (private) about
 */
static gboolean
about(GtkWidget *widget, GtkWidget *parent)
{
  notice(parent, ICON_SNAPSHOT, "\n%s %s %s", Program, Release, Information);
  return FALSE;
} /* </about> */

/*
 * clean application exit
 */
static gboolean
finis(GtkWidget *widget, gpointer data)
{
  gtk_main_quit ();
  return FALSE;
} /* </finis> */

/*
 * refresh view of the canvas area
 */
static void
refresh(GtkWidget *canvas, gpointer data)
{
  GdkPixbuf *image = global->image;

  if (image) {
    redraw_pixbuf (canvas, image);
  }
} /* </refresh> */

/*
 * callback to print image
 */
static gboolean
printimage (gpointer data)
{
  FILE *out;		/* either a file or pipe to print->command */
  PrintDialog *print = (PrintDialog *)data;
  gboolean status = TRUE;
  gchar message[MAX_BUFFER_SIZE];

  if (print->device == PRINT_FILE) {
    const gchar *outfile = filechooser_get_selected_name (print->chooser);

    if (access(outfile, F_OK) == 0) {  /* file already exists */
      if (overwrite(print->dialog, outfile) != GTK_RESPONSE_OK)
        status = FALSE;
    }

    if (status) {
      out = fopen(outfile, "w");
      print_pixbuf (out, global->image, print->paper);
      fclose(out);

      sprintf(message, "%s: PostScript", outfile);
      notice(print->dialog, ICON_SNAPSHOT, message);
    }
  }
  else {
    const gchar *command = gtk_entry_get_text (GTK_ENTRY(print->command));

    if (strlen(command) < 2 || strncmp(command, "lp", 2) != 0) {
      notice(print->dialog, ICON_ERROR, _("Invalid print command"));
    }
    else {
      out = popen(command, "w");
      print_pixbuf (out, global->image, print->paper);
      pclose(out);

      sprintf(message, "%s: %s", _("Job sent to the printer"), command);
      notice(print->dialog, ICON_INFO, message);
    }
  }
  return status;
} /* </printimage> */

/*
 * write out snapshot to a file (bmp, jpeg, png)
 */
static gboolean
savefile(gpointer data)
{
  SaveDialog *save = (SaveDialog *)data;
  gboolean status = TRUE;

  const gchar *format = get_file_extension(save->imgtype);
  const gchar *filename = filechooser_get_selected_name (save->chooser);

  if (access(filename, F_OK) != 0)  /* file does not exist */
    gdk_pixbuf_save(global->image, filename, format, NULL, NULL);
  else {
    if (overwrite(save->dialog, filename) == GTK_RESPONSE_OK)
      gdk_pixbuf_save(global->image, filename, format, NULL, NULL);
    else
      status = FALSE;
  }
  return status;
} /* </savefile> */

/*
 * screenshot callback
 */
static void
screenshot(GtkWidget *widget, gpointer data)
{
  gint xpos, ypos;
  GtkWidget *window = global->window;
  gint mode = gtk_combo_box_get_active (GTK_COMBO_BOX(global->modes));

  /* Remember the main window position and hide */
  if (global->hide) {
    gtk_window_get_position (GTK_WINDOW(window), &xpos, &ypos);
    gtk_widget_hide (window);
    gdk_display_sync (gtk_widget_get_display (window));
  }

  /* Capture in the specified mode and store image in global->image */
  if (global->delay > 0) grab_pointer_sleep (global->delay);
  global->image = capture(mode, global->decorations);
  if (data) refresh(global->viewer, NULL);

  /* Restore the main window original position */
  if (global->hide) {
    gtk_window_move (GTK_WINDOW(window), xpos, ypos);
    gtk_widget_show (window);
  }
} /* </screenshot> */

/*
 * save_as callback
 */
static void
save_as(GtkWidget *widget, gpointer data)
{
  SaveDialog *save = global->save;
  gtk_widget_show (save->dialog);
} /* </save_as> */

/*
 * print to file callback
 */
static void printofile(GtkWidget *widget, gpointer data)
{
  PrintDialog *print = global->print;
  gtk_widget_show (print->dialog);
} /* </printofile> */

/* 
 * (private) set_snapshot_delay
 * (private) set_window_decorations
 * (private) set_application_hide
 */
static void set_snapshot_delay(GtkAdjustment *adjust)
{
  global->delay = (unsigned int)adjust->value;
} /* </set_snapshot_delay> */

static void
set_window_decorations(GtkToggleButton *button)
{
  global->decorations = button->active;
} /* </set_window_decorations> */

static void
set_application_hide(GtkToggleButton *button)
{
  global->hide = button->active;
} /* </set_application_hide> */

/*
 * construct_viewer - construct view and control area
 */
static void
construct_viewer(GtkWidget *layout, GtkWidget *window)
{
  GtkWidget *button;
  GtkWidget *canvas;
  GtkWidget *hbox;
  GtkWidget *vbox;
  GtkWidget *box;
  gchar *label;

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (layout), hbox, FALSE, TRUE, 0);
  gtk_widget_show (hbox);

  /* Assemble display canvas area */
  canvas = global->viewer = gtk_drawing_area_new ();
  gtk_widget_set_size_request (GTK_WIDGET (canvas), VIEW_WIDTH, VIEW_HEIGHT);
  gtk_box_pack_start (GTK_BOX (hbox), canvas, TRUE, TRUE, 0);
  gtk_widget_show (canvas);

  /* Signals used to handle backing pixmap */
  g_signal_connect (G_OBJECT (canvas), "expose_event",
		    G_CALLBACK (refresh), NULL);

  /* Assemble vbox for CAPTURE, SAVE, PRINT buttons */
  vbox = gtk_vbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (hbox), vbox, FALSE, TRUE, BOX_SPACING);
  gtk_widget_show (vbox);

  button = xpm_button(ICON_CAMERA, _("New Snapshot"));
  gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, TRUE, 0);
  g_signal_connect_swapped (G_OBJECT (button), "clicked",
                            G_CALLBACK (screenshot),
                            G_OBJECT (window));
  gtk_widget_show (button);

  box = gtk_vbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (vbox), box, FALSE, TRUE, 20);
  gtk_widget_show (box);

  label = g_strdup_printf("%s...", _("Save As"));
  button = xpm_button(ICON_SAVE_AS, label);
  gtk_box_pack_start (GTK_BOX (box), button, FALSE, TRUE, 0);
  g_signal_connect_swapped (G_OBJECT (button), "clicked",
                            G_CALLBACK (save_as),
                            G_OBJECT (window));
  gtk_widget_show (button);
  g_free(label);

  label = g_strdup_printf("%s...", _("Print"));
  button = xpm_button(ICON_PRINT, label);
  gtk_box_pack_start (GTK_BOX (box), button, TRUE, TRUE, 0);
  g_signal_connect_swapped (G_OBJECT (button), "clicked",
                            G_CALLBACK (printofile),
                            G_OBJECT (window));
  gtk_widget_show (button);
  g_free(label);
} /* </construct_viewer> */

/*
 * construct_modes - construct modes area
 */
static void
construct_modes(GtkWidget *layout, GtkWidget *window)
{
  GtkWidget *nobs;    /* widgets for mode, delay and decorations */

  GtkObject *adjust;
  GtkWidget *box, *button, *label, *options, *scale;


  /* Assemble container
  */
  nobs = gtk_vbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (layout), nobs, FALSE, TRUE, 0);
  gtk_widget_show (nobs);

  /* Assemble options menu for capture mode
  */
  box = gtk_hbox_new (FALSE, BORDER_WIDTH);
  gtk_container_set_border_width (GTK_CONTAINER (box), BORDER_WIDTH);
  gtk_box_pack_start (GTK_BOX (nobs), box, FALSE, TRUE, 0);
  gtk_widget_show (box);

  label = gtk_label_new (_("Capture mode"));
  gtk_box_pack_start (GTK_BOX (box), label, FALSE, FALSE, 0);
  gtk_widget_show (label);

  options = global->modes = gtk_combo_box_new_text ();
  gtk_box_pack_start (GTK_BOX (box), options, TRUE, TRUE, 0);

  gtk_combo_box_append_text(GTK_COMBO_BOX(options), _("Entire Screen"));
  gtk_combo_box_append_text(GTK_COMBO_BOX(options), _("Window under cursor"));
  gtk_combo_box_append_text(GTK_COMBO_BOX(options), _("Region"));

  gtk_combo_box_set_active(GTK_COMBO_BOX(options), GRAB_SCREEN);
  gtk_widget_show (options);

  /* Assemble slider to control snapshot delay
  */
  box = gtk_hbox_new (FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (box), 0);
  gtk_box_pack_start (GTK_BOX (nobs), box, FALSE, TRUE, 0);
  gtk_widget_show (box);

  label = gtk_label_new (_("Snapshot delay"));
  gtk_box_pack_start (GTK_BOX (box), label, FALSE, FALSE, 0);
  gtk_widget_show (label);

  adjust = gtk_adjustment_new (0.0, 0.0, 20.0, 1.0, 1.0, 0.0);
  g_signal_connect (G_OBJECT (adjust), "value_changed",
                    G_CALLBACK (set_snapshot_delay), NULL);

  scale = gtk_hscale_new (GTK_ADJUSTMENT (adjust));
  gtk_scale_set_digits (GTK_SCALE (scale), 0);
  gtk_box_pack_start (GTK_BOX (box), scale, TRUE, TRUE, 0);
  gtk_widget_show (scale);

  /* Assemble checkbutton to control including window decorations */
  box = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (nobs), box, TRUE, TRUE, 0);
  gtk_widget_show (box);

  button = gtk_check_button_new_with_label(_("Include window decorations"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button),
                                global->decorations);

  g_signal_connect (G_OBJECT (button), "toggled",
                    G_CALLBACK (set_window_decorations), NULL);

  gtk_box_pack_start (GTK_BOX (box), button, TRUE, TRUE, 0);
  gtk_widget_show (button);

  /* Assemble checkbutton to control hiding application window */
  button = gtk_check_button_new_with_label(_("Hide application window"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button),
                                global->hide);

  g_signal_connect (G_OBJECT (button), "toggled",
                    G_CALLBACK (set_application_hide), NULL);

  gtk_box_pack_start (GTK_BOX (box), button, TRUE, TRUE, 0);
  gtk_widget_show (button);
} /* </construct_modes> */

/*
 * (private) construct_trail - construct trail area
 */
static void
construct_trail(GtkWidget *layout, GtkWidget *window)
{
  GtkWidget *info;   /* about program icon button */
  GtkWidget *quit;   /* programme clean exit button */
  GtkWidget *trail;  /* trailer hbox (info, quit)   */

  trail = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (layout), trail, FALSE, TRUE, 4);
  gtk_widget_show (trail);

  /* Add the about program icon button. */
  info = xpm_button(ICON_HELP, _("Information"));
  gtk_box_pack_start (GTK_BOX (trail), info, FALSE, FALSE, 5);
  gtk_button_set_relief (GTK_BUTTON (info), GTK_RELIEF_NONE);
  g_signal_connect (G_OBJECT(info), "clicked", G_CALLBACK(about), window);
  gtk_widget_show (info);

  /* Assemble the quit button.. we are done! */
  quit = xpm_button(ICON_CLOSE, _("Close"));
  gtk_box_pack_end (GTK_BOX (trail), quit, FALSE, TRUE, 9);
  /* gtk_button_set_relief (GTK_BUTTON (quit), GTK_RELIEF_NONE); */
  g_signal_connect (G_OBJECT(quit), "clicked", G_CALLBACK(finis), NULL);

  GTK_WIDGET_SET_FLAGS (quit, GTK_CAN_DEFAULT);
  gtk_widget_grab_default (quit);

  gtk_widget_show (quit);
} /* </construct_trail> */

/*
 * interface - construct user interface
 */
GtkWidget *
interface(GtkWidget *window)
{
  GtkWidget *layout;                   /* Interface main layout */

  layout = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (window), layout);
  gtk_widget_show (layout);

  /* Assemble viewer area */
  construct_viewer(layout, window);
  add_vertical_space(layout, BOX_SPACING);
  add_horizontal_separator(layout);

  /* Assemble modes area */
  construct_modes(layout, window);
  add_horizontal_separator(layout);

  /* Assemble trail area */
  construct_trail(layout, window);

  return layout;
} /* </interface> */

/*
 * main - main program implementation
 */
int
main(int argc, char *argv[])
{
  GtkWidget *window;        /* GtkWidget is the storage type for widgets */
  struct _GlobalSnapshot memory;

  Program = basename(argv[0]);
  Release = "1.0.2";

#ifdef GETTEXT_PACKAGE
  bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);
  gtk_disable_setlocale();
#endif

  /* Initialization of the GTK */
  gtk_init (&argc, &argv);
  gtk_set_locale ();

  /* Create a new top level window */
  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

  /* Initialize program data structure */
  global = &memory;

  global->decorations = FALSE;
  global->delay       = 0;
  global->hide        = TRUE;
  global->image       = NULL;
  global->window      = window;
  global->layout      = interface (window);
  global->print       = printdialog_new (window, printimage);
  global->save        = savedialog_new (window, savefile);

  /* Set title and border width of the main window */
  gtk_window_set_title (GTK_WINDOW (window), Program);
  gtk_window_set_icon (GTK_WINDOW (window), xpm_pixbuf(ICON_TITLE, NULL));
  gtk_container_set_border_width (GTK_CONTAINER (window), BORDER_WIDTH);
  gtk_window_set_position (GTK_WINDOW (window), GTK_WIN_POS_CENTER);
  gtk_window_set_resizable (GTK_WINDOW(window), FALSE);

  /* Set event handlers for window close/destroy */
  g_signal_connect (G_OBJECT(window), "destroy", G_CALLBACK(finis), NULL);
  g_signal_connect (G_OBJECT(window), "delete_event", G_CALLBACK(finis), NULL);

  /* Realize the visual components */
  screenshot(window, NULL);  /* Initialize global->image with screenshot */
  gtk_widget_show (window);

  /* Main event loop */
  gtk_main ();
    
  return Success;
} /* </gsnapshot> */
