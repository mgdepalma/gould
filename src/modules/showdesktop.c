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

#include "gould.h"
#include "gpanel.h"
#include "module.h"

#include "greenwindow.h"
#include "xutil.h"

extern const char *Authors;	/* see, gpanel.c */

/*
* Data structures used by this module.
*/
typedef struct _ShowDesktop ShowDesktop;

struct _ShowDesktop
{
  GList       *hidden;		/* list of hidden spaces */
  GList       *winlist;		/* list of active windows */
  GtkTooltips *tooltips;	/* [Hide | Show] Desktop */
};

static ShowDesktop local_;     /* private structure singleton */

/*
 * (private) desktop_changed
 * (private) showdesktop_filter
 * (private) toggle_showing_desktop
 */
static void
desktop_changed (Green *green, GtkWidget *widget)
{
  int desktop = green_get_active_workspace (green);
  gpointer hidden = g_list_find (local_.hidden, GUINT_TO_POINTER (desktop));

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(widget),
                                (hidden != NULL) ? TRUE : FALSE);
} /* </desktop_changed> */

gboolean
showdesktop_filter (Window xid, int desktop)
{
  gboolean result = FALSE;	/* TRUE => reject */

  if (desktop >= 0 && desktop != get_window_desktop (xid))
    result = TRUE;
  else {
    XWindowState xws;
    XWindowType  xwt;

    if (get_window_state (xid, &xws) &&
        (xws.hidden || xws.skip_pager || xws.skip_taskbar))
      result = TRUE;

    if (get_window_type (xid, &xwt) && (xwt.desktop || xwt.dock || xwt.splash))
      result = TRUE;
  }

  return result;
} /* </showdesktop_filter> */

static void
toggle_showing_desktop (GtkWidget *widget, GlobalPanel *panel)
{
  Screen  *screen  = green_get_screen (panel->green);
  Display *display = DisplayOfScreen(screen);

  int screenumber = XScreenNumberOfScreen(screen);
  int desktop = get_current_desktop (screen);

  gboolean active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(widget));
  gpointer hidden = g_list_find (local_.hidden, GUINT_TO_POINTER (desktop));

  GList *iter;		/* list of windows iterator */
  Window xid;
  int place;


  if (active) {
    gtk_tooltips_set_tip (local_.tooltips, widget, _("Show Desktop"), NULL);
    local_.winlist = green_get_windows (panel->green, showdesktop_filter, -1);

    for (iter = local_.winlist; iter != NULL; iter = iter->next) {
      xid = green_window_get_xid (iter->data);
      place = get_window_desktop (xid);

      if (place >= 0 && place == desktop)
        XIconifyWindow(display, xid, screenumber);
    }

    if (hidden == NULL)		/* add desktop to the hidden list */
      local_.hidden = g_list_prepend (local_.hidden,
                                      GUINT_TO_POINTER (desktop));
  }
  else {
    gtk_tooltips_set_tip (local_.tooltips, widget, _("Hide Desktop"), NULL);

    for (iter = local_.winlist; iter != NULL; iter = iter->next) {
      xid = green_window_get_xid (iter->data);
      place = get_window_desktop (xid);

      if (place >= 0 && place == desktop)
        XMapWindow(display, xid);
    }

    if (hidden != NULL)		/* remove desktop from the hidden list */
      local_.hidden = g_list_remove (local_.hidden,
                                     GUINT_TO_POINTER (desktop));
    g_list_free (local_.winlist);
    local_.winlist = NULL;
  }
} /* </toggle_showing_desktop> */

/*
 * module_settings provides configuration pages
 */
GtkWidget *
module_settings (Modulus *applet, GlobalPanel *panel)
{
  return settings_missing_new (applet, panel);
} /* </module_settings> */

/*
 * Required methods
 */
void
module_init (Modulus *applet)
{
  /* GlobalPanel *panel = applet->data; */

  /* Modulus structure initialization. */
  applet->name    = "showdesktop";
  applet->icon    = "showdesktop.png";
  applet->place   = PLACE_START;
  applet->space   = MODULI_SPACE_TASKBAR;
  applet->release = "1.1";
  applet->authors = Authors;

  /* Initialize private data structure singleton. */
  memset(&local_, 0, sizeof (ShowDesktop));
  local_.tooltips = gtk_tooltips_new ();

  /* Construct the settings pages.
  applet->label       = "Show Desktop";
  applet->description = _(
"Showdesktop hides and shows the windows on the desktop."
);
  applet->settings = settings_notebook_new (applet, panel,
                       _("Settings"), module_settings (applet, panel),
                       _("About"), settings_manpage_new (applet, panel),
                       -1);
  */
} /* </module_init> */

void
module_open (Modulus *applet)
{
  GtkWidget *button;
  GtkWidget *widget = NULL;
  GlobalPanel *panel = applet->data;
  PanelIcons *icons = panel->icons;

  /* Construct the user interface. */
  button = gtk_toggle_button_new ();
  gtk_button_set_relief (GTK_BUTTON(button), GTK_RELIEF_NONE);

  g_signal_connect (G_OBJECT (panel->green), "active_workspace_changed",
                    G_CALLBACK (desktop_changed), button);

  g_signal_connect (G_OBJECT (button), "toggled",
                    G_CALLBACK (toggle_showing_desktop), panel);

  if (applet->icon) {
    const gchar *file = icon_path_finder (icons, applet->icon);

    if (file != NULL)
      widget = image_new_from_file_scaled (file, icons->size, icons->size);
  }

  if (widget == NULL)
    widget = gtk_label_new (applet->name);

  gtk_container_add (GTK_CONTAINER (button), widget);
  gtk_widget_show (widget);

  /* Setup tooltips. */
  gtk_tooltips_set_tip (local_.tooltips, button, _("Hide Desktop"), NULL);

  applet->widget = button;
} /* module_open */

void
module_close (Modulus *applet)
{
  gtk_widget_destroy (applet->widget);
} /* module_close */
