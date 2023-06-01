/**
 * Copyright (C) Generations Linux <bugs@softcraft.org>
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; You may only use version 2 of the License,
 * you have no option to use any other version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "gould.h"
#include "module.h"

#include <signal.h>
#include <unistd.h>

/*
* Data structures used by implementation.
*/
static char *placement_[] = {
  "NONE",
  "SCREEN",
  "CONTAINER",
  "START",
  "END",
  NULL
};

/*
 * module_load
 */
Modulus *
module_load (const gchar *file, guint spaces, gpointer data)
{
  Modulus *applet = NULL;
  GModule *module = g_module_open (file, G_MODULE_BIND_MASK);

  if (module) {
    gpointer module_init, module_open, module_close;
    applet = g_new0 (Modulus, 1);

    /* Mandatory symbols of a struct _modulus */
    if (!g_module_symbol(module, "module_init", &module_init)) {
      g_warning("Error loading module: %s\n", file);
      g_return_val_if_reached(NULL);
    }

    applet->data = data;		/* must be set before module_init */
    applet->module = module;		/* save struct _GModule */

    applet->module_init = module_init;
    applet->module_init (applet);	/* initialize plugin module */

    if (applet->space & spaces) {	/* must belong in the spaces mask */
      if (g_module_symbol(module, "module_open", &module_open))
        applet->module_open = module_open;

      if (g_module_symbol(module, "module_close", &module_close))
        applet->module_close = module_close;
    }
    else {
      g_free (applet);
      applet = NULL;
    }
  }
  else {
    g_warning(g_module_error());
    g_return_val_if_reached(NULL);
  }
  return applet;
} /* module_load */

/*
 * module_search
 */
Modulus *
module_search (GList *moduli, const gchar* name)
{
  GList *iter;
  Modulus *applet;
  Modulus *module = NULL;

  for (iter = moduli; iter != NULL; iter = iter->next) {
    applet = (Modulus *)iter->data;

    if (applet->name && strcmp(applet->name, name) == 0) {
      module = applet;
      break;
    }
  }
  return module;
} /* </module_search> */

/*
 * module_place_convert_string
 * module_place_convert_enum
 */
ModulusPlace
module_place_convert_string (const char *string)
{
  int idx;
  ModulusPlace place = PLACE_NONE;

  for (idx = 0; placement_[idx] != NULL; idx++)
    if (strcmp(placement_[idx], string) == 0) {
      place = (ModulusPlace)idx;
      break;
    }

  return place;
} /* </module_place_convert_string> */

const char *
module_place_convert_enum (ModulusPlace idx)
{
  return placement_[idx];
} /* </module_place_convert_enum> */

/*
 * moduli_remove
 */
GList *
moduli_remove (GList *moduli, Modulus *applet)
{
  GList *list;

  list = g_list_remove (moduli, applet);
  g_free (applet);

  return list;
} /* </moduli_remove> */

/*
 * moduli_space loads all modules from given path (and space(s))
 */
GList *
moduli_space (GList *moduli, const gchar *path, guint spaces, gpointer data)
{
  struct stat current;
  struct stat loaded;

  if (lstat(path, &current) == 0) {	/* we must be able to stat path */
    struct dirent **names;
    Modulus *applet, *modulus;
    int count = scandir(path, &names, NULL, alphasort);
    char file[FILENAME_MAX];
    char *name, *scan;
    int idx;

    for (idx = 0; idx < count; idx++) {
      name = names[idx]->d_name;

      if (name[0] == '.' || strcmp(strrchr(name, '.'), ".so"))
        continue;
      
      strcpy(file, name);	/* isolate file name without extension */
      scan = strrchr(file, '.');
      *scan = (char)0;

      /*
       * See if a module with the same name is already loaded.
       */
      if ((applet = module_search(moduli, file)) != NULL) {
        sprintf(file, "%s/%s", path, name);

        if (lstat(file, &current) != 0)	/* should not happen */
          continue;

        if (lstat(g_module_name(applet->module), &loaded) != 0)
          continue;

        if (loaded.st_mtime > current.st_mtime) {
          continue;	/* previously loaded module more current */
        }
        else {
          if ((modulus = module_load(file, spaces, data)) != NULL)
            moduli_remove(moduli, applet);
          else
            continue;	/* stick with previously loaded module */
        }
      }
      else {
        sprintf(file, "%s/%s", path, name);
        modulus = module_load(file, spaces, data);
      }

      if (modulus) { /* NULL if not found or not in spaces mask */
        moduli = g_list_append (moduli, modulus);
      }
    }
  }
  return moduli;
} /* </moduli_space> */

/*
 * killproc
 * killwait
 */
void
killproc (pid_t *proc, int sig)
{
  pid_t pid  = *proc;

  if (pid > 0) {
    kill(pid, sig);
    *proc = 0;
  }
} /* </killproc> */

void
killwait (pid_t *proc, int sig)
{
  int status = 0;
  pid_t pid  = *proc;

  if (pid > 0) {
    kill(pid, sig);
    wait(&status);
    *proc = 0;
  }
} /* </killwait> */

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
