/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010 Red Hat, Inc.
 * Copyright (C) 2012-2021 MATE Developers
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

#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>

#include "mate-settings-plugin.h"
#include "msd-smartcard-plugin.h"
#include "msd-smartcard-manager.h"

struct MsdSmartcardPluginPrivate {
        MsdSmartcardManager *manager;
        GDBusProxy          *screensaver_proxy;
        guint32              is_active : 1;
};

typedef enum
{
    MSD_SMARTCARD_REMOVE_ACTION_NONE,
    MSD_SMARTCARD_REMOVE_ACTION_LOCK_SCREEN,
    MSD_SMARTCARD_REMOVE_ACTION_FORCE_LOGOUT,
} MsdSmartcardRemoveAction;

#define SCREENSAVER_DBUS_NAME      "org.mate.ScreenSaver"
#define SCREENSAVER_DBUS_PATH      "/"
#define SCREENSAVER_DBUS_INTERFACE "org.mate.ScreenSaver"

#define SM_DBUS_NAME      "org.gnome.SessionManager"
#define SM_DBUS_PATH      "/org/gnome/SessionManager"
#define SM_DBUS_INTERFACE "org.gnome.SessionManager"
#define SM_LOGOUT_MODE_FORCE 2

#define MSD_SMARTCARD_SCHEMA "org.mate.peripherals-smartcard"
#define KEY_REMOVE_ACTION "removal-action"

MATE_SETTINGS_PLUGIN_REGISTER_WITH_PRIVATE (MsdSmartcardPlugin, msd_smartcard_plugin);

static void
simulate_user_activity (MsdSmartcardPlugin *plugin)
{
        GError     *error = NULL;
        GVariant   *ret;

        g_debug ("MsdSmartcardPlugin telling screensaver about smart card insertion");
        ret = g_dbus_proxy_call_sync (plugin->priv->screensaver_proxy,
                                     "SimulateUserActivity",
                                      g_variant_new ("()"),
                                      G_DBUS_CALL_FLAGS_NONE,
                                      -1,
                                      NULL,
                                      &error);
        if (ret == NULL) {
                g_warning ("MsdSmartcardPlugin Unable to force logout: %s", error->message);
                g_error_free (error);
        } else {
                g_variant_unref (ret);
        }
}

static void
lock_screen (MsdSmartcardPlugin *plugin)
{
        GError     *error = NULL;
        GVariant   *ret;

        g_debug ("MsdSmartcardPlugin telling screensaver to lock screen");

        ret = g_dbus_proxy_call_sync (plugin->priv->screensaver_proxy,
                                     "Lock",
                                      g_variant_new ("()"),
                                      G_DBUS_CALL_FLAGS_NONE,
                                      -1,
                                      NULL,
                                      &error);
        if (ret == NULL) {
                g_warning ("MsdSmartcardPlugin Unable to force logout: %s", error->message);
                g_error_free (error);
        } else {
                g_variant_unref (ret);
        }
}

static void
force_logout (MsdSmartcardPlugin *plugin)
{
        GDBusProxy *sm_proxy;
        GError     *error = NULL;
        GVariant   *ret;

        g_debug ("MsdSmartcardPlugin telling session manager to force logout");
        sm_proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
                                                  G_DBUS_PROXY_FLAGS_NONE,
                                                  NULL,
                                                  SM_DBUS_NAME,
                                                  SM_DBUS_PATH,
                                                  SM_DBUS_INTERFACE,
                                                  NULL,
                                                  &error);
        if (sm_proxy == NULL) {
                g_warning ("Unable to contact session manager daemon: %s\n", error->message);
                g_error_free (error);
                return;
        }

        ret = g_dbus_proxy_call_sync (sm_proxy,
                                      "Logout",
                                      g_variant_new ("(u", SM_LOGOUT_MODE_FORCE),
                                      G_DBUS_CALL_FLAGS_NONE,
                                      -1,
                                      NULL,
                                      &error);
        if (ret == NULL) {
                g_warning ("MsdSmartcardPlugin Unable to force logout: %s", error->message);
                g_error_free (error);
        } else {
                g_variant_unref (ret);
        }

        g_object_unref (sm_proxy);
}

static void
msd_smartcard_plugin_init (MsdSmartcardPlugin *plugin)
{
        plugin->priv = msd_smartcard_plugin_get_instance_private (plugin);

        g_debug ("MsdSmartcardPlugin initializing");

        plugin->priv->manager = msd_smartcard_manager_new (NULL);
}

static void
msd_smartcard_plugin_finalize (GObject *object)
{
        MsdSmartcardPlugin *plugin;

        g_return_if_fail (object != NULL);
        g_return_if_fail (MSD_IS_SMARTCARD_PLUGIN (object));

        g_debug ("MsdSmartcardPlugin finalizing");

        plugin = MSD_SMARTCARD_PLUGIN (object);

        g_return_if_fail (plugin->priv != NULL);

        if (plugin->priv->manager != NULL) {
                g_object_unref (plugin->priv->manager);
        }

        G_OBJECT_CLASS (msd_smartcard_plugin_parent_class)->finalize (object);
}

static void
smartcard_inserted_cb (MsdSmartcardManager *card_monitor G_GNUC_UNUSED,
                       MsdSmartcard        *card,
                       MsdSmartcardPlugin  *plugin)
{
        char *name;

        name = msd_smartcard_get_name (card);
        g_debug ("MsdSmartcardPlugin smart card '%s' inserted", name);
        g_free (name);

        simulate_user_activity (plugin);
}

static gboolean
user_logged_in_with_smartcard (void)
{
    return g_getenv ("PKCS11_LOGIN_TOKEN_NAME") != NULL;
}

static MsdSmartcardRemoveAction
get_configured_remove_action (MsdSmartcardPlugin *plugin)
{
        GSettings *settings;
        char *remove_action_string;
        MsdSmartcardRemoveAction remove_action;

        settings = g_settings_new (MSD_SMARTCARD_SCHEMA);
        remove_action_string = g_settings_get_string (settings,
                                                      KEY_REMOVE_ACTION);

        if (remove_action_string == NULL) {
                g_warning ("MsdSmartcardPlugin unable to get smartcard remove action");
                remove_action = MSD_SMARTCARD_REMOVE_ACTION_NONE;
        } else if (strcmp (remove_action_string, "none") == 0) {
                remove_action = MSD_SMARTCARD_REMOVE_ACTION_NONE;
        } else if (strcmp (remove_action_string, "lock_screen") == 0) {
                remove_action = MSD_SMARTCARD_REMOVE_ACTION_LOCK_SCREEN;
        } else if (strcmp (remove_action_string, "force_logout") == 0) {
                remove_action = MSD_SMARTCARD_REMOVE_ACTION_FORCE_LOGOUT;
        } else {
                g_warning ("MsdSmartcardPlugin unknown smartcard remove action");
                remove_action = MSD_SMARTCARD_REMOVE_ACTION_NONE;
        }

        g_object_unref (settings);

        return remove_action;
}

static void
process_smartcard_removal (MsdSmartcardPlugin *plugin)
{
        MsdSmartcardRemoveAction remove_action;

        g_debug ("MsdSmartcardPlugin processing smartcard removal");
        remove_action = get_configured_remove_action (plugin);

        switch (remove_action)
        {
            case MSD_SMARTCARD_REMOVE_ACTION_NONE:
                return;
            case MSD_SMARTCARD_REMOVE_ACTION_LOCK_SCREEN:
                lock_screen (plugin);
                break;
            case MSD_SMARTCARD_REMOVE_ACTION_FORCE_LOGOUT:
                force_logout (plugin);
                break;
        }
}

static void
smartcard_removed_cb (MsdSmartcardManager *card_monitor G_GNUC_UNUSED,
                      MsdSmartcard        *card,
                      MsdSmartcardPlugin  *plugin)
{

        char *name;

        name = msd_smartcard_get_name (card);
        g_debug ("MsdSmartcardPlugin smart card '%s' removed", name);
        g_free (name);

        if (!msd_smartcard_is_login_card (card)) {
                g_debug ("MsdSmartcardPlugin removed smart card was not used to login");
                return;
        }

        process_smartcard_removal (plugin);
}

static void
impl_activate (MateSettingsPlugin *plugin)
{
        GError *error;
        MsdSmartcardPlugin *smartcard_plugin = MSD_SMARTCARD_PLUGIN (plugin);

        if (smartcard_plugin->priv->is_active) {
                g_debug ("MsdSmartcardPlugin Not activating smartcard plugin, because it's "
                         "already active");
                return;
        }

        if (!user_logged_in_with_smartcard ()) {
                g_debug ("MsdSmartcardPlugin Not activating smartcard plugin, because user didn't use "
                         " smartcard to log in");
                smartcard_plugin->priv->is_active = FALSE;
                return;
        }

        g_debug ("MsdSmartcardPlugin Activating smartcard plugin");

        error = NULL;
        smartcard_plugin->priv->screensaver_proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
                                                                                   G_DBUS_PROXY_FLAGS_NONE,
                                                                                   NULL,
                                                                                   SCREENSAVER_DBUS_NAME,
                                                                                   SCREENSAVER_DBUS_PATH,
                                                                                   SCREENSAVER_DBUS_INTERFACE,
                                                                                   NULL,
                                                                                   &error);

        if (smartcard_plugin->priv->screensaver_proxy == NULL) {
                g_warning ("MsdSmartcardPlugin Unable to connect to session bus: %s", error->message);
                g_error_free (error);
                return;
        }

        if (!msd_smartcard_manager_start (smartcard_plugin->priv->manager, &error)) {
                g_warning ("MsdSmartcardPlugin Unable to start smartcard manager: %s", error->message);
                g_error_free (error);
        }

        g_signal_connect (smartcard_plugin->priv->manager,
                          "smartcard-removed",
                          G_CALLBACK (smartcard_removed_cb), smartcard_plugin);

        g_signal_connect (smartcard_plugin->priv->manager,
                          "smartcard-inserted",
                          G_CALLBACK (smartcard_inserted_cb), smartcard_plugin);

        if (!msd_smartcard_manager_login_card_is_inserted (smartcard_plugin->priv->manager)) {
                g_debug ("MsdSmartcardPlugin processing smartcard removal immediately user logged in with smartcard "
                         "and it's not inserted");
                process_smartcard_removal (smartcard_plugin);
        }

        smartcard_plugin->priv->is_active = TRUE;
}

static void
impl_deactivate (MateSettingsPlugin *plugin)
{
        MsdSmartcardPlugin *smartcard_plugin = MSD_SMARTCARD_PLUGIN (plugin);

        if (!smartcard_plugin->priv->is_active) {
                g_debug ("MsdSmartcardPlugin Not deactivating smartcard plugin, "
                         "because it's already inactive");
                return;
        }

        g_debug ("MsdSmartcardPlugin Deactivating smartcard plugin");

        msd_smartcard_manager_stop (smartcard_plugin->priv->manager);

        g_signal_handlers_disconnect_by_func (smartcard_plugin->priv->manager,
                                              smartcard_removed_cb, smartcard_plugin);

        g_signal_handlers_disconnect_by_func (smartcard_plugin->priv->manager,
                                              smartcard_inserted_cb, smartcard_plugin);
        if (smartcard_plugin->priv->screensaver_proxy != NULL)
                g_object_unref (smartcard_plugin->priv->screensaver_proxy);
        smartcard_plugin->priv->is_active = FALSE;
}

static void
msd_smartcard_plugin_class_init (MsdSmartcardPluginClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        MateSettingsPluginClass *plugin_class = MATE_SETTINGS_PLUGIN_CLASS (klass);

        object_class->finalize = msd_smartcard_plugin_finalize;

        plugin_class->activate = impl_activate;
        plugin_class->deactivate = impl_deactivate;
}

static void
msd_smartcard_plugin_class_finalize (MsdSmartcardPluginClass *klass)
{
}

