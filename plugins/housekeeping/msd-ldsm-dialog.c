/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * msd-ldsm-dialog.c
 * Copyright (C) Chris Coulson 2009 <chrisccoulson@googlemail.com>
 * 
 * msd-ldsm-dialog.c is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * msd-ldsm-dialog.c is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <glib/gi18n.h>
#include <gio/gio.h>

#include "msd-ldsm-dialog.h"

#define SETTINGS_SCHEMA "org.mate.SettingsDaemon.plugins.housekeeping"
#define SETTINGS_IGNORE_PATHS "ignore-paths"

enum
{
        PROP_0,
        PROP_OTHER_USABLE_PARTITIONS,
        PROP_OTHER_PARTITIONS,
        PROP_HAS_TRASH,
        PROP_SPACE_REMAINING,
        PROP_PARTITION_NAME,
        PROP_MOUNT_PATH
};

struct MsdLdsmDialogPrivate
{
        GtkWidget *primary_label;
        GtkWidget *secondary_label;
        GtkWidget *ignore_check_button;
        gboolean other_usable_partitions;
        gboolean other_partitions;
        gboolean has_trash;
        gint64 space_remaining;
        gchar *partition_name;
        gchar *mount_path;
};

#define MSD_LDSM_DIALOG_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), MSD_TYPE_LDSM_DIALOG, MsdLdsmDialogPrivate))

static void     msd_ldsm_dialog_class_init  (MsdLdsmDialogClass *klass);
static void     msd_ldsm_dialog_init        (MsdLdsmDialog      *dialog);

G_DEFINE_TYPE (MsdLdsmDialog, msd_ldsm_dialog, GTK_TYPE_DIALOG);

static const gchar*
msd_ldsm_dialog_get_checkbutton_text (MsdLdsmDialog *dialog)
{
        g_return_val_if_fail (MSD_IS_LDSM_DIALOG (dialog), NULL);
        
        if (dialog->priv->other_partitions)
                return _("Don't show any warnings again for this file system");
        else
                return _("Don't show any warnings again");
}

static gchar*
msd_ldsm_dialog_get_primary_text (MsdLdsmDialog *dialog)
{
        gchar *primary_text, *free_space;
	
        g_return_val_if_fail (MSD_IS_LDSM_DIALOG (dialog), NULL);
	
        free_space = g_format_size (dialog->priv->space_remaining);
	
        if (dialog->priv->other_partitions) {
                primary_text = g_strdup_printf (_("The volume \"%s\" has only %s disk space remaining."),
                                                dialog->priv->partition_name, free_space);
        } else {
                primary_text = g_strdup_printf (_("This computer has only %s disk space remaining."),
                                                free_space);
        }
	
        g_free (free_space);
	
        return primary_text;	
}

static const gchar*
msd_ldsm_dialog_get_secondary_text (MsdLdsmDialog *dialog)
{
        g_return_val_if_fail (MSD_IS_LDSM_DIALOG (dialog), NULL);
	
        if (dialog->priv->other_usable_partitions) {
                if (dialog->priv->has_trash) {
                        return _("You can free up disk space by emptying the Trash, removing " \
                                 "unused programs or files, or moving files to another disk or partition.");
                } else {
                        return _("You can free up disk space by removing unused programs or files, " \
                                 "or by moving files to another disk or partition.");
                }
        } else {
                if (dialog->priv->has_trash) {
                        return _("You can free up disk space by emptying the Trash, removing unused " \
                                 "programs or files, or moving files to an external disk.");
                } else {
                        return _("You can free up disk space by removing unused programs or files, " \
                                 "or by moving files to an external disk.");
                }
        }
}

static gint
ignore_path_compare (gconstpointer a,
                     gconstpointer b)
{
        return g_strcmp0 ((const gchar *)a, (const gchar *)b);
}

static gboolean
update_ignore_paths (GSList **ignore_paths,
                     const gchar *mount_path,
                     gboolean ignore)
{
        GSList *found;
        gchar *path_to_remove;
        
        found = g_slist_find_custom (*ignore_paths, mount_path, (GCompareFunc) ignore_path_compare);
        
        if (ignore && (found == NULL)) {
                *ignore_paths = g_slist_prepend (*ignore_paths, g_strdup (mount_path));
                return TRUE;
        }
        
        if (!ignore && (found != NULL)) {
                path_to_remove = found->data;
                *ignore_paths = g_slist_remove (*ignore_paths, path_to_remove);
                g_free (path_to_remove);
                return TRUE;
        }
                
        return FALSE;
}

static void
ignore_check_button_toggled_cb (GtkToggleButton *button,
                                gpointer user_data)
{
        MsdLdsmDialog *dialog = (MsdLdsmDialog *)user_data;
        GSettings *settings;
        gchar **settings_list;
        GSList *ignore_paths = NULL;
        gboolean ignore, updated;
        gint i;

        settings = g_settings_new (SETTINGS_SCHEMA);

        settings_list = g_settings_get_strv (settings, SETTINGS_IGNORE_PATHS);

        for (i = 0; i < G_N_ELEMENTS (settings_list); i++) {
                if (settings_list[i] != NULL)
                        ignore_paths = g_slist_prepend (ignore_paths, g_strdup (settings_list[i]));
        }
        g_strfreev (settings_list);

        if (i > 0)
                ignore_paths = g_slist_reverse (ignore_paths);

        ignore = gtk_toggle_button_get_active (button);
        updated = update_ignore_paths (&ignore_paths, dialog->priv->mount_path, ignore); 
        
        if (updated) {
            GSList *l;
            GPtrArray *array = g_ptr_array_new ();

            for (l = ignore_paths; l != NULL; l = l->next)
                    g_ptr_array_add (array, l->data);

            g_ptr_array_add (array, NULL);

            if (!g_settings_set_strv (settings, SETTINGS_IGNORE_PATHS, (const gchar **) array->pdata)) {
                    g_warning ("Cannot change ignore preference - failed to commit changes");
            }

            g_ptr_array_free (array, FALSE);
        }

        g_slist_foreach (ignore_paths, (GFunc) g_free, NULL);
        g_slist_free (ignore_paths);
        g_object_unref (settings);     
}

static void
msd_ldsm_dialog_init (MsdLdsmDialog *dialog)
{
        GtkWidget *main_vbox, *text_vbox, *hbox;
        GtkWidget *image;
	
        dialog->priv = MSD_LDSM_DIALOG_GET_PRIVATE (dialog);
        
        main_vbox = gtk_dialog_get_content_area (GTK_DIALOG (dialog));

        /* Set up all the window stuff here */
        gtk_window_set_title (GTK_WINDOW (dialog), _("Low Disk Space"));
        gtk_window_set_icon_name (GTK_WINDOW (dialog), 
                                  "dialog-warning");
        gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
        gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);
        gtk_window_set_urgency_hint (GTK_WINDOW (dialog), TRUE);
        gtk_window_set_focus_on_map (GTK_WINDOW (dialog), FALSE);
        gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);

        /* Create the image */
        image = gtk_image_new_from_icon_name ("dialog-warning", GTK_ICON_SIZE_DIALOG);
        gtk_widget_set_halign (image, GTK_ALIGN_START);
        gtk_widget_set_valign (image, GTK_ALIGN_END);

        /* Create the labels */
        dialog->priv->primary_label = gtk_label_new (NULL);	
        gtk_label_set_line_wrap (GTK_LABEL (dialog->priv->primary_label), TRUE);
        gtk_label_set_single_line_mode (GTK_LABEL (dialog->priv->primary_label), FALSE);
        gtk_label_set_max_width_chars (GTK_LABEL (dialog->priv->primary_label), 70);
        gtk_label_set_xalign (GTK_LABEL (dialog->priv->primary_label), 0.0);
        gtk_label_set_yalign (GTK_LABEL (dialog->priv->primary_label), 0.0);
	
        dialog->priv->secondary_label = gtk_label_new (NULL);
        gtk_label_set_line_wrap (GTK_LABEL (dialog->priv->secondary_label), TRUE);
        gtk_label_set_single_line_mode (GTK_LABEL (dialog->priv->secondary_label), FALSE);
        gtk_label_set_max_width_chars (GTK_LABEL (dialog->priv->secondary_label), 70);
        gtk_label_set_xalign (GTK_LABEL (dialog->priv->secondary_label), 0.0);
        gtk_label_set_yalign (GTK_LABEL (dialog->priv->secondary_label), 0.0);

        /* Create the check button to ignore future warnings */
        dialog->priv->ignore_check_button = gtk_check_button_new ();
        /* The button should be inactive if the dialog was just called.
         * I suppose it could be possible for the user to manually edit the GSettings key between
         * the mount being checked and the dialog appearing, but I don't think it matters
         * too much */
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->priv->ignore_check_button), FALSE);
        g_signal_connect (dialog->priv->ignore_check_button, "toggled",
                          G_CALLBACK (ignore_check_button_toggled_cb), dialog);
        
        /* Now set up the dialog's GtkBox's' */
        gtk_box_set_spacing (GTK_BOX (main_vbox), 14);
	
        hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
        gtk_container_set_border_width (GTK_CONTAINER (hbox), 5);
	
        text_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
        
        gtk_box_pack_start (GTK_BOX (text_vbox), dialog->priv->primary_label, FALSE, FALSE, 0);
        gtk_box_pack_start (GTK_BOX (text_vbox), dialog->priv->secondary_label, TRUE, TRUE, 0);
        gtk_box_pack_start (GTK_BOX (text_vbox), dialog->priv->ignore_check_button, FALSE, FALSE, 0);
        gtk_box_pack_start (GTK_BOX (hbox), image, FALSE, FALSE, 0);
        gtk_box_pack_start (GTK_BOX (hbox), text_vbox, TRUE, TRUE, 0);	
        gtk_box_pack_start (GTK_BOX (main_vbox), hbox, FALSE, FALSE, 0);
						
        /* Set up the action area */
        gtk_box_set_spacing (GTK_BOX (gtk_dialog_get_action_area (GTK_DIALOG (dialog))), 6);
        gtk_container_set_border_width (GTK_CONTAINER (gtk_dialog_get_action_area (GTK_DIALOG (dialog))), 5);
	
        gtk_widget_show_all (hbox);
}

static void
msd_ldsm_dialog_finalize (GObject *object)
{
        MsdLdsmDialog *self;
	
        g_return_if_fail (object != NULL);
        g_return_if_fail (MSD_IS_LDSM_DIALOG (object));

        self = MSD_LDSM_DIALOG (object);
	
        if (self->priv->partition_name)
                g_free (self->priv->partition_name);
	
        if (self->priv->mount_path)
                g_free (self->priv->mount_path);
	
        G_OBJECT_CLASS (msd_ldsm_dialog_parent_class)->finalize (object);
}

static void
msd_ldsm_dialog_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
        MsdLdsmDialog *self;
	
        g_return_if_fail (MSD_IS_LDSM_DIALOG (object));
	
        self = MSD_LDSM_DIALOG (object);

        switch (prop_id)
        {
        case PROP_OTHER_USABLE_PARTITIONS:
                self->priv->other_usable_partitions = g_value_get_boolean (value);
                break;
        case PROP_OTHER_PARTITIONS:
                self->priv->other_partitions = g_value_get_boolean (value);
                break;
        case PROP_HAS_TRASH:
                self->priv->has_trash = g_value_get_boolean (value);
                break;
        case PROP_SPACE_REMAINING:
                self->priv->space_remaining = g_value_get_int64 (value);
                break;
        case PROP_PARTITION_NAME:
                self->priv->partition_name = g_value_dup_string (value);
                break;
        case PROP_MOUNT_PATH:
                self->priv->mount_path = g_value_dup_string (value);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static void
msd_ldsm_dialog_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
        MsdLdsmDialog *self;
	
        g_return_if_fail (MSD_IS_LDSM_DIALOG (object));
	
        self = MSD_LDSM_DIALOG (object);

        switch (prop_id)
        {
        case PROP_OTHER_USABLE_PARTITIONS:
                g_value_set_boolean (value, self->priv->other_usable_partitions);
                break;
        case PROP_OTHER_PARTITIONS:
                g_value_set_boolean (value, self->priv->other_partitions);
                break;
        case PROP_HAS_TRASH:
                g_value_set_boolean (value, self->priv->has_trash);
                break;
        case PROP_SPACE_REMAINING:
                g_value_set_int64 (value, self->priv->space_remaining);
                break;
        case PROP_PARTITION_NAME:
                g_value_set_string (value, self->priv->partition_name);
                break;
        case PROP_MOUNT_PATH:
                g_value_set_string (value, self->priv->mount_path);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static void
msd_ldsm_dialog_class_init (MsdLdsmDialogClass *klass)
{
        GObjectClass* object_class = G_OBJECT_CLASS (klass);

        object_class->finalize = msd_ldsm_dialog_finalize;
        object_class->set_property = msd_ldsm_dialog_set_property;
        object_class->get_property = msd_ldsm_dialog_get_property;

        g_object_class_install_property (object_class,
                                         PROP_OTHER_USABLE_PARTITIONS,
                                         g_param_spec_boolean ("other-usable-partitions",
                                                               "other-usable-partitions",
                                                               "Set to TRUE if there are other usable partitions on the system",
                                                               FALSE,
                                                               G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

        g_object_class_install_property (object_class,
                                         PROP_OTHER_PARTITIONS,
                                         g_param_spec_boolean ("other-partitions",
                                                               "other-partitions",
                                                               "Set to TRUE if there are other partitions on the system",
                                                               FALSE,
                                                               G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

        g_object_class_install_property (object_class,
                                         PROP_HAS_TRASH,
                                         g_param_spec_boolean ("has-trash",
                                                               "has-trash",
                                                               "Set to TRUE if the partition has files in it's trash folder that can be deleted",
                                                               FALSE,
                                                               G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
	                                                       	                                                       
        g_object_class_install_property (object_class,
                                         PROP_SPACE_REMAINING,
                                         g_param_spec_int64 ("space-remaining",
                                                             "space-remaining",
                                                             "Specify how much space is remaining in bytes",
                                                             G_MININT64, G_MAXINT64, 0,
                                                             G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
	                                                   
        g_object_class_install_property (object_class,
                                         PROP_PARTITION_NAME,
                                         g_param_spec_string ("partition-name",
                                                              "partition-name",
                                                              "Specify the name of the partition",
                                                              "Unknown",
                                                              G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
	                                                      
        g_object_class_install_property (object_class,
                                         PROP_MOUNT_PATH,
                                         g_param_spec_string ("mount-path",
                                                              "mount-path",
                                                              "Specify the mount path for the partition",
                                                              "Unknown",
                                                              G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

        g_type_class_add_private (klass, sizeof (MsdLdsmDialogPrivate));
}

MsdLdsmDialog*
msd_ldsm_dialog_new (gboolean     other_usable_partitions,
                     gboolean     other_partitions,
                     gboolean     display_baobab,
                     gboolean     display_empty_trash,
                     gint64       space_remaining,
                     const gchar *partition_name,
                     const gchar *mount_path)
{
        MsdLdsmDialog *dialog;
        GtkWidget *button_empty_trash, *button_ignore, *button_analyze;
        GtkWidget *empty_trash_image, *analyze_image, *ignore_image;
        gchar *primary_text, *primary_text_markup;
        const gchar *secondary_text, *checkbutton_text;
	
        dialog = MSD_LDSM_DIALOG (g_object_new (MSD_TYPE_LDSM_DIALOG,
                                                "other-usable-partitions", other_usable_partitions,
                                                "other-partitions", other_partitions,
                                                "has-trash", display_empty_trash,
                                                "space-remaining", space_remaining,
                                                "partition-name", partition_name,
                                                "mount-path", mount_path,
                                                NULL));
	
        /* Add some buttons */
        if (dialog->priv->has_trash) {
                button_empty_trash = gtk_dialog_add_button (GTK_DIALOG (dialog),
                                                            _("Empty Trash"),
                                                            MSD_LDSM_DIALOG_RESPONSE_EMPTY_TRASH);
                empty_trash_image = gtk_image_new_from_icon_name ("edit-clear", GTK_ICON_SIZE_BUTTON);
                gtk_button_set_image (GTK_BUTTON (button_empty_trash), empty_trash_image);
        }
	
        if (display_baobab) {
                button_analyze = gtk_dialog_add_button (GTK_DIALOG (dialog),
                                                        _("Examine…"),
                                                        MSD_LDSM_DIALOG_RESPONSE_ANALYZE);
                analyze_image = gtk_image_new_from_icon_name ("baobab", GTK_ICON_SIZE_BUTTON);
                gtk_button_set_image (GTK_BUTTON (button_analyze), analyze_image);
        }
			
        button_ignore = gtk_dialog_add_button (GTK_DIALOG (dialog), 
                                               _("Ignore"), 
                                               GTK_RESPONSE_CANCEL);
        ignore_image = gtk_image_new_from_stock (GTK_STOCK_CANCEL, GTK_ICON_SIZE_BUTTON);
        gtk_button_set_image (GTK_BUTTON (button_ignore), ignore_image);
	
        gtk_widget_grab_default (button_ignore);
	
        /* Set the label text */	
        primary_text = msd_ldsm_dialog_get_primary_text (dialog);
        primary_text_markup = g_markup_printf_escaped ("<big><b>%s</b></big>", primary_text);
        gtk_label_set_markup (GTK_LABEL (dialog->priv->primary_label), primary_text_markup);

        secondary_text = msd_ldsm_dialog_get_secondary_text (dialog);
        gtk_label_set_text (GTK_LABEL (dialog->priv->secondary_label), secondary_text);
        
        checkbutton_text = msd_ldsm_dialog_get_checkbutton_text (dialog);
        gtk_button_set_label (GTK_BUTTON (dialog->priv->ignore_check_button), checkbutton_text);
        
        g_free (primary_text);
        g_free (primary_text_markup);
	
        return dialog;
} 
