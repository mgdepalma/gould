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

#ifndef PAGER_H
#define PAGER_H

#include "greenwindow.h"

G_BEGIN_DECLS

#define GREEN_TYPE_PAGER            (pager_get_type ())
#define GREEN_PAGER(object)         (G_TYPE_CHECK_INSTANCE_CAST ((object), \
                                     GREEN_TYPE_PAGER, Pager))

#define IS_GREEN_PAGER(object)      (G_TYPE_CHECK_INSTANCE_TYPE ((object), \
                                     GREEN_TYPE_PAGER))

#define GREEN_PAGER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), \
                                     GREEN_TYPE_PAGER, PagerClass))

#define IS_GREEN_PAGER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), \
                                     GREEN_TYPE_PAGER))

#define GREEN_PAGER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
                                     GREEN_TYPE_PAGER, PagerClass))

typedef struct _Pager        Pager;
typedef struct _PagerClass   PagerClass;
typedef struct _PagerPrivate PagerPrivate;

struct _Pager
{
  GtkContainer instance;	/* parent instance */

  PagerPrivate *priv;		/* private data */
};

struct _PagerClass
{
  GtkContainerClass parent;	/* parent class */
};

typedef enum {
  PAGER_DISPLAY_NAME,
  PAGER_DISPLAY_CONTENT
} PagerDisplayMode;

GType pager_get_type (void) G_GNUC_CONST;

void pager_set_n_rows (Pager *pager, int cells);
void pager_set_display_mode (Pager *pager, PagerDisplayMode mode);
void pager_set_orientation (Pager *pager, GtkOrientation orientation);
void pager_set_shadow_type (Pager *pager, GtkShadowType shadow);

Pager *pager_new (Green *screen);

G_END_DECLS

#endif /* PAGER_H */

