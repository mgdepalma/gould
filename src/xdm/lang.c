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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <string.h>

#include "gdm/gdm-languages.h"
#include "lang.h"

/*
 * private methods
 */
static int
cmpstr(const void *p1, const void *p2)
{
  return strcmp(* (char * const *) p1, * (char * const *) p2);
}

static char **
get_all_language_names(void)
{
  char **list, **lang;
  int len;

  list = gdm_get_all_language_names();
  if(!list) return NULL;

  for (lang = list; *lang != NULL;lang++) {
    char *normal = gdm_normalize_language_name(*lang);
    if (normal) {
      g_free(*lang);
      *lang = normal;
    }
  }

  len = g_strv_length(list);
  qsort(list, len, sizeof(char*), (void*)cmpstr);

  return list;
} /* get_all_language_names */

static char **
get_config_language_names(GKeyFile *config)
{
  char **list;

  list = g_key_file_get_string_list(config, "cache", "langs", NULL, NULL);
  if(!list) list = g_malloc0(sizeof(char*));

  return list;
} /* get_config_language_names */

/*
 * [public] gould_load_langs
 */
void
gould_load_langs(GKeyFile *config, gboolean all, void *arg,
                 void (*cb)(void *arg, char *lang, char *desc))
{
  char **lang;
  int idx;

  cb(arg, "", _("Default")); /* default is to use the system wide settings */
  lang = all ? get_all_language_names() : get_config_language_names(config);

  if (lang) {
    for (idx = 0; lang[idx] != NULL; idx++) {
      char *readable = gdm_get_language_from_name(lang[idx], lang[idx]);
      cb(arg, lang[idx], readable);
      g_free(readable);
    }
    g_strfreev(lang);
  }
} /* gould_load_langs */

