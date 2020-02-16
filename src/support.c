/*
Asunder

Copyright(C) 2005 Eric Lathrop <eric@ericlathrop.com>
Copyright(C) 2007 Andrew Smith <http://littlesvr.ca/contact.php>

Any code in this file may be redistributed or modified under the terms of
the GNU General Public Licence as published by the Free Software 
Foundation; version 2 of the licence.

*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

#include <gtk/gtk.h>

#include "support.h"
#include "main.h"

/* set in doRip() first */
bool overwriteAll;
bool overwriteNone;

bool confirmOverwrite(const char* pathAndName)
{
    GtkWidget* dialog;
    GtkWidget* label;
    GtkWidget* checkbox;
    char* lastSlash;
    int rc;
    char msgStr[1024];
    
    if(overwriteAll)
        return true;
    if(overwriteNone)
        return false;
    
    dialog = gtk_dialog_new_with_buttons(_("Overwrite?"),
                                         GTK_WINDOW(win_main),
                                         GTK_DIALOG_DESTROY_WITH_PARENT,
                                         GTK_STOCK_YES,
                                         GTK_RESPONSE_ACCEPT,
                                         GTK_STOCK_NO,
                                         GTK_RESPONSE_REJECT,
                                         NULL);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_REJECT);
    
    lastSlash = strrchr(pathAndName, '/');
    lastSlash++;
    
    snprintf(msgStr, 1024, _("The file '%s' already exists. Do you want to overwrite it?\n"), lastSlash);
    
    label = gtk_label_new(msgStr);
    gtk_widget_show(label);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), label, TRUE, TRUE, 0);
    
    checkbox = gtk_check_button_new_with_mnemonic(_("Remember the answer for _all the files made from this CD"));
    gtk_widget_show(checkbox);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), checkbox, TRUE, TRUE, 0);
    
    rc = gtk_dialog_run(GTK_DIALOG(dialog));
    
    if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(checkbox)))
    {
        if(rc == GTK_RESPONSE_ACCEPT)
            overwriteAll = true;
        else
            overwriteNone = true;
    }
    
    gtk_widget_destroy(dialog);
    
    if(rc == GTK_RESPONSE_ACCEPT)
        return true;
    else
        return false;
}

GtkWidget*
lookup_widget                          (GtkWidget       *widget,
                                        const gchar     *widget_name)
{
  GtkWidget *parent, *found_widget;

  for (;;)
    {
      if (GTK_IS_MENU (widget))
        parent = gtk_menu_get_attach_widget (GTK_MENU (widget));
      else
        parent = widget->parent;
      if (!parent)
        parent = (GtkWidget*) g_object_get_data (G_OBJECT (widget), "GladeParentKey");
      if (parent == NULL)
        break;
      widget = parent;
    }

  found_widget = (GtkWidget*) g_object_get_data (G_OBJECT (widget),
                                                 widget_name);
  if (!found_widget)
    g_warning ("Widget not found: %s", widget_name);
  return found_widget;
}

static GList *pixmaps_directories = NULL;

/* Use this function to set the directory containing installed pixmaps. */
void
add_pixmap_directory                   (const gchar     *directory)
{
  pixmaps_directories = g_list_prepend (pixmaps_directories,
                                        g_strdup (directory));
}

/* This is an internally used function to find pixmap files. */
static gchar*
find_pixmap_file                       (const gchar     *filename)
{
  GList *elem;

  /* We step through each of the pixmaps directory to find it. */
  elem = pixmaps_directories;
  while (elem)
    {
      gchar *pathname = g_strdup_printf ("%s%s%s", (gchar*)elem->data,
                                         G_DIR_SEPARATOR_S, filename);
      if (g_file_test (pathname, G_FILE_TEST_EXISTS))
        return pathname;
      g_free (pathname);
      elem = elem->next;
    }
  return NULL;
}

/* This is an internally used function to create pixmaps. */
GtkWidget*
create_pixmap                          (GtkWidget       *widget,
                                        const gchar     *filename)
{
  gchar *pathname = NULL;
  GtkWidget *pixmap;

  if (!filename || !filename[0])
      return gtk_image_new ();

  pathname = find_pixmap_file (filename);

  if (!pathname)
    {
      g_warning ("Couldn't find pixmap file: %s", filename);
      return gtk_image_new ();
    }

  pixmap = gtk_image_new_from_file (pathname);
  g_free (pathname);
  return pixmap;
}

/* This is an internally used function to create pixmaps. */
GdkPixbuf*
create_pixbuf                          (const gchar     *filename)
{
  gchar *pathname = NULL;
  GdkPixbuf *pixbuf;
  GError *error = NULL;

  if (!filename || !filename[0])
      return NULL;

  pathname = find_pixmap_file (filename);

  if (!pathname)
    {
      g_warning ("Couldn't find pixmap file: %s", filename);
      return NULL;
    }

  pixbuf = gdk_pixbuf_new_from_file (pathname, &error);
  if (!pixbuf)
    {
      fprintf (stderr, "Failed to load pixbuf file: %s: %s\n",
               pathname, error->message);
      g_error_free (error);
    }
  g_free (pathname);
  return pixbuf;
}

/* This is used to set ATK action descriptions. */
void
glade_set_atk_action_description       (AtkAction       *action,
                                        const gchar     *action_name,
                                        const gchar     *description)
{
  gint n_actions, i;

  n_actions = atk_action_get_n_actions (action);
  for (i = 0; i < n_actions; i++)
    {
      if (!strcmp (atk_action_get_name (action, i), action_name))
        atk_action_set_description (action, i, description);
    }
}

