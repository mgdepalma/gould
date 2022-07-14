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

#define XLIB_ILLEGAL_ACCESS
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gdk/gdkkeysyms.h>
#include <X11/Xlib.h>

#include <string.h>
#include <poll.h>
#include <grp.h>
#include <unistd.h>
#include <ctype.h>
#include <signal.h>
#include <sys/wait.h>

#include "gould.h"
#include "util.h"

#define MAX_INPUT_CHARS     32
#define MAX_VISIBLE_CHARS   14

static Display *dpy;
static GdkRectangle rc;
static GdkWindow *root, *win;
static GdkColor bg, border, hint, text, msg;
static PangoLayout *layout;

static char user[MAX_INPUT_CHARS];
static char pass[MAX_INPUT_CHARS];
static int stage;                   /* 0 => user, 1 => pass, 2 => authenticate */

static GSList *sessions;
static int session_select = -1;

static char *message;

static pid_t greeter = -1;
static guint greeter_watch = 0;
static int greeter_pipe[2];
static GIOChannel *greeter_io;
static guint io_id;

static void
draw_text(cairo_t *cr, double x, double y, char *text, GdkColor *color)
{
  pango_layout_set_text(layout, text, -1);
  cairo_move_to(cr, x, y);
  gdk_cairo_set_source_color(cr, color);
  pango_cairo_show_layout(cr, layout);
}

static void
get_text_layout(char *s, int *w, int *h)
{
  pango_layout_set_text(layout, s, -1);
  pango_layout_get_pixel_size(layout, w, h);
}

/*
 * xsessions_scan
*/
GSList *
xsessions_scan(void)
{
  GSList *xsessions = NULL;
  GDir *d = g_dir_open(XSESSIONS_DIR, 0, NULL);
  char *file_path, *name, *exec;
  const char *basename;
  gboolean loaded;
  GKeyFile *f;

  if (d == NULL)
    return NULL;

  f = g_key_file_new();
  while ((basename = g_dir_read_name(d) ) != NULL) {
    if (!g_str_has_suffix(basename, ".desktop"))
      continue;

    file_path = g_build_filename(XSESSIONS_DIR, basename, NULL);
    loaded = g_key_file_load_from_file(f, file_path, G_KEY_FILE_NONE, NULL);
    g_free(file_path);

    if (loaded) {
      name = g_key_file_get_locale_string(f, "Desktop Entry", "Name", NULL, NULL);

      if (name) {
        exec = g_key_file_get_string(f, "Desktop Entry", "Exec", NULL);

        if (exec) {
          Session* sess = g_new( Session, 1 );
          sess->name = name;
          sess->exec = exec;
          sess->desktop = g_strdup(basename);
          xsessions = g_slist_append(xsessions, sess);
          continue; /* load next file */
          g_free(exec);
        }
        g_free(name);
      }
    }
  }

  g_dir_close(d);
  g_key_file_free(f);

  return xsessions;
} /* xsessions_scan */

void
xsessions_free(GSList *l)
{
  GSList *p;
  Session *sess;

  for (p = l; p; p = p->next) {
    sess = p->data;
    g_free(sess->name);
    g_free(sess->exec);
    g_free(sess);
  }

  g_slist_free(l);
}

static void
on_interface_expose(void)
{
  cairo_t *cr = gdk_cairo_create(win);
  char *p = (stage == 0) ? user : pass;
  int len = strlen(p);
  GdkColor *color=&text;

  gdk_cairo_set_source_color(cr, &bg);
  cairo_rectangle(cr, 0, 0, rc.width, rc.height);
  cairo_fill(cr);
  gdk_cairo_set_source_color(cr, &border);
  cairo_set_line_width(cr, 1.0);
  cairo_stroke(cr);
  cairo_rectangle(cr, 0, 0, rc.width, rc.height);

  if (message) {
    color = &msg;
    p = message;
  }
  else if (stage == 0) {
    if( len < MAX_VISIBLE_CHARS )
      p = user;
    else
      p = user + len - MAX_VISIBLE_CHARS;

    if (len == 0) {
      p = "Username";
      color = &hint;
    }
  }
  else if (stage == 1) {
    char spy[MAX_VISIBLE_CHARS + 1];
    p = spy;

    if(len < MAX_VISIBLE_CHARS) {
      memset(spy, '*', len);
      p[len] = 0;
    }
    else {
      memset(spy, '*', MAX_VISIBLE_CHARS);
      p[MAX_VISIBLE_CHARS] = 0;
    }
    if(len == 0) {
      p = "Password";
      color = &hint;
    }
  }

  draw_text(cr, 3, 3, p, color);
  cairo_destroy(cr);
} /* on_interface_expose */

static void
on_interface_key(GdkEventKey *event)
{
  char *p;
  int len;
  int key;

  if (stage != 0 && stage != 1)
    return;

  key = event->keyval;
  p = (stage == 0) ? user : pass;
  len = strlen(p);
  message = 0;

  if (key == GDK_Escape) {
    session_select = -1;
    user[0] = 0;
    pass[0] = 0;
    stage = 0;
  }
  else if (key == GDK_BackSpace) {
    if (len > 0)
      p[--len] = 0;
  }
  else if (key == GDK_Return) {
    if (stage == 0 && len == 0) return;
    stage++;

    if (stage == 1)
      if (!strcmp(user, "reboot") || !strcmp(user, "shutdown") || !strcmp(user, "exit"))
        stage = 2;
  }
  else if (key == GDK_F1) {
    Session *sess;

    if (!sessions) {
      sessions = xsessions_scan();
      session_select = 0;
    }
    else {
      session_select++;
      if (session_select >= g_slist_length(sessions))
        session_select = 0;
    }

    sess = g_slist_nth_data(sessions, session_select);
    if (sess) message = sess->name;
  }
  else if (key >= 0x20 && key <= 0x7e)
    if (len < MAX_INPUT_CHARS - 1) {
      p[len++] = key;
      p[len] = 0;
    }

  if (stage == 2)
    interface_do_login();
  else
    on_interface_expose();
} /* on_interface_key */

static void
greeter_setup(gpointer user)
{
}

static gchar *
greeter_param(char *str, char *name)
{
  char *match, *scan;
  char ret[128];
  int  idx;

  match = g_strdup_printf(" %s=", name);
  scan = strstr(str, match);

  if (!scan) {
    g_free(match);
    return NULL;
  }

  scan += strlen(match);
  g_free(match);

  for (idx = 0; idx < 127; idx++) {
    if(!scan[idx] || isspace(scan[idx])) break;
    ret[idx] = scan[idx];
  }
  ret[idx] = 0;

  return g_strdup(ret);
} /* greeter_param */

static gboolean
on_greeter_input(GIOChannel *source, GIOCondition condition, gpointer data)
{
  GIOStatus ret;
  char *str;

  if (!(G_IO_IN & condition))
    return FALSE;

  ret = g_io_channel_read_line(source, &str, NULL, NULL, NULL);
  if (ret != G_IO_STATUS_NORMAL)
    return FALSE;

  if (!strncmp(str, "suspend", 7))
    gould_do_suspend();
  else if (!strncmp(str, "shutdown", 8))
    gould_do_shutdown();
  else if (!strncmp(str, "reboot", 6))
    gould_do_reboot();
  else if (!strncmp(str, "exit", 4))
    gould_do_exit();
  else if (!strncmp(str, "login ", 6)) {
    char *user = greeter_param(str, "user");
    char *pass = greeter_param(str, "pass");
    char *session = greeter_param(str, "session");
    char *lang = greeter_param(str, "lang");

    if (user && pass) {
      struct passwd *pw;
      int ret = gould_auth_user(user, pass, &pw);

      if (AUTH_SUCCESS == ret && pw != NULL) {
        interface_drop();
        gould_do_login(pw, session, lang);

        if (gould_cur_session() <= 0)
          interface_init();
      }
      else
        write(greeter_pipe[0], "reset\n", 6);
    }
    g_free(user);
    g_free(pass);
    g_free(session);
    g_free(lang);
  }
  else if (!strncmp(str, "log ", 4)) {
    log_print(str + 4);
  }
  g_free(str);

  return TRUE;
} /* on_greeter_input */

static void
on_greeter_exit(GPid pid, gint status, gpointer data)
{
  if (pid != greeter)
    return;

  greeter = -1;
  g_source_remove(greeter_watch);
}

/*
 * interface_add_cursor
 * interface_setbg
 */
void
interface_add_cursor(void)
{
  GdkCursor *cur;
  if(!root) return;
  cur = gdk_cursor_new(GDK_LEFT_PTR);
  gdk_window_set_cursor(root, cur);
  gdk_cursor_unref(cur);
}

GError *
interface_setbg(GdkWindow *window)
{
  GError *error = NULL;
  char   *bg    = g_key_file_get_string(config, "display", "bg", 0);

  if (bg) {            /* act on background color or file name */
    if (bg[0] == '#') {
      GdkColor bgcolor;
      GdkColormap *map = gdk_window_get_colormap (window);

      gdk_color_parse (bg, &bgcolor);
      gdk_color_alloc (map, &bgcolor);
      gdk_window_set_background (window, &bgcolor);
      gdk_window_clear (window);  /* clear window to effect change */
    }
    else {
      GdkPixbuf *bgimg = gdk_pixbuf_new_from_file (bg, &error);

      if (bgimg) {
        GdkPixmap *pix = NULL;
        char *style = g_key_file_get_string(config, "display", "bg_style", 0);

        if (!style || !strcmp(style, "stretch")) {
          // FIXME! get the window dimensions not the screen display
          GdkPixbuf *pb = gdk_pixbuf_scale_simple(bgimg,
                                                  gdk_screen_width(),
                                                  gdk_screen_height(),
                                                  GDK_INTERP_HYPER);
          g_object_unref (bgimg);
          bgimg = pb;
        }

        gdk_pixbuf_render_pixmap_and_mask(bgimg, &pix, NULL, 0);
        g_object_unref (bgimg);

        /* call x directly, because gdk will ref the pixmap */
        XSetWindowBackgroundPixmap(GDK_WINDOW_XDISPLAY(window),
                                   GDK_WINDOW_XID(window), GDK_PIXMAP_XID(pix));
        g_object_unref (pix);
      }
      else {
        log_print("%s\n", error->message);
      }
    }
  }

  return error;
} /* interface_setbg */

void
interface_event_cb(GdkEvent *event, gpointer data)
{
  if (stage == 2)
    return;

  if (event->type == GDK_KEY_PRESS)
    on_interface_key((GdkEventKey*)event);
  else if( event->type == GDK_EXPOSE )
    on_interface_expose();
}

/*
 * public methods
 */
void
interface_clean(void)
{
  if (greeter > 0) {
    g_source_remove(greeter_watch);
    stop_pid(greeter);
    greeter = -1;
  }
}

void
interface_drop(void)
{
  /* drop connect event */
  if (dpy)
    if (win) {
      gdk_window_destroy(win);
      win = NULL;
      XUngrabKeyboard(dpy, CurrentTime);
    }

  /* destroy the font */
  /* drop connect event */
  if (layout) {
    g_object_unref(layout);
    layout = NULL;
  }

  /* if greeter, do quit */
  if (greeter > 0) {
    write(greeter_pipe[0], "exit\n", 5);
    g_source_remove(io_id);
    io_id = 0;
    g_io_channel_unref(greeter_io);
    greeter_io = NULL;
    close(greeter_pipe[1]);
    close(greeter_pipe[0]);

    g_source_remove(greeter_watch);
    waitpid(greeter, 0, 0) ;
    greeter = -1;
  }
} /* interface_drop */

void
interface_init(void)
{
  PangoFontDescription *desc;
  cairo_t *cr;
  char *p;
  int w, h;

  if (gould_cur_session() > 0)   /* see if session is running */
    return;

  /* get current display */
  dpy  = gdk_x11_get_default_xdisplay();
  root = gdk_get_default_root_window();

  XSetInputFocus(dpy,GDK_WINDOW_XWINDOW(root), RevertToNone, CurrentTime);
  p = g_key_file_get_string(config, "core", "greeter", NULL);

  if (p && p[0]) {         /* run greeter with pipes */
    char **argv;
    gboolean ret;
        
    if (greeter > 0 && kill(greeter, 0) == 0)       /* already running */
      return;

    g_shell_parse_argv(p, NULL, &argv, NULL);
    ret = g_spawn_async_with_pipes(NULL, argv, NULL,
                                   G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD, greeter_setup, 0,
                                   &greeter, greeter_pipe + 0, greeter_pipe + 1,
                                   NULL, NULL);
    g_strfreev(argv);

    if (ret == TRUE) {
      g_free(p);
      greeter_io = g_io_channel_unix_new(greeter_pipe[1]);
      io_id = g_io_add_watch(greeter_io, G_IO_IN | G_IO_HUP | G_IO_ERR,
                                                on_greeter_input, NULL);
      greeter_watch = g_child_watch_add(greeter, on_greeter_exit, 0);
      return;
    }
  }
  g_free(p);

  if (sessions)            /* initialize  */
    xsessions_free(sessions);

  sessions = 0;
  session_select = 0;
  user[0] = pass[0] = 0;
  stage = 0;

  p = g_key_file_get_string(config, "input", "border", 0);
  if(!p) p = g_strdup("#CBCAE6");
  gdk_color_parse(p, &border);
  g_free(p);

  p = g_key_file_get_string(config, "input", "bg", 0);
  if(!p) p = g_strdup("#ffffff");
  gdk_color_parse(p, &bg);
  g_free(p);

  p = g_key_file_get_string(config, "input", "hint", 0);
  if(!p) p = g_strdup("#CBCAE6");
  gdk_color_parse(p, &hint);
  g_free(p);

  p = g_key_file_get_string(config, "input", "text", 0);
  if(!p) p = g_strdup("#000000");
  gdk_color_parse(p, &text);
  g_free(p);

  p = g_key_file_get_string(config, "input", "msg", 0);
  if(!p) p = g_strdup("#ff0000");
  gdk_color_parse(p, &msg);
  g_free(p);

  if (!win) {              /* create the window */
    GdkWindowAttr attr;
    int mask = 0;
    memset( &attr, 0, sizeof(attr) );
    attr.window_type = GDK_WINDOW_CHILD;
    attr.event_mask = GDK_EXPOSURE_MASK | GDK_KEY_PRESS_MASK;
    attr.wclass = GDK_INPUT_OUTPUT;
    win = gdk_window_new(0, &attr, mask);
  }

  if (layout) {             /* create the font */
    g_object_unref(layout);
    layout = NULL;
  }
  cr = gdk_cairo_create(win);
  layout = pango_cairo_create_layout(cr);
  cairo_destroy(cr);

  p = g_key_file_get_string(config, "input", "font", 0);
  if(!p) p = g_strdup("Sans 14");

  desc = pango_font_description_from_string(p);
  pango_layout_set_font_description(layout, desc);
  pango_font_description_free(desc);
  g_free(p);

  if (layout) {             /* set window size */
    char temp[MAX_VISIBLE_CHARS + 1 + 1];
    memset(temp, 'A', sizeof(temp));
    temp[sizeof(temp) - 1] = 0;
    get_text_layout(temp, &w, &h);
    rc.width = w + 6; rc.height = h + 6;
    rc.x = (gdk_screen_width() - rc.width) / 2;
    rc.y = (gdk_screen_height() - rc.height) / 2;
    gdk_window_move_resize(win, rc.x, rc.y, rc.width, rc.height);
  }

  /* set main window background as specified by SYSCONFIG/gould.conf */
  interface_setbg(root); /* FIXME! not working */
  /* interface_setbg(win);  FIXME! that's not solution. */

  /* connect event */
  gdk_window_set_events(win, GDK_EXPOSURE_MASK | GDK_KEY_PRESS_MASK);
  XGrabKeyboard(dpy, GDK_WINDOW_XWINDOW(win), False,
                               GrabModeAsync, GrabModeAsync, CurrentTime);

  /* draw the first time */
  gdk_window_show(win);
  XSetInputFocus(dpy, GDK_WINDOW_XWINDOW(win), RevertToNone, CurrentTime);
} /* interface_init */

int
interface_do_login(void)
{
  int ret = -1;
  struct passwd *pw;

  if (stage != 2)
    return -1;

  if (!strcmp(user, "reboot"))
    gould_do_reboot();
  else if (!strcmp(user, "shutdown"))
    gould_do_shutdown();
  else if (!strcmp(user, "exit"))
    gould_do_exit();
  else {
    ret = gould_auth_user(user, pass, &pw);

    if (AUTH_SUCCESS == ret && pw != NULL) {
      char *exec = 0;

      if (sessions && session_select > 0) {
        Session *sess;
        sess = g_slist_nth_data(sessions, session_select);
        exec = g_strdup(sess->exec);
        xsessions_free(sessions);
      }

      sessions = 0;
      session_select = -1;

      interface_drop();
      gould_do_login(pw, exec, 0);
      g_free(exec);

      if (gould_cur_session() <= 0)
        interface_init();
    }
    else {
      user[0] = pass[0] = 0;
      stage = 0;
    }
  }

  return 0;
} /* interface_do_login */

int
interface_main(void)
{
  GMainLoop *loop = g_main_loop_new(NULL, 0);

  interface_init();
  interface_add_cursor();

  if (greeter == -1)     /* if greeter is not used */
    gdk_event_handler_set(interface_event_cb, 0, 0);

  g_main_loop_run(loop);

  return 0;
} /* interface_main */

