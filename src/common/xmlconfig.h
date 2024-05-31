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

#ifndef XML_CONFIG_H
#define XML_CONFIG_H

#include <fcntl.h>
#include <stdbool.h>
#include <libxml/xmlreader.h>
#include <string.h>
#include <glib.h>

/* Safe libc:free(3) wrapper (should be inline method). */
#define MFREE(x) if ((x) != NULL) free((x));

#define SCHEMA_VERSION(a,b,c) (((a) << 16) + ((b) << 8) + (c))

G_BEGIN_DECLS

/* Global program data structure */
typedef struct _ConfigurationAttrib ConfigurationAttrib;
typedef struct _ConfigurationNode ConfigurationNode;
typedef struct _SchemaVersion SchemaVersion;

struct _ConfigurationAttrib
{
  gchar *name;			/* attribute name */
  gchar *value;			/* attribute value */
  ConfigurationAttrib *next;	/* next record */
};

struct _ConfigurationNode
{
  ConfigurationAttrib *attrib;	/* attributes list */
  ConfigurationNode *back;	/* previous record */
  ConfigurationNode *next;	/* next record */

  gchar *element;		/* element name/value */

  gpointer data;		/* user defined data - generic */
  gpointer widget;		/* user defined data - widget */

  guint depth;			/* element depth */
  guint type;			/* element type */
};

struct _SchemaVersion
{
  unsigned short major;		/* major release number */
  unsigned short minor;		/* minor release number */
  unsigned short patch;		/* patch level number */
};

/* Methods exported in the implementation */
void configuration_attrib_remove (ConfigurationAttrib *attrib);

gchar *configuration_attrib (ConfigurationNode *node, gchar *name);
gchar *configuration_path (ConfigurationNode *node);

ConfigurationNode *configuration_find (ConfigurationNode *node,
                                       const gchar *name);

ConfigurationNode *configuration_find_end (ConfigurationNode *node);

ConfigurationNode *configuration_find_next (ConfigurationNode *node,
                                            const gchar *name);

ConfigurationNode *configuration_insert (ConfigurationNode *node,
                                         ConfigurationNode *site,
                                         gint nesting);

ConfigurationNode *configuration_remove (ConfigurationNode *node);

ConfigurationNode *configuration_move (ConfigurationNode *node,
                                       ConfigurationNode *site);

ConfigurationNode *configuration_clone (ConfigurationNode *node);

ConfigurationNode *configuration_read (const gchar *data,
                                       const gchar *schema,
                                       bool  memory);

void configuration_replace (ConfigurationNode *config, gchar *data,
                            const char *header, const char *section,
                            const char *ident);
void
configuration_update (ConfigurationNode *node, const char *key, gchar *value);

int configuration_write (ConfigurationNode *config,
                         const char *header,
                         FILE *stream);

SchemaVersion *configuration_schema_version (ConfigurationNode *config);

G_END_DECLS

#endif /* </XML_CONFIG_H> */
