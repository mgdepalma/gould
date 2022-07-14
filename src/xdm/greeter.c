/**
 * Copyright (C) Generations Linux <bugs@softcraft.org>
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
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

#include <pwd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <glib/gi18n.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>

#include "lang.h"
#include "util.h"


enum {
  COL_SESSION_NAME,
  COL_SESSION_EXEC,
  COL_SESSION_DESKTOP_FILE,
  N_SESSION_COLS
};

enum {
  COL_LANG_NAME,
  COL_LANG_DESC,
  N_LANG_COLS
};

typedef struct {
  char *name;
  char *pass;
} UserInfo;

/*
 * private attributes
 */
static GKeyFile *cache;
static GKeyFile *config;

static GIOChannel *greeter_io;

static GtkWidget *greeter;
static GtkWidget *header;

static GtkWidget *login_prompt;
static GtkWidget *login_entry;

static GtkWidget *lang;
static GtkWidget *lang_menu;
static GtkWidget *lang_name;

static GtkWidget *session;
static GtkWidget *session_menu;
static GtkWidget *session_name;

static GtkWidget *suspend;
static GtkWidget *shutdown;    /* shutdown buttons */
static GtkWidget *reboot;

static GtkWidget *quit;
static GtkWidget *quit_menu;

static char *session_desktop;
static char *session_lang;

static char *user = NULL;
static char *pass = NULL;

static const unsigned short ScaleMinimum = 50;
static float scale;          /* scale and center ui */

/*
 * private methods
 */
static void
_label_set_text(GtkWidget *label, const char *fmt, ...)
{
  char *text;
  va_list ap;

  va_start(ap, fmt);
  text = g_strdup_vprintf(fmt, ap);
  va_end(ap);

  gtk_label_set_text(GTK_LABEL(label), text);
  g_free(text);
} /* _label_set_text */

/*
 * on_timeout - update clock
 */
static gboolean
on_timeout(GtkLabel *label)
{
  char buf[MAX_PATHNAME];
  struct tm *tmbuf;
  time_t t;

  time(&t);
  tmbuf = localtime(&t);
  strftime(buf, MAX_PATHNAME, "%c", tmbuf);
  gtk_label_set_text(label, buf);

  return TRUE;
} /* on_timeout */

/*
 * on_entry_activate - center stage logic
 */
static void
on_entry_activate(GtkEntry *entry, gpointer data)
{
  const char *name;

  if (user) {
    GtkTreeIter it;
    pass = g_strdup(gtk_entry_get_text(entry));

    if (strchr(pass, ' ')) {
      g_free(user); user = NULL;
      g_free(pass); pass = NULL;
      gtk_label_set_text(GTK_LABEL(login_prompt), _("Username:"));
      gtk_entry_set_visibility(GTK_ENTRY(entry), TRUE);
      gtk_entry_set_text(GTK_ENTRY(entry), "");
      return;
    }

    /* write to the greeter stdout pipe */
    printf("login user=%s pass=%s session=%s lang=%s\n",
               user, pass, session_desktop, session_lang);

    /* password check failed */
    g_free(user); user = NULL;
    g_free(pass); pass = NULL;

    gtk_widget_hide(login_prompt);
    gtk_widget_hide(GTK_WIDGET(entry));

    gtk_label_set_text(GTK_LABEL(login_prompt), _("Username:"));
    gtk_entry_set_visibility(GTK_ENTRY(entry), TRUE);
    gtk_entry_set_text(GTK_ENTRY(entry), "");
  }
  else {
    gtk_label_set_text(GTK_LABEL(login_prompt), _("Password:"));
    user = g_strdup(gtk_entry_get_text(GTK_ENTRY(entry)));
    gtk_entry_set_text(GTK_ENTRY(entry), "");

    if (strchr(user, ' ')) {
      g_free(user);
      user = NULL;
      return;
    }
    gtk_entry_set_visibility(entry, FALSE);
  }
} /* on_entry_activate */

/*
 * on_user_activate
 */
on_user_activate(GtkWidget *button, UserInfo *info)
{
  /* FIXME
  gtk_label_set_text(GTK_LABEL(login_prompt), _("Username:"));
  gtk_entry_set_text(GTK_ENTRY(login_entry), info->name);
  gtk_entry_set_visibility(GTK_ENTRY(login_entry), TRUE);
  */

  if (info->pass) /* write to the greeter stdout pipe */
    printf("login user=%s pass=%s session=%s lang=%s\n",
               info->name, info->pass, session_desktop, session_lang);
  else {
    GtkEntry *entry = GTK_ENTRY(login_entry);
    gtk_entry_set_text(entry, info->name);
    on_entry_activate(entry, info);
  }
}

static void
on_screen_size_changed(GdkScreen *scr, GtkWindow *win)
{
  gint width  = gdk_screen_get_width(scr);
  gint height = gdk_screen_get_height(scr);

  gtk_window_resize(win, scale*width, scale*height);
  gtk_window_move(win, width*(1-scale)/2, height*(1-scale)/2);
}

/*
 * language pulldown menu
 */
static gint
lang_cmpr(GtkTreeModel *list, GtkTreeIter *a,GtkTreeIter *b,gpointer data)
{
  gint  ret;
  gchar *as, *bs;

  gtk_tree_model_get(list, a, 1, &as, -1);
  gtk_tree_model_get(list, b, 1, &bs, -1);
  ret = strcmp(as, bs);
  g_free(as);
  g_free(bs);

  return ret;
}

static void
do_lang_default(void)
{
  const char *name = session_lang_get("/etc/sysconfig/i18n");
  _label_set_text(lang_name, "[%s]", name);
  session_lang = (char *)name;
}

static void
do_lang_item(GtkWidget *item, GtkListStore *list)
{
  GtkTreeIter it;
  const gchar *desc = gtk_menu_item_get_label(GTK_MENU_ITEM(item));

  if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(list), &it))
    while ( 1 ) {
      gchar *lang;
      gtk_tree_model_get(GTK_TREE_MODEL(list), &it, COL_LANG_DESC, &lang, -1);

      if (strcmp(desc, lang) == 0) {
        _label_set_text(lang_name, "[%s]", desc);
        g_free(lang);

        gtk_tree_model_get(GTK_TREE_MODEL(list), &it, COL_LANG_NAME, &lang, -1);
        session_lang = (char *)lang;
        break;
      }

      g_free(lang);
      if (!gtk_tree_model_iter_next(GTK_TREE_MODEL(list), &it))
        break;
    }
} /* do_lang_item */

static void
on_lang_clicked(GtkButton *button, gpointer data)
{
  gtk_menu_popup(GTK_MENU(lang_menu), NULL, NULL, NULL, NULL,
                         0, gtk_get_current_event_time());
} /* on_lang_clicked */

static void
load_lang_cb(void *arg, char *lang, char *desc)
{
  GtkListStore *list = (GtkListStore *)arg;
  GtkTreeIter it;

  gchar *lingua;
  gchar *ptr;

  lingua = g_strdup(lang);
  ptr = strchr(lingua, '.');
  if(ptr) *ptr = (gchar)0;

  gtk_list_store_append(list, &it);
  gtk_list_store_set(list, &it,
                     COL_LANG_NAME, lingua,
                     COL_LANG_DESC, desc ? desc : "",
                     -1);
  g_free(lingua);
} /* load_lang_cb */

static void
load_lang_menu()
{
  GtkWidget    *item;
  GtkListStore *list;
  GtkTreeIter  it;

  list = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_STRING);
  gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(list), 0, GTK_SORT_ASCENDING);
  gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(list), 0, lang_cmpr, NULL, NULL);
  gould_load_langs(config, TRUE, list, load_lang_cb);
  lang_menu = gtk_menu_new();

  item = gtk_image_menu_item_new_with_mnemonic( _("Default") );
  g_signal_connect(item, "activate", G_CALLBACK(do_lang_default), NULL);
  gtk_menu_shell_append(GTK_MENU_SHELL(lang_menu), item);

  if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(list), &it))
    while ( 1 ) {
      gchar *lang;
      gtk_tree_model_get(GTK_TREE_MODEL(list), &it, COL_LANG_NAME, &lang, -1);

      if (strlen(lang) > 0) {
        g_free(lang);
        gtk_tree_model_get(GTK_TREE_MODEL(list), &it, COL_LANG_DESC, &lang, -1);

        item = gtk_image_menu_item_new_with_label(lang);
        g_signal_connect(item, "activate", G_CALLBACK(do_lang_item), list);
        gtk_menu_shell_append(GTK_MENU_SHELL(lang_menu), item);
      }

      g_free(lang);
      if (!gtk_tree_model_iter_next(GTK_TREE_MODEL(list), &it))
        break;
    }

  g_signal_connect(lang, "clicked", G_CALLBACK(on_lang_clicked), NULL);
  gtk_widget_show_all(lang_menu);
} /* load_lang_menu */

/*
 * session pulldown menu
 */
static void
do_session_default(void)
{
  const char *name = session_desktop_get("/etc/sysconfig/desktop");
  _label_set_text(session_name, "[%s]", name);
  session_desktop = (char *)name;
}
static void
do_session_failsafe(void)
{
  _label_set_text(session_name, "[%s]", _("failsafe"));
  session_desktop = "failsafe";
}

static void
on_session_clicked(GtkButton *button, gpointer data)
{
  gtk_menu_popup(GTK_MENU(session_menu), NULL, NULL, NULL, NULL,
                         0, gtk_get_current_event_time());
} /* on_session_clicked */

static void
load_session_menu()
{
  GtkWidget *item;
  session_menu = gtk_menu_new();

  item = gtk_image_menu_item_new_with_mnemonic(_("Default"));
  g_signal_connect(item, "activate", G_CALLBACK(do_session_default), NULL);
  gtk_menu_shell_append(GTK_MENU_SHELL(session_menu), item);

  item = gtk_image_menu_item_new_with_mnemonic(_("failsafe"));
  g_signal_connect(item, "activate", G_CALLBACK(do_session_failsafe), NULL);
  gtk_menu_shell_append(GTK_MENU_SHELL(session_menu), item);

  g_signal_connect(session, "clicked", G_CALLBACK(on_session_clicked), NULL);
  gtk_widget_show_all(session_menu);
} /* load_session_menu */

/*
 * shutdown buttons
 */
static void do_suspend(void)
{
  printf("suspend\n");
}
static void do_shutdown(void)
{
  printf("shutdown\n");
}
static void do_reboot(void)
{
  printf("reboot\n");
}

/*
 * quit button
 */
static void
do_exit(void)
{
  gboolean child = FALSE;
  char *path = g_strdup_printf("/proc/%d/cmdline", getppid());
  FILE *fd = fopen(path, "r");
  g_free(path);

  if (fd) {
    char line[MAX_PATHNAME];
    fgets(line, MAX_PATHNAME, fd);
    extern char *basename(const char *name);
    if (strcmp(basename(line), PACKAGE_NAME) == 0) child = TRUE;
    fclose(fd);
  }

  if (child)       /* child of PACKAGE_NAME - communicating via pipes */
    printf("exit\n");
  else
    exit(0);
} /* do_exit */

static void
on_quit_clicked(GtkButton *button, gpointer data)
{
  gtk_menu_popup(GTK_MENU(quit_menu), NULL, NULL, NULL, NULL,
                         0, gtk_get_current_event_time());
} /* on_quit_clicked */

static void
load_quit_menu()
{
  GtkWidget *item;
  quit_menu = gtk_menu_new();

  item = gtk_image_menu_item_new_with_mnemonic(_("_Shutdown"));
  g_signal_connect(item, "activate", G_CALLBACK(do_shutdown), NULL);
  gtk_menu_shell_append(GTK_MENU_SHELL(quit_menu), item);

  item = gtk_image_menu_item_new_with_mnemonic(_("_Reboot"));
  g_signal_connect(item, "activate", G_CALLBACK(do_reboot), NULL);
  gtk_menu_shell_append(GTK_MENU_SHELL(quit_menu), item);

  item = gtk_image_menu_item_new_with_mnemonic(_("_Exit"));
  g_signal_connect(item, "activate", G_CALLBACK(do_exit), NULL);
  gtk_menu_shell_append(GTK_MENU_SHELL(quit_menu), item);

  g_signal_connect(quit, "clicked", G_CALLBACK(on_quit_clicked), NULL);
  gtk_widget_show_all(quit_menu);
} /* load_quit_menu */

/*
 * build_users_ui
 */
void
build_users_ui(GtkBuilder *builder, GtkFrame *frame,
               const char *theme, const char* users)
{
  GtkWidget *layout;
  char *themedir = g_build_filename(GOULD_DATA_DIR "/themes", theme, NULL);
  char **list = g_strsplit(users, ",", -1);
  char *name, *pass;
  int idx;

  gtk_frame_set_shadow_type (GTK_FRAME(frame), GTK_SHADOW_ETCHED_OUT);

  /* Create the inside window layout. */
  layout = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER(frame), layout);
  gtk_widget_show (layout);

  for (idx=0; list[idx]; idx++) {
    struct passwd *pw;

    name = (list[idx][0] == '"') ? &list[idx][1] : list[idx];
    if(name[strlen(name) - 1] == '"') name[strlen(name) - 1] = (char)0;
    if(pass = strstr(name, "/")) *pass++ = (char)0;

    if (pw = getpwnam(name)) {
      GdkPixbuf *pixbuf = NULL;
      GtkWidget *item, *image = NULL;
      UserInfo *info = g_new0 (UserInfo, 1);
      char *icon;

      item = gtk_button_new_with_label (pw->pw_gecos);
      gtk_button_set_relief (GTK_BUTTON(item), GTK_RELIEF_NONE);
      gtk_box_pack_start (GTK_BOX(layout), item, FALSE, FALSE, 0);
      gtk_widget_show (item);

      info->name = g_strdup(name);
      info->pass = (pass) ? g_strdup(pass) : NULL;

      g_signal_connect (G_OBJECT(item), "clicked",
                        G_CALLBACK(on_user_activate), info);

      /* use the appropriate icon image depending on uid */
      icon = g_strdup_printf("%s/.face.icon", pw->pw_dir);

      if (!g_file_test (icon, G_FILE_TEST_EXISTS)) {
        if (pw->pw_uid == 0)
          icon = g_build_filename(themedir, "admin.png", NULL);
        else
          icon = g_build_filename(themedir, "user.png", NULL);
      }

      if (g_file_test(icon, G_FILE_TEST_EXISTS)) {
        GError *error  = NULL;
        pixbuf = gdk_pixbuf_new_from_file (icon, &error);

        if (pixbuf) {
          GdkPixbuf *render = gdk_pixbuf_scale_simple (pixbuf, 20, 20,
                                                       GDK_INTERP_BILINEAR);
          image = gtk_image_new_from_pixbuf (render);
          gtk_button_set_image (GTK_BUTTON(item), image);
          g_object_unref (pixbuf);
          g_object_unref (render);
        }
      }
      g_free(icon);
    }
  }

  g_strfreev(list);
  g_free(themedir);
} /* build_users_ui */

/*
 * build_ui
 */
static void
build_ui(const char *ui_file, const char *theme)
{
  GdkScreen *scr;
  GSList *objs, *it;
  GtkBuilder *builder;
  GtkWidget *w;

  char *node, *path;
  gint height, width;      /* screen height and width */

  builder = gtk_builder_new ();
  gtk_builder_add_from_file (builder, ui_file, NULL);
  greeter = GTK_WIDGET (gtk_builder_get_object(builder, PACKAGE_NAME));

  scr = gtk_widget_get_screen(greeter);
  g_signal_connect(scr, "size-changed",
                   G_CALLBACK(on_screen_size_changed), greeter);

  /* clock widget */
  if ((w = GTK_WIDGET (gtk_builder_get_object(builder, "clock"))) != NULL) {
    guint timeout = g_timeout_add (1000, (GSourceFunc)on_timeout, w);
    g_signal_connect_swapped (w, "destroy",
                    G_CALLBACK(g_source_remove), GUINT_TO_POINTER(timeout));
    on_timeout((GtkLabel *)w);
  }

  /* pre-defined users */
  if (w = GTK_WIDGET (gtk_builder_get_object(builder, "users")))
    if (node = g_key_file_get_string (config, "display", "users", 0)) {
      build_users_ui(builder, (GtkFrame *)w, theme, node);
      g_free(node);
    }
    else
      gtk_widget_hide(w);

  /* See if the header has %h => hostname */
  header = GTK_WIDGET (gtk_builder_get_object(builder, "header"));
  path = (char *)gtk_label_get_text(GTK_LABEL(header));
  node = strstr(path, "%h");

  if (node) {
    node[1] = 's';
    node = sysname();
    path = g_strdup_printf(_(path), sysname());
    gtk_label_set_text(GTK_LABEL(header), path);
    g_free(path);
    g_free(node);
  }

  /* user authentication login box */
  login_entry  = GTK_WIDGET (gtk_builder_get_object(builder, "login_entry"));
  login_prompt = GTK_WIDGET (gtk_builder_get_object(builder, "login_prompt"));
  g_signal_connect(login_entry, "activate", G_CALLBACK(on_entry_activate),NULL);

  if (node = g_key_file_get_string(cache, "display", "username", 0)) {
    gtk_entry_set_text (GTK_ENTRY(login_entry), node);
    gtk_editable_set_position (GTK_EDITABLE(login_entry), strlen(node) + 1);
    g_free(node);
  }

  /* language selection pulldown */
  lang = GTK_WIDGET (gtk_builder_get_object(builder, "lang"));
  load_lang_menu();

  lang_name = GTK_WIDGET (gtk_builder_get_object(builder, "lang_name"));
  _label_set_text(lang_name, "[%s]", _(PREVIOUS));
  session_lang = PREVIOUS;

  /* session selection pulldown */
  session = GTK_WIDGET (gtk_builder_get_object(builder, "session"));
  load_session_menu();

  session_name = GTK_WIDGET (gtk_builder_get_object(builder, "session_name"));
  _label_set_text(session_name, "[%s]", _(PREVIOUS));
  session_desktop = PREVIOUS;

  /* shutdown buttons */
  if (suspend = GTK_WIDGET (gtk_builder_get_object(builder, "suspend"))) {
    gboolean resume = FALSE;
    FILE *fd = fopen("/proc/cmdline", "r");

    if (fd) {
      char line[MAX_PATHNAME];
      fgets(line, MAX_PATHNAME, fd);
      resume = strstr(line, "resume=") != NULL;
      if (resume == FALSE) resume = strstr(line, "restore=") != NULL;
      fclose(fd);
    }

    if (resume)
      g_signal_connect (G_OBJECT(suspend), "clicked",
                        G_CALLBACK(do_suspend), NULL);
    else
      gtk_widget_set_sensitive(suspend, FALSE);
  }

  if (shutdown = GTK_WIDGET (gtk_builder_get_object(builder, "shutdown"))) {
    g_signal_connect (G_OBJECT(shutdown), "clicked",
                      G_CALLBACK(do_shutdown), NULL);
  }

  if (reboot = GTK_WIDGET (gtk_builder_get_object(builder, "reboot"))) {
    g_signal_connect (G_OBJECT(reboot), "clicked",
                      G_CALLBACK(do_reboot), NULL);
  }

  /* <Quit> button - conditional on quit=1 in configuration */
  if (quit = GTK_WIDGET (gtk_builder_get_object(builder, "quit"))) {
    if (g_key_file_get_integer(config, "display", "quit", 0)) {
      if (shutdown)            /* only using buttons */
        g_signal_connect (G_OBJECT(quit), "clicked",
                          G_CALLBACK(do_exit), NULL);
      else
        load_quit_menu();      /* using pulldown menu */
    }
    else {
      gtk_widget_hide(quit);
    }
  }

  /* all done with the ui, release GtkBuilder object */
  g_object_unref(builder);

  /* optionally scale and center the ui */
  if (scale = g_key_file_get_integer(config, "display", "scale", 0)) {
    if (scale < ScaleMinimum || scale > 100) scale = ScaleMinimum;
    scale /= 100;
  }
  else
    scale = 1;

  g_debug("scale => %d%%", (int)((scale + 0.005) * 100));
  height = gdk_screen_get_height(scr);
  width = gdk_screen_get_width(scr);

  gtk_window_set_default_size(GTK_WINDOW(greeter), scale*width, scale*height);
  gtk_window_move(GTK_WINDOW(greeter), width*(1-scale)/2, height*(1-scale)/2);
  gtk_window_present(GTK_WINDOW(greeter));

  gtk_widget_realize(login_entry);
  gtk_widget_grab_focus(login_entry);
} /* build_ui */

/*
 * apply_theme
 */
static int
apply_theme(const char* theme)
{
  char *themedir = g_build_filename(GOULD_DATA_DIR "/themes", theme, NULL);
  char *ui_file  = g_build_filename(themedir, "greeter.ui", NULL);
  int status;

  if (g_file_test(ui_file, G_FILE_TEST_EXISTS)) {
    char *rc = g_build_filename(themedir, "gtkrc", NULL);

    if (g_file_test(rc, G_FILE_TEST_EXISTS)) {
      g_debug("%s", rc);
      gtk_rc_parse(rc);
    }
    g_free(rc);

    g_debug("theme => %s, ui => greeter.ui", theme);
    build_ui(ui_file, theme);  /* create the interface */
    status = EXIT_SUCCESS;
  }
  else {
    g_debug("%s missing.\n", ui_file);
    log_print("greeter: %s missing.\n", ui_file);
    status = EXIT_FAILURE;
  }

  g_free(ui_file);
  g_free(themedir);

  return status;
} /* apply_theme */

/*
 * listen_stdin
 */
static gboolean
on_command(GIOChannel *source, GIOCondition condition, gpointer data)
{
  GIOStatus ret;
  char *str;

  if (!(G_IO_IN & condition))
    return FALSE;

  ret = g_io_channel_read_line(source, &str, NULL, NULL, NULL);
  if(ret != G_IO_STATUS_NORMAL) return FALSE;

  if (!strncmp(str, "quit", 4) || !strncmp(str, "exit", 4))
    gtk_main_quit();
  else if (!strncmp(str, "reset", 5)) {
    gtk_widget_show(login_prompt);
    gtk_widget_show(login_entry);
    gtk_widget_grab_focus(login_entry);
  }
  g_free(str);
  return TRUE;
} /* on_command */

void
listen_stdin(void)
{
  greeter_io = g_io_channel_unix_new(0);
  g_io_add_watch(greeter_io, G_IO_IN, on_command, NULL);
}

/*
 * main program
 */
int
main(int arc, char *arg[])
{
  char* theme;
  int status;

#ifdef GETTEXT_PACKAGE
  bindtextdomain (PACKAGE_NAME, LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (PACKAGE_NAME);
  gtk_disable_setlocale();
#endif

  gtk_init (&arc, &arg);     /* initialization of the GTK */
  gtk_set_locale ();

  cache = g_key_file_new();
  g_key_file_load_from_file(cache, CONFIG_CACHE, G_KEY_FILE_KEEP_COMMENTS,NULL);

  config = g_key_file_new();
  g_key_file_load_from_file(config, CONFIG_FILE, G_KEY_FILE_KEEP_COMMENTS,NULL);

  if (theme = g_key_file_get_string(config, "display", "gtk_theme", NULL)) {
    GtkSettings* settings = gtk_settings_get_default();
    g_object_set(settings, "gtk-theme-name", theme, NULL);
    g_free(theme);
  }

  /* load gtkrc-based themes */
  if (theme = g_key_file_get_string(config, "display", "theme", NULL)) {
    status = apply_theme(theme);
    g_free(theme);
  }
  else {
    /* g_debug("Error: no theme, CONFIG_FILE => %s", CONFIG_FILE); */
    fprintf(stderr, "error: no theme specified (CONFIG_FILE => %s)\n", CONFIG_FILE);
    status = EXIT_FAILURE;
  }

  if (status == EXIT_SUCCESS) {
    listen_stdin();
    /* buffered stdout for inter-process-communcation of single-line-commands */
    setvbuf(stdout, NULL, _IOLBF, 0);
    gtk_main();               /* main event loop */
  }

  g_key_file_free(config);
  g_key_file_free(cache);

  return status;
} /*</greeter>*/
