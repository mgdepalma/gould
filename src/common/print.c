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

#include "config.h"
#include "print.h"

extern const char *Program;	/* published program name identifier */
extern const char *Release;	/* published program release version */

static PaperSize Paper[] = {
  { "Letter",     "8.5 x 11 in",       850,  1100 },
  { "Legal",      "8.5 x 14 in",       850,  1400 },
  { "Tabloid",    "11 x 17 in",        1100, 1700 },
  { "Ledger",     "17 x 11 in",        1700, 1100 },
  { "Executive",  "7.5 x 10 in",       750,  1000 },
  { "A3",         "11.69 x 16.53 in",  1169, 1653 },
  { "A4",         "8.26 x 11.69 in",   826,  1169 },
  { "A5",         "5.83 x 8.26 in",    583,  826  },
  { "B4",         "9.83 x 13.93 in",   983,  1393 },
  { "B5",         "7.17 x 10.12 in",   717,  1012 },
  { NULL,         NULL,                0,    0    }
};


/*
 * get_papersize
 */
PaperSize *
get_papersize()
{
  return Paper;
} /* </get_papersize> */

/*
 * get_printers_list
 */
GList *
get_printers_list()
{
  static GList *printers = NULL;

  if (printers == NULL) {
    printers = g_list_append (printers, _("Default"));
  }

  return printers;
} /* </get_printers_list> */

/*
 * (private) write_ascii85_tuple
 */
static void
write_ascii85_tuple(FILE *out, ASCII85 *ascii85)
{
  char nibble[5], *scan = nibble;
  int idx = 5;
  do {
    *scan++ = ascii85->tuple % 85;
    ascii85->tuple /= 85;
  } while (--idx > 0);

  idx = ascii85->count;
  do {
    putc(*--scan + '!', out);

    if (ascii85->chars++ >= MAX_CHARS_PER_LINE) {
      ascii85->chars = 0;
      putc('\n', out);
    }
  } while (idx-- > 0);
} /* </write_ascii85_tuple> */

/*
 * (private) write_ascii85
 */
static void
write_ascii85(FILE *out, ASCII85 *ascii85, unsigned char byte)
{
  switch (ascii85->count++) {
    case 0: ascii85->tuple |= (byte << 24); break;
    case 1: ascii85->tuple |= (byte << 16); break;
    case 2: ascii85->tuple |= (byte <<  8); break;
    case 3:
      ascii85->tuple |= byte;

      if (ascii85->tuple == 0) {
        putc('z', out);

        if (ascii85->chars++ >= MAX_CHARS_PER_LINE) {
          ascii85->chars = 0;
          putc('\n', out);
        }
      }
      else {
        write_ascii85_tuple(out, ascii85);
      }

      ascii85->count = 0;
      ascii85->tuple = 0;
      break;
  }
} /* </write_ascii85> */

/*
 * print_pixbuf_header
 */
void
print_pixbuf_header(FILE *out, int width, int height, int index)
{
  static char *stamp[64];       /* i18n date/time stamp */
  static int bits = 8;          /* 8 bits for color images */
  static int margins = 15;      /* hardcoded paper margins */
  static int dpi = 72;          /* # of pixels per inch, at 100% scaling */

  time_t clock;
  double pos_inx, pos_iny;     /* top-left offset of image, in inches */
  int iw, ih, ox, oy;
  int idx = 0;

  double psizex = Paper[index].width  / INCH_DIVISOR;
  double psizey = Paper[index].height / INCH_DIVISOR;

  double sz_inx = (double) width  / dpi;    /* image width  in inches @dpi */
  double sz_iny = (double) height / dpi;    /* image height in inches @dpi */

  /* use equal scale on x,y dimensions (allow 1% margins) */
  double scale = fmin(psizex/sz_inx, psizey/sz_iny) * .99;

  /* adjust image size */
  if (sz_inx > psizex || sz_iny > psizey) {
    sz_inx *= scale;
    sz_iny *= scale;
  }

  /* center image on the paper */
  pos_inx = psizex/2 - sz_inx/2;
  pos_iny = psizey/2 - sz_iny/2;

  /* compute offset to bottom-left of image (in picas) */
  ox = (int) (pos_inx * dpi + 0.5) + margins;
  oy = (int) (pos_iny * dpi + 0.5) + margins;
  /* oy = (int) ((psizey - (pos_iny + sz_iny)) * dpi + 0.5); */

  /* printed image will have size iw,ih (in picas) */
  iw = (int) (sz_inx * dpi + 0.5);
  ih = (int) (sz_inx * dpi + 0.5);

  /* obtain localtime */
  time(&clock);
  strftime((char*)stamp,sizeof(stamp)-1, "%Y-%m-%d %H:%M %Z",localtime(&clock));

  /* emit header */
  fprintf(out, "%%!PS-Adobe-3.0\n");
  fprintf(out, "%%%%Creator: %s (%s)\n", Program, Release);
  fprintf(out, "%%%%CreationDate: %s\n", (char*)stamp);
  fprintf(out, "%%%%BoundingBox: %d %d %d %d\n", ox, oy, ox+iw, oy+ih);
  fprintf(out, "%%%%DocumentData: Clean7Bit\n");
  fprintf(out, "%%%%LanguageLevel: 2\n");
  fprintf(out, "%%%%Pages: 1\n");
  fprintf(out, "%%%%Orientation: Portrait\n");
  fprintf(out, "%%%%EndComments\n");
  fprintf(out, "%%%%Page: 1 1\n");
  fprintf(out, "gsave\n");
  fprintf(out, "%d %d translate\n", ox, ox+iw);
  fprintf(out, "%.3f %.3f scale\n", scale, scale);
  fprintf(out, "/DeviceRGB setcolorspace\n");
  fprintf(out, "<<\n");
  fprintf(out, "\t/ImageType 1\n");
  fprintf(out, "\t/Width %d\n", width);
  fprintf(out, "\t/Height %d\n", height);
  fprintf(out, "\t/BitsPerComponent %d\n", bits);
  fprintf(out, "\t/Decode [ 0 1 0 1 0 1 ]\n");
  fprintf(out, "\t/DataSource currentfile /ASCII85Decode filter\n");
  fprintf(out, "\t/ImageMatrix [ 1 0 0 -1 0 1 ]\n");
  fprintf(out, ">>\n");
  fprintf(out, "image\n");
} /* </print_pixbuf_header> */

/*
 * print_pixbuf encodes binary data as ASCII base-85 encoding.
 * The exact definition of ASCII base-85 encoding can be found
 * in the PostScript Language Reference (3rd ed.) chapter 3.13.3.
 */
void
print_pixbuf(FILE *out, GdkPixbuf *image, int index)
{
  gboolean alpha  = gdk_pixbuf_get_has_alpha (image);
  gint     stride = gdk_pixbuf_get_rowstride (image);
  gint     height = gdk_pixbuf_get_height (image);
  gint     width  = gdk_pixbuf_get_width (image);
  guchar  *pixels = gdk_pixbuf_get_pixels (image);

  ASCII85 _ascii85, *ascii85 = &_ascii85;
  int row, col, idx;


  /* set LC_NUMERIC locale [TODO: save current locale and restore later] */
  setlocale(LC_NUMERIC, "POSIX");

  /** ASCII85 initialize.
   * The following would be normal, but it breaks image data!
   *
   *   fprintf(out, "<~");
   *   ascii85->chars = 2;
  */
  ascii85->chars = 0;
  ascii85->count = 0;
  ascii85->tuple = 0;

  /* emit PostScript header */
  print_pixbuf_header(out, width, height, index);

  if (alpha) {
    guchar *bytes = g_new (guchar, height * width * 3);
    guchar *rgb, *scan;
    short  nibble;

    /* red   = (pixel & 0xff000000) >> 24;
     * green = (pixel & 0x00ff0000) >> 16;
     * blue  = (pixel & 0x0000ff00) >> 8;
     * alpha = (pixel & 0x000000ff);
     */
    for (row = 0; row < height; row++) {
      scan = pixels + row * stride;
      rgb  = bytes + row * width * 3;
      
      for (col = 0; col < width; col++) {
        for (idx = 0; idx < 3; idx++) {
          nibble = (scan[idx] - 0xff) * scan[3];
          rgb[idx] = 0xff + ((nibble + 0x80) >> 8);
          write_ascii85(out, ascii85, rgb[idx]);
        }
        scan += 4;
        rgb += 3;
      }
    }
    g_free(bytes);
  }
  else {
    for (row = 0; row < height; row++) {
      for (col = 0; col < width; col++) {
	idx = row * stride + col*3;
        write_ascii85(out, ascii85, pixels[idx]);
        write_ascii85(out, ascii85, pixels[idx+1]);
        write_ascii85(out, ascii85, pixels[idx+2]);
      }
    }
  }

  /* ASCII85 finalize */
  if (ascii85->count > 0) write_ascii85_tuple(out, ascii85);
  if (ascii85->chars + 2 > MAX_CHARS_PER_LINE) putc('\n', out);
  fprintf(out, "~>\n");

  /* emit PostScript trailer */
  fprintf(out, "grestore\n");
  fprintf(out, "showpage\n");
  fprintf(out, "%%%%Trailer\n");
  fprintf(out, "%%%%EOF\n");
} /* </print_pixbuf> */
