/**
 * Copyright (C) Generations Linux <bugs@softcraft.org>
 * All Rights Reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "filechooser.h"

#ifndef get_current_dir_name
extern char *get_current_dir_name(void);
#endif

extern const char *Program;	/* see, gpanel.c and possibly others */
extern unsigned short debug;	/* must be declared in main program */

/*
 * (private) variables
 */
static GtkObjectClass *parent = NULL;

static FileChooserTypes filetypes[] = {
  { ".mp3",	ICON_AUDIO },
  { ".wav",	ICON_AUDIO },
  { ".gif",	ICON_IMAGE },
  { ".jpg",	ICON_IMAGE },
  { ".jpeg",	ICON_IMAGE },
  { ".JPG",	ICON_IMAGE },
  { ".png",	ICON_IMAGE },
  { ".ppm",	ICON_IMAGE },
  { ".tif",	ICON_IMAGE },
  { ".tiff",	ICON_IMAGE },
  { ".xpm",	ICON_IMAGE },
  { ".jar",	ICON_JAVA },
  { ".java",	ICON_JAVA },
  { ".bz2",	ICON_PACKAGE },
  { ".deb",	ICON_PACKAGE },
  { ".rpm",	ICON_PACKAGE },
  { ".tar",	ICON_PACKAGE },
  { ".tar.gz",	ICON_PACKAGE },
  { ".tar.bz2", ICON_PACKAGE },
  { ".tgz",	ICON_PACKAGE },
  { ".zip",	ICON_PACKAGE },
  { ".pdf",	ICON_PDF },
  { ".ps",	ICON_POSTSCRIPT },
  { ".avi",	ICON_VIDEO },
  { ".mov",	ICON_VIDEO },
  { ".mpeg",	ICON_VIDEO },
  { ".mpeg1",	ICON_VIDEO },
  { ".mpeg2",	ICON_VIDEO },
  { ".mpg",	ICON_VIDEO },
  { ".mpg1",	ICON_VIDEO },
  { ".mpg2",	ICON_VIDEO },
  { ".wmv",	ICON_VIDEO },
  { ".",	ICON_ERROR }
};


/*
 * (private) dump_names_list
 */
static void
dump_names_list (void)
{
#ifdef DEBUG
  GList *iter;
  for (iter = self->_names; iter != NULL; iter = iter->next) {
      printf("%s\n", iter->data);
  }
#endif
} /* </dump_names_list> */

/*
 * (private) agent
 */
static void
agent(GtkIconView *iconview, FileChooser *self)
{
  const gchar *curdir;
  gchar *name, pathname[FILENAME_MAX];
  struct stat info;
  gboolean action;

  /* Compose pathname with self->curdir and the selected icon. */
  curdir = gtk_label_get_text (GTK_LABEL(self->path));
  name = iconbox_get_selected (self->iconbox, COLUMN_LABEL);
  if (name) gtk_entry_set_text (GTK_ENTRY(self->name), name);

  if (strcmp(curdir, "/") == 0)
    sprintf(pathname, "/%s", name);
  else
    sprintf(pathname, "%s/%s", curdir, name);

  /* Populate self->iconbox with directory contents. */
  if (lstat(pathname, &info) == 0)
    if (S_ISDIR(info.st_mode) || S_ISLNK(info.st_mode)) {
      action = filechooser_update (self, pathname, self->clearname);
    }

  if (self->agent)  { /* if specified invoke additional agent callback */
    self->datum->file = pathname;
    action = (*self->agent) (self->datum);
  }

  gtk_widget_set_sensitive (GTK_WIDGET(self->dirup), TRUE);
} /* </agent> */

/*
 * (private) dirup
 * (private) dirhome
 */
static void
dirup(GtkWidget *button, FileChooser *self)
{
  const gchar *curdir = gtk_label_get_text (GTK_LABEL(self->path));
  gchar *scan;

  if (strlen(curdir) > 1) {
    if ( (scan = strrchr(curdir, '/')) )
      *scan = (char)0;
    else
      curdir = "/";
  }

  if (strlen(curdir) == 0)
    curdir = "/";

  if (strcmp(curdir, "/") == 0)
    gtk_widget_set_sensitive (GTK_WIDGET(self->dirup), FALSE);

  filechooser_update (self, curdir, self->clearname); /* update directory */
} /* </dirup> */

static void
dirhome(GtkWidget *button, FileChooser *self)
{
  filechooser_update (self, getenv("HOME"), self->clearname);
  gtk_widget_set_sensitive (GTK_WIDGET(self->dirup), TRUE);
} /* </dirhome> */

/*
 * (private) showhidden
 * (private) showthumbs
 */
static void
showhidden(GtkWidget *button, FileChooser *self)
{
  const gchar *curdir = gtk_label_get_text (GTK_LABEL(self->path));

  if (self->showhidden) {
    gtk_button_set_image (GTK_BUTTON (button), xpm_image(ICON_HIDDEN));
    self->showhidden = FALSE;
  }
  else {
    gtk_button_set_image (GTK_BUTTON (button), xpm_image(ICON_VISIBLE));
    self->showhidden = TRUE;
  }

  filechooser_update (self, curdir, FALSE);
} /* </showhidden> */

static void
showthumbs(GtkWidget *button, FileChooser *self)
{
  const gchar *curdir = gtk_label_get_text (GTK_LABEL(self->path));
  GtkWidget *view = self->iconbox->view;

  if (self->showthumbs) {
    gtk_button_set_image (GTK_BUTTON (button), xpm_image(ICON_ICONS));
    gtk_icon_view_set_orientation (view, GTK_ORIENTATION_HORIZONTAL);
    self->showthumbs = FALSE;
  }
  else {
    gtk_button_set_image (GTK_BUTTON (button), xpm_image(ICON_THUMBNAIL));
    gtk_icon_view_set_orientation (view, GTK_ORIENTATION_VERTICAL);
    self->showthumbs = TRUE;
  }

  filechooser_update (self, curdir, FALSE);
} /* </showthumbs> */

/*
 * (private) get_icon_from_type
 */
static IconIndex
get_icon_from_type(const gchar *pathname)
{
  IconIndex index = ICON_FILE;
  struct stat info;

  if (lstat(pathname, &info) == 0) {
    if ( S_ISDIR(info.st_mode) )
      index = ICON_DIRS;
    else if ( S_ISLNK(info.st_mode) )
      index = ICON_SYMLINK;
    else if (info.st_mode & 0111)
      index = ICON_EXEC;
    else {
      gchar *scan = strrchr(pathname, '.');
      if (scan && strlen(scan) > 0) {
        FileChooserTypes *types = filetypes;

        while (types && types->index != ICON_ERROR) {
          if (strcmp(scan, types->pattern) == 0) {
            index = types->index;
            break;
          }
          ++types;
        }
      }
    }
  }
  return index;
} /* </get_icon_from_type> */

/*
 * (private) icon_pixbuf
 */
static GdkPixbuf *
icon_pixbuf(FileChooser *self, IconIndex index, const char *pathname)
{
  GdkPixbuf *pixbuf = NULL;

  if (index == ICON_IMAGE && self->showthumbs) {
    GError    *error = NULL;
    GdkPixbuf *image = gdk_pixbuf_new_from_file (pathname, &error);

    if (image) {
      pixbuf = pixbuf_scale (image, self->thumbsize, self->thumbsize);
      g_object_unref (image);
    }
    else {
      g_printerr("%s: %s\n", Program, error->message);
    }
  }

  if (pixbuf == NULL) {		/* fallback xpm_pixbuf(index, WHITE) */
    GdkColor white = { 0, 65535, 65535, 65535 };
    pixbuf = xpm_pixbuf(index, &white);
  }
  return pixbuf;
} /* </icon_pixbuf> */

/*
 * (private) filechooser_class_init
 */
static void
filechooser_class_init (FileChooserClass *klass)
{
  parent = g_type_class_peek_parent (klass);
}

/*
 * (private) filechooser_init
 */
static void
filechooser_init (FileChooser *self)
{
  char *mode = getenv("THUMBVIEW") ? getenv("THUMBVIEW") : "Tiles";
  int iconsize = getenv("THUMBSIZE") ? atoi(getenv("THUMBSIZE")) : 0;

  self->_names = NULL;		/* must be initialized */
  self->_cursor = -1;
  self->_count = -1;

  self->clearname  = FALSE;	/* do not clear file name */
  self->showhidden = FALSE;	/* do not show hidden files */

  if (strcmp(mode, "Tiles") == 0)
    self->showthumbs = FALSE;	/* do not show thumbnail images */
  else {
    self->showthumbs = TRUE;

    if (iconsize == 0) {	/* not explicitly given */
      if (strcmp(mode, "Small") == 0)
        iconsize = 32;
      else if (strcmp(mode, "Medium") == 0)
        iconsize = 48;
      else if (strcmp(mode, "Large") == 0)
        iconsize = 64;
      else if (strcmp(mode, "Huge") == 0)
        iconsize = 128;
    }
  }
  self->thumbsize = (iconsize > 0) ? iconsize : 48;
} /* </filechooser_init> */

/*
 * filechooser_get_type
 */
GType
filechooser_get_type (void)
{
  static GType type = 0;

  if (type == 0) {
    static const GTypeInfo info =
    {
	sizeof (FileChooserClass),
	NULL,		/* base_init */
        NULL,		/* base_finalize */
	(GClassInitFunc)filechooser_class_init,
	NULL,		/* class_finalize */
	NULL,		/* class_data */
        sizeof (FileChooser),
        0,		/* n_preallocs */
        (GInstanceInitFunc)filechooser_init
    };

    type = g_type_register_static (GTK_TYPE_OBJECT, "FileChooser", &info, 0);
  }
  return type;
} /* </filechooser_get_type> */

/*
 * filechooser_new
 */
FileChooser *
filechooser_new (const gchar *dirname)
{
  GtkWidget *box, *button, *entry, *label;
  FileChooser *self = gtk_type_new (FILECHOOSER_TYPE);
  IconBox *iconbox;
  gchar *path;

  if (dirname == FILECHOOSER_CWD)
    path = (gchar *)get_current_dir_name ();
  else
    path = (gchar *)dirname;

  /* Construct IconBox (builds from GtkIconView) */
  if (self->showthumbs)
    iconbox = self->iconbox = iconbox_new (GTK_ORIENTATION_VERTICAL);
  else
    iconbox = self->iconbox = iconbox_new (GTK_ORIENTATION_HORIZONTAL);

  g_signal_connect (G_OBJECT(iconbox->view), "selection_changed",
					G_CALLBACK(agent), self);
  self->viewer = iconbox->view;

  /* Construct the hbox to display the current file name. */
  box = self->namebox = gtk_hbox_new (FALSE, 2);

  button = self->actuator = xpm_button (ICON_OPEN, NULL);
  gtk_box_pack_start (GTK_BOX(box), button, FALSE, FALSE, 0);
  gtk_button_set_relief (GTK_BUTTON(button), GTK_RELIEF_NONE);
  gtk_widget_show (button);

  /* self->name readonly entry to display file name */
  entry = self->name = gtk_entry_new();
  gtk_box_pack_start (GTK_BOX(box), entry, FALSE, TRUE, 0);
  gtk_widget_show (entry);

  /* Construct the hbox for directory navigation. */
  box = self->dirbox = gtk_hbox_new (FALSE, 0);

  button = self->dirup = xpm_button (ICON_UP, NULL);
  gtk_box_pack_start (GTK_BOX(box), button, FALSE, FALSE, 0);
  gtk_button_set_relief (GTK_BUTTON(button), GTK_RELIEF_NONE);
  g_signal_connect (G_OBJECT(button), "clicked",
                          G_CALLBACK (dirup), self);
  gtk_widget_show (button);

  button = self->home = xpm_button (ICON_HOME, NULL);
  gtk_box_pack_start (GTK_BOX(box), button, FALSE, FALSE, 0);
  gtk_button_set_relief (GTK_BUTTON(button), GTK_RELIEF_NONE);
  g_signal_connect (G_OBJECT (button), "clicked",
                          G_CALLBACK (dirhome), self);
  gtk_widget_show (button);

  /* self->path to display directory name */
  label = self->path = gtk_label_new (path);
  gtk_box_pack_start (GTK_BOX (box), label, FALSE, FALSE, 0);
  gtk_widget_show (label);

  button = xpm_button (ICON_HIDDEN, NULL);
  gtk_box_pack_start (GTK_BOX (box), button, FALSE, FALSE, 0);
  gtk_button_set_relief (GTK_BUTTON(button), GTK_RELIEF_NONE);
  g_signal_connect (G_OBJECT (button), "clicked",
                          G_CALLBACK (showhidden), self);
  gtk_widget_show (button);

  button = xpm_button (self->showthumbs ? ICON_THUMBNAIL : ICON_ICONS, NULL);
  gtk_box_pack_start (GTK_BOX (box), button, FALSE, FALSE, 0);
  gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
  g_signal_connect (G_OBJECT (button), "clicked",
                          G_CALLBACK (showthumbs), self);
  gtk_widget_show(button);

  /* Populate self->iconbox with the directory contents. */
  filechooser_update (self, path, FALSE);

  return self;
} /* </filechooser_new> */

/*
 * filechooser_update
 */
gboolean
filechooser_update (FileChooser *self, const gchar *path, gboolean clearname)
{
  gchar *name;
  gchar pathname[FILENAME_MAX];

  struct dirent **names;
  struct stat info;

  GdkWindow *window = (self->viewer)->window;
  IconIndex index;
  int idx;


  if (lstat(path, &info) != 0)	/* make sure we can read the directory */
    return FALSE;

  if ( S_ISLNK(info.st_mode) ) {
    char symlink[FILENAME_MAX];
    int mark = readlink(path, symlink, FILENAME_MAX);
    if (mark <= 0) return FALSE;

    /* readlink does not append terminating (char)0 */
    symlink[mark] = (char)0;

    if (symlink[0] == '/')
      strcpy(pathname, symlink);
    else {
      const gchar *curdir = gtk_label_get_text (GTK_LABEL(self->path));
      if (strcmp(curdir, "/") == 0)
        sprintf(pathname, "/%s", symlink);
      else
        sprintf(pathname, "%s/%s", curdir, symlink);
    }
  }
  else {
    strcpy(pathname, path);
  }

  /* Set display cursor to busy. */
  if (GDK_IS_WINDOW(window)) {
    gdk_window_set_cursor (window, gdk_cursor_new (GDK_WATCH));
    gdk_display_sync (gtk_widget_get_display (self->viewer));
  }

  /* Clear data structures and free memory allocated. */
  iconbox_clear (self->iconbox);

  g_list_foreach (self->_names, (GFunc)g_free, NULL);
  g_list_free (self->_names);
  self->_names = NULL;

  /* Populate self->icons with directory content. */
  self->_count  = scandir(pathname, &names, NULL, alphasort);
  self->_cursor = 0;

  for (idx = 0; idx < self->_count; idx++) {
    name = names[idx]->d_name;

    if (name[0] == '.' && self->showhidden == FALSE)
      continue;

    if (strcmp(name, ".") && strcmp(name, "..")) {
      /* gtk_entry_set_text (GTK_ENTRY(self->name), name); */
      sprintf(pathname, "%s/%s", path, name);

      if (lstat(pathname, &info) == 0)
        index = get_icon_from_type(pathname);
      else
        /* g_warning("cannot stat: %s", pathname); */
        continue;

      iconbox_append (self->iconbox, icon_pixbuf(self, index, pathname), name);
      self->_names = g_list_append(self->_names, strdup(name));
    }
  }

  /* Update self->path and self->name */
  gtk_label_set_text (GTK_LABEL(self->path), path);
  if (clearname) gtk_entry_set_text (GTK_ENTRY(self->name), "");

  dump_names_list();		/* only with -DDEBUG */
  if (GDK_IS_WINDOW(window))
    gdk_window_set_cursor (window, (self->iconbox)->cursor);

  return TRUE;
} /* </filechooser_update> */

/*
 * filechooser_get_selected_name
 */
gchar *
filechooser_get_selected_name (FileChooser *self)
{
  gchar *selected = NULL;
  static gchar pathname[FILENAME_MAX];

  const gchar *name = gtk_entry_get_text (GTK_ENTRY(self->name));
  const gchar *path = gtk_label_get_text (GTK_LABEL(self->path));

  if (name && strlen(name) > 0) {
    int index = filechooser_get_index (self, name);
    filechooser_set_cursor (self, index);	/* remember selection */

    if (strcmp(path, "/") == 0)  /* sprintf(.., "%s", "/") won't work */
      sprintf(pathname, "/%s", name);
    else
      sprintf(pathname, "%s/%s", path, name);

    selected = pathname;
  }
  return selected;
} /* </filechooser_get_selected_name> */

/*
 * filechooser_get_index
 * filechooser_get_count
 */
int
filechooser_get_index (FileChooser *self, const gchar *name)
{
  GList *iter;
  int scan  = 0;
  int index = -1;

  for (iter = self->_names; iter != NULL; iter = iter->next) {
    if (strcmp (iter->data, name) == 0) {
      index = scan;
      break;
    }
    ++scan;
  }
  return index;
} /* </filechooser_get_index> */

int
filechooser_get_count (FileChooser *self)
{
  return self->_count;
}

/*
 * filechooser_get_cursor
 * filechooser_set_cursor
 */
int
filechooser_get_cursor (FileChooser *self)
{
  return self->_cursor;
}

void
filechooser_set_cursor (FileChooser *self, int index)
{
  GList *iter;
  int scan = 0;

  for (iter = self->_names; iter != NULL; iter = iter->next) {
    if (index == scan++) {
      gtk_entry_set_text (GTK_ENTRY(self->name), iter->data);
      self->_cursor = index;
      break;
    }
  }
  iconbox_unselect (self->iconbox);
} /* </filechooser_set_cursor> */

/*
 * filechooser_set_name
 */
void
filechooser_set_name (FileChooser *self, const gchar *name)
{
  int index = filechooser_get_index (self, name);
  if (index >= 0) filechooser_set_cursor (self, index);
  gtk_entry_set_text (GTK_ENTRY(self->name), name);
} /* </filechooser_set_name> */

/*
 * filechooser_layout
 */
GtkWidget *
filechooser_layout (FileChooser *self)
{
  GtkWidget *area, *box, *layout, *scroll;

  /* Assemble scrollable file chooser. */
  layout = gtk_vbox_new(FALSE, 1);

  /* Assemble box containing navigation and current directory. */
  box = self->dirbox;
  gtk_box_pack_start (GTK_BOX(layout), box, FALSE, FALSE, 2);
  gtk_widget_show(box);

  /* Assemble file selector. */
  scroll = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
                                 GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_box_pack_start (GTK_BOX(layout), scroll, TRUE, TRUE, 5);
  gtk_widget_show(scroll);

  area = self->viewer;
  gtk_container_add (GTK_CONTAINER(scroll), area);
  gtk_widget_show(area);

  box = self->namebox;
  gtk_box_pack_start (GTK_BOX(layout), box, FALSE, FALSE, 0);
  gtk_widget_show (box);

  return layout;
} /* </filechooser_layout> */

/*
 * filechoser_set_callback
 */
void
filechooser_set_callback (FileChooser *self, GtkFunction agent, gpointer data)
{
  FileChooserDatum *datum = g_new0 (FileChooserDatum, 1);

  /* Initialize persistent FileChooserDatum fields. */
  datum->self = self;
  datum->user = data;

  /* Save in the FileChooser instance attributes. */
  self->agent = agent;
  self->datum = datum;
} /* </filechooser_set_callback> */
