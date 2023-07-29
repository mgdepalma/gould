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

#include "gould.h"
#include "tasklist.h"

#include <X11/Xatom.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk/gdkx.h>
#include <math.h>

extern const char *Program;	/* must be declared in main program */

/**
* Private data structures.
*/
#define ALL_WORKSPACES (0xFFFFFFFF)

#define DEFAULT_BUTTON_WIDTH   200
#define DEFAULT_GROUPING_LIMIT 80

#define DEFAULT_HEIGHT 48
#define DEFAULT_WIDTH  1

#define MAX_WIDTH_CHARS 32
#define MIN_ICON_SIZE   16

#define N_SCREEN_CONNECTIONS 5

typedef enum
{
  TASK_CLASS_GROUP,
  TASK_STARTUP_SEQUENCE,
  TASK_WINDOW
} TasklistItemType;

typedef struct
{
  GtkWidget *desktop;
  GtkWidget *restore;
  GtkWidget *move;
  GtkWidget *resize;
  GtkWidget *minimize;
  GtkWidget *maximize;
} TasklistItemActions;

struct _TasklistItem
{
  Tasklist *tasklist;		/* GREEN_TASKLIST container */

  TasklistItemType type;
  TasklistItemActions actions;

  GreenWindow *window;		/* GreenWindow association */

  GtkWidget *button;		/* visual representation */
  GtkWidget *image;
  GtkWidget *label;

  GtkWidget *members;		/* same resource class members menu */
  GtkWidget *menu;		/* {Restore,Move,.., Close} menu */

  GdkPixbuf *screenshot;
  GdkPixbuf *screenshot_faded;

  bool button_toggle;		/* true => programmatic toggle */

  gdouble glow_start_time;
  guint   glow_button;
};

struct _TasklistPrivate {
  Green *green;			/* GREEN instance acted upon */

  TasklistGroupingType grouping;
  bool include_all_workspaces;
  GtkOrientation orientation;
  GtkReliefStyle relief;

  guint connection[N_SCREEN_CONNECTIONS];

  GHashTable *winhash;		/* hash table <Window, TasklistItem*> */
  GList *ungrouped;		/* complete ungrouped TasklistItem* list */
  GList *visible;		/* visible [grouped] TasklistItem* list */

  GdkPixbuf *pixbuf;
  GdkPixmap *background;

  GtkTooltips *tooltips;

  gint max_button_width;
  gint max_button_height;

  gint min_button_width;
  gint min_button_height;

  gint minimum_width;
  gint minimum_height;

  guint agent;			/* update handler reentrant guard */
};

/* (private) TaskList methods */
static gpointer parent_class_;		/* parent class instance */

static void tasklist_item_stop_glow (TasklistItem *item);

static void tasklist_connect_window (GreenWindow *window, Tasklist *tasklist);
static void tasklist_disconnect_window (GreenWindow *window);

static void tasklist_remove_startup_sequences (Tasklist *tasklist,
                                               GreenWindow *Window);

static inline void tasklist_queue_update_lists (Tasklist *tasklist,
                                                TasklistEventType event);

static void tasklist_update_lists (Tasklist *tasklist);

static void tasklist_viewports_changed (Green *green, Tasklist *tasklist);

static void
tasklist_window_opened (Green *green, GreenWindow *window, Tasklist *tasklist);

static void
tasklist_window_closed (Green *green, GreenWindow *window, Tasklist *tasklist);

/*
* tasklist_item_lookup
*/
inline TasklistItem *
tasklist_item_lookup (GreenWindow* window, Tasklist *tasklist)
{
  TasklistItem *item;
  TasklistPrivate *context = tasklist->priv;

  for (GList *iter = context->ungrouped; iter != NULL; iter = iter->next) {
    item = (TasklistItem *)iter->data;
    if (item->window == window) {
      return item;
    }
  }
  return NULL;
} /* </tasklist_item_lookup> */

/*
* tasklist_cleanup
*/
static inline void
tasklist_cleanup(Tasklist *tasklist)
{
  TasklistItem *item;
  TasklistPrivate *context = tasklist->priv;

  if (context->ungrouped) {
    for (GList *iter = context->ungrouped; iter != NULL; iter = iter->next) {
      item = iter->data;

      /**
      * if we just unref the item it means we lose our ref to the
      * item before we unparent the button, which breaks stuff.
      */
      tasklist_item_stop_glow (item);
#ifdef NEVER
      // see, tasklist_item_free()
      gtk_widget_destroy (item->button);
      g_free (item);
#endif
    }

    g_list_free (context->ungrouped);
    context->ungrouped = NULL;
  }

  if (context->visible) {
    g_list_free (context->visible);
    context->visible = NULL;
  }
  g_hash_table_remove_all (context->winhash);
} /* </tasklist_cleanup */

/*
* tasklist_event_string - TasklistEventType to string
*/
static inline char *
tasklist_event_string(TasklistEventType event)
{
  char *string = "<unsupported>";

  switch (event) {
    case TASKLIST_REALIZE:
      string = "realise";
      break;

    case TASKLIST_ACTIVE_WINDOW_CHANGED:
      string = "active-window-changed";
      break;
    case TASKLIST_ACTIVE_WORKSPACE_CHANGED:
      string = "active-workspace-changed";
      break;

    case TASKLIST_SET_GROUPING:
      string = "set-grouping";
      break;
    case TASKLIST_SET_INCLUDE_ALL_WORKSPACES:
      string = "set-include-all-workspaces";
      break;

    case TASKLIST_VIEWPORTS_CHANGED:
      string = "viewports-changed";
      break;

    case TASKLIST_WINDOW_ADDED:
      string = "window-added";
      break;
    case TASKLIST_WINDOW_ICON_CHANGED:
      string = "window-icon-changed";
      break;
    case TASKLIST_WINDOW_NAME_CHANGED:
      string = "window-name-changed";
      break;
    case TASKLIST_WINDOW_STATE_CHANGED:
      string = "window-state-changed";
      break;
    case TASKLIST_WINDOW_WORKSPACE_CHANGED:
      string = "window-workspace-changed";
      break;
    case TASKLIST_WINDOW_REMOVED:
      string = "window-removed";
      break;

    case TASKLIST_UNREALIZE:
      string = "unrealise";
      break;
  }
  return string;
} /* </tasklist_event_string> */

/*
* tasklist_item_pixbuf_glow
* tasklist_item_button_glow
* tasklist_item_queue_glow
* tasklist_item_stop_glow
*/
static GdkPixbuf *
tasklist_item_pixbuf_glow(TasklistItem *item, gdouble factor)
{
  GdkPixbuf *destination = gdk_pixbuf_copy (item->screenshot);
  
  if (destination)
    gdk_pixbuf_composite (item->screenshot_faded, destination, 0, 0,
                          gdk_pixbuf_get_width (item->screenshot),
                          gdk_pixbuf_get_height (item->screenshot),
                          0, 0, 1, 1, GDK_INTERP_NEAREST,
                          ABS((int)(factor * G_MAXUINT8)));
  
  return destination;
} /* </tasklist_item_pixbuf_glow> */

static bool
tasklist_item_button_glow(TasklistItem *item)
{
  GTimeVal tv;
  GdkPixbuf *glowing_screenshot;
  gdouble glow_factor, now;
  gfloat fade_opacity, loop_time;

  if (item->screenshot == NULL)
    return true;

  g_get_current_time (&tv);
  now = (tv.tv_sec * (1.0 * G_USEC_PER_SEC) + tv.tv_usec) / G_USEC_PER_SEC;

  if (item->glow_start_time <= G_MINDOUBLE)
    item->glow_start_time = now;

  gtk_widget_style_get (GTK_WIDGET (item->tasklist),
                        "fade-opacity", &fade_opacity,
                        "fade-loop-time", &loop_time,
                        NULL);
  
  glow_factor = fade_opacity * (0.5 - 0.5 * cos ((now - item->glow_start_time) * M_PI * 2.0 / loop_time));

  glowing_screenshot = tasklist_item_pixbuf_glow (item, glow_factor);
  if (glowing_screenshot == NULL)
    return true;

  gdk_draw_pixbuf (item->button->window,
                   item->button->style->fg_gc[GTK_WIDGET_STATE (item->button)],
                   glowing_screenshot,
                   0, 0, 
                   item->button->allocation.x, 
                   item->button->allocation.y,
                   gdk_pixbuf_get_width (glowing_screenshot),
                   gdk_pixbuf_get_height (glowing_screenshot),
                   GDK_RGB_DITHER_NORMAL, 0, 0);
  gdk_pixbuf_unref (glowing_screenshot);

  return true;
} /* </tasklist_item_button_glow > */

static void
tasklist_item_clear_glow_timeout(TasklistItem *item)
{
  item->glow_button = 0;
}

static void
tasklist_item_queue_glow(TasklistItem *item)
{
  if (item->glow_button == 0) {
    item->glow_start_time = 0.0;

    /* The animation doesn't speed up or slow down based on the
     * timeout value, but instead will just appear smoother or 
     * choppier.
     */
    item->glow_button =
        g_timeout_add_full (G_PRIORITY_DEFAULT_IDLE, 
                            50,
                            (GSourceFunc) tasklist_item_button_glow, item,
                            (GDestroyNotify) tasklist_item_clear_glow_timeout);
  }
} /* </tasklist_item_queue_glow> */

static void
tasklist_item_stop_glow(TasklistItem *item)
{
  if (item->glow_button != 0) {
    g_source_remove (item->glow_button);
    item->glow_button = 0;
    item->glow_start_time = 0.0;
  }
} /* </tasklist_item_stop_glow> */

/*
* tasklist_item_expose_widget
* tasklist_item_screenshot
*/
static void
tasklist_item_draw_dot(GdkWindow *window, GdkGC *lgc, GdkGC *dgc, int x, int y)
{
  gdk_draw_point (window, dgc, x,   y);
  gdk_draw_point (window, lgc, x+1, y+1);
}

static void
tasklist_item_expose_widget (GtkWidget *widget,
                             GdkPixmap *pixmap,
                             gint       x,
                             gint       y)
{
  GdkWindow *window;
  GdkEventExpose event;

  event.type = GDK_EXPOSE;
  event.window = pixmap;
  event.send_event = FALSE;
  event.region = NULL;
  event.count = 0;

  window = widget->window;
  widget->window = pixmap;
  widget->allocation.x += x;
  widget->allocation.y += y;

  event.area = widget->allocation;

  gtk_widget_send_expose (widget, (GdkEvent *) &event);

  widget->window = window;
  widget->allocation.x -= x;
  widget->allocation.y -= y;
} /* </tasklist_item_expose_widget> */

static GdkPixbuf*
tasklist_item_screenshot(TasklistItem *item)
{
  Tasklist  *tasklist = item->tasklist;
  GtkWidget *widget = GTK_WIDGET (tasklist);
  GdkPixmap *pixmap;
  GdkPixbuf *screenshot;

  gboolean overlay_rect;
  gint width, height;
  
  width = item->button->allocation.width;
  height = item->button->allocation.height;
  
  pixmap = gdk_pixmap_new (item->button->window, width, height, -1);
  
  /* First draw the button */
  gtk_widget_style_get (widget, "fade-overlay-rect", &overlay_rect, NULL);
  if (overlay_rect) {
    /* Draw a rectangle with bg[SELECTED] */
    gdk_draw_rectangle (pixmap, item->button->style->bg_gc[GTK_STATE_SELECTED],
                        TRUE, 0, 0, width + 1, height + 1);
  }
  else {
    GtkStyle *style;
    GtkStyle *attached_style;
      
    /* copy the style to change its colors around. */
    style = gtk_style_copy (item->button->style);
    style->bg[item->button->state] = style->bg[GTK_STATE_SELECTED];

    /* Now attach it to the window */
    attached_style = gtk_style_attach (style, pixmap);
    g_object_ref (attached_style);
      
    /* Copy the background */
    gdk_draw_drawable (pixmap, attached_style->bg_gc[GTK_STATE_NORMAL],
                       tasklist->priv->background,
                       item->button->allocation.x, item->button->allocation.y,
                       0, 0, width, height);
      
    /* Draw the button with our modified style instead of the real one. */
    gtk_paint_box (attached_style, (GdkWindow*) pixmap, item->button->state,
                   GTK_SHADOW_OUT, NULL, item->button, "button",
                   0, 0, width, height);

    g_object_unref (style);
    gtk_style_detach (attached_style);
    g_object_unref (attached_style);
  }
  
  /* Then the image and label */
  tasklist_item_expose_widget (item->image, pixmap,
                               -item->button->allocation.x,
                               -item->button->allocation.y);

  tasklist_item_expose_widget (item->label, pixmap,
                               -item->button->allocation.x,
                               -item->button->allocation.y);
  
  /* get the screenshot, and return */
  screenshot = gdk_pixbuf_get_from_drawable (NULL, pixmap, NULL, 0, 0,
                                             0, 0, width, height);
  g_object_unref (pixmap);
  
  return screenshot;
} /* </tasklist_item_screenshot> */

static void
tasklist_item_cleanup_screenshots(TasklistItem *item)
{
  if (item->screenshot) {
    g_object_unref (item->screenshot);
    item->screenshot = NULL;
  }
  if (item->screenshot_faded) {
    g_object_unref (item->screenshot_faded);
    item->screenshot_faded = NULL;
  }
} /* </tasklist_item_cleanup_screenshots> */

/*
* tasklist_item_expose
*/
static bool
tasklist_item_expose(GtkWidget *widget,
		     GdkEventExpose *event,
		     TasklistItem *item)
{
  GtkStyle *style;
  GdkGC *lgc, *dgc;
  int x, y, i, j;

  tasklist_item_cleanup_screenshots (item);	/* cleanup screenshots */

  switch (item->type) {
    case TASK_CLASS_GROUP:
      style = widget->style;
      lgc = style->light_gc[GTK_STATE_NORMAL];
      dgc = style->dark_gc[GTK_STATE_NORMAL];

      x = widget->allocation.x + widget->allocation.width -
          (GTK_CONTAINER (widget)->border_width + style->ythickness + 10);
      y = widget->allocation.y + style->xthickness + 2;

      for (i = 0; i < 3; i++)
        for (j = i; j < 3; j++)
          tasklist_item_draw_dot (widget->window, lgc, dgc, x + j*3, y + i*3);
      /* fall through to get screenshot */

    case TASK_WINDOW:
      if ((event->area.x <= widget->allocation.x) &&
          (event->area.y <= widget->allocation.y) &&
          (event->area.width >= widget->allocation.width) &&
          (event->area.height >= widget->allocation.height)) {
        if (item->glow_button != 0) {
          item->screenshot =
            gdk_pixbuf_get_from_drawable (NULL,
                                          widget->window,
                                          NULL,
                                          widget->allocation.x,
                                          widget->allocation.y,
                                          0, 0,
                                          widget->allocation.width,
                                          widget->allocation.height);

          /* we also need to take a screenshot for the faded state */
          item->screenshot_faded = tasklist_item_screenshot (item);
        }
      }
      break;

    case TASK_STARTUP_SEQUENCE:
      break;
  }
  return false;
} /* </tasklist_item_expose> */

/*
* tasklist_item_activate_window
*/
static void
tasklist_item_activate_window(TasklistItem *item, guint32 stamp)
{
  Green *green = item->tasklist->priv->green;
  XWindowState *state = green_window_get_state (item->window);

  int desktop = green_window_get_desktop (item->window);
  int space = green_get_active_workspace (green);

  if (space != desktop)		      /* always switch to window's desktop */
    green_change_workspace (green, desktop, stamp);

  if (state->hidden)		      /* restore minimized window */
    green_window_restore (item->window);
  else if (space == desktop)
    green_window_minimize (item->window);
} /* </tasklist_item_activate_window> */

/*
* tasklist_item_get_text
* tasklist_item_is_active
*/
const gchar *
tasklist_item_get_text(TasklistItem *item, Green *green, bool winame)
{
  const gchar *name, *text = NULL;

  if (winame || item->type == TASK_WINDOW) {
    XWindowState *state = green_window_get_state (item->window);
    name = green_window_get_name (item->window);

    if (state->hidden)
      text = g_strdup_printf ("[%s]", name);
    else if (state->shaded)
      text = g_strdup_printf ("=%s=", name);
    else
      text = g_strdup (name);
  }
  else {
    GList *list = green_get_group_list (green, item->window);
    name = green_window_get_class (item->window);
    text = g_strdup_printf ("%s (%d)", name, g_list_length (list));
  }

  return text;
} /* </tasklist_item_get_text> */

bool
tasklist_item_is_active(TasklistItem *item, Window window)
{
  bool state = false;

  if (item->type == TASK_WINDOW)
    state = (green_window_get_xid (item->window) == window) ? true : false;
  else if (item->type == TASK_CLASS_GROUP) {
    Green *green = item->tasklist->priv->green;
    GList *iter, *list = green_get_group_list (green, item->window);

    for (iter = list; iter != NULL; iter = iter->next)
      if (green_window_get_xid (iter->data) == window) {
        state = true;
        break;
      }
  }
  return state;
} /* </tasklist_item_is_active */

/*
* tasklist_item_activate_menu
* tasklist_item_position_menu
*/
static void
tasklist_item_activate_menu(GtkMenuItem *menuitem, TasklistItem *item)
{
  if (menuitem)
    tasklist_item_activate_window (item, gtk_get_current_event_time ());
  else {
    TasklistItemActions *action = &item->actions;
    XWindowState *state = green_window_get_state (item->window);

    if (green_get_workspace_count (item->tasklist->priv->green) > 1)
      gtk_widget_show (action->desktop);
    else
      gtk_widget_hide (action->desktop);

    if (state->hidden || (state->maximized_horz && state->maximized_vert)) {
      gtk_widget_set_sensitive (action->desktop, FALSE);
      gtk_widget_set_sensitive (action->restore, TRUE);
      gtk_widget_set_sensitive (action->move, FALSE);
      gtk_widget_set_sensitive (action->resize, FALSE);
      gtk_widget_set_sensitive (action->minimize, FALSE);
      gtk_widget_set_sensitive (action->maximize, FALSE);
    }
    else {
      gtk_widget_set_sensitive (action->desktop, TRUE);
      gtk_widget_set_sensitive (action->restore, FALSE);
      gtk_widget_set_sensitive (action->move, TRUE);
      gtk_widget_set_sensitive (action->resize, TRUE);
      gtk_widget_set_sensitive (action->minimize, TRUE);
      gtk_widget_set_sensitive (action->maximize, TRUE);
    }
  }
} /* </tasklist_item_activate_menu> */

static void
tasklist_item_position_menu(GtkMenu  *menu,
                            gint     *xpos,
                            gint     *ypos,
                            bool     *pushin,
                            gpointer data)
{
  GtkWidget *widget = GTK_WIDGET (data);
  GtkRequisition requisition;
  gint menu_xpos;
  gint menu_ypos;
  gint pointer_x;
  gint pointer_y;

  gtk_widget_size_request (GTK_WIDGET (menu), &requisition);
  gdk_window_get_origin (widget->window, &menu_xpos, &menu_ypos);
  gtk_widget_get_pointer (widget, &pointer_x, &pointer_y);

  menu_xpos += widget->allocation.x;
  menu_ypos += widget->allocation.y;

  if (menu_ypos >  gdk_screen_height () / 2)
    menu_ypos -= requisition.height;
  else
    menu_ypos += widget->allocation.height;

  if (requisition.width < pointer_x)
    menu_xpos += MIN (pointer_x, widget->allocation.width - requisition.width);

  *xpos = menu_xpos;
  *ypos = menu_ypos;
  *pushin = true;
} /* </tasklist_item_position_menu> */

/*
* tasklist_item_action_close
* tasklist_item_action_desktop
* tasklist_item_action_maximize
* tasklist_item_action_minimize
* tasklist_item_action_move
* tasklist_item_action_resize
* tasklist_item_action_restore
* tasklist_item_action_sticky
*/
static void
tasklist_item_action_close(GtkMenuItem *menuitem, TasklistItem *item)
{
  vdebug(2, "%s xid => 0x%x\n", __func__, green_window_get_xid(item->window));
  green_window_close (item->window);
} /* </tasklist_item_action_close> */

static void
tasklist_item_action_desktop (GtkMenuItem *menuitem, TasklistItem *item)
{
  int desktop = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (menuitem), "desktop"));
  green_window_change_workspace (item->window, desktop);
} /* </tasklist_item_action_desktop> */

static void
tasklist_item_action_maximize(GtkMenuItem *menuitem, TasklistItem *item)
{
  green_window_maximize (item->window);
} /* </tasklist_item_action_maximize> */

static void
tasklist_item_action_minimize(GtkMenuItem *menuitem, TasklistItem *item)
{
  green_window_minimize (item->window);
} /* </tasklist_item_action_minimize> */

static void
tasklist_item_action_move(GtkMenuItem *menuitem, TasklistItem *item)
{
  green_window_move (item->window);
} /* </tasklist_item_action_move> */

static void
tasklist_item_action_resize(GtkMenuItem *menuitem, TasklistItem *item)
{
  green_window_resize (item->window);
} /* </tasklist_item_action_resize> */

static void
tasklist_item_action_sticky(GtkMenuItem *menuitem, TasklistItem *item)
{
  green_window_change_workspace (item->window, ALL_WORKSPACES);
} /* </tasklist_item_action_sticky> */

static void
tasklist_item_action_restore(GtkMenuItem *menuitem, TasklistItem *item)
{
  Green *green = item->tasklist->priv->green;
  int desktop = green_window_get_desktop (item->window);

  if (green_get_active_workspace (green) != desktop)
    green_change_workspace (green, desktop, gtk_get_current_event_time ());

  green_window_restore (item->window);
} /* </tasklist_item_action_restore> */

/*
* tasklist_item_actions_menu_new
*/
static GtkWidget *
tasklist_item_actions_menu_new(TasklistItem *item)
{
  Green *green = item->tasklist->priv->green;
  GtkWidget *image, *menuitem, *menu = gtk_menu_new ();
  TasklistItemActions *ami = &item->actions;

  int space = green_window_get_desktop (item->window);
  int desktop = green_get_active_workspace (green);
  int n_spaces = green_get_workspace_count (green);

  /* Send to desktop submenu. */
  ami->desktop = gtk_image_menu_item_new_with_label (_("Send to desktop"));
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), ami->desktop);

  if (n_spaces > 1) {
    GtkWidget *submenu;
    const gchar *name;
    int idx;

    submenu = gtk_menu_new ();
    gtk_menu_item_set_submenu (GTK_MENU_ITEM (ami->desktop), submenu);
    image = xpm_image_scaled (ICON_WORKSPACE, MIN_ICON_SIZE, MIN_ICON_SIZE);

    for (idx = 0; idx < n_spaces; idx++) {
      name = green_get_workspace_name (green, idx);
      menuitem = gtk_image_menu_item_new_with_label (name);
      g_object_set_data (G_OBJECT(menuitem), "desktop", GINT_TO_POINTER (idx));

      if (space < 0 && desktop == idx)
        gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menuitem), image);

      g_signal_connect (G_OBJECT (menuitem), "activate",
                        G_CALLBACK (tasklist_item_action_desktop), item);

      gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);
      gtk_widget_show (menuitem);
    }

    /* Add menu item separator before the last menu item. */
    menuitem = gtk_separator_menu_item_new ();
    gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);
    gtk_widget_show (menuitem);

    menuitem = gtk_image_menu_item_new_with_label (_("All desktops"));

    if (space < 0)	/* item->window is already on all desktops */
      gtk_widget_set_sensitive (menuitem, FALSE);
    else {
      gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menuitem), image);

      g_signal_connect (G_OBJECT (menuitem), "activate",
                        G_CALLBACK (tasklist_item_action_sticky), item);
    }

    gtk_menu_shell_append (GTK_MENU_SHELL (submenu), menuitem);
    gtk_widget_show (menuitem);
  }

  /* Restore from minimized and/or maximized state. */
  ami->restore = menuitem = gtk_image_menu_item_new_with_label (_("Restore"));
  image = xpm_image_scaled (ICON_RESTORE, MIN_ICON_SIZE, MIN_ICON_SIZE);
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menuitem), image);

  g_signal_connect (G_OBJECT (menuitem), "activate",
                    G_CALLBACK (tasklist_item_action_restore), item);

  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
  gtk_widget_show (menuitem);

  /* Move window within its current workspace */
  ami->move = menuitem = gtk_image_menu_item_new_with_label (_("Move"));

  g_signal_connect (G_OBJECT (menuitem), "activate",
                    G_CALLBACK (tasklist_item_action_move), item);

  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
  gtk_widget_show (menuitem);

  /* Resize window */
  ami->resize = menuitem = gtk_image_menu_item_new_with_label (_("Resize"));

  g_signal_connect (G_OBJECT (menuitem), "activate",
                    G_CALLBACK (tasklist_item_action_resize), item);

  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
  gtk_widget_show (menuitem);

  /* Minimize window */
  ami->minimize = menuitem = gtk_image_menu_item_new_with_label (_("Minimize"));
  image = xpm_image_scaled (ICON_MINIMIZE, MIN_ICON_SIZE, MIN_ICON_SIZE);
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menuitem), image);

  g_signal_connect (G_OBJECT (menuitem), "activate",
                    G_CALLBACK (tasklist_item_action_minimize), item);

  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
  gtk_widget_show (menuitem);

  /* Maximize window */
  ami->maximize = menuitem = gtk_image_menu_item_new_with_label (_("Maximize"));
  image = xpm_image_scaled (ICON_MAXIMIZE, MIN_ICON_SIZE, MIN_ICON_SIZE);
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menuitem), image);

  g_signal_connect (G_OBJECT (menuitem), "activate",
                    G_CALLBACK (tasklist_item_action_maximize), item);

  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
  gtk_widget_show (menuitem);

  /* Add menu item separator before the last menu item. */
  menuitem = gtk_separator_menu_item_new ();
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
  gtk_widget_show (menuitem);

  menuitem = gtk_image_menu_item_new_with_label (_("Close"));
  image = gtk_image_new_from_stock (GTK_STOCK_CLOSE, GTK_ICON_SIZE_MENU);
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menuitem), image);

  g_signal_connect (G_OBJECT (menuitem), "activate",
                    G_CALLBACK (tasklist_item_action_close), item);

  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
  gtk_widget_show (menuitem);

  return menu;
} /* </tasklist_item_actions_menu_new> */

/*
* tasklist_item_actions_menu
* tasklist_item_members_menu
*/
static void
tasklist_item_actions_menu(TasklistItem *item)
{
  if (item->menu == NULL)
    item->menu = tasklist_item_actions_menu_new (item);

  tasklist_item_activate_menu (NULL, item);  /* enable/disable actions */

  gtk_menu_popup (GTK_MENU (item->menu), NULL, NULL,
                  tasklist_item_position_menu, item->button,
                  1, gtk_get_current_event_time ());

  gtk_widget_show (item->menu);
} /* </tasklist_item_actions_menu> */

static void
tasklist_item_members_menu(TasklistItem *item)
{
  Window xid;
  TasklistItem *scan;
  TasklistPrivate *context = item->tasklist->priv;

  GdkPixbuf *pixbuf;
  GtkWidget *image, *submenu, *menuitem, *menu = item->members;

  GList *iter, *list;
  const gchar *text;

  if (menu) {		/* remove old menu contents */
    list = gtk_container_get_children (GTK_CONTAINER (menu));

    for (iter = list; iter != NULL; iter = iter->next) {
      GtkWidget *child = GTK_WIDGET (iter->data);
      gtk_container_remove (GTK_CONTAINER (menu), child);
    }

    g_list_free (list);
  }
  else {
    item->members = menu = gtk_menu_new ();
    g_object_ref_sink (item->members);
  }

  /* Obtain the list of same resource class members */
  list = green_get_group_list (context->green, item->window);

  for (iter = list; iter != NULL; iter = iter->next) {
    xid = green_window_get_xid (scan->window);
    scan = g_hash_table_lookup (context->winhash, GUINT_TO_POINTER(xid));
    text = tasklist_item_get_text (scan, context->green, TRUE);
    menuitem = gtk_image_menu_item_new_with_label (text);
    g_free ((gchar *)text);

    if ((pixbuf = get_window_icon_scaled (xid, MIN_ICON_SIZE, MIN_ICON_SIZE))) {
      image = gtk_image_new_from_pixbuf (pixbuf);
      gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menuitem), image);
      gtk_widget_show (image);
      g_object_unref (pixbuf);
    }

    submenu = tasklist_item_actions_menu_new (scan);
    gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), submenu);
    tasklist_item_activate_menu (NULL, scan);

    gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
    gtk_widget_show (menuitem);
  }

  gtk_menu_popup (GTK_MENU (menu), NULL, NULL,
                  tasklist_item_position_menu, item->button,
                  1, gtk_get_current_event_time ());

  gtk_widget_show (menu);
} /* </tasklist_item_members_menu> */

/*
* tasklist_item_button_press
* tasklist_item_button_toggled
*/
static bool
tasklist_item_button_press(GtkWidget *widget,
                           GdkEventButton *event,
                           TasklistItem *item)
{
  switch (item->type) {
    case TASK_CLASS_GROUP:
      if (event->button == 1)
        tasklist_item_members_menu (item);
      break;

    case TASK_WINDOW:
      if (event->button == 1)
        tasklist_item_activate_window (item, gtk_get_current_event_time ());
      else if (event->button == 3)
        tasklist_item_actions_menu (item);
      break;

    case TASK_STARTUP_SEQUENCE:
      break;
  }
  return false;
} /* </tasklist_item_button_press> */

static bool
tasklist_item_button_toggled(GtkToggleButton *button, TasklistItem *item)
{
  g_return_val_if_fail (GTK_IS_TOGGLE_BUTTON (button), FALSE);

  if (item->button_toggle) /* gtk_toggle_button_set_active() called by code */
    return true;

  item->button_toggle = TRUE;
  gtk_toggle_button_set_active (button, !gtk_toggle_button_get_active (button));
  item->button_toggle = FALSE;

  return false;
} /* </tasklist_item_button_toggled> */

/*
* tasklist_item_init
* tasklist_item_new
*/
static void
tasklist_item_init(TasklistItem *item, Tasklist *tasklist)
{
  static const GtkTargetEntry targets[] = {
    { "application/x-gould-window-id", 0, 0 }
  };

  TasklistPrivate *context = tasklist->priv;
  Window xid = green_window_get_xid (item->window);
  GtkWidget *label, *image, *box;
  GdkPixbuf *pixbuf;
  gchar *text;

  if (context->orientation == GTK_ORIENTATION_HORIZONTAL)
    box = gtk_hbox_new (FALSE, 0);
  else
    box = gtk_vbox_new (FALSE, 0);

  if (item->type != TASK_STARTUP_SEQUENCE)
    item->button = gtk_toggle_button_new ();
  else
    item->button = gtk_button_new ();

  gtk_button_set_relief (GTK_BUTTON (item->button), context->relief);
  gtk_drag_dest_set (GTK_WIDGET(item->button), 0, NULL, 0, 0);
  gtk_widget_set_name (item->button, green_window_get_name (item->window));

  if (item->type == TASK_WINDOW)
    gtk_drag_source_set (GTK_WIDGET (item->button),
                         GDK_BUTTON1_MASK,
                         targets, 1,
                         GDK_ACTION_MOVE);

  /* Pack the icon image to the box container. */
  if ((pixbuf = get_window_icon_scaled (xid, MIN_ICON_SIZE, MIN_ICON_SIZE))) {
    image = gtk_image_new_from_pixbuf (pixbuf);
    g_object_unref (pixbuf);
  }
  else
    image = gtk_image_new_from_stock (GTK_STOCK_MISSING_IMAGE,
                                      GTK_ICON_SIZE_MENU);

  gtk_box_pack_start (GTK_BOX (box), image, FALSE, FALSE, 4);
  gtk_widget_show (image);
  item->image = image;

  /* Pack the text label to the box container, also set tooltip. */
  text = (gchar *)tasklist_item_get_text (item, context->green, FALSE);
  gtk_tooltips_set_tip (context->tooltips, item->button, text, NULL);
  label = gtk_label_new (text);
  g_free (text);

  if (context->orientation == GTK_ORIENTATION_HORIZONTAL) {
    gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
    gtk_box_pack_start (GTK_BOX (box), label, TRUE, TRUE, 2);
  }
  else {
    gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.0);
    gtk_box_pack_start (GTK_BOX (box), label, FALSE, FALSE, 0);
  }

  gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
  gtk_label_set_max_width_chars (GTK_LABEL (label), MAX_WIDTH_CHARS);

  gtk_widget_show (label);
  item->label = label;

  /* Add the box with the icon image and text label to the button. */
  gtk_container_add (GTK_CONTAINER (item->button), box);
  gtk_widget_show (box);

  /* Set up signals. */
  if (GTK_IS_TOGGLE_BUTTON (item->button))
    g_signal_connect (G_OBJECT (item->button), "toggled",
                      G_CALLBACK (tasklist_item_button_toggled), item);

  g_signal_connect (G_OBJECT (item->button), "button_press_event",
                    G_CALLBACK (tasklist_item_button_press), item);

  g_signal_connect (item->button, "expose_event",
                    G_CALLBACK (tasklist_item_expose), item);

  tasklist_connect_window (item->window, tasklist);
} /* </tasklist_item_init> */

/*
* tasklist_item_new
* tasklist_item_free
*/
TasklistItem *
tasklist_item_new(GreenWindow *window,
                  TasklistItemType type,
                  Tasklist *tasklist)
{
  TasklistItem *item = g_new0 (TasklistItem, 1);

  item->window   = window;
  item->tasklist = tasklist;
  item->type     = type;

  tasklist_item_init (item, tasklist);
  tasklist_remove_startup_sequences (tasklist, window);

  return item;
} /* </tasklist_item_new> */

void
tasklist_item_free(TasklistItem *item, Tasklist *tasklist)
{
  TasklistPrivate *context = tasklist->priv;
  Window xid = green_window_get_xid (item->window);

  tasklist_item_stop_glow (item);
  g_hash_table_remove(context->winhash, GUINT_TO_POINTER(xid));

  for (GList *iter = context->ungrouped; iter != NULL; iter = iter->next)
    if (item == (TasklistItem *)iter->data) {
      context->ungrouped = g_list_remove (context->ungrouped, item);
      break;
    }

  //DEBUG gtk_widget_hide (item->button);
  gtk_widget_destroy (item->button);
  //SIGSEGV g_free (item);
} /* </tasklist_item_free> */

/*
* tasklist_item_update
*/
static void
tasklist_item_update(TasklistItem *item, TasklistEventType event)
{
  TasklistPrivate *context = item->tasklist->priv;
  Window xid = green_window_get_xid (item->window);
  GdkPixbuf *pixbuf;
  gchar *text;

  vdebug (2, "Tasklist::item_update: event => %s\n",
				tasklist_event_string (event));

  pixbuf = get_window_icon_scaled (xid, MIN_ICON_SIZE, MIN_ICON_SIZE);
  gtk_image_set_from_pixbuf (GTK_IMAGE (item->image), pixbuf);
  g_object_unref (pixbuf);

  if ((text = (gchar *)tasklist_item_get_text (item, context->green, FALSE))) {
    PangoFontDescription *fontdesc = pango_font_description_new ();
    XWindowState *state = green_window_get_state (item->window);
    bool attention = false;

#ifdef _NET_WM_STATE_DEMANDS_ATTENTION_PROPERLY_HANDLED
    if (state && state->demands_attention)
      attention = true;
    else if (item->type == TASK_CLASS_GROUP) {
      GList *iter, *list = green_get_group_list (context->green, item->window);

      for (iter = list; iter; iter = iter->next) {
        xid = green_window_get_xid (item->window);
        if (g_hash_table_lookup (context->winhash, GUINT_TO_POINTER(xid))) {
          state = green_window_get_state (iter->data);

          if (state && state->demands_attention) {
            attention = true;
            break;
          }
        }
      }
    }
#endif

    gtk_label_set_text (GTK_LABEL (item->label), text);
    g_free (text);

    if (attention) {
      pango_font_description_set_weight (fontdesc, PANGO_WEIGHT_BOLD);
      tasklist_item_queue_glow (item);
    }
    else {
      pango_font_description_set_weight (fontdesc, PANGO_WEIGHT_NORMAL);
      tasklist_item_stop_glow (item);
    }

    gtk_widget_modify_font (item->label, fontdesc);
    pango_font_description_free (fontdesc);
  }
} /* </tasklist_item_update> */

/*
* tasklist_layout - returns the maximal possible button width
*
* (i.e. if you don't want to stretch the buttons to fill the allocation
*  the width can be smaller)
*/
static int
tasklist_layout(GtkAllocation *allocation,
                int            max_width,
                int            max_height,
                int            n_buttons,
                int           *n_cols_out,
                int           *n_rows_out)
{
  int n_cols, n_rows;

  n_rows = allocation->height / max_height; /* how many rows fit allocation */
  n_rows = MIN (n_rows, n_buttons);   /* do not have more rows than buttons */
  n_rows = MAX (n_rows, 1);	      /* at least one row */

  /* We want to use as many rows as possible to limit the width. */
  n_cols = (n_buttons + n_rows - 1) / n_rows;
  n_cols = MAX (n_cols, 1);	      /* at least one column */

  *n_cols_out = n_cols;
  *n_rows_out = n_rows;

  return allocation->width / n_cols;  /* evenly sized by allocation width */
} /* </tasklist_layout> */

/*
* tasklist_widget_realize
* tasklist_widget_unrealize
* tasklist_widget_size_request
* tasklist_widget_size_allocate
* tasklist_widget_expose
*/
static void
tasklist_widget_realize(GtkWidget *widget)
{
  (* GTK_WIDGET_CLASS (parent_class_)->realize) (widget);
  vdebug(2, "%s %s: widget => 0x%lx\n", timestamp(), __func__, widget);
  tasklist_queue_update_lists (GREEN_TASKLIST(widget), TASKLIST_REALIZE);
} /* </tasklist_widget_realize> */

static void
tasklist_widget_unrealize (GtkWidget *widget)
{
  (* GTK_WIDGET_CLASS (parent_class_)->unrealize) (widget);
  vdebug(2, "%s %s: instance => 0x%lx\n", timestamp(), __func__, widget);
  tasklist_queue_update_lists (GREEN_TASKLIST(widget), TASKLIST_UNREALIZE);
} /* </tasklist_widget_unrealize> */

static void
tasklist_widget_size_request (GtkWidget *widget, GtkRequisition *requisition)
{
  Tasklist *tasklist = GREEN_TASKLIST (widget);
  TasklistPrivate *context = tasklist->priv;

  GtkRequisition measure;
  GList *iter;

  gint height = 1;
  gint width  = 1;


  /* Calculate max needed height and width of the buttons */
  for (iter = context->visible; iter != NULL; iter = iter->next) {
    TasklistItem *item = iter->data;

    gtk_widget_size_request (item->button, &measure);

    height = MAX (measure.height, height);
    width  = MAX (measure.width, width);
  }

  context->max_button_width  = MAX (context->min_button_width, width);
  context->max_button_height = height;

  requisition->width  = context->minimum_width;
  requisition->height = context->minimum_height;
} /* </tasklist_widget_size_request> */

static void
tasklist_widget_size_allocate(GtkWidget *widget, GtkAllocation *allocation)
{
  Tasklist *tasklist = GREEN_TASKLIST (widget);
  TasklistPrivate *context = tasklist->priv;

  TasklistItem *item;
  GtkAllocation measure;
  GList *iter;

  int idx = 0, n_items = g_list_length (context->visible);
  int max_width, n_cols, n_rows;


  max_width = tasklist_layout (allocation,
                               context->max_button_width,
                               context->max_button_height,
                               n_items, &n_cols, &n_rows);

  /* Recalculate max_width using max_button_width for optimal layout. */
  max_width = MIN (context->max_button_width * n_cols, allocation->width);

  for (iter = context->visible; iter != NULL; idx++,iter = iter->next) {
    item = (TasklistItem *)iter->data;

    int row = idx % n_rows;
    int col = idx / n_rows;

    if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL)
      col = n_cols - col - 1;

    measure.x = max_width * col / n_cols;
    measure.y = allocation->height * row / n_rows;
    measure.width = max_width * (col + 1) / n_cols - measure.x;
    measure.height = allocation->height * (row + 1) / n_rows - measure.y;
    measure.x += allocation->x;
    measure.y += allocation->y;

    gtk_widget_size_allocate (item->button, &measure);
    gtk_widget_set_child_visible (item->button, TRUE);
  }

  GTK_WIDGET_CLASS (parent_class_)->size_allocate (widget, allocation);
} /* </tasklist_widget_size_allocate> */

static gint
tasklist_widget_expose(GtkWidget *widget, GdkEventExpose *event)
{
  g_return_val_if_fail (IS_GREEN_TASKLIST (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  if (GTK_WIDGET_DRAWABLE (widget)) {
    Tasklist *tasklist = GREEN_TASKLIST (widget);
    TasklistPrivate *context = tasklist->priv;
    GdkGC *gc;

    if (context->background != NULL)
      g_object_unref (context->background);

    context->background = gdk_pixmap_new (widget->window,
                                          widget->allocation.width,
                                          widget->allocation.height,
                                          -1);
    gc = gdk_gc_new (context->background);
    gdk_draw_drawable (context->background, gc, widget->window,
                       widget->allocation.x, widget->allocation.y, 0, 0,
                       widget->allocation.width, widget->allocation.height);
    g_object_unref (gc);
  }

  return (*GTK_WIDGET_CLASS(parent_class_)->expose_event)(widget, event);
} /* </tasklist_widget_expose> */

/*
* tasklist_container_forall
* tasklist_container_remove
*/
static void
tasklist_container_forall(GtkContainer *container,
                          bool         internals,
                          GtkCallback  callback,
                          gpointer     data)
{
  Tasklist *tasklist = GREEN_TASKLIST (container);
  TasklistPrivate *context = tasklist->priv;
  time_t start = time(0L);
  GList *iter;

  for (iter = context->visible; iter != NULL; iter = iter->next) {
    TasklistItem *item = iter->data;

    if (time(0L) - start > 2) { /* do not loop more than 2 seconds, debug */
      g_printerr ("%s: tasklist_container_forall: not responding.\n", Program);
      break;
    }

    switch (item->type) {
      case TASK_CLASS_GROUP:
      case TASK_WINDOW:
        (* callback) (item->button, data);
        break;

      case TASK_STARTUP_SEQUENCE:
        break;
    }
  }
} /* </tasklist_container_forall> */

static void
tasklist_container_remove(GtkContainer *container, GtkWidget *widget)
{
  Tasklist *tasklist = GREEN_TASKLIST (container);
  TasklistPrivate *context = tasklist->priv;
  TasklistItem *item;

  for (GList *iter = context->visible; iter != NULL; iter = iter->next) {
    item = (TasklistItem *)iter->data;

    if (item->button == widget) {
      Window xid = green_window_get_xid (item->window);
      g_hash_table_remove(context->winhash, GUINT_TO_POINTER(xid));
      context->visible = g_list_remove (context->visible, item);
      gtk_widget_unparent (widget);
      g_free (item);
      break;
    }
  }
  gtk_widget_queue_resize (GTK_WIDGET (container));
} /* </tasklist_container_remove> */

/*
* tasklist_idle_act
* tasklist_idle_agent
* tasklist_idle_cancel
*/
static bool
tasklist_idle_act(Tasklist *tasklist)
{
  tasklist->priv->agent = 0;      /* reentrancy guard */
  vdebug (2, "%s: tasklist => 0x%lx\n", __func__, tasklist);
  tasklist_update_lists (tasklist);
  return false;
} /* </tasklist_idle_act> */

static void
tasklist_idle_agent(Tasklist *tasklist, TasklistEventType event)
{
  TasklistPrivate *context = tasklist->priv;

  if (context->agent == 0) {
    vdebug (2, "%s: tasklist => 0x%lx, event => %s\n",
		__func__, tasklist, tasklist_event_string (event));

    if (event == TASKLIST_UNREALIZE)
      // GLib-CRITICAL Source ID %d was not found when attempting to remove it
      // GLib-GObject-WARNING gsignal.c:2732 instance '0x%lx' has no handler..
      vdebug(1, "%s: tasklist => 0x%lx\n", __func__, tasklist);
    else
      context->agent = g_idle_add ((GSourceFunc)tasklist_idle_act, tasklist);
  }
} /* </tasklist_idle_agent> */

static void
tasklist_idle_cancel(Tasklist *tasklist)
{
  TasklistPrivate *context = tasklist->priv;

  if (context->agent != 0) {
    vdebug (2, "%s: tasklist => 0x%lx\n", __func__, tasklist);
    g_source_remove (context->agent);
    context->agent = 0;
  }
} /* </tasklist_idle_cancel> */

/*
* tasklist_queue_update_lists
* tasklist_construct_visible_list
* tasklist_update_active_list
* tasklist_update_lists
*/
static inline void
tasklist_queue_update_lists(Tasklist *tasklist, TasklistEventType event)
{
  vdebug(2, "%s: event => %s\n", __func__, tasklist_event_string (event));
  tasklist_idle_agent (tasklist, event);
} /* </tasklist_queue_update_lists> */

static void
tasklist_construct_visible_list(Tasklist *tasklist, int desktop)
{
  GList *iter, *list;
  TasklistItem *item;
  TasklistPrivate *context = tasklist->priv;
  int space;

  if (context->visible) {  /* always start with empty visible list */
    g_list_free (context->visible);
    context->visible = NULL;
  }

  for (iter = context->ungrouped; iter != NULL; iter = iter->next) {
    item  = (TasklistItem *)iter->data;
    space = green_window_get_desktop (item->window);

    if (context->include_all_workspaces == FALSE &&
			space >= 0 && space != desktop)
      continue;

    if (context->grouping == TASKLIST_NEVER_GROUP) /* simple case */
      context->visible = g_list_append (context->visible, item);
    else {
      GreenWindow *window = item->window;
      list = green_get_group_list (context->green, window);

      if (list && g_list_length (list) > 1) {
        bool append = true;
        GList *member, *part;
        TasklistItem *scan;
        Window xid;

        for (member = list; member != NULL; member = member->next) {
          xid = green_window_get_xid (member->data); //DEBUG
          scan = g_hash_table_lookup (context->winhash, GUINT_TO_POINTER(xid));
          gtk_widget_hide (scan->button); //DEBUG

          if (append) /* check presence of scan item in visible list */
            for (part = context->visible; part != NULL; part = part->next)
              if (((TasklistItem *)part->data)->window == scan->window) {
                append = false;
                break;
              }
        }

        if (append) {
          //DEBUG item = tasklist_item_new (window, TASK_CLASS_GROUP, tasklist);
          context->visible = g_list_append (context->visible, item);
        }
      }
      else  /* only one client in the resource class group */
        context->visible = g_list_append (context->visible, item);
    }
  }
} /* </tasklist_construct_visible_list> */

static void
tasklist_update_active_list(Tasklist *tasklist, int workspace)
{
  TasklistPrivate *context = tasklist->priv;
  GtkWidget *widget = GTK_WIDGET (tasklist);
  Window active = get_active_window (green_get_root_window (context->green));
  GList *iter;

  if (context->include_all_workspaces)
    vdebug (3, "Tasklist::update_active_list (all workspaces)\n");
  else
    vdebug (3, "Tasklist::update_active_list (workspace %d)\n", workspace);

  for (iter = context->visible; iter != NULL; iter = iter->next) {
    TasklistItem *item = iter->data;
    Window xid = green_window_get_xid (item->window);

    if (GTK_IS_TOGGLE_BUTTON (item->button) == FALSE)
      continue;

    if (WindowValidate (xid)) {
      if (tasklist_item_is_active (item, active)) {
        item->button_toggle = TRUE;
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(item->button), TRUE);
        vdebug (3, "\t0x%x ACTIVE\n", xid);
      }
      else {
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(item->button), FALSE);
        vdebug (3, "\t0x%x\n", xid);
      }
    }
    else {
      vdebug (3, "\t0x%x BadWindow\n", xid);
      continue;
    }

    if (!gtk_widget_get_parent (item->button)) {
      gtk_widget_set_parent (item->button, widget);
      gtk_widget_show (item->button);
    }
  }
} /* </tasklist_update_active_list> */

static void
tasklist_update_lists(Tasklist *tasklist)
{
  TasklistPrivate *context = tasklist->priv;
  int space, workspace = green_get_active_workspace (context->green);
  int idx, count = green_get_workspace_count (context->green);

  GList *iter, *list;
  GreenWindow *window;
  TasklistItem *item;
  Window xid;

  /* Iterate though the windows list creating all new items. */
  list = green_get_windows (context->green, WindowTaskbarFilter, -1);

  for (idx = 0; idx < count; idx++) {
    int mark = 1;
    vdebug(3, "Tasklist::update_lists: workspace => %d\n", idx);

    for (iter = list; iter != NULL; iter = iter->next) {
      window = iter->data;
      xid = green_window_get_xid (window);

      if (!g_hash_table_lookup (context->winhash, GUINT_TO_POINTER(xid))) {
        space = green_window_get_desktop (window);

        if (space == -1 || space == idx) {
          vdebug(3, "Tasklist::GreenWindow %d xid => 0x%x\n", mark++, xid);
          item = tasklist_item_new (window, TASK_WINDOW, tasklist);
          context->ungrouped = g_list_append (context->ungrouped, item);
          g_hash_table_insert (context->winhash, GUINT_TO_POINTER(xid), item);
        }
      }
    }
  }

  /* Rebuild visible list of tasklist items. */
  tasklist_construct_visible_list (tasklist, workspace);

  /* Iterate visible list for the active window. */
  tasklist_update_active_list (tasklist, workspace);
} /* </tasklist_update_lists> */

/*
* tasklist_active_window_changed
* tasklist_active_workspace_changed
*/
static void
tasklist_active_window_changed (Green *green, Tasklist *tasklist)
{
  vdebug (2, "Tasklist::update: event => active-window-changed\n");
  tasklist_update_active_list (tasklist, green_get_active_workspace (green));
} /* </tasklist_active_window_changed> */

static void
tasklist_active_workspace_changed (Green *green, Tasklist *tasklist)
{
  int workspace = green_get_active_workspace (green);
  vdebug (2, "Tasklist::update: event => active-workspace-changed\n");

  tasklist_construct_visible_list (tasklist, workspace);
  tasklist_update_active_list (tasklist, workspace);

  gtk_widget_queue_resize (GTK_WIDGET (tasklist));
} /* </tasklist_active_workspace_changed> */

/*
* tasklist_window_opened
* tasklist_window_closed
*/
static void
tasklist_window_opened(Green *green, GreenWindow* window, Tasklist *tasklist)
{
  Window xwindow = green_window_get_xid (window);
  vdebug(2, "%s: WINDOW_OPENED, xid => 0x%lx\n", __func__, xwindow);
  tasklist_queue_update_lists (tasklist, TASKLIST_WINDOW_ADDED);
  gtk_widget_queue_resize (GTK_WIDGET (tasklist));
} /* </tasklist_window_opened> */

static void
tasklist_window_closed(Green *green, GreenWindow *window, Tasklist *tasklist)
{
  int workspace = green_get_active_workspace (green);
  TasklistItem *item = tasklist_item_lookup (window, tasklist);
  Window xwindow = green_window_get_xid (window);

  if (xwindow == 0)  { // DEBUG why are we being called
    gould_diagnostics ("%s %s: %s\n", timestamp(), Program, __func__);
    vdebug(1, "%s %s::%s: tasklist => 0x%lx, item => 0x%lx, xid => 0x0\n",
		timestamp(), Program, __func__, tasklist, item);
    return;
  }
  // Handle both "delete-event" and "destroy" together.
  vdebug(2, "%s: WINDOW_CLOSED, xid => 0x%lx\n", __func__, xwindow);

  tasklist_disconnect_window (window);
  green_remove_window (green, xwindow);
  if(item) tasklist_item_free (item, tasklist); // not being tracked ex. gpanel

  tasklist_construct_visible_list (tasklist, workspace);
  tasklist_queue_update_lists (tasklist, TASKLIST_WINDOW_REMOVED);
  gtk_widget_queue_resize (GTK_WIDGET (tasklist));
} /* </tasklist_window_closed> */

/*
* tasklist_window_icon_changed
* tasklist_window_name_changed
* tasklist_window_state_changed
* tasklist_window_workspace_changed
*/
static void
tasklist_window_icon_changed(GreenWindow *window, Tasklist *tasklist)
{
  Window xid = green_window_get_xid (window);
  TasklistItem *item = g_hash_table_lookup (tasklist->priv->winhash,
						GUINT_TO_POINTER(xid));
  if (item != NULL) {
    tasklist_item_update (item, TASKLIST_WINDOW_ICON_CHANGED);
  }
} /* </tasklist_window_icon_changed> */

static void
tasklist_window_name_changed(GreenWindow *window, Tasklist *tasklist)
{
  Window xid = green_window_get_xid (window);
  TasklistItem *item = g_hash_table_lookup (tasklist->priv->winhash,
						GUINT_TO_POINTER(xid));
  if (item != NULL) {
    tasklist_item_update (item, TASKLIST_WINDOW_NAME_CHANGED);
  }
} /* </tasklist_window_name_changed> */

static void
tasklist_window_state_changed (GreenWindow *window, Tasklist *tasklist)
{
  Window xid = green_window_get_xid (window);
  TasklistItem *item = g_hash_table_lookup (tasklist->priv->winhash,
						GUINT_TO_POINTER(xid));
  if (item != NULL) {
    /* window remains visible, update state */
    tasklist_item_update (item, TASKLIST_WINDOW_STATE_CHANGED);
  }
} /* </tasklist_window_state_changed> */

static void
tasklist_window_workspace_changed(GreenWindow *window, Tasklist *tasklist)
{
  TasklistPrivate *context = tasklist->priv;
  int workspace = green_get_active_workspace (context->green);

  if (workspace != green_window_get_desktop (window))
    for (GList *iter = context->ungrouped; iter != NULL; iter = iter->next)
      if (((TasklistItem *)iter->data)->window == window) {
        tasklist_active_workspace_changed (context->green, tasklist);
        break;
      }
} /* </tasklist_window_workspace_changed> */

/*
* tasklist_viewports_changed
*/
static void
tasklist_viewports_changed(Green *green, Tasklist *tasklist)
{
  tasklist_queue_update_lists (tasklist, TASKLIST_VIEWPORTS_CHANGED);
  gtk_widget_queue_resize (GTK_WIDGET (tasklist));
} /* </tasklist_viewports_changed> */

/*
* tasklist_connect_screen
* tasklist_disconnect_screen
*/
static void
tasklist_connect_screen(Tasklist *tasklist, Green *green)
{
  guint *conn = tasklist->priv->connection;
  unsigned short idx = 0;

  vdebug(2, "%s %s: tasklist => 0x%lx, green => 0x%lx\n",
		timestamp(), __func__, tasklist, green);

  conn[idx++] = g_signal_connect_object (
                 G_OBJECT (green), "active-window-changed",
                 G_CALLBACK (tasklist_active_window_changed),
                 tasklist, 0);

  conn[idx++] = g_signal_connect_object (
                  G_OBJECT (green), "active-workspace-changed",
                  G_CALLBACK (tasklist_active_workspace_changed),
                  tasklist, 0);

  conn[idx++] = g_signal_connect_object (
                  G_OBJECT (green), "window-opened",
                  G_CALLBACK (tasklist_window_opened),
                  tasklist, 0);

  conn[idx++] = g_signal_connect_object (
                 G_OBJECT (green), "window-closed",
                 G_CALLBACK (tasklist_window_closed),
                 tasklist, 0);

  conn[idx++] = g_signal_connect_object (
                 G_OBJECT (green), "viewports-changed",
                 G_CALLBACK (tasklist_viewports_changed),
                 tasklist, 0);
  
  g_assert (idx == N_SCREEN_CONNECTIONS); /* sanity check */
} /* </tasklist_connect_screen> */

static void
tasklist_disconnect_screen(Tasklist *tasklist)
{
  TasklistPrivate *context = tasklist->priv;
  guint *conn = context->connection;

  vdebug(2, "%s %s: tasklist => 0x%lx, green => 0x%lx\n",
		timestamp(), __func__, tasklist, context->green);

  for (int idx = 0; idx < N_SCREEN_CONNECTIONS; idx++)
    if (conn[idx] > 0) {
      g_signal_handler_disconnect (G_OBJECT (context->green), conn[idx]);
      conn[idx] = 0;
    }
} /* </tasklist_disconnect_screen> */

/*
* tasklist_connect_window
* tasklist_disconnect_window
*/
static void
tasklist_connect_window(GreenWindow *window, Tasklist *tasklist)
{
  guint *conn = window->connection;
  unsigned short idx = 0;

  vdebug(2, "%s: xid => 0x%lx\n", __func__, green_window_get_xid (window));

  conn[idx++] = g_signal_connect_object (G_OBJECT (window), "icon_changed",
                           G_CALLBACK (tasklist_window_icon_changed),
                           tasklist, 0);

  conn[idx++] = g_signal_connect_object (G_OBJECT (window), "name_changed",
                           G_CALLBACK (tasklist_window_name_changed),
                           tasklist, 0);

  conn[idx++] = g_signal_connect_object (window, "state_changed",
                           G_CALLBACK (tasklist_window_state_changed),
                           tasklist, 0);

  conn[idx++] = g_signal_connect_object (window, "workspace_changed",
                           G_CALLBACK (tasklist_window_workspace_changed),
                           tasklist, 0);

  g_assert (idx == N_WINDOW_CONNECTIONS); /* sanity check */
} /* </tasklist_connect_window> */

static void
tasklist_disconnect_window(GreenWindow *window)
{
  guint *conn = window->connection;
  vdebug(2, "%s: xid => 0x%lx\n", __func__, green_window_get_xid (window));

  for (int idx = 0; idx < N_WINDOW_CONNECTIONS; idx++)
    if (conn[idx] > 0) {
      g_signal_handler_disconnect (G_OBJECT (window), conn[idx]);
      conn[idx] = 0;
    }
} /* </tasklist_disconnect_window> */

/*
* tasklist_remove_startup_sequences
*/
static void
tasklist_remove_startup_sequences(Tasklist *tasklist, GreenWindow *window)
{
  /* nothing */
} /* </tasklist_remove_startup_sequences> */

/*
* Tasklist and TasklistClass implementation.
*/
static void
tasklist_init(Tasklist *tasklist)
{
  GtkWidget *widget = GTK_WIDGET (tasklist);
  TasklistPrivate *context = g_new0 (TasklistPrivate, 1);

  vdebug(2, "%s %s: tasklist => 0x%lx\n", timestamp(), __func__, tasklist);
  GTK_WIDGET_SET_FLAGS (widget, GTK_NO_WINDOW);
  
  context->grouping = TASKLIST_NEVER_GROUP;

  context->include_all_workspaces = FALSE;

  context->orientation = GTK_ORIENTATION_HORIZONTAL;

  context->relief = GTK_RELIEF_NORMAL;

  context->min_button_width  = DEFAULT_BUTTON_WIDTH;
  context->min_button_height = MIN_ICON_SIZE;

  context->minimum_width  = DEFAULT_WIDTH;
  context->minimum_height = DEFAULT_HEIGHT;

  context->tooltips = gtk_tooltips_new ();
  context->winhash  = g_hash_table_new (NULL, NULL);
  
  tasklist->priv = context;
} /* </tasklist_init> */

static void
tasklist_finalize(GObject *object)
{
  Tasklist *tasklist = GREEN_TASKLIST (object);
  TasklistPrivate *context = tasklist->priv;

  tasklist_idle_cancel (tasklist);	  /* inert g_idle_add() */
  tasklist_cleanup (tasklist);

  if (context->visible) {
    g_list_free (context->visible);
    context->visible = NULL;
  }

  if (context->ungrouped) {
    g_list_free (context->ungrouped);
    context->ungrouped = NULL;
  }

  g_hash_table_destroy (context->winhash);
  context->winhash = NULL;

  g_free (tasklist->priv);
  tasklist->priv = NULL;  
  
  G_OBJECT_CLASS (parent_class_)->finalize (object);
} /* </tasklist_finalize> */

static void
tasklist_class_init(TasklistClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS (klass);
  guint32 flags = G_PARAM_READABLE |
                  G_PARAM_STATIC_NAME |
                  G_PARAM_STATIC_NICK |
                  G_PARAM_STATIC_BLURB;
  
  parent_class_ = g_type_class_peek_parent (klass);

  object_class->finalize = tasklist_finalize;

  widget_class->realize       = tasklist_widget_realize;
  widget_class->unrealize     = tasklist_widget_unrealize;
  widget_class->size_request  = tasklist_widget_size_request;
  widget_class->size_allocate = tasklist_widget_size_allocate;
  widget_class->expose_event  = tasklist_widget_expose;

  container_class->forall     = tasklist_container_forall;
  container_class->remove     = tasklist_container_remove;

  gtk_widget_class_install_style_property (widget_class,
      g_param_spec_float ("fade-loop-time", "Loop time", "One loop fading.",
                          0.2, 10.0, 3.0, flags));

  gtk_widget_class_install_style_property (widget_class,
      g_param_spec_boolean ("fade-overlay-rect", "Overlay rect", "Themes.",
                            TRUE, flags));

  gtk_widget_class_install_style_property (widget_class,
      g_param_spec_float ("fade-opacity", "Final opacity", "Default: 0.8",
                          0.0, 1.0, 0.8, flags));
} /* </tasklist_class_init> */

/**
* Public methods
*/
GType
tasklist_get_type(void)
{
  static GType object_type = 0;
  g_type_init ();			/* initialize the type system */
  
  if (!object_type) {
    const GTypeInfo object_info =
    {
      sizeof (TasklistClass),
      (GBaseInitFunc) NULL,		/* base_init */
      (GBaseFinalizeFunc) NULL,		/* base_finalize */
      (GClassInitFunc) tasklist_class_init,
      NULL,           			/* class_finalize */
      NULL,           			/* class_data */
      sizeof (Tasklist),
      0,              			/* n_preallocs */
      (GInstanceInitFunc) tasklist_init,
      NULL            			/* value_table */
    };
      
    object_type = g_type_register_static (GTK_TYPE_CONTAINER, "Tasklist",
                                          &object_info, 0);
  }
  return object_type;
} /* </tasklist_get_type> */

/*
* tasklist_set_button_relief - govern buttons relief
* tasklist_set_grouping - govern window class resource grouping
* tasklist_set_include_all_workspaces - govern including all workspaces
* tasklist_set_orientation - govern orientation
*/
void
tasklist_set_button_relief(Tasklist *tasklist, GtkReliefStyle relief)
{
  g_return_if_fail (IS_GREEN_TASKLIST (tasklist));
  TasklistPrivate *context = tasklist->priv;

  if (context->relief != relief) {
    for (GList *iter = context->ungrouped; iter; iter = iter->next) {
      TasklistItem *item = iter->data;
      gtk_button_set_relief (GTK_BUTTON (item->button), relief);
    }
    context->relief = relief;	/* save the setting */
  }
} /* </tasklist_set_button_relief> */

void
tasklist_set_grouping(Tasklist *tasklist, TasklistGroupingType grouping)
{
  g_return_if_fail (IS_GREEN_TASKLIST (tasklist));
  TasklistPrivate *context = tasklist->priv;

  if (context->grouping != grouping) {
    context->grouping = grouping;
    tasklist_queue_update_lists (tasklist, TASKLIST_SET_GROUPING);
    gtk_widget_queue_resize (GTK_WIDGET (tasklist));
  }
} /* </tasklist_set_grouping> */

void
tasklist_set_include_all_workspaces(Tasklist *tasklist, bool include_all)
{
  g_return_if_fail (IS_GREEN_TASKLIST (tasklist));
  TasklistPrivate *context = tasklist->priv;

  if (context->include_all_workspaces != include_all) {
    context->include_all_workspaces = include_all;
    tasklist_queue_update_lists (tasklist, TASKLIST_SET_INCLUDE_ALL_WORKSPACES);
    gtk_widget_queue_resize (GTK_WIDGET (tasklist));
  }
} /* </tasklist_set_include_all_workspaces> */

void
tasklist_set_orientation(Tasklist *tasklist, GtkOrientation orientation)
{
  g_return_if_fail (IS_GREEN_TASKLIST (tasklist));
  TasklistPrivate *context = tasklist->priv;

  if (context->orientation != orientation) {
    context->orientation = orientation;
    gtk_widget_queue_resize (GTK_WIDGET (tasklist));
  }
} /* </tasklist_set_orientation> */

/*
 * tasklist_new - constructor and activator
 */
Tasklist *
tasklist_new(Green *green)
{
  Tasklist *tasklist = g_object_new (GREEN_TYPE_TASKLIST, NULL);
  TasklistPrivate *context = tasklist->priv;

  gtk_object_ref (GTK_OBJECT (context->tooltips));
  gtk_object_sink (GTK_OBJECT (context->tooltips));

  context->green = green;			// complete initialization
  vdebug(2, "%s %s: tasklist => 0x%lx\n", timestamp(), __func__, tasklist);
#ifdef CONSIDER
  tasklist_connect_screen (tasklist, green);	// connect to GREEN instance

  /* callback when there is a scroll-event for switching to the next window  */
  g_signal_connect_object (G_OBJECT (tasklist),
                           "scroll-event",
                           G_CALLBACK (tasklist_scroll_cb),
                           G_OBJECT (tasklist),
                           0);
#endif
  return tasklist;
} /* </tasklist_new> */

