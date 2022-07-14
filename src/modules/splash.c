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

#include "module.h"
#include "docklet.h"
#include "gpanel.h"

extern const char *Authors;	/* see, gpanel.c */

/*
* Data structures used by this module.
*/
typedef struct _SplashConfig  SplashConfig;
typedef struct _SplashPrivate SplashPrivate;

struct _SplashConfig
{
  guint expire;			/* timeout in seconds (0 => disable) */
  const gchar *command;         /* command to execute instead of panel */
  const gchar *backdrop;	/* ghost window backdrop */
  const gchar *fontdesc;	/* PangoFontDescription to use */
  const gchar *message;		/* message displayed */
  const gchar *pencil;
  GdkColor *fg;			/* foreground color */
  GdkColor *bg;			/* background color */
};

struct _SplashPrivate
{
  gboolean enable;		/* enable (or not) user selection */

  SplashConfig  cache;		/* cache for configuration data */
  SplashConfig* splash;		/* applet configuration data */

  GtkWidget *enable_toggle;	/* enable (or not) check box */
  GtkWidget *fontfamily;
  GtkWidget *fontbold;
  GtkWidget *fontsize;

  gint width, height;		/* splash panel initial size */
};

static SplashPrivate local_;	/* private global structure singleton */

/*
 * (private) dismiss callback to unmap splash widgets
 * (private) realize callback to display splash widgets
 * (private) onclick callback to unmap splash widget on button_press
 */
static gboolean
dismiss (Modulus *applet)
{
  GlobalPanel  *panel  = applet->data;
  SplashConfig *splash = local_.splash;

  if (splash->backdrop) gtk_widget_hide (panel->backdrop);
  gtk_widget_hide (applet->widget);

  return FALSE;
} /* </dismiss> */

static void
realize (GtkWidget *button, Modulus *applet)
{
  if (applet->widget != NULL) {
    GlobalPanel  *panel  = applet->data;
    SplashConfig *splash = local_.splash;

    if (splash->backdrop) 	/* ghost backdrop window */
      gtk_widget_show (panel->backdrop);

    if (button != NULL)
      gtk_widget_show (applet->widget);

    if (splash->expire > 0)	/* dismiss after 'expire' seconds */
      g_timeout_add (1000 * splash->expire, (GSourceFunc)dismiss, applet);
  }
} /* </realize> */

static void
onclick (GtkWidget *widget, GdkEventButton *event, Modulus *applet)
{
  dismiss (applet);
} /* </onclick> */

/*
 * splash_configuration_read
 */
static void
splash_configuration_read (Modulus *applet, SplashConfig *splash)
{
  GlobalPanel *panel = applet->data;
  ConfigurationNode *chain = configuration_find (panel->config, "modules");
  ConfigurationNode *item;

  const gchar *attrib;


  /* Configuration for the splash screen. */
  applet->enable = TRUE;
  splash->fontdesc = "Sans Bold 20";		/* fallback default */

  while ((item = configuration_find (chain, "applet")) != NULL) {
    attrib = configuration_attrib (item, "name");

    if (strcmp(attrib, applet->name) == 0) {
      if ((attrib = configuration_attrib (item, "enable")) != NULL)
        if (strcmp(attrib, "off") == 0 || strcmp(attrib, "no") == 0)
          applet->enable = FALSE;

      if ((attrib = configuration_attrib (item, "command")) != NULL)
        splash->command = attrib;

      if ((attrib = configuration_attrib (item, "icon")) != NULL)
        applet->icon = attrib;

      if ((item = configuration_find (item, "settings")) != NULL) {
        if ((attrib = configuration_attrib (item, "backdrop")) != NULL)
          splash->backdrop = attrib;

        if ((attrib = configuration_attrib (item, "expire")) != NULL)
          splash->expire = atoi(attrib);

        if ((attrib = configuration_attrib (item, "message")) != NULL)
          splash->message = attrib;

        if ((attrib = configuration_attrib (item, "font")) != NULL)
          splash->fontdesc = attrib;

        if ((attrib = configuration_attrib (item, "fg")) != NULL) {
          splash->fg = g_new0 (GdkColor, 1);
          gdk_color_parse (attrib, splash->fg);
          gdk_colormap_alloc_color (gdk_colormap_get_system(),
                                    splash->fg, TRUE, TRUE);
          splash->pencil = attrib;
        }

        if ((attrib = configuration_attrib (item, "bg")) != NULL) {
          splash->bg = g_new0 (GdkColor, 1);
          gdk_color_parse (attrib, splash->bg);
          gdk_colormap_alloc_color (gdk_colormap_get_system(),
                                    splash->bg, TRUE, TRUE);
        }
      }
      break;
    }
    chain = configuration_find_next (chain, "applet");
  }

  /* If without splash->message, use `lsb_release -s -d -r -c` */
  if (splash->message == NULL) {
    splash->message = lsb_release_full();
  }
} /* </splash_configuration_read> */

/*
 * splash_settings_apply callback to save settings
 * splash_settings_cancel callback to rollback settings
 */
static void
splash_settings_apply (Modulus *applet)
{
  GlobalPanel *panel = applet->data;
  SplashConfig *splash = local_.splash;

  const char *spec;
  gchar *data;


  /* Save configuration settings from singleton. */
  memcpy(splash, &local_.cache, sizeof(SplashConfig));
  applet->enable = local_.enable;

  /* Save configuration settings in cache. */
  spec =
"<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
"<applet name=\"%s\" icon=\"%s\"%s>\n"
" <settings expire=\"%d\" font=\"%s\" fg=\"%s\" />\n"
"</applet>\n";

  data = g_strdup_printf (spec, applet->name, applet->icon,
                          ((applet->enable) ? "" : " enable=\"no\""),
                          splash->expire, splash->fontdesc, splash->pencil);
  vdebug (3, "\n%s\n", data);

  /* Replace configuration item. */
  configuration_replace (panel->config, data, "modules", "applet",applet->name);
  g_free (data);

  /* Memory was freed when original item was removed, read configuration. */
  splash_configuration_read (applet, splash);

  /* Save configuration cache data. */
  memcpy(&local_.cache, splash, sizeof(SplashConfig));
} /* </splash_settings_apply> */

static void
splash_settings_cancel (Modulus *applet)
{
  SplashConfig *splash = local_.splash;	/* persistent settings */

  memcpy(&local_.cache, splash, sizeof(SplashConfig));
  local_.enable = applet->enable;

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(local_.enable_toggle),
                                applet->enable);
} /* </splash_settings_cancel> */

/*
 * (private) splash_enable
 */
static void
splash_enable (GtkWidget *button, Modulus *applet)
{
  gboolean state = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(button));

  if (state != local_.enable) {
    GlobalPanel *panel = applet->data;
    PanelSetting *settings = panel->settings;

    /* Register callbacks for apply and cancel. */
    settings_save_enable (settings, TRUE);
    settings_set_agents (settings,
                         (gpointer)splash_settings_apply,
                         (gpointer)splash_settings_cancel,
                         NULL);

    local_.enable = state;
  }
} /* </splash_enable> */

/*
 * module_settings provides configuration pages
 */
GtkWidget *
module_settings (Modulus *applet, GlobalPanel *panel)
{
  SplashConfig *splash = local_.splash;

  GtkWidget *layout = gtk_vbox_new (FALSE, 2);
  GtkWidget *frame, *area, *button, *image, *label, *layer, *widget;

  const gchar *icon = icon_path_finder (panel->icons, applet->icon);
  gchar *caption = g_strdup_printf ("%s [%s]", _(applet->label), _("plugin"));
  char *font = "Sans Bold 12";  /* FIXME! scale from splash->font */

  /* Initialize private data structure singleton settings cache. */
  memcpy(&local_.cache, splash, sizeof(SplashConfig));
  local_.enable = applet->enable;

  /* Construct settings page. */
  frame = gtk_frame_new (caption);
  gtk_frame_set_shadow_type (GTK_FRAME(frame), GTK_SHADOW_ETCHED_IN);
  gtk_container_set_border_width (GTK_CONTAINER (frame), 10);
  gtk_container_add (GTK_CONTAINER(layout), frame);
  gtk_widget_show (frame);
  g_free (caption);

  area = gtk_vbox_new (FALSE, 4);
  gtk_container_add (GTK_CONTAINER(frame), area);
  gtk_widget_show (area);

  /* Start with enable (or not) check box. */
  layer = gtk_hbox_new (FALSE, 2);
  gtk_box_pack_start (GTK_BOX(area), layer, FALSE, FALSE, 0);
  gtk_widget_show (layer);

  button = local_.enable_toggle = gtk_check_button_new_with_label (_("Enable"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(button), applet->enable);
  gtk_box_pack_start (GTK_BOX(layer), button, FALSE, TRUE, 6);
  gtk_widget_show (button);

  g_signal_connect (G_OBJECT(button), "toggled",
                    G_CALLBACK(splash_enable), applet);

  /* Build preview window. */
  widget = gtk_hbox_new (FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (layer), 2);
  gtk_widget_show (widget);

  image = image_new_from_file_scaled (icon, 64, 64);
  gtk_box_pack_start (GTK_BOX (widget), image, FALSE, FALSE, 3);
  gtk_widget_show (image);

  label = gtk_label_new (splash->message);
  gtk_box_pack_start (GTK_BOX (widget), label, FALSE, TRUE, 3);
  gtk_label_set_use_markup (GTK_LABEL (label), TRUE);

  gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
  //gtk_label_set_max_width_chars (GTK_LABEL (label), 20);
  gtk_widget_modify_font (label, pango_font_description_from_string(font));
  gtk_widget_show (label);

  button = gtk_button_new ();
  gtk_container_add (GTK_CONTAINER (button), widget);

  g_signal_connect (G_OBJECT(button), "clicked",
                    G_CALLBACK(realize), applet);

  layer = gtk_hbox_new (FALSE, 4);
  gtk_box_pack_start (GTK_BOX(area), layer, FALSE, FALSE, 0);
  gtk_widget_show (layer);

  label = gtk_label_new ("");
  gtk_box_pack_start (GTK_BOX(layer), label, FALSE, FALSE, 8);
  gtk_widget_show (label);

  widget = gtk_vbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX(layer), widget, FALSE, FALSE, 8);
  gtk_widget_show (widget);

  gtk_box_pack_start (GTK_BOX(widget), button, FALSE, FALSE, 0);
  gtk_widget_show (button);

  label = gtk_label_new (_(".. click to preview"));
  gtk_box_pack_start (GTK_BOX(widget), label, FALSE, FALSE, 0);
  gtk_widget_show (label);

  return layout;
} /* </module_settings> */

/*
 * module_interface contructs the splash panel window
 */
GtkWidget *
module_interface (Modulus *applet, GlobalPanel *panel)
{
  const gchar  *icon   = icon_path_finder (panel->icons, applet->icon);
  SplashConfig *splash = local_.splash;

  gint width  = green_screen_width ();
  gint height = gdk_screen_height ();

  gint xsize  = 400; /* fallback hardcoded preferred dimensions */
  gint ysize  = 200;


  if (icon != NULL) {
    GError *error = NULL;
    GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file (icon, &error);

    if (pixbuf) {
      xsize = gdk_pixbuf_get_width (pixbuf);
      ysize = gdk_pixbuf_get_height (pixbuf);
      g_object_unref (pixbuf);
    }
  }

  /* Initial splash panel window preferred size and position. */
  local_.width  = (width > 800) ? 7 * xsize / 4 : 6 * xsize / 5;
  local_.height = ysize;

  Docklet *docklet = docklet_new (GDK_WINDOW_TYPE_HINT_SPLASHSCREEN,
                                  local_.width, local_.height, -1, -1,
                                  GTK_ORIENTATION_HORIZONTAL,
                                  GTK_VISIBILITY_FULL,
                                  icon,
                                  splash->message,
				  splash->fontdesc,
                                  splash->fg,
                                  splash->bg);

  GtkWidget *widget = docklet->window;

  /* We do not want drag-n-drop Docklet behavior. */
  g_signal_handler_disconnect (G_OBJECT (widget), docklet->buttonHandler);
  g_signal_handler_disconnect (G_OBJECT (widget), docklet->windowHandler);

  /* Keep the window above all others, and change cursor. */
  gtk_window_set_keep_above (GTK_WINDOW(widget), TRUE);
  docklet_set_cursor (docklet, GDK_CIRCLE);

  /* Center the splash panel window. */
  gtk_window_get_size (GTK_WINDOW(widget), &xsize, &ysize);
  gtk_window_move (GTK_WINDOW(widget), (width-xsize) / 2, (height-ysize) / 2);

  return widget;
} /* </module_interface> */

/*
 * Required methods
 */
void
module_init (Modulus *applet)
{
  GlobalPanel  *panel  = applet->data;
  SplashConfig *splash = g_new0 (SplashConfig, 1);

  /* Modulus structure initialization. */
  applet->name    = "splash";
  applet->icon    = "welcome.png";
  applet->place   = PLACE_SCREEN;
  applet->space   = MODULI_SPACE_DESKTOP;
  applet->release = "0.9.11";
  applet->authors = Authors;

  /* Read configuration data for the splash screen. */
  splash_configuration_read (applet, splash);

  /* Initialize private data structure singleton. */
  memset(&local_, 0, sizeof (SplashPrivate));
  local_.splash = splash;

  /* Construct the settings pages. */
  applet->label       = "Splash Screen";
  applet->description = _(
"Splash screen displayed upon startup."
);
  applet->settings = settings_notebook_new (applet, panel,
                       _("Settings"), module_settings (applet, panel),
                       _("About"), settings_manpage_new (applet, panel),
                       -1);
} /* </module_init> */

/* Construct the user interface. */
void
module_open (Modulus *applet)
{
  GlobalPanel  *panel  = applet->data;
  SplashConfig *splash = local_.splash;

  if (splash->command) {
    /* shutdown (panel, SIGHUP); */
    vdebug (1, "\nsplash->command => %s\n", splash->command);
    g_spawn_command_line_sync (splash->command, 0, 0, 0, 0);
    /* shutdown (panel, SIGCONT); */
  }
  else if (splash->message != NULL) {  /* without splash->message, abend */
    applet->widget = module_interface (applet, panel);
    realize (NULL, applet);

    g_signal_connect(G_OBJECT (applet->widget), "button_press_event",
                     G_CALLBACK (onclick), applet);
  }
} /* module_open */

void
module_close (Modulus *applet)
{
  if (GTK_IS_WIDGET(applet->widget))
    gtk_widget_destroy (applet->widget);
} /* module_close */
