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

#include "config.h"

#include <glib/gi18n-lib.h>
#include <gmodule.h>

#include "mate-settings-plugin.h"
#include "msd-clipboard-plugin.h"
#include "msd-clipboard-manager.h"

struct MsdClipboardPluginPrivate {
        MsdClipboardManager *manager;
};

MATE_SETTINGS_PLUGIN_REGISTER_WITH_PRIVATE (MsdClipboardPlugin, msd_clipboard_plugin)

static void
msd_clipboard_plugin_init (MsdClipboardPlugin *plugin)
{
        plugin->priv = msd_clipboard_plugin_get_instance_private (plugin);

        g_debug ("MsdClipboardPlugin initializing");

        plugin->priv->manager = msd_clipboard_manager_new ();
}

static void
msd_clipboard_plugin_finalize (GObject *object)
{
        MsdClipboardPlugin *plugin;

        g_return_if_fail (object != NULL);
        g_return_if_fail (MSD_IS_CLIPBOARD_PLUGIN (object));

        g_debug ("MsdClipboardPlugin finalizing");

        plugin = MSD_CLIPBOARD_PLUGIN (object);

        g_return_if_fail (plugin->priv != NULL);

        if (plugin->priv->manager != NULL) {
                g_object_unref (plugin->priv->manager);
        }

        G_OBJECT_CLASS (msd_clipboard_plugin_parent_class)->finalize (object);
}

static void
impl_activate (MateSettingsPlugin *plugin)
{
        gboolean res;
        GError  *error;

        g_debug ("Activating clipboard plugin");

        error = NULL;
        res = msd_clipboard_manager_start (MSD_CLIPBOARD_PLUGIN (plugin)->priv->manager, &error);
        if (! res) {
                g_warning ("Unable to start clipboard manager: %s", error->message);
                g_error_free (error);
        }
}

static void
impl_deactivate (MateSettingsPlugin *plugin)
{
        g_debug ("Deactivating clipboard plugin");
        msd_clipboard_manager_stop (MSD_CLIPBOARD_PLUGIN (plugin)->priv->manager);
}

static void
msd_clipboard_plugin_class_init (MsdClipboardPluginClass *klass)
{
        GObjectClass           *object_class = G_OBJECT_CLASS (klass);
        MateSettingsPluginClass *plugin_class = MATE_SETTINGS_PLUGIN_CLASS (klass);

        object_class->finalize = msd_clipboard_plugin_finalize;

        plugin_class->activate = impl_activate;
        plugin_class->deactivate = impl_deactivate;
}

static void
msd_clipboard_plugin_class_finalize (MsdClipboardPluginClass *klass)
{
}

