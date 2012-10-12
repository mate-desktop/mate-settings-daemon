/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007 William Jon McCann <mccann@jhu.edu>
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

#ifndef __MSD_FONT_PLUGIN_H__
#define __MSD_FONT_PLUGIN_H__

#include <glib.h>
#include <glib-object.h>
#include <gmodule.h>

#include "mate-settings-plugin.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MSD_TYPE_FONT_PLUGIN                (msd_font_plugin_get_type ())
#define MSD_FONT_PLUGIN(o)                  (G_TYPE_CHECK_INSTANCE_CAST ((o), MSD_TYPE_FONT_PLUGIN, MsdFontPlugin))
#define MSD_FONT_PLUGIN_CLASS(k)            (G_TYPE_CHECK_CLASS_CAST((k), MSD_TYPE_FONT_PLUGIN, MsdFontPluginClass))
#define MSD_IS_FONT_PLUGIN(o)               (G_TYPE_CHECK_INSTANCE_TYPE ((o), MSD_TYPE_FONT_PLUGIN))
#define MSD_IS_FONT_PLUGIN_CLASS(k)         (G_TYPE_CHECK_CLASS_TYPE ((k), MSD_TYPE_FONT_PLUGIN))
#define MSD_FONT_PLUGIN_GET_CLASS(o)        (G_TYPE_INSTANCE_GET_CLASS ((o), MSD_TYPE_FONT_PLUGIN, MsdFontPluginClass))

typedef struct MsdFontPluginPrivate MsdFontPluginPrivate;

typedef struct
{
        MateSettingsPlugin    parent;
        MsdFontPluginPrivate *priv;
} MsdFontPlugin;

typedef struct
{
        MateSettingsPluginClass parent_class;
} MsdFontPluginClass;

GType   msd_font_plugin_get_type            (void) G_GNUC_CONST;

/* All the plugins must implement this function */
G_MODULE_EXPORT GType register_mate_settings_plugin (GTypeModule *module);

#ifdef __cplusplus
}
#endif

#endif /* __MSD_FONT_PLUGIN_H__ */
