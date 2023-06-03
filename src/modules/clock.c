/**
 * Copyright (C) Generations Linux <bugs@softcraft.org>
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; You may only use version 2 of the License,
 * you have no option to use any other version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "gould.h"
#include "gpanel.h"
#include "module.h"

#define CLOCK_FORMAT_MAX 80

extern const char *Program;	/* see, gpanel.c */
extern const char *Authors;	/* .. */

/*
* Data structures used by this module.
*/
typedef struct _ClockConfig  ClockConfig;
typedef struct _ClockPrivate ClockPrivate;

struct _ClockConfig
{
  GtkWidget *display;		/* display drawing area */
  const gchar *format;		/* display format (strftime(3)) */
};

struct _ClockPrivate
{
  ClockConfig *clock;		/* applet configuration data */
  const gchar *format;

  gboolean enable;
  gboolean show24hour;
  gboolean showseconds;

  GtkTooltips *tooltips;

  GtkWidget *enable_toggle;
  GtkWidget *hour24_toggle;
  GtkWidget *seconds_toggle;

  GtkWidget *calendar;
  GtkWidget *preview;
};

static ClockPrivate local_;	/* private global structure singleton */


/*
 * clock_configuration_read
 */
static void
clock_configuration_read (Modulus *applet, ClockConfig *clock)
{
  GlobalPanel *panel = applet->data;
  ConfigurationNode *chain = configuration_find (panel->config, "modules");
  ConfigurationNode *item;

  const gchar *attrib;


  /* Configuration for the clock applet. */
  applet->enable  = TRUE;
  clock->format = "%H:%M";	/* hardcoded display format fallback value */

  while ((item = configuration_find (chain, "applet")) != NULL) {
    attrib = configuration_attrib (item, "name");

    if (strcmp(attrib, applet->name) == 0) {
      if ((attrib = configuration_attrib (item, "icon")) != NULL)
        applet->icon = attrib;

      if ((attrib = configuration_attrib (item, "enable")) != NULL)
        if (strcmp(attrib, "off") == 0 || strcmp(attrib, "no") == 0)
          applet->enable = FALSE;

      if ((item = configuration_find (item, "settings")) != NULL) {
        if ((attrib = configuration_attrib (item, "format")) != NULL)
          clock->format = attrib;
      }
      break;
    }
    chain = configuration_find_next (chain, "applet");
  }
} /* </clock_configuration_read> */

/*
 * (private) show_calendar
 * (private) show_preview_time
 * (private) show_time
 */
static gboolean
show_calendar (GtkWidget *widget, GlobalPanel *panel)
{
  g_return_val_if_fail (GTK_IS_WIDGET(widget), FALSE);

  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(widget)))
    gtk_widget_show (local_.calendar);
  else
    gtk_widget_hide (local_.calendar);

  return FALSE;
} /* </show_calendar> */

static gboolean
show_preview_time (ClockConfig *clock)
{
  char text[CLOCK_FORMAT_MAX];
  time_t now = time(NULL);

  if (local_.enable)
    strftime(text, CLOCK_FORMAT_MAX, local_.format, localtime(&now));
  else
    strcpy(text, "--:--");

  gtk_label_set_markup (GTK_LABEL(local_.preview), text);

  return TRUE;
} /* </show_preview_time> */

static gboolean
show_time (Modulus *applet)
{
  ClockConfig *clock = local_.clock;
  char text[CLOCK_FORMAT_MAX];
  time_t now = time(NULL);

  strftime(text, CLOCK_FORMAT_MAX, clock->format, localtime(&now));
  gtk_label_set_markup (GTK_LABEL(clock->display), text);

  if (applet->widget != NULL && GDK_IS_WINDOW (applet->widget->window)) {
    strftime(text, CLOCK_FORMAT_MAX, "%A, %x", localtime(&now));
    gtk_tooltips_set_tip (local_.tooltips, applet->widget, text, NULL);
  }

  return TRUE;
} /* </show_time> */

/*
 * calendar_new
 */
static GtkWidget *
calendar_new (Modulus *applet, GlobalPanel *panel)
{
  GtkWidget *inside, *window;
  gint xsize, ysize;
  gint xpos, ypos;

  gint width  = gdk_screen_width ();
  gint height = gdk_screen_height ();

  /* Create a popup window with a frame. */
  window = gtk_window_new (GTK_WINDOW_POPUP);

  inside = gtk_calendar_new ();
  gtk_container_add (GTK_CONTAINER(window), inside);
  gtk_widget_show (inside);

  gtk_widget_realize (window);
  gtk_window_get_size (GTK_WINDOW(window), &xsize, &ysize);

  /* Calculate position based on dimensions. */
  if (panel->orientation == GTK_ORIENTATION_HORIZONTAL) {
    if (applet->place == PLACE_START)
      xpos = panel->spos + xsize;
    else
      xpos = width - panel->margin - xsize - 2;
      /* xpos = panel->epos - xsize; */
  
    if (panel->place == GTK_POS_BOTTOM)
      ypos = height - panel->height - ysize - 2;
    else
      ypos = panel->height;
  }
  else {
    if (applet->place == PLACE_START)
      ypos = panel->spos + ysize;
    else
      ypos = height - panel->margin - ysize - 2;
      /* ypos = panel->epos - ysize; */
  
    if (panel->place == GTK_POS_RIGHT)
      xpos = width - panel->width - xsize;
    else
      xpos = panel->width;
  }

  /* Move calendar popup window using calculated values. */
  gtk_window_move (GTK_WINDOW(window), xpos, ypos);

  return window;
} /* </calendar_new> */

/*
 * clock_settings_apply
 * clock_settings_cancel
 */
static void
clock_settings_apply (Modulus *applet)
{
  gboolean state = local_.enable;

  GlobalPanel *panel = applet->data;
  ClockConfig *clock = local_.clock;

  const char *spec;
  gchar *data;


  /* Show or hide pager widget according to user selection. */
  if (state) {
    gtk_widget_show (applet->widget);
    gtk_widget_show (clock->display);
  }
  else {
    gtk_widget_hide (clock->display);
    gtk_widget_hide (applet->widget);
  }

  /* Save configuration settings in cache. */
  spec =
"<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
"<applet name=\"%s\" icon=\"%s\"%s>\n"
" <settings format=\"%s\" />\n"
"</applet>\n";

  data = g_strdup_printf (spec, applet->name, applet->icon,
                          ((state) ? "" : " enable=\"no\""),
                          local_.format);
  vdebug (3, "\n%s\n", data);

  /* Replace configuration item. */
  configuration_replace (panel->config, data, "modules", "applet",applet->name);
  g_free (data);

  /* Memory was freed when original item was removed, read configuration. */
  clock_configuration_read (applet, clock);

  /* Save configuration cache data. */
  local_.format = clock->format;
  local_.enable = applet->enable;
} /* </clock_settings_apply> */

static void
clock_settings_cancel (Modulus *applet)
{
  gboolean state = applet->enable;
  ClockConfig *clock = local_.clock;

  /* Show or hide pager widget according to previous setting. */
  if (state) {
    gtk_widget_show (applet->widget);
    gtk_widget_show (clock->display);
  }
  else {
    gtk_widget_hide (clock->display);
    gtk_widget_hide (applet->widget);
  }

  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(local_.enable_toggle), state);

  state = (strncmp(clock->format, "%H", 2) == 0) ? TRUE : FALSE;
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(local_.hour24_toggle), state);

  state = (strlen(clock->format) > 5) ? TRUE : FALSE;
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(local_.seconds_toggle), state);

  /* Restore previous settings. */
  local_.format = clock->format;
  local_.enable = applet->enable;
} /* </clock_settings_cancel> */

/*
 * (private) clock_enable
 * (private) clock_24hour
 * (private) clock_seconds
 */
static void
clock_enable (GtkWidget *button, Modulus *applet)
{
  gboolean state = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(button));

  if (state != local_.enable) {
    GlobalPanel *panel = applet->data;
    PanelSetting *settings = panel->settings;

    /* Register callbacks for apply and cancel. */
    settings_save_enable (settings, TRUE);
    settings_set_agents (settings,
                         (gpointer)clock_settings_apply,
                         (gpointer)clock_settings_cancel,
                         NULL);

    local_.enable = state;
  }
} /* </clock_enable> */

static void
clock_24hour (GtkWidget *button, Modulus *applet)
{
  gboolean state = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(button));

  if (state != local_.show24hour) {
    GlobalPanel *panel = applet->data;
    PanelSetting *settings = panel->settings;

    /* Register callbacks for apply and cancel. */
    settings_save_enable (settings, TRUE);
    settings_set_agents (settings,
                         (gpointer)clock_settings_apply,
                         (gpointer)clock_settings_cancel,
                         NULL);

    if (local_.showseconds)
      local_.format = (state) ? "%H:%M:%S" : "%I:%M:%S %p";
    else
      local_.format = (state) ? "%H:%M" : "%I:%M %p";

    local_.show24hour = state;
  }
} /* </clock_24hour> */

static void
clock_seconds (GtkWidget *button, Modulus *applet)
{
  gboolean state = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(button));

  if (state != local_.showseconds) {
    GlobalPanel *panel = applet->data;
    PanelSetting *settings = panel->settings;

    /* Register callbacks for apply and cancel. */
    settings_save_enable (settings, TRUE);
    settings_set_agents (settings,
                         (gpointer)clock_settings_apply,
                         (gpointer)clock_settings_cancel,
                         NULL);

    if (state)
      local_.format = (local_.show24hour) ? "%H:%M:%S" : "%I:%M:%S %p";
    else
      local_.format = (local_.show24hour) ? "%H:%M" : "%I:%M %p";

    local_.showseconds = state;
  }
} /* </clock_seconds> */

/*
 * module_settings provides configuration pages
 */
GtkWidget *
module_settings (Modulus *applet, GlobalPanel *panel)
{
  ClockConfig *clock = local_.clock;	/* persistent settings */

  GtkWidget   *frame, *area, *layer, *widget;
  GtkWidget   *layout = gtk_vbox_new (FALSE, 2);

  gchar *caption = g_strdup_printf ("%s [%s]", _(applet->label), _("plugin"));

  /* Initialize private data structure singleton. */
  local_.enable = applet->enable;
  local_.format = clock->format;

  local_.show24hour  = FALSE;
  local_.showseconds = FALSE;

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

  widget = gtk_label_new ("");
  gtk_box_pack_start (GTK_BOX(area), widget, FALSE, TRUE, 6);
  gtk_widget_show (widget);

  /* Add option to enable or disable clock. */
  layer = gtk_hbox_new (FALSE, 6);
  gtk_box_pack_start (GTK_BOX(area), layer, FALSE, FALSE, 0);
  gtk_widget_show (layer);

  widget = gtk_label_new ("");
  gtk_box_pack_start (GTK_BOX(layer), widget, FALSE, TRUE, 6);
  gtk_widget_show (widget);

  widget = gtk_check_button_new_with_label (_("Enable"));
  gtk_box_pack_start (GTK_BOX(layer), widget, FALSE, TRUE, 0);
  gtk_widget_show (local_.enable_toggle = widget);

  g_signal_connect (G_OBJECT(widget), "toggled",
                    G_CALLBACK(clock_enable), applet);

  /* Add preview window area. */
  layer = gtk_hbox_new (FALSE, 4);
  gtk_box_pack_start (GTK_BOX(area), layer, FALSE, FALSE, 0);
  gtk_widget_show (layer);

  widget = gtk_label_new ("00:00");
  gtk_box_pack_start (GTK_BOX(layer), widget, FALSE, TRUE, 40);
  gtk_widget_modify_font(widget, pango_font_description_from_string("Sans 30"));
  gtk_widget_set_sensitive (widget, FALSE);
  local_.preview = widget;
  gtk_widget_show (widget);

  g_timeout_add (1000, (GSourceFunc)show_preview_time, clock);
  show_preview_time (clock);

  /* Add option to display 24 hour instead of 12 hour time. */
  layer = gtk_hbox_new (FALSE, 4);
  gtk_box_pack_start (GTK_BOX(area), layer, FALSE, FALSE, 0);
  gtk_widget_show (layer);

  widget = gtk_label_new ("");
  gtk_box_pack_start (GTK_BOX(layer), widget, FALSE, TRUE, 6);
  gtk_widget_show (widget);

  widget = gtk_check_button_new_with_label (
                _("Display 24 hour instead of 12 hour time"));
  gtk_box_pack_start (GTK_BOX(layer), widget, FALSE, TRUE, 0);
  local_.hour24_toggle = widget;
  gtk_widget_show (widget);

  if (strncmp(clock->format, "%H", 2) == 0) {
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(widget), TRUE);
    local_.show24hour = TRUE;
  }
  g_signal_connect (G_OBJECT(widget), "toggled",
                          G_CALLBACK(clock_24hour), applet);

  /* Add option to show seconds. */
  layer = gtk_hbox_new (FALSE, 4);
  gtk_box_pack_start (GTK_BOX(area), layer, FALSE, FALSE, 0);
  gtk_widget_show (layer);

  widget = gtk_label_new ("");
  gtk_box_pack_start (GTK_BOX(layer), widget, FALSE, TRUE, 6);
  gtk_widget_show (widget);

  widget = gtk_check_button_new_with_label (_("Show seconds"));
  gtk_box_pack_start (GTK_BOX(layer), widget, FALSE, TRUE, 0);
  local_.seconds_toggle = widget;
  gtk_widget_show (widget);

  if (strlen(clock->format) > 5) {
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(widget), TRUE);
    local_.showseconds = TRUE;
  }
  g_signal_connect (G_OBJECT(widget), "toggled",
                    G_CALLBACK(clock_seconds), applet);
  return layout;
} /* </module_settings> */

/*
 * Required methods
 */
void
module_init (Modulus *applet)
{
  GlobalPanel *panel = applet->data;
  ClockConfig *clock = g_new0 (ClockConfig, 1);

  /* Modulus structure initialization. */
  applet->name    = "clock";
  applet->icon    = "clock.png";
  applet->place   = PLACE_END;
  applet->space   = MODULI_SPACE_TASKBAR;
  applet->release = "1.2";
  applet->authors = Authors;

  /* Read configuration data for the splash screen. */
  clock_configuration_read (applet, clock);

  /* Initialize private data structure singleton. */
  memset(&local_, 0, sizeof (ClockPrivate));

  local_.tooltips = gtk_tooltips_new();
  local_.clock = clock;

  /* Construct the settings pages. */
  applet->label       = "Clock";
  applet->description = _(
"Displays a clock and calendar."
);
  applet->settings = settings_notebook_new (applet, panel,
                       _("Settings"), module_settings (applet, panel),
                       _("About"), settings_manpage_new (applet, panel),
                       -1);
} /* </module_init> */

void
module_open (Modulus *applet)
{
  GtkWidget *button, *display;
  GlobalPanel *panel = applet->data;
  ClockConfig *clock = local_.clock;

  /* Construct the user interface. */
  applet->widget = button = gtk_toggle_button_new();
  gtk_button_set_relief (GTK_BUTTON(button), GTK_RELIEF_NONE);

  g_signal_connect (G_OBJECT(button), "toggled",
                    G_CALLBACK (show_calendar), panel);

  display = clock->display = gtk_label_new ("--:--");
  gtk_container_add (GTK_CONTAINER(button), display);
  gtk_widget_show (display);

  /* Construct calendar widget before updating pack position. */
  local_.calendar = calendar_new (applet, panel);

  /* Update local_.enable_toggle on the settings page. */
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(local_.enable_toggle),
                                                  applet->enable);
  settings_save_enable (panel->settings, FALSE);

  /* Set up timer to update the display every 1 second. */
  g_timeout_add (1000, (GSourceFunc)show_time, applet);
} /* module_open */

void
module_close (Modulus *applet)
{
  gtk_widget_destroy (local_.calendar);
  gtk_widget_destroy (applet->widget);
} /* module_close */
