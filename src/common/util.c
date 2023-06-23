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

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <mntent.h>
#include <unistd.h>
#include <fcntl.h>
#include <pwd.h>

#include "gould.h"
#include "util.h"


/*
* Private data structures.
*/
typedef struct _FileMagicPrivate FileMagicPrivate;

struct _FileMagicPrivate
{
  FileMagic   magic;
  const char *bytes;
};

static FileMagicPrivate sword_[] = {
  { PGP_MAGIC, "-----BEGIN PGP"   },
  { RPM_MAGIC, "\xed\xab\xee\xdb" },
  { MAX_MAGIC, NULL               }
};

static char *distro_[LSB_VERSION + 1] = {
  "DISTRIB_ID",
  "DISTRIB_CODENAME",
  "DISTRIB_DESCRIPTION",
  "DISTRIB_RELEASE",
  "LSB_VERSION"
};

/**
* Methods (functions) implemention.
*/
#if GLIB_CHECK_VERSION(2,14,0) == 0
static void
_hash_table_add_key (gpointer key, gpointer value, GPtrArray *array)
{ g_ptr_array_add (array, key); }

static void
_hash_table_add_value (gpointer key, gpointer value, GPtrArray *array)
{ g_ptr_array_add (array, value); }

GList *
g_hash_table_get_keys (GHashTable *hash)
{
  GList *list = NULL;
  GPtrArray *array = g_ptr_array_new ();
  int idx;

  g_hash_table_foreach (hash, (GHFunc)_hash_table_add_key, array);

  for (idx = 0; idx < array->len; idx++)
    list = g_list_prepend (list, g_ptr_array_index (array, idx));

  g_ptr_array_free (array, TRUE);

  return list;
} /* </g_hash_table_get_keys> */

GList *
g_hash_table_get_values (GHashTable *hash)
{
  GList *list = NULL;
  GPtrArray *array = g_ptr_array_new ();
  int idx;

  g_hash_table_foreach (hash, (GHFunc)_hash_table_add_value, array);

  for (idx = 0; idx < array->len; idx++)
    list = g_list_prepend (list, g_ptr_array_index (array, idx));

  g_ptr_array_free (array, TRUE);

  return list;
} /* </g_hash_table_get_values> */
#endif

/*
* get_file_magic
*/
FileMagic
get_file_magic (const char *pathname)
{
  static int maxlen = 0;	/* longest entry */

  FileMagic magic = -1;
  int idx, fd = open(pathname, O_RDONLY);

  if (maxlen == 0)
    for (idx = 0; idx < MAX_MAGIC; idx++)
      maxlen = MAX(maxlen, strlen(sword_[idx].bytes));

  if (fd >= 0) {
    char buffer[maxlen + 1];
    int bytes = read(fd, buffer, maxlen);

    if (bytes == maxlen)
      for (idx = 0; idx < MAX_MAGIC; idx++)
        if (memcmp(buffer, sword_[idx].bytes, strlen(sword_[idx].bytes)) == 0) {
          magic = sword_[idx].magic;
          break;
        }

    close(fd);
  }

  return magic;
} /* </get_file_magic> */

/*
* glist_find
*/
bool
glist_find (GList *list, const char *item)
{
  GList *iter;
  bool result = false;

  for (iter = list; iter; iter = iter->next)
    if (strcmp((char*)iter->data, item) == 0) {
      result = true;
      break;
    }

  return result;
} /* </glist_find> */

/*
* sudoallowed
*/
bool
sudoallowed (const char *command)
{
  bool allowed = false;
  char *line = g_strdup_printf("sudo -A -l %s 2>&1", command);
  FILE *stream = popen(line, "r");

  if (stream != NULL) {
    char text[FILENAME_MAX];

    if (fgets(text, FILENAME_MAX, stream) != NULL) {
      if (strncmp(text, command, strlen(command)) == 0)
        allowed = true;
    }

    /* See if 'sudo -l <command>' come back with (or without) a path. */
    /* allowed = (count > 1) ? true : false; */

    vdebug (1, "[shutdown]sudo %s is %sallowed.\n",
            command, (allowed) ? "" : "not ");

    pclose(stream);
  }
  g_free (line);

  return allowed;
} /* </sudoallowed> */

/*
* simple_list_find - find a string match from a linked list.
*/
const char *
simple_list_find (GList *list, const char *pattern)
{
  GList *iter;
  char  *match = NULL;

  for (iter = list; iter != NULL; iter = iter->next)
    if (strncmp(iter->data, pattern, MINSTR(iter->data, pattern)) == 0) {
      match = iter->data;
      break;
    }

  return match;
} /* </simple_list_find> */

/*
* simple_list_free - deallocate all memory from a linked list.
*/
GList *
simple_list_free (GList *list)
{
  if (list != NULL) {
    g_list_foreach (list, (GFunc)g_free, NULL);
    g_list_free (list);
  }
  return NULL;
} /* </simple_list_free> */

/*
* path_finder - search for file name from a path list
*/
const gchar *
path_finder (GList *list, const char *name)
{
  static char file[MAX_PATHNAME];
  char *pathname = NULL;
  GList *iter;

  for (iter = list; iter != NULL; iter = iter->next) {
    sprintf(file, "%s/%s", (char *)iter->data, name);

    if (g_file_test(file, G_FILE_TEST_EXISTS)) {
      pathname = (char *)file;
      break;
    }
  }

  return pathname;
} /* </path_finder_theme> */

/*
* get_disk_list look for {hd,sd}* matches in /proc/partitions
*/
GList *
get_disk_list (GList *exclude)
{
  GList *list = NULL;
  FILE *stream = fopen ("/proc/partitions", "r");

  if (stream) {
    char line[MAX_PATHNAME];
    int idx = 0;

    while (fgets(line, MAX_PATHNAME, stream))
      if (++idx > 2) {			   /* skip the first two lines */
        char *scan = line;

        while (++scan && *scan && strlen(scan) > 1 && *scan != '\n')
          if (strncmp(scan, "hd", 2) == 0 || strncmp(scan, "sd", 2) == 0) {
            if (!isdigit(scan[3])) {
              scan[strlen(scan) - 1] = '\0';

              if (!glist_find (exclude, scan))		/* not excluded */
                list = g_list_append (list, strdup(scan));
            }
            break;
          }
      }

    fclose(stream);
  }

  return list;
} /* </get_disk_list> */

/*
* get_internal_partitions
*/
GList *
get_internal_partitions (void)
{
  GList *iter, *list = NULL;
  GList *part, *internal = get_disk_list (get_usb_storage());

  for (iter = internal; iter; iter = iter->next)
    for (part = get_partitions (iter->data); part; part = part->next)
      list = g_list_append (list, part->data);

  return list;
} /* </get_internal_partitions> */

/*
* get_mounted_devices
*/
GList *
get_mounted_devices (GList *exclude)
{
  GList *list = NULL;
  FILE *stream = setmntent (MOUNTED, "r");

  if (stream) {
    struct mntent *mnt;

    while ((mnt = getmntent (stream))) {
      char *name = mnt->mnt_fsname;

      if (strlen(name) > 3 && strncmp(name, "/dev", 4) == 0) {
        if (!glist_find (exclude, &name[5])) {		/* not excluded */
          DeviceInfo *dev = g_new0 (DeviceInfo, 1);

          dev->capability = get_device_capability (mnt->mnt_fsname);
          dev->fsname = strdup(mnt->mnt_fsname);
          dev->mntdir = strdup(mnt->mnt_dir);

          list = g_list_append (list, dev);
        }
      }
    }

    fclose(stream);
  }

  return list;
} /* </get_mounted_devices> */

/*
* get_partitions
*/
GList *
get_partitions (const char *device)
{
  GList *list = NULL;
  FILE *stream = fopen ("/proc/partitions", "r");

  if (stream) {
    char line[MAX_PATHNAME];
    int idx = 0, mark = strlen(device);

    while (fgets(line, MAX_PATHNAME, stream))
      if (++idx > 2) {                     /* skip the first two lines */
        char *scan = line;

        while (++scan && *scan && strlen(scan) >= mark && *scan != '\n')
          if (strncmp(scan, device, mark) == 0) { /* look for device match */
            if (strlen(scan) > mark+1) {	  /* only the partitions */
              scan[strlen(scan) - 1] = '\0';
              list = g_list_append (list, strdup(scan));
            }
            break;
          }
      }

    fclose(stream);
  }

  return list;
} /* </get_partitions> */

/*
* get_usb_storage

   $ ls /proc/scsi/usb-storage
   1

   $ cat /proc/scsi/usb-storage/1 
      Host scsi10: usb-storage
          Vendor: Kingston
         Product: DataTraveler 2.0
   Serial Number: 899801162008011514259F3C
        Protocol: Transparent SCSI
       Transport: Bulk
          Quirks:

   $ cd /sys/bus/usb/drivers/usb-storage/1-1:1.0/host1/target1:0:0/1:0:0:0
   $ ls block:*
   block:sda
*/
GList *
get_usb_storage (void)
{
  const char *path = "/proc/scsi/usb-storage";
  const char *sysbus = "/sys/bus/usb/drivers/usb-storage";

  struct dirent **names;
  int count = scandir(path, &names, NULL, alphasort);
  GPtrArray *scsid = g_ptr_array_new ();
  GList *list = NULL;

  char *name;
  int idx;


  /* The first step is to look in /proc/scsi/usb-storage/ */
  for (idx = 0; idx < count; idx++) {
    name = names[idx]->d_name;

    if (isdigit(name[0]))	/* we only want numeric names */
      g_ptr_array_add (scsid, GINT_TO_POINTER(atoi(name)));
  }

  if (scsid->len > 0) {		/* any usb-storage scsi ids? */
    GPtrArray *usbus = g_ptr_array_new ();
    GPtrArray *hosts = g_ptr_array_new ();

    int id, ndx, udx;
    gchar *scan;

    /* Find usb bus entries in `sysbus` */
    vdebug (5, "%s: devices => %d\n", path, scsid->len);
    count = scandir(sysbus, &names, NULL, alphasort);

    for (idx = 0; idx < count; idx++) {
      name = names[idx]->d_name;

      if (isdigit(name[0]))	/* we only want numeric names */
        g_ptr_array_add (usbus, strdup(name));
    }

    /* A little complex to find all usb-storage scsi host devices. */
    for (idx = 0; idx < scsid->len; idx++) {
      id = (int)g_ptr_array_index (scsid, idx);

      for (udx = 0; udx < usbus->len; udx++) {
        scan = g_strdup_printf ("%s/%s", sysbus, 
                                (char*)g_ptr_array_index (usbus, udx));

        count = scandir(scan, &names, NULL, alphasort);
        vdebug (5, "%s/host%d\n", scan, id);

        g_free (scan);
        scan = g_strdup_printf ("host%d", id);

        for (ndx = 0; ndx < count; ndx++) {
          name = names[ndx]->d_name;

          if (strcmp(scan, name) == 0) {
            g_ptr_array_add (hosts,
                g_strdup_printf ("%s/%s/host%d/target%d:0:0/%d:0:0:0",
                  sysbus, (char*)g_ptr_array_index (usbus, udx), id, id, id));
            break;
          }
        }
        g_free (scan);
      }
    }

    /* Populate return list with device names. */
    for (idx = 0; idx < hosts->len; idx++) {
      scan = g_ptr_array_index (hosts, idx);
      count = scandir(scan, &names, NULL, alphasort);
      vdebug (5, "%s\n", scan);

      for (ndx = 0; ndx < count; ndx++) {
        name = names[ndx]->d_name;

        if (strlen(name) > 8 && strncmp(name, "block:", 5) == 0) {
          list = g_list_append (list, strdup(&name[6]));
          break;
        }
      }
      g_free (scan);
    }
    g_ptr_array_free (hosts, TRUE);

    /* Release memory allocated for storing usb bus entries. */
    for (udx = 0; udx < usbus->len; udx++) {
      scan = g_ptr_array_index (usbus, udx);
      g_free (scan);
    }
    g_ptr_array_free (usbus, TRUE);
  }
  g_ptr_array_free (scsid, TRUE);

  return list;
} /* </get_usb_storage> */

/*
* get_username
*/
const char *
get_username (bool full)
{
  static char name[MAX_LABEL];
  struct passwd *user = getpwuid(getuid());

  if (user == NULL || full == false)
    strcpy(name, getenv("LOGNAME"));
  else
    strcpy(name, user->pw_gecos);
  
  return (char *)name;
} /* </get_username> */

/*
* lsb_release yield description of LSB option given
*/
const char *
lsb_release (LinuxStandardBase option)
{
  static char *_lsb_release = "/etc/lsb-release";
  static char description[MAX_PATHNAME];
  char *result = NULL;

  if (g_file_test(_lsb_release, G_FILE_TEST_EXISTS)) {
    FILE *stream = fopen(_lsb_release, "r");

    if (stream) {
      const char *match = distro_[option];
      char line[MAX_PATHNAME];
      int len = strlen(match);

      while (fgets(line, MAX_PATHNAME, stream))
        if (strncmp(line, match, len) == 0) {
          strcpy(description, &line[len + 2]);		/* go past =" */
          description[strlen(description) - 2] = '\0';	/* chomp ending " */
          result = description;
          break;
        }

      fclose(stream);
    }
  }

  return result;
} /* </lsb_release> */

const char *
lsb_release_full (void)
{
  static char description[MAX_PATHNAME];
  const char *distro = lsb_release (DISTRIB_DESCRIPTION);
  char *result = NULL;

  if (distro) {
    char *distrib = strdup (distro);
    const char *version = lsb_release (DISTRIB_RELEASE);

    if (version == NULL)
      strcpy(description, distrib); 
    else {
      char *release  = strdup(version);
      const char *codename = lsb_release (DISTRIB_CODENAME);

      if (codename == NULL)
        sprintf(description, "%s %s", distrib, release);
      else
        sprintf(description, "%s %s (%s)", distrib, release, codename);

      free((void *)release);
    }

    free((void *)distrib);
    result = description;
  }

  return result;
} /* </lsb_release_full> */

/*
* get_device_capability
*/
int
get_device_capability (const char *device)
{
  char capability[MAX_PATHNAME];
  char media[MAX_PATHNAME];
  struct stat info;
  int mark, word;
  FILE *stream;

  /* Follow the symbolic link, when applicable. */
  strcpy(media, device);

  if (lstat(device, &info) == 0 && S_ISLNK(info.st_mode)) {
    if ((mark = readlink(device, media, MAX_PATHNAME)) > 0)
      media[mark] = (char)0;     /* readlink does not append \0 */
  }

  /* Chomp the last character(s) when they are digit. */
  for (mark = strlen(media) - 1; mark > 4; mark--)
    if (isdigit(media[mark]))
      media[mark] = (char)0;
    else
      break;

  /* Consult /sys/block/%s/capability to get device type. */
  sprintf(capability, "/sys/block/%s/capability", &media[5]);

  if ((stream = fopen(capability, "r"))) {
    if (fgets(media, MAX_PATHNAME, stream))  /* WARNING: not fully tested */
      word = atoi(media);

    fclose(stream);
  }
  else {
    word = 0;
  }

  return word;
} /* </get_device_capability> */

/*
* glist_compare
*/
int
glist_compare (GList *alist, GList *blist)
{
  int result = g_list_length (alist) - g_list_length (blist);

  if (result == 0) {			 /* memory compare */
    int idx, count = g_list_length (alist);

    gpointer *avector = g_new (gpointer, count);
    gpointer *bvector = g_new (gpointer, count);

    GList *iter;

    for (idx = 0, iter = alist; iter != NULL; iter = iter->next)
      avector[idx++] = iter->data;

    for (idx = 0, iter = blist; iter != NULL; iter = iter->next)
      bvector[idx++] = iter->data;

    result = memcmp (avector, bvector, sizeof (gpointer) * count);

    g_free (avector);
    g_free (bvector);
  }

  return result;
} /* </glist_compare> */

/*
* get_process_id - get {program} PID or -1, if not running  
*/
pid_t
get_process_id(const char *program)
{
  pid_t instance = -1;

  char command[MAX_COMMAND];
  static char answer[MAX_COMMAND];
  FILE *stream;

  sprintf(command, "pidof %s", program);
  stream = popen(command, "r");

  if (stream) {
    const char delim[2] = " ";
    char *master, *token;

    fgets(answer, MAX_COMMAND, stream);	/* answer maybe a list */
    master = strtok(answer, delim);

    for (token = master; token != NULL; ) {
      token = strtok(NULL, delim);
      if(token != NULL) master = token;	/* pid is last on the list */
    }
    instance = strtoul(master, NULL, 10);
    fclose (stream);
  }
  return instance;
} /* </get_process_id> */

/*
* get_process_name - get name of process given a pid
*/
char *
get_process_name(pid_t pid)
{
  FILE *stream;
  char command[MAX_LABEL], name = NULL;
  static char answer[MAX_LABEL];

  sprintf(command, "ps -p %d -o comm=", pid);
  stream = popen(command, "r");

  if (stream) {
    fgets(answer, MAX_LABEL, stream);
    answer[strlen(answer) - 1] = (char)0; // chomp newline
    if(strlen(answer) > 0) name = answer;
    pclose(stream);
  }
  return name;
} /* </get_process_name> */

/*
* spawn forks child process
*/
pid_t
spawn (const char* cmdline)
{
  pid_t pid ;
#ifdef USE_GLIB_SPAWN
  gboolean result = g_spawn_command_line_async(cmdline, 0);
#else
  if ((pid = fork()) == 0) {                    /* child process */
    const char *shell = "/bin/sh";

    setsid();
    execlp(shell, shell, "-f", "-c", cmdline, NULL);
    exit(0);
  }
#endif
  return pid;
} /* </spawn> */

/*
* pidof - find process ID by name
*/
char *
pidof(const char *program)
{
  char command[MAX_COMMAND];
  static char answer[MAX_COMMAND];

  char *pidlist = NULL;
  FILE *stream;

  sprintf(command, "pidof %s", program);
  stream = popen(command, "r");

  if (stream) {
    fgets(answer, MAX_COMMAND, stream);

    if (strlen(answer) > 0) {
      answer[strlen(answer) - 1] = (char)0;	/* chomp newline */
      pidlist = answer;
    }
    fclose(stream);
  }
  return pidlist;
} /* </pidof> */

/*
* killall {program} {signum}
*/
int
killall(const char *program, int signum)
{
  char *pidlist = pidof (program);
  pid_t pid, self = getpid();
  int killed = 0;

  if (pidlist) {
    const char delim[2] = " ";
    char *token = strtok(pidlist, delim);

    while (token != NULL) {
      if ((pid = atoi(token)) != self) { /* will not kill {self} */
        if(kill(pid, signum) == 0) ++killed;
      }
      token = strtok(NULL, delim);
    }
  }
  return killed;
} /* </killall> */

/*
* scale_factor
*/
float
scale_factor(float width, float height)
{
  /* Cannot use, float factor = fmax(width,height) * 0.99;
  *  due to a bug with fmax() triggering a SIGILL on VIA C3
  */
  float maxim = (width > height) ? width : height;
  return maxim * 0.99;
} /* </scale_factor> */

/*
* vdebug - variadic debug print
*/
extern debug_t debug;	/* must be declared in main program */

void
vdebug (unsigned short level, const char *format, ...)
{
  va_list args;
  va_start (args, format);
  if(debug >= level) vprintf (format, args);
  va_end (args);
} /* vdebug */

