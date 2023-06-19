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
#include "gould.h"
#include "grabber.h"
#include "gwindow.h"
#include "xpmglyphs.h"
#include "docklet.h"

extern const char *Program;	/* see, gpanel.c and possibly others */
extern unsigned short debug;	/* must be declared in main program */

/*
 * Docklet implementation.
 */
static GtkObjectClass *parent = NULL;	/* see, docklet_class_init() */

/*
 * docklet_button_event - handler for button-press-event
 * docklet_config_event - handler for configure-event
 */
static bool
docklet_button_event (GtkWidget *widget,
                      GdkEventButton *event,
                      Docklet *instance)
{
  bool fire = true;		/* invoke (or not) instance->agent */

  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  if (event->type == GDK_BUTTON_PRESS || event->type == GDK_2BUTTON_PRESS) {
    if (event->button == 1) {		/* left button: move and drag */
      if (event->type != GDK_2BUTTON_PRESS) {
        gtk_window_begin_move_drag (GTK_WINDOW (instance->window),
                                    event->button,
                                    event->x_root,
                                    event->y_root,
                                    event->time);
        instance->moved = true;
        fire = FALSE;
      }
    }


    if (fire && instance->agent) {	/* call instance->agent if defined */
      instance->datum->event = (GdkEvent *)event;
      (*instance->agent) (instance->datum);
    }
  }
  return false;
} /* </docklet_button_event> */

static bool
docklet_config_event (GtkWidget *widget, GdkEvent *event, Docklet *instance)
{
  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  /* Call instance->agent if defined and the window has been moved. */
  if (instance->agent && instance->moved) {
    instance->datum->event = event;
    (*instance->agent) (instance->datum);
    instance->moved = false;
  }
  return false;
} /* </docklet_config_event> */

#if 0 /* experimental code */
static bool
docklet_expose_event (GtkWidget *widget,
                      GdkEventExpose *event,
                      Docklet *instance)
{
  return true;
}
#endif

/*
 * (private) docklet_class_init
 * (private) docklet_init
 */
static void
docklet_class_init (DockletClass *class)
{
  parent = g_type_class_peek_parent (class);
} /* </docklet_class_init> */

static void
docklet_init (Docklet *instance)
{
  /* nothing to do, ..presently */
} /* </docklet_init> */

/*
 * docklet_get_type
 */
GType
docklet_get_type (void)
{
  static GType type = 0;

  if (type == 0) {
    static const GTypeInfo info =
    {
	sizeof (DockletClass),
	NULL,		/* base_init */
        NULL,		/* base_finalize */
	(GClassInitFunc)docklet_class_init,
	NULL,		/* class_finalize */
	NULL,		/* class_data */
        sizeof (Docklet),
        0,		/* n_preallocs */
        (GInstanceInitFunc)docklet_init
    };

    type = g_type_register_static (GTK_TYPE_OBJECT, "Docklet", &info, 0);
  }
  return type;
} /* </docklet_get_type> */

/*
 * docklet_refresh
 */
bool
docklet_refresh (GtkWidget *canvas, GdkEventExpose *event, Docklet *instance)
{
  redraw_pixbuf(canvas, instance->render);
  return true;
} /* </docklet_refresh> */

/*
 * docklet_render
 */
GdkPixbuf *
docklet_render (Docklet *instance, GdkPixbuf *pixbuf)
{
  const gchar *text = instance->text;
  GtkWidget *widget = instance->window;
  GdkWindow *window = gdk_get_default_root_window ();

  PangoLayout *layout = gtk_widget_create_pango_layout (widget, _(text));
  GdkColormap *colormap = gdk_colormap_get_system ();

  gint width  = instance->width;
  gint height = instance->height;

  GdkGC     *gc;	/* GDK Graphic Context used for drawing */
  GdkColor  black;
  GdkColor  white;
  GdkColor  *fg;
  GdkColor  *bg;

  GdkPixmap *pixmap;
  GdkPixbuf *render;

  gint xsize, ysize;
  gint xpos, ypos;
  int wrap;

  gdk_color_black (colormap, &black);
  gdk_color_white (colormap, &white);

  fg = (instance->fg) ? instance->fg : &white;
  bg = (instance->bg) ? instance->bg : NULL;

  /* Obtain the PangoFontDescription to use, if instance->font */
  if (instance->font != NULL) {
    PangoFontDescription *fontdesc;
    fontdesc = pango_font_description_from_string (instance->font);
    pango_layout_set_font_description (layout, fontdesc);
    pango_font_description_free (fontdesc);
  }

  /* Obtain the pixel width and height of the text. */
  pango_layout_get_pixel_size (layout, &xsize, &ysize);

  if (instance->place == GTK_ORIENTATION_VERTICAL) {
    width *= 1.25;				/* make width 25% larger */
    xpos = (width - instance->width) / 2;	/* center pixbuf */

    if (xsize > width)
      height += (xsize / width) * ysize;
    else
      height += ysize;
  }
  else {
    width *= 3;					/* make width 300% larger */
    xpos = 0;
  }

  /* Create a pixmap for the drawing area. */
  gc = gdk_gc_new (window);

  if (bg != NULL) {
    gdk_gc_set_foreground (gc, bg);
    pixmap = gdk_pixmap_new (window, width, height, -1);
    gdk_draw_rectangle (pixmap, gc, TRUE, 0, 0, width, height);
  }
  else {
    render = xpm_pixbuf_scale (ICON_BLANK, width, height, NULL);
    gdk_pixbuf_render_pixmap_and_mask (render, &pixmap, NULL, 255);
  }

  /* Draw the instance->icon pixbuf passed. */
  draw_pixbuf (pixmap, pixbuf, gc, xpos, 0);

  /* Draw the instance->text. */
  if (instance->place == GTK_ORIENTATION_VERTICAL) {
    xpos = (xsize > width) ? 0 : (width-xsize) / 2;
    ypos = instance->height;
    wrap = 2 * width;
  }
  else {
    xpos = instance->width;
    ypos = (ysize > height) ? 0 : (height-ysize) / 2;
    wrap = 2 * width / 3;	/* adjust for width percentage increase */
  }

  pango_layout_set_width (layout, wrap * PANGO_SCALE);
  pango_layout_set_wrap (layout, PANGO_WRAP_WORD_CHAR);

  /* Draw the text with a drop shadow. */
  gdk_draw_layout_with_colors (pixmap, gc, xpos+1, ypos+1, layout, &black, bg);
  gdk_draw_layout_with_colors (pixmap, gc, xpos, ypos, layout, fg, bg);

  g_object_unref (layout);
  g_object_unref (gc);

  /* Convert drawable to a pixbuf. */
  render = gdk_pixbuf_get_from_drawable(NULL, pixmap, colormap,
                                        0, 0, 0, 0, width, height);
  g_object_unref (pixmap);

  return render;
} /* </docklet_render> */

/*
 * docklet_layout - construct main window layout contents
 */
GdkPixbuf *
docklet_layout (Docklet *instance, gint *xsize, gint *ysize)
{
  GdkBitmap *mask;
  GdkPixbuf *pixbuf;
  GdkPixmap *pixmap;

  gint width  = instance->width;
  gint height = instance->height;

  if (instance->icon != NULL)
    pixbuf = pixbuf_new_from_file_scaled (instance->icon, width, height);
  else
    pixbuf = xpm_pixbuf_scale (ICON_BROKEN, width, height, NULL);

  if (instance->text != NULL) {
    GdkPixbuf *render = docklet_render (instance, pixbuf);
    g_object_unref (pixbuf);
    pixbuf = render;
  }

  /* Dimensions may have changed, consult resulting pixbuf. */
  *xsize = gdk_pixbuf_get_width (pixbuf);
  *ysize = gdk_pixbuf_get_height (pixbuf);

  /* Shape drawable for a transparent look. */
  if (instance->visa == GTK_VISIBILITY_NONE) {
    gdk_pixbuf_render_pixmap_and_mask (pixbuf, &pixmap, &mask, 255);
    if (mask == NULL) mask = create_mask_from_pixmap (pixmap, *xsize, *ysize);
    gtk_widget_shape_combine_mask (instance->window, mask, 0, 0);
  }
  return pixbuf;
} /* </docklet_layout> */

/*
 * docklet_new - constructor
 */
Docklet *
docklet_new (GdkWindowTypeHint hint,
             gint width, gint height,
             gint xpos,  gint ypos,
             GtkOrientation place,
             GtkVisibility visa,
             const gchar *icon,
             const gchar *text,
             const gchar *font,
             GdkColor *fg,
             GdkColor *bg)
{
  Docklet   *instance = gtk_type_new (DOCKLET_TYPE);
  GtkWidget *dispatch = gtk_event_box_new ();
  GtkWidget *widget;
  GtkWidget *inside;
  gint xsize, ysize;

  /* Construct the main window and add the GdkEventBox. */
  widget = sticky_window_new (hint, width, height, xpos, ypos);
  gtk_container_add (GTK_CONTAINER (widget), dispatch);

  /* Change cursor entering GtkEventBox */
  gtk_widget_show (dispatch);
  gtk_widget_realize (dispatch);	/* widget must be realized */
  gdk_window_set_cursor (dispatch->window, gdk_cursor_new (GDK_HAND2));

  /* Mouse click to drag and callbacks for the main window. */
  instance->buttonHandler = g_signal_connect (
                              G_OBJECT (widget), "button-press-event",
                              G_CALLBACK (docklet_button_event), instance);

  instance->windowHandler = g_signal_connect (
                              G_OBJECT (widget), "configure-event",
                              G_CALLBACK (docklet_config_event), instance);

  /* FIXME: experimental code 
  g_signal_connect (G_OBJECT (widget), "expose_event",
                    G_CALLBACK (docklet_expose_event), instance);
  */

  /* Save position, dimensions and other key data. */
  instance->window = widget;
  instance->layout = dispatch;
  instance->height = height;
  instance->width  = width;
  instance->place  = place;
  instance->visa   = visa;
  instance->xpos   = xpos;
  instance->ypos   = ypos;
  instance->icon   = icon;
  instance->text   = text;
  instance->font   = font;
  instance->fg     = fg;
  instance->bg     = bg;

  /* Application window main layout. */
  instance->render = docklet_layout (instance, &xsize, &ysize);
  instance->inside = inside = gtk_drawing_area_new ();

  g_signal_connect (G_OBJECT (inside), "expose_event",
                    G_CALLBACK (docklet_refresh), instance);

  /*
   * GTK_VISIBILITY_NONE => transparent
   * GTK_VISIBILITY_PARTIAL => flat decoration
   * GTK_VISIBILITY_FULL => bevel shadow out
  */
  if (visa == GTK_VISIBILITY_NONE)
    gtk_container_add (GTK_CONTAINER(dispatch), inside);
  else {
    GtkWidget *frame = gtk_frame_new (NULL);

    if (visa == GTK_VISIBILITY_FULL)
      gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_OUT);

    gtk_container_add (GTK_CONTAINER (dispatch), frame);
    gtk_container_add (GTK_CONTAINER(frame), inside);
    gtk_widget_show (frame);
  }

  /* Caller is responsible for making visible (or not) instance->window */
  gtk_widget_show (inside);

  /* Adjust instance->window dimensions, if needed. */
  if (xsize != width || ysize != height) {
    gtk_widget_set_size_request (widget, xsize, ysize);
    instance->width  = xsize;
    instance->height = ysize;
  }

  return instance;
} /* </docklet_new> */

/*
 * docklet_update
 */
void
docklet_update (Docklet *instance, const gchar *icon, const gchar *text)
{
  GdkBitmap *mask;
  GdkPixmap *pixmap;
  GdkPixbuf *pixbuf = NULL;
  gint width, height;

  instance->icon = icon;	/* potentially a different icon */
  instance->text = text;	/* potentially different text */

  if (icon != NULL && g_file_test(icon, G_FILE_TEST_EXISTS)) {
    GError *error = NULL;
    pixbuf = gdk_pixbuf_new_from_file (icon, &error);

    if (pixbuf == NULL) {
      g_printerr("%s: %s\n", Program, error->message);
      pixbuf = xpm_pixbuf (ICON_BROKEN, NULL);
    }
  }

  if (instance->text != NULL) {
    GdkPixbuf *render;

    instance->width  = gdk_pixbuf_get_width (pixbuf);
    instance->height = gdk_pixbuf_get_height (pixbuf);
    render = docklet_render (instance, pixbuf);

    g_object_unref (pixbuf);
    pixbuf = render;
  }

  /* Dimensions may have changed, consult resulting pixbuf. */
  width  = gdk_pixbuf_get_width (pixbuf);
  height = gdk_pixbuf_get_height (pixbuf);

  if (width != instance->width || height != instance->height) {
    instance->width  = width;
    instance->height = height;
    gtk_widget_set_size_request (instance->window, width, height);
  }

  /* Shape drawable for a transparent look. */
  if (instance->visa == GTK_VISIBILITY_NONE) {
    gdk_pixbuf_render_pixmap_and_mask (pixbuf, &pixmap, &mask, 255);
    if (mask == NULL) mask = create_mask_from_pixmap (pixmap, width, height);
    gtk_widget_shape_combine_mask (instance->window, mask, 0, 0);
  }

  g_object_unref (instance->render);
  redraw_pixbuf(instance->inside, pixbuf);  /* redraw */
  instance->render = pixbuf;
} /* </docklet_update> */

/*
 * docklet_set_callback - specify (set) user callback agent
 */
void
docklet_set_callback (Docklet *instance, GtkFunction callback, gpointer data)
{
  DockletDatum *datum = g_new0 (DockletDatum, 1);

  /* Initialize persistent DockletDatum fields. */
  datum->docklet = instance;
  datum->payload = data;

  /* Save in the Docklet instance attributes. */
  instance->agent = callback;
  instance->datum = datum;
} /* </docklet_set_callback> */

/*
 * docklet_set_cursor - set/change cursor
 */
void
docklet_set_cursor (Docklet *instance, GdkCursorType cursor)
{
  gdk_window_set_cursor (instance->layout->window, gdk_cursor_new (cursor));
} /* </docklet_set_cursor> */
