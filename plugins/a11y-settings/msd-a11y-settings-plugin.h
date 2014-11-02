/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2011 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
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

#ifndef __MSD_A11Y_SETTINGS_PLUGIN_H__
#define __MSD_A11Y_SETTINGS_PLUGIN_H__

#include <glib.h>
#include <glib-object.h>
#include <gmodule.h>

#include "mate-settings-plugin.h"

G_BEGIN_DECLS

#define MSD_TYPE_A11Y_SETTINGS_PLUGIN                (msd_a11y_settings_plugin_get_type ())
#define MSD_A11Y_SETTINGS_PLUGIN(o)                  (G_TYPE_CHECK_INSTANCE_CAST ((o), MSD_TYPE_A11Y_SETTINGS_PLUGIN, MsdA11ySettingsPlugin))
#define MSD_A11Y_SETTINGS_PLUGIN_CLASS(k)            (G_TYPE_CHECK_CLASS_CAST((k), MSD_TYPE_A11Y_SETTINGS_PLUGIN, MsdA11ySettingsPluginClass))
#define MSD_IS_A11Y_SETTINGS_PLUGIN(o)               (G_TYPE_CHECK_INSTANCE_TYPE ((o), MSD_TYPE_A11Y_SETTINGS_PLUGIN))
#define MSD_IS_A11Y_SETTINGS_PLUGIN_CLASS(k)         (G_TYPE_CHECK_CLASS_TYPE ((k), MSD_TYPE_A11Y_SETTINGS_PLUGIN))
#define MSD_A11Y_SETTINGS_PLUGIN_GET_CLASS(o)        (G_TYPE_INSTANCE_GET_CLASS ((o), MSD_TYPE_A11Y_SETTINGS_PLUGIN, MsdA11ySettingsPluginClass))

typedef struct MsdA11ySettingsPluginPrivate MsdA11ySettingsPluginPrivate;

typedef struct
{
        MateSettingsPlugin           parent;
        MsdA11ySettingsPluginPrivate *priv;
} MsdA11ySettingsPlugin;

typedef struct
{
        MateSettingsPluginClass parent_class;
} MsdA11ySettingsPluginClass;

GType   msd_a11y_settings_plugin_get_type            (void) G_GNUC_CONST;

/* All the plugins must implement this function */
G_MODULE_EXPORT GType register_mate_settings_plugin (GTypeModule *module);

G_END_DECLS

#endif /* __MSD_A11Y_SETTINGS_PLUGIN_H__ */
