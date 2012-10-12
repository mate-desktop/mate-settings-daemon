/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2008 Lennart Poettering <lennart@poettering.net>
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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#ifndef __MSD_SOUND_PLUGIN_H__
#define __MSD_SOUND_PLUGIN_H__

#include <glib.h>
#include <glib-object.h>
#include <gmodule.h>

#include "mate-settings-plugin.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MSD_TYPE_SOUND_PLUGIN                (msd_sound_plugin_get_type ())
#define MSD_SOUND_PLUGIN(o)                  (G_TYPE_CHECK_INSTANCE_CAST ((o), MSD_TYPE_SOUND_PLUGIN, MsdSoundPlugin))
#define MSD_SOUND_PLUGIN_CLASS(k)            (G_TYPE_CHECK_CLASS_CAST ((k), MSD_TYPE_SOUND_PLUGIN, MsdSoundPluginClass))
#define MSD_IS_SOUND_PLUGIN(o)               (G_TYPE_CHECK_INSTANCE_TYPE ((o), MSD_TYPE_SOUND_PLUGIN))
#define MSD_IS_SOUND_PLUGIN_CLASS(k)         (G_TYPE_CHECK_CLASS_TYPE ((k), MSD_TYPE_SOUND_PLUGIN))
#define MSD_SOUND_PLUGIN_GET_CLASS(o)        (G_TYPE_INSTANCE_GET_CLASS ((o), MSD_TYPE_SOUND_PLUGIN, MsdSoundPluginClass))

typedef struct MsdSoundPluginPrivate MsdSoundPluginPrivate;

typedef struct
{
        MateSettingsPlugin parent;
        MsdSoundPluginPrivate *priv;
} MsdSoundPlugin;

typedef struct
{
        MateSettingsPluginClass parent_class;
} MsdSoundPluginClass;

GType msd_sound_plugin_get_type (void) G_GNUC_CONST;

/* All the plugins must implement this function */
G_MODULE_EXPORT GType register_mate_settings_plugin (GTypeModule *module);

#ifdef __cplusplus
}
#endif

#endif /* __MSD_SOUND_PLUGIN_H__ */
