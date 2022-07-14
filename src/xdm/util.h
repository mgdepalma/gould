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

#ifndef _GOULD_UTIL_H_
#define _GOULD_UTIL_H_

#include <glib.h>

#ifndef CONFIG_CACHE
#define CONFIG_CACHE "/var/run/gould/cache"
#endif

#ifndef MAX_PATHNAME
#define MAX_PATHNAME 128
#endif

#define PREVIOUS "previous"

void log_print(char *fmt, ...);
void log_sigsegv(void);

void saveconfig(GKeyFile *config, const char *path);

const char *session_desktop_get(const char *resource);
gboolean session_desktop_put(const char *resource, const char *name);

const char *session_lang_get(const char *resource);
gboolean session_lang_put(const char *resource, const char *name);

void stop_pid(int);
char *sysname(void);

#endif /*_GOULD_UTIL_H_*/

