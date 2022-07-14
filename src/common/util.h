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

#ifndef UTIL_H
#define UTIL_H

#include "config.h"

#include <glib.h>

#define DEV_REMOVABLE 1
#define DEV_DRIVERFS  2

G_BEGIN_DECLS

/**
 * Public data structures.
 */
typedef struct _DeviceInfo DeviceInfo;

typedef enum _FileMagic         FileMagic;
typedef enum _LinuxStandardBase LinuxStandardBase;

struct _DeviceInfo
{
  unsigned short capability;
  char *fsname;
  char *mntdir;
};

enum _FileMagic
{
  PGP_MAGIC,
  RPM_MAGIC,
  MAX_MAGIC
};

enum _LinuxStandardBase
{
  DISTRIB_ID,
  DISTRIB_CODENAME,
  DISTRIB_DESCRIPTION,
  DISTRIB_RELEASE,
  LSB_VERSION
};


/**
 * Public methods (util.c) exported in the implementation.
 */
FileMagic get_file_magic (const char *pathname);

#if GLIB_CHECK_VERSION(2,14,0) == 0
GList *g_hash_table_get_keys (GHashTable *hash);
GList *g_hash_table_get_values (GHashTable *hash);
#endif

int glist_compare (GList *alist, GList *blist);
gboolean glist_find (GList *list, const char *item);

const char *path_finder (GList *list, const char *name);

pid_t pidof (const char *name, const char *pidfile);

const char *simple_list_find (GList *list, const char *pattern);
GList *simple_list_free (GList *list);

const char *getprogname (void);
void setprogname (const char *progname); 

const char *lsb_release (LinuxStandardBase option);
const char *lsb_release_full (void);

GList *get_disk_list (GList *exclude);
GList *get_internal_partitions (void);
GList *get_mounted_devices (GList *exclude);
GList *get_partitions (const char *device);
GList *get_usb_storage (void);

int get_device_capability (const char *device);

char *get_username (gboolean full);

float scale_factor(float width, float height);

gboolean sudoallowed (const char *command);

void vdebug (unsigned short level, const char *format, ...);

G_END_DECLS

#endif /* </UTIL_H */
