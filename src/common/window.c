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
#include "window.h"
#include "grabber.h"
#include "xpmglyphs.h"

#include <X11/Xutil.h>
#include <X11/extensions/shape.h>

extern const char *Program;	/* see, gpanel.c and possibly others */
extern unsigned short debug;	/* must be declared in main program */


/*
 * apply_gould_theme
 */
gboolean
apply_gtk_theme (const char *confile)
{
  GKeyFile *config = g_key_file_new();
  gboolean done = g_key_file_load_from_file(config, confile,
                                            G_KEY_FILE_KEEP_COMMENTS, NULL);
  if (done) {
    char *theme = g_key_file_get_string(config, "display", "gtk_theme", NULL);

    if (theme) {
      GtkSettings* settings = gtk_settings_get_default();
      g_object_set(settings, "gtk-theme-name", theme, NULL);
      g_free (theme);
    }
    else {
      done = FALSE;
    }
  }

  return done;
} /* apply_gould_theme */

/*
 * colormap_from_pixmap
 */
GdkColormap *
colormap_from_pixmap (GdkPixmap *pixmap)
{
  GdkColormap *cmap = gdk_drawable_get_colormap (pixmap);

  if (cmap)
    g_object_ref (G_OBJECT (cmap));

  if (cmap == NULL) {
    if (gdk_drawable_get_depth (pixmap) == 1) {
      cmap = NULL;	/* try null cmap */
    }
    else {		/* try system cmap */
      GdkScreen *screen = gdk_drawable_get_screen (GDK_DRAWABLE (pixmap));
      cmap = gdk_screen_get_system_colormap (screen);
      g_object_ref (G_OBJECT (cmap));
    }
  }

  /* Be sure we aren't going to blow up due to visual mismatch */
  if (cmap &&
      (gdk_colormap_get_visual(cmap)->depth != gdk_drawable_get_depth(pixmap)))
    cmap = NULL;
  
  return cmap;
} /* </colormap_from_pixmap */

/*
 * create_mask_from_pixmap - create the bitmap mask for a pixmap.
 */
GdkBitmap *
create_mask_from_pixmap (GdkPixmap *pixmap, gint width, gint height)
{
  GdkBitmap *mask = NULL;
  GdkImage *image = gdk_drawable_get_image (pixmap, 0, 0, width, height);
  guint32 pixelmask = gdk_image_get_pixel (image, 0, 0);

  /* Bitmap mask, one bit per pixel, each row starts on a byte boundary. */
  size_t length = (width + 7) / 8 * height;  /* bits per line * height */
  guint8 *data = g_new (guint8, length);
  memset (data, 255, length);	/* set bits are unmasked */
  int row, col;
  int idx = 0;

  for (row = 0; row < height; row++) {
    for (col = 0; col < width; col++, idx++) {
      //if (debug) printf("%x ", gdk_image_get_pixel (image, col, row));
      if (gdk_image_get_pixel (image, col, row) == pixelmask)
        data[idx >> 3] ^= 1 << (idx & 7);
    }
    //if (debug) printf("\n");
    idx = (idx + 7) & ~7u;	/* move index to next byte boundary */
  }

  mask = gdk_bitmap_create_from_data (NULL, (gchar*)data, width, height);
  g_free (data);

  return mask;
} /* </create_mask_from_pixmap> */

/*
 * get_label_size
 */
GtkAllocation *
get_label_size (GtkWidget *label)
{
  gint xsize, ysize;
  GtkAllocation *allocation = g_new0(GtkAllocation, 1);

  PangoLayout *layout = gtk_label_get_layout (GTK_LABEL(label));
  pango_layout_get_pixel_size (layout, &xsize, &ysize);
  allocation->height = ysize;
  allocation->width = xsize;

  return allocation;
} /* </get_label_size> */

/*
 * image_new_from_file_scaled - returns a scaled image given a file name
 */
GtkWidget *
image_new_from_file_scaled (const gchar *file, gint width, gint height)
{
  GtkWidget *image = NULL;
  GdkPixbuf *pixbuf = NULL;
 
  if (file != NULL && g_file_test(file, G_FILE_TEST_EXISTS))
    pixbuf = pixbuf_new_from_file_scaled (file, width, height);

  if (pixbuf == NULL)
    pixbuf = xpm_pixbuf_scale (ICON_BROKEN, width, height, NULL);

  image = gtk_image_new_from_pixbuf (pixbuf);
  g_object_unref (pixbuf);

  return image;
} /* </image_new_from_file_scaled> */

GtkWidget *
image_new_from_path_scaled (GList *paths, const gchar *icon,
                            guint width, guint height)
{
  const gchar *file = path_finder (paths, icon);
  return image_new_from_file_scaled (file, width, height);
} /* </image_new_from_path_scaled> */

/*
 * pixbuf_new_from_file_scaled - return scaled GdkPixbuf from a file
 */
GdkPixbuf *
pixbuf_new_from_file_scaled (const gchar *file, gint width, gint height)
{
  GdkPixbuf *render = NULL;
  GdkPixbuf *pixbuf;

  if (file != NULL && g_file_test(file, G_FILE_TEST_EXISTS)) {
    GError *error  = NULL;
    render = gdk_pixbuf_new_from_file(file, &error);

    if (render == NULL) {
      g_printerr("%s: %s\n", Program, error->message);
      render = xpm_pixbuf (ICON_BROKEN, NULL);
    }
  }

  if (width > 0 || height > 0) {
    pixbuf = pixbuf_scale (render, width, height);
    g_object_unref (render);

    return pixbuf;
  }
  return render;
} /* </pixbuf_new_from_file_scaled> */

/*
 * pixbuf_new_from_path_scaled
 */
GdkPixbuf *
pixbuf_new_from_path_scaled (GList *paths, const gchar *icon,
                             guint width, guint height)
{
  const gchar *file = path_finder (paths, icon);
  return pixbuf_new_from_file_scaled (file, width, height);
} /* </pixbuf_new_from_path_scaled> */

/*
 * pixbuf_scale - scale a GdkPixbuf to given width and height
 */
GdkPixbuf *
pixbuf_scale (GdkPixbuf *pixbuf, gint width, gint height)
{
  return gdk_pixbuf_scale_simple(pixbuf, width, height, GDK_INTERP_BILINEAR);
} /* </pixbuf_scale> */

/*
 * redraw_pixbuf - redraw GdkPixbuf of a canvas drawing area
 */
gboolean
redraw_pixbuf (GtkWidget *canvas, GdkPixbuf *image)
{
  gint xsize, ysize;
  gboolean vote = FALSE;

  g_return_val_if_fail (GDK_IS_DRAWABLE(canvas->window), FALSE);

  /* Obtain width and height of the canvas area. */
  gdk_drawable_get_size (canvas->window, &xsize, &ysize);
  gdk_window_clear_area (canvas->window, 0, 0, xsize, ysize);

  if (image) {
    GdkPixbuf *pixbuf;
    gboolean   scaled;	/* we may create a new scaled pixbuf */

    gint width  = gdk_pixbuf_get_width (image);
    gint height = gdk_pixbuf_get_height (image);
    gint xorg, yorg;                            /* start (x,y) coordinates */

    vdebug (2, "redraw_pixbuf width => %d, height => %d\n", width, height);

    /* Adjust width and height by scale factor. */
    if (width > xsize || height > ysize) {
      float factor = scale_factor((float)width / xsize, (float)height / ysize);

      width  = (float)width / factor;
      height = (float)height / factor;
      pixbuf = pixbuf_scale (image, width, height);
      scaled = TRUE;
    }
    else {
      pixbuf = image;
      scaled = FALSE;
    }

    /* Adjust image (x,y) start coordinates. */
    xorg = abs(xsize - width) / 2;
    yorg = abs(ysize - height) / 2;

    /* Draw scaled pixbuf onto the canvas area. */
    gdk_draw_pixbuf (canvas->window,
                     canvas->style->fg_gc[GTK_WIDGET_STATE (canvas)],
                     pixbuf, 0, 0, xorg, yorg, width, height,
                     GDK_RGB_DITHER_NONE, 0, 0);
    if (scaled)
      g_object_unref (pixbuf);

    vote = TRUE;
  }
  return vote;
} /* </redraw_pixbuf> */

/*
 * set_background_pixbuf - set background pixmap of window using pixbuf
 */
GError *
set_background_pixbuf (GdkWindow *window, GdkPixbuf *pixbuf)
{
  GdkGC     *gc;
  GdkPixbuf *image;
  GdkPixmap *pixmap;

  gint width, height;
  gint xres, yres;


  /* Get the window and image sizes */
  gdk_window_get_size (window, &xres, &yres);

  height = gdk_pixbuf_get_height (pixbuf);
  width  = gdk_pixbuf_get_width (pixbuf);

  /* Scale GdkPixbuf *image as needed */
  if (width != xres || height != yres)
    image = pixbuf_scale (pixbuf, xres, yres);
  else
    image = pixbuf;

  /* Create pixmap at window size and render image */
  pixmap = gdk_pixmap_new (window, xres, yres, gdk_visual_get_best_depth());
  gdk_pixbuf_render_pixmap_and_mask (image, &pixmap, NULL, 255);

  /* Set background pixmap on window */
  gdk_window_set_back_pixmap (window, pixmap, FALSE);
  g_object_unref (pixmap);

  gdk_window_clear (window);   /* clear window to effect change */
  gdk_flush();

  return NULL;
} /* </set_background_pixbuf> */

/*
 * set_background - set background of a window
 */
GError *
set_background (GdkWindow *window, const gchar *pathname)
{
  static GError *error = NULL;

  if (pathname == '#') {
    GdkColor bgcolor;
    GdkColormap *map = gdk_window_get_colormap(window);

    gdk_color_parse (pathname, &bgcolor);
    gdk_color_alloc (map, &bgcolor);
    gdk_window_set_background (window, &bgcolor);
    gdk_window_clear (window);  /* clear window to effect change */
  }
  else {
    gchar *command = g_strdup_printf (
    "gconftool -t string -s /desktop/gnome/background/picture_filename \"%s\"",
                                                               pathname);
    g_spawn_command_line_async(command, &error);
    g_free (command);

    /* unique to Generations Linux xorg-x11-core >= 3.0 */
    command = g_strdup_printf ("xsetbg %s", pathname);
    g_spawn_command_line_async(command, &error);
    g_free (command);
  }

  return error;
} /* </set_background> */

/*
 * draw_pixbuf
 */
void
draw_pixbuf (GdkDrawable *target,
             GdkPixbuf *pixbuf, GdkGC *gc,
             gint xpos, gint ypos)
{
  gdk_draw_pixbuf (target,	/* destination drawable */
                   gc,		/* GdkGC, used for clipping, or NULL */
                   pixbuf,	/* source GdkPixbuf */
                   0, 0,	/* source (x,y) coordinates */
                   xpos, ypos,	/* destination (x,y) coordinates */
                   -1, -1,	/* dimensions or -1 to use pixbuf size */
                   GDK_RGB_DITHER_NORMAL,
                   0, 0);	/* dither (x,y) coordinates */
} /* </draw_pixbuf> */

/*
 * draw_pixmap - set/unset bits
 *
 * sample usage:
 *
  gdk_pixbuf_render_pixmap_and_mask (pixbuf, &pixmap, &mask, 255);
  GdkBitmap *inverse = bitmap_new_with_pixel (window, width, height, 1);
  GdkGC *gc = gdk_gc_new (inverse);

  // Tells the Graphics context to draw using GDK_CLEAR.
  // That is, everywhere it wishes to draw it will actually
  // clear the associated pixmap (in this case bitmap) instead
  gdk_gc_set_function(gc, GDK_CLEAR);

  // Tells the Graphics Context to ONLY draw the places where
  // mask has been drawn on.
  gdk_gc_set_clip_mask(gc, mask);

  // Make the call.
  draw_pixmap (inverse, mask, gc, 0, 0);
 */
void
draw_pixmap (GdkDrawable *target,
             GdkPixmap *pixmap, GdkGC *gc,
             gint xpos, gint ypos)
{
  gdk_draw_drawable(
    target,	/* GdkDrawable *destination */
    gc,		/* GdkGC, used for clipping, or NULL */
    pixmap,	/* GdkDrawable *source */
    0, 0,	/* source (x,y) coordinates */
    xpos, ypos,	/* destination (x,y) coordinates */
    -1, -1	/* width and height; (-1,-1) => source dimensions */
  );
} /* </draw_pixmap> */

/*
 * sticky_window_new - create a sticky GTK_WINDOW_TOPLEVEL window
 */
GtkWidget *
sticky_window_new (GdkWindowTypeHint hint,
                   gint width, gint height,
                   gint xpos,  gint ypos)
{ 
  GtkWidget *widget;
  GtkWindow *window;

  /* Create a new window and set attributes. */
  widget = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  window = GTK_WINDOW (widget);

  gtk_widget_set_size_request (widget, width, height);
  gtk_window_set_default_size (window, width, height);
  gtk_window_set_decorated (window, FALSE);
  gtk_window_set_resizable (window, FALSE);

  gtk_window_set_skip_pager_hint (window, TRUE);
  gtk_window_set_skip_taskbar_hint (window, TRUE);
  gtk_window_set_type_hint (window, hint);

  gtk_window_move (window, xpos, ypos);
  gtk_window_stick(window);

#if 0
  if (hint == GDK_WINDOW_TYPE_HINT_DOCK || hint == GDK_WINDOW_TYPE_HINT_TOOLBAR)
  {
    /* http://standards.freedesktop.org/wm-spec/1.3/ar01s05.html */
    GdkAtom card  = gdk_atom_intern("CARDINAL",      FALSE);
    GdkAtom strut = gdk_atom_intern("_NET_WM_STRUT", FALSE);

    long wm_strut[] = { 0,			/* Left   */
                        0,			/* Right  */
                        height,			/* Top    */
                        0,			/* Bottom */
                      };

    gdk_property_change (GDK_WINDOW (widget->window), strut, card,
                         32, GDK_PROP_MODE_REPLACE,
                         (guchar *)(&wm_strut), 4);

    /* Explicitly keep the window above all others. */
    gtk_window_set_keep_above (window, TRUE);
  }
#endif

  if (hint != GDK_WINDOW_TYPE_HINT_MENU) {
    /* Set event handlers for window close/destroy */
    g_signal_connect (G_OBJECT (widget), "destroy",
                      G_CALLBACK (gtk_widget_destroy), NULL);

    g_signal_connect (G_OBJECT (widget), "delete_event",
                      G_CALLBACK (gtk_widget_destroy), NULL);
  }
  return widget;
} /* </sticky_window_new> */

/*
 * backdrop_window_new
 */
GtkWidget *
backdrop_window_new (GdkWindowTypeHint hint,
                     gint width, gint height,
                     gint xpos, gint ypos,
                     const char *shape)
{
  Display *display = GDK_WINDOW_XDISPLAY( GDK_ROOT_PARENT() );

  GtkWidget *widget;
  Window window;
  Pixmap bitmap;
  Pixmap pixels;
  XGCValues gcv;
  GC gc;

  /* Realize GTK_WIDGET to obtain XID (Window) */
  widget = sticky_window_new (hint, width, height, xpos, ypos);
  gtk_widget_realize (widget);

  window = gdk_x11_drawable_get_xid (widget->window);
  bitmap = XCreateBitmapFromData(display, window, "\0\377", 2, 2);
  pixels = XCreatePixmap(display, window, width, height, 1);

  gcv.foreground = 1;
  gcv.background = 0;
  gc = XCreateGC(display, pixels, (GCForeground|GCBackground), &gcv);

  XSetStipple(display, gc, bitmap);
  XSetFillStyle(display, gc, FillOpaqueStippled);
  XFillRectangle(display, pixels, gc, 0, 0, width, height);
  XFreeGC(display, gc);

  XShapeCombineMask(display, window, ShapeBounding, 0, 0, pixels, ShapeSet);
  XSetWindowBackground(display, window,
                       BlackPixel(display, DefaultScreen(display)));
  return widget;
} /* </backdrop_window_new> */

/*
 * inset_new
 * inset_frame_new
 */
GtkWidget *
inset_new (GtkWidget *box, guint spacing)
{
  GtkWidget *inset, *widget;

  widget = gtk_label_new (NULL);
  gtk_box_pack_start (GTK_BOX(box), widget, FALSE, FALSE, spacing);
  gtk_widget_show (widget);

  widget = gtk_label_new (NULL);
  gtk_box_pack_end (GTK_BOX(box), widget, FALSE, FALSE, spacing);
  gtk_widget_show (widget);

  inset = gtk_vbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX(box), inset, TRUE, TRUE, 0);
  gtk_widget_show (inset);

  widget = gtk_label_new (NULL);
  gtk_box_pack_start (GTK_BOX(inset), widget, FALSE, FALSE, spacing/2);
  gtk_widget_show (widget);

  widget = gtk_label_new (NULL);
  gtk_box_pack_end (GTK_BOX(inset), widget, FALSE, FALSE, spacing/2);
  gtk_widget_show (widget);

  return inset;
} /* </inset_new> */

GtkWidget *
inset_frame_new (GtkWidget *box, guint spacing, const gchar *tag)
{
  GtkWidget *frame, *layer;
  GtkWidget *inset = inset_new (box, spacing);

  frame = gtk_frame_new (tag);
  gtk_container_add (GTK_CONTAINER(inset), frame);
  gtk_widget_show (frame);

  layer = gtk_hbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER(frame), layer);
  gtk_widget_show (layer);

  return inset_new (layer, spacing);
} /* </inset_frame_new> */

/*
 * stock_button_new
 */
GtkWidget *
stock_button_new (const char *name, const char *stockid, int padding)
{
  GtkWidget *button, *image, *label;
  GtkWidget *box = gtk_hbox_new (FALSE, 0);

  image = gtk_image_new_from_stock (stockid, GTK_ICON_SIZE_BUTTON);
  gtk_box_pack_start (GTK_BOX(box), image, FALSE, FALSE, 0);
  gtk_widget_show (image);

  label = gtk_label_new (name);
  gtk_box_pack_start (GTK_BOX(box), label, FALSE, FALSE, padding);
  gtk_widget_show (label);

  if (padding > 0) {
    label = gtk_label_new ("");
    gtk_box_pack_start (GTK_BOX(box), label, FALSE, FALSE, padding);
    gtk_widget_show (label);
  }

  button = gtk_button_new ();
  gtk_container_add (GTK_CONTAINER (button), box);
  gtk_widget_show (box);

  return button;
} /* </stock_button_new> */
