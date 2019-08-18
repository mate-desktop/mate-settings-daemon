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
#include "msd-typing-break-plugin.h"
#include "msd-typing-break-manager.h"

struct MsdTypingBreakPluginPrivate {
        MsdTypingBreakManager *manager;
};

MATE_SETTINGS_PLUGIN_REGISTER_WITH_PRIVATE (MsdTypingBreakPlugin, msd_typing_break_plugin)

static void
msd_typing_break_plugin_init (MsdTypingBreakPlugin *plugin)
{
        plugin->priv = msd_typing_break_plugin_get_instance_private (plugin);

        g_debug ("MsdTypingBreakPlugin initializing");

        plugin->priv->manager = msd_typing_break_manager_new ();
}

static void
msd_typing_break_plugin_finalize (GObject *object)
{
        MsdTypingBreakPlugin *plugin;

        g_return_if_fail (object != NULL);
        g_return_if_fail (MSD_IS_TYPING_BREAK_PLUGIN (object));

        g_debug ("MsdTypingBreakPlugin finalizing");

        plugin = MSD_TYPING_BREAK_PLUGIN (object);

        g_return_if_fail (plugin->priv != NULL);

        if (plugin->priv->manager != NULL) {
                g_object_unref (plugin->priv->manager);
        }

        G_OBJECT_CLASS (msd_typing_break_plugin_parent_class)->finalize (object);
}

static void
impl_activate (MateSettingsPlugin *plugin)
{
        gboolean res;
        GError  *error;

        g_debug ("Activating typing_break plugin");

        error = NULL;
        res = msd_typing_break_manager_start (MSD_TYPING_BREAK_PLUGIN (plugin)->priv->manager, &error);
        if (! res) {
                g_warning ("Unable to start typing_break manager: %s", error->message);
                g_error_free (error);
        }
}

static void
impl_deactivate (MateSettingsPlugin *plugin)
{
        g_debug ("Deactivating typing_break plugin");
        msd_typing_break_manager_stop (MSD_TYPING_BREAK_PLUGIN (plugin)->priv->manager);
}

static void
msd_typing_break_plugin_class_init (MsdTypingBreakPluginClass *klass)
{
        GObjectClass           *object_class = G_OBJECT_CLASS (klass);
        MateSettingsPluginClass *plugin_class = MATE_SETTINGS_PLUGIN_CLASS (klass);

        object_class->finalize = msd_typing_break_plugin_finalize;

        plugin_class->activate = impl_activate;
        plugin_class->deactivate = impl_deactivate;
}

static void
msd_typing_break_plugin_class_finalize (MsdTypingBreakPluginClass *klass)
{
}
