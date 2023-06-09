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

#include "gould.h"		  /* common package declarations */
#include "gpanel.h"

static struct _dialogue dialogue; /* (private)store message dialog parts */

/*
* alert dialog with CANCEL and OK buttons
*/
static GtkWidget *
alert(GtkWidget *parent, IconIndex icon, const gchar *message)
{
  GtkWidget *button;
  GtkWidget *dialog = gtk_dialog_new ();
  GtkWidget *layout = gtk_hbox_new (FALSE, 8);
  GtkWidget *inside = xpm_label (icon, message);
  GtkWindow *window = GTK_WINDOW(dialog);

  /* Customize behavior and look */
  gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);

  gtk_window_set_modal (window, TRUE);
  gtk_window_set_resizable(window, FALSE);
  gtk_window_set_decorated (window, FALSE);
  gtk_window_set_position (window, GTK_WIN_POS_MOUSE);
  gtk_window_set_transient_for (window, GTK_WINDOW (parent));
  gtk_window_set_skip_taskbar_hint (window, TRUE);
  gtk_window_set_skip_pager_hint (window, TRUE);
  gtk_window_set_keep_above (window, TRUE);
  /*
  gtk_window_set_role(window, "notify_dialog");
  g_signal_connect(G_OBJECT(dialog), "response",
                      G_CALLBACK(fire_alert), dialog);
  */

  /* Assemble contents of dialog window with icon and message */
  gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), layout);
  gtk_box_pack_start(GTK_BOX(layout), inside, FALSE, TRUE, 5);
  gtk_widget_show (layout);

  /* Add dismiss (acknowledge) button */
  button = add_button(dialog, ICON_CANCEL, _("Cancel"), GTK_RESPONSE_CANCEL);
  gtk_widget_hide (button);  /* must be enabled elsewhere */

  /* Store major components in class dialogue */
  dialogue.window = dialog;
  dialogue.layout = layout;
  dialogue.cancel = button;
  dialogue.apply  =
     add_button(dialog, ICON_DONE, _("OK"), GTK_RESPONSE_OK);

  return dialog;
} /* </alert> */

/*
* alert wrapper method
*/
static GtkWidget *
message(GtkWidget *parent, IconIndex icon, const gchar *text)
{
  static GtkWidget *MessageDialog = NULL; /* singleton message dialog */

  if (MessageDialog == NULL)    /* singleton message dialog */
    MessageDialog = alert(parent, icon, text);
  else {
    GtkWidget *container = GTK_DIALOG(MessageDialog)->vbox;
    GtkWidget *layout    = gtk_hbox_new (FALSE, 8);
    GtkWidget *inside    = xpm_label(icon, text);

    gtk_container_remove(GTK_CONTAINER (container), dialogue.layout);
    gtk_box_pack_start(GTK_BOX (layout), inside , FALSE, FALSE, 2);
    gtk_container_add(GTK_CONTAINER (container), layout);
    gtk_widget_show (layout);

    dialogue.layout = layout;
  }
  return MessageDialog;
} /* </message> */

/*
* provide a GTK+ dialog at (xpos,ypos), IconIndex icon, message,...
*/
void
gpanel_dialog(gint xpos, gint ypos, IconIndex icon, const gchar* format, ...)
{
  GtkWidget *dialog;
  gchar *text;

  va_list args;			/* declare a va_list type variable */
  va_start (args, format);	/* initialize va_list variable with '...' */
  text = g_strdup_vprintf (format, args);
  va_end (args);		/* clean up the va_list */

  dialog = message(NULL, icon, text);
  g_free (text);

  if (xpos > 0 && ypos > 0)
    gtk_window_move (GTK_WINDOW(dialog), xpos, ypos);
  else {
    gint xres = gdk_screen_width ();
    gint yres = gdk_screen_height ();
    gtk_window_move (GTK_WINDOW(dialog), (xres/2) - 200, (yres/2) - 100);
  }

  gtk_widget_show (dialog);
  gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_hide (dialog);
} /* </gpanel_dialog> */

/*
* provide a GTK+ dialog in a separate process
*/
pid_t
spawn_dialog(gint xpos, gint ypos, IconIndex icon, const gchar* format, ...)
{
  pid_t pid = fork();

  if (pid == 0) {		/* child process */
    //setsid();			/* create a new session */
    va_list args;
    va_start (args, format);
    /* Forward the '...' to gpanel_dialog */
    gpanel_dialog(xpos, ypos, icon, format, args);
    va_end (args);

    _exit(EX_OK);
  }
  return pid;
} /* </spawn_dialog> */
