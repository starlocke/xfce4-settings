/*
 *  xfce4-settings-manager
 *
 *  Copyright (c) 2008 Brian Tarricone <bjt23@cornell.edu>
 *                     Jannis Pohlmann <jannis@xfce.org>
 *  Copyright (c) 2012 Nick Schermer <nick@xfce.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License ONLY.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <gtk/gtk.h>

#include <libxfce4util/libxfce4util.h>
#include <libxfce4ui/libxfce4ui.h>
#include <garcon/garcon.h>
#include <exo/exo.h>

#include "xfce-settings-manager-dialog.h"
#include "xfce-text-renderer.h"

#define TEXT_WIDTH (128)
#define ICON_WIDTH (48)



struct _XfceSettingsManagerDialog
{
    XfceTitledDialog __parent__;

    GarconMenu     *menu;

    GtkListStore   *store;

    GtkWidget      *search_entry;

    GtkWidget      *category_viewport;
    GtkWidget      *category_scroll;
    GtkWidget      *category_box;
    GList          *category_iconviews;

    GtkWidget      *socket_scroll;
    GtkWidget      *socket_viewport;
    GarconMenuItem *socket_item;

    GtkWidget      *button_back;
    GtkWidget      *button_help;

    gchar          *help_page;
    gchar          *help_component;
};

struct _XfceSettingsManagerDialogClass
{
    XfceTitledDialogClass __parent__;
};

enum
{
    COLUMN_NAME,
    COLUMN_ICON_NAME,
    COLUMN_TOOLTIP,
    COLUMN_MENU_ITEM,
    COLUMN_MENU_DIRECTORY,
    N_COLUMNS
};



static void xfce_settings_manager_dialog_finalize     (GObject                   *object);
static void xfce_settings_manager_dialog_style_set    (GtkWidget                 *widget,
                                                       GtkStyle                  *old_style);
static void xfce_settings_manager_dialog_response     (GtkDialog                 *widget,
                                                       gint                       response_id);
static void xfce_settings_manager_dialog_header_style (GtkWidget                 *header,
                                                       GtkStyle                  *old_style,
                                                       GtkWidget                 *ebox);
static void xfce_settings_manager_dialog_set_title    (XfceSettingsManagerDialog *dialog,
                                                       const gchar               *title,
                                                       const gchar               *icon_name,
                                                       const gchar               *subtitle);
static void xfce_settings_manager_dialog_go_back      (XfceSettingsManagerDialog *dialog);
static void xfce_settings_manager_dialog_menu_reload  (XfceSettingsManagerDialog *dialog);



G_DEFINE_TYPE (XfceSettingsManagerDialog, xfce_settings_manager_dialog, XFCE_TYPE_TITLED_DIALOG)



static void
xfce_settings_manager_dialog_class_init (XfceSettingsManagerDialogClass *klass)
{
    GObjectClass   *gobject_class;
    GtkDialogClass *gtkdialog_class;
    GtkWidgetClass *gtkwiget_class;

    gobject_class = G_OBJECT_CLASS (klass);
    gobject_class->finalize = xfce_settings_manager_dialog_finalize;

    gtkwiget_class = GTK_WIDGET_CLASS (klass);
    gtkwiget_class->style_set = xfce_settings_manager_dialog_style_set;

    gtkdialog_class = GTK_DIALOG_CLASS (klass);
    gtkdialog_class->response = xfce_settings_manager_dialog_response;
}



static void
xfce_settings_manager_dialog_init (XfceSettingsManagerDialog *dialog)
{
    GtkWidget *scroll;
    GtkWidget *dialog_vbox;
    GtkWidget *viewport;
    gchar     *path;
    GtkWidget *hbox;
    GtkWidget *entry;
    GtkWidget *align;
    GList     *children;
    GtkWidget *header;
    GtkWidget *ebox;
    GtkWidget *bbox;

    dialog->store = gtk_list_store_new (N_COLUMNS,
                                        G_TYPE_STRING,
                                        G_TYPE_STRING,
                                        G_TYPE_STRING,
                                        GARCON_TYPE_MENU_ITEM,
                                        GARCON_TYPE_MENU_DIRECTORY);

    path = xfce_resource_lookup (XFCE_RESOURCE_CONFIG, "menus/xfce-settings-manager.menu");
    dialog->menu = garcon_menu_new_for_path (path);
    g_free (path);

    gtk_window_set_default_size (GTK_WINDOW (dialog), 640, 500);
    xfce_settings_manager_dialog_set_title (dialog, NULL, NULL, NULL);

    dialog->button_back = xfce_gtk_button_new_mixed (GTK_STOCK_GO_BACK, _("_All Settings"));
    bbox = gtk_dialog_get_action_area (GTK_DIALOG (dialog));
    gtk_container_add (GTK_CONTAINER (bbox), dialog->button_back);
    gtk_widget_set_sensitive (dialog->button_back, FALSE);
    gtk_widget_show (dialog->button_back);
    g_signal_connect_swapped (G_OBJECT (dialog->button_back), "clicked",
        G_CALLBACK (xfce_settings_manager_dialog_go_back), dialog);

    dialog->button_help = gtk_dialog_add_button (GTK_DIALOG (dialog),
                                                 GTK_STOCK_HELP, GTK_RESPONSE_HELP);
    gtk_dialog_add_button (GTK_DIALOG (dialog), GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE);

    /* add box at start of the main box */
    hbox = gtk_hbox_new (FALSE, 0);
    dialog_vbox = gtk_bin_get_child (GTK_BIN (dialog));
    gtk_box_pack_start (GTK_BOX (dialog_vbox), hbox, FALSE, TRUE, 0);
    gtk_box_reorder_child (GTK_BOX (dialog_vbox), hbox, 0);
    gtk_widget_show (hbox);

    /* move the xfce-header in the hbox */
    children = gtk_container_get_children (GTK_CONTAINER (dialog_vbox));
    header = g_list_nth_data (children, 1);
    g_object_ref (G_OBJECT (header));
    gtk_container_remove (GTK_CONTAINER (dialog_vbox), header);
    gtk_box_pack_start (GTK_BOX (hbox), header, TRUE, TRUE, 0);
    g_object_unref (G_OBJECT (header));
    g_list_free (children);

    ebox = gtk_event_box_new ();
    gtk_box_pack_start (GTK_BOX (hbox), ebox, FALSE, TRUE, 0);
    g_signal_connect (header, "style-set",
        G_CALLBACK (xfce_settings_manager_dialog_header_style), ebox);
    gtk_widget_show (ebox);

    align = gtk_alignment_new (0.0f, 1.0f, 0.0f, 0.0f);
    gtk_container_add (GTK_CONTAINER (ebox), align);
    gtk_container_set_border_width (GTK_CONTAINER (align), 6);
    gtk_widget_show (align);

    dialog->search_entry = entry = gtk_entry_new ();
    gtk_container_add (GTK_CONTAINER (align), entry);
    gtk_entry_set_icon_from_stock (GTK_ENTRY (entry), GTK_ENTRY_ICON_SECONDARY, GTK_STOCK_FIND);
    gtk_entry_set_icon_sensitive (GTK_ENTRY (entry), GTK_ENTRY_ICON_SECONDARY, FALSE);
    gtk_widget_show (entry);

    dialog_vbox = gtk_dialog_get_content_area (GTK_DIALOG (dialog));

    dialog->category_scroll = scroll = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scroll), GTK_SHADOW_ETCHED_IN);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start (GTK_BOX (dialog_vbox), scroll, TRUE, TRUE, 0);
    gtk_container_set_border_width (GTK_CONTAINER (scroll), 6);
    gtk_widget_show (scroll);

    viewport = dialog->category_viewport = gtk_viewport_new (NULL, NULL);
    gtk_container_add (GTK_CONTAINER (scroll), viewport);
    gtk_viewport_set_shadow_type (GTK_VIEWPORT (viewport), GTK_SHADOW_NONE);
    gtk_widget_show (viewport);

    dialog->category_box = gtk_vbox_new (FALSE, 6);
    gtk_container_add (GTK_CONTAINER (viewport), dialog->category_box);
    gtk_container_set_border_width (GTK_CONTAINER (dialog->category_box), 6);
    gtk_widget_show (dialog->category_box);
    gtk_widget_set_size_request (dialog->category_box,
                                 TEXT_WIDTH   /* text */
                                 + ICON_WIDTH /* icon */
                                 + (5 * 6)    /* borders */, -1);

    /* pluggable dialog scrolled window and viewport */
    dialog->socket_scroll = scroll = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scroll), GTK_SHADOW_NONE);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start (GTK_BOX (dialog_vbox), scroll, TRUE, TRUE, 0);
    gtk_container_set_border_width (GTK_CONTAINER (scroll), 0);

    dialog->socket_viewport = viewport = gtk_viewport_new (NULL, NULL);
    gtk_container_add (GTK_CONTAINER (scroll), viewport);
    gtk_viewport_set_shadow_type (GTK_VIEWPORT (viewport), GTK_SHADOW_NONE);
    gtk_widget_show (viewport);

    xfce_settings_manager_dialog_menu_reload (dialog);

    g_signal_connect_swapped (G_OBJECT (dialog->menu), "reload-required",
        G_CALLBACK (xfce_settings_manager_dialog_menu_reload), dialog);
}



static void
xfce_settings_manager_dialog_finalize (GObject *object)
{
    XfceSettingsManagerDialog *dialog = XFCE_SETTINGS_MANAGER_DIALOG (object);

    g_free (dialog->help_page);
    g_free (dialog->help_component);

    if (dialog->socket_item != NULL)
        g_object_unref (G_OBJECT (dialog->socket_item));

    g_object_unref (G_OBJECT (dialog->menu));
    g_object_unref (G_OBJECT (dialog->store));

    G_OBJECT_CLASS (xfce_settings_manager_dialog_parent_class)->finalize (object);
}



static void
xfce_settings_manager_dialog_style_set (GtkWidget *widget,
                                        GtkStyle  *old_style)
{
    XfceSettingsManagerDialog *dialog = XFCE_SETTINGS_MANAGER_DIALOG (widget);

    GTK_WIDGET_CLASS (xfce_settings_manager_dialog_parent_class)->style_set (widget, old_style);

    /* set viewport to color icon view uses for background */
    gtk_widget_modify_bg (dialog->category_viewport,
                          GTK_STATE_NORMAL,
                          &widget->style->base[GTK_STATE_NORMAL]);
}



static void
xfce_settings_manager_dialog_response (GtkDialog *widget,
                                       gint       response_id)
{
    XfceSettingsManagerDialog *dialog = XFCE_SETTINGS_MANAGER_DIALOG (widget);
    const gchar               *help_component;

    if (response_id == GTK_RESPONSE_HELP)
    {
        if (dialog->help_component != NULL)
            help_component = dialog->help_component;
        else
            help_component = "xfce4-settings";

        xfce_dialog_show_help (GTK_WINDOW (widget),
                               help_component,
                               dialog->help_page, NULL);
    }
    else
    {
        gtk_main_quit ();
    }
}



static void
xfce_settings_manager_dialog_header_style (GtkWidget *header,
                                           GtkStyle  *old_style,
                                           GtkWidget *ebox)
{
    /* use the header background */
    gtk_widget_modify_bg (ebox, GTK_STATE_NORMAL, &header->style->base[GTK_STATE_NORMAL]);
}



static void
xfce_settings_manager_dialog_set_title (XfceSettingsManagerDialog *dialog,
                                        const gchar               *title,
                                        const gchar               *icon_name,
                                        const gchar               *subtitle)
{
    g_return_if_fail (XFCE_IS_SETTINGS_MANAGER_DIALOG (dialog));

    if (icon_name == NULL)
        icon_name = "preferences-desktop";
    if (title == NULL)
        title = _("Settings");
    if (subtitle == NULL)
        subtitle = _("Customize your desktop");

    gtk_window_set_title (GTK_WINDOW (dialog), title);
    xfce_titled_dialog_set_subtitle (XFCE_TITLED_DIALOG (dialog), subtitle);
    gtk_window_set_icon_name (GTK_WINDOW (dialog), icon_name);
}



static gboolean
xfce_settings_manager_dialog_iconview_keynav_failed (ExoIconView               *current_view,
                                                     GtkDirectionType           direction,
                                                     XfceSettingsManagerDialog *dialog)
{
    GList        *li;
    GtkTreePath  *path;
    ExoIconView  *new_view;
    gboolean      result = FALSE;
    GtkTreeModel *model;
    GtkTreeIter   iter;
    gint          col_old, col_new;
    gint          dist_prev, dist_new;
    GtkTreePath  *sel_path;

    if (direction == GTK_DIR_UP || direction == GTK_DIR_DOWN)
    {
        li = g_list_find (dialog->category_iconviews, current_view);
        if (direction == GTK_DIR_DOWN)
            li = g_list_next (li);
        else
            li = g_list_previous (li);

        /* leave there is no view obove or below this one */
        if (li == NULL)
            return FALSE;

        new_view = EXO_ICON_VIEW (li->data);

        if (exo_icon_view_get_cursor (current_view, &path, NULL))
        {
            col_old = exo_icon_view_get_item_column (current_view, path);
            gtk_tree_path_free (path);

            dist_prev = 1000;
            sel_path = NULL;

            model = exo_icon_view_get_model (new_view);
            if (gtk_tree_model_get_iter_first (model, &iter))
            {
                do
                {
                     path = gtk_tree_model_get_path (model, &iter);
                     col_new = exo_icon_view_get_item_column (new_view, path);
                     dist_new = ABS (col_new - col_old);

                     if ((direction == GTK_DIR_UP && dist_new <= dist_prev)
                         || (direction == GTK_DIR_DOWN  && dist_new < dist_prev))
                     {
                         if (sel_path != NULL)
                             gtk_tree_path_free (sel_path);

                         sel_path = path;
                         dist_prev = dist_new;
                     }
                     else
                     {
                         gtk_tree_path_free (path);
                     }
                }
                while (gtk_tree_model_iter_next (model, &iter));
            }

            if (G_LIKELY (sel_path != NULL))
            {
                /* move cursor, grab-focus will handle the selection */
                exo_icon_view_set_cursor (new_view, sel_path, NULL, FALSE);
                gtk_tree_path_free (sel_path);

                gtk_widget_grab_focus (GTK_WIDGET (new_view));

                result = TRUE;
            }
        }
    }

    return result;
}



static gboolean
xfce_settings_manager_dialog_query_tooltip (GtkWidget                 *iconview,
                                            gint                       x,
                                            gint                       y,
                                            gboolean                   keyboard_mode,
                                            GtkTooltip                *tooltip,
                                            XfceSettingsManagerDialog *dialog)
{
    GtkTreePath    *path;
    GValue          value = { 0, };
    GtkTreeModel   *model;
    GtkTreeIter     iter;
    GarconMenuItem *item;
    const gchar    *comment;

    if (keyboard_mode)
    {
        if (!exo_icon_view_get_cursor (EXO_ICON_VIEW (iconview), &path, NULL))
            return FALSE;
    }
    else
    {
        path = exo_icon_view_get_path_at_pos (EXO_ICON_VIEW (iconview), x, y);
        if (G_UNLIKELY (path == NULL))
            return FALSE;
    }

    model = exo_icon_view_get_model (EXO_ICON_VIEW (iconview));
    if (gtk_tree_model_get_iter (model, &iter, path))
    {
        gtk_tree_model_get_value (model, &iter, COLUMN_MENU_ITEM, &value);
        item = g_value_get_object (&value);
        g_assert (GARCON_IS_MENU_ITEM (item));

        comment = garcon_menu_item_get_comment (item);
        if (!exo_str_is_empty (comment))
            gtk_tooltip_set_text (tooltip, comment);

        g_value_unset (&value);
    }

    gtk_tree_path_free (path);

    return TRUE;
}



static gboolean
xfce_settings_manager_dialog_iconview_focus (GtkWidget                 *iconview,
                                             GdkEventFocus             *event,
                                             XfceSettingsManagerDialog *dialog)
{
    GtkTreePath *path;

    if (event->in)
    {
        /* a mouse click will have focus, tab events not */
        if (!exo_icon_view_get_cursor (EXO_ICON_VIEW (iconview), &path, NULL))
        {
           path = gtk_tree_path_new_from_indices (0, -1);
           exo_icon_view_set_cursor (EXO_ICON_VIEW (iconview), path, NULL, FALSE);
        }

        exo_icon_view_select_path (EXO_ICON_VIEW (iconview), path);
        gtk_tree_path_free (path);
    }
    else
    {
        exo_icon_view_unselect_all (EXO_ICON_VIEW (iconview));
    }

    return FALSE;
}



static void
xfce_settings_manager_dialog_go_back (XfceSettingsManagerDialog *dialog)
{
    GtkWidget *socket;

    /* make sure no cursor is shown */
    gdk_window_set_cursor (GTK_WIDGET (dialog)->window, NULL);

    /* reset dialog info */
    xfce_settings_manager_dialog_set_title (dialog, NULL, NULL, NULL);

    gtk_widget_show (dialog->category_scroll);
    gtk_widget_hide (dialog->socket_scroll);

    g_free (dialog->help_page);
    dialog->help_page = NULL;
    g_free (dialog->help_component);
    dialog->help_component = NULL;

    gtk_widget_set_sensitive (dialog->button_back, FALSE);
    gtk_widget_set_sensitive (dialog->button_help, TRUE);
    gtk_widget_set_sensitive (dialog->search_entry, TRUE);

    socket = gtk_bin_get_child (GTK_BIN (dialog->socket_viewport));
    if (G_LIKELY (socket != NULL))
        gtk_widget_destroy (socket);

    if (dialog->socket_item != NULL)
    {
        g_object_unref (G_OBJECT (dialog->socket_item));
        dialog->socket_item = NULL;
    }
}



static void
xfce_settings_manager_dialog_plug_added (GtkWidget                 *socket,
                                         XfceSettingsManagerDialog *dialog)
{
    /* set dialog information from desktop file */
    xfce_settings_manager_dialog_set_title (dialog,
        garcon_menu_item_get_name (dialog->socket_item),
        garcon_menu_item_get_icon_name (dialog->socket_item),
        garcon_menu_item_get_comment (dialog->socket_item));

    /* show socket and hide the categories view */
    gtk_widget_show (dialog->socket_scroll);
    gtk_widget_hide (dialog->category_scroll);

    /* button sensitivity */
    gtk_widget_set_sensitive (dialog->button_back, TRUE);
    gtk_widget_set_sensitive (dialog->button_help, dialog->help_page != NULL);
    gtk_widget_set_sensitive (dialog->search_entry, FALSE);

    /* plug startup complete */
    gdk_window_set_cursor (GTK_WIDGET (dialog)->window, NULL);
}



static void
xfce_settings_manager_dialog_plug_removed (GtkWidget                 *socket,
                                           XfceSettingsManagerDialog *dialog)
{
    /* this shouldn't happen */
    g_critical ("pluggable dialog \"%s\" crashed",
                garcon_menu_item_get_command (dialog->socket_item));

    /* restore dialog */
    xfce_settings_manager_dialog_go_back (dialog);
}



static void
xfce_settings_manager_dialog_spawn (XfceSettingsManagerDialog *dialog,
                                    GarconMenuItem            *item)
{
    const gchar    *command;
    gboolean        snotify;
    GdkScreen      *screen;
    GError         *error = NULL;
    GFile          *desktop_file;
    gchar          *filename;
    XfceRc         *rc;
    gboolean        pluggable = FALSE;
    gchar          *cmd;
    GtkWidget      *socket;
    GdkCursor      *cursor;

    g_return_if_fail (GARCON_IS_MENU_ITEM (item));

    screen = gtk_window_get_screen (GTK_WINDOW (dialog));
    command = garcon_menu_item_get_command (item);

    /* we need to read some more info from the desktop
     *  file that is not supported by garcon */
    desktop_file = garcon_menu_item_get_file (item);
    filename = g_file_get_path (desktop_file);
    g_object_unref (desktop_file);

    rc = xfce_rc_simple_open (filename, TRUE);
    g_free (filename);
    if (G_LIKELY (rc != NULL))
    {
        pluggable = xfce_rc_read_bool_entry (rc, "X-XfcePluggable", FALSE);
        if (pluggable)
        {
            dialog->help_page = g_strdup (xfce_rc_read_entry (rc, "X-XfceHelpPage", NULL));
            dialog->help_component = g_strdup (xfce_rc_read_entry (rc, "X-XfceHelpComponent", NULL));
        }

        xfce_rc_close (rc);
    }

    if (pluggable)
    {
        /* fake startup notification */
        cursor = gdk_cursor_new (GDK_WATCH);
        gdk_window_set_cursor (GTK_WIDGET (dialog)->window, cursor);
        gdk_cursor_unref (cursor);

        /* create fresh socket */
        socket = gtk_socket_new ();
        gtk_container_add (GTK_CONTAINER (dialog->socket_viewport), socket);
        g_signal_connect (G_OBJECT (socket), "plug-added",
            G_CALLBACK (xfce_settings_manager_dialog_plug_added), dialog);
        g_signal_connect (G_OBJECT (socket), "plug-removed",
            G_CALLBACK (xfce_settings_manager_dialog_plug_removed), dialog);
        gtk_widget_show (socket);

        /* for info when the plug is attached */
        dialog->socket_item = g_object_ref (item);

        /* spawn dialog with socket argument */
        cmd = g_strdup_printf ("%s --socket-id=%d", command, gtk_socket_get_id (GTK_SOCKET (socket)));
        if (!xfce_spawn_command_line_on_screen (screen, cmd, FALSE, FALSE, &error))
        {
            gdk_window_set_cursor (GTK_WIDGET (dialog)->window, NULL);

            xfce_dialog_show_error (GTK_WINDOW (dialog), error,
                                    _("Unable to start \"%s\""), command);
            g_error_free (error);
        }
        g_free (cmd);
    }
    else
    {
        snotify = garcon_menu_item_supports_startup_notification (item);
        if (!xfce_spawn_command_line_on_screen (screen, command, FALSE, snotify, &error))
        {
            xfce_dialog_show_error (GTK_WINDOW (dialog), error,
                                    _("Unable to start \"%s\""), command);
            g_error_free (error);
        }
    }
}



static void
xfce_settings_manager_dialog_item_activated (ExoIconView               *iconview,
                                             GtkTreePath               *path,
                                             XfceSettingsManagerDialog *dialog)
{
    GtkTreeModel   *model;
    GtkTreeIter     iter;
    GarconMenuItem *item;

    model = exo_icon_view_get_model (iconview);
    if (gtk_tree_model_get_iter (model, &iter, path))
    {
        gtk_tree_model_get (model, &iter, COLUMN_MENU_ITEM, &item, -1);
        g_assert (GARCON_IS_MENU_ITEM (item));

        xfce_settings_manager_dialog_spawn (dialog, item);

        g_object_unref (G_OBJECT (item));
    }
}



static gboolean
xfce_settings_manager_dialog_filter_category (GtkTreeModel *model,
                                              GtkTreeIter  *iter,
                                              gpointer      data)
{
    GValue   value = { 0, };
    gboolean visible;

    gtk_tree_model_get_value (model, iter, COLUMN_MENU_DIRECTORY, &value);
    visible = g_value_get_object (&value) == data;
    g_value_unset (&value);

    return visible;
}



static void
xfce_settings_manager_dialog_add_category (XfceSettingsManagerDialog *dialog,
                                           GarconMenuDirectory       *directory)
{
    GtkTreeModel    *filter;
    GtkWidget       *alignment;
    GtkWidget       *iconview;
    GtkWidget       *label;
    GtkWidget       *separator;
    GtkWidget       *vbox;
    PangoAttrList   *attrs;
    GtkCellRenderer *render;

    /* filter category from main store */
    filter = gtk_tree_model_filter_new (GTK_TREE_MODEL (dialog->store), NULL);
    gtk_tree_model_filter_set_visible_func (GTK_TREE_MODEL_FILTER (filter),
        xfce_settings_manager_dialog_filter_category,
        g_object_ref (directory), g_object_unref);

    vbox = gtk_vbox_new (FALSE, 0);
    gtk_box_pack_start (GTK_BOX (dialog->category_box), vbox, FALSE, TRUE, 0);
    gtk_widget_show (vbox);

    /* create a label for the category title */
    label = gtk_label_new (garcon_menu_directory_get_name (directory));
    attrs = pango_attr_list_new ();
    pango_attr_list_insert (attrs, pango_attr_weight_new (PANGO_WEIGHT_BOLD));
    gtk_label_set_attributes (GTK_LABEL (label), attrs);
    pango_attr_list_unref (attrs);
    gtk_misc_set_alignment (GTK_MISC (label), 0.0f, 0.5f);
    gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, TRUE, 0);
    gtk_widget_show (label);

    /* separate title and content using a horizontal line */
    separator = gtk_hseparator_new ();
    gtk_box_pack_start (GTK_BOX (vbox), separator, FALSE, TRUE, 0);
    gtk_widget_show (separator);

    /* use an alignment to separate the category content from the title */
    alignment = gtk_alignment_new (0.0f, 0.0f, 1.0f, 1.0f);
    gtk_alignment_set_padding (GTK_ALIGNMENT (alignment), 6, 0, 0, 0);
    gtk_container_add (GTK_CONTAINER (vbox), alignment);
    gtk_widget_show (alignment);

    iconview = exo_icon_view_new_with_model (GTK_TREE_MODEL (filter));
    gtk_container_add (GTK_CONTAINER (alignment), iconview);
    exo_icon_view_set_orientation (EXO_ICON_VIEW (iconview), GTK_ORIENTATION_HORIZONTAL);
    exo_icon_view_set_margin (EXO_ICON_VIEW (iconview), 0);
    exo_icon_view_set_single_click (EXO_ICON_VIEW (iconview), TRUE);
    exo_icon_view_set_enable_search (EXO_ICON_VIEW (iconview), FALSE);
    exo_icon_view_set_item_width (EXO_ICON_VIEW (iconview), TEXT_WIDTH + ICON_WIDTH);
    gtk_widget_show (iconview);

    /* list used for unselecting */
    dialog->category_iconviews = g_list_append (dialog->category_iconviews, iconview);

    gtk_widget_set_has_tooltip (iconview, TRUE);
    g_signal_connect (G_OBJECT (iconview), "query-tooltip",
        G_CALLBACK (xfce_settings_manager_dialog_query_tooltip), dialog);
    g_signal_connect (G_OBJECT (iconview), "focus-in-event",
        G_CALLBACK (xfce_settings_manager_dialog_iconview_focus), dialog);
    g_signal_connect (G_OBJECT (iconview), "focus-out-event",
        G_CALLBACK (xfce_settings_manager_dialog_iconview_focus), dialog);
    g_signal_connect (G_OBJECT (iconview), "keynav-failed",
        G_CALLBACK (xfce_settings_manager_dialog_iconview_keynav_failed), dialog);
    g_signal_connect (G_OBJECT (iconview), "item-activated",
        G_CALLBACK (xfce_settings_manager_dialog_item_activated), dialog);

    render = gtk_cell_renderer_pixbuf_new ();
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (iconview), render, FALSE);
    gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (iconview), render, "icon-name", COLUMN_ICON_NAME);
    g_object_set (G_OBJECT (render),
                  "stock-size", GTK_ICON_SIZE_DIALOG,
                  "follow-state", TRUE,
                  NULL);

    render = xfce_text_renderer_new ();
    gtk_cell_layout_pack_end (GTK_CELL_LAYOUT (iconview), render, FALSE);
    gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (iconview), render, "text", COLUMN_NAME);
    g_object_set (G_OBJECT (render),
                  "wrap-mode", PANGO_WRAP_WORD,
                  "wrap-width", TEXT_WIDTH,
                  "follow-prelit", TRUE,
                  "follow-state", TRUE,
                  NULL);

    g_object_unref (G_OBJECT (filter));
}



static void
xfce_settings_manager_dialog_menu_collect (GarconMenu  *menu,
                                           GList      **items)
{
    GList *elements, *li;

    g_return_if_fail (GARCON_IS_MENU (menu));

    elements = garcon_menu_get_elements (menu);

    for (li = elements; li != NULL; li = li->next)
    {
        if (GARCON_IS_MENU_ITEM (li->data))
        {
            /* only add visible items */
            if (garcon_menu_element_get_visible (li->data))
                *items = g_list_prepend (*items, li->data);
        }
        else if (GARCON_IS_MENU (li->data))
        {
            /* we collect only 1 level deep in a category, so
             * add the submenu items too (should never happen tho) */
            xfce_settings_manager_dialog_menu_collect (li->data, items);
        }
    }

    g_list_free (elements);
}



static gint
xfce_settings_manager_dialog_menu_sort (gconstpointer a,
                                        gconstpointer b)
{
    return g_utf8_collate (garcon_menu_item_get_name (GARCON_MENU_ITEM (a)),
                           garcon_menu_item_get_name (GARCON_MENU_ITEM (b)));
}



static void
xfce_settings_manager_dialog_menu_reload (XfceSettingsManagerDialog *dialog)
{
    GError              *error = NULL;
    GList               *elements, *li;
    GarconMenuDirectory *directory;
    GList               *items, *lp;
    gint                 i = 0;

    g_return_if_fail (XFCE_IS_SETTINGS_MANAGER_DIALOG (dialog));
    g_return_if_fail (GARCON_IS_MENU (dialog->menu));

    if (garcon_menu_load (dialog->menu, NULL, &error))
    {
        /* get all menu elements (preserve layout) */
        elements = garcon_menu_get_elements (dialog->menu);
        for (li = elements; li != NULL; li = li->next)
        {
            /* only accept toplevel menus */
            if (!GARCON_IS_MENU (li->data))
                continue;

            directory = garcon_menu_get_directory (li->data);
            if (G_UNLIKELY (directory == NULL))
                continue;

            items = NULL;

            xfce_settings_manager_dialog_menu_collect (li->data, &items);

            /* add the new category if it has visible items */
            if (G_LIKELY (items != NULL))
            {
                /* insert new items in main store */
                items = g_list_sort (items, xfce_settings_manager_dialog_menu_sort);
                for (lp = items; lp != NULL; lp = lp->next)
                {
                    gtk_list_store_insert_with_values (dialog->store, NULL, i++,
                        COLUMN_NAME, garcon_menu_item_get_name (lp->data),
                        COLUMN_ICON_NAME, garcon_menu_item_get_icon_name (lp->data),
                        COLUMN_TOOLTIP, garcon_menu_item_get_comment (lp->data),
                        COLUMN_MENU_ITEM, lp->data,
                        COLUMN_MENU_DIRECTORY, directory, -1);
                }
                g_list_free (items);

                /* add the new category to the box */
                xfce_settings_manager_dialog_add_category (dialog, directory);
            }
        }

        g_list_free (elements);
    }
    else
    {
        g_critical ("Failed to load menu: %s", error->message);
        g_error_free (error);
    }
}


GtkWidget *
xfce_settings_manager_dialog_new (void)
{
    return g_object_new (XFCE_TYPE_SETTINGS_MANAGER_DIALOG, NULL);
}



gboolean
xfce_settings_manager_dialog_show_dialog (XfceSettingsManagerDialog *dialog,
                                          const gchar               *dialog_name)
{
    GtkTreeModel   *model = GTK_TREE_MODEL (dialog->store);
    GtkTreeIter     iter;
    GarconMenuItem *item;
    const gchar    *desktop_id;
    gchar          *name;
    gboolean        found = FALSE;

    g_return_val_if_fail (XFCE_IS_SETTINGS_MANAGER_DIALOG (dialog), FALSE);

    name = g_strdup_printf ("%s.desktop", dialog_name);

    if (gtk_tree_model_get_iter_first (model, &iter))
    {
        do
        {
             gtk_tree_model_get (model, &iter, COLUMN_MENU_ITEM, &item, -1);
             g_assert (GARCON_IS_MENU_ITEM (item));

             desktop_id = garcon_menu_item_get_desktop_id (item);
             if (g_strcmp0 (desktop_id, name) == 0)
             {
                  xfce_settings_manager_dialog_spawn (dialog, item);
                  found = TRUE;
             }

             g_object_unref (G_OBJECT (item));
        }
        while (!found && gtk_tree_model_iter_next (model, &iter));
    }

    g_free (name);

    return found;
}
