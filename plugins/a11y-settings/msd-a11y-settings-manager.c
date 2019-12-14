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

#include "config.h"

#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include <locale.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <gio/gio.h>

#include "mate-settings-profile.h"
#include "msd-a11y-settings-manager.h"

struct MsdA11ySettingsManagerPrivate
{
        GSettings *interface_settings;
        GSettings *a11y_apps_settings;
};

enum {
        PROP_0,
};

static void     msd_a11y_settings_manager_finalize    (GObject *object);

G_DEFINE_TYPE_WITH_PRIVATE (MsdA11ySettingsManager, msd_a11y_settings_manager, G_TYPE_OBJECT)

static gpointer manager_object = NULL;

static void
apps_settings_changed (GSettings              *settings,
		       const char             *key,
		       MsdA11ySettingsManager *manager)
{
	gboolean screen_reader, keyboard;

	if (g_str_equal (key, "screen-reader-enabled") == FALSE &&
	    g_str_equal (key, "screen-keyboard-enabled") == FALSE)
		return;

	g_debug ("screen reader or OSK enablement changed");

	screen_reader = g_settings_get_boolean (manager->priv->a11y_apps_settings, "screen-reader-enabled");
	keyboard = g_settings_get_boolean (manager->priv->a11y_apps_settings, "screen-keyboard-enabled");

	if (screen_reader || keyboard) {
		g_debug ("Enabling accessibility, screen reader or OSK enabled");
		g_settings_set_boolean (manager->priv->interface_settings, "accessibility", TRUE);
	} else if (screen_reader == FALSE && keyboard == FALSE) {
		g_debug ("Disabling accessibility, screen reader and OSK disabled");
		g_settings_set_boolean (manager->priv->interface_settings, "accessibility", FALSE);
	}
}

gboolean
msd_a11y_settings_manager_start (MsdA11ySettingsManager *manager,
                                 GError                **error)
{
        g_debug ("Starting a11y_settings manager");
        mate_settings_profile_start (NULL);

	manager->priv->interface_settings = g_settings_new ("org.mate.interface");
	manager->priv->a11y_apps_settings = g_settings_new ("org.gnome.desktop.a11y.applications");

	g_signal_connect (G_OBJECT (manager->priv->a11y_apps_settings), "changed",
			  G_CALLBACK (apps_settings_changed), manager);

	/* If any of the screen reader or on-screen keyboard are enabled,
	 * make sure a11y is enabled for the toolkits.
	 * We don't do the same thing for the reverse so it's possible to
	 * enable AT-SPI for the toolkits without using an a11y app */
	if (g_settings_get_boolean (manager->priv->a11y_apps_settings, "screen-keyboard-enabled") ||
	    g_settings_get_boolean (manager->priv->a11y_apps_settings, "screen-reader-enabled"))
		g_settings_set_boolean (manager->priv->interface_settings, "accessibility", TRUE);

        mate_settings_profile_end (NULL);
        return TRUE;
}

void
msd_a11y_settings_manager_stop (MsdA11ySettingsManager *manager)
{
	if (manager->priv->interface_settings) {
		g_object_unref (manager->priv->interface_settings);
		manager->priv->interface_settings = NULL;
	}
	if (manager->priv->a11y_apps_settings) {
		g_object_unref (manager->priv->a11y_apps_settings);
		manager->priv->a11y_apps_settings = NULL;
	}
        g_debug ("Stopping a11y_settings manager");
}

static GObject *
msd_a11y_settings_manager_constructor (GType                  type,
                                       guint                  n_construct_properties,
                                       GObjectConstructParam *construct_properties)
{
        MsdA11ySettingsManager      *a11y_settings_manager;

        a11y_settings_manager = MSD_A11Y_SETTINGS_MANAGER (G_OBJECT_CLASS (msd_a11y_settings_manager_parent_class)->constructor (type,
                                                                                                                                 n_construct_properties,
                                                                                                                                 construct_properties));

        return G_OBJECT (a11y_settings_manager);
}

static void
msd_a11y_settings_manager_dispose (GObject *object)
{
        G_OBJECT_CLASS (msd_a11y_settings_manager_parent_class)->dispose (object);
}

static void
msd_a11y_settings_manager_class_init (MsdA11ySettingsManagerClass *klass)
{
        GObjectClass   *object_class = G_OBJECT_CLASS (klass);

        object_class->constructor = msd_a11y_settings_manager_constructor;
        object_class->dispose = msd_a11y_settings_manager_dispose;
        object_class->finalize = msd_a11y_settings_manager_finalize;
}

static void
msd_a11y_settings_manager_init (MsdA11ySettingsManager *manager)
{
        manager->priv = msd_a11y_settings_manager_get_instance_private (manager);

}

static void
msd_a11y_settings_manager_finalize (GObject *object)
{
        MsdA11ySettingsManager *a11y_settings_manager;

        g_return_if_fail (object != NULL);
        g_return_if_fail (MSD_IS_A11Y_SETTINGS_MANAGER (object));

        a11y_settings_manager = MSD_A11Y_SETTINGS_MANAGER (object);

        g_return_if_fail (a11y_settings_manager->priv != NULL);

        G_OBJECT_CLASS (msd_a11y_settings_manager_parent_class)->finalize (object);
}

MsdA11ySettingsManager *
msd_a11y_settings_manager_new (void)
{
        if (manager_object != NULL) {
                g_object_ref (manager_object);
        } else {
                manager_object = g_object_new (MSD_TYPE_A11Y_SETTINGS_MANAGER, NULL);
                g_object_add_weak_pointer (manager_object,
                                           (gpointer *) &manager_object);
        }

        return MSD_A11Y_SETTINGS_MANAGER (manager_object);
}
