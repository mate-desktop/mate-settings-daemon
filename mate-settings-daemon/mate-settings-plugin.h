/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2002-2005 Paolo Maggi
 * Copyright (C) 2007      William Jon McCann <mccann@jhu.edu>
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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston,
 * MA  02110-1301, USA.
 */

#ifndef __MATE_SETTINGS_PLUGIN_H__
#define __MATE_SETTINGS_PLUGIN_H__

#include <glib-object.h>
#include <gmodule.h>

#ifdef __cplusplus
extern "C" {
#endif
#define MATE_TYPE_SETTINGS_PLUGIN              (mate_settings_plugin_get_type())
#define MATE_SETTINGS_PLUGIN(obj)              (G_TYPE_CHECK_INSTANCE_CAST((obj), MATE_TYPE_SETTINGS_PLUGIN, MateSettingsPlugin))
#define MATE_SETTINGS_PLUGIN_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass),  MATE_TYPE_SETTINGS_PLUGIN, MateSettingsPluginClass))
#define MATE_IS_SETTINGS_PLUGIN(obj)           (G_TYPE_CHECK_INSTANCE_TYPE((obj), MATE_TYPE_SETTINGS_PLUGIN))
#define MATE_IS_SETTINGS_PLUGIN_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), MATE_TYPE_SETTINGS_PLUGIN))
#define MATE_SETTINGS_PLUGIN_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS((obj),  MATE_TYPE_SETTINGS_PLUGIN, MateSettingsPluginClass))

typedef struct
{
        GObject parent;
} MateSettingsPlugin;

typedef struct
{
        GObjectClass parent_class;

        /* Virtual public methods */
        void            (*activate)                     (MateSettingsPlugin *plugin);
        void            (*deactivate)                   (MateSettingsPlugin *plugin);
} MateSettingsPluginClass;

GType            mate_settings_plugin_get_type           (void) G_GNUC_CONST;

void             mate_settings_plugin_activate           (MateSettingsPlugin *plugin);
void             mate_settings_plugin_deactivate         (MateSettingsPlugin *plugin);

/*
 * Utility macro used to register plugins
 *
 * use: MATE_SETTINGS_PLUGIN_REGISTER (PluginName, plugin_name)
 */
#define MATE_SETTINGS_PLUGIN_REGISTER(PluginName, plugin_name)                 \
        G_DEFINE_DYNAMIC_TYPE (PluginName,                                     \
                               plugin_name,                                    \
                               MATE_TYPE_SETTINGS_PLUGIN)                      \
                                                                               \
G_MODULE_EXPORT GType                                                          \
register_mate_settings_plugin (GTypeModule *type_module)                       \
{                                                                              \
        plugin_name##_register_type (type_module);                             \
                                                                               \
        return plugin_name##_get_type();                                       \
}

#ifdef __cplusplus
}
#endif

#endif  /* __MATE_SETTINGS_PLUGIN_H__ */
