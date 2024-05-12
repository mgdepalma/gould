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

#include "gould.h"      /* common package declarations */
#include "gpanel.h"
#include "gsession.h"
#include "screensaver.h"
#include "module.h"
#include "util.h"

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <gdk/gdkkeysyms.h>
#include <dirent.h>

#define CREATION_DELAY 30	/* should be >10 and <min(45,(LOCK_MINS*30)) */
#define COMMAND_MAX   128	/* command line max length */

//G_LOCK_DEFINE_STATIC (applet);
G_LOCK_DEFINE (applet);

extern const char *Program;	/* see, gpanel.c */
extern const char *Release;	/* ... */
extern debug_t debug;		/* ... */

/*
* Data structures used in implementation.
*/
typedef struct _ScreensaverQueue     ScreensaverQueue;
typedef struct _ScreensaverQueueItem ScreensaverQueueItem;

typedef struct _ScreensaverSettings  ScreensaverSettings;
typedef struct _ScreensaverPrivate   ScreensaverPrivate;

struct _ScreensaverQueueItem
{
  ScreensaverQueueItem *next;
  time_t creationtime;
  Window window;
};

struct _ScreensaverQueue
{
  ScreensaverQueueItem *head;
  ScreensaverQueueItem *tail;
  Display *display;
  Window root;
  guint sleep;			/* elapse time to activate screensaver/lock */
};

struct _ScreensaverSettings
{
  GlobalPanel *panel;		/* GlobalPanel instance */
  FileChooser *chooser;		/* FileChooser instance */

  gboolean active;		/* screensaver active (or not) */
  const gchar *savermode;	/* screensaver mode {off,one,random,blank} */
  const gchar *program;		/* screensaver program when mode => one */
  gint16 timer;			/* screensaver timeout in minutes */
  gint16 cycle;			/* screensaver cycle in minutes */
  gint16 lockout;		/* screensaver lock in minutes */
  gint16 selection;		/* screensaver mode selection */
  gint16 index;			/* _SCREENSAVER_MODES index */
  gint16 count;			/* max selections available */
  bool lock;			/* screensaver lock (bool) */
};

struct _ScreensaverPrivate	/* private global structure */
{
  ScreensaverSettings *config;	/* configuration settings cache struct pointer */
  const gchar *confile;		/* buffered _xscreensaver_user_config contents */
  long confile_size;		/* buffered _xscreensaver_user_config size */

  const gchar *icon;		/* maybe legacy (no longer needed) */
  GtkTooltips *tooltips;

  GtkWidget *mode_selection;	/* screensaver mode pane */
  GtkWidget *selector_pane;	/* selected screensaver pane */
  GtkWidget *settings_pane;	/* settings pane widget */
  GtkWidget *preview_name;	/* preview program name */
  GtkWidget *preview_pane;	/* preview pane widget */
  GtkWidget *fullscreen;	/* fullscreen button */

  GtkWidget *cycle_entry;	/* screensaver recycle */
  GtkWidget *timer_entry;	/* screensaver timeout */
  GtkWidget *lock_entry;	/* lock after timeout */
  GtkWidget *lock_check;	/* lock toggle button */

  pid_t monitor;		/* idle monitor process ID */
  pid_t preview;		/* screensaver preview process ID */
  pid_t screenview;		/* screensaver screenview process ID */
};

static ScreensaverPrivate local_;  /* private global structure singleton */
static char *screensaver_modes_[MAX_SCREENSAVER];

/* implementation prototypes forward declarations */
static void events_select (Display *display, Window window, Bool substructure);
static void queue_append (ScreensaverQueue *queue, Window window);
static void queue_process (ScreensaverQueue *queue, time_t age);

void screensaver_settings_apply (Modulus *applet);
void screensaver_settings_cancel (Modulus *applet);
void screensaver_settings_close (Modulus *applet);

#ifndef __GDK_DRAWING_CONTEXT_H__ // <gtk-3.0/gdk/gdkdrawingcontext.h>
void
gtk_window_close (GtkWindow* gwindow)
{
  gtk_widget_hide (gwindow);
  killproc (&local_.screenview, SIGTERM);
} /* </gtk_window_close> */
#endif

#ifdef _SCREENSAVER_DIAGNOSTICS
/*
* diagnostics - low level Xlib routines
*/
static inline int
_X_ERROR_TRAP_POP (const char *ident)
{
  return _x_error_trap_pop (ident);
  return 0;
}

static inline void
_X_ERROR_TRAP_PUSH ()
{
  gdk_error_trap_push ();
}

/*
* X error handler, safely ignore everything (mainly windows
* that die before we even get to see them).
*/
static int
events_ignore (Display *display, XErrorEvent event)
{
  return 0;
} /* </events_ignore> */

/*
* Function for processing any events that have come in since 
* last time. It is crucial that this function does not block
* in case nothing interesting happened.
*/
void
events_process (ScreensaverQueue *queue)
{
  while (XPending (queue->display)) {
    XEvent event;

    if (XCheckMaskEvent (queue->display, SubstructureNotifyMask, &event)) {
      if (event.type == CreateNotify)
        queue_append (queue, event.xcreatewindow.window);
    }
    else {
      XNextEvent (queue->display, &event);
    }

    /*
     * Reset the triggers if and only if the event is a
     * KeyPress event *and* was not generated by XSendEvent().
    */
    if (event.type == KeyPress && !event.xany.send_event) {
      queue->sleep = 0;
    }
  }

  /*
  * Check the window queue for entries that are older than
  * CREATION_DELAY seconds.
  */
  queue_process (queue, (time_t)CREATION_DELAY);
} /* </events_process> */

/*
* select all interesting events on a given tree of window(s).
*/
static void 
events_select (Display *display, Window window, Bool substructure)
{
  /* const char *ident = "xlock::events_select"; */
  const char *ident = NULL;

  XWindowAttributes xwa;
  Window root, parent, *children;
  unsigned count = 0;
  unsigned idx;

  // Start by querying the server about the root and parent windows.
  _X_ERROR_TRAP_PUSH ();

  if (!XQueryTree (display, window, &root, &parent, &children, &count)) {
    _X_ERROR_TRAP_POP (ident);
    return;
  }

  if (count)
    XFree ((char*)children);

  /*
  * Build the appropriate event mask without interfering with the normal
  * event propagation mechanism.  On the root window, we need to ask for
  * both substructureNotify and KeyPress events.  On all other windows,
  * we always need substructureNotify, but only need Keypress if some
  *
  * other client also asked for them, or if they are not being propagated
  * up the window tree.
  */
  if (substructure) {
    XSelectInput (display, window, SubstructureNotifyMask);
  }
  else {
    if (parent == None) {
      xwa.all_event_masks = 
      xwa.do_not_propagate_mask = KeyPressMask;
    }
    else if (!XGetWindowAttributes (display, window, &xwa)) {
      _X_ERROR_TRAP_POP (ident);
      return;
    }

    XSelectInput (display, window, 
                  SubstructureNotifyMask
                  | (  (  xwa.all_event_masks
                  | xwa.do_not_propagate_mask)
                  & KeyPressMask));
  }

  /*
  * Now ask for the list of children again, since it might have changed
  * in between the last time and us selecting SubstructureNotifyMask.
  *
  * There is a (very small) chance that we might process a subtree twice:
  * child windows that have been created after our XSelectinput() has
  * been processed but before we get to the XQueryTree() bit will be 
  * in this situation. This is harmless. It could be avoided by using
  * XGrabServer(), but that'd be an impolite thing to do, and since it
  * isn't required...
  */
  if (!XQueryTree (display, window, &root, &parent, &children, &count)) {
    _X_ERROR_TRAP_POP (ident);
    return;
  }

  /*
  *  Now do the same thing for all children.
  */
  for (idx = 0; idx < count; ++idx)
    events_select (display, children[idx], substructure);

  if (count)
    XFree ((char*)children);

  _X_ERROR_TRAP_POP (ident);
} /* </events_select> */

/*
* queue_append
* queue_process
*/
static void
queue_append (ScreensaverQueue *queue, Window window)
{
  ScreensaverQueueItem *item = g_new0 (ScreensaverQueueItem, 1);

  item->creationtime = time (0);
  item->window = window;
  item->next = 0;

  if (!queue->head) queue->head = item;
  if (queue->tail) queue->tail->next = item;

  queue->tail = item;
} /* </queue_append> */

static void
queue_process (ScreensaverQueue *queue, time_t age)
{
  if (queue->head) {
    time_t now = time (0);
    ScreensaverQueueItem *current = queue->head;

    while (current && current->creationtime + age < now) {
      events_select (queue->display, current->window, false);
      queue->head = current->next;
      free (current);
      current = queue->head;
    }

    if (!queue->head) queue->tail = 0;
  }
} /* </queue_process> */

/*
* initialize events queue
*/
ScreensaverQueue *
queue_initialize (Display *display)
{
  int screen;
  Window root;
  ScreensaverQueue *queue = g_new0 (ScreensaverQueue, 1);

  queue->display = display;
  queue->root = DefaultRootWindow (display);
  queue->tail = 0;
  queue->head = 0; 
  queue->sleep= 0; 

  for (screen = -1; ++screen < ScreenCount (display); ) {
    root = RootWindowOfScreen (ScreenOfDisplay (display, screen));
    queue_append (queue, root);
    events_select (display, root, True);
  }
  return queue;
} /* </queue_initialize> */

/*
* find out whether the pointer has moved using XQueryPointer
*/
void
query_pointer (ScreensaverQueue *queue)
{
  static Bool once = True;
  static Screen* screen;
  static Window root;

  static struct
  {
    int xpos, ypos;
    unsigned mask;
  } saved;

  Display *display = queue->display;
  Window window;

  int xpos, ypos;
  int idx, mark;
  unsigned mask;

  if (once) {
    screen = ScreenOfDisplay (display, DefaultScreen(display));
    root = DefaultRootWindow (display);
    saved.xpos = saved.ypos = -1;
    saved.mask = 0;
    once = false;
  }

  if (!XQueryPointer (display, root, &root, &window, &xpos, &ypos,
						&mark, &mark, &mask)) {
    /* Pointer has moved to another screen. */
    for (idx = -1; ++idx < ScreenCount (display); ) {
      screen = ScreenOfDisplay (display, idx);
      break;
    }
  }

  if (xpos != saved.xpos || ypos != saved.ypos || mask != saved.mask) {
    queue->sleep = 0;
    saved.xpos = xpos;
    saved.ypos = ypos;
    saved.mask = mask;
  }
} /* </query_pointer> */
#endif // _SCREENSAVER_DIAGNOSTICS

/*
* (private) shutdown_cb - callback for system shutdown
*/
static void
shutdown_cb (GtkWidget *widget, GlobalPanel *panel)
{
  shutdown_panel (panel, SIGTERM);
} /* </shutdown_cb> */

/*
* screensaver_refresh - enable screensaver settings save/cancel
*/
static void
screensaver_refresh (GtkWidget *canvas, GdkEventExpose *ev, gpointer data)
{
  ScreensaverSettings *config = local_.config;
  PanelSetting *settings = config->panel->settings;

  /* Register callbacks for apply and cancel. */
  settings_save_enable (settings, TRUE);
  settings_set_agents (settings,
                       (gpointer)screensaver_settings_apply,
                       (gpointer)screensaver_settings_cancel,
                       (gpointer)screensaver_settings_close);
} /* </screensaver_refresh> */

/*
* screensaver_configuration_read - reads configuration settings
*/
static void
screensaver_configuration_read (Modulus *applet, ScreensaverSettings *saver)
{
  GlobalPanel *panel = applet->data;
  ConfigurationNode *chain = configuration_find (panel->config, "modules");
  ConfigurationNode *item;

  const gchar *attrib;

  /* Configuration for the screen saver. */
  while ((item = configuration_find (chain, "applet")) != NULL) {
    attrib = configuration_attrib (item, "name");

    if (strcmp(attrib, "screensaver") == 0) {
      if ((attrib = configuration_attrib (item, "icon")) != NULL)
        applet->icon = attrib;

      if ((attrib = configuration_attrib (item, "mode")) != NULL) {
        if (strcmp(attrib, "off") == 0 || strcmp(attrib, "blank") == 0 ||
            strcmp(attrib, "one") == 0 || strcmp(attrib, "random") == 0)
          saver->savermode = attrib;	// {off,blank,one,random}
        else
          attrib = "blank";
      }

      if ((item = configuration_find (item, "settings")) != NULL) {
        if ((attrib = configuration_attrib (item, "timeout")) != NULL)
          saver->timer = atoi(attrib);

        if ((attrib = configuration_attrib (item, "cycle")) != NULL)
          saver->cycle = atoi(attrib);

        if ((attrib = configuration_attrib (item, "lock")) != NULL)
          saver->lockout = atoi(attrib);

        if ((attrib = configuration_attrib (item, "program")) != NULL)
          saver->program = attrib;

      }
      break;
    }
    chain = configuration_find_next (chain, "applet");
  }
} /* </screensaver_configuration_read> */

/*
* screensaver_fullscreen - display screensaver preview in fullscreen mode
*/
static void
screensaver_fullscreen (GtkWidget *button, ScreensaverSettings *config)
{
  const gint16 margin = 0;
  static char command[COMMAND_MAX];
  char xidstring[MAX_LABEL];

  gint width  = green_screen_width() - margin;
  gint height = gdk_screen_height();

  GtkWidget *widget = sticky_window_new (GDK_WINDOW_TYPE_HINT_DOCK,
					  width, height, margin, 0);
  gtk_widget_show (widget); // widget must be realized!

  GdkWindow *gdkwindow = gtk_widget_get_window (GTK_WIDGET (widget));
  GdkEventMask gdkevmask = GDK_BUTTON_PRESS_MASK | GDK_KEY_PRESS_MASK;
  Window xwin = GDK_WINDOW_XWINDOW (gdkwindow);

  g_assert (GDK_IS_WINDOW (gdkwindow));
  gdk_window_set_events (gdkwindow, gdkevmask);

  g_signal_connect (G_OBJECT(widget), "button-press-event",
                     G_CALLBACK (gtk_window_close), widget);

  g_signal_connect (G_OBJECT(widget), "key-press-event",
                     G_CALLBACK (gtk_window_close), widget);

  gtk_widget_set_sensitive (GTK_WIDGET(button), FALSE);
  pid_t process = local_.screenview = fork();

  if (process == 0) {		/* child process */
    sprintf(xidstring, "0x%lx", (unsigned long)xwin);
    sprintf(command, "%s/%s", _SCREENSAVER_MODES, config->program);
    vdebug(2, "%s command => %s, xid => 0x%lx\n", __func__, command, xwin);

    execlp(command, command,
           "--window-id", xidstring,
           NULL);
  }
  gtk_widget_set_sensitive (GTK_WIDGET(button), TRUE);	  // TODO: not yet!
} /* </screensaver_fullscreen> */

/*
* screensaver_preview - display screensaver preview in GdkWindow
*/
static pid_t
screensaver_preview (GdkWindow *gdkwindow, const char *program)
{
  static char command[COMMAND_MAX];
  ScreensaverSettings *_config = local_.config;
  Window xwin = GDK_WINDOW_XWINDOW (gdkwindow);
  char xidstring[MAX_LABEL];
  char *caption;

  killproc (&local_.preview, SIGTERM);	/* ensure preview process sigleton */
  pid_t process = fork();

  gint width, height;
  gdk_drawable_get_size (gdkwindow, &width, &height);

  caption = filechooser_iconbox_selection (_config->program, _config->chooser);
  gtk_label_set_text (GTK_LABEL(local_.preview_name), caption);

  if (process == 0) {			/* child process */
    sprintf(xidstring, "0x%lx", (unsigned long)xwin);
    sprintf(command, "%s/%s", _SCREENSAVER_MODES, program);
    vdebug(2, "%s command => %s, xid => 0x%lx\n", __func__, command, xwin);

    execlp(command, command,
           "--window-id", xidstring,
           NULL);
  }
  return process;
} /* </screensaver_preview> */

/*
* screensaver_preview_pane - call screensaver_preview with current presets
*/
pid_t
screensaver_preview_pane (void)
{
  pid_t process = 0;

  if (local_.preview == 0) {
    ScreensaverSettings *config = local_.config;
    screensaver_modes_t mode = config->selection;

    if (mode == SCREENSAVER_SINGLE || mode == SCREENSAVER_RANDOM) {
      GdkWindow *gdkwindow = gtk_widget_get_window (local_.preview_pane);
      //[unsafe]GdkWindow *gdkwindow = local_.preview_pane->window;

      if (gdkwindow != NULL) {
        const char *program = screensaver_modes_[config->index];
        char *caption = filechooser_iconbox_selection (config->program,
							config->chooser);

        gtk_label_set_text (GTK_LABEL(local_.preview_name), caption);
        process = local_.preview = screensaver_preview (gdkwindow, program);
      }
    }
  }
  return process;
} /* </screensaver_preview_pane> */

/*
* screensaver_settings_apply callback to save settings
*/
void
screensaver_settings_apply (Modulus *applet)
{
  static char *spec =
"<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
"<applet name=\"%s\" icon=\"%s\" mode=\"%s\">\n"
" <settings timeout=\"%d\" cycle=\"%d\" lock=\"%d\" program=\"%s\" />\n"
"</applet>\n";

  GlobalPanel *panel = applet->data;
  ScreensaverSettings *config = local_.config;
  const char *savermode = config->savermode;
  gchar *data;

  if (strcmp(savermode, "off") == 0 || strcmp(savermode, "blank") == 0 ||
      strcmp(savermode, "one") == 0 || strcmp(savermode, "random") == 0)
     config->savermode = savermode;
   else
     config->savermode = "blank";

  /* Save configuration settings. */
  data = g_strdup_printf (spec, "screensaver", local_.icon, config->savermode,
				config->timer, config->cycle, config->lockout,
				config->program);
  vdebug (3, "\n%s\n", data);

  /* Replace configuration item. */
  configuration_replace (panel->config, data, "modules",
				"applet", "screensaver");
  g_free (data);

  if (local_.confile) {		// Rewrite _xscreensaver_user_config
    static char newline = '\n';
    static char *pattern = "mode:\t\t";
    static char *timeout = "timeout:\t";

    char pathname[UNIX_PATH_MAX];
    sprintf(pathname, "%s/%s", getenv("HOME"), _xscreensaver_user_config);
    FILE *stream = fopen(pathname, "w");

    if (stream) {
      ScreensaverSettings *config = local_.config;

      const char *savermode;
      const char *buffer = local_.confile;
      char *scan = strchr(buffer, newline);
      char line[MAX_PATHNAME];

      gint16 selection = config->selection;
      int offset = 0;

      switch (selection) {
        case SCREENSAVER_DISABLE:	// Disable Screen Saver
          savermode = "off";
          break;
        case SCREENSAVER_SINGLE:	// Only One Screen Saver
          savermode = "one";
          break;
        case SCREENSAVER_RANDOM:	// Random Screen Saver
          savermode = "random";
          break;
        case SCREENSAVER_BLANK:		// Blank Screen Only
          savermode = "blank";
          break;
      }
      vdebug(2, "%s mode => %s [%d]\n", __func__, savermode, selection);

      while (scan != NULL) {
        *scan = 0;
        strcpy(line, &buffer[offset]);
        *scan = newline;		   // restore newline in buffer

        /* order is important: timers (timeout,cycle,lock) are first */
        if (strstr(line, timeout) != NULL) { // timeout,cycle, lock,lockTimeout
          char *lock = (config->lock) ? "True" : "False";

          vdebug(2, "%s timeout => %d, cycle => %d, lock(%d) => %s\n",__func__,
			config->timer, config->cycle, config->lockout, lock);

          sprintf(line, "%s0:%02d:00", timeout, config->timer);
          fprintf(stream, "%s\n", line);

          offset += strlen(line) + 1;
          scan = strchr(&buffer[offset], newline);

          *scan = 0;
          strcpy(line, &buffer[offset]);
          fprintf(stream, "cycle:\t\t0:%02d:00\n", config->cycle);

          offset += strlen(line) + 1;
          *scan = newline;

          sprintf(line, "lock:\t\t%s\nlockTimeout:\t0:%02d:00",
						lock, config->lockout);
          fprintf(stream, "%s\n", line);
	}
        else if (strstr(line, pattern) != NULL) { // mode, selected
          fprintf(stream, "%s%s\n", pattern, savermode);

          offset += strlen(line) + 1;
          scan = strchr(&buffer[offset], newline);

          *scan = 0;
          strcpy(line, &buffer[offset]);
          fprintf(stream, "selected:\t%d\n", config->index);
          *scan = newline;
        }
        else {
          fprintf(stream, "%s\n", line);
        }

        offset += strlen(line) + 1;	// must account for newline
        scan = strchr(&buffer[offset], newline);
      }
      fclose(stream);
    }
  }
} /* </screensaver_settings_apply> */

/*
* screensaver_settings_cancel callback to rollback settings
*/
void
screensaver_settings_cancel (Modulus *applet)
{
  if (local_.confile) {		// Restore _xscreensaver_user_config
    char pathname[UNIX_PATH_MAX];
    sprintf(pathname, "%s/%s", getenv("HOME"), _xscreensaver_user_config);
    FILE *stream = fopen(pathname, "w");

    if (stream) {
      const char *buffer = local_.confile;
      fwrite(buffer, local_.confile_size, 1, stream);
      fclose(stream);
    }
  }
} /* </screensaver_settings_cancel> */

/*
* screensaver_settings_close callback
*/
void
screensaver_settings_close (Modulus *applet)
{
  GlobalPanel *panel = (GlobalPanel *)applet->data;
  settings_set_agents (panel->settings, NULL, NULL, NULL);
} /* </screensaver_settings_close> */

/*
* screensaver_set_mode - set screensaver mode according to selection
*/
static void
screensaver_set_mode (GtkComboBox *combo, GlobalPanel *panel)
{
  ScreensaverSettings *config = local_.config;
  gint selection = gtk_combo_box_get_active (combo);

  if(selection == config->selection) return;	// do nothing.. return
  killproc (&local_.preview, SIGTERM);		// DEBUG maybe draconian
  config->selection = selection;

  switch (selection) {
    case SCREENSAVER_DISABLE:	// Disable Screen Saver
      gtk_label_set_text (GTK_LABEL(local_.preview_name), "");
      gtk_widget_set_sensitive(local_.preview_pane, FALSE);
      gtk_widget_set_sensitive(local_.selector_pane, FALSE);
      gtk_widget_set_sensitive(local_.settings_pane, FALSE);
      gtk_widget_set_sensitive(local_.fullscreen, FALSE);
      system_command (_SCREENSAVER_DISABLE);
      killproc (&local_.preview, SIGTERM);
      break;

    case SCREENSAVER_SINGLE:	// Only One Screen Saver
      gtk_widget_set_sensitive(local_.preview_pane, TRUE);
      gtk_widget_set_sensitive(local_.selector_pane, TRUE);
      gtk_widget_set_sensitive(local_.settings_pane, TRUE);
      gtk_widget_set_sensitive(local_.fullscreen, TRUE);
      local_.preview = screensaver_preview_pane();
      break;

    case SCREENSAVER_RANDOM:	// Random Screen Saver
      gtk_widget_set_sensitive(local_.preview_pane, TRUE);
      gtk_widget_set_sensitive(local_.selector_pane, TRUE);
      gtk_widget_set_sensitive(local_.settings_pane, TRUE);
      gtk_widget_set_sensitive(local_.fullscreen, TRUE);
      local_.preview = screensaver_preview_pane();
      break;

    case SCREENSAVER_BLANK:	// Blank Screen Only
      gtk_label_set_text (GTK_LABEL(local_.preview_name), "");
      gtk_widget_set_sensitive(local_.preview_pane, FALSE);
      gtk_widget_set_sensitive(local_.selector_pane, FALSE);
      gtk_widget_set_sensitive(local_.settings_pane, TRUE);
      gtk_widget_set_sensitive(local_.fullscreen, FALSE);
      killproc (&local_.preview, SIGTERM);
      break;
  }
  screensaver_refresh (local_.preview_pane, NULL, NULL);
} /* </screensaver_set_mode> */

/*
* screensaver_populate_modes - populate screensaver_modes_ array
*
* NOTE: program logic tied to xscreensaver >= 6.06 with _SCREENSAVER_MODES
*       and _SCREENSAVER_CONFIG separate directories.
*/
static gint16
screensaver_populate_modes(const char *buffer)
{
  static char newline = '\n';
  static char *pattern = "programs:";
  const size_t pattern_length = strlen(pattern);

  char line[MAX_PATHNAME];
  char *mark, *scan;
  gint16 count = 0;
  int offset = 0;

  while ((scan = strchr(&buffer[offset], newline)) != NULL) {
    *scan = 0;
    strcpy(line, &buffer[offset]);
    offset += strlen(line) + 1;		// must account for newline
    *scan = newline;			// restore newline in buffer

    if (strstr(line, pattern) != NULL) {
      while ((scan = strchr(&buffer[offset], newline)) != NULL) {
        *scan = 0;
        strcpy(line, &buffer[offset]);
        offset += strlen(line) + 1;	// must account for newline
        *scan = newline;		// restore newline in buffer

        if(line[0] >= 'a' && line[0] <= 'z') break;	// end of list

        // get to the first [a-z] character
        for (int pos = 0; pos < strlen(line); pos++) {
          if (line[pos] >= 'a' && line[pos] <= 'z') {
            mark = strchr(&line[pos], ' ');		// end of name
            *mark = 0;
            screensaver_modes_[count++] = strdup(&line[pos]);
            break;
          }
        }
        if(count == MAX_SCREENSAVER - 1) break;	// boundary check
      }
    }
  }
  screensaver_modes_[count] = 0;
  return count;
} /* </screensaver_populate_modes> */

/*
* (private) screensaver_timer_moretime - increase local_.config->timer
*/
static void
screensaver_timer_moretime(GtkWidget *widget, ScreensaverSettings *config)
{
  const int _max_timer = 60;
  GtkWidget *timer = local_.timer_entry; 
  gint16 value = atoi(gtk_entry_get_text (GTK_ENTRY(timer))) + 1;

  if (value < _max_timer) {
    char stamp[MAX_STAMP];
    sprintf(stamp, "%d", value);
    gtk_entry_set_text (GTK_ENTRY(timer), stamp);
    screensaver_refresh (local_.preview_pane, NULL, NULL);
    config->timer += 1;
  }
} /* </screensaver_timer_moretime> */

/*
* (private) screensaver_timer_lesstime - reduce local_.config->timer
*/
static void
screensaver_timer_lesstime(GtkWidget *widget, ScreensaverSettings *config)
{
  GtkWidget *timer = local_.timer_entry; 
  gint16 value = atoi(gtk_entry_get_text (GTK_ENTRY(timer))) - 1;

  if (value > 0) {
    char stamp[MAX_STAMP];
    sprintf(stamp, "%d", value);
    gtk_entry_set_text (GTK_ENTRY(timer), stamp);
    screensaver_refresh (local_.preview_pane, NULL, NULL);
    config->timer -= 1;
  }
} /* </screensaver_timer_lesstime> */

/*
* (private) screensaver_cycle_moretime - increase local_.config->cycle
*/
static void
screensaver_cycle_moretime(GtkWidget *widget, ScreensaverSettings *config)
{
  const int _max_cycle = 60;
  GtkWidget *cycle = local_.cycle_entry; 
  gint16 value = atoi(gtk_entry_get_text (GTK_ENTRY(cycle))) + 1;

  if (value < _max_cycle) {
    char stamp[MAX_STAMP];
    sprintf(stamp, "%d", value);
    gtk_entry_set_text (GTK_ENTRY(cycle), stamp);
    screensaver_refresh (local_.preview_pane, NULL, NULL);
    config->cycle += 1;
  }
} /* </screensaver_cycle_moretime> */

/*
* (private) screensaver_cycle_lesstime - reduce local_.config->cycle
*/
static void
screensaver_cycle_lesstime(GtkWidget *widget, ScreensaverSettings *config)
{
  GtkWidget *cycle = local_.cycle_entry; 
  gint16 value = atoi(gtk_entry_get_text (GTK_ENTRY(cycle))) - 1;

  if (value >= 0) {
    char stamp[MAX_STAMP];
    sprintf(stamp, "%d", value);
    gtk_entry_set_text (GTK_ENTRY(cycle), stamp);
    screensaver_refresh (local_.preview_pane, NULL, NULL);
    config->cycle -= 1;
  }
} /* </screensaver_cycle_lesstime> */

/*
* (private) screensaver_lockout_toggle - toggle screensaver lockout
*/
static void
screensaver_lockout_toggle(GtkWidget *widget, ScreensaverSettings *config)
{
  screensaver_refresh (local_.preview_pane, NULL, NULL);
  config->lock = !config->lock;
} /* </screensaver_lockout_toggle>

/*
* (private) screensaver_lockout_moretime - increase local_.config->lockout
*/
static void
screensaver_lockout_moretime(GtkWidget *widget, ScreensaverSettings *config)
{
  const int _max_lockout = 60;
  GtkWidget *lockout = local_.lock_entry; 
  gint16 value = atoi(gtk_entry_get_text (GTK_ENTRY(lockout))) + 1;

  if (value < _max_lockout) {
    char stamp[MAX_STAMP];
    sprintf(stamp, "%d", value);
    gtk_entry_set_text (GTK_ENTRY(lockout), stamp);
    screensaver_refresh (local_.preview_pane, NULL, NULL);
    config->lockout += 1;
  }
} /* </screensaver_lockout_moretime> */

/*
* (private) screensaver_lockout_lesstime - reduce local_.config->lockout
*/
static void
screensaver_lockout_lesstime(GtkWidget *widget, ScreensaverSettings *config)
{
  GtkWidget *lockout = local_.lock_entry;
  gint16 value = atoi(gtk_entry_get_text (GTK_ENTRY(lockout))) - 1;

  if (value >= 0) {
    char stamp[MAX_STAMP];
    sprintf(stamp, "%d", value);
    gtk_entry_set_text (GTK_ENTRY(lockout), stamp);
    screensaver_refresh (local_.preview_pane, NULL, NULL);
    config->lockout -= 1;
  }
} /* </screensaver_cycle_lesstime> */

/*
* numeric_entry - callback that limits input to be numeric
*/
void
numeric_entry(GtkEditable *editable, const gchar *text, gint length, gint *position, gpointer data)
{
  for (int idx = 0; idx < length; idx++) {
    if (!isdigit(text[idx])) {
      g_signal_stop_emission_by_name(G_OBJECT(editable), "insert-text");
      return;
    }
  }
} /* </numeric_entry> */

/*
* Blank After, Cycle After, and Lock Screen After
*/
GtkWidget *
screensaver_settings_grid(ScreensaverSettings *config)
{
  GtkWidget *button, *combo, *entry, *label;
  GtkWidget *layout = gtk_table_new(3, 3, FALSE);

  const _height = 22;
  const _entry_max = 3;
  const _entry_width = 36;
  const _button_width = 24;

  char stamp[MAX_STAMP];

  /* Blank After */
  combo = gtk_hbox_new (FALSE, 1);
  label = gtk_label_new (_("\t\tBlank After: "));
  gtk_box_pack_start (GTK_BOX(combo), label, FALSE, FALSE, 0);
  gtk_widget_show (label);

  entry = local_.timer_entry = gtk_entry_new ();
  gtk_entry_set_editable (GTK_ENTRY(entry), TRUE);
  gtk_entry_set_visibility (GTK_ENTRY(entry), TRUE);
  gtk_widget_set_can_focus (GTK_WIDGET(entry), TRUE);
  gtk_entry_set_overwrite_mode (GTK_ENTRY(entry), TRUE);
  gtk_entry_set_max_length (GTK_ENTRY(entry), _entry_max);
  gtk_widget_set_size_request (entry, _entry_width, _height);
  gtk_container_add (GTK_CONTAINER(combo), entry);

  sprintf(stamp, "%d", config->timer);
  gtk_entry_set_text (GTK_ENTRY(entry), stamp);
  g_signal_connect (G_OBJECT(entry), "insert-text",
				G_CALLBACK(numeric_entry), NULL);
  gtk_widget_show (entry);

  button = gtk_button_new_with_label ("+");
  gtk_widget_set_size_request (button, _button_width, _height);
  gtk_box_pack_start (GTK_BOX(combo), button, FALSE, FALSE, 0);
  g_signal_connect (G_OBJECT(button), "clicked",
				G_CALLBACK(screensaver_timer_moretime),
				config);
  gtk_widget_show (button);

  button = gtk_button_new_with_label ("-");
  gtk_widget_set_size_request (button, _button_width, _height);
  gtk_box_pack_start (GTK_BOX(combo), button, FALSE, FALSE, 0);
  g_signal_connect (G_OBJECT(button), "clicked",
				G_CALLBACK(screensaver_timer_lesstime),
				config);
  gtk_widget_show (button);

  label = gtk_label_new (_("minutes"));
  gtk_box_pack_start (GTK_BOX(combo), label, FALSE, FALSE, 0);
  gtk_widget_show (label);

  gtk_table_attach_defaults (GTK_TABLE(layout), combo, 0, 1, 0, 1);
  gtk_widget_show (combo);

  /* Cycle After */
  combo = gtk_hbox_new (FALSE, 1);
  label = gtk_label_new (_("\t\tCycle After: "));
  gtk_box_pack_start (GTK_BOX(combo), label, FALSE, FALSE, 0);
  gtk_widget_show (label);

  entry = local_.cycle_entry = gtk_entry_new ();
  gtk_entry_set_editable (GTK_ENTRY(entry), TRUE);
  gtk_entry_set_visibility (GTK_ENTRY(entry), TRUE);
  gtk_widget_set_can_focus (GTK_WIDGET(entry), TRUE);
  gtk_entry_set_overwrite_mode (GTK_ENTRY(entry), TRUE);
  gtk_entry_set_max_length (GTK_ENTRY(entry), _entry_max);
  gtk_widget_set_size_request (entry, _entry_width, _height);
  gtk_container_add (GTK_CONTAINER(combo), entry);

  sprintf(stamp, "%d", config->cycle);
  gtk_entry_set_text (GTK_ENTRY(entry), stamp);
  g_signal_connect (G_OBJECT(entry), "insert-text",
				G_CALLBACK(numeric_entry), NULL);
  gtk_widget_show (entry);

  button = gtk_button_new_with_label ("+");
  gtk_widget_set_size_request (button, _button_width, _height);
  gtk_box_pack_start (GTK_BOX(combo), button, FALSE, FALSE, 0);
  g_signal_connect (G_OBJECT(button), "clicked",
				G_CALLBACK(screensaver_cycle_moretime),
				config);
  gtk_widget_show (button);

  button = gtk_button_new_with_label ("-");
  gtk_widget_set_size_request (button, _button_width, _height);
  gtk_box_pack_start (GTK_BOX(combo), button, FALSE, FALSE, 0);
  g_signal_connect (G_OBJECT(button), "clicked",
				G_CALLBACK(screensaver_cycle_lesstime),
				config);
  gtk_widget_show (button);

  label = gtk_label_new (_("minutes"));
  gtk_box_pack_start (GTK_BOX(combo), label, FALSE, FALSE, 0);
  gtk_widget_show (label);

  gtk_table_attach_defaults (GTK_TABLE(layout), combo, 0, 1, 1, 2);
  gtk_widget_show (combo);

  /* Lock Screen After */
  combo = gtk_hbox_new (FALSE, 1);
  button = local_.lock_check = gtk_check_button_new_with_label (_("Lock Screen After: "));
  gtk_box_pack_start (GTK_BOX(combo), button, FALSE, FALSE, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), config->lock);
  gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
  g_signal_connect (G_OBJECT(button), "clicked",
				G_CALLBACK(screensaver_lockout_toggle),
				config);
  gtk_widget_show (button);

  entry = local_.lock_entry = gtk_entry_new ();
  gtk_entry_set_editable (GTK_ENTRY(entry), TRUE);
  gtk_entry_set_visibility (GTK_ENTRY(entry), TRUE);
  gtk_widget_set_can_focus (GTK_WIDGET(entry), TRUE);
  gtk_entry_set_overwrite_mode (GTK_ENTRY(entry), TRUE);
  gtk_entry_set_max_length (GTK_ENTRY(entry), _entry_max);
  gtk_widget_set_size_request (entry, _entry_width, _height);
  gtk_container_add (GTK_CONTAINER(combo), entry);

  sprintf(stamp, "%d", config->lockout);
  gtk_entry_set_text (GTK_ENTRY(entry), stamp);
  g_signal_connect (G_OBJECT(entry), "insert-text",
				G_CALLBACK(numeric_entry), NULL);
  gtk_widget_show (entry);

  button = gtk_button_new_with_label ("+");
  gtk_widget_set_size_request (button, _button_width, _height);
  gtk_box_pack_start (GTK_BOX(combo), button, FALSE, FALSE, 0);
  g_signal_connect (G_OBJECT(button), "clicked",
				G_CALLBACK(screensaver_lockout_moretime),
				config);
  gtk_widget_show (button);

  button = gtk_button_new_with_label ("-");
  gtk_widget_set_size_request (button, _button_width, _height);
  gtk_box_pack_start (GTK_BOX(combo), button, FALSE, FALSE, 0);
  g_signal_connect (G_OBJECT(button), "clicked",
				G_CALLBACK(screensaver_lockout_lesstime),
				config);
  gtk_widget_show (button);

  label = gtk_label_new (_("minutes"));
  gtk_box_pack_start (GTK_BOX(combo), label, FALSE, FALSE, 0);
  gtk_widget_show (label);

  gtk_table_attach_defaults (GTK_TABLE(layout), combo, 0, 1, 2, 3);
  gtk_widget_show (combo);

  return layout;
} /* </screensaver_settings_grid> */

/*
* screensaver_xmlfile - IconboxFilter classification (xscreensaver-6.06)
*/
static char *
screensaver_xmlfile_readline(char *buffer)
{
  static char newline = '\n';
  static char line[MAX_PATHNAME];

  char *scan = strchr(buffer, newline);
  int pos = scan - buffer;

  buffer[pos] = 0;
  strcpy(line, buffer);
  buffer[pos] = newline;	// restore newline in buffer

  return line;
} /* screensaver_xmlfile_readline */

static char *
screensaver_xmlfile_label(char *buffer)
{
  static char quotechar = '"';
  static char *labeltag = "_label=";
  static char *pattern = "<screensaver name=";

  const size_t labeltag_length = strlen(labeltag);
  const size_t pattern_length = strlen(pattern);

  char *scan = strstr(buffer, pattern) + pattern_length + 1;
  char *line = screensaver_xmlfile_readline (scan);
  char *label = strstr(line, labeltag) + labeltag_length + 1;

  if (label != NULL) {
    scan = strchr(label, quotechar);
    *scan = 0;
  }
  return label;
} /* screensaver_xmlfile_label */

bool
screensaver_xmlfile(IconboxDatum *ptr)
{
  bool active = true;
  static char *buffer = NULL;

  const char *xmlfile = ptr->name;
  const char *ext = get_filename_ext (xmlfile);


  if (ext == NULL || strcmp(ext, "xml") != 0)
    active = false;
  else {
    struct stat info;

    if (lstat(xmlfile, &info) != 0)		/* unhandled error condition */
      ptr->name[strlen(ptr->name) - 4] = '\0';  /* filename without extension */
    else {
      FILE *stream = fopen(xmlfile, "r");
      fseek(stream, 0, SEEK_END);
      long filesize = ftell(stream);

      if(buffer != NULL) free(buffer);
      buffer = malloc(filesize + 1);		/* allocate memory (+NULL) */ 

      fseek(stream, 0, SEEK_SET);		/* same as rewind() */
      fread(buffer, filesize, 1, stream);	/* read xmlfile in memory */
      buffer[filesize] = 0;
      fclose(stream);

      char *label = screensaver_xmlfile_label (buffer);
      if(label != NULL) strcpy(ptr->label, label);
    }
  }
  return active;
} /* screensaver_xmlfile */

/*
* screensaver_selection - selection callback function
*/
static bool
screensaver_selection (FileChooserDatum *datum)
{
  static char buffer[MAX_PATHNAME];
  FILE *stream = fopen(datum->file, "r");
  //[reminder] FileChooser *chooser = datum->self;
  bool found = false;

  if (stream != NULL) {
    static char quotechar = '"';
    static char *pattern = "<screensaver name=";
    const size_t pattern_length = strlen(pattern);

    static char *name = NULL;
    char *scan;

    while (fgets(buffer, sizeof(buffer)-1, stream) != NULL) {
      name = strstr(buffer, pattern);

      if (name != NULL) {
        name += pattern_length + 1;
        scan = strchr(name, quotechar);
        *scan = 0;

        vdebug(2, "%s name => %s\n", __func__, name);

        if (datum->user) {	// preview window
          ScreensaverSettings *config = datum->user;
          //[unsafe]GdkWindow *gdkwindow = local_.preview_pane->window;
          GdkWindow *gdkwindow = gtk_widget_get_window (local_.preview_pane);

          for (int idx = 0; idx < config->count; idx++) {
            if (strcmp(screensaver_modes_[idx], name) == 0) {
              config->index = idx;
              config->program = name;
              vdebug(1, "%s config->index => %d\n", __func__, config->index);
              screensaver_refresh (local_.preview_pane, NULL, NULL);
              local_.preview = screensaver_preview (gdkwindow, name);
              found = true;
              break;
            }
          }
        }
        break;
      }
    }
    fclose(stream);
  }
  return found;
} /* </screensaver_selection> */

/*
* screensaver_get_mode - get screensaver enum mode from memory config
*/
const int
screensaver_get_mode(const char *buffer)
{
  static char newline = '\n';
  static char *pattern = "mode:\t\t";
  const size_t pattern_length = strlen(pattern);

  char *mark = strstr(buffer, pattern);
  int mode = -1;

  if (mark != NULL) {
    mark += pattern_length;
    char *scan = strchr(mark, newline);

    if (scan != NULL) {
      *scan = 0;

      if (strcmp(mark, "off") == 0)
        mode = 0;
      else if (strcmp(mark, "one") == 0)
        mode = 1;
      else if (strcmp(mark, "random") == 0)
        mode = 2;
      else if (strcmp(mark, "blank") == 0)
        mode = 3;

      *scan = newline;		// restore newline in buffer
    }
  }
  return mode;
} /* </screensaver_get_mode> */

/*
* screensaver_get_selected - get screensaver_modes_[index] from memory config
*/
const int
screensaver_get_selected(const char *buffer)
{
  static char newline = '\n';
  static char *pattern = "selected:\t";
  const size_t pattern_length = strlen(pattern);

  char *mark = strstr(buffer, pattern);
  int selected = -1;

  if (mark != NULL) {
    mark += pattern_length;
    char *scan = strchr(mark, newline);

    if (scan != NULL) {
      *scan = 0;
      selected = atoi (mark);
      *scan = newline;		// restore newline in buffer
    }
  }
  return selected;
} /* </screensaver_get_selected> */

/*
* screensaver_get_setting - get screensaver setting (generic)
*/
const char *
screensaver_get_setting(const char *buffer, const char *pattern)
{
  static char newline = '\n';
  static char attrib[MAX_LABEL];
  char *mark = strstr(buffer, pattern);

  if (mark != NULL) {
    mark += strlen(pattern);
    char *scan = strchr(mark, newline);

    if (scan != NULL) {
      *scan = 0;
      strcpy(attrib, mark);
      *scan = newline;		// restore newline in buffer
    }
  }
  return (mark != NULL) ? attrib : "False";
} /* </screensaver_get_setting> */

/*
* screensaver_module_init - screen saver module initialization
*/
bool
screensaver_module_init (Modulus *applet, GlobalPanel *panel)
{
  if (path_finder(panel->path, _SCREENSAVER_COMMAND) == NULL) {
    /* TODO: warn that we did not find the screen saver application */
    return false;
  }
  char *homedir = getenv("HOME"); /* _xscreensaver_user_config at $HOME */
  ScreensaverSettings *_config;

  /* Initialize private data structure singleton. */
  memset(&local_, 0, sizeof (ScreensaverPrivate));
  _config = local_.config = g_new0 (ScreensaverSettings, 1);

  /* Modulus structure initialization. */
  applet->authors     = Authors;
  applet->release     = Release;
  applet->data        = panel;
  applet->description = _("Screen saver and lock module.");
  applet->name        = "screensaver";
  applet->icon        = "exit.png";
  applet->label       = "Screen Saver";
  applet->place       = PLACE_END;
  applet->space       = MODULI_SPACE_SERVICE | MODULI_SPACE_TASKBAR;

  /* Read configuration data for screensaver. */
  screensaver_configuration_read (applet, local_.config);

  char pathname[UNIX_PATH_MAX];
  sprintf(pathname, "%s/%s", homedir, _xscreensaver_user_config);

  if (access(pathname, R_OK) != 0)   // _xscreensaver_system_config fallback
    sprintf(pathname, "%s", _xscreensaver_system_config);

  if (access(pathname, R_OK) != 0) { // not enough! program will crash later
    static char *message = "screensaver initialization error.";
    gpanel_dialog (200, 300, ICON_ERROR, "%s: %s", Program, _(message));
    gould_error ("%s::%s: %s\n", Program, __func__, message);
  }
  else {
    static char *buffer = NULL;

    if (buffer == NULL) {
      FILE *stream = fopen(pathname, "r");

      if (stream != NULL) {
        fseek(stream, 0, SEEK_END);
        long filesize = local_.confile_size = ftell(stream);

        buffer = local_.confile = malloc(filesize + 1);	/* allocate memory  */
        fseek(stream, 0, SEEK_SET);			/* same as rewind() */

        fread(buffer, filesize, 1, stream);
        buffer[filesize] = 0;
        fclose(stream);

        /* write _xscreensaver_backup_config */
        sprintf(pathname, "%s/%s", homedir, _xscreensaver_backup_config);

        if ((stream = fopen(pathname, "w")) != NULL) {
          fwrite(buffer, filesize, 1, stream);
          fclose(stream);
        }
        else {
          static char *message = "cannot write";
          gpanel_dialog (300, 400, ICON_WARNING, "%s: %s: %s\n",
					__func__, _(message), pathname);
        }
      }
      else {	// not enough! program will crash later
        static char *message = "cannot read";
        gould_error ("%s: %s: %s\n", __func__, message, pathname);
        gpanel_dialog (300, 400, ICON_ERROR, "%s: %s: %s\n",
				__func__, _(message), pathname);
      }
    }

    if (buffer != NULL) {
      char *attrib = screensaver_get_setting (buffer, "lock:\t\t");

      _config->active = TRUE;		/* screensaver is enabled */
      _config->count = screensaver_populate_modes (buffer);
      _config->index = screensaver_get_selected (buffer);
      _config->selection = screensaver_get_mode (buffer);
      _config->program = screensaver_modes_[_config->index];
      _config->lock = strcmp(attrib, "True") ? false : true;
    }
    else {	// [TODO]confirm safe fallbacks settings
      static char *message = "internal program error (null buffer)";

      _config->active = FALSE;  	/* screensaver is disabled */
      _config->count = 0;
      _config->index = -1;
      _config->selection = 0;
      _config->program = "qix";
      _config->lock = false;

      gpanel_dialog (300, 400, ICON_ERROR, "%s %s\n", __func__, _(message));
    }
  }
  applet->settings = screensaver_settings_new (applet, panel);

  local_.icon     = "exit.png";		/* should match with applet->icon */
  local_.tooltips = gtk_tooltips_new();

  return true;
} /* </screensaver_module_init> */

/*
* screensaver_module_open - user interface constructed may not be used 
*/
void
screensaver_module_open (Modulus *applet)
{
  GtkWidget *button;
  GtkWidget *label = NULL;

  GlobalPanel *panel = applet->data;
  PanelIcons  *icons = panel->icons;

  /* Construct the user interface. */
  button = gtk_button_new();
  gtk_button_set_relief (GTK_BUTTON(button), GTK_RELIEF_NONE);
  g_signal_connect (G_OBJECT(button), "clicked", G_CALLBACK(shutdown_cb), panel);

  if (applet->icon) {
    const gchar *file = icon_path_finder (icons, applet->icon);
    if(file != NULL) label = image_new_from_file_scaled (file, icons->size, icons->size);
  }
  if(label == NULL) label = gtk_label_new (applet->name);

  gtk_container_add (GTK_CONTAINER(button), label);
  gtk_widget_show (label);

  /* Setup tooltips. */
  /* gtk_tooltips_set_tip (local_.tooltips, button, _("Screen Lock"), NULL); */
  gtk_tooltips_set_tip (local_.tooltips, button, _("Exit"), NULL);
  applet->widget = button;
} /* </screensaver_module_open */

/*
* screensaver_module_close
*/
void
screensaver_module_close (Modulus *applet)
{
  killproc (&local_.preview, SIGTERM);	/* ensure preview process singleton */
  if (local_.monitor) killproc (&local_.monitor, SIGTERM);
  gtk_widget_destroy (applet->widget);
} /* saver_module_close */

/*
* screensaver_settings_new provides the configuration page
*/
GtkWidget*
screensaver_settings_new (Modulus *applet, GlobalPanel *panel)
{
  GtkWidget *layout = gtk_vbox_new (FALSE, 0);
  GtkWidget *area, *button, *canvas, *frame, *glue;
  GtkWidget *layer, *pane, *scroll, *split, *widget;
  GtkTextBuffer *buffer;
  GdkColor color;

  FileChooser *chooser = filechooser_new (_SCREENSAVER_CONFIG,
						screensaver_xmlfile);
  ScreensaverSettings *_config = local_.config;
  char *caption = "";

  filechooser_set_callback (chooser, screensaver_selection, _config);
  local_.selector_pane = chooser->viewer;

  /* major reflection variables */
  _config->panel = panel;
  _config->chooser = chooser;

  /* spacer on the top */
  glue = gtk_label_new (" ");
  gtk_box_pack_start (GTK_BOX(layout), glue, FALSE, FALSE, 2);
  gtk_widget_show (glue);

  /* Split view: chooser on the left, preview on the right. */
  split = gtk_hbox_new (FALSE, 1);
  gtk_box_pack_start (GTK_BOX(layout), split, TRUE, TRUE, 2);
  gtk_widget_show (split);

  /* Assemble scrollable screensaver chooser. */
  area = gtk_vbox_new(FALSE, 0);
  gtk_box_pack_start (GTK_BOX (split), area, FALSE, FALSE, 0);
  gtk_widget_show (area);

  /* screensaver modes: label + combo_box */
  pane = gtk_hbox_new (FALSE, 1);
  gtk_box_pack_start (GTK_BOX(area), pane, FALSE, FALSE, 2);
  gtk_widget_show (pane);

  widget = gtk_label_new (_("Mode:"));
  gtk_box_pack_start (GTK_BOX(pane), widget, FALSE, FALSE, 2);
  gtk_widget_show (widget);

  widget = local_.mode_selection = gtk_combo_box_new_text ();
  gtk_combo_box_append_text (GTK_COMBO_BOX(widget), _("Disable Screen Saver"));
  gtk_combo_box_append_text (GTK_COMBO_BOX(widget), _("Only One Screen Saver"));
  gtk_combo_box_append_text (GTK_COMBO_BOX(widget), _("Random Screen Saver"));
  gtk_combo_box_append_text (GTK_COMBO_BOX(widget), _("Blank Screen Only"));
  gtk_combo_box_set_active (GTK_COMBO_BOX(widget), _config->selection);
  gtk_box_pack_start (GTK_BOX(pane), widget, FALSE, FALSE, 0);
  gtk_widget_show (widget);

  g_signal_connect (G_OBJECT(widget), "changed",
                      G_CALLBACK (screensaver_set_mode), panel);

  /* Assemble pane containing navigation. */
  layer = gtk_vbox_new (FALSE, 2);
  gtk_box_pack_start (GTK_BOX(area), layer, TRUE, TRUE, 2);
  gtk_widget_show (layer);

  /* spacer on right side */
  glue = gtk_label_new (" ");
  gtk_box_pack_start (GTK_BOX(layer), glue, FALSE, FALSE, 2);
  gtk_widget_show (glue);

  /* Assemble file selector in a scrolled window. */
  scroll = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW(scroll),
                                  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_box_pack_start (GTK_BOX(layer), scroll, TRUE, TRUE, 5);
  gtk_widget_show (scroll);

  widget = chooser->viewer;
  gtk_container_add (GTK_CONTAINER(scroll), widget);
  gtk_widget_show (widget);

  /* Blank After, Cycle After, and Lock Screen After */
  widget = local_.settings_pane = screensaver_settings_grid (_config);
  gtk_box_pack_start (GTK_BOX(area), widget, FALSE, FALSE, 4);
  gtk_widget_show (widget);

  /* Assemble preview display area. */
  area = gtk_vbox_new(FALSE, 1);
  gtk_box_pack_start (GTK_BOX(split), area, TRUE, TRUE, 8);
  gtk_widget_show (area);

  /* program descriptive name above preview area */
  if(_config->selection == SCREENSAVER_SINGLE
     || _config->selection == SCREENSAVER_RANDOM)
    caption = filechooser_iconbox_selection (_config->program, chooser);

  widget = local_.preview_name = gtk_label_new (caption);
  gdk_color_parse ("blue", &color);
  gtk_widget_modify_fg (widget, GTK_STATE_NORMAL, &color);
  gtk_box_pack_start (GTK_BOX(area), widget, FALSE, FALSE, 2);
  gtk_widget_show (widget);

  canvas = local_.preview_pane = gtk_text_view_new ();
  gtk_text_view_set_editable (GTK_TEXT_VIEW(canvas), FALSE);
  gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW(canvas), FALSE);
  gtk_text_view_set_justification (GTK_TEXT_VIEW(canvas), GTK_JUSTIFY_CENTER);
  gtk_text_view_set_pixels_above_lines (GTK_TEXT_VIEW(canvas), 30);

  gdk_color_parse ("red", &color);
  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (canvas));
  gtk_text_buffer_set_text (buffer, _("\nScreen saver preview"), -1);
  gtk_widget_modify_text (canvas, GTK_STATE_NORMAL, &color);

  gtk_box_pack_start (GTK_BOX(area), canvas, FALSE, FALSE, 6);
  gtk_widget_set_size_request (canvas, 300, 225);
  gtk_widget_show (canvas);

  /* Draw frame around display area. */
  frame = gtk_frame_new (NULL);
  gtk_container_add (GTK_CONTAINER(canvas), frame);
  gtk_widget_show (frame);

  //g_signal_connect (G_OBJECT (canvas), "expose_event",
  //                         G_CALLBACK (preview_refresh), NULL);

  pane = gtk_hbox_new(FALSE, 4);
  gtk_box_pack_start (GTK_BOX(area), pane, FALSE, TRUE, 0);
  gtk_widget_show (pane);

  /* right justify button */
  widget = gtk_label_new("");
  gtk_box_pack_start (GTK_BOX(pane), widget, TRUE, TRUE, 0);
  gtk_widget_show (widget);

  button = local_.fullscreen = gtk_button_new_with_label(_("Fullscreen"));
  gtk_box_pack_start (GTK_BOX(pane), button, FALSE, FALSE, 5);
  gtk_widget_show (button);

  g_signal_connect (G_OBJECT(button), "clicked",
                     G_CALLBACK(screensaver_fullscreen), _config);

  switch (_config->selection) {
    case SCREENSAVER_DISABLE:
      gtk_widget_set_sensitive(local_.preview_pane, FALSE);
      gtk_widget_set_sensitive(local_.selector_pane, FALSE);
      gtk_widget_set_sensitive(local_.settings_pane, FALSE);
      gtk_widget_set_sensitive(local_.fullscreen, FALSE);
      break;

    case SCREENSAVER_BLANK:
      gtk_widget_set_sensitive(local_.preview_pane, FALSE);
      gtk_widget_set_sensitive(local_.selector_pane, FALSE);
      //gtk_widget_set_sensitive(local_.settings_pane, TRUE);
      gtk_widget_set_sensitive(local_.fullscreen, FALSE);
      break;
  }
  return layout;
} /* </screensaver_settings_new> */
