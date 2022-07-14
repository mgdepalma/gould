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

#include "genesis.h"
#include "interface.h"

#include <ctype.h>
#include <unistd.h>
#include <dirent.h>

/*
 * Protected data structures.
*/
gboolean logout_ = FALSE;	/* applies only to interactive mode */
gboolean development_ = FALSE;	/* TRUE => development, FALSE => production */
unsigned short debug = 0;	/* must be declared in main program */

const char *Program;		/* published program name */
const char *Release;		/* program release version */

const char *Schema = "depot";	/* XML configuration schema */
const char *ConfigurationHeader =
"<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
"<%s>\n"
" <!-- Copyright (C) Generations Linux -->\n"
"\n";

const char *License = "(GNU General Public License version 3)";

const char *Terms =
" The program is developed for Generations Linux (http://www.softcraft.org/)"
" and distributed under\n"
" the terms and conditions of the GNU Public License (GPL) version 3.";

const char *Usage =
"License: GPL %s\n"
"\n"
"usage: %s [-h | -V | -d<xmlfile> [-c<catalog>] [-g] [-v[n]] [-l] <root>]\n"
"\n"
"\t-h print help usage (what you are reading)\n"
"\t-V print version information and exit\n"
"\t-d <xmlfile> Software Depot XML specification file\n"
"\t-c <catalog> software packages catalog to use\n"
"\t-g generate a script to install packages\n"
"\t-v [n] verbosity level [n]\n"
"\t-l logout on exit\n"
"\n"
"<xmlfile> specifies the software catalogs and packages. If no other\n"
"options are provided then the list of available catalogs is displayed.\n"
"\n"
"<catalog> is one of the available catalogs as specified in the <xmlfile>.\n"
"The combination of -d<xmlfile> and -c<catalog> will display the packages\n"
"adding -g an installation script is emitted.\n"
"\n"
"<root> is the installation path, default /\n"
"\n";

/*
 * dump_catalogs - print the list of available catalogs.
 */
static void
dump_catalogs (FILE *stream, Depot *depot)
{
  ConfigurationNode *item, *node;
  GList *list;

  for (list = depot->signatures; list != NULL; list = list->next) {
    item = (ConfigurationNode *)list->data;
    fprintf(stream, "<import \"%s\">\n", configuration_attrib (item, "file"));
  }

  for (list = depot->catalogs; list != NULL; list = list->next) {
    item = (ConfigurationNode *)list->data;
    fprintf(stream, "<catalog \"%s\">\n", configuration_attrib (item, "name"));

    for (node = item->next; node->depth > item->depth; node = node->next)
      if (strcmp(node->element, "include") == 0)
        fprintf(stream, "\t<include \"%s\">\n", node->next->element);
      else if (debug && node->attrib != NULL)
        fprintf(stream, "\t%s-%s\n", configuration_attrib (node, "name"),
                                     configuration_attrib (node, "version"));
  }
} /* </dump_catalogs> */

/*
 * get_catalog_node - return the node from a list of a given catalog name.
 */
ConfigurationNode *
get_catalog_node (GList *list, const char *name)
{
  ConfigurationNode *node = NULL;
  GList *iter;

  for (iter = list; iter != NULL; iter = iter->next)
    if (strcmp(name, configuration_attrib (iter->data, "name")) == 0)
      node = (ConfigurationNode *)iter->data;

  return node;
} /* </get_catalog_node> */

/*
 * get_catalog_list - available catalogs list from configuration cache
 */
GList *
get_catalog_list (ConfigurationNode *config)
{
  GList *list = NULL;
  ConfigurationNode *node;

  if (config && config->next)
    for (node = config->next; node != NULL; node = node->next)
      if (node->type == XML_READER_TYPE_ELEMENT &&
                                       strcmp(node->element, "catalog") == 0)
        list = g_list_append (list, node);

  return list;
} /* </get_catalog_list> */

/*
 * get_signature_list - available signatures list from configuration cache
 */
GList *
get_signature_list (ConfigurationNode *config)
{
  GList *list = NULL;
  ConfigurationNode *node;

  if (config && config->next)
    for (node = config->next; node != NULL; node = node->next)
      if (node->type == XML_READER_TYPE_ELEMENT &&
                                    strcmp(node->element, "signature") == 0)
        list = g_list_append (list, node);

  return list;
} /* </get_signature_list> */

/*
 * get_source_list - list of package source pathnames
 */
GList *
get_source_list (ConfigurationNode *config)
{
  GList *list = NULL;
  ConfigurationNode *chain;

  if ((chain = configuration_find (config, "source")) != NULL) {
    ConfigurationNode *item;
    gchar *path = configuration_path (chain);

    if (path != NULL) {
      list = g_list_append (list, path);

      /* FIXME: danger of an infinite loop */
      while (path != NULL) {
        item = configuration_find (chain, "path");  /* next <path> */

        if (item->next && item->next->next) {
          chain = item->next->next;

          if ((path = configuration_path (chain)) != NULL)
            list = g_list_append (list, path);
        }
      }
    }
  }

  if (list == NULL) {		/* hardcoded source pathnames */
    list = g_list_append (list, "/master/packages");
    list = g_list_append (list, "/usr/src/package/RPMS");
  }

  return list;
} /* </get_source_list> */

/*
 * get_dirent_list - gives the list of files for a given path
 */
static GList *
get_dirent_list (GList *start, const char *path)
{
  GList *list = start;
  struct dirent **names;
  int count = scandir(path, &names, NULL, alphasort);
  char *name;
  int idx;

  for (idx = 0; idx < count; idx++) {
    name = names[idx]->d_name;

    if (name[0] == '.')		/* skip all hidden files */
      continue;

    list = g_list_prepend (list, strdup(name));
  }

  return list;
} /* </get_dirent_list> */

/*
 * get_package_list - return link list of packages for a given catalog.
 */
GList *
get_package_list (GList *catalog, GList *list, const char *name)
{
  ConfigurationNode *item = get_catalog_node (catalog, name);

  if (item != NULL) {
    ConfigurationNode *node;

    for (node = item->next; node->depth > item->depth; node = node->next)
      if (strcmp(node->element, "include") == 0)
        list = get_package_list (catalog, list, node->next->element);
      else if (node->attrib != NULL)
        list = g_list_append (list, node);
  }
  else
    fprintf(stderr, "%s: %s: catalog is not available.\n", Program, name);

  return list;
} /* </get_package_list */

/*
 * print_package_list - emit list of packages for a given catalog.
 */
static int
print_package_list (Depot *global, const char* catalog)
{
  int missing = 0;	/* number of package files missing */
  const char *file, *name, *version, *scan;
  char path[MAX_PATHNAME];
  GList *iter;

  printf("<catalog \"%s\">\n", catalog);
  for (iter = global->packages; iter != NULL; iter = iter->next) {
    name    = configuration_attrib (iter->data, "name");
    version = configuration_attrib (iter->data, "version");

    sprintf(path, "%s-%s-", name, version);	/* partial basename */

    if ((scan = simple_list_find (global->files, path)) != NULL) {
      file = path_finder (global->source, scan);

      if (debug)
        printf("\t%s-%s\t(%s)\n", name, version, file);
      else
        printf("\t%s-%s\n", name, version);
    }
    else {
      if (debug)
        printf("\t%s-%s\t/** %s */\n", name, version, _("not found"));
      else
        printf("\t%s-%s\n", name, version);

      ++missing;
    }
  }

  return missing;
} /* </print_package_list */

/*
 * read_depot_configuration - read <xmlfile> and initialize cache.
 */
Depot *
read_depot_configuration (const char *path, const char *catalog)
{
  Depot *depot = NULL;
  ConfigurationNode *config = NULL;

  if (access(path, R_OK) != 0)
    printf("%s: %s: no such file or directory.\n", Program, path);
  else if ((config = configuration_read (path, Schema, FALSE))) {
    ConfigurationNode *item;
    GList *iter;

    depot = g_new0 (Depot, 1);

    depot->config = config;
    depot->catalogs = get_catalog_list (config);
    depot->signatures = get_signature_list (config);
    depot->source = get_source_list (config);

    for (iter = depot->source; iter != NULL; iter = iter->next)
      depot->files = get_dirent_list (depot->files, iter->data);

    depot->distribution = configuration_attrib (config, "distribution");
    depot->release = configuration_attrib (config, "release");

    if ((item = configuration_find (config, "arch")) != NULL)
      depot->arch = item->element;

    if (catalog != NULL)
      depot->packages = get_package_list (depot->catalogs, NULL, catalog);
  }

  return depot;
} /* </read_depot_configuration> */

/*
 * write_installation_script - generate installation script.
 *
 * Note: Only a single ..options="<value>" is handled, it is used
 *       to construct the command: 'rpm --<value> ..' when present.
 */
void
write_installation_script (Depot *global, const char *catalog, FILE *stream)
{
  static const char *script_ =
"#!/bin/sh\n"
"##\n"
"# %s (release %s) [%s architecture] %s\n"
"#\n"
"# Automatically generated with %s %s on %s\n"
"#\n"
"#\t\t%s\n"
"#\n"
"ROOT=%s\n"
"PACKAGE=\"rpm --root $ROOT\"\n"
"INSTALL=\"$PACKAGE -Uhv\"\n"
"\n"
"# Panic! no such file or directory.\n"
"panic()\n"
"{\n"
"  echo \"$1: No such file or directory.\"\n"
"  exit 1\n"
"}\n"
"\n"
"# Show program usage and exit.\n"
"usage()\n"
"{\n"
"  echo \"`basename $0` <installation path>\"\n"
"  exit 1\n"
"}\n"
"[ -n \"$ROOT\" ] || usage\n"
"[ -d \"$ROOT\" ] || panic $ROOT\n"
"\n";

  static const char *import_ =
"QUERY=`$PACKAGE -q %s`\n"
"[ $? -eq 0 ] || $PACKAGE --import %s\n"
"\n";

  gchar *arg1 = g_strdup_printf ("${1:-%s}", global->root);
  const char *root = (global->root) ? arg1 : "$1";
  const char *file, *name, *version, *options, *scan;
  static char path[MAX_PATHNAME];

  time_t clock;
  GList *iter;

  time(&clock);		/* obtain localtime */
  strftime((char*)path,sizeof(path)-1, "%Y-%m-%d %H:%M %Z",localtime(&clock));

  fprintf(stream, script_
         ,global->distribution, global->release, global->arch, catalog
         ,Program, Release, path
         ,License
         ,root
         );
  g_free (arg1);

  for (iter = global->signatures; iter != NULL; iter = iter->next) {
    ConfigurationNode *item = iter->data;
    name = configuration_attrib (item, "file");
    fprintf(stream, import_, item->next->element,
                             path_finder (global->source, name));
  }

  for (iter = global->packages; iter != NULL; iter = iter->next) {
    name    = configuration_attrib (iter->data, "name");
    version = configuration_attrib (iter->data, "version");
    options = configuration_attrib (iter->data, "options");

    sprintf(path, "%s-%s-", name, version);	/* partial basename */

    if ((scan = simple_list_find (global->files, path)) != NULL) {
      file = path_finder (global->source, scan);

      if (options != NULL)
        fprintf(stream, "$INSTALL --%s %s\n", options, file);
      else
        fprintf(stream, "$INSTALL %s\n", file);
    }
    else {
      fprintf(stream, "echo \"missing package: %s-%s\"\n", name, version);
    }
  }
} /* </write_installation_script> */

/*
 * genesis - main program method.
 */
int
main(int argc, char* argv[])
{
  const char *catalog = NULL;	/* default: no catalog given */
  const char *xmlfile = NULL;	/* default: no xmlfile given */

  gboolean generate = FALSE;	/* default: no script emitted */

  Depot *global;		/* instance of Depot class */
  int    flag;			/* getopt() selection */

  setlocale(LC_CTYPE, "");	/* set locale for other languages */
  Program = basename(argv[0]);	/* strip leading path for Progname */
  Release = "0.4.8";

  /* Change the process name using Program variable. */
  strncpy(argv[0], Program, strlen(argv[0]));
  setprogname (Program = argv[0]);
  opterr = 0;			/* disable invalid option messages */

  while ((flag = getopt (argc, argv, "hVv:ald:c:g")) != -1)
    switch (flag) {
      case 'h':
        printf(Usage, License, Program);
        exit(0);

      case 'V':
        printf("%s version %s %s\n", Program, Release, License);
        exit(0);

      case 'a':
        development_ = TRUE;
        break;
      case 'l':		/* consulted only in interactive mode */
        logout_ = TRUE;
        break;

      case 'c':
        catalog = optarg;
        break;
      case 'd':
        xmlfile = optarg;
        break;
      case 'g':
        generate = TRUE;
        break;

      case 'v':
        debug = atoi(optarg);
        break;

      default:
        if (optopt == 'v')
          debug = 1;
        else {
          printf("%s: invalid option, use -h for help usage.\n", Program);
          exit(1);
        }
    }

  /* revoke SUID priviledge if sudo is not allowed */
  if (debug) {
    printf("geteuid => %d, getuid => %d\n", geteuid(), getuid());
    printf("sudoallowed(%s) => %s\n", Program,
				sudoallowed(Program) ? "true" : "false");
  }
  if (geteuid() == 0 && getuid() != 0 && sudoallowed(Program) == FALSE)
    seteuid(getuid());

  if (xmlfile == NULL)    	/* draconian logic ignores other options */
    interactive (argc, argv);
  else if ((global = read_depot_configuration (xmlfile, catalog)) != NULL) {
    global->root = argv[optind];  /* last command line argument <root> */

    if (debug > 3)
      configuration_write (global->config, ConfigurationHeader, stdout);

    if (generate) {		  /* generate an installation script */
      if (global->packages != NULL)
        write_installation_script (global, catalog, stdout);
      else if (catalog == NULL)	/* no catalog specified for generate */
        printf("%s: %s\n", Program,
               _("-g can only be used with -d<xmlfile> -c<catalog> (see, -h)"));
    }
    else {			/* no script, show configuration gradients */
      printf("%s (release %s) [%s architecture]\n",
              global->distribution, global->release, global->arch);

      if (global->packages != NULL)
        print_package_list (global, catalog);
      else
        dump_catalogs (stdout, global);
    }
  }

  return 0;
} /* </main> */
