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

#ifndef GENESIS_H
#define GENESIS_H

#include "util.h"
#include "package.h"
#include "xmlconfig.h"

#define DEFAULT_XMLFILE "/generations/software"


G_BEGIN_DECLS

/* Data structures declarations. */
typedef struct _Depot Depot;

struct _Depot
{
  ConfigurationNode *config;	/* Software Depot configuration cache */

  GList *catalogs;		/* list of software catalogs */
  GList *signatures;		/* list of signatures in the depot */
  GList *packages;		/* list of packages in a given catalog */
  GList *source;		/* list of source package path names */
  GList *files;			/* list of package files available */

  const char *arch;		/* machine architecture */
  const char *distribution;	/* distribution name string */
  const char *release;		/* distribution release string */

  const char *root;		/* target installation path */
};

/* Methods exported in the implementation. */
Depot *read_depot_configuration (const char *path, const char *catalog);

void interactive (int argc, char *argv[]);

G_END_DECLS

#endif /* </GENESIS_H> */
