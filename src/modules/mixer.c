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

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/soundcard.h>
#include <alsa/asoundlib.h>

#define _SCALE 100		/* slider bar long dimension */

extern const char *Program;     /* see, gpanel.c */
const char *Release = "1.2.4";


/*
* Data structures used by this module.
*/
typedef struct _MixerConfig  MixerConfig;
typedef struct _MixerPrivate MixerPrivate;

struct _MixerConfig
{
  const gchar *device;
  guint volume;			/* main volume 0..100 initial setting */
};

struct _MixerPrivate
{
  gboolean enable;		/* enable (or not) user selection */

  MixerConfig cache;		/* cache for configuration data */
  MixerConfig *mixer;		/* applet configuration data */

  GtkWidget *enable_toggle;	/* enable (or not) check box */
  GtkWidget *device_entry;	/* audio device to open entry */
  GtkWidget *volume_scale;	/* initial volume scale */

  GtkWidget *icon_active;	/* icon image for volume control */
  GtkWidget *icon_passive;	/* icon image for no control */

  GtkWidget *slider;		/* volume control slider window */
  int device;			/* device file descriptor */
};

static MixerPrivate local_;	/* private global structure singleton */


/**
 * ALSA support [EXPRIMENTAL]
*/
int SetAlsaMasterVolume(long volume)
{
  const char *card = "default";
  const char *selem_name = "Master";

  snd_mixer_t *handle;
  snd_mixer_selem_id_t *sid;
  int status = 0;

  snd_mixer_open(&handle, 0);
  snd_mixer_attach(handle, card);
  snd_mixer_selem_register(handle, NULL, NULL);
  snd_mixer_load(handle);

  snd_mixer_selem_id_alloca(&sid);
  snd_mixer_selem_id_set_index(sid, 0);
  snd_mixer_selem_id_set_name(sid, selem_name);
  snd_mixer_elem_t* elem = snd_mixer_find_selem(handle, sid);

  if (elem) {
    long min, max;
    snd_mixer_selem_get_playback_volume_range(elem, &min, &max);
    snd_mixer_selem_set_playback_volume_all(elem, volume * max / 100);
  }
  else {
    vdebug(1, "snd_mixer_find_selem() failed!\n");
    status = 1;
  }

  snd_mixer_close(handle);
  return status;
}

/*
 * mixer_default_device_name
 */
static char *mixer_default_device_name()
{
  static char *name = "/dev/mixer";

  int card = -1;	/* card == -1 tells ALSA to fetch the first card */
  register int err = snd_card_next(&card);

  if (err < 0) {
    vdebug(2, "Can't get the next card number: %s\n", snd_strerror(err));
  }
  else {
    snd_ctl_t *handle;
    static char str[64];
    sprintf(str, "hw:%i", card);

    if ((err = snd_ctl_open(&handle, str, 0)) < 0) {
      vdebug(2, "Can't open card %i: %s\n", card, snd_strerror(err));
    }
    else {
      snd_ctl_card_info_t *info;

      /* We need to get a snd_ctl_card_info_t. Just alloc it on the stack. */
      snd_ctl_card_info_alloca(&info);

      if ((err = snd_ctl_card_info(handle, info)) < 0) {
        vdebug(2, "Can't get info for card %i: %s\n", card, snd_strerror(err));
      }
      else {
        strcpy(str, snd_ctl_card_info_get_name(info));
        vdebug(2, "Card %i = %s\n", card, name);
        name = str;
      }

      /* Close the card's control interface after we're done with it. */
      snd_ctl_close(handle);
    }
    snd_config_update_free_global();
  }
  return name;
}

/*
 * mixer_configuration_read
 */
static void
mixer_configuration_read (Modulus *applet, MixerConfig *mixer)
{
  GlobalPanel *panel = applet->data;
  ConfigurationNode *chain = configuration_find (panel->config, "modules");
  ConfigurationNode *item;

  const gchar *attrib;


  /* Configuration for the mixer applet. */
  applet->enable = TRUE;
  mixer->device  = mixer_default_device_name();

  while ((item = configuration_find (chain, "applet")) != NULL) {
    attrib = configuration_attrib (item, "name");

    if (strcmp(attrib, applet->name) == 0) {
      if ((attrib = configuration_attrib (item, "enable")) != NULL)
        if (strcmp(attrib, "off") == 0 || strcmp(attrib, "no") == 0)
          applet->enable = FALSE;

      if ((attrib = configuration_attrib (item, "icon")) != NULL)
        applet->icon = attrib;

      if ((attrib = configuration_attrib (item, "place")) != NULL) {
        if (strcmp(attrib, "START") == 0)
          applet->place = PLACE_START;
        else if (strcmp(attrib, "END") == 0)
          applet->place = PLACE_END;
      }

      if ((item = configuration_find (item, "settings")) != NULL) {
        if ((attrib = configuration_attrib (item, "device")) != NULL)
          mixer->device = attrib;

        if ((attrib = configuration_attrib (item, "volume")) != NULL)
          mixer->volume = atoi(attrib);
      }
      break;
    }
    chain = configuration_find_next (chain, "applet");
  }
} /* </mixer_configuration_read> */

/*
 * mixer_settings_apply
 * mixer_settings_cancel
 */
static void
mixer_settings_apply (Modulus *applet)
{
  GlobalPanel *panel = applet->data;
  MixerConfig *mixer = local_.mixer;

  const char *spec;
  gchar *data;


  /* Save configuration settings from singleton cache settings. */
  memcpy(mixer, &local_.cache, sizeof(MixerConfig));
  applet->enable = local_.enable;

  /* Save configuration settings in cache. */
  spec =
"<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
"<applet name=\"%s\" icon=\"%s\"%s>\n"
" <settings device=\"%s\" volume=\"%d\" />\n"
"</applet>\n";

  data = g_strdup_printf (spec, applet->name, applet->icon,
                          ((applet->enable) ? "" : " enable=\"no\""),
                          mixer->device, mixer->volume);
  vdebug (3, "\n%s\n", data);

  /* Replace configuration item. */
  configuration_replace (panel->config, data, "modules", "applet",applet->name);
  g_free (data);

  /* Memory was freed when original item was removed, read configuration. */
  mixer_configuration_read (applet, mixer);

  /* Save configuration to singleton settings cache. */
  memcpy(&local_.cache, mixer, sizeof(MixerConfig));

  /* Show or hide pager widget according to user selection. */
  if (applet->enable)
    gtk_widget_show (applet->widget);
  else
    gtk_widget_hide (applet->widget);
} /* <mixer_settings_apply> */

static void
mixer_settings_cancel (Modulus *applet)
{
  MixerConfig *mixer = local_.mixer;	/* persistent settings */

  memcpy(&local_.cache, mixer, sizeof(MixerConfig));
  local_.enable = applet->enable;

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(local_.enable_toggle),
                                applet->enable);

  gtk_entry_set_text (GTK_ENTRY(local_.device_entry), mixer->device);
  gtk_range_set_value (GTK_RANGE(local_.volume_scale), mixer->volume);
} /* <mixer_settings_apply> */

/*
 * mixer_enable callback
 */
static void
mixer_enable (GtkWidget *button, Modulus *applet)
{
  gboolean state = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(button));

  if (state != local_.enable) {
    GlobalPanel *panel = applet->data;
    PanelSetting *settings = panel->settings;

    /* Register callbacks for apply and cancel. */
    settings_save_enable (settings, TRUE);
    settings_set_agents (settings,
                         (gpointer)mixer_settings_apply,
                         (gpointer)mixer_settings_cancel,
                         NULL);

    local_.enable = state;
  }
} /* </mixer_enable> */

/*
 * mixer_device callback
 */
static void
mixer_device (GtkEntry *entry, GdkEventKey *event, Modulus *applet)
{
  /* [2008/03/14] Code for changing audio device not-yet-implemented. */
} /* </mixer_device> */

/*
 * mixer_volume callback
 */
static void
mixer_volume (GtkRange *range, Modulus *applet)
{
  GlobalPanel *panel = applet->data;
  PanelSetting *settings = panel->settings;

  settings_save_enable (settings, TRUE);
  settings_set_agents (settings,
                       (gpointer)mixer_settings_apply,
                       (gpointer)mixer_settings_cancel,
                       NULL);

  local_.cache.volume = (int)gtk_range_get_value (range);
} /* </mixer_volume> */

/*
 * (private) volume_adjust
 * (private) volume_control
 */
static gboolean
volume_adjust (GtkAdjustment *adjust, Modulus *applet)
{
  gboolean done = FALSE;
  gint volume = (gint)adjust->value;

  if (local_.device > 0) {
    volume += (gint)adjust->value << 8;

    if ((ioctl(local_.device, SOUND_MIXER_WRITE_VOLUME, &volume)) >= 0) {
      local_.cache.volume = adjust->value;
      done = TRUE;
    }
  }
  else {
    if (SetAlsaMasterVolume(volume) == 0) {
      local_.cache.volume = adjust->value;
      done = TRUE;
    }
    vdebug (2, "volume_adjust::setAlsaMasterVolume( %d )\n", volume);
  }
  return done;
} /* </volume_adjust> */

static GtkWidget *
volume_button_new (Modulus *applet)
{
  GtkWidget *button = gtk_volume_button_new ();
  GtkAdjustment *adjust = (GtkAdjustment *)gtk_adjustment_new (_SCALE, 0, _SCALE+1, 1, 10, 1);

  gtk_scale_button_set_adjustment(GTK_SCALE_BUTTON(button), GTK_ADJUSTMENT(adjust));
  gtk_scale_button_set_value(GTK_SCALE_BUTTON(button), (local_.mixer)->volume);

  g_signal_connect (G_OBJECT(adjust), "value_changed",
                          G_CALLBACK(volume_adjust), applet);
  local_.slider = button;

  return button;
}

static void
volume_control (GtkWidget *widget, Modulus *applet)
{
  GtkWidget *slider = local_.slider;
  g_return_if_fail (GTK_IS_WIDGET (widget));

  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(widget)))
    gtk_widget_show (slider);
  else
    gtk_widget_hide (slider);
} /* </volume_control> */

#if 0
/*
 * slider_new
 */
static GtkWidget *
slider_new (Modulus *applet)
{
  GtkAdjustment *adjust;
  GtkWidget *frame, *range, *window;

  GlobalPanel *panel = applet->data;
  guint iconsize = panel->icons->size;

  gint width  = gdk_screen_width ();
  gint height = gdk_screen_height ();

  gint xpos, ypos;
  int volume = 0;


  /* ioctl to obtain volume */
  if (local_.device > 0) {
    ioctl (local_.device, SOUND_MIXER_READ_VOLUME, &volume);
  }

  adjust = (GtkAdjustment*)gtk_adjustment_new (_SCALE, 0, _SCALE+1, 1, 10, 1);
  gtk_adjustment_set_value (adjust, volume >> 8);

  /* Calculate position based on dimensions. */
  if (panel->orientation == GTK_ORIENTATION_HORIZONTAL) {
     if (applet->place == PLACE_START)
       xpos = panel->spos + iconsize / 2;
     else
       xpos = panel->epos;
  
     if (panel->place == GTK_POS_BOTTOM)
       ypos = height - panel->height - _SCALE - 4;
     else
       ypos = panel->height;
  
     range = gtk_vscale_new (adjust);
     gtk_widget_set_size_request (GTK_WIDGET(range), iconsize, _SCALE);
   }
   else {
     if (applet->place == PLACE_START)
       ypos = panel->spos + iconsize;
     else
       ypos = panel->epos + iconsize / 2;
  
     if (panel->place == GTK_POS_RIGHT)
       xpos = width - panel->width - _SCALE - 4;
     else
       xpos = panel->width;
  
     range = gtk_hscale_new (adjust);
     gtk_widget_set_size_request (GTK_WIDGET(range), _SCALE, iconsize);
   }

  if (panel->place == GTK_POS_BOTTOM || panel->place == GTK_POS_RIGHT)
    gtk_range_set_inverted (GTK_RANGE(range), TRUE);

  gtk_scale_set_digits (GTK_SCALE (range), 0);
  gtk_scale_set_draw_value (GTK_SCALE (range), FALSE);

  /* Create a popup window with a frame. */
  window = gtk_window_new (GTK_WINDOW_POPUP);

  gtk_window_set_keep_above (GTK_WINDOW(window), TRUE);
  gtk_window_move (GTK_WINDOW(window), xpos, ypos);

  frame = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME(frame), GTK_SHADOW_OUT);
  gtk_container_add (GTK_CONTAINER(window), frame);
  gtk_widget_show (frame);

  gtk_container_add (GTK_CONTAINER(frame), range);
  g_signal_connect (G_OBJECT(adjust), "value_changed",
                          G_CALLBACK(volume_adjust), applet);
  gtk_widget_show (range);

  return window;
} /* </slider_new> */
#endif

/*
 * access_mixer_device dual purpose open mixer device and retry same
 */
static int
access_mixer_device (Modulus *applet)
{
  MixerConfig *mixer = local_.mixer;
  unsigned int volume = mixer->volume;

  if (SetAlsaMasterVolume(volume) == 0) {
    vdebug (2, "access_mixer_device::SetAlsaMasterVolume( %d )\n", volume);
    local_.device = 0;
  }
  else if ((local_.device = open(mixer->device, O_RDWR)) > 0) {
    volume += mixer->volume << 8;
    ioctl(local_.device, SOUND_MIXER_WRITE_VOLUME, &volume);
  }

  /* FIXME!
  if (local_.device >= 0) {
    gtk_button_set_image (GTK_BUTTON(applet->widget), local_.icon_active);

    g_signal_connect (G_OBJECT(applet->widget), "toggled",
                      G_CALLBACK(volume_control), applet);
  }
  */
  return local_.device;
} /* </access_mixer_device> */

/*
 * module_settings provides configuration pages
 */
GtkWidget *
module_settings (Modulus *applet, GlobalPanel *panel)
{
  MixerConfig *mixer = local_.mixer;	/* persistent settings */

  GtkWidget *area, *frame, *layer, *widget;
  GtkWidget   *layout = gtk_vbox_new (FALSE, 2);

  gchar *caption = g_strdup_printf ("%s [%s]", _(applet->label), _("plugin"));

  /* Initialize private data structure singleton settings cache. */
  memcpy(&local_.cache, mixer, sizeof(MixerConfig));
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

  widget = gtk_label_new ("");
  gtk_box_pack_start (GTK_BOX(area), widget, FALSE, TRUE, 6);
  gtk_widget_show (widget);

  /* Add option to enable or disable mixer. */
  layer = gtk_hbox_new (FALSE, 6);
  gtk_box_pack_start (GTK_BOX(area), layer, FALSE, FALSE, 0);
  gtk_widget_show (layer);

  widget = gtk_label_new ("");
  gtk_box_pack_start (GTK_BOX(layer), widget, FALSE, TRUE, 6);
  gtk_widget_show (widget);

  widget = gtk_check_button_new_with_label (_("Enable"));
  gtk_box_pack_start (GTK_BOX(layer), widget, FALSE, TRUE, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(widget), applet->enable);
  gtk_widget_show (local_.enable_toggle = widget);

  /* FIXME!
  g_signal_connect (G_OBJECT(widget), "toggled",
                    G_CALLBACK(mixer_enable), applet);
  */

  if (applet->icon != NULL) {
    const gchar *icon = icon_path_finder (panel->icons, applet->icon);

    if (icon != NULL) {
      const guint size = 32;
      GtkWidget *image = image_new_from_file_scaled (icon, size, size);

      gtk_box_pack_end (GTK_BOX(layer), image, FALSE, FALSE, 12);
      gtk_widget_show (image);
    }
  }

  /* Add entry for audio device to open. */
  layer = gtk_hbox_new (FALSE, 6);
  gtk_box_pack_start (GTK_BOX(area), layer, FALSE, FALSE, 0);
  gtk_widget_show (layer);

  caption = g_strdup_printf ("\t%s:", _("Audio Device"));
  widget = gtk_label_new (caption);
  gtk_box_pack_start (GTK_BOX(layer), widget, FALSE, TRUE, 2);
  gtk_widget_show (widget);
  g_free (caption);

  widget = local_.device_entry = gtk_entry_new();
  gtk_entry_set_text (GTK_ENTRY(widget), mixer->device);
  gtk_box_pack_start (GTK_BOX(layer), widget, FALSE, FALSE, 0);
  gtk_entry_set_max_length (GTK_ENTRY(widget), 40);
  gtk_widget_show (widget);

  g_signal_connect (G_OBJECT (widget), "key_press_event",
                    G_CALLBACK (mixer_device), applet);

  /* [2008/03/14] Code for changing audio device not-yet-implemented. */
  gtk_entry_set_editable (GTK_ENTRY(widget), FALSE);

  /* Add scale for initial volume setting. */
  layer = gtk_hbox_new (FALSE, 6);
  gtk_box_pack_start (GTK_BOX(area), layer, FALSE, FALSE, 0);
  gtk_widget_show (layer);

  caption = g_strdup_printf ("\t%s:", _("Initial volume setting"));
  widget = gtk_label_new (caption);
  gtk_box_pack_start (GTK_BOX(layer), widget, FALSE, TRUE, 2);
  gtk_widget_show (widget);
  g_free (caption);

  widget = local_.volume_scale = gtk_hscale_new_with_range (0, _SCALE, 1);
  gtk_box_pack_start (GTK_BOX(layer), widget, FALSE, TRUE, 0);
  gtk_range_set_value (GTK_RANGE(widget), mixer->volume);
  gtk_widget_set_size_request (GTK_WIDGET(widget), _SCALE, 40);
  gtk_scale_set_digits (GTK_SCALE (widget), 0);
  gtk_widget_show (widget);

  g_signal_connect (G_OBJECT (widget), "value_changed",
                    G_CALLBACK (mixer_volume), applet);
  return layout;
} /* </module_settings> */

/*
 * Required methods
 */
void
module_init (Modulus *applet)
{
  GlobalPanel *panel = applet->data;
  MixerConfig *mixer = g_new0 (MixerConfig, 1);

  /* Modulus structure initialization. */
  applet->name    = "mixer";
  applet->icon    = "mixer.png";
  applet->place   = PLACE_END;
  applet->space   = MODULI_SPACE_TASKBAR;
  applet->release = Release;
  applet->authors = Authors;

   /* Read configuration data for the mixer applet. */
  mixer_configuration_read (applet, mixer);

  /* Initialize private data structure singleton. */
  memset(&local_, 0, sizeof (MixerPrivate));
  local_.mixer = mixer;

  /* Construct the settings pages. */
  applet->label       = "Volume";
  applet->description = _(
"Allows volume control of the audio device."
);
  applet->settings = settings_notebook_new (applet, panel,
                       _("Settings"), module_settings (applet, panel),
                       _("About"), settings_manpage_new (applet, panel),
                       -1);
} /* </module_init> */

/*
 * module_open - construct user interface
 *
 * [TDB] display local_.icon_passive when mixer device is not available
 */
void
module_open (Modulus *applet)
{
  GlobalPanel *panel = applet->data;
  PanelIcons  *icons = panel->icons;
  unsigned int iconsize = icons->size;
  const gchar *icon;

  /* Construct the icon images for volume control and no control. */
  icon = icon_path_finder (icons, applet->icon);
  local_.icon_active = image_new_from_file_scaled (icon, iconsize, iconsize);

  icon = icon_path_finder (icons, "mute.png");
  local_.icon_passive = image_new_from_file_scaled (icon, iconsize, iconsize);

  /* Construct the user interface and access mixer device. */
  GtkWidget  *layout = gtk_toggle_button_new();
  gtk_button_set_relief (GTK_BUTTON(layout), GTK_RELIEF_NONE);

  /* Construct volume control slider widget. */
  applet->widget = volume_button_new (applet);

  /* FIXME!
  if (access_mixer_device (applet) < 0) {
    gtk_button_set_image (GTK_BUTTON(layout), local_.icon_passive);
    g_timeout_add (10000, (GSourceFunc)access_mixer_device, applet);
  }
  */
} /* </module_open> */

void
module_close (Modulus *applet)
{
  if(local_.device > 0) close(local_.device);
  gtk_widget_destroy (applet->widget);
} /* </module_close> */
