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
#define COMMAND_MAX    128	/* command line max length */

//G_LOCK_DEFINE_STATIC (applet);
G_LOCK_DEFINE (applet);

extern const char *Program;	/* see, gpanel.c */
extern const char *Release;	/* ... */
extern debug_t debug;		/* ... */

/*
* Data structures used in implementation.
*/
typedef struct _SaverQueue     SaverQueue;
typedef struct _SaverQueueItem SaverQueueItem;

typedef struct _SaverCommand   SaverCommand;
typedef struct _SaverConfig    SaverConfig;
typedef struct _SaverPrivate   SaverPrivate;

struct _SaverQueueItem
{
  SaverQueueItem *next;
  time_t creationtime;
  Window window;
};

struct _SaverQueue
{
  SaverQueueItem *head;
  SaverQueueItem *tail;
  Display *display;
  Window root;
  guint sleep;			/* elapse time to activate screen saver/lock */
};

struct _SaverCommand		/* structure for screen saver commands */
{
  char screenlock[COMMAND_MAX];
  char screensave[COMMAND_MAX];
};

struct _SaverConfig
{
  gboolean active;		/* saver active (or not) */
  const gchar *mode;		/* screen save/lock mode */
  int selection;		/* save/lock mode selection */
  guint time;			/* screen save in minutes */
  guint lock;			/* screen lock in minutes */
  int index;			/* _xscreensaver_modes_directory index */
};

struct _SaverPrivate		/* private global structure */
{
  SaverConfig cache;		/* cache for screen saver data */
  SaverConfig *saver;		/* initial configuration data */

  const gchar *icon;
  GtkTooltips *tooltips;

  GtkWidget *enable_toggle;
  GtkWidget *mode_selection;
  GtkWidget *preview_pane;
  GtkWidget *time_entry;
  GtkWidget *lock_entry;

  pid_t monitor;		/* idle monitor process ID */
  pid_t preview;		/* saver preview process ID */
};

static char *modes_[MAX_SCREENSAVER];

static SaverPrivate local_;	/* private global structure singleton */

/* implementation prototypes forward declarations */
static void events_select (Display *display, Window window, Bool substructure);
static void queue_append (SaverQueue *queue, Window window);
static void queue_process (SaverQueue *queue, time_t age);

/*
* diagnostics
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
* (private) action
*/
static void
action (GtkWidget *widget, GlobalPanel *panel)
{
  shutdown_panel (panel, SIGTERM);
} /* </action> */

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
events_process (SaverQueue *queue)
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
queue_append (SaverQueue *queue, Window window)
{
  SaverQueueItem *item = g_new0 (SaverQueueItem, 1);

  item->creationtime = time (0);
  item->window = window;
  item->next = 0;

  if (!queue->head) queue->head = item;
  if (queue->tail) queue->tail->next = item;

  queue->tail = item;
} /* </queue_append> */

static void
queue_process (SaverQueue *queue, time_t age)
{
  if (queue->head) {
    time_t now = time (0);
    SaverQueueItem *current = queue->head;

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
SaverQueue *
queue_initialize (Display *display)
{
  int screen;
  Window root;
  SaverQueue *queue = g_new0 (SaverQueue, 1);

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
query_pointer (SaverQueue *queue)
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

/*
* screensaver_debug (<string>reason, <SaverConfig>saver, <bool>minutes)
*/
static void
screensaver_debug (const char *reason, SaverConfig *saver, gboolean minutes)
{
  if (minutes)
    printf("%s::saver.%s(mode=>%s,time=>%d,lock=>%d)\n", Program, reason,
                             saver->mode, saver->time, saver->lock);
  else
    printf("%s::saver.%s(mode=>%s,time=>%d,lock=>%d)\n", Program, reason,
                             saver->mode, saver->time/60, saver->lock/60);
} /* </screensaver_debug> */

/*
* Initialize the commands for screensaver and screenlock.
*/
void
screensaver_commands (SaverCommand *command, SaverConfig *config)
{
  screensaver_debug ("resume", config, false);

  sprintf(command->screenlock, "%s --activate", _SCREENSAVER_COMMAND);
  sprintf(command->screensave, "%s --select %d", _SCREENSAVER_COMMAND,
						config->index);

  config->active = (config->time > 0 || config->lock > 0) ? true : false;
} /* </screensaver_commands> */

/*
* check for control message and adjust settings
*/
SaverCommand *
screensaver_control (Display *display, GlobalShare *shared, SaverConfig *saver)
{
  static SaverCommand commands;	/* screen saver control data structure */
  static gboolean once = TRUE;

  Atom type;
  int format, status;
  unsigned long count, bytes;
  unsigned char *value = NULL;
  Window window = shared->window;

  if (once) {
    screensaver_commands (&commands, saver);
    once = FALSE;
  }

  /* See how much data is associated with Atom shared->saver */
  _X_ERROR_TRAP_PUSH ();

  status = XGetWindowProperty (display, window, shared->saver,
                               0L, 0L, False, AnyPropertyType,
                               &type, &format, &count, &bytes,
                               (unsigned char **)&value);
  if (status != Success)
    printf("%s screensaver_control[1] status => %d\n",
			_SCREENSAVER_COMMAND, status);

  if (bytes > 0) {
    status = XGetWindowProperty (display, window, shared->saver,
                                 0L, bytes, False, AnyPropertyType,
                                 &type, &format, &count, &bytes,
                                 (unsigned char **)&value);
    if (status != Success)
      printf("%s screensaver_control[2] status => %d\n",
			_SCREENSAVER_COMMAND, status);

    if (status == Success) {
      char *word = strchr((char*)value, ':');

      if (word) {		/* parse "mode:time:lock" */
        char *scan;

        *word++ = (char)0;
        saver->mode = (char *)value;

        scan = strchr(word, ':');	/* advance to :time: */
        *scan++ = (char)0;
        saver->time = atoi(word) * 60;
        saver->lock = atoi(scan) * 60;

        screensaver_commands (&commands, saver);
      }
      else if (strcmp((char *)value, "resume") == 0) {
        screensaver_debug ("resume", saver, False);
        saver->active = TRUE;
      }
      else if (strcmp((char *)value, "suspend") == 0) {
        screensaver_debug ("suspend", saver, False);
        saver->active = FALSE;
      }
      else {
        screensaver_debug ("debug", saver, False);
      }
      XDeleteProperty (display, window, shared->saver);
    }
  }
  _X_ERROR_TRAP_POP ("xlock::screensaver_control");

  return &commands;
} /* </screensaver_control> */

/*
* screensaver_fullscreen
*/
static void
screensaver_fullscreen (GtkWidget *button, SaverConfig *saver)
{
  char command[COMMAND_MAX];

  gtk_widget_set_sensitive (GTK_WIDGET(button), FALSE);
  sprintf(command, "%s/%s", _xscreensaver_modes_directory, saver->mode);
  system(command);

  gtk_widget_set_sensitive (GTK_WIDGET(button), TRUE);
} /* </screensaver_fullscreen> */

/*
* screensaver_preview
*/
static pid_t
screensaver_preview (GdkWindow *window, const gchar *mode)
{
  Window xwin = GDK_WINDOW_XWINDOW(window);
  char geometry[COMMAND_MAX], parent[COMMAND_MAX];
  pid_t process = fork();

  gint width, height;
  gdk_drawable_get_size (window, &width, &height);

  sprintf(parent, "%ld", (unsigned long)xwin);
  sprintf(geometry, "%dx%d", width, height);

  if (process > 0) {	/* not in spawned process */
    int stat;
    waitpid(process, &stat, 0);
    return process;
  }

  execlp(_SCREENSAVER_COMMAND,
	   _SCREENSAVER_COMMAND,
           "--activate",
           NULL);

  return 0;
} /* </screensaver_preview> */

/*
* setup screensaver child process
*/
void
screensaver (Display *display, GlobalShare *shared, SaverConfig *saver)
{
  pid_t parent = getppid();
  SaverQueue *queue = queue_initialize (display);
  SaverCommand *command;

  if (debug == 0) {		/* close stderr and stdout */
    fclose (stderr);
    fclose (stdout);
  }

  /* We need set error handler to ignore events. */
  XSetErrorHandler ((XErrorHandler)events_ignore);
  XSync (display, 0);

  /* Convert saver->time and saver->lock in seconds. */
  saver->time *= 60;
  saver->lock *= 60;

  while (kill(parent, 0) == 0) {	/* main event loop */
    command = screensaver_control (display, shared, saver);

    if (saver->active) {
      events_process (queue);		/* check for key press */
      query_pointer (queue);		/* check for pointer movement */

      if (queue->sleep > 0) {		/* screen saver and lock commands */
        if (saver->lock > 0 && queue->sleep >= saver->lock) {
          printf("%s: activating screenlock after %d minutes\n",
                                     Program, queue->sleep / 60);
          system (command->screenlock);
          queue->sleep = 0;
        }
        else if (saver->time > 0 && queue->sleep >= saver->time) {
          printf("%s: activating screensaver after %d minutes\n",
                                      Program, queue->sleep / 60);
          system (command->screensave);
          queue->sleep = 0;
        }
      }
      queue->sleep += 1;
    }

    sleep(1);	   /* 1 seconds resolution */
  }
} /* </screensaver> */

/*
* saver_configuration_read
*/
static void
saver_configuration_read (Modulus *applet, SaverConfig *saver)
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

      if ((item = configuration_find (item, "settings")) != NULL) {
        if ((attrib = configuration_attrib (item, "mode")) != NULL)
          saver->mode = attrib;

        if ((attrib = configuration_attrib (item, "time")) != NULL)
          saver->time = atoi(attrib);

        if ((attrib = configuration_attrib (item, "lock")) != NULL)
          saver->lock = atoi(attrib);
      }
      break;
    }
    chain = configuration_find_next (chain, "applet");
  }
} /* </saver_configuration_read> */

/*
* saver_settings_apply callback to save settings
* saver_settings_cancel callback to rollback settings
*/
void
saver_settings_apply (Modulus *applet)
{
  GlobalPanel *panel  = applet->data;
  GlobalShare *shared = panel->shared;
  SaverConfig *saver  = local_.saver;

  const char *spec;
  gchar *data;

  /* Save configuration settings from singleton. */
  memcpy(saver, &local_.cache, sizeof(SaverConfig));

  /* Communicate with child process via GlobalShare. */
  data = (saver->active)
       ? g_strdup_printf ("%s:%d:%d", saver->mode, saver->time, saver->lock)
       : g_strdup ("suspend");

  XChangeProperty (shared->display, shared->window, shared->saver, XA_STRING,
                   8, PropModeReplace, (unsigned char*)data, strlen(data));
  g_free (data);

  /* Save configuration settings in cache. */
  spec =
"<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
"<applet name=\"%s\" icon=\"%s\">\n"
" <settings mode=\"%s\" time=\"%d\" lock=\"%d\" />\n"
"</applet>\n";

  data = g_strdup_printf (spec, "screensaver", local_.icon,
                          ((saver->mode) ? saver->mode : "blank"),
                          saver->time, saver->lock);
  vdebug (1, "\n%s\n", data);

  /* Replace configuration item. */
  configuration_replace (panel->config, data, "modules",
				"applet", "screensaver");
  g_free (data);

  /* Memory was freed when original item was removed, read configuration. */
  saver_configuration_read (applet, saver);

  /* Save configuration cache data. */
  memcpy(&local_.cache, saver, sizeof(SaverConfig));
} /* </saver_settings_apply> */

void
saver_settings_cancel (Modulus *applet)
{
  GlobalPanel *panel  = applet->data;
  GlobalShare *shared = panel->shared;
  SaverConfig *saver  = local_.saver;

  int idx, mark = 0;
  gchar *value;


  /* Revert widgets to original values. */
  value = g_strdup_printf ("%d", saver->time);
  gtk_entry_set_text (GTK_ENTRY(local_.time_entry), value);
  g_free (value);

  value = g_strdup_printf ("%d", saver->lock);
  gtk_entry_set_text (GTK_ENTRY(local_.lock_entry), value);
  g_free (value);

  if (saver->mode)
    for (idx = 0; modes_[idx] != NULL; idx++)
      if (strcmp(modes_[idx], saver->mode) == 0) {
        mark = idx;
        break;
      }

  gtk_combo_box_set_active (GTK_COMBO_BOX(local_.mode_selection), mark);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(local_.enable_toggle),
                                saver->active);

  /* Communicate with child process via GlobalShare. */
  value = (saver->active) ? "resume" : "suspend";

  XChangeProperty (shared->display, shared->window, shared->saver, XA_STRING,
                   8, PropModeReplace, (unsigned char*)value, strlen(value));

  /* Cache configuration settings in singleton. */
  memcpy(&local_.cache, saver, sizeof(SaverConfig));
} /* </saver_settings_cancel> */

/*
* (private) saver_enable
*/
static void
saver_enable (GtkWidget *button, Modulus *applet)
{
  gboolean state = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(button));
  SaverConfig *saver_ = &local_.cache;

  if (state != saver_->active) {
    GlobalPanel *panel = applet->data;
    PanelSetting *settings = panel->settings;

    /* Register callbacks for apply and cancel. */
    settings_save_enable (settings, TRUE);
    settings_set_agents (settings,
                         (gpointer)saver_settings_apply,
                         (gpointer)saver_settings_cancel,
                         NULL);

    saver_->active = state;
    saver_wake (NULL, panel);
  }
} /* </saver_enable> */

/*
* screensaver_mode
*/
static void
screensaver_mode (GtkComboBox *combo, GlobalPanel *panel)
{
  char command[UNIX_PATH_MAX];
  int selection = gtk_combo_box_get_active (combo);

  vdebug(2, "%s: selection => %d\n", __func__, selection);

  switch (selection) {
    case 0:		// Disable Screen Saver => "=:gsession 1 0:="
      sprintf(command, _GSESSION_SERVICE_DISABLE, _SCREENSAVER);
      dispatch (panel->session, command);
      break;
    case 1:		// Only One Screen Saver
      break;
    case 2:		// Random Screen Saver
      break;
    case 3:		// Blank Screen Only
	system_command (_SCREENSAVER_ACTIVATE);
      break;
  }
} /* </screensaver_mode> */

/*
* (private) sendmode allows to change screen saver mode
* (private) sendtime allows to adjust screen saver timer
* (private) sendlock allows to adjust screen lock timer
*/
static void
sendmode (GtkComboBox *combo, GlobalPanel *panel)
{
  int selection = gtk_combo_box_get_active (combo);

  if (selection >= 0) {
    PanelSetting *settings = panel->settings;
    SaverConfig  *saver_   = &local_.cache;

    settings_save_enable (settings, TRUE);
    settings_set_agents (settings,
                         (gpointer)saver_settings_apply,
                         (gpointer)saver_settings_cancel,
                         NULL);

    if (strcmp(saver_->mode, modes_[selection]) != 0) {
      saver_->mode = modes_[selection];
      saver_wake (NULL, panel);
    }
  }
} /* </sendmode> */

static void
sendtime (GtkEntry *entry, GdkEventKey *event, GlobalPanel *panel)
{
  if (event->keyval == GDK_BackSpace || isdigit(event->keyval)) {
    PanelSetting *settings = panel->settings;
    SaverConfig *saver_ = &local_.cache;
    gchar *value;

    if (event->keyval == GDK_BackSpace) {
      value = g_strdup_printf ("%d", saver_->time);
      value[strlen(value) - 1] = (gchar)0;

      if (strlen(value) == 0)
        strcpy(value, "0");
    }
    else {
      if (saver_->time > 0)
        value = g_strdup_printf ("%d%c", saver_->time, event->keyval);
      else
        value = g_strdup_printf ("%c", event->keyval);
    }

    gtk_entry_set_text (entry, value);
    gtk_editable_set_position (GTK_EDITABLE(entry), -1);
    saver_->time = atoi(value);
    g_free (value);

    /* Register callbacks for apply and cancel. */
    settings_save_enable (settings, TRUE);
    settings_set_agents (settings,
                         (gpointer)saver_settings_apply,
                         (gpointer)saver_settings_cancel,
                         NULL);
  }
} /* </sendtime> */

static void
sendlock (GtkEntry *entry, GdkEventKey *event, GlobalPanel *panel)
{
  if (event->keyval == GDK_BackSpace || isdigit(event->keyval)) {
    SaverConfig *saver_ = &local_.cache;
    PanelSetting *settings = panel->settings;
    gchar *value;

    if (event->keyval == GDK_BackSpace) {
      value = g_strdup_printf ("%d", saver_->lock);
      value[strlen(value) - 1] = (gchar)0;

      if (strlen(value) == 0)
        strcpy(value, "0");
    }
    else {
      if (saver_->lock > 0)
        value = g_strdup_printf ("%d%c", saver_->lock, event->keyval);
      else
        value = g_strdup_printf ("%c", event->keyval);
    }

    gtk_entry_set_text (entry, value);
    gtk_editable_set_position (GTK_EDITABLE(entry), -1);
    saver_->lock = atoi(value);
    g_free (value);

    /* Register callbacks for apply and cancel. */
    settings_save_enable (settings, TRUE);
    settings_set_agents (settings,
                         (gpointer)saver_settings_apply,
                         (gpointer)saver_settings_cancel,
                         NULL);
  }
} /* </sendlock> */

/*
* populate_modes - populate modes_ from {dirname}
*/
static int
populate_modes(const char *dirname, const int maxcount)
{
  /* populate modes_ */
  DIR *dir = opendir (dirname);
  int mark = 0;

  if (dir) {
    struct dirent **list;
    int count = scandir(dirname, &list, NULL, alphasort);
    char *name;

    for (int idx = 0; idx < count; idx++) {
      name = list[idx]->d_name;
      if(name[0] == '.') continue;

      modes_[mark++] = name;
      if(mark == maxcount - 1) break;
    }
    modes_[mark] = (char*)0;
    closedir (dir);
  }
  return mark;
} /* </populate_modes> */

/*
* saver_settings provides configuration pages
*/
GtkWidget *
saver_settings (Modulus *applet, GlobalPanel *panel)
{
  GtkWidget *canvas, *frame, *scroll, *split;
  GtkWidget *area, *glue, *layer, *pane, *widget;
  GtkWidget *layout = gtk_vbox_new (FALSE, 0);

  SaverConfig *_saver = local_.saver;
  SaverConfig *_cache = &local_.cache;

  FileChooser *chooser = filechooser_new (_SCREENSAVER_CONFIG);

  int idx, mark = 0;
  int count = populate_modes(_SCREENSAVER_MODES, MAX_SCREENSAVER);

  if (_saver->mode)
    for (idx = 0; modes_[idx] != NULL; idx++)
      if (strcmp(modes_[idx], _saver->mode) == 0) {
        mark = idx;
        break;
      }

  /* Initialize private data structure singleton. */
  memcpy(_cache, _saver, sizeof(SaverConfig));

  /* Split view: chooser on the left, preview on the right. */
  split = gtk_hbox_new (FALSE, 1);
  gtk_box_pack_start (GTK_BOX(layout), split, FALSE, FALSE, 4);
  gtk_widget_show (split);

  /* Assemble scrollable screensaver chooser. */
  area = gtk_vbox_new(FALSE, 1);
  //area = desktop->browser = gtk_vbox_new(FALSE, 1);
  gtk_box_pack_start (GTK_BOX (split), area, FALSE, TRUE, 5);
  gtk_widget_show (area);

  /* screensaver modes: label + combo_box */
  pane = gtk_hbox_new (FALSE, 1);
  gtk_box_pack_start (GTK_BOX(area), pane, FALSE, FALSE, 2);
  gtk_widget_show (pane);

  widget = gtk_label_new (_("Mode:"));
  gtk_box_pack_start (GTK_BOX(pane), widget, FALSE, TRUE, 2);
  gtk_widget_show (widget);

  widget = local_.mode_selection = gtk_combo_box_new_text ();
  gtk_combo_box_append_text (GTK_COMBO_BOX(widget), _("Disable Screen Saver"));
  gtk_combo_box_append_text (GTK_COMBO_BOX(widget), _("Only One Screen Saver"));
  gtk_combo_box_append_text (GTK_COMBO_BOX(widget), _("Random Screen Saver"));
  gtk_combo_box_append_text (GTK_COMBO_BOX(widget), _("Blank Screen Only"));
  gtk_box_pack_start (GTK_BOX(pane), widget, FALSE, FALSE, 0);
  gtk_combo_box_set_active (GTK_COMBO_BOX(widget), 1);
  gtk_widget_show (widget);

  g_signal_connect (G_OBJECT(widget), "changed",
                    G_CALLBACK (screensaver_mode), panel);

  /* Assemble pane containing navigation. */
  layer = gtk_vbox_new (FALSE, 1);
  gtk_box_pack_start (GTK_BOX(area), layer, FALSE, FALSE, 2);
  gtk_widget_show (layer);

  pane = chooser->dirbox;
  gtk_box_pack_start (GTK_BOX(layer), pane, FALSE, FALSE, 2);
  gtk_widget_show (pane);

  /* Assemble file selector. */
  scroll = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW(scroll),
                                  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_box_pack_start (GTK_BOX(layer), scroll, TRUE, TRUE, 5);
  gtk_widget_show (scroll);

  widget = chooser->viewer;
  gtk_container_add (GTK_CONTAINER(scroll), widget);
  gtk_widget_show (widget);

  /* Blank After, Cycle After, and Lock Screen After */
  pane = gtk_hbox_new (FALSE, 1);
  gtk_box_pack_start (GTK_BOX(layer), pane, FALSE, FALSE, 2);
  gtk_widget_show (pane);

  widget = gtk_label_new (_("Blank After:"));
  gtk_box_pack_start (GTK_BOX(pane), widget, TRUE, TRUE, 2);
  gtk_widget_show (widget);

  pane = gtk_hbox_new (FALSE, 1);
  gtk_box_pack_start (GTK_BOX(layer), pane, FALSE, FALSE, 2);
  gtk_widget_show (pane);

  widget = gtk_label_new (_("Cycle After:"));
  gtk_box_pack_start (GTK_BOX(pane), widget, TRUE, TRUE, 2);
  gtk_widget_show (widget);

  pane = gtk_hbox_new (FALSE, 1);
  gtk_box_pack_start (GTK_BOX(layer), pane, FALSE, FALSE, 2);
  gtk_widget_show (pane);

  glue = gtk_hbox_new (FALSE, 1);
  gtk_box_pack_start (GTK_BOX(pane), glue, TRUE, TRUE, 2);
  gtk_widget_show (glue);

  widget = gtk_check_button_new ();
  gtk_box_pack_start (GTK_BOX(glue), widget, FALSE, FALSE, 2);
  gtk_widget_show (widget);

  widget = gtk_label_new (_("Lock Screen After:"));
  gtk_box_pack_start (GTK_BOX(glue), widget, FALSE, FALSE, 2);
  gtk_widget_show (widget);

  /* Assemble canvas display area. */
  area = gtk_vbox_new(FALSE, 4);
  //area = desktop->viewer = gtk_vbox_new(FALSE, 1);
  gtk_box_pack_start (GTK_BOX(split), area, TRUE, TRUE, 8);
  gtk_widget_show (area);

  pane = gtk_hbox_new(FALSE, 4);
  gtk_box_pack_start (GTK_BOX(area), pane, FALSE, FALSE, 0);
  gtk_widget_show (pane);

  widget = gtk_label_new(_("Ants walk around a simple maze."));
  gtk_box_pack_start (GTK_BOX(pane), widget, FALSE, FALSE, 0);
  gtk_widget_show (widget);

  /* Draw frame around display area. */
  frame = gtk_frame_new (NULL);
  gtk_container_add (GTK_CONTAINER(area), frame);
  gtk_widget_show (frame);

  canvas = gtk_drawing_area_new ();
  //canvas = desktop->canvas = gtk_drawing_area_new ();
  gtk_container_add (GTK_CONTAINER(frame), canvas);
  gtk_widget_show (canvas);

  //g_signal_connect (G_OBJECT (canvas), "expose_event",
  //                         G_CALLBACK (preview_refresh), NULL);

  pane = gtk_hbox_new(FALSE, 4);
  gtk_box_pack_start (GTK_BOX(area), pane, FALSE, TRUE, 0);
  gtk_widget_show (pane);

  /* right justify button */
  widget = gtk_label_new("");
  gtk_box_pack_start (GTK_BOX(pane), widget, TRUE, TRUE, 0);
  gtk_widget_show (widget);

  widget = gtk_button_new_with_label(_("Preview"));
  gtk_box_pack_start (GTK_BOX(pane), widget, FALSE, FALSE, 0);
  gtk_widget_show (widget);

//il SEGNO per oggi

  return layout;
} /* </saver_settings> */

/*
* saver_init screen saver module initialization
*/
bool
saver_init (Modulus *applet, GlobalPanel *panel)
{
  if (path_finder(panel->path, _SCREENSAVER_COMMAND) == NULL) {
    /* TODO: warn that we did not find the screen saver application */
    return false;
  }

  /* Modulus structure initialization. */
  applet->data    = panel;
  applet->name    = "screensaver";
  applet->icon    = "exit.png";
  applet->place   = PLACE_END;
  applet->space   = MODULI_SPACE_SERVICE | MODULI_SPACE_TASKBAR;
  applet->authors = Authors;
  applet->release = Release;

  /* Initialize private data structure singleton. */
  memset(&local_, 0, sizeof (SaverPrivate));
  local_.saver = g_new0 (SaverConfig, 1);

  /* Read configuration data for screen saver. */
  saver_configuration_read (applet, local_.saver);

  local_.icon     = "lock.png";
  //local_.icon     = "exit.png";
  local_.tooltips = gtk_tooltips_new();

  /* Construct the settings pages. */
  applet->label       = "Screen Saver";
  applet->description = _("Screen saver and lock module.");
  applet->settings = saver_settings (applet, panel);

  return true;
} /* </saver_init> */

/*
* saver_open
* saver_close
*/
void
saver_open (Modulus *applet)
{
  GtkWidget *button;
  GtkWidget *label = NULL;

  GlobalPanel *panel = applet->data;
  PanelIcons  *icons = panel->icons;

  /* Construct the user interface. */
  button = gtk_button_new();
  gtk_button_set_relief (GTK_BUTTON(button), GTK_RELIEF_NONE);
  g_signal_connect (G_OBJECT (button), "clicked", G_CALLBACK (action), panel);

  if (applet->icon) {
    const gchar *file = icon_path_finder (icons, applet->icon);

    if (file != NULL)
      label = image_new_from_file_scaled (file, icons->size, icons->size);
  }

  if (label == NULL)
    label = gtk_label_new (applet->name);

  gtk_container_add (GTK_CONTAINER(button), label);
  gtk_widget_show (label);

  /* Setup tooltips. */
  /* gtk_tooltips_set_tip (local_.tooltips, button, _("Screen Lock"), NULL); */
  gtk_tooltips_set_tip (local_.tooltips, button, _("Exit"), NULL);

  applet->widget = button;
} /* saver_open */

void
saver_close (Modulus *applet)
{
  killproc (&local_.preview, SIGTERM);	/* ensure preview singleton */
  if (local_.monitor) killproc (&local_.monitor, SIGTERM);
  gtk_widget_destroy (applet->widget);
} /* saver_close */

/*
* saver_get_mode
* saver_get_selection
*/
const char *
saver_get_mode()
{
  SaverConfig *saver = local_.saver;
  return saver->mode;
} /* saver_get_mode */

const int
saver_get_selection()
{
  SaverConfig *saver = local_.saver;
  return saver->selection;
} /* saver_get_selection */

/*
* saver_wake
*/
void
saver_wake (GtkWidget *widget, GlobalPanel *panel)
{
  SaverConfig *saver_ = &local_.cache;
  GdkWindow *window = local_.preview_pane->window;

  killproc (&local_.preview, SIGTERM);	/* ensure preview singleton */

  if (saver_->active && GDK_IS_DRAWABLE(window))
    local_.preview = screensaver_preview (window, saver_->mode);
} /* </saver_wake> */
