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
#include "gwindow.h"
#include "xutil.h"

#include "green.h"
#include "systray.h"
#include "tasklist.h"
#include "pager.h"

#include <stdio.h>
#include <string.h>

const char *Program = "gtaskbar";  /* (public) published program name */
const char *Release = "0.8.1";	  /* (public) published program version */

debug_t debug = 0;	/* debug verbosity (0 => none) {must be declared} */

/*
* clean application exit
*/
static void
finis(GtkWidget *instance, gpointer data)
{
  gtk_main_quit ();
} /* </finis> */

/*
* selfexclude - WindowFiler to exclude self from GREEN window list
*/
static gboolean
selfexclude (Window xid, int desktop)
{
  const gchar *wname = get_window_name (xid);
  return (wname) ? strcmp(wname, Program) == 0 : TRUE;
} /* </selfexclude> */

/*
* dump_window_client_lists
*/
static void
dump_window_client_lists (Window xroot, int desktop)
{
  Atom atom;
  Window *wins;
  int count, idx;

  atom = get_atom_property ("_NET_CLIENT_LIST");
  wins = get_window_list (xroot, atom, desktop, &count);

  if (desktop < 0)
    printf("_NET_CLIENT_LIST (all desktops)\n");
  else
    printf("_NET_CLIENT_LIST (desktop #%d)\n", desktop);

  for (idx = 0; idx < count; idx++)
    printf("\t0x%x (%s)\n", (int)wins[idx], get_window_name (wins[idx]));

  g_free (wins);
  atom = get_atom_property ("_NET_CLIENT_LIST_STACKING");
  wins = get_window_list (xroot, atom, desktop, &count);

  if (desktop < 0)
    printf("_NET_CLIENT_LIST_STACKING (all desktops)\n");
  else
    printf("_NET_CLIENT_LIST_STACKING (desktop #%d)\n", desktop);

  for (idx = 0; idx < count; idx++)
    printf("\t0x%x (%s)\n", (int)wins[idx], get_window_class (wins[idx]));

  g_free (wins);
} /* </dump_window_client_lists> */

static void
tasklist_allspaces (GtkToggleButton *button, Tasklist *tasklist)
{
  gboolean state = gtk_toggle_button_get_active (button);
  tasklist_set_include_all_workspaces (tasklist, state);
} /* </tasklist_allspaces> */

static void
tasklist_grouping (GtkComboBox *options, Tasklist *tasklist)
{
  TasklistGroupingType grouping = gtk_combo_box_get_active (options);
  tasklist_set_grouping (tasklist, grouping);
} /* </tasklist_grouping> */

static void
tasklist_relief (GtkComboBox *options, Tasklist *tasklist)
{
  GtkReliefStyle relief = gtk_combo_box_get_active (options);
  tasklist_set_button_relief (tasklist, relief);
} /* </tasklist_relief> */

/*
* interface - creates the graphical user interface
*/
GtkWidget *
interface (void)
{
  /* Green *green = green_get_default (); */
  Green *green= green_filter_new (selfexclude, DefaultScreen(gdk_display));
  GdkScreen *screen =  green_get_gdk_screen (green);

  GtkWidget *button, *layout, *tasklist, *widget;
  TasklistGroupingType grouping = TASKLIST_NEVER_GROUP;
  Systray *systray = systray_new ();

  if (debug > 1)  /* dump _NET_CLIENT_LIST and _NET_CLIENT_LIST_STACKING */
    dump_window_client_lists (green_get_root_window (green), -1);

  /* Horizonal layout. */
  layout = gtk_hbox_new (FALSE, 1);
  gtk_widget_show (layout);

  /* Add TASKLIST */
  tasklist = GTK_WIDGET (tasklist_new (green));
  tasklist_set_grouping (GREEN_TASKLIST (tasklist), grouping);
  gtk_container_add (GTK_CONTAINER(layout), tasklist);
  gtk_widget_show (tasklist);

  /* Last button is to quit application. */
  button = gtk_button_new_with_label (_("Close"));
  gtk_box_pack_end (GTK_BOX(layout), button, FALSE, TRUE, 2);
  g_signal_connect (G_OBJECT(button), "clicked", G_CALLBACK(finis), NULL);
  gtk_widget_show (button);

  /* Add controls */
  widget = gtk_combo_box_new_text ();
  gtk_combo_box_append_text(GTK_COMBO_BOX(widget), _("Never group"));
  gtk_combo_box_append_text(GTK_COMBO_BOX(widget), _("Always group"));
  gtk_combo_box_append_text(GTK_COMBO_BOX(widget), _("Auto group"));

  gtk_combo_box_set_active(GTK_COMBO_BOX(widget), grouping);
  gtk_box_pack_end (GTK_BOX(layout), widget, FALSE, FALSE, 0);
  gtk_widget_show (widget);

  g_signal_connect (G_OBJECT(widget), "changed",
                    G_CALLBACK(tasklist_grouping), tasklist);

  /* All workspaces checkbox */
  widget = gtk_check_button_new_with_label (_("All spaces"));
  gtk_box_pack_end (GTK_BOX(layout), widget, FALSE, FALSE, 0);
  gtk_widget_show (widget);

  g_signal_connect (G_OBJECT(widget), "toggled",
                    G_CALLBACK(tasklist_allspaces), tasklist);

  /* Add PAGER */
  widget = GTK_WIDGET (pager_new (green));
  pager_set_shadow_type (GREEN_PAGER(widget), GTK_SHADOW_IN);
  /* pager_set_display_mode (GREEN_PAGER(widget), PAGER_DISPLAY_NAME); */
  gtk_box_pack_end (GTK_BOX(layout), widget, FALSE, FALSE, 0);
  gtk_widget_show (widget);

  /* Button relief selection */
  widget = gtk_combo_box_new_text ();
  gtk_combo_box_append_text(GTK_COMBO_BOX(widget), _("Normal relief"));
  gtk_combo_box_append_text(GTK_COMBO_BOX(widget), _("Half relief"));
  gtk_combo_box_append_text(GTK_COMBO_BOX(widget), _("No relief"));

  gtk_combo_box_set_active(GTK_COMBO_BOX(widget), 0);
  gtk_box_pack_end (GTK_BOX(layout), widget, FALSE, FALSE, 0);
  gtk_widget_show (widget);

  g_signal_connect (G_OBJECT(widget), "changed",
                    G_CALLBACK(tasklist_relief), tasklist);

  /* Add systemtray */
  widget = gtk_hbox_new (FALSE, 1);
  gtk_box_pack_end (GTK_BOX(layout), widget, FALSE, FALSE, 0);
  gtk_widget_show (widget);

  if (systray_check_running_screen (screen))
    g_printerr("%s: %s\n", Program, _("Another systemtray already running."));
  else
    systray_connect (systray, screen, widget);

  return layout;
} /* </interface> */

/*
* prova main program
*/
int
main(int argc, char *argv[])
{
  GdkScreen *screen;
  GtkWidget *frame, *window;
  int idx, width;

#ifdef GETTEXT_PACKAGE
  bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);
#endif

  gtk_init (&argc, &argv);	/* initialization of the GTK */

  for (idx = 1; idx < argc; idx++)
    if (strcmp(argv[idx], "-d") == 0)
      debug = (idx < argc-1) ? atoi (argv[++idx]) : 1;

  vdebug (1, "%s started with debug level => %d\n", Program, debug);

  /* Allocate 90% of screen width for the gui. */
  screen = gdk_screen_get_default();
  width = 9 * gdk_screen_get_width (screen) / 10;
  vdebug (1, "gdk_screen_get_width() => %d\n", 10 * width / 9);

  if (screen && GDK_IS_SCREEN(screen)) {
    GdkRectangle rect;
    gint count = gdk_screen_get_n_monitors(screen);
    gint idx;

    gdk_screen_get_monitor_geometry(screen, 0, &rect);
    vdebug (1, "monitor[0] width => %d, heigth => %d\n",
				rect.width, rect.height);
    width = 9 * rect.width / 10;

    for (idx = 1; idx < count; idx++) {
      gdk_screen_get_monitor_geometry(screen, idx, &rect);
      vdebug (1, "monitor[%d] width => %d, heigth => %d\n",
				idx, rect.width, rect.height);
    }
  }

  /* Create the graphical user interface. */
  window = sticky_window_new (GDK_WINDOW_TYPE_HINT_DOCK, width, 32, 20, 50);
  /* gtk_container_add (GTK_CONTAINER(window), interface ()); */
  gtk_widget_show (window);

  /* Give the interface() a nicer look. */
  frame = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_OUT);
  gtk_container_add (GTK_CONTAINER(frame), interface ());
  gtk_container_add (GTK_CONTAINER(window), frame);
  gtk_widget_show (frame);

  /* Set event handlers for window close/destroy. */
  g_signal_connect (G_OBJECT(window), "destroy", G_CALLBACK(finis), NULL);
  g_signal_connect (G_OBJECT(window), "delete_event", G_CALLBACK(finis), NULL);

  gtk_main ();		/* main event loop */

  return 0;
} /* </prova> */
