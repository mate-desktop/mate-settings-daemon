/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007 William Jon McCann <mccann@jhu.edu>
 * Copyright (C) 2014 Michal Ratajsky <michal.ratajsky@gmail.com>
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

#ifndef __MSD_MEDIA_KEYS_PLUGIN_H__
#define __MSD_MEDIA_KEYS_PLUGIN_H__

#include <glib.h>
#include <glib-object.h>
#include <gmodule.h>

#include "mate-settings-plugin.h"

G_BEGIN_DECLS

#define MSD_TYPE_MEDIA_KEYS_PLUGIN                (msd_media_keys_plugin_get_type ())
#define MSD_MEDIA_KEYS_PLUGIN(o)                  (G_TYPE_CHECK_INSTANCE_CAST ((o), MSD_TYPE_MEDIA_KEYS_PLUGIN, MsdMediaKeysPlugin))
#define MSD_MEDIA_KEYS_PLUGIN_CLASS(k)            (G_TYPE_CHECK_CLASS_CAST((k), MSD_TYPE_MEDIA_KEYS_PLUGIN, MsdMediaKeysPluginClass))
#define MSD_IS_MEDIA_KEYS_PLUGIN(o)               (G_TYPE_CHECK_INSTANCE_TYPE ((o), MSD_TYPE_MEDIA_KEYS_PLUGIN))
#define MSD_IS_MEDIA_KEYS_PLUGIN_CLASS(k)         (G_TYPE_CHECK_CLASS_TYPE ((k), MSD_TYPE_MEDIA_KEYS_PLUGIN))
#define MSD_MEDIA_KEYS_PLUGIN_GET_CLASS(o)        (G_TYPE_INSTANCE_GET_CLASS ((o), MSD_TYPE_MEDIA_KEYS_PLUGIN, MsdMediaKeysPluginClass))

typedef struct _MsdMediaKeysPlugin         MsdMediaKeysPlugin;
typedef struct _MsdMediaKeysPluginClass    MsdMediaKeysPluginClass;
typedef struct _MsdMediaKeysPluginPrivate  MsdMediaKeysPluginPrivate;

struct _MsdMediaKeysPlugin
{
        MateSettingsPlugin          parent;
        MsdMediaKeysPluginPrivate  *priv;
};

struct _MsdMediaKeysPluginClass
{
        MateSettingsPluginClass     parent_class;
};

GType msd_media_keys_plugin_get_type (void) G_GNUC_CONST;

/* All the plugins must implement this function */
G_MODULE_EXPORT GType register_mate_settings_plugin (GTypeModule *module);

G_END_DECLS

#endif /* __MSD_MEDIA_KEYS_PLUGIN_H__ */
