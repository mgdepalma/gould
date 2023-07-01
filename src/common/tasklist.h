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

#ifndef TASKLIST_H
#define TASKLIST_H

#include "greenwindow.h"

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GREEN_TYPE_TASKLIST            (tasklist_get_type ())
#define GREEN_TASKLIST(object)         (G_TYPE_CHECK_INSTANCE_CAST ((object), \
                                        GREEN_TYPE_TASKLIST, Tasklist))

#define IS_GREEN_TASKLIST(object)      (G_TYPE_CHECK_INSTANCE_TYPE ((object), \
                                        GREEN_TYPE_TASKLIST))

#define GREEN_TASKLIST_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), \
                                        GREEN_TYPE_TASKLIST, TasklistClass))

#define IS_GREEN_TASKLIST_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), \
                                        GREEN_TYPE_TASKLIST))

#define GREEN_TASKLIST_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
                                        GREEN_TYPE_TASKLIST, TasklistClass))

typedef struct _Tasklist        Tasklist;
typedef struct _TasklistClass   TasklistClass;
typedef struct _TasklistPrivate TasklistPrivate;

struct _Tasklist
{
  GtkContainer instance;	/* parent instance */
  TasklistPrivate *priv;	/* private data */
};

struct _TasklistClass
{
  GtkContainerClass parent;	/* parent class */
};

typedef enum {
  TASKLIST_REALIZE,			// realize
  TASKLIST_ACTIVE_WINDOW_CHANGED,	// active-window-changed
  TASKLIST_ACTIVE_WORKSPACE_CHANGED,	// active-workspace-changed
  TASKLIST_SET_GROUPING,		// set-grouping
  TASKLIST_SET_INCLUDE_ALL_WORKSPACES,	// set-include-all-workspaces"
  TASKLIST_VIEWPORTS_CHANGED,		// viewports-changed
  TASKLIST_WINDOW_ADDED,		// window-added
  TASKLIST_WINDOW_ICON_CHANGED,		// window-icon-changed
  TASKLIST_WINDOW_NAME_CHANGED,		// window-name-changed
  TASKLIST_WINDOW_STATE_CHANGED,	// window-state-changed
  TASKLIST_WINDOW_WORKSPACE_CHANGED,	// window-workspace-changed
  TASKLIST_WINDOW_REMOVED,		// window-removed
  TASKLIST_UNREALIZE			// unrealize
} TasklistEventType;

typedef enum {
  TASKLIST_NEVER_GROUP,
  TASKLIST_ALWAYS_GROUP,
  TASKLIST_AUTO_GROUP
} TasklistGroupingType;

GType tasklist_get_type (void) G_GNUC_CONST;

void tasklist_set_button_relief (Tasklist *tasklist, GtkReliefStyle relief);
void tasklist_set_grouping (Tasklist *tasklist, TasklistGroupingType grouping);
void tasklist_set_orientation (Tasklist *tasklist, GtkOrientation orien);
void tasklist_set_include_all_workspaces (Tasklist *tasklist, bool all);

Tasklist *tasklist_new (Green *screen);

G_END_DECLS

#endif /* TASKLIST_H */

