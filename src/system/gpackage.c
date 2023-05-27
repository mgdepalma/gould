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

#include "interface.h"
#include "xpmglyphs.h"

/*
 * Private data structures.
*/
#define MAX_PKGNAME_LENGTH 15
#define MAX_SUMMARY_LENGTH 30

enum
{
  MARK_COLUMN,
  NAME_COLUMN,
  VERS_COLUMN,
  ARCH_COLUMN,
  DESC_COLUMN,
  DATA_COLUMN,
  N_COLUMNS
};

enum
{
  ABOUT_PAGE,		/* about page */
  DEPOT_PAGE,		/* installed packages and updates */
  LAST_PAGE		/* last page marker */
};

const char *About =
" The program presents two notebook style tabs, one for viewing"
" and managing packages already installed\n"
" and one showing updates available from /var/spool/rpm/ directory for"
" installation and/or file removal.\n"
"\n"
" Each directory path can be changed. ROOT installation path"
" likely will always be /. UPDATES path is\n"
" expected to contain RPM files.\n";

const char *License = "(GNU General Public License version 3)";

const char *Terms =
" The program is developed for Generations Linux (http://www.softcraft.org/)"
" and distributed under\n"
" the terms and conditions of the GNU Public License (GPL) version 3.";

const char *Usage =
"License: GPL %s\n"
"\n"
"usage: %s [-d[n] | -h | -v <root>]\n"
"\n"
"\t-d debug level [n (default => 1)]\n"
"\t-h print help usage (what you are reading)\n"
"\t-v print version information and exit\n"
"\n"
"<root> is the installation path, default / {i am root}\n"
"\n";

GlobalData *global_;		/* global program data singleton */
unsigned short debug = 0;	/* must be declared in main program */

const char *Program;		/* published program name */
const char *Release;		/* program release version */

/*
 * finis clean program exit
 */
static void
finis (GtkWidget *widget, gpointer data)
{
  gtk_main_quit ();
} /* </finis> */

/*
 * depot_changed
 */
static void
depot_changed (GtkNotebook *book, GtkNotebookPage *page,
                 gint index, gpointer data)
{
  if (index == INSTALL) {
    gtk_widget_hide (global_->install);
    gtk_widget_hide (global_->remove);
    gtk_widget_show (global_->verify);
    gtk_widget_show (global_->erase);
  }
  else {
    gtk_widget_show (global_->install);
    gtk_widget_show (global_->remove);
    gtk_widget_hide (global_->verify);
    gtk_widget_hide (global_->erase);
  }
} /* </depot_changed> */

/*
 * depot_column_header
 */
static void
depot_column_header (GtkWidget *view, const char *name, guint place)
{
  GtkTreeViewColumn *column;
  GtkCellRenderer   *renderer;

  column = gtk_tree_view_column_new();
  gtk_tree_view_column_set_title (column, name);
  gtk_tree_view_append_column (GTK_TREE_VIEW(view), column);

  renderer = gtk_cell_renderer_text_new();
  gtk_tree_view_column_pack_start (column, renderer, TRUE);
  gtk_tree_view_column_add_attribute (column, renderer, "text", place);
} /* </depot_column_header> */

/*
 * depot_layout
 */
static GtkWidget *
depot_layout (GlobalDepot *depot)
{
  GtkWidget *layout = gtk_hbox_new (FALSE, 2);
  GtkWidget *label, *info, *layer, *notebook, *scrolled, *view;
  GtkTreeStore *store;

  /* Add a scrolled window for the selections. */
  scrolled = gtk_scrolled_window_new (NULL, NULL);
  gtk_box_pack_start (GTK_BOX(layout), scrolled, FALSE, FALSE, 0);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW(scrolled),
                                  GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_widget_show (scrolled);

  /* [packages]Left side of layout. */
  store = depot->store = gtk_tree_store_new (N_COLUMNS,
                                             G_TYPE_STRING,
                                             G_TYPE_STRING,
                                             G_TYPE_STRING,
                                             G_TYPE_STRING,
                                             G_TYPE_STRING,
                                             G_TYPE_POINTER);

  view = depot->viewer = gtk_tree_view_new_with_model (GTK_TREE_MODEL(store));
  gtk_container_add (GTK_CONTAINER(scrolled), view);
  gtk_widget_show (view);

  /* Add column headers. */
  depot_column_header (view, "", MARK_COLUMN);
  depot_column_header (view, _("Group | Package"), NAME_COLUMN);
  depot_column_header (view, _("Version-Release"), VERS_COLUMN);
  depot_column_header (view, _("Architecture"), ARCH_COLUMN);
  depot_column_header (view, _("Summary"), DESC_COLUMN);

  /* [depots]Right side of layout. */
  notebook = gtk_notebook_new ();
  gtk_notebook_set_show_border (GTK_NOTEBOOK(notebook), FALSE);
  gtk_notebook_set_tab_pos (GTK_NOTEBOOK (notebook), GTK_POS_TOP);

  /* Description tab. */
  layer = gtk_vbox_new (FALSE, 1);
  label = gtk_label_new (_("Description"));
  gtk_notebook_append_page (GTK_NOTEBOOK(notebook), layer, label);
  gtk_widget_show (layer);

  scrolled = gtk_scrolled_window_new (NULL, NULL);
  gtk_box_pack_start (GTK_BOX(layer), scrolled, TRUE, TRUE, 0);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW(scrolled),
                                  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_widget_show (scrolled);

  depot->describe = gtk_text_buffer_new (NULL);
  info = gtk_text_view_new_with_buffer (depot->describe);
  gtk_container_add (GTK_CONTAINER(scrolled), info);
  gtk_widget_show (info);

  /* File list tab. */
  layer = gtk_vbox_new (FALSE, 1);
  label = gtk_label_new (_("File list"));
  gtk_notebook_append_page (GTK_NOTEBOOK(notebook), layer, label);
  gtk_widget_show (layer);

  scrolled = gtk_scrolled_window_new (NULL, NULL);
  gtk_box_pack_start (GTK_BOX(layer), scrolled, TRUE, TRUE, 0);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW(scrolled),
                                  GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_widget_show (scrolled);

  depot->filelist = gtk_text_buffer_new (NULL);
  info = gtk_text_view_new_with_buffer (depot->filelist);
  gtk_container_add (GTK_CONTAINER(scrolled), info);
  gtk_widget_show (info);

  gtk_box_pack_start (GTK_BOX(layout), notebook, TRUE, TRUE, 0);
  gtk_notebook_set_current_page (GTK_NOTEBOOK(notebook), 0);
  gtk_widget_show (notebook);

  return layout;
} /* </depot_layout> */

/*
 * depot_selection
 * depot_selection_handler
 */
static void
depot_selection (GtkTreeModel *model, GtkTreePath *path,
                   GtkTreeIter *iter, GlobalDepot *depot)
{
  Package *package;
  gint page = gtk_notebook_get_current_page (global_->packages);

  gtk_tree_model_get (model, iter,
                      DATA_COLUMN, &package,
                      -1);
  if (package) {
    GPtrArray *details;
    gint len;

    if ((details = pkg_query_info (package, QueryInformation))) {
      char *text[MAX_PATHNAME * details->len];
      len = pkg_query_details (details, text);

      //if (debug > 2) printf("%s text => %s\n", __func__, text);
      gtk_text_buffer_set_text (depot->describe, text, len);
      g_ptr_array_free (details, TRUE);
    }

    if ((details = pkg_query_info (package, QueryListing))) {
      char *text[MAX_PATHNAME * details->len];
      len = pkg_query_details (details, text);

      gtk_text_buffer_set_text (depot->filelist, text, len);
      g_ptr_array_free (details, TRUE);
    }

    if (page == DEPOT_PAGE) {
      gtk_widget_set_sensitive (global_->erase, TRUE);
      gtk_widget_set_sensitive (global_->verify, TRUE);
    }
    else {
      gtk_widget_set_sensitive (global_->install, TRUE);
      gtk_widget_set_sensitive (global_->remove, TRUE);
    }
  }
} /* </depot_selection> */

static void
depot_selection_handler (GtkTreeSelection *selection, GlobalDepot *depot)
{
  GtkTreeModel *model;
  GList *list = gtk_tree_selection_get_selected_rows (selection, &model);

  if (list && list->data) {	/* we only process the first selection */
    GtkTreeIter  iter;
    GtkTreePath *path = list->data;

    if (debug > 2) {
      gchar *s_path = gtk_tree_path_to_string(path);
      printf("%s path => %s\n", __func__, s_path);
      g_free(s_path);
    }
    gtk_tree_model_get_iter (model, &iter, path);
    depot_selection (model, path, &iter, depot);
    global_->selection = selection;
  }
} /* </depot_selection_handler> */

static inline void
depot_add_package (Package *package, GtkTreeStore *store, GtkTreeIter *node)
{
  char pkgname[strlen(package->name) + 1];
  char version[strlen(package->version) + strlen(package->release) + 2];
  char architecture[strlen(package->arch) + 1];
  char summary[strlen(package->summary) + 1];

  strcpy(pkgname, package->name);
  sprintf(version, "%s-%s", package->version, package->release);
  strcpy(architecture, package->arch);
  strcpy(summary, package->summary);

  if (strlen(pkgname) > MAX_PKGNAME_LENGTH) {
    pkgname[MAX_PKGNAME_LENGTH - 2] = '.';
    pkgname[MAX_PKGNAME_LENGTH - 1] = '.';
    pkgname[MAX_PKGNAME_LENGTH] = '\0';
  }

  if (strlen(summary) > MAX_SUMMARY_LENGTH) {
    summary[MAX_SUMMARY_LENGTH - 2] = '.';
    summary[MAX_SUMMARY_LENGTH - 1] = '.';
    summary[MAX_SUMMARY_LENGTH] = '\0';
  }

  gtk_tree_store_set (store, node,
                      MARK_COLUMN, "",
                      NAME_COLUMN, pkgname,
                      VERS_COLUMN, version,
                      ARCH_COLUMN, architecture,
                      DESC_COLUMN, summary,
                      DATA_COLUMN, package,
                      -1);
} /* </depot_add_package> */

/*
 * depot_group_hash
 */
static void inline
depot_group_hash (GHashTable *hash, GList *list)
{
  GList *iter;
  g_hash_table_remove_all (hash);

  for (iter = list; iter; iter = iter->next) {
    Package *package = iter->data;
    GList *members = g_hash_table_lookup (hash, package->group);

    members = g_list_prepend (members, (gpointer)package);
    g_hash_table_replace (hash, (gpointer)package->group, members);
  }
} /* </depot_group_hash> */

/*
 * depot_update
 */
static void
depot_update (GlobalDepot *depot, GHashTable *groups)
{
  GtkTreeIter   cell;
  GtkTreeIter   child;
  GtkTreeStore *store = depot->store;
  GList *iter, *list = g_hash_table_get_keys (groups);

  /* Sort groups by name. */
  list = g_list_sort (list, alpha_name_sort);
  gtk_tree_store_clear (store);

  for (iter = list; iter; iter = iter->next) {
    GList *part, *members = g_hash_table_lookup (groups, iter->data);
    members = g_list_sort (members, pkg_name_sort);

    gtk_tree_store_append (store, &cell, NULL);
    gtk_tree_store_set (store, &cell,
                        MARK_COLUMN, "",
                        NAME_COLUMN, iter->data,
                        -1);

    for (part = members; part; part = part->next) {
      gtk_tree_store_append (store, &child, &cell);
      depot_add_package (part->data, store, &child);
    }
  }

  g_list_free (list);
} /* </depot_update> */

/*
 * gpackage_induct
 * gpackage_remove
 */
static void
_prototype_helper (GtkTreeModel *model, GtkTreePath *path,
                   GtkTreeIter *iter, gpointer data)
{
  const char *name;
  gint page = gtk_notebook_get_current_page (global_->packages);

  gtk_tree_model_get (model, iter,
                      NAME_COLUMN, &name,
                      -1);

  printf("%s selection => %s\n", (page == 0) ? "installed" : "updates", name);
} /* </_prototype_helper> */

static void
gpackage_induct (GtkWidget *widget, gpointer data)
{
  gtk_tree_selection_selected_foreach (global_->selection,
          (GtkTreeSelectionForeachFunc)_prototype_helper,
                                       GINT_TO_POINTER(INSTALL));
} /* </gpackage_induct> */

static void
gpackage_remove (GtkWidget *widget, gpointer data)
{
  gtk_tree_selection_selected_foreach (global_->selection,
          (GtkTreeSelectionForeachFunc)_prototype_helper,
                                       GINT_TO_POINTER(UPDATES));
} /* </gpackage_remove> */

/*
 * gpackage_page
 */
void
gpackage_page (GtkWidget *widget, gpointer data)
{
  gint page = GPOINTER_TO_INT(data);
  GtkEntry *entry = GTK_ENTRY(global_->dirent[page]);

  const gchar *path = gtk_entry_get_text (entry);
  gboolean changed = strcmp(global_->path[page], path) != 0;

  if (changed) {
    gchar *dbpath = (page == INSTALL)
                    ? g_strdup_printf ("%s%s", path, RPM_DEFAULT_DBPATH)
                    : g_strdup (path);

    if (access(dbpath, R_OK) != 0)
      gtk_entry_set_text (entry, global_->path[page]);
    else {
      GList *list;

      gdk_window_set_cursor (widget->window, global_->cursor[BUSY_CURSOR]);
      gdk_display_sync (gtk_widget_get_display (widget));

      list = (page == INSTALL) ? pkg_query_list(path) : pkg_dirent_list(path);

      depot_group_hash (global_->hashing[page], list);
      depot_update (global_->depot[page], global_->hashing[page]);
      g_list_free (list);	/* TODO free up all memory */

      gdk_window_set_cursor (widget->window, global_->cursor[NORMAL_CURSOR]);
      gtk_notebook_set_current_page (global_->packages, page);

      g_free ((gchar *)global_->path[page]);
      global_->path[page] = g_strdup (path);
    }
    g_free (dbpath);
  }
} /* </gpackage_page> */

/*
 * gpackage_installed
 * gpackage_updates
 */
static GtkWidget *
gpackage_installed (const char *root)
{
  GlobalDepot *depot = g_new0 (GlobalDepot, 1);
  GtkWidget *layout = depot_layout (depot);
  GtkWidget *view = depot->viewer;

  GtkTreeSelection *selection;
  GList *list = pkg_query_list (root);

  global_->depot[INSTALL]   = depot;
  global_->hashing[INSTALL] = g_hash_table_new (g_str_hash, g_str_equal);
  global_->path[INSTALL]    = g_strdup (root);

  /* Populate installed packages layout. */
  depot_group_hash (global_->hashing[INSTALL], list);
  depot_update (depot, global_->hashing[INSTALL]);
  g_list_free (list);	/* TODO free up all memory */

  /* Add callback for selection change. */
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW(view));
  gtk_tree_selection_set_mode (selection, GTK_SELECTION_MULTIPLE);

  g_signal_connect (G_OBJECT(selection), "changed",
                    G_CALLBACK(depot_selection_handler), depot);
  return layout;
} /* </gpackage_installed> */

static GtkWidget *
gpackage_updates (const char *path)
{
  GlobalDepot *depot = g_new0 (GlobalDepot, 1);
  GtkWidget *layout = depot_layout (depot);
  GtkWidget *view = depot->viewer;

  GtkTreeSelection *selection;
  GList *list = pkg_dirent_list (path);

  global_->depot[UPDATES]   = depot;
  global_->hashing[UPDATES] = g_hash_table_new (g_str_hash, g_str_equal);
  global_->path[UPDATES]    = g_strdup (path);

  /* Populate package updates layout. */
  depot_group_hash (global_->hashing[UPDATES], list);
  depot_update (depot, global_->hashing[UPDATES]);
  g_list_free (list);	/* TODO free up all memory */

  /* Add callback for selection change. */
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW(view));
  gtk_tree_selection_set_mode (selection, GTK_SELECTION_MULTIPLE);

  g_signal_connect (G_OBJECT(selection), "changed",
                    G_CALLBACK(depot_selection_handler), depot);

  global_->depot[UPDATES] = depot;

  return layout;
} /* </gpackage_updates> */


/*
 * gpackage_navigate
 */
static GtkWidget *
gpackage_navigate (const char *root, const char *path)
{
  GtkWidget *layer = gtk_hbox_new (FALSE, 0);
  GtkWidget *button, *entry, *image, *label;

  /* Leading widget image. */
  image = gtk_image_new_from_stock (GTK_STOCK_COPY, GTK_ICON_SIZE_BUTTON);
  gtk_box_pack_start (GTK_BOX(layer), image, FALSE, FALSE, 6);
  gtk_widget_show (image);

  /* Widget for setting root directory. */
  label = gtk_label_new (_("ROOT:"));
  gtk_box_pack_start (GTK_BOX(layer), label, FALSE, FALSE, 2);
  gtk_widget_show (label);

  entry = gtk_entry_new();
  gtk_entry_set_text (GTK_ENTRY(entry), root);
  gtk_widget_set_size_request (entry, 180, 20);
  gtk_box_pack_start (GTK_BOX(layer), entry, FALSE, FALSE, 0);
  gtk_widget_show (global_->dirent[INSTALL] = entry);

  button = stock_button_new ("", GTK_STOCK_DIRECTORY, 0);
  gtk_button_set_relief (GTK_BUTTON(button), GTK_RELIEF_NONE);
  gtk_box_pack_start (GTK_BOX(layer), button, FALSE, FALSE, 0);
  gtk_widget_show (button);

  g_signal_connect (G_OBJECT(button), "clicked",
                    G_CALLBACK(gpackage_page), GINT_TO_POINTER(INSTALL));

  /* Add spacing between ROOT and UPDATES widgets. */
  label = gtk_label_new ("");
  gtk_box_pack_start (GTK_BOX(layer), label, FALSE, FALSE, 10);
  gtk_widget_show (label);

  /* Widget for setting updates directory. */
  label = gtk_label_new (_("UPDATES:"));
  gtk_box_pack_start (GTK_BOX(layer), label, FALSE, FALSE, 2);
  gtk_widget_show (label);

  entry = gtk_entry_new();
  gtk_entry_set_text (GTK_ENTRY(entry), path);
  gtk_widget_set_size_request (entry, 300, 20);
  gtk_box_pack_start (GTK_BOX(layer), entry, FALSE, FALSE, 0);
  gtk_widget_show (global_->dirent[UPDATES] = entry);

  button = stock_button_new ("", GTK_STOCK_DIRECTORY, 0);
  gtk_button_set_relief (GTK_BUTTON(button), GTK_RELIEF_NONE);
  gtk_box_pack_start (GTK_BOX(layer), button, FALSE, FALSE, 0);
  gtk_widget_show (button);

  g_signal_connect (G_OBJECT(button), "clicked",
                    G_CALLBACK(gpackage_page), GINT_TO_POINTER(UPDATES));

  /* Final widget, information button. */
  button = stock_button_new ("", GTK_STOCK_INFO, 0);
  gtk_button_set_relief (GTK_BUTTON(button), GTK_RELIEF_NONE);
  gtk_box_pack_end (GTK_BOX(layer), button, FALSE, FALSE, 8);
  gtk_widget_show (button);

  g_signal_connect (G_OBJECT(button), "clicked",
                    G_CALLBACK(navigation_page), GINT_TO_POINTER(ABOUT_PAGE));
  return layer;
} /* </gpackage_navigate> */

/*
 * gpackage_about
 * gpackage_layout
 */
GtkWidget *
gpackage_about (void)
{
  GtkWidget *layout = gtk_vbox_new (FALSE, 2);
  GtkWidget *layer  = navigation_frame (layout);

  GtkWidget *button, *info, *label;
  gchar *description;

  /* Add contents.. */
  info = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX(layer), info, FALSE, FALSE, 25);
  gtk_widget_show (info);

  button = xpm_image_scaled (ICON_PACKAGE, 32, 32);
  gtk_box_pack_start (GTK_BOX(info), button, FALSE, FALSE, 2);
  gtk_widget_show (button);

  description = g_strdup_printf (
                  "<span font_desc=\"Arial 16\">%s %s</span>", getprogname(),
                  _("is a software package manager for RPM based systems.")
                );

  label = gtk_label_new (description);
  gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
  gtk_box_pack_start (GTK_BOX(info), label, FALSE, FALSE, 2);
  gtk_widget_show (label);
  g_free (description);

  description = g_strdup_printf ("<span font_desc=\"Arial 14\">%s\n%s</span>",
                                 _(About), _(Terms));

  info = gtk_label_new (description);
  gtk_label_set_use_markup (GTK_LABEL (info), TRUE);
  gtk_misc_set_alignment (GTK_MISC(info), 0.06, 0.5);
  gtk_box_pack_start (GTK_BOX(layer), info, FALSE, FALSE, 9);
  gtk_widget_show (info);
  g_free (description);

  /* Add button to go back to DEPOT_PAGE */
  info = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX(layout), info, FALSE, FALSE, 0);
  gtk_widget_show (info);

  button = stock_button_new (_("Continue"), GTK_STOCK_REDO, 8);
  gtk_box_pack_end (GTK_BOX(info), button, FALSE, FALSE, 8);
  gtk_widget_show (button);

  g_signal_connect (G_OBJECT(button), "clicked",
                    G_CALLBACK(navigation_page), GINT_TO_POINTER(DEPOT_PAGE));
  return layout;
} /* </gpackage_about> */

GtkWidget *
gpackage_layout (const char *root, const char *path)
{
  GtkWidget *layout = gtk_vbox_new (FALSE, 0);
  GtkWidget *button, *label, *layer, *notebook;

  /* Top navigation widgets. */
  layer = gpackage_navigate (root, path);
  gtk_box_pack_start (GTK_BOX(layout), layer, FALSE, FALSE, 2);
  gtk_widget_show (layer);

  /* Create a new notebook, place the position of the tabs. */
  notebook = gtk_notebook_new ();
  global_->packages = GTK_NOTEBOOK(notebook);

  gtk_notebook_set_show_border (GTK_NOTEBOOK(notebook), FALSE);
  gtk_notebook_set_tab_pos (GTK_NOTEBOOK (notebook), GTK_POS_TOP);

  /* Installed packaged tab from RPM database. */
  layer = gpackage_installed (root);
  label = gtk_label_new (_("Installed"));
  gtk_notebook_append_page (GTK_NOTEBOOK(notebook), layer, label);
  gtk_widget_show (layer);

  /* RPMs from update spool area. */
  layer = gpackage_updates (path);
  label = gtk_label_new (_("Updates"));
  gtk_notebook_append_page (GTK_NOTEBOOK(notebook), layer, label);
  gtk_widget_show (layer);

  gtk_container_add (GTK_CONTAINER(layout), notebook);
  gtk_notebook_set_current_page (GTK_NOTEBOOK(notebook), 0);
  gtk_widget_show (notebook);

  g_signal_connect (G_OBJECT(notebook), "switch_page",
                    G_CALLBACK(depot_changed), global_);

  /* Create action buttons. */
  layer = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX(layout), layer,  FALSE, FALSE, 2);
  gtk_widget_show (layer);

  /* Button to verify packages. */
  button = stock_button_new (_("Verify"), GTK_STOCK_APPLY, 5);
  gtk_box_pack_start (GTK_BOX(layer), button, FALSE, FALSE, 5);
  gtk_widget_set_sensitive (global_->verify = button, FALSE);

  /*
  g_signal_connect (G_OBJECT(button), "clicked",
                    G_CALLBACK(gpackage_verify), NULL);
  */

  /* Buttons to erase (uninstall) packages. */
  button = stock_button_new (_("Remove"), GTK_STOCK_REMOVE, 5);
  gtk_box_pack_start (GTK_BOX(layer), button, FALSE, FALSE, 0);
  gtk_widget_set_sensitive (global_->erase = button, FALSE);

  g_signal_connect (G_OBJECT(button), "clicked",
                    G_CALLBACK(gpackage_remove), NULL);

  /* Button to install/update packages. */
  button = stock_button_new (_("Install/update"), GTK_STOCK_COPY, 5);
  gtk_box_pack_start (GTK_BOX(layer), button, FALSE, FALSE, 5);
  gtk_widget_set_sensitive (global_->install = button, FALSE);

  g_signal_connect (G_OBJECT(button), "clicked",
                    G_CALLBACK(gpackage_induct), NULL);

  /* Button to remove RPM files. */
  button = stock_button_new (_("Delete"), GTK_STOCK_DELETE, 5);
  gtk_box_pack_start (GTK_BOX(layer), button, FALSE, FALSE, 0);
  gtk_widget_set_sensitive (global_->remove = button, FALSE);

  /*
  g_signal_connect (G_OBJECT(button), "clicked",
                    G_CALLBACK(gpackage_remove), NULL);
  */

  button = stock_button_new (_("Quit"), GTK_STOCK_QUIT, 5);
  gtk_box_pack_end (GTK_BOX(layer), button, FALSE, FALSE, 5);
  gtk_widget_show (button);

  g_signal_connect (G_OBJECT(button), "clicked",
                      G_CALLBACK(finis), NULL);
  return layout;
} /* </gpackage_layout> */

/*
 * constructor main notebook pages
 */
GtkWidget *
constructor (GtkWidget *widget, const char *root)
{
  GtkWidget *layer, *layout;

  /* Set busy cursor while we are contructing the pages. */
  gdk_window_set_cursor (widget->window, global_->cursor[BUSY_CURSOR]);
  gdk_display_sync (gtk_widget_get_display (widget));

  /* Construct layout starting with the about page. */
  layout = gtk_notebook_new ();
  global_->pages = GTK_NOTEBOOK(layout);

  gtk_notebook_set_show_border (GTK_NOTEBOOK(layout), FALSE);
  gtk_notebook_set_show_tabs (GTK_NOTEBOOK(layout), FALSE);

  layer = gpackage_about();
  gtk_notebook_append_page (GTK_NOTEBOOK(layout), layer, NULL);
  gtk_widget_show (layer);

  /* Add the installed and updates packages page. */
  layer = gpackage_layout (root, RPM_DEFAULT_SPOOL);
  gtk_notebook_append_page (GTK_NOTEBOOK(layout), layer, NULL);
  gtk_widget_show (layer);

  gtk_widget_show (global_->verify);
  gtk_widget_show (global_->erase);

  /* All done, set normal cursor. */
  gdk_window_set_cursor (widget->window, global_->cursor[NORMAL_CURSOR]);
  gtk_notebook_set_current_page (GTK_NOTEBOOK(layout), DEPOT_PAGE);

  return layout;
} /* </constructor> */

/*
 * interactive graphical user interface
 */
int
main (int argc, char *argv[])
{
  GtkWindow *window;
  GtkWidget *interface;
  GtkWidget *layout;

  const char *root;
  int flag;

  gint width, height;
  gint yres;

  Program = basename(argv[0]);	/* strip leading path for Progname */
  Release = "0.9.1";

  /* Change the process name using Program variable. */
  strncpy(argv[0], Program, strlen(argv[0]));
  setprogname (Program = argv[0]);
  opterr = 0;			/* disable invalid option messages */

  while ((flag = getopt (argc, argv, "d:hv")) != -1)
    switch (flag) {
      case 'v':
        printf("%s version %s %s\n", Program, Release, License);
        exit(0);

      case 'h':
        printf(Usage, License, Program);
        exit(0);

      case 'd':
        debug = atoi(optarg);
        break;

      default:
        if (optopt == 'd')
          debug = 1;
        else {
          printf("%s: invalid option, use -h for help usage.\n", Program);
          exit(1);
        }
    }

  root = argv[optind] ? argv[optind] : "/";

#ifdef GETTEXT_PACKAGE
  bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);
  gtk_disable_setlocale();
#endif

  gtk_init (&argc, &argv);      /* initialization of the GTK */
  gtk_set_locale ();

  /* Adjust width and height according to screen resolution. */
  yres = gdk_screen_height ();
  height = 3 * yres / 5;
  width  = 2 * height;

  /* Allocate memory for the global_ singleton. */
  global_ = g_new0 (GlobalData, 1);

  global_->cursor[BUSY_CURSOR]   = gdk_cursor_new (GDK_WATCH);
  global_->cursor[NORMAL_CURSOR] = gdk_cursor_new (GDK_TOP_LEFT_ARROW);

  /* Construct the user interface. */
  interface = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_widget_set_size_request (interface, width, height);
  window = GTK_WINDOW (interface);
  gtk_widget_show (interface);

  gtk_window_set_keep_above (window, TRUE);
  gtk_window_set_position (window, GTK_WIN_POS_CENTER);
  gtk_window_set_icon (window, xpm_pixbuf(ICON_PACKAGE, NULL));
  gtk_window_set_title (window, _("GOULD Package Manager"));

  /* Set event handlers for window close/destroy. */
  g_signal_connect (G_OBJECT(interface), "destroy", G_CALLBACK(finis), NULL);
  g_signal_connect (G_OBJECT(interface), "delete-event",G_CALLBACK(finis),NULL);

  layout = constructor (interface, root);  /* realize the visual components */
  gtk_container_add (GTK_CONTAINER(interface), layout);
  gtk_widget_show (layout);

  gtk_main ();				   /* main event loop */

  return 0;
} /* </main> */
