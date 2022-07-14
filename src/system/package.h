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

#ifndef PACKAGE_H
#define PACKAGE_H

#include <stdio.h>
#include <string.h>
#include <libintl.h>
#include <dirent.h>
#include <fcntl.h>
#include <glib.h>

/* #define _(String) gettext (String) */

#define RPM_DEFAULT_DBPATH "/var/lib/rpm"
#define RPM_DEFAULT_SPOOL  "/var/spool/rpm"

#define RPMCONFIGDIR "/usr/lib/rpm"
#define RPMOPTALIAS  RPMCONFIGDIR "/rpmpopt"
#define RPMPROGRAM   "rpm"

G_BEGIN_DECLS

/* Data structures declarations */
typedef struct _Package   Package;
typedef enum   _QueryMode QueryMode;

struct _Package
{
  const char *name;
  const char *version;
  const char *release;
  const char *summary;
  const char *group;
  const char *size;

  const char *file;	/* NULL if package is from the RPM database */
  const char *root;	/* directory of RPM file or database root */

  FileMagic magic;	/* only applies to RPM file */
};

enum _QueryMode
{
  QueryInformation,
  QueryListing
};

/* Methods exported in the implementation */
GPtrArray *
pkg_cli_query (Package *package, const char *query, const char *source);
GPtrArray *pkg_query_info (Package *package, QueryMode mode);

GList *pkg_dirent_list (const char *path);
GList *pkg_query_list (const char *root);

gint alpha_name_sort (gconstpointer a, gconstpointer b);
gint pkg_name_sort (gconstpointer a, gconstpointer b);

void pkg_parse_header (Package *package, gchar *header);
void pkg_init (void);

G_END_DECLS

#endif /* </PACKAGE_H> */
