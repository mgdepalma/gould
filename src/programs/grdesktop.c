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

#include "gould.h"	/* common package declarations */
#include "grdesktop.h"
#include "util.h"

static GlobalRemote *global;	/* (protected) encapsulated program data */

const char *Program;		/* (public) published program name */
const char *Release;		/* (public) published program version */
const char *Information =
"is a gtk-based frontend for the rdesktop program.\n"
"\n"
"The program is developed for Generations Linux and distributed\n"
"under the terms and condition of the GNU Public License.\n";

debug_t debug = 0;	/* debug verbosity (0 => none) {must be declared} */


/*
 * (private) about
 */
static gboolean
about (GtkWidget *widget, GtkWidget *parent)
{
  notice(parent, ICON_SNAPSHOT, NULL, 0, "\n%s %s %s", Program, Release, Information);
  return FALSE;
} /* </about> */

/*
 * clean application exit
 */
static gboolean
finis (GtkWidget *widget, gpointer data)
{
  gtk_main_quit ();
  return FALSE;
} /* </finis> */

/*
 * invoke the rdesktop program
 */
static gboolean
rdesktop (GtkWidget *widget, gpointer data)
{
  GtkWidget *window = global->window;

  const gchar *username = gtk_entry_get_text (GTK_ENTRY(global->username));
  const gchar *server = gtk_entry_get_text (GTK_ENTRY(global->server));
  const gchar *domain = gtk_entry_get_text (GTK_ENTRY(global->domain));
  gchar command[FILENAME_MAX];

  if (strlen(server) == 0) {
    notice (global->window, ICON_BULB, NULL, 0, _("Please specify where to Log on to."));
    return FALSE;
  }

  if (global->fullscreen)
    strcpy(command, "rdesktop -f");
  else
    sprintf(command, "rdesktop -g %s",
            gtk_combo_box_get_active_text (GTK_COMBO_BOX(global->resolution)));

  if (strlen(username) > 0) {
    strcat(command, " -u ");
    strcat(command, username);
  }

  if (strlen(domain) > 0) {
    strcat(command, " -d ");
    strcat(command, domain);
  }

  strcat(command, " ");
  strcat(command, server);
  strcat(command, " >/dev/null 2>&1");

  gtk_widget_hide (window);
  gdk_display_sync (gtk_widget_get_display (window));

  system (command);
  gtk_widget_show (window);

  return FALSE;
} /* </rdesktop> */

/*
 * set_fullscreen_mode
 */
static void
set_fullscreen_mode (GtkToggleButton *button, gpointer data)
{
  global->fullscreen = button->active;
} /* </set_fullscreen_mode> */

/*
 * interface - construct user interface
 */
GtkWidget *
interface(GtkWidget *window)
{
  GtkWidget *area, *frame, *field, *entry, *info, *item, *options, *quit;
  GtkWidget *layout;			/* main interface layout */
  gchar *label;

  layout = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (window), layout);
  gtk_widget_show (layout);

  /* Add the XPM image for this program. */
  info = xpm_image (ICON_RDESKTOP);
  gtk_box_pack_start (GTK_BOX (layout), info, FALSE, FALSE, 0);
  gtk_widget_show (info);

  /* Add the user input area in a frame. */
  frame = gtk_frame_new (NULL);
  gtk_box_pack_start (GTK_BOX (layout), frame, FALSE, FALSE, 0);
  gtk_widget_show (frame);

  area = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER(frame), area);
  gtk_widget_show (area);

  item = gtk_label_new ("");
  gtk_box_pack_start (GTK_BOX (area), item, FALSE, FALSE, 0);
  gtk_widget_show (item);

  /* Add input field for username. */
  field = gtk_hbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER(area), field);
  gtk_widget_show (field);

  info = gtk_label_new (label = g_strdup_printf ("%s:", _("User name")));
  gtk_box_pack_start (GTK_BOX (field), info, FALSE, TRUE, 53 - strlen(label));
  gtk_widget_show (info);

  entry = global->username = gtk_entry_new ();
  gtk_entry_set_text (GTK_ENTRY(entry), getenv("USER"));
  gtk_container_add (GTK_CONTAINER(field), entry);
  gtk_widget_show (entry);

  item = gtk_label_new ("");
  gtk_box_pack_end (GTK_BOX (field), item, FALSE, FALSE, 5);
  gtk_widget_show (item);

  /* Add input field for server. */
  field = gtk_hbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER(area), field);
  gtk_widget_show (field);

  info = gtk_label_new (label = g_strdup_printf ("%s:", _("Log on to")));
  gtk_box_pack_start (GTK_BOX (field), info, FALSE, TRUE, 57 - strlen(label));
  gtk_widget_show (info);

  entry = global->server = gtk_entry_new ();
  gtk_container_add (GTK_CONTAINER(field), entry);
  gtk_widget_show (entry);

  item = gtk_label_new ("");
  gtk_box_pack_end (GTK_BOX (field), item, FALSE, FALSE, 5);
  gtk_widget_show (item);

  /* Add input field for domain. */
  field = gtk_hbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER(area), field);
  gtk_widget_show (field);

  info = gtk_label_new (label = g_strdup_printf ("%s:", _("Domain")));
  gtk_box_pack_start (GTK_BOX (field), info, FALSE, TRUE, 58 - strlen(label));
  gtk_widget_show (info);

  entry = global->domain = gtk_entry_new ();
  gtk_container_add (GTK_CONTAINER(field), entry);
  gtk_widget_show (entry);

  item = gtk_label_new ("");
  gtk_box_pack_end (GTK_BOX (field), item, FALSE, FALSE, 5);
  gtk_widget_show (item);

  /* Add resolution selection. */
  field = gtk_hbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER(area), field);
  gtk_widget_show (field);

  info = gtk_label_new (label = g_strdup_printf ("%s:", _("Resolution")));
  gtk_box_pack_start (GTK_BOX (field), info, FALSE, TRUE, 54 - strlen(label));
  gtk_widget_show (info);

  options = global->resolution = gtk_combo_box_new_text();
  gtk_box_pack_start (GTK_BOX (field), options, TRUE, TRUE, 0);

  gtk_combo_box_append_text(GTK_COMBO_BOX(options), "640x480");
  gtk_combo_box_append_text(GTK_COMBO_BOX(options), "800x600");
  gtk_combo_box_append_text(GTK_COMBO_BOX(options), "1024x768");
  gtk_combo_box_append_text(GTK_COMBO_BOX(options), "1280x1024");

  gtk_combo_box_set_active(GTK_COMBO_BOX(options), 2);
  gtk_widget_show (options);

  item = gtk_check_button_new_with_label (_("Fullscreen"));
  gtk_box_pack_start (GTK_BOX (field), item, FALSE, FALSE, 4);
  gtk_widget_show (item);

  g_signal_connect (G_OBJECT (item), "toggled",
                    G_CALLBACK (set_fullscreen_mode), NULL);

  item = gtk_label_new ("");
  gtk_box_pack_end (GTK_BOX (field), item, FALSE, FALSE, 5);
  gtk_widget_show (item);

  /* Contruct tail end with information and action buttons. */
  area = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (layout), area, FALSE, TRUE, 2);
  gtk_widget_show (area);

  /* Add the about program icon button. */
  info = xpm_button(ICON_HELP, NULL, 0, _("Information"));
  gtk_box_pack_start (GTK_BOX (area), info, FALSE, FALSE, 2);
  gtk_button_set_relief (GTK_BUTTON (info), GTK_RELIEF_NONE);
  g_signal_connect (G_OBJECT(info), "clicked", G_CALLBACK(about), window);
  gtk_widget_show (info);

  /* Assemble the quit button.. we are done! */
  quit = xpm_button(ICON_CLOSE, NULL, 0, _("Close"));
  gtk_box_pack_end (GTK_BOX (area), quit, FALSE, TRUE, 2);
  g_signal_connect (G_OBJECT(quit), "clicked", G_CALLBACK(finis), NULL);
  gtk_widget_show (quit);

  item = xpm_button(ICON_REMOTE, NULL, 0, _("Connect"));
  gtk_box_pack_end (GTK_BOX (area), item, FALSE, TRUE, 2);
  g_signal_connect (G_OBJECT(item), "clicked", G_CALLBACK(rdesktop), NULL);
  gtk_widget_show (item);

  return layout;
} /* </interface> */

/*
 * main - main program implementation
 */
int
main(int argc, char *argv[])
{
  GtkWidget *window;	/* GtkWidget is the storage type for widgets */
  struct _GlobalRemote memory;

#ifdef GETTEXT_PACKAGE
  bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);
  gtk_disable_setlocale();
#endif

  gtk_init (&argc, &argv);	/* initialization of the GTK */
  gtk_set_locale ();

  Program = basename(argv[0]);
  Release = "1.0";

  /* Create a new top level window */
  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

  /* Initialize program data structure */
  global = &memory;

  global->fullscreen = FALSE;
  global->layout     = interface (window);
  global->window     = window;

  /* Set title and border width of the main window */
  gtk_window_set_title (GTK_WINDOW (window), Program);
  gtk_window_set_icon (GTK_WINDOW (window), xpm_pixbuf(ICON_REMOTE, NULL));
  gtk_container_set_border_width (GTK_CONTAINER (window), BORDER_WIDTH);
  gtk_window_set_position (GTK_WINDOW (window), GTK_WIN_POS_CENTER);
  gtk_window_set_resizable (GTK_WINDOW(window), FALSE);

  /* Set event handlers for window close/destroy */
  g_signal_connect (G_OBJECT(window), "destroy", G_CALLBACK(finis), NULL);
  g_signal_connect (G_OBJECT(window), "delete_event", G_CALLBACK(finis), NULL);

  /* Realize the visual components */
  gtk_widget_show (window);

  /* Main event loop */
  gtk_main ();
    
  return Success;
} /* </grdesktop> */
