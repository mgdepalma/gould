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

#include "gould.h"		/* common package declarations */
#include "gscreen.h"
#include "util.h"

const char *Program = "gscreen"; /* (public) published program name */
const char *Release = "1.0.1";	 /* (public) published program version */

const char *Description =
"is a gtk-based control panel utility.\n"
"\n"
"The program is developed for Generations Linux and distributed\n"
"under the terms and condition of the GNU Public License.\n";


static GlobalControl *global;	/* (protected) encapsulated program data */
unsigned short debug = 0;	/* (protected) must be present */

/*
* (private) about
*/
static gboolean
about (GtkWidget *instance, GtkWidget *parent)
{
  static gchar *message = NULL;

  if (message == NULL)
    message = g_strdup_printf("\n%s %s %s", Program, Release, Description);

  notice(parent, ICON_SNAPSHOT, message);

  return FALSE;
} /* </about> */

/*
* clean application exit
*/
static void
finis (GtkWidget *instance, gpointer data)
{
  gtk_main_quit ();
} /* </finis> */

/*
* (private) agent
*/
static void
agent (GtkIconView *iconview, GlobalControl *control)
{
  gchar *name = iconbox_get_selected (control->iconbox, COLUMN_LABEL);
  GdkPixbuf *icon = iconbox_get_selected (control->iconbox, COLUMN_IMAGE);

  if (name) {
    char *command = NULL;

    if (strcmp(name, "System") == 0)
      command = "hardinfo";
    else if (icon == xpm_pixbuf(ICON_CHOOSER, NULL))
      command = "gdisplay";

    printf("%s::agent: %s\n", Program, name);
    if (command) system(command);
  }
} /* </agent> */

/*
* interface - construct user interface
*/
static GtkWidget*
interface (GlobalControl *control)
{
  GtkWidget *window = control->window;
  GtkWidget *layout = gtk_vbox_new (FALSE, 0);
  GtkWidget *box, *button, *scroll, *view;
  GdkColor color = { 0, 65535, 65535, 65535 };;

  gtk_widget_modify_bg (window, GTK_STATE_NORMAL, &color); 

  box = gtk_hbox_new(FALSE, 1);
  gtk_box_pack_start(GTK_BOX (layout), box, FALSE, FALSE, 0);
  gtk_widget_show(box);

  /* Program information (about) button. */
  button = xpm_button(ICON_HELP, _("Information"));
  gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
  g_signal_connect (G_OBJECT(button), "clicked", G_CALLBACK(about), window);
  gtk_box_pack_end (GTK_BOX(box), button, FALSE, TRUE, 2);
  gtk_widget_show (button);

  /* Control panel icons. */
  box = gtk_vbox_new(FALSE, 1);
  gtk_box_pack_start(GTK_BOX (layout), box, TRUE, TRUE, 0);
  gtk_widget_show(box);

  scroll = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
                                 GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_box_pack_start(GTK_BOX (box), scroll, TRUE, TRUE, 5);
  gtk_widget_show(scroll);

  view = (control->iconbox)->view;
  gtk_container_add (GTK_CONTAINER(scroll), view);
  gtk_widget_show(view);

  /* CLOSE button */
  box = gtk_hbox_new(FALSE, 1);
  gtk_box_pack_start(GTK_BOX (layout), box, FALSE, FALSE, 0);
  gtk_widget_show(box);

  button = xpm_button(ICON_CLOSE, _("Close"));
  gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
  g_signal_connect (G_OBJECT(button), "clicked", G_CALLBACK(finis), NULL);
  /* gtk_box_pack_start(GTK_BOX (box), button, FALSE, FALSE, 0); */
  gtk_box_pack_end (GTK_BOX(box), button, FALSE, TRUE, 2);
  gtk_widget_show(button);

  return layout;
} /* </interface> */

/*
* initialize
*/
static void
initialize (GlobalControl *control, char *imagepath)
{
  IconBox *iconbox;

  iconbox = control->iconbox = iconbox_new (GTK_ORIENTATION_VERTICAL);
  g_signal_connect (G_OBJECT(iconbox->view), "selection_changed",
                               G_CALLBACK(agent), control);

  iconbox_append (iconbox, xpm_pixbuf(ICON_CHOOSER, NULL), _("File"));
  iconbox_append (iconbox, xpm_pixbuf(ICON_SNAPSHOT, NULL), _("New Snapshot"));
  iconbox_append (iconbox, xpm_pixbuf(ICON_SETTING, NULL), _("System"));

  control->width  = (4 * gdk_screen_width ())  / 9;
  control->height = (3 * gdk_screen_height ()) / 9;

  global = control;   /* global program data structure */
} /* </initialize> */

/*
* gcontrol main program
*/
int
main(int argc, char *argv[])
{
  struct _GlobalControl memory;	/* see, gcontrol.h */

  GtkWidget *window;		/* main window     */
  GtkWidget *layout;		/* user interface  */

#ifdef GETTEXT_PACKAGE
  bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);
#endif

  gtk_init (&argc, &argv);	/* initialization of the GTK */

  /* Initialize program data structure */
  initialize (&memory, (argc > 1) ? argv[1] : NULL);

  /* Create a new top level window */
  window = global->window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

  /* Set title and border width of the main window */
  gtk_window_set_title (GTK_WINDOW (window), Program);
  gtk_window_set_icon (GTK_WINDOW (window), xpm_pixbuf(ICON_CHOOSER, NULL));
  gtk_window_set_position (GTK_WINDOW (window), GTK_WIN_POS_CENTER);
  gtk_widget_set_usize (window, global->width, global->height);
  gtk_widget_show (window);

  /* Construct the user interface */
  layout = interface (global);
  gtk_container_add (GTK_CONTAINER(window), layout);
  gtk_widget_show (layout);

  /* Set event handlers for window close/destroy */
  g_signal_connect (G_OBJECT(window), "destroy", G_CALLBACK(finis), NULL);
  g_signal_connect (G_OBJECT(window), "delete_event", G_CALLBACK(finis), NULL);

  /* Main event loop */
  gtk_main ();
    
  return Success;
} /* </gcontrol> */
