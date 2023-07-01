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
#include "gould.h"
#include "gwindow.h"
#include "pager.h"

#include <gdk-pixbuf/gdk-pixbuf.h>
//#include <gdk-pixbuf-xlib/gdk-pixbuf-xlib.h>
#include <gdk/gdkx.h>

extern unsigned short debug;	/* must be declared in main program */

#define DEFAULT_CELL_SIZE    48
#define DEFAULT_ICON_SIZE    32
#define N_SCREEN_CONNECTIONS 9

struct _PagerPrivate {
  Green *green;			/* GREEN instance */

  PagerDisplayMode mode;	/* display contents or names */
  unsigned short n_rows;	/* columns for vertical orientation */
  GtkOrientation orientation;
  GtkShadowType  shadow;

  GdkPixbuf *backdrop;		/* background pixbuf */

  guint connection[N_SCREEN_CONNECTIONS];

  GreenWindow *drag_window;
  gboolean drag_prelight;	/* flag drag-n-drop in progress */
  gboolean drag_active;

  guint drag_agent;		/* GSource that triggers switching workspace */
  guint drag_time;		/* time of last event during drag-n-drop */

  int drag_space;		/* workspace mouse is hovering over */
  int drag_start_x;
  int drag_start_y;

  int cell_size;		/* drawn workspace cell size */
  int token;			/* workspace manager token */
};

static gpointer parent_class_;	/* parent class instance */

/* Prototype forward declarations. */
static void pager_connect_window (GreenWindow *object, Pager *pager);

static void pager_drag_clear (Pager *pager);

static void pager_draw_workspace (Pager *pager, int desktop,
                                  GdkRectangle *rect, GdkPixbuf *pixbuf);

static GdkPixbuf *pager_get_background (Pager *pager, int width, int height);

static GdkRectangle *pager_get_workspace_rectangle (Pager *pager, int space);
static GdkRectangle *pager_get_window_rectangle (Window xid,
                                                 const GdkRectangle *base,
                                                 GdkScreen *gdkscr);

static void pager_set_layout_hint (Pager *pager);

/*
 * pager_point_in_rectangle
 * pager_workspace_at_point
 */
static inline gboolean
pager_point_in_rectangle (const GdkRectangle *rect, int xpos, int ypos)
{
  return (xpos >= rect->x && xpos < rect->x + rect->width &&
          ypos >= rect->y && ypos < rect->y + rect->height) ? TRUE : FALSE;
} /* </pager_point_in_rectangle> */

static int
pager_workspace_at_point (Pager *pager, int xpos, int ypos)
{
  PagerPrivate *priv = pager->priv;
  GtkWidget *widget = GTK_WIDGET (pager);
  int n_spaces = green_get_workspace_count (priv->green);
  int xthickness;
  int ythickness;
  int adjust;
  int idx;

  gtk_widget_style_get (GTK_WIDGET (pager),
			"focus-line-width", &adjust,
			NULL);

  if (priv->shadow != GTK_SHADOW_NONE) {
      xthickness = adjust + widget->style->xthickness;
      ythickness = adjust + widget->style->ythickness;
  }
  else {
    xthickness = adjust;
    ythickness = adjust;
  }

  for (idx = 0; idx < n_spaces; idx++) {
    GdkRectangle *rect = pager_get_workspace_rectangle (pager, idx);

    /* If workspace is on the edge, pretend points on the frame belong
     * to the workspace. Otherwise, pretend the right/bottom line separating
     * two workspaces belong to the workspace.
     */
    if (rect->x == xthickness) {
      rect->x = 0;
      rect->width += xthickness;
    }

    if (rect->y == ythickness) {
      rect->y = 0;
      rect->height += ythickness;
    }

    if (rect->y + rect->height == widget->allocation.height - ythickness)
      rect->height += ythickness;
    else
      rect->height += 1;

    if (rect->x + rect->width == widget->allocation.width - xthickness)
      rect->width += xthickness;
    else
      rect->width += 1;

    if (pager_point_in_rectangle (rect, xpos, ypos))
      return idx;
  }

  return -1;
} /* </pager_workspace_at_point> */

/*
 * pager_queue_draw_workspace
 * pager_queue_draw_window
 */
static void
pager_queue_draw_workspace (Pager *pager, int workspace)
{
  GdkRectangle *rect = pager_get_workspace_rectangle (pager, workspace);

  gtk_widget_queue_draw_area (GTK_WIDGET (pager),
                              rect->x, rect->y,
                              rect->width, rect->height);
} /* </pager_queue_draw_workspace> */

static void
pager_queue_draw_window (GreenWindow *window, Pager *pager)
{
  int workspace = green_window_get_desktop (window);
  pager_queue_draw_workspace (pager, workspace);
} /* </pager_queue_draw_window> */

/*
 * pager_check_prelight
 */
static void
pager_check_prelight (Pager *pager, gint x, gint y, gboolean prelight)
{
  PagerPrivate *priv = pager->priv;
  gint space = (x < 0 || y < 0) ? -1 : pager_workspace_at_point (pager, x, y);;

  if (priv->drag_space != space) {
    pager_queue_draw_workspace (pager, priv->drag_space);
    pager_queue_draw_workspace (pager, space);
    priv->drag_prelight = prelight;
    priv->drag_space = space;
  }
  else if (priv->drag_prelight != prelight) {
    pager_queue_draw_workspace (pager, priv->drag_space);
    priv->drag_prelight = prelight;
  }
} /* </pager_check_prelight> */

/*
 * pager_realize
 * pager_unrealize
 */
static void
pager_realize (GtkWidget *widget)
{
  GdkWindowAttr attributes;
  Pager *pager = GREEN_PAGER (widget);
  gint mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;

  GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);

  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.x = widget->allocation.x;
  attributes.y = widget->allocation.y;
  attributes.width = widget->allocation.width;
  attributes.height = widget->allocation.height;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.visual = gtk_widget_get_visual (widget);
  attributes.colormap = gtk_widget_get_colormap (widget);
  attributes.event_mask = gtk_widget_get_events (widget) | GDK_EXPOSURE_MASK |
                          GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
                          GDK_LEAVE_NOTIFY_MASK | GDK_POINTER_MOTION_MASK |
                          GDK_POINTER_MOTION_HINT_MASK;

  widget->window = gdk_window_new (gtk_widget_get_parent_window (widget),
                                   &attributes, mask);

  gdk_window_set_user_data (widget->window, widget);
  widget->style = gtk_style_attach (widget->style, widget->window);
  gtk_style_set_background (widget->style, widget->window, GTK_STATE_NORMAL);

  pager_set_layout_hint (pager);
} /* </pager_realize> */

static void
pager_unrealize (GtkWidget *widget)
{
  Pager *pager = GREEN_PAGER (widget);
  PagerPrivate *priv = pager->priv;

  pager_drag_clear (pager);
  priv->drag_space = -1;

  green_release_workspace_layout (priv->green, priv->token);

  GTK_WIDGET_CLASS (parent_class_)->unrealize (widget);
} /* </pager_unrealize> */

/*
 * pager_size_adjust_text
 */
static int
pager_size_adjust_text (Pager *pager)
{
  Green *green = pager->priv->green;
  PangoLayout *layout;

  int n_spaces = green_get_workspace_count (green);
  int idx, size, width;

  layout = gtk_widget_create_pango_layout (GTK_WIDGET (pager), NULL);
  size = 1;

  for (idx = 0; idx < n_spaces; idx++) {
    pango_layout_set_text (layout,
                           green_get_workspace_name (green, idx),
                           -1);

    pango_layout_get_pixel_size (layout, &width, NULL);
    size = MAX (size, width);
  }
	  
  g_object_unref (layout);
  size += 2;

  return size;
} /* </pager_size_adjust_text> */

/*
 * pager_size_request
 * pager_size_allocate
 */
static void
pager_size_request (GtkWidget *widget, GtkRequisition *requisition)
{
  Pager *pager = GREEN_PAGER (widget);
  PagerPrivate *priv = pager->priv;
  GdkScreen *gdkscr = green_get_gdk_screen (priv->green);
  int n_spaces = green_get_workspace_count (priv->green);

  double screen_aspect;

  int adjust;
  int n_rows;
  int split;			/* spaces per row */
  int hsize;
  int vsize;

  g_assert (priv->n_rows > 0);	/* sanity check */

  n_rows = priv->n_rows;
  hsize  = priv->cell_size;
  split  = (n_spaces + n_rows - 1) / n_rows;
  
  if (priv->orientation == GTK_ORIENTATION_VERTICAL) {
    screen_aspect = (double)gdk_screen_get_height (gdkscr) / 
                    (double)gdk_screen_get_width (gdkscr);

    if (priv->mode == PAGER_DISPLAY_NAME)
      vsize = pager_size_adjust_text (pager);
    else
      vsize = screen_aspect * hsize;

    requisition->width  = hsize * n_rows + (n_rows - 1);
    requisition->height = vsize * split + (split - 1);
  }
  else {
    screen_aspect = (double)gdk_screen_get_width (gdkscr) /
                    (double)gdk_screen_get_height (gdkscr);

    if (priv->mode == PAGER_DISPLAY_NAME)
      vsize = pager_size_adjust_text (pager);
    else
      vsize = screen_aspect * hsize;
      
    requisition->width  = vsize * split + (split - 1);
    requisition->height = hsize * n_rows + (n_rows - 1);
  }

  if (priv->shadow != GTK_SHADOW_NONE) {
    requisition->width  += 2 * widget->style->xthickness;
    requisition->height += 2 * widget->style->ythickness;
  }

  gtk_widget_style_get (widget,
			"focus-line-width", &adjust,
			NULL);

  requisition->width  += 2 * adjust;
  requisition->height += 2 * adjust;
} /* </pager_size_request> */

static void
pager_size_allocate (GtkWidget *widget, GtkAllocation *allocation)
{
  Pager *pager = GREEN_PAGER (widget);
  PagerPrivate *priv = pager->priv;

  int cell_size;
  int adjust;
  int height;
  int width;

  g_assert (priv->n_rows > 0);		/* sanity check */

  gtk_widget_style_get (widget,
			"focus-line-width", &adjust,
			NULL);

  width  = allocation->width  - 2 * adjust;
  height = allocation->height - 2 * adjust;

  if (priv->shadow != GTK_SHADOW_NONE) {
    width  -= 2 * widget->style->xthickness;
    height -= 2 * widget->style->ythickness;
  }

  g_assert (width  > 0);		/* sanity check */
  g_assert (height > 0);		/* sanity check */

  if (priv->orientation == GTK_ORIENTATION_VERTICAL)
    cell_size = (width - (priv->n_rows - 1))  / priv->n_rows;
  else
    cell_size = (height - (priv->n_rows - 1)) / priv->n_rows;

  if (cell_size == priv->cell_size)
    GTK_WIDGET_CLASS (parent_class_)->size_allocate (widget, allocation);
  else {
    priv->cell_size = cell_size;
    gtk_widget_queue_resize (widget);
  }

} /* </pager_size_allocate> */

/*
 * pager_expose_event - costly updates
 */
static gboolean
pager_expose_event (GtkWidget *widget, GdkEventExpose *event)
{
  GdkPixbuf *pixbuf = NULL;
  GdkRectangle *area, intersect;
  gboolean once = TRUE;

  Pager *pager = GREEN_PAGER (widget);
  PagerPrivate *priv = pager->priv;

  int n_spaces = green_get_workspace_count (priv->green);
  int adjust, idx;


  gtk_widget_style_get (widget,
                        "focus-line-width", &adjust,
                        NULL);

  if (GTK_WIDGET_HAS_FOCUS (widget))
    gtk_paint_focus (widget->style,
                     widget->window,
                     GTK_WIDGET_STATE (widget),
                     NULL,
                     widget,
                     "pager",
                     0, 0,
                     widget->allocation.width,
                     widget->allocation.height);

  if (priv->shadow != GTK_SHADOW_NONE)
    gtk_paint_shadow (widget->style,
                      widget->window,
                      GTK_WIDGET_STATE (widget),
                      priv->shadow,
                      NULL,
                      widget,
                      "pager",
                      adjust,
                      adjust,
                      widget->allocation.width - 2 * adjust,
                      widget->allocation.height - 2 * adjust);

  for (idx = 0; idx < n_spaces; idx++) {
    area = pager_get_workspace_rectangle (pager, idx);

    if (once && priv->mode == PAGER_DISPLAY_CONTENT) {
      pixbuf = pager_get_background (pager, area->width, area->height);
      once = FALSE;
    }

    if (gdk_rectangle_intersect (&event->area, area, &intersect))
      pager_draw_workspace (pager, idx, area, pixbuf);
  }

  return FALSE;
} /* </pager_expose_event> */

/*
 * pager_button_press
 * pager_button_release
 */
static gboolean
pager_button_press (GtkWidget *widget, GdkEventButton *event)
{
  Pager *pager = GREEN_PAGER (widget);
  PagerPrivate *priv = pager->priv;
  int space;

  if (event->button != 1)	/* only act on left mouse button */
    return FALSE;

  if ((space = pager_workspace_at_point (pager, event->x, event->y)) >= 0) {
    Green *green = priv->green;
    GdkScreen *gdkscr = green_get_gdk_screen (green);
    GdkRectangle *rect = pager_get_workspace_rectangle (pager, space);
    GList *iter, *list = green_get_windows (priv->green,
                                            WindowPagerFilter,
                                            space);

    /* save the start coordinates so we can know if we need to change
     * workspace when the button is released */
    priv->drag_start_x = event->x;
    priv->drag_start_y = event->y;

    for (iter = list; iter != NULL; iter = iter->next) {
      Window xid = green_window_get_xid (iter->data);
      GdkRectangle *winrect = pager_get_window_rectangle (xid, rect, gdkscr);

      if (pager_point_in_rectangle (winrect, event->x, event->y)) {
        priv->drag_window = iter->data;
        break;
      }
    }

    g_list_free (list);
  }

  return TRUE;
} /* </pager_button_press> */

static gboolean
pager_button_release (GtkWidget *widget, GdkEventButton *event)
{
  Pager *pager = GREEN_PAGER (widget);
  PagerPrivate *priv = pager->priv;

  if (event->button != 1)	/* only act on left mouse button */
    return FALSE;

  if (!priv->drag_active) {
    int desktop = green_get_active_workspace (priv->green);
    int space = pager_workspace_at_point (pager, event->x, event->y);
    int drag = pager_workspace_at_point (pager, priv->drag_start_x,
                                                priv->drag_start_y);

    if (space == drag && space >= 0 && space != desktop)
      green_change_workspace (priv->green, space, event->time);

    pager_drag_clear (pager);
  }

  return FALSE;
} /* </pager_button_release> */

/*
 * pager_motion_notify
 * pager_leave_notify
 */
static gboolean
pager_motion_notify (GtkWidget *widget, GdkEventMotion *event)
{
  Pager *pager = GREEN_PAGER (widget);
  PagerPrivate *priv = pager->priv;
  int x, y;

  gdk_window_get_pointer (widget->window, &x, &y, NULL);

  if (!priv->drag_active &&
      priv->drag_window != NULL &&
      gtk_drag_check_threshold (widget,
                                pager->priv->drag_start_x,
                                pager->priv->drag_start_y,
                                x, y)) {
    GdkDragContext *context;

    context = gtk_drag_begin (widget,
                              gtk_drag_dest_get_target_list (widget),
                              GDK_ACTION_MOVE,
                              1, (GdkEvent *)event);
    priv->drag_prelight = TRUE;
    priv->drag_active = TRUE;
  }
    
  pager_check_prelight (pager, x, y, priv->drag_prelight);

  return TRUE;
} /* </pager_motion_notify> */

static gboolean
pager_leave_notify (GtkWidget *widget, GdkEventCrossing *event)
{
  Pager *pager = GREEN_PAGER (widget);

  pager_check_prelight (pager, -1, -1, FALSE);

  return FALSE;
} /* </pager_leave_notify> */

/*
 * pager_drag_data_get
 * pager_drag_data_received
 * pager_drag_drop
 * pager_drag_leave
 * pager_drag_motion_timeout
 * pager_drag_motion
 * pager_drag_end
 */
static void
pager_drag_data_get (GtkWidget        *widget,
                     GdkDragContext   *context,
                     GtkSelectionData *situs,
                     guint             info,
                     guint             time)
{
  Pager *pager = GREEN_PAGER (widget);

  if (pager->priv->drag_window) {
    gulong xid = green_window_get_xid (pager->priv->drag_window);

    gtk_selection_data_set (situs, situs->target,
                            8, (guchar *)&xid, sizeof (gulong));
  }
} /* </pager_drag_data_get> */

static void
pager_drag_data_received (GtkWidget         *widget,
                          GdkDragContext    *context,
                          gint               x,
                          gint               y,
                          GtkSelectionData  *situs,
                          guint              info,
                          guint              time)
{
  Pager *pager = GREEN_PAGER (widget);
  int space = pager_workspace_at_point (pager, x, y);
  gboolean success = FALSE;

  if (space >= 0 && (situs->length == sizeof(gulong)) && (situs->format == 8)) {
    GList *iter, *list = green_get_windows (pager->priv->green,
                                            WindowPagerFilter,
                                            -1);
    gulong xid = *((gulong *)situs->data);
   
    for (iter = list; iter != NULL; iter = iter->next) {
      if (xid == green_window_get_xid (iter->data)) {
        GreenWindow *window = green_find_window (xid);

        if (window) {			/* should always work */
          green_window_change_workspace (window, space);

          if (space == green_get_active_workspace (pager->priv->green))
            green_window_activate (window, time);
        }

        success = TRUE;
        break;
      }
    }

    g_list_free (list);
  }

  gtk_drag_finish (context, success, FALSE, time);
} /* </pager_drag_data_received> */

static gboolean
pager_drag_drop (GtkWidget      *widget,
                 GdkDragContext *context,
                 gint            x,
                 gint            y,
                 guint           time)
{
  Pager *pager = GREEN_PAGER (widget);
  GdkAtom target = gtk_drag_dest_find_target (widget, context, NULL);

  if (target != GDK_NONE)
    gtk_drag_get_data (widget, context, target, time);
  else
    gtk_drag_finish (context, FALSE, FALSE, time);

  pager_drag_clear (pager);
  pager_check_prelight (pager, x, y, FALSE);

  return TRUE;
} /* </pager_drag_drop> */

static void
pager_drag_leave (GtkWidget *widget, GdkDragContext *context, guint time)
{
  Pager *pager = GREEN_PAGER (widget);
  PagerPrivate *priv = pager->priv;

  if (priv->drag_agent != 0) {
    g_source_remove (priv->drag_agent);
    priv->drag_agent = 0;
  }

  priv->drag_time = 0;
  pager_check_prelight (pager, -1, -1, FALSE);
} /* </pager_drag_leave> */

static gboolean
pager_drag_motion_timeout (Pager *pager)
{
  PagerPrivate *priv = pager->priv;
  int workspace = green_get_active_workspace (priv->green);

  priv->drag_agent = 0;		/* cancel drag activation */
  
  if (priv->drag_space != workspace)
    green_change_workspace (priv->green, priv->drag_space, priv->drag_time);

  return FALSE;
} /* </pager_drag_motion_timeout> */

static gboolean
pager_drag_motion (GtkWidget          *widget,
                   GdkDragContext     *context,
                   gint                x,
                   gint                y,
                   guint               time)
{
  Pager *pager = GREEN_PAGER (widget);
  PagerPrivate *priv = pager->priv;
  int warp = priv->drag_space;		/* original workspace */

  pager_check_prelight (pager, x, y, TRUE);

  if (gtk_drag_dest_find_target (widget, context, NULL))
    gdk_drag_status (context, context->suggested_action, time);
  else {
    gdk_drag_status (context, 0, time);

    if (priv->drag_space != warp && priv->drag_agent != 0) {
      /* remove timeout, the window we hover over changed */
      g_source_remove (priv->drag_agent);
      priv->drag_agent = 0;
      priv->drag_time = 0;
    }

    if (priv->drag_agent == 0 && priv->drag_space > -1) {
      priv->drag_agent = g_timeout_add (GREEN_ACTIVATE_TIMEOUT,
                                        (GSourceFunc)pager_drag_motion_timeout,
                                        pager);
      priv->drag_time = time;
    }
  }

  return (priv->drag_space != -1) ? TRUE : FALSE;
} /* </pager_drag_motion> */

static void
pager_drag_end (GtkWidget *widget, GdkDragContext*context)
{
  pager_drag_clear (GREEN_PAGER (widget));
} /* </pager_drag_end> */

/*
 * pager_focus
 */
static gboolean
pager_focus (GtkWidget *widget, GtkDirectionType direction)
{
  return GTK_WIDGET_CLASS (parent_class_)->focus (widget, direction);
} /* </pager_focus> */

/*
 * pager_draw_window
 * pager_draw_workspace
 */
static void
pager_draw_window (Window              xid,
                   GtkStateType        state,
                   const GdkRectangle *area,
                   gboolean            active,
                   gboolean            opaque,
                   Pager              *pager)
{
  int icon_x, icon_y, icon_w, icon_h;
  gdouble transparency = (opaque) ? 0.4 : 1.0;

  GtkWidget *widget = GTK_WIDGET (pager);
  GdkDrawable *drawable = widget->window;
  GdkColor *color;
  GdkPixbuf *icon;
  cairo_t *cr;

  cr = gdk_cairo_create (drawable);
  cairo_rectangle (cr, area->x, area->y, area->width, area->height);
  cairo_clip (cr);

  if (active)
    color = &widget->style->light[state];
  else
    color = &widget->style->bg[state];

  cairo_set_source_rgba (cr,
                         color->red / 65535.,
                         color->green / 65535.,
                         color->blue / 65535.,
                         transparency);
  cairo_rectangle (cr,
                   area->x + 1, area->y + 1, 
                   MAX (0, area->width - 2), MAX (0, area->height - 2));
  cairo_fill (cr);

  icon = get_window_icon (xid);
  icon_w = icon_h = 0;

  if (icon) {
    icon_w = gdk_pixbuf_get_width (icon);
    icon_h = gdk_pixbuf_get_height (icon);

    if (icon_w > area->width - 2 || icon_h > area->height - 2) {
      GdkPixbuf *smaller = gdk_pixbuf_scale_simple (icon,
                                                    DEFAULT_ICON_SIZE,
                                                    DEFAULT_ICON_SIZE,
                                                    GDK_INTERP_BILINEAR);
      g_object_unref (icon);
      icon_w = icon_h = DEFAULT_ICON_SIZE;
      icon = smaller;
    }

    icon_x = area->x + (area->width - icon_w) / 2;
    icon_y = area->y + (area->height - icon_h) / 2;

    cairo_save (cr);
    gdk_cairo_set_source_pixbuf (cr, icon, icon_x, icon_y);
    cairo_rectangle (cr, icon_x, icon_y, icon_w, icon_h);
    cairo_clip (cr);
    cairo_paint_with_alpha (cr, transparency);
    cairo_restore (cr);
  }

  if (active)		/* we want to do something else in the future */
    color = &widget->style->fg[state];
  else
    color = &widget->style->fg[state];

  cairo_set_source_rgba (cr,
                         color->red / 65535.,
                         color->green / 65535.,
                         color->blue / 65535.,
                         transparency);

  cairo_set_line_width (cr, 1.0);
  cairo_rectangle (cr,
                   area->x + 0.5, area->y + 0.5,
                   MAX (0, area->width - 1), MAX (0, area->height - 1));
  cairo_stroke (cr);

  cairo_destroy (cr);
} /* </pager_draw_window> */

static void
pager_draw_workspace (Pager *pager, int workspace,
                      GdkRectangle *bound, GdkPixbuf *backdrop)
{
  PagerPrivate *priv = pager->priv;
  GtkWidget *widget = GTK_WIDGET (pager);
  GdkScreen *gdkscr = green_get_gdk_screen (priv->green);
  GtkStateType state;

  int xresolution = gdk_screen_get_width (gdkscr);
  int yresolution = gdk_screen_get_height (gdkscr);

  int xoffset = 1;
  int yoffset = 1;
 
  int desktop = green_get_active_workspace (priv->green);
  gboolean current = (desktop >= 0) && desktop == workspace;
  Window active = get_active_window (green_get_root_window (priv->green));
  cairo_t *cr;

  if (current)
    state = GTK_STATE_SELECTED;
  else if (priv->drag_space == workspace)
    state = GTK_STATE_PRELIGHT;
  else
    state = GTK_STATE_NORMAL;

  /* FIXME in names mode, should probably draw things like a button. */
  if (backdrop)
    gdk_pixbuf_render_to_drawable (backdrop,
                                   widget->window,
                                   widget->style->dark_gc[state],
                                   0, 0,
                                   bound->x, bound->y,
                                   -1, -1,
                                   GDK_RGB_DITHER_MAX,
                                   0, 0);
  else {
    double vx, vy, vw, vh;		/* viewport */
    double hscale, vscale;

    hscale = bound->width / (double)xresolution;
    vscale = bound->height / (double)yresolution;
    vx = bound->x + hscale * xoffset;
    vy = bound->y + vscale * yoffset;
    vw = hscale * xresolution;
    vh = vscale * yresolution;

    cr = gdk_cairo_create (widget->window);
    gdk_cairo_set_source_color (cr, &widget->style->dark[state]);
    cairo_rectangle (cr, vx, vy, vw, vh);
    cairo_fill (cr);
    cairo_destroy (cr);
  }

  if (priv->mode == PAGER_DISPLAY_CONTENT) {      
    GList *iter, *list = green_get_windows_stacking (priv->green,
                                                     WindowPagerFilter,
                                                     workspace);

    Window xdrag = green_window_get_xid (priv->drag_window);

    for (iter = list; iter != NULL; iter = iter->next) {
      Window xid = green_window_get_xid (iter->data);
      XWindowState xws;

      if (get_window_state (xid, &xws) && xws.hidden)	/* iconized */
        continue;
	  
      pager_draw_window (xid,
                         state,
                         pager_get_window_rectangle (xid, bound, gdkscr),
		         (xid == active) ? TRUE : FALSE,
                         (priv->drag_active && xid == xdrag) ? TRUE : FALSE,
                         pager);
    }
      
    g_list_free (list);
  }
  else {			/* PAGER_DISPLAY_NAME */
    const char *workspace_name;
    PangoLayout *layout;
    int w, h;

    workspace_name = green_get_workspace_name (priv->green, workspace);

    layout = gtk_widget_create_pango_layout (widget, workspace_name);
    pango_layout_get_pixel_size (layout, &w, &h);
      
    gdk_draw_layout  (widget->window,
                      current ? widget->style->fg_gc[GTK_STATE_SELECTED]
			      : widget->style->fg_gc[GTK_STATE_NORMAL],
		      bound->x + (bound->width - w) / 2,
		      bound->y + (bound->height - h) / 2,
		      layout);

    g_object_unref (layout);
  }

  if (priv->drag_space == workspace && priv->drag_prelight) {
    /* stolen directly from gtk source so it matches nicely */

    gtk_paint_shadow (widget->style, widget->window,
	              GTK_STATE_NORMAL, GTK_SHADOW_OUT,
		      NULL, widget, "dnd",
		      bound->x, bound->y, bound->width, bound->height);

    cr = gdk_cairo_create (widget->window);
    cairo_set_source_rgb (cr, 0.0, 0.0, 0.0);	/* black */
    cairo_set_line_width (cr, 1.0);
    cairo_rectangle (cr,
		     bound->x + 0.5, bound->y + 0.5,
		     bound->width - 1, bound->height - 1);
    cairo_stroke (cr);
    cairo_destroy (cr);
  }
} /* </pager_draw_workspace> */

/*
 * pager_get_background
 */
GdkPixbuf *
pager_get_background (Pager *pager, int width, int height)
{
  PagerPrivate *priv = pager->priv;
  GdkPixbuf *pixbuf = NULL;
  GdkPixmap *pixmap;
  Pixmap backdrop;

  /* Careful alternating between width/height values, serious performance. */
  if (priv->backdrop &&
      gdk_pixbuf_get_width (priv->backdrop) == width &&
      gdk_pixbuf_get_height (priv->backdrop) == height)
    return priv->backdrop;

  if (priv->backdrop) {
    g_object_unref (G_OBJECT (priv->backdrop));
    priv->backdrop = NULL;
  }

  if ((backdrop = green_get_background_pixmap (priv->green)) != None) {
    pixmap = gdk_pixmap_foreign_new (backdrop);
    pixbuf = gdk_pixbuf_get_from_drawable (NULL, pixmap,
                                           colormap_from_pixmap (pixmap),
                                           0, 0, 0, 0, -1, -1);
    if (pixbuf) {
      priv->backdrop = gdk_pixbuf_scale_simple (pixbuf, width, height,
                                                GDK_INTERP_BILINEAR);
      g_object_unref (pixbuf);
    }
  }
  return priv->backdrop;
} /* </pager_get_background> */

/*
 * pager_get_workspace_rectangle
 * pager_get_window_rectangle
 */
static GdkRectangle *
pager_get_workspace_rectangle (Pager *pager, int space)
{
  static GdkRectangle area;
  GdkRectangle *rect = &area;

  GtkWidget *widget = GTK_WIDGET (pager);
  PagerPrivate *priv = pager->priv;

  int adjust;
  int count;
  int hsize;
  int vsize;
  int split;		/* spaces per row */
  int xpos;
  int ypos;

  gtk_widget_style_get (widget, "focus-line-width", &adjust, NULL);

  count = green_get_workspace_count (priv->green);
  hsize = widget->allocation.width - 2 * adjust;
  vsize = widget->allocation.height - 2 * adjust;
  split = (count + priv->n_rows - 1) / priv->n_rows;

  if (priv->shadow != GTK_SHADOW_NONE) {
    hsize -= 2 * widget->style->xthickness;
    vsize -= 2 * widget->style->ythickness;
  }

  if (priv->orientation == GTK_ORIENTATION_VERTICAL) {
    rect->width  = (hsize - (priv->n_rows - 1)) / priv->n_rows;
    rect->height = (vsize - (split - 1)) / split;

    xpos = space / split;
    ypos = space % split;

    if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL)
      xpos = priv->n_rows - xpos - 1;

    rect->x = (rect->width + 1) * xpos;
    rect->y = (rect->height + 1) * ypos;

    if (xpos == priv->n_rows - 1)
      rect->width = hsize - rect->x;

    if (ypos == split - 1)
      rect->height = vsize - rect->y;
  }
  else {
    rect->width  = (hsize - (split - 1)) / split;
    rect->height = (vsize - (priv->n_rows - 1)) / priv->n_rows;

    xpos = space % split;
    ypos = space / split;

    if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL)
      xpos = split - xpos - 1;

    rect->x = (rect->width + 1) * xpos;
    rect->y = (rect->height + 1) * ypos;

    if (xpos == split - 1)
      rect->width = hsize - rect->x;

    if (ypos == priv->n_rows - 1)
      rect->height = vsize - rect->y;
  }

  rect->x += adjust;
  rect->y += adjust;

  if (priv->shadow != GTK_SHADOW_NONE) {
    rect->x += widget->style->xthickness;
    rect->y += widget->style->ythickness;
  }

  return rect;
} /* </pager_get_workspace_rectangle> */

static GdkRectangle *
pager_get_window_rectangle (Window xid,
                            const GdkRectangle *base,
                            GdkScreen *gdkscr)
{
  static GdkRectangle area;
  GdkRectangle clip, *rect = &area;
  double hscale, vscale;

  /* scale window down by same ratio we scaled workspace */
  hscale = (double)base->width / (double)gdk_screen_get_width (gdkscr);
  vscale = (double)base->height / (double)gdk_screen_get_height (gdkscr);

  get_window_geometry (xid, &clip);

  clip.x = base->x + clip.x * hscale + 0.5;
  clip.y = base->y + clip.y * vscale + 0.5;

  clip.width  = clip.width * hscale + 0.5;
  clip.height = clip.height * vscale + 0.5;

  if (clip.width < 3)
    clip.width = 3;

  if (clip.height < 3)
    clip.height = 3;

  gdk_rectangle_intersect ((GdkRectangle *)base, &clip, rect);

  return rect;
} /* </pager_get_window_rectangle> */

/*
 * pager_drag_clear
 */
static void
pager_drag_clear (Pager *pager)
{
  PagerPrivate *priv = pager->priv;

  if (priv->drag_active)
    pager_queue_draw_window (priv->drag_window, pager);

  priv->drag_active = FALSE;
  priv->drag_window = NULL;
  priv->drag_start_x = -1;
  priv->drag_start_y = -1;
} /* </pager_drag_clear> */

/*
 * pager_active_window_changed
 * pager_active_workspace_changed
 * pager_background_changed
 * pager_window_opened
 * pager_window_closed
 * pager_window_stacking_changed
 * pager_workspace_created
 * pager_workspace_destroyed
 * pager_viewports_changed
 */
static void
pager_active_window_changed (Green *green, Pager *pager)
{
  gtk_widget_queue_draw (GTK_WIDGET (pager));
} /* </pager_active_window_changed> */

static void
pager_active_workspace_changed (Green *green, Pager *pager)
{
  gtk_widget_queue_draw (GTK_WIDGET (pager));
} /* </pager_active_workspace_changed> */

static void
pager_background_changed (Green *green, Pager *pager)
{
  PagerPrivate *priv = pager->priv;

  if (priv->backdrop) {
    g_object_unref (priv->backdrop);
    priv->backdrop = NULL;
  }

  gtk_widget_queue_draw (GTK_WIDGET (pager));
} /* </pager_background_changed> */

static void
pager_window_opened (Green *green, GreenWindow *window, Pager *pager)
{
  pager_connect_window (window, pager);
  pager_queue_draw_window (window, pager);
} /* </pager_window_opened> */

static void
pager_window_closed (Green *green, GreenWindow *window, Pager *pager)
{
  if (pager->priv->drag_window == window)
    pager_drag_clear (pager);

  pager_queue_draw_window (window, pager);
} /* </pager_window_closed> */

static void
pager_window_stacking_changed (Green *green, Pager *pager)
{
  gtk_widget_queue_draw (GTK_WIDGET (pager));
} /* </pager_window_stacking_changed> */

static void
pager_workspace_name_changed (Green *green, Pager *pager)
{
  gtk_widget_queue_resize (GTK_WIDGET (pager));
} /* </pager_workspace_name_changed> */

static void
pager_workspace_created (Green *green, int workspace, Pager *pager)
{
  g_signal_connect (G_OBJECT (pager), "name-changed",
                    G_CALLBACK (pager_workspace_name_changed),
                    GUINT_TO_POINTER (workspace));

  gtk_widget_queue_resize (GTK_WIDGET (pager));
} /* </pager_workspace_created> */

static void
pager_workspace_destroyed (Green *green, int workspace, Pager *pager)
{
  g_signal_handlers_disconnect_by_func (G_OBJECT (pager),
                             G_CALLBACK (pager_workspace_name_changed),
                                        GUINT_TO_POINTER (workspace));

  gtk_widget_queue_resize (GTK_WIDGET (pager));
} /* </pager_workspace_destroyed> */

static void
pager_viewports_changed (Green *green, Pager *pager)
{
  gtk_widget_queue_resize (GTK_WIDGET (pager));
} /* </pager_viewports_changed> */

/*
 * pager_window_icon_changed
 * pager_window_name_changed
 *
 */
static void
pager_window_icon_changed (GreenWindow *window, Pager *pager)
{
  pager_queue_draw_window (window, pager);
} /* </pager_window_icon_changed> */

static void
pager_window_name_changed (GreenWindow *window, Pager *pager)
{
  pager_queue_draw_window (window, pager);
} /* </pager_window_name_changed> */

static void
pager_window_state_changed (GreenWindow *window, Pager *pager)
{
  pager_queue_draw_window (window, pager);
} /* </pager_window_state_changed> */

static void
pager_window_geometry_changed (GreenWindow *window, Pager *pager)
{
  pager_queue_draw_window (window, pager);
} /* </pager_window_geometry_changed> */

static void
pager_window_workspace_changed (GreenWindow *window, Pager *pager)
{
  gtk_widget_queue_draw (GTK_WIDGET (pager));
} /* </pager_window_workspace_changed> */

/*
 * pager_set_layout_hint
 */
static void
pager_set_layout_hint (Pager *pager)
{
  PagerPrivate *priv = pager->priv;
  int cols, rows;

  if (priv->orientation == GTK_ORIENTATION_HORIZONTAL) {
    cols = 0;
    rows = priv->n_rows;
  }
  else {
    cols = priv->n_rows;
    rows = 0;
  }
  
  priv->token = green_request_workspace_layout (priv->green,
                                                priv->token,
                                                rows, cols);
} /* </pager_set_layout_hint> */

/*
 * pager_connect_screen
 * pager_connect_window
 * pager_disconnect_screen
 */
static void
pager_connect_screen (Pager *pager, Green *green)
{
  GList *windows = green_get_windows (green, WindowPagerFilter, -1);
  guint *conn = pager->priv->connection;
  unsigned short idx = 0;

  conn[idx++] = g_signal_connect_object (
                 G_OBJECT (green), "active-window-changed",
                 G_CALLBACK (pager_active_window_changed),
                 pager, 0);

  conn[idx++] = g_signal_connect_object (
                  G_OBJECT (green), "active-workspace-changed",
                  G_CALLBACK (pager_active_workspace_changed),
                  pager, 0);

  conn[idx++] = g_signal_connect_object (
                  G_OBJECT (green), "background-changed",
                  G_CALLBACK (pager_background_changed),
                  pager, 0);

  conn[idx++] = g_signal_connect_object (
                  G_OBJECT (green), "window-opened",
                  G_CALLBACK (pager_window_opened),
                  pager, 0);

  conn[idx++] = g_signal_connect_object (
                 G_OBJECT (green), "window-closed",
                 G_CALLBACK (pager_window_closed),
                 pager, 0);

  conn[idx++] = g_signal_connect_object (
                  G_OBJECT (green), "window-stacking-changed",
                  G_CALLBACK (pager_window_stacking_changed),
                  pager, 0);

  conn[idx++] = g_signal_connect_object (
                  G_OBJECT (green), "workspace-created",
                  G_CALLBACK (pager_workspace_created),
                  pager, 0);

  conn[idx++] = g_signal_connect_object (
                  G_OBJECT (green), "workspace-destroyed",
                  G_CALLBACK (pager_workspace_destroyed),
                  pager, 0);

  conn[idx++] = g_signal_connect_object (
                 G_OBJECT (green), "viewports-changed",
                 G_CALLBACK (pager_viewports_changed),
                 pager, 0);

  g_assert (idx == N_SCREEN_CONNECTIONS);	/* sanity check */

#ifdef HANDLE_WORKSPACE_CHANGES
  for (idx = 0; idx < green_get_workspace_count (green); idx++) {
    g_signal_connect (G_OBJECT (pager), "name_changed",
                      G_CALLBACK (pager_workspace_name_changed),
                      GUINT_TO_POINTER (idx));
  }
#endif

  /* Setup signal handlers for each GreenWindow object. */
  g_list_foreach (windows, (GFunc)pager_connect_window, pager);
  g_list_free (windows);
} /* </pager_connect_screen> */

static void
pager_connect_window (GreenWindow *window, Pager *pager)
{
  g_signal_connect_object (G_OBJECT (window), "icon_changed",
                           G_CALLBACK (pager_window_icon_changed),
                           pager, 0);

  g_signal_connect_object (G_OBJECT (window), "name_changed",
                           G_CALLBACK (pager_window_name_changed),
                           pager, 0);

  g_signal_connect_object (G_OBJECT (window), "state_changed",
                           G_CALLBACK (pager_window_state_changed),
                           pager, 0);

  g_signal_connect_object (G_OBJECT (window), "geometry_changed",
                           G_CALLBACK (pager_window_geometry_changed),
                           pager, 0);

  g_signal_connect_object (G_OBJECT (window), "workspace_changed",
                           G_CALLBACK (pager_window_workspace_changed),
                           pager, 0);
} /* </pager_connect_window> */

static void
pager_disconnect_screen (Pager *pager)
{
  guint *conn = pager->priv->connection;
  unsigned short idx = 0;

  for (idx = 0; idx < N_SCREEN_CONNECTIONS; idx++) {
    g_signal_handler_disconnect (G_OBJECT (pager->priv->green), conn[idx]);
    conn[idx] = 0;
  }

#ifdef HANDLE_WORKSPACE_CHANGES
  unsigned short count = green_get_workspace_count (pager->priv->green);

  for (idx = 0; idx < count; idx++)
    g_signal_handlers_disconnect_by_func (G_OBJECT (pager),
                               G_CALLBACK (pager_workspace_name_changed),
                                          GUINT_TO_POINTER (idx));
#endif
} /* </pager_disconnect_screen> */

/**
 * Pager and PagerClass construction
 */
static void
pager_init (Pager *pager)
{
  static const GtkTargetEntry target[] = {
    { "application/x-gould-window-id", 0, 0}
  };

  GtkWidget *widget = GTK_WIDGET (pager);
  PagerPrivate *priv = g_new0 (PagerPrivate, 1);

  gtk_drag_dest_set (widget, 0, target, G_N_ELEMENTS(target), GDK_ACTION_MOVE);
  GTK_WIDGET_SET_FLAGS (widget, GTK_CAN_FOCUS);

  priv->mode        = PAGER_DISPLAY_CONTENT;
  priv->orientation = GTK_ORIENTATION_HORIZONTAL;
  priv->shadow      = GTK_SHADOW_NONE;
  priv->n_rows      = 1;

  priv->token       = GREEN_NO_TOKEN;
  priv->cell_size   = DEFAULT_CELL_SIZE;
  priv->drag_space  = -1;

  pager->priv = priv;
} /* </pager_init> */

static void
pager_finalize (GObject *object)
{
  Pager *pager = GREEN_PAGER (object);
  PagerPrivate *priv = pager->priv;

  pager_disconnect_screen (pager);	/* disconnect from GREEN instance */

  if (priv->backdrop) {
    g_object_unref (priv->backdrop);
    priv->backdrop = NULL;
  }

  if (priv->drag_agent != 0) {
    g_source_remove (priv->drag_agent);
    priv->drag_agent = 0;
  }

  g_free (pager->priv);
  pager->priv = NULL;  
  
  G_OBJECT_CLASS (parent_class_)->finalize (object);
} /* </pager_finalize> */

static void
pager_class_init (PagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  
  parent_class_ = g_type_class_peek_parent (klass);
  object_class->finalize = pager_finalize;

  widget_class->realize = pager_realize;
  widget_class->unrealize = pager_unrealize;
  widget_class->size_request = pager_size_request;
  widget_class->size_allocate = pager_size_allocate;
  widget_class->expose_event = pager_expose_event;
  widget_class->button_press_event = pager_button_press;
  widget_class->button_release_event = pager_button_release;
  widget_class->motion_notify_event = pager_motion_notify;
  widget_class->leave_notify_event = pager_leave_notify;
  widget_class->drag_data_get = pager_drag_data_get;
  widget_class->drag_data_received = pager_drag_data_received;
  widget_class->drag_drop = pager_drag_drop;
  widget_class->drag_leave = pager_drag_leave;
  widget_class->drag_motion = pager_drag_motion;
  widget_class->drag_end = pager_drag_end;
  widget_class->focus = pager_focus;
} /* </pager_class_init> */

/**
 * Public methods
 */
GType
pager_get_type (void)
{
  static GType object_type = 0;
  g_type_init ();			/* initialize the type system */

  if (!object_type) {
    const GTypeInfo object_info =
    {
      sizeof (PagerClass),
      (GBaseInitFunc) NULL,		/* base_init */
      (GBaseFinalizeFunc) NULL,		/* base_finalize */
      (GClassInitFunc) pager_class_init,
      NULL,           			/* class_finalize */
      NULL,           			/* class_data */
      sizeof (Pager),
      0,              			/* n_preallocs */
      (GInstanceInitFunc) pager_init,
      NULL            			/* value_table */
    };
      
    object_type = g_type_register_static (GTK_TYPE_WIDGET, "Pager",
                                          &object_info, 0);
  }
  
  return object_type;
} /* </pager_get_type> */

/*
 * pager_set_display_mode
 * pager_set_n_rows
 * pager_set_orientation
 * pager_set_shadow_type
 */
void
pager_set_display_mode (Pager *pager, PagerDisplayMode mode)
{
  g_return_if_fail (IS_GREEN_PAGER (pager));

  if (pager->priv->mode != mode) {
    pager->priv->mode = mode;
    gtk_widget_queue_resize (GTK_WIDGET (pager));
  }
} /* </pager_set_display_mode> */

void
pager_set_n_rows (Pager *pager, int n_rows)
{
  g_return_if_fail (IS_GREEN_PAGER (pager));
  g_return_if_fail (n_rows > 0);

  if (pager->priv->n_rows != n_rows) {
    pager->priv->n_rows = n_rows;
    gtk_widget_queue_resize (GTK_WIDGET (pager));
    pager_set_layout_hint (pager);
  }
} /* </pager_set_n_rows> */

void
pager_set_orientation (Pager *pager, GtkOrientation orientation)
{
  g_return_if_fail (IS_GREEN_PAGER (pager));

  if (pager->priv->orientation != orientation) {
    pager->priv->orientation = orientation;
    gtk_widget_queue_resize (GTK_WIDGET (pager));
    pager_set_layout_hint (pager);
  }
} /* </pager_set_orientation> */

void
pager_set_shadow_type (Pager *pager, GtkShadowType shadow)
{
  g_return_if_fail (IS_GREEN_PAGER (pager));

  if (pager->priv->shadow != shadow) {
    pager->priv->shadow = shadow;
    gtk_widget_queue_resize (GTK_WIDGET (pager));
  }
} /* </pager_set_shadow_type> */

/*
 * pager_new - constructor and activator
 */
Pager *
pager_new (Green *green)
{
  Pager *pager = g_object_new (GREEN_TYPE_PAGER, NULL);

  pager->priv->green = green;		/* complete initialization */
  pager_connect_screen (pager, green);	/* connect to GREEN instance */

  return pager;
} /* </pager_new> */
