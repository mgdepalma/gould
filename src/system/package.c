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

#include "util.h"
#include "package.h"

#include <rpm/rpmcli.h>
#include <rpm/rpmlib.h>
#include <rpm/rpmlog.h>
#include <rpm/rpmdb.h>
#include <rpm/rpmts.h>

/* Set up a table of options. (rpm-4.11.1/rpmqv.c) */
static int quiet;
static struct poptOption _poptOptionsTable[] = {
  { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmQVSourcePoptTable, 0,
    "Query/Verify package selection options:",
    NULL },

  { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmQueryPoptTable, 0,
    "Query options (with -q or --query):",
    NULL },

  { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmVerifyPoptTable, 0,
    "Verify options (with -V or --verify):",
    NULL },

  { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmInstallPoptTable, 0,
    "Install/Upgrade/Erase options:",
    NULL },

  { "quiet", '\0', POPT_ARGFLAG_DOC_HIDDEN, &quiet, 0, NULL, NULL },

  { NULL, '\0', POPT_ARG_INCLUDE_TABLE, rpmcliAllPoptTable, 0,
    "Common options for all rpm modes and executables:",
    NULL },

  POPT_AUTOALIAS
  POPT_AUTOHELP
  POPT_TABLEEND
};

/*
* Private method instance variables.
*/
static GPtrArray  *_output;

static const char  _delimit = '\x1f';
static const char *_packageHeaderQuery =
"%{NAME}\n%{VERSION}\n%{RELEASE}\n%{SUMMARY}\n%{GROUP}\n%{SIZE}";

/*
* queryHeader (rpm-4.4.2.3/lib/query.c)
*/
static inline const char *
queryHeader (Header h, const char *qfmt)
{
  const char *errstr = "(unkown error)";
  const char *str;

  str = headerFormat(h, qfmt, &errstr);
  if (str == NULL)
    rpmlog(RPMLOG_ERR, _("incorrect format: %s\n"), errstr);

  return str;
} /* </queryHeader> */

/*
* captive_query_output (rpm-4.4.2.3/lib/query.c:showQueryPackage())
*/
int
captive_query_output (QVA_t qva, rpmts ts, Header h)
{
  if (qva->qva_queryFormat != NULL) {
    const char *str = queryHeader(h, qva->qva_queryFormat);

    if (str) {
      char *scan = strchr(str, _delimit);

      if (scan) *scan = '\0';		/* truncate at _delimit */
      g_ptr_array_add (_output, strdup(str));
      if (scan) *scan = _delimit;
      free((void *)str);
    }
  }
  return 0;
} /* </captive_query_output> */

/*
* pkg_cli_init - rpmcliInit() on error causes a program exit!
*/
poptContext
pkg_cli_init (int argc, char **argv, struct poptOption *optionsTable)
{
  poptContext context;
  const char *prog = argv[0];	/* must be accounted for in RPMOPTALIAS */
  int rc;

  rpmSetVerbosity(RPMLOG_NOTICE);
  context = poptGetContext(prog, argc, (const char **)argv, optionsTable, 0);
  poptReadConfigFile(context, RPMOPTALIAS);
  poptReadDefaultConfig(context, 1);

  /* Process all options, whine if unknown. */
  while ((rc = poptGetNextOpt(context)) > 0) {
    fprintf(stderr,_("%s: option table misconfigured (%d)\n"), prog, rc);
    return NULL;
  }

  /* Read rpm configuration (if not already read). */
  rpmcliConfigured();

  return context;
} /* </pkg_cli_init> */

/*
* pkg_cli_call
*/
GPtrArray *
pkg_cli_call (int argc, char **argv)
{
  poptContext context;
  GPtrArray *result = NULL;

  QVA_t qva = &rpmQVKArgs;
  memset(qva, 0, sizeof(struct rpmQVKArguments_s));

  if ((context = pkg_cli_init (argc, argv, _poptOptionsTable))) {
    rpmts ts = rpmtsCreate();

    _output = g_ptr_array_new ();
    qva->qva_showPackage = captive_query_output;

    rpmcliQuery(ts, qva, (ARGV_const_t)poptGetArgs(context));
    result = _output;

    context = rpmcliFini(context);
    ts = rpmtsFree(ts);
  }
  return result;
} /* </pkg_cli_call> */

/*
* pkg_cli_query uses RPM API for command line interface
*/
GPtrArray *
pkg_cli_query (Package *package, const char *query, const char *source)
{
  gchar *formula = g_strdup_printf ("%s%c", query, _delimit);
  gchar *dbpath  = g_strdup_printf ("%s%s", package->root, RPM_DEFAULT_DBPATH);

  char *argv_file[6] = {
    RPMPROGRAM
   ,"-qp"
   ,"--qf"
   ,formula
   ,(char *)source
   ,"\0"
  };

  char *argv_db[8] = {
    RPMPROGRAM
   ,"--dbpath"
   ,dbpath
   ,"-q"
   ,"--qf"
   ,formula
   ,(char *)source
   ,"\0"
  };

  int argc = (package->file) ? 5 : 7;
  char **argv = (package->file) ? argv_file : argv_db;
  GPtrArray *result = pkg_cli_call (argc, argv);

  g_free (formula);
  g_free (dbpath);

  return result;
} /* </pkg_cli_query> */

/*
* pkg_cli_query_info variant of pkg_cli_query() to get bulk information
*/
GPtrArray *
pkg_cli_query_info (Package *package, QueryMode mode)
{
  gchar *formula = g_strdup_printf ("[%%{FILENAMES}\n]%c", _delimit);
  gchar *dbpath = g_strdup_printf ("%s%s", package->root, RPM_DEFAULT_DBPATH);
  gchar *source = (package->file)
                  ? g_strdup_printf ("%s/%s", package->root, package->file)
                  : g_strdup_printf ("%s-%s", package->name, package->version);

  char *argv_file[6] = {
    RPMPROGRAM
   ,(mode == QueryInformation) ? "-q" : "-qp"
   ,(mode == QueryInformation) ? "-i" : "--qf"
   ,(mode == QueryInformation) ? "-p" : formula
   ,source
   ,"\0"
  };

  char *argv_db[8] = {
    RPMPROGRAM
   ,"--dbpath"
   ,dbpath
   ,"-q"
   ,(mode == QueryInformation) ? "-i" : "--qf"
   ,(mode == QueryInformation) ? "-v" : formula
   ,source
   ,"\0"
  };

  int argc = (package->file) ? 5 : 7;
  char **argv = (package->file) ? argv_file : argv_db;
  GPtrArray *result = pkg_cli_call (argc, argv);

  g_free (dbpath);
  g_free (formula);
  g_free (source);

  return result;
} /* </pkg_cli_query_info> */

/*
* pkg_query_info handle both PGP_MAGIC and RPM_MAGIC
*/
GPtrArray *
pkg_query_info (Package *package, QueryMode mode)
{
  GPtrArray *result;

   if (!(package->file && package->magic == PGP_MAGIC))
     result = pkg_cli_query_info (package, mode);
   else {
     result = g_ptr_array_new ();

     if (mode == QueryInformation)
       g_ptr_array_add (result, _("PGP armored data public key block"));
     else
       g_ptr_array_add (result, _("<no applicable list>"));
   }

  return result;
} /* </pkg_query_info> */

/*
* pkg_parse_header parse header data
*/
void
pkg_parse_header (Package *package, gchar *header)
{
  package->name    = strtok(header, "\n");
  package->version = strtok(NULL, "\n");
  package->release = strtok(NULL, "\n");
  package->summary = strtok(NULL, "\n");
  package->group   = strtok(NULL, "\n");
  package->size    = strtok(NULL, "\n");
} /* </pkg_parse_header> */

/*
* pkg_dirent_list equivalent to pkg_query_list() using directory path
*/
GList *
pkg_dirent_list (const char *path)
{
  struct dirent **names;
  int count = scandir(path, &names, NULL, alphasort);
  char source[MAX_PATHNAME];
  GList *list = NULL;

  GPtrArray *result;
  Package *package;
  char *name;
  int magic;
  int idx;

  /* Iterate directory path forming list of packages. */
  vdebug (3, "pkg_dirent_list: path => %s\n", path);

  for (idx = 0; idx < count; idx++) {
    gchar *header = NULL;	/* header data string newline delimited */
    name = names[idx]->d_name;

    if (name[0] == '.')         /* skip all hidden files */
      continue;

    sprintf(source, "%s/%s", path, name);
    magic = get_file_magic (source);

    if (magic != PGP_MAGIC && magic != RPM_MAGIC)
      continue;

    /* Allocate memory for a struct _Package */
    package = g_new0 (Package, 1);

    package->file  = strdup(name);
    package->magic = magic;
    package->root  = path;		/* =note= no memory allocated */

    if (magic == PGP_MAGIC) {
      header = g_strdup_printf ("%s\n%s\n%s\n%s\n%s\n%d", name, "-", "-",
                                "PGP armored data public key block",
                                "Public Keys", 0);
    }
    else {
      if ((result = pkg_cli_query (package, _packageHeaderQuery, source)))
        header = g_ptr_array_index (result, 0);
      else {
        free((void *)package->file);
        g_free (package);
      }
    }

    if (header) {			/* parse header data string */
      pkg_parse_header (package, header);
      list = g_list_prepend (list, package);

      vdebug (3, "\t%s-%s [%s] %.10s..\n", package->name, package->version,
              package->group, package->summary);
    }
  }

  return list = g_list_sort (list, pkg_name_sort);
} /* </pkg_dirent_list> */

/*
* pkg_query_list: RPMPROGRAM -qa --queryformat {_packageHeaderQuery}
*/
GList *
pkg_query_list (const char *root)
{
  gchar *dbpath  = g_strdup_printf ("%s%s", root, RPM_DEFAULT_DBPATH);
  gchar *formula = g_strdup_printf ("%s%c", _packageHeaderQuery, _delimit);

  int argc = 6;
  char *argv[7] = {
    RPMPROGRAM
   ,"--dbpath"
   ,dbpath
   ,"-qa"
   ,"--qf"
   ,formula
   ,"\0"
  };

  GPtrArray *result = pkg_cli_call (argc, argv);
  GList *list = NULL;

  vdebug (3, "pgk_query_list: dbpath => %s\n", dbpath);
  g_free (formula);
  g_free (dbpath);

  if (result) {
    int idx;

    for (idx = 0; idx < result->len; idx++) {
      Package *package = g_new0 (Package, 1);

      package->file  = NULL;
      package->magic = MAX_MAGIC;
      package->root  = root;		/* =note= no memory allocated */

      pkg_parse_header (package, g_ptr_array_index (result, idx));
      list = g_list_prepend (list, package);

      vdebug (3, "\t%s-%s [%s] %.10s..\n", package->name, package->version,
              package->group, package->summary);
    }
    /* g_ptr_array_free (result, TRUE); */
  }

  return list;
} /* </pkg_query_list> */

/*
* alpha_name_sort
* pkg_name_sort
*/
gint
alpha_name_sort (gconstpointer a, gconstpointer b)
{
  return g_utf8_collate (a, b);
} /* </alpha_name_sort> */

gint
pkg_name_sort (gconstpointer a, gconstpointer b)
{
  return g_utf8_collate (((Package *)a)->name, ((Package *)b)->name);
} /* </pkg_name_sort> */
