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

#include "interface.h"
#include "xpmglyphs.h"

#include <sys/types.h>
#include <signal.h>

/*
 * Protected data structures.
*/
enum
{
  ABOUT_PAGE,		/* about page */
  START_PAGE,		/* media selection page */
  SETUP_PAGE,		/* dynamic content page */
  DEPOT_PAGE,		/* packages to be installed */
  LAST_PAGE		/* last page marker */
};

const char *DiskInstallation =
" Selecting this option allows you to install to disk drives on the system."
" You will need\n"
" to confirm the disk(s) and partitions that need to be created. Mininum"
" you will need\n"
" to choose one partition for the root file system and"
" one partition for swap space."
"\n";

const char *MediaInstallation =
" Selecting this option allows you to install to a USB drive."
" The program will ask you\n"
" to confirm the USB attached to the system."
"\n";

extern gboolean logout_;	/* declared in the main program */
extern gboolean development_;	/* declared in the main program */

GlobalData *global_;		/* global program data singleton */

/*
 * finis clean program exit
 */
void
finis (GtkWidget *widget, gpointer data)
{
  if (strcmp(getprogname(), GENESIS) == 0 && logout_) {
    Window window = DefaultRootWindow (gdk_display);
    unsigned char *data = get_xprop_name (window, "_OPENBOX_PID");

    if (data == NULL)
      data = get_xprop_name (window, "_BLACKBOX_PID");

    if (data != NULL) {
      pid_t pid = *(unsigned long *)data;

      if (kill(pid, SIGTERM) == 0 && kill(pid, 0) != 0)
        kill(pid, SIGKILL);	/* may need SIGKILL for fluxbox */
    }
    else {
      printf("%s: Cannot obtain PID of window manager.\n", Program);
    }
  }
  gtk_main_quit ();
} /* </finis> */

/*
 * [ABOUT_PAGE] about page
 */
static void
about (GtkNotebook *book)
{
  GtkWidget *layout = gtk_vbox_new (FALSE, 2);
  GtkWidget *layer  = navigation_frame (layout);

  GtkWidget *button, *info, *label;
  gchar *description;

  /* Add contents.. */
  info = gtk_hbox_new (FALSE, 2);
  gtk_box_pack_start (GTK_BOX(layer), info, FALSE, FALSE, 5);
  gtk_widget_show (info);

  button = xpm_image (ICON_WELCOME);
  gtk_box_pack_start (GTK_BOX(info), button, FALSE, FALSE, 5);
  gtk_widget_show (button);

  description = g_strdup_printf (
                   "<span font_desc=\"Sans 18\" weight=\"bold\">%s\n</span>"
                   "<span font_desc=\"Arial 12\">\n\n%s</span>",
                   lsb_release_full(), Terms);

  label = gtk_label_new (description);
  gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
  gtk_misc_set_alignment (GTK_MISC(label), 0.06, 0.5);
  gtk_box_pack_start (GTK_BOX(info), label, FALSE, FALSE, 8);
  gtk_widget_show (label);
  g_free (description);

  /* Add button to go to START_PAGE */
  info = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX(layout), info, FALSE, FALSE, 0);
  gtk_widget_show (info);

  button = stock_button_new (_("Continue"), GTK_STOCK_REDO, 8);
  gtk_box_pack_end (GTK_BOX(info), button, FALSE, FALSE, 8);
  gtk_widget_show (button);

  g_signal_connect (G_OBJECT(button), "clicked",
                    G_CALLBACK(navigation_page), GINT_TO_POINTER(START_PAGE));

  gtk_notebook_append_page (book, layout, NULL);
  gtk_widget_show (layout);
} /* </about> */

/*
 * [START_PAGE] select_media
 */
void
select_media (GtkNotebook *book)
{
  GtkWidget *layout = gtk_vbox_new (FALSE, 0);
  GtkWidget *layer  = navigation_frame (layout);

  GtkWidget *button, *info, *label;
  gchar *description;

  /* Add general instructions first. */
  info = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX(layer), info, FALSE, FALSE, 30);
  gtk_widget_show (info);

  button = xpm_image_scaled (ICON_GENESIS, 28, 28);
  gtk_box_pack_start (GTK_BOX(info), button, FALSE, FALSE, 20);
  gtk_widget_show (button);

  description = g_strdup_printf (
                   "<span font_desc=\"Arial 14\" weight=\"bold\">%s</span>",
                   _("Please choose from one of the following options:"));

  label = gtk_label_new (description);
  gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
  /* gtk_misc_set_alignment (GTK_MISC(label), 0.06, 0.5); */
  gtk_box_pack_start (GTK_BOX(info), label, FALSE, FALSE, 0);
  gtk_widget_show (label);
  g_free (description);

  /* Hard-disk installation option. */
  info = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX(layer), info, FALSE, FALSE, 0);
  gtk_widget_show (info);

  button = xpm_image_button (ICON_HARDISK);
  gtk_box_pack_start (GTK_BOX(info), button, FALSE, FALSE, 10);
  gtk_widget_show (button);

  if (development_ || geteuid() == 0) /* get a free pass in development mode */
    g_signal_connect (G_OBJECT(button), "clicked",
                      G_CALLBACK(navigation_page),GINT_TO_POINTER(SETUP_PAGE));
  else
    gtk_widget_set_sensitive (button, FALSE);

  description = g_strdup_printf (
                   "<span font_desc=\"Arial 14\">%s</span>",
                    DiskInstallation);

  label = gtk_label_new (description);
  gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
  gtk_misc_set_alignment (GTK_MISC(label), 0.06, 0.5);
  gtk_box_pack_start (GTK_BOX(info), label, FALSE, FALSE, 8);
  gtk_widget_show (label);
  g_free (description);

  /* USB Drive installation option. */
  info = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX(layer), info, FALSE, FALSE, 20);
  gtk_widget_show (info);

  button = xpm_image_button (ICON_USBDRIVE);
  gtk_box_pack_start (GTK_BOX(info), button, FALSE, FALSE, 10);
  gtk_widget_show (button);

  g_signal_connect (G_OBJECT(button), "clicked",
                    G_CALLBACK(navigation_page), GINT_TO_POINTER(SETUP_PAGE));

  description = g_strdup_printf (
                   "<span font_desc=\"Arial 14\">%s</span>",
                    MediaInstallation);

  label = gtk_label_new (description);
  gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
  gtk_misc_set_alignment (GTK_MISC(label), 0.06, 0.5);
  gtk_box_pack_start (GTK_BOX(info), label, FALSE, FALSE, 8);
  gtk_widget_show (label);
  g_free (description);

  /* Add warning when not all options are enabled. */
  if (geteuid() != 0) {
    info = gtk_hbox_new (FALSE, 0);
    gtk_box_pack_end (GTK_BOX(layer), info, FALSE, FALSE, 6);
    gtk_widget_show (info);

    button = xpm_image_scaled (ICON_WARNING, 24, 22);
    gtk_box_pack_start (GTK_BOX(info), button, FALSE, FALSE, 8);
    gtk_widget_show (button);

    description = g_strdup_printf (
                   "<span font_desc=\"Arial 12\" weight=\"bold\">%s</span>",
         _("Sorry, you need root permissions for all options to be enabled."));

    label = gtk_label_new (description);
    gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
    gtk_misc_set_alignment (GTK_MISC(label), 0.06, 0.5);
    gtk_box_pack_start (GTK_BOX(info), label, FALSE, FALSE, 2);
    gtk_widget_show (label);
    g_free (description);
  }

  /* Add close application button. */
  layer = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_end (GTK_BOX(layout), layer, FALSE, FALSE, 4);
  gtk_widget_show (layer);

  button = stock_button_new (_("Quit"), GTK_STOCK_QUIT, 8);
  gtk_box_pack_end (GTK_BOX(layer), button, FALSE, FALSE, 8);
  gtk_widget_show (button);

  g_signal_connect (G_OBJECT(button), "clicked",
                    G_CALLBACK(finis), NULL);

  gtk_notebook_append_page (book, layout, NULL);
  gtk_widget_show (layout);
} /* </select_media> */

/*
 * [SETUP_PAGE] select_partitions
 */
void
select_partitions (GtkNotebook *book)
{
  GtkWidget *layout = gtk_vbox_new (FALSE, 0);
  GtkWidget *layer = navigation_frame (layout);

  GList *iter, *list = get_disk_list (get_usb_storage());
  GList *internal = NULL, *part;

  GtkWidget *label = gtk_label_new ("SELECT PARTITIONS");
  gtk_box_pack_start (GTK_BOX(layer), label, FALSE, FALSE, 30);
  gtk_widget_show (label);

  for (iter = list; iter; iter = iter->next) {
    label = gtk_label_new (iter->data);
    gtk_box_pack_start (GTK_BOX(layer), label, FALSE, FALSE, 0);
    gtk_widget_show (label);

    for (part = get_partitions (iter->data); part; part = part->next)
      internal = g_list_append (internal, part->data);
  }

  for (iter = internal; iter; iter = iter->next)
    printf("select_partitions [all] => %s\n", (char *)iter->data);

  for (iter = get_usb_storage(); iter; iter = iter->next)
    for (part = get_partitions (iter->data); part; part = part->next)
      printf("select_partitions [usb] => %s\n", (char *)part->data);

  printf("\n========== mounted removable devices\n");
  for (iter = get_mounted_devices (internal); iter; iter = iter->next) {
    DeviceInfo *item = iter->data;
    printf("%s %s\n", item->fsname, item->mntdir);
  }

  /* Buttons to go back and forward a page. */
  navigation_buttons (layout, START_PAGE, DEPOT_PAGE);

  gtk_notebook_append_page (book, layout, NULL);
  gtk_widget_show (layout);
} /* </select_partions> */

/*
 * [DEPOT_PAGE] select_packages
 */
void
select_packages (GtkNotebook *book)
{
  GtkWidget *layout = gtk_vbox_new (FALSE, 0);
  GtkWidget *layer  = navigation_frame (layout);

  GtkWidget *label = gtk_label_new ("SELECT PACKAGES");
  gtk_box_pack_start (GTK_BOX(layer), label, FALSE, FALSE, 30);
  gtk_widget_show (label);

  /* Buttons to go back and forward a page. */
  navigation_buttons (layout, START_PAGE, ABOUT_PAGE);

  gtk_notebook_append_page (book, layout, NULL);
  gtk_widget_show (layout);
} /* </select_packages> */

/*
 * select_usb_drive
 */
void
select_usb_drive (GtkNotebook *book)
{
  GtkWidget *layout = gtk_vbox_new (FALSE, 0);
  GtkWidget *layer  = navigation_frame (layout);

  GtkWidget *label = gtk_label_new ("SELECT USB DRIVE");
  gtk_box_pack_start (GTK_BOX(layer), label, FALSE, FALSE, 30);
  gtk_widget_show (label);

  get_usb_storage();	/* DEBUG */

  /* Buttons to go back and forward a page. */
  navigation_buttons (layout, START_PAGE, ABOUT_PAGE);

  gtk_notebook_append_page (book, layout, NULL);
  gtk_widget_show (layout);
} /* </select_usb_drive> */

/*
 * interface_visible
 */
static gboolean
interface_visible (GtkWidget *widget, GdkEvent *event, gpointer data)
{
  /*
  Atom actions[] = {
    get_atom_property ("_NET_WM_ACTION_CLOSE"),
    get_atom_property ("_NET_WM_ACTION_MOVE"),
    get_atom_property ("_NET_WM_ACTION_RESIZE"),
    0, 0
  };
  */

  gint width  = gdk_screen_width ();
  gint height = gdk_screen_height ();

  gint xsize, ysize;	/* get size to calculate move to the middle */

  gtk_window_get_size (GTK_WINDOW(widget), &xsize, &ysize);
  gtk_window_move (GTK_WINDOW(widget), (width-xsize) / 2, (height-ysize) / 2);

  /* FIXME! set_window_allowed_actions() not working as expected.
  set_window_allowed_actions (GDK_WINDOW_XWINDOW (widget->window), actions, 3);
  */

  return FALSE;
} /* </interface_visible> */

/*
 * constructor main notebook pages
 */
GtkWidget *
constructor (GtkWidget *widget, const char *root)
{
  GtkNotebook *book;
  GtkWidget   *layout;

  /* Construct layout starting with the about page. */
  layout = gtk_notebook_new ();
  book = global_->pages = GTK_NOTEBOOK(layout);

  about (book);			/* ABOUT_PAGE */
  select_media (book);		/* START_PAGE */
  select_partitions (book);	/* SETUP_PAGE */
  select_packages (book);	/* DEPOT_PAGE */

  gtk_notebook_set_current_page (book, ABOUT_PAGE);
  gtk_notebook_set_show_border (book, FALSE);
  gtk_notebook_set_show_tabs (book, FALSE);

  return layout;
} /* </constructor> */

/*
 * interactive graphical user interface
 */
void
interactive (int argc, char *argv[])
{
  GtkWindow *window;
  GtkWidget *interface;
  GtkWidget *layout;

  gint width, height;
  gint xres;

  const char *distro = lsb_release (DISTRIB_DESCRIPTION);
  const char *root = (argc > 1 && argv[argc-1]) ? argv[argc-1] : "/";

  if (root[0] == '-' || isdigit(root[0]))
    root = "/";

#ifdef GETTEXT_PACKAGE
  bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);
  gtk_disable_setlocale();
#endif

  gtk_init (&argc, &argv);      /* initialization of the GTK */
  gtk_set_locale ();

  /* Adjust width and height according to screen resolution. */
  xres = gdk_screen_width ();

  width  = 75 * xres / 100;
  height = 50 * width / 100;

  /* Allocate memory for the global_ singleton. */
  global_ = g_new0 (GlobalData, 1);

  global_->cursor[BUSY_CURSOR]   = gdk_cursor_new (GDK_WATCH);
  global_->cursor[NORMAL_CURSOR] = gdk_cursor_new (GDK_TOP_LEFT_ARROW);

  /* Construct the user interface. */
  interface = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_widget_set_size_request (interface, width, height);
  window = GTK_WINDOW (interface);
  gtk_widget_show (interface);

  gtk_window_set_skip_pager_hint (window, TRUE);
  /* gtk_window_set_skip_taskbar_hint (window, TRUE); */
  gtk_window_stick (window);

  gtk_window_set_keep_above (window, TRUE);
  gtk_window_set_position (window, GTK_WIN_POS_CENTER);
  gtk_window_set_icon (window, xpm_pixbuf(ICON_GENESIS, NULL));

  if (distro == NULL)				/* should never happen */
    gtk_window_set_title (window, getprogname());
  else {
    gchar *text = g_strdup_printf ("%s %s", distro, _("installer"));
    gtk_window_set_title (window, text);
    g_free (text);
  }

  /* Set event handlers for window close/destroy. */
  g_signal_connect (G_OBJECT(interface), "destroy", G_CALLBACK(finis), NULL);
  g_signal_connect (G_OBJECT(interface), "delete-event",G_CALLBACK(finis),NULL);

  /* Set event handler for window becoming visible. */
  g_signal_connect (G_OBJECT(interface), "map-event",
                    G_CALLBACK(interface_visible), (gpointer)root);

  layout = constructor (interface, root);  /* realize the visual components */
  gtk_container_add (GTK_CONTAINER(interface), layout);
  gtk_widget_show (layout);

  gtk_main ();				   /* main event loop */
} /* interactive */
