/**
* Copyright (C) Generations Linux <bugs@softcraft.org>
* All Rights Reserved.
*
* This library is free software; you can redistribute it and/or
* modify it under the terms of the GNU Library General Public
* License as published by the Free Software Foundation; either
* version 2 of the License, or (at your option) any later version.
*
* This library is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
* Library General Public License for more details.
*
* You should have received a copy of the GNU Library General Public
* License along with this library; if not, write to the
* Free Software Foundation, Inc., 59 Temple Place - Suite 330,
* Boston, MA 02111-1307, USA.
*/

#ifndef __GDK_DRAWING_CONTEXT_H__ // <gtk-3.0/gdk/gdkdrawingcontext.h>
#include <cairo-xlib.h>

cairo_surface_t *
gdk_window_create_similar_image_surface (GdkWindow *window,
				         cairo_format_t format,
				         int            width,
				         int            height,
				         int            scale)
{
  cairo_content_t  content = CAIRO_CONTENT_COLOR_ALPHA;
  cairo_surface_t *surface =
                    gdk_window_create_similar_surface (window,
                                                       content,
                                                       width,
                                                       height);
  return surface;
} /* </gdk_window_create_similar_image_surface> */

/**
* gdk_cairo_surface_create_from_pixbuf: available in GTK3
*/
cairo_surface_t *
gdk_cairo_surface_create_from_pixbuf (const GdkPixbuf *pixbuf,
                                      int scale, GdkWindow *gdkwindow)
{
  if(!GDK_IS_WINDOW(gdkwindow)) return NULL;

  gint width = gdk_pixbuf_get_width (pixbuf);
  gint height = gdk_pixbuf_get_height (pixbuf);
  guchar *gdk_pixels = gdk_pixbuf_get_pixels (pixbuf);
  int gdk_rowstride = gdk_pixbuf_get_rowstride (pixbuf);
  int n_channels = gdk_pixbuf_get_n_channels (pixbuf);
  int cairo_stride;
  guchar *cairo_pixels;
  cairo_format_t format;
  cairo_surface_t *surface;
  int row;

  if (n_channels == 3)
    format = CAIRO_FORMAT_RGB24;
  else
    format = CAIRO_FORMAT_ARGB32;

  surface = gdk_window_create_similar_image_surface (gdkwindow,
					             format,
					             width, height,
					             scale);

  cairo_stride = cairo_image_surface_get_stride (surface);
  cairo_pixels = cairo_image_surface_get_data (surface);

  for (row = height; row; row--)
  {
      guchar *p = gdk_pixels;
      guchar *q = cairo_pixels;

      if (n_channels == 3)
        {
          guchar *end = p + 3 * width;

          while (p < end)
            {
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
              q[0] = p[2];
              q[1] = p[1];
              q[2] = p[0];
#else
              q[1] = p[0];
              q[2] = p[1];
              q[3] = p[2];
#endif
              p += 3;
              q += 4;
            }
        }
      else
        {
          guchar *end = p + 4 * width;
          guint t1,t2,t3;

#define MULT(d,c,a,t) G_STMT_START { t = c * a + 0x7f; d = ((t >> 8) + t) >> 8; } G_STMT_END

          while (p < end)
            {
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
              MULT(q[0], p[2], p[3], t1);
             MULT(q[1], p[1], p[3], t2);
              MULT(q[2], p[0], p[3], t3);
              q[3] = p[3];
#else
              q[0] = p[3];
              MULT(q[1], p[0], p[3], t1);
              MULT(q[2], p[1], p[3], t2);
              MULT(q[3], p[2], p[3], t3);
#endif

              p += 4;
              q += 4;
            }

#undef MULT
        }

      gdk_pixels += gdk_rowstride;
      cairo_pixels += cairo_stride;
    }

  cairo_surface_mark_dirty (surface);
  return surface;
} /* </gdk_cairo_surface_create_from_pixbuf> */

/**
* gdk_pixbuf_get_from_surface: available in GTK3
*/
GdkPixbuf *
gdk_pixbuf_get_from_surface (cairo_surface_t *surface,
                             int              src_x,
                             int              src_y,
                             int              width,
                             int              height)
{
  cairo_content_t content;
  GdkPixbuf *dest;

  /* General sanity checks */
  g_return_val_if_fail (surface != NULL, NULL);
  g_return_val_if_fail (width > 0 && height > 0, NULL);

  content = cairo_surface_get_content (surface) | CAIRO_CONTENT_COLOR;
  dest = gdk_pixbuf_new (GDK_COLORSPACE_RGB,
                         !!(content & CAIRO_CONTENT_ALPHA),
                         8,
                         width, height);

  surface = cairo_surface_reference (surface);
  cairo_surface_flush (surface);

  if (cairo_surface_status (surface) || dest == NULL)
    {
      cairo_surface_destroy (surface);
      g_clear_object (&dest);
      return NULL;
    }

#if 0
  if (gdk_pixbuf_get_has_alpha (dest))
    convert_alpha (gdk_pixbuf_get_pixels (dest),
                   gdk_pixbuf_get_rowstride (dest),
                   cairo_image_surface_get_data (surface),
                   cairo_image_surface_get_stride (surface),
                   src_x, src_y,
                   width, height);
  else
    convert_no_alpha (gdk_pixbuf_get_pixels (dest),
                      gdk_pixbuf_get_rowstride (dest),
                      cairo_image_surface_get_data (surface),
                      cairo_image_surface_get_stride (surface),
                      src_x, src_y,
                      width, height);
#endif
  cairo_surface_destroy (surface);

  return dest;
} /* </gdk_pixbuf_get_from_surface> */
#endif // ! __GDK_DRAWING_CONTEXT_H__
