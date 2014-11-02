/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007 William Jon McCann <mccann@jhu.edu>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */

#ifndef __MSD_A11Y_SETTINGS_MANAGER_H
#define __MSD_A11Y_SETTINGS_MANAGER_H

#include <glib-object.h>

G_BEGIN_DECLS

#define MSD_TYPE_A11Y_SETTINGS_MANAGER         (msd_a11y_settings_manager_get_type ())
#define MSD_A11Y_SETTINGS_MANAGER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), MSD_TYPE_A11Y_SETTINGS_MANAGER, MsdA11ySettingsManager))
#define MSD_A11Y_SETTINGS_MANAGER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), MSD_TYPE_A11Y_SETTINGS_MANAGER, MsdA11ySettingsManagerClass))
#define MSD_IS_A11Y_SETTINGS_MANAGER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), MSD_TYPE_A11Y_SETTINGS_MANAGER))
#define MSD_IS_A11Y_SETTINGS_MANAGER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), MSD_TYPE_A11Y_SETTINGS_MANAGER))
#define MSD_A11Y_SETTINGS_MANAGER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), MSD_TYPE_A11Y_SETTINGS_MANAGER, MsdA11ySettingsManagerClass))

typedef struct MsdA11ySettingsManagerPrivate MsdA11ySettingsManagerPrivate;

typedef struct
{
        GObject                        parent;
        MsdA11ySettingsManagerPrivate *priv;
} MsdA11ySettingsManager;

typedef struct
{
        GObjectClass   parent_class;
} MsdA11ySettingsManagerClass;

GType                   msd_a11y_settings_manager_get_type            (void);

MsdA11ySettingsManager *msd_a11y_settings_manager_new                 (void);
gboolean                msd_a11y_settings_manager_start               (MsdA11ySettingsManager *manager,
                                                                       GError         **error);
void                    msd_a11y_settings_manager_stop                (MsdA11ySettingsManager *manager);

G_END_DECLS

#endif /* __MSD_A11Y_SETTINGS_MANAGER_H */
