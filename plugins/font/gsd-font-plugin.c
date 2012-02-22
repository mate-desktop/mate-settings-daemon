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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */

#include "config.h"

#include <glib/gi18n-lib.h>
#include <gmodule.h>

#include "mate-settings-plugin.h"
#include "msd-font-plugin.h"
#include "msd-font-manager.h"

struct MsdFontPluginPrivate {
        MsdFontManager *manager;
};

#define MSD_FONT_PLUGIN_GET_PRIVATE(object) (G_TYPE_INSTANCE_GET_PRIVATE ((object), MSD_TYPE_FONT_PLUGIN, MsdFontPluginPrivate))

MATE_SETTINGS_PLUGIN_REGISTER (MsdFontPlugin, msd_font_plugin)

static void
msd_font_plugin_init (MsdFontPlugin *plugin)
{
        plugin->priv = MSD_FONT_PLUGIN_GET_PRIVATE (plugin);

        g_debug ("MsdFontPlugin initializing");

        plugin->priv->manager = msd_font_manager_new ();
}

static void
msd_font_plugin_finalize (GObject *object)
{
        MsdFontPlugin *plugin;

        g_return_if_fail (object != NULL);
        g_return_if_fail (MSD_IS_FONT_PLUGIN (object));

        g_debug ("MsdFontPlugin finalizing");

        plugin = MSD_FONT_PLUGIN (object);

        g_return_if_fail (plugin->priv != NULL);

        if (plugin->priv->manager != NULL) {
                g_object_unref (plugin->priv->manager);
        }

        G_OBJECT_CLASS (msd_font_plugin_parent_class)->finalize (object);
}

static void
impl_activate (MateSettingsPlugin *plugin)
{
        gboolean res;
        GError  *error;

        g_debug ("Activating font plugin");

        error = NULL;
        res = msd_font_manager_start (MSD_FONT_PLUGIN (plugin)->priv->manager, &error);
        if (! res) {
                g_warning ("Unable to start font manager: %s", error->message);
                g_error_free (error);
        }
}

static void
impl_deactivate (MateSettingsPlugin *plugin)
{
        g_debug ("Deactivating font plugin");
        msd_font_manager_stop (MSD_FONT_PLUGIN (plugin)->priv->manager);
}

static void
msd_font_plugin_class_init (MsdFontPluginClass *klass)
{
        GObjectClass           *object_class = G_OBJECT_CLASS (klass);
        MateSettingsPluginClass *plugin_class = MATE_SETTINGS_PLUGIN_CLASS (klass);

        object_class->finalize = msd_font_plugin_finalize;

        plugin_class->activate = impl_activate;
        plugin_class->deactivate = impl_deactivate;

        g_type_class_add_private (klass, sizeof (MsdFontPluginPrivate));
}
