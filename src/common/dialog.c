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
#include "dialog.h"

extern const char *Program;	/* published program name identifier */
extern const char *Release;	/* published program name version */

static GtkWidget *MessageDialog = NULL;	/* singleton message dialog */

static struct _dialogue dialogue;	/* store message dialog parts */
static struct _print_dialog printer;	/* store print image dialog */
static struct _save_dialog story;	/* store save file dialog   */

static FileBinding binding[] = {
   { PNG,        "png" }
  ,{ JPEG,       "jpeg" }
  ,{ BMP,        "bmp" }
  ,{ PostScript, "ps" }
};

/*
* Return the file extension based of the given FileType index.
*/
const gchar *
get_file_extension(FileType index)
{
  return binding[index].extension;
} /* </get_file_extension> */

/*
* Add button to GtkDialog widget and optionally set image from an XPM file.
*/
GtkWidget *
add_button(GtkWidget *dialog, IconIndex index,
	      gchar *label, GtkResponseType handle)
{
  GtkWidget *button;
  gchar *padding = g_strdup_printf("%6s", label);
  button = gtk_dialog_add_button (GTK_DIALOG(dialog), padding, handle);
  gtk_button_set_image (GTK_BUTTON(button), xpm_image(index));
  g_free(padding);

  return button;
} /* </add_button> */

/*
* Remove button on a GtkDialog widget.
*/
void
remove_button(GtkWidget *dialog, GtkWidget* button)
{
  GtkWidget *container = GTK_DIALOG(dialog)->action_area;
  gtk_container_remove(GTK_CONTAINER (container), button);
} /* </remove_button> */

/*
* Assemble options menu for file types
*/
GtkWidget *
add_filetypes(GtkWidget **list)
{
  GtkWidget *selections, *label, *options, *menu, *item;

  selections = gtk_hbox_new (FALSE, 10);
  gtk_container_set_border_width (GTK_CONTAINER (selections), 10);
  gtk_widget_show (selections);

  label = xpm_label(ICON_PAINT, NULL, 0, _("File Type"));
  gtk_box_pack_start (GTK_BOX (selections), label, FALSE, FALSE, 0);
  gtk_widget_show (label);

  options = *list = gtk_option_menu_new ();
  menu = gtk_menu_new ();
  
  item = make_menu_item (_("PNG image format"),
                         G_CALLBACK (set_file_type), GINT_TO_POINTER (PNG));
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  
  item = make_menu_item (_("JPEG image format"),
                         G_CALLBACK (set_file_type), GINT_TO_POINTER (JPEG));
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  
  item = make_menu_item (_("BMP image format"),
                         G_CALLBACK (set_file_type), GINT_TO_POINTER (BMP));
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

  gtk_option_menu_set_menu (GTK_OPTION_MENU (options), menu);
  gtk_box_pack_start (GTK_BOX (selections), options, TRUE, TRUE, 0);
  gtk_widget_show (options);

  return selections;
} /* </add_filetypes> */

/*
* callback to set file type
*/
void
set_file_type(GtkWidget *dialog, gpointer value)
{
  static bool program = true;
  FileChooser *chooser = story.chooser;

  story.imgtype = GPOINTER_TO_INT(value);  /* get the file type from 'value' */

  if (program) {                        /* set the default save file name */
    gchar *name = (gchar*)gtk_entry_get_text (GTK_ENTRY(chooser->name));
    gchar savename[FILENAME_MAX];
    gchar *scan;

    strcpy(savename, name);
    scan = g_strrstr(savename, ".");

    if (scan) {
      scan[0] = (gchar)0;  /* chop basename at the start of file extension */

      if (g_str_equal(savename, Program)) {
	sprintf(savename, "%s.%s", Program, get_file_extension(story.imgtype));
        gtk_entry_set_text (GTK_ENTRY(chooser->name), savename);
      }
      else {
        program = false;
      }
    }
  }
  gtk_option_menu_set_history(GTK_OPTION_MENU (story.options), story.imgtype);
} /* </set_file_type> */

/*
* Assemble options menu for paper size
*/
GtkWidget *
add_papersize(PaperSize *list)
{
  GtkWidget *options, *menu, *item;
  GtkWidget *layout = gtk_hbox_new(FALSE, 0);
  int idx;

  item = xpm_image(ICON_PAPER);
  gtk_box_pack_start (GTK_BOX (layout), item, TRUE, TRUE, 0);
  gtk_widget_show (item);

  options = gtk_option_menu_new ();
  menu = gtk_menu_new ();
  idx = 0;

  while (list[idx].name != NULL) {
    item = make_menu_item (list[idx].name,
                           G_CALLBACK (set_papersize), GINT_TO_POINTER (idx));
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
    ++idx;
  }

  gtk_option_menu_set_menu (GTK_OPTION_MENU (options), menu);
  gtk_box_pack_start (GTK_BOX (layout), options, TRUE, TRUE, 0);
  gtk_widget_show (options);
  
  return layout;
} /* </add_papersize> */

/*
* callback to set paper size
*/
void
set_papersize(GtkWidget *widget, gpointer value)
{
  printer.paper = GPOINTER_TO_INT(value);
  if (printer.paper != 0)
    unimplemented(printer.parent, NULL);
} /* </set_papersize> */

/*
* Assemble options menu for available printers
*/
GtkWidget *
add_printers(GList *list)
{
  GtkWidget *layout = gtk_hbox_new(FALSE, 0);
  GtkWidget *options, *menu, *item;
  GList *iter;
  int idx;

  item = xpm_image(ICON_PRINTER);
  gtk_box_pack_start (GTK_BOX (layout), item, FALSE, FALSE, 5);
  gtk_widget_show (item);

  options = gtk_option_menu_new ();
  menu = gtk_menu_new ();
  idx = 0;

  for (iter = list; iter != NULL; iter = iter->next) {
    item = make_menu_item (iter->data,
                           G_CALLBACK(set_papersize), GINT_TO_POINTER (idx));
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
    idx++;
  }

  gtk_option_menu_set_menu (GTK_OPTION_MENU (options), menu);
  gtk_box_pack_start (GTK_BOX (layout), options, TRUE, TRUE, 0);
  gtk_widget_show (options);
  
  return layout;
} /* </add_printers> */

/*
* callback to set the printer
*/
void
set_printer(GtkWidget *widget, gpointer value)
{
  GList *iter;
  GList *printers = get_printers_list();
  int index = GPOINTER_TO_INT(value);
  int mark = 0;

  for (iter = printers; iter != NULL; iter = iter->next) {
    if (mark == index) break;
    ++mark;
  }
} /* </set_printer> */

/*
* add_horizontal_separator convenience function
*/
void
add_horizontal_separator(GtkWidget* layout)
{
  GtkWidget *separator = gtk_hseparator_new ();
  gtk_box_pack_start (GTK_BOX (layout), separator, FALSE, TRUE, 0);
  gtk_widget_show (separator);
}

/*
* add_horizontal_space convenience function
*/
void
add_horizontal_space(GtkWidget* layout, gint space)
{
  GtkWidget *area  = gtk_hbox_new(TRUE, space);
  GtkWidget *label = gtk_label_new (" ");

  gtk_box_pack_start (GTK_BOX (layout), area, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (area), label, TRUE, TRUE, space);

  gtk_widget_show (label);
  gtk_widget_show (area);
} /* </add_horizontal_space> */

/*
* add_vertical_space convenience function
*/
void
add_vertical_space(GtkWidget* layout, gint space)
{
  GtkWidget *area  = gtk_vbox_new(TRUE, space);
  GtkWidget *label = gtk_label_new (" ");

  gtk_box_pack_start (GTK_BOX (layout), area, FALSE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (area), label, TRUE, TRUE, space);

  gtk_widget_show (label);
  gtk_widget_show (area);
} /* </add_vertical_space> */

/*
* make_menu_item convenience function
*/
GtkWidget *
make_menu_item (gchar *name, GCallback callback, gpointer data)
{
  GtkWidget *item;
  
  item = gtk_menu_item_new_with_label (name);
  g_signal_connect (G_OBJECT(item), "activate", callback, data);
  gtk_widget_show (item);

  return item;
} /* </make_menu_item> */

/*
* handler for selecting print mode in {PRINT_DEVICE, PRINT_FILE}
*/
static void
print_device(GtkWidget *button, PrintDialog *print)
{
  GtkWidget *dialog = print->dialog;

  gint width  = print->width;	/* mininal print->dialog size */
  gint height = print->height;


  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(button))) {
    print->device = PRINT_FILE;

    gtk_widget_show (print->filer);
    gtk_entry_set_editable (GTK_ENTRY(print->command), FALSE);

    gtk_widget_set_usize (dialog, width, 3*height);
    gdk_window_resize (dialog->window, width, 3*height);

    gtk_window_set_resizable (GTK_WINDOW(dialog), TRUE);
  }
  else {
    print->device = PRINT_DEVICE;

    gtk_widget_hide (print->filer);
    gtk_entry_set_editable (GTK_ENTRY(print->command), TRUE);

    gtk_widget_set_usize (dialog, width, height);
    gdk_window_resize (dialog->window, width, height);

    gtk_window_set_resizable (GTK_WINDOW(dialog), FALSE);
  }
} /* </print_device> */

/*
* (private) select print orientation in {PRINT_LANDSCAPE, PRINT_PORTRAIT}
*/
static void
print_orientation(GtkWidget *button, gpointer value)
{
  switch (GPOINTER_TO_INT (value)) {
    case PRINT_LANDSCAPE:
      printer.landscape = true;
      unimplemented(printer.parent, _("Landscape"));
      break;

    case PRINT_PORTRAIT:
      printer.landscape = false;
      break;
  }
} /* </print_orientation> */

/*
* (private) create file chooser callback method
*/
static bool
printdialog_callback(GtkWidget *button, PrintDialog *print)
{
  bool status = true;

  if (print->callback)	/* callback can veto status */
    status = (*print->callback) (print);
  else
    unimplemented(print->dialog, NULL);

  if (status) {
    gtk_widget_hide (print->dialog);
  }
  return status;
} /* </printdialog_callback> */

/*
* (private) create print dialog
*/
static PrintDialog *
printdialog_create(GtkWidget *parent, GtkFunction callback)
{
  const gint width  = 400;
  const gint height = 120;

  GtkWidget *dialog = gtk_dialog_new ();
  GtkWidget *layout = gtk_vbox_new (FALSE, 2);  /* print dialog layout */

  GtkWidget *selector = add_printers (get_printers_list ());
  GtkWidget *area, *button, *field, *filer, *frame;

  FileChooser *chooser = filechooser_new (FILECHOOSER_CWD, NULL);
  gchar *savefile;


  /* Customize behavior and look */
  gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);
  gtk_widget_set_usize (dialog, width, height);

  gtk_window_set_title (GTK_WINDOW (dialog), _("Print"));
  gtk_window_set_icon (GTK_WINDOW (dialog), xpm_pixbuf(ICON_PRINT, NULL));
  gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (parent));
  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_MOUSE);
  gtk_window_set_resizable(GTK_WINDOW(dialog), FALSE);
  gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);

  /* Construct print command area */
  frame = gtk_frame_new(NULL);
  gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), frame);
  gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_ETCHED_IN);
  gtk_container_set_border_width(GTK_CONTAINER (frame), 2);
  gtk_widget_show(frame);

  gtk_container_add(GTK_CONTAINER(frame), layout);
  gtk_widget_show(layout);

  /* Add container for available printers */
  area = gtk_hbox_new (FALSE, 5);
  gtk_box_pack_start (GTK_BOX(layout), area, FALSE, FALSE, 0);
  gtk_widget_show (area);

  /* Add available printers */
  gtk_box_pack_start (GTK_BOX(area), selector, FALSE, TRUE, 5);
  gtk_widget_set_usize (selector, 7*width/9, 0);
  gtk_widget_show(selector);

  /* Print command is invisible to the user (for now) */
  field = printer.command = gtk_entry_new();
  gtk_entry_set_text (GTK_ENTRY(field), "lpr");

  /* Add save to file  */
  area = gtk_hbox_new (FALSE, 2);
  gtk_box_pack_start (GTK_BOX(layout), area, FALSE, FALSE, 5);
  gtk_widget_show(area);

  /* Create check box for printing to a file */
  add_horizontal_space (area, width/3);
  button = gtk_check_button_new_with_label(_("Print to file"));
  gtk_box_pack_start (GTK_BOX(area), button, FALSE, TRUE, 0);
  g_signal_connect (G_OBJECT(button), "toggled",
                          G_CALLBACK (print_device), &printer);
  printer.outfile = button;
  gtk_widget_show(button);

  /* Setup save to file chooser */
  filer = filechooser_layout (chooser);
  gtk_box_pack_start (GTK_BOX(layout), filer, TRUE, TRUE, 5);

  /* Adjust size of chooser->name entry field. */
  gtk_widget_set_usize (chooser->name, 7*width/9, 0);

  /* Initialize default output file name */
  savefile = g_strdup_printf("%s.%s", Program, get_file_extension(PostScript));
  gtk_entry_set_text(GTK_ENTRY(chooser->name), savefile);

  /* Assemble the buttons */
  button = add_button(dialog, ICON_CANCEL, _("Cancel"),
                               GTK_RESPONSE_CANCEL);

  g_signal_connect_swapped (G_OBJECT (button), "clicked",
                            GTK_SIGNAL_FUNC (gtk_widget_hide),
                            G_OBJECT (dialog));
  gtk_widget_show(button);

  button = add_button(dialog, ICON_PRINT, _("Print"), GTK_RESPONSE_OK);
  g_signal_connect (G_OBJECT(button), "clicked",
                         G_CALLBACK (printdialog_callback), &printer);
  gtk_widget_show(button);

  /* Set event handlers for window close/destroy */
  g_signal_connect (G_OBJECT (dialog), "destroy",
                      G_CALLBACK (gtk_widget_hide), NULL);

  g_signal_connect (G_OBJECT (dialog), "delete_event",
                      G_CALLBACK (gtk_widget_hide), NULL);

  /* One time initialization of struct _print_dialog */
  printer.callback= callback;
  printer.chooser = chooser;

  printer.parent  = parent;
  printer.dialog  = dialog;
  printer.filer   = filer;

  printer.width   = width;
  printer.height  = height;

  printer.device  = PRINT_DEVICE;
  printer.paper   = 0;		/* PaperSize Paper[] index (see, print.c) */

  return &printer;
} /* </printdialog_create> */

/*
* print image dialog wrapper
*/
PrintDialog *
printdialog_new (GtkWidget *parent, GtkFunction agent)
{
  static PrintDialog *self = NULL;
  return (self != NULL) ? self : (self = printdialog_create(parent, agent));
} /* </printdialog> */

/*
* (private) create file chooser callback method
*/
static bool
savedialog_callback(GtkWidget *button, SaveDialog *save)
{
  //SaveDialog *save = (SaveDialog *)data;
  const gchar *outfile = filechooser_get_selected_name (save->chooser);
  bool status = true;
  struct stat info;

  if (lstat(outfile, &info) == 0 && S_ISDIR(info.st_mode))
    status = false;
  else if ( (status = (*save->callback) (save)) )
    gtk_widget_hide (save->dialog);

  return status;
} /* </savedialog_callback> */

/*
* (private) create file chooser dialog
*/
static SaveDialog *
savedialog_create(GtkWidget *parent, GtkFunction callback)
{
  const gint width  = 400;
  const gint height = 300;

  GtkWidget *dialog   = gtk_dialog_new ();
  GtkWidget *layout   = gtk_vbox_new (FALSE, 2);  /* save dialog layout */

  GtkWidget *selector = add_filetypes(&(story.options));
  GtkWidget *apply, *cancel, *filer, *frame;

  FileChooser *chooser = filechooser_new (FILECHOOSER_CWD, NULL);
  gchar *savefile;

  /* Customize behavior and look */
  gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);
  gtk_widget_set_usize (dialog, width, height);

  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_MOUSE);
  gtk_window_set_icon (GTK_WINDOW (dialog), xpm_pixbuf(ICON_SAVE, NULL));
  gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (parent));
  /* gtk_window_set_resizable(GTK_WINDOW(dialog), FALSE); */
  gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);

  /* Set event handlers for window close/destroy */
  g_signal_connect (G_OBJECT (dialog), "destroy",
	              G_CALLBACK (gtk_widget_hide), NULL);

  g_signal_connect (G_OBJECT (dialog), "delete_event",
	 	      G_CALLBACK (gtk_widget_hide), NULL);

  /* Assemble contents of dialog window */
  frame = gtk_frame_new(NULL);
  gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), frame);
  gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_ETCHED_IN);
  gtk_container_set_border_width(GTK_CONTAINER (frame), 2);
  gtk_widget_show(frame);

  gtk_container_add(GTK_CONTAINER(frame), layout);
  gtk_widget_show(layout);

  filer = filechooser_layout (chooser);
  gtk_box_pack_start (GTK_BOX(layout), filer, TRUE, TRUE, 5);
  gtk_widget_show (filer);

  /* Adjust size of chooser->name entry field. */
  gtk_widget_set_usize (chooser->name, 8*width/9, 0);

  /* Initialize default output file name */
  savefile = g_strdup_printf("%s.%s", Program, get_file_extension(PNG));
  gtk_entry_set_text(GTK_ENTRY(chooser->name), savefile);

  /* Add file types selection */
  gtk_box_pack_start (GTK_BOX(layout), selector, FALSE, FALSE, 0);
  gtk_widget_show(selector);

  /* Add CANCEL and APPLY buttons */
  cancel = add_button(dialog, ICON_CANCEL, _("Cancel"),
                        GTK_RESPONSE_CANCEL);

  /* Connect the cancel button to hide the widget */
  g_signal_connect_swapped (G_OBJECT (cancel), "clicked",
                            G_CALLBACK (gtk_widget_hide),
                            G_OBJECT (dialog));

  apply = add_button(dialog, ICON_SAVE, _("Save"), GTK_RESPONSE_OK);
  gtk_signal_connect (GTK_OBJECT (apply), "clicked",
                       G_CALLBACK (savedialog_callback), &story);

  /* One time initialization of module data structures */
  story.parent  = parent;
  story.dialog  = dialog;
  story.filer   = filer;

  story.callback= callback;
  story.chooser = chooser;
  story.imgtype = PNG;

  story.cancel  = cancel;
  story.apply   = apply;

  return &story;
} /* </savedialog_create> */

/*
* save file chooser dialog wrapper
*/
SaveDialog *
savedialog_new (GtkWidget *parent, GtkFunction callback)
{
  static SaveDialog *self = NULL;
  return (self != NULL) ? self : (self = savedialog_create(parent, callback));
} /* </savedialog> */

/*
* alert dialog
*/
static GtkWidget *
alert(GtkWidget *parent, IconIndex icon,
	const char *fontname, const int fontsize, const gchar *message)
{
  GtkWidget *button;
  GtkWidget *dialog = gtk_dialog_new ();
  GtkWidget *layout = gtk_hbox_new (FALSE, 8);
  GtkWidget *inside = xpm_label (icon, fontname, fontsize, message);
  GtkWindow *window = GTK_WINDOW(dialog);

  /* Customize behavior and look */
  gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);

  gtk_window_set_modal (window, TRUE);
  gtk_window_set_resizable(window, FALSE);
  gtk_window_set_decorated (window, FALSE);
  gtk_window_set_position (window, GTK_WIN_POS_MOUSE);
  gtk_window_set_transient_for (window, GTK_WINDOW (parent));
  gtk_window_set_skip_taskbar_hint (window, TRUE);
  gtk_window_set_skip_pager_hint (window, TRUE);
  gtk_window_set_keep_above (window, TRUE);
  /*
  gtk_window_set_role(window, "notify_dialog");
  g_signal_connect(G_OBJECT(dialog), "response",
                      G_CALLBACK(fire_alert), dialog);
  */

  /* Assemble contents of dialog window with icon and message */
  gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), layout);
  gtk_box_pack_start(GTK_BOX(layout), inside, FALSE, TRUE, 5);
  gtk_widget_show (layout);

  /* Add dismiss (acknowledge) button */
  button = add_button(dialog, ICON_CANCEL, _("Cancel"), GTK_RESPONSE_CANCEL);
  gtk_widget_hide (button);  /* must be enabled elsewhere */

  /* Store major components in class dialogue */
  dialogue.window = dialog;
  dialogue.layout = layout;
  dialogue.cancel = button;
  dialogue.apply  = add_button(dialog, ICON_DONE, _("OK"), GTK_RESPONSE_OK);

  return dialog;
} /* </alert> */

/*
* alert wrapper method
*/
static GtkWidget *
message(GtkWidget *parent, IconIndex icon,
	const char *fontname, const int fontsize, const gchar *text)
{
  if (MessageDialog == NULL)    /* singleton message dialog */
    MessageDialog = alert(parent, icon, fontname, fontsize, text);
  else {
    GtkWidget *container = GTK_DIALOG(MessageDialog)->vbox;
    GtkWidget *inside    = xpm_label(icon, fontname, fontsize, text);
    GtkWidget *layout    = gtk_hbox_new (FALSE, 8);

    gtk_container_remove(GTK_CONTAINER (container), dialogue.layout);
    gtk_box_pack_start(GTK_BOX (layout), inside , FALSE, FALSE, 2);
    gtk_container_add(GTK_CONTAINER (container), layout);
    gtk_widget_show (layout);

    dialogue.layout = layout;
  }
  return MessageDialog;
} /* </message> */

/*
* fire up an alert with a given icon and message
*/
gint
notice(GtkWidget *parent, IconIndex icon,
	const char *fontname, const int fontsize, const gchar *format, ...)
{
  GtkWidget *dialog;
  gchar *text;
  gint status;

  va_list args;
  va_start (args, format);
  text = g_strdup_vprintf (format, args);
  va_end (args);

  dialog = message(parent, icon, fontname, fontsize, text);
  g_free (text);

  gtk_widget_show (dialog);
  status = gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_hide (dialog);

  return status;
} /* </notice> */

gint
notice_at (gint xpos, gint ypos, IconIndex icon,
	const char *fontname, const int fontsize, const gchar *format, ...)
{
  GtkWidget *dialog;
  gchar *text;
  gint status;

  va_list args;
  va_start (args, format);
  text = g_strdup_vprintf (format, args);
  va_end (args);

  dialog = message(NULL, icon, fontname, fontsize, text);
  gtk_window_move (GTK_WINDOW(dialog), xpos, ypos);
  g_free (text);

  gtk_widget_show (dialog);
  status = gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_hide (dialog);

  return status;
} /* </notice_at> */

/*
* notify dialog for file write over
*/
gint
overwrite(GtkWidget *parent, const gchar* filename)
{
  GtkWidget *dialog, *apply;
  gchar *text = g_strdup_printf("%s: %s", filename, _("Already exists"));
  gint status;

  dialog = message(parent, ICON_QUESTION, NULL, 0, text);
  apply  = dialogue.apply;

  gtk_widget_show (dialogue.cancel);
  gtk_button_set_image(GTK_BUTTON (apply), xpm_image(ICON_SAVE));
  gtk_button_set_label(GTK_BUTTON (apply), _("Write Over"));

  gtk_widget_show (dialog);
  status = gtk_dialog_run (GTK_DIALOG (dialog));
  g_free(text);

  gtk_widget_hide (dialogue.cancel);
  gtk_button_set_image(GTK_BUTTON (apply), xpm_image(ICON_DONE));
  gtk_button_set_label(GTK_BUTTON (apply), _("OK"));
  gtk_widget_hide (dialog);

  return status;
} /* </overwrite> */

/*
* fire up an alert saying feature is not implemented
*/
void
unimplemented(GtkWidget *parent, const gchar* feature)
{
  static IconIndex icon = ICON_WARNING;

  if (feature != NULL)
    notice(parent, icon, NULL, 0, "[%s] %s", feature, _("Sorry, not implemented"));
  else
    notice(parent, icon, NULL, 0, _("Sorry, not implemented"));
} /* </unimplemented> */
