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

#ifndef PRINT_H
#define PRINT_H

#include <math.h>
#include <time.h>
#include <locale.h>
#include <stdlib.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define PRINT_DEVICE      0
#define PRINT_FILE        1

#define PRINT_LANDSCAPE  10
#define PRINT_PORTRAIT   11

#define MAX_CHARS_PER_LINE 72   /* max chars per line   */


/* ASCII base-85 encoding */
typedef struct {
  int           chars;   /* characters written */
  int           count;   /* tuple byte counter */
  unsigned long tuple;   /* base-85 tuple      */
} ASCII85;

/* Paper size definition */
typedef struct {
  char   *name;          /* name label for the paper size   */
  char   *dimensions;    /* description of paper dimensions */
  int    width;          /* width  in 1/100 inches          */
  int    height;         /* height in 1/100 inches          */
} PaperSize;

#define INCH_DIVISOR 100 /* divisor to convert to inches    */


/* Methods exported by the implementation */
PaperSize *get_papersize();
GList *get_printers_list();

void print_pixbuf_header(FILE *out, int width, int height, int index);
void print_pixbuf(FILE *out, GdkPixbuf *image, int index);

G_END_DECLS

#endif /* </PRINT_H> */
