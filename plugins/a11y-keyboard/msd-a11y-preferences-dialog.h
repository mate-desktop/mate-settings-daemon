/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2008 William Jon McCann <jmccann@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#ifndef __MSD_A11Y_PREFERENCES_DIALOG_H
#define __MSD_A11Y_PREFERENCES_DIALOG_H

#include <glib-object.h>
#include <gtk/gtk.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MSD_TYPE_A11Y_PREFERENCES_DIALOG         (msd_a11y_preferences_dialog_get_type ())
#define MSD_A11Y_PREFERENCES_DIALOG(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), MSD_TYPE_A11Y_PREFERENCES_DIALOG, MsdA11yPreferencesDialog))
#define MSD_A11Y_PREFERENCES_DIALOG_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), MSD_TYPE_A11Y_PREFERENCES_DIALOG, MsdA11yPreferencesDialogClass))
#define MSD_IS_A11Y_PREFERENCES_DIALOG(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), MSD_TYPE_A11Y_PREFERENCES_DIALOG))
#define MSD_IS_A11Y_PREFERENCES_DIALOG_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), MSD_TYPE_A11Y_PREFERENCES_DIALOG))
#define MSD_A11Y_PREFERENCES_DIALOG_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), MSD_TYPE_A11Y_PREFERENCES_DIALOG, MsdA11yPreferencesDialogClass))

typedef struct MsdA11yPreferencesDialogPrivate MsdA11yPreferencesDialogPrivate;

typedef struct
{
        GtkDialog                        parent;
        MsdA11yPreferencesDialogPrivate *priv;
} MsdA11yPreferencesDialog;

typedef struct
{
        GtkDialogClass   parent_class;
} MsdA11yPreferencesDialogClass;

GType                  msd_a11y_preferences_dialog_get_type                   (void);

GtkWidget            * msd_a11y_preferences_dialog_new                        (void);

#ifdef __cplusplus
}
#endif

#endif /* __MSD_A11Y_PREFERENCES_DIALOG_H */
