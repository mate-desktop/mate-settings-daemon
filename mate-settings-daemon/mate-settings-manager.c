/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 * vim: set ts=8 sts=8 sw=8 expandtab:
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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <glib/gi18n.h>
#include <gio/gio.h>
#include <gtk/gtk.h>

#include "mate-settings-plugin-info.h"
#include "mate-settings-manager.h"
#include "manager-dbus-generated.h"
#include "mate-settings-profile.h"

#define MSD_MANAGER_DBUS_PATH "/org/mate/SettingsDaemon"

#define DEFAULT_SETTINGS_PREFIX "org.mate.SettingsDaemon"

#define PLUGIN_EXT ".mate-settings-plugin"

enum {
        PROP_0,
        PROP_REPLACE,
        PROP_INIT_FLAG,
        LAST_PROP
};

struct MateSettingsManagerPrivate
{
        GSList                     *plugins;
        gint                        init_load_priority;
        gint                        load_init_flag;
        OrgMateSettingsDaemon      *skeleton;
        guint                       bus_name_id;
        gboolean                    replace;
};

enum {
        PLUGIN_ACTIVATED,
        PLUGIN_DEACTIVATED,
        LAST_SIGNAL
};

static GParamSpec *properties[LAST_PROP] = { NULL, };
static guint signals [LAST_SIGNAL] = { 0, };

static void     mate_settings_manager_finalize (GObject *object);
gboolean mate_settings_manager_awake_handler (OrgMateSettingsDaemon *object,
                                              GDBusMethodInvocation *invocation,
                                              gpointer               user_data);
gboolean mate_settings_manager_start_handler (OrgMateSettingsDaemon *object,
                                              GDBusMethodInvocation *invocation,
                                              gpointer               user_data);

G_DEFINE_TYPE_WITH_PRIVATE (MateSettingsManager, mate_settings_manager, G_TYPE_OBJECT)

static gpointer manager_object = NULL;

GQuark
mate_settings_manager_error_quark (void)
{
        static GQuark ret = 0;
        if (ret == 0) {
                ret = g_quark_from_static_string ("mate_settings_manager_error");
        }

        return ret;
}

static void
maybe_activate_plugin (MateSettingsPluginInfo *info,
                       MateSettingsManager    *manager)
{
        if (mate_settings_plugin_info_get_enabled (info)) {
                int plugin_priority;
                plugin_priority = mate_settings_plugin_info_get_priority (info);

                if (manager->priv->load_init_flag == PLUGIN_LOAD_ALL ||
                   (manager->priv->load_init_flag == PLUGIN_LOAD_INIT && plugin_priority <= manager->priv->init_load_priority) ||
                   (manager->priv->load_init_flag == PLUGIN_LOAD_DEFER && plugin_priority > manager->priv->init_load_priority)) {
                        gboolean res;
                        res = mate_settings_plugin_info_activate (info);
                        if (res) {
                                g_debug ("Plugin %s: active", mate_settings_plugin_info_get_location (info));
                        } else {
                                g_debug ("Plugin %s: activation failed", mate_settings_plugin_info_get_location (info));
                        }
                } else {
                        g_debug ("Plugin %s: loading deferred or previously loaded", mate_settings_plugin_info_get_location (info));
                }
        } else {
                g_debug ("Plugin %s: inactive", mate_settings_plugin_info_get_location (info));
        }
}

static gint
compare_location (MateSettingsPluginInfo *a,
                  MateSettingsPluginInfo *b)
{
        const char *loc_a;
        const char *loc_b;

        loc_a = mate_settings_plugin_info_get_location (a);
        loc_b = mate_settings_plugin_info_get_location (b);

        if (loc_a == NULL || loc_b == NULL) {
                return -1;
        }

        return strcmp (loc_a, loc_b);
}

static int
compare_priority (MateSettingsPluginInfo *a,
                  MateSettingsPluginInfo *b)
{
        int prio_a;
        int prio_b;

        prio_a = mate_settings_plugin_info_get_priority (a);
        prio_b = mate_settings_plugin_info_get_priority (b);

        return prio_a - prio_b;
}

static void
on_plugin_activated (MateSettingsPluginInfo *info,
                     MateSettingsManager    *manager)
{
        const char *name;
        name = mate_settings_plugin_info_get_location (info);
        g_debug ("MateSettingsManager: emitting plugin-activated %s", name);
        g_signal_emit (manager, signals [PLUGIN_ACTIVATED], 0, name);
}

static void
on_plugin_deactivated (MateSettingsPluginInfo *info,
                       MateSettingsManager    *manager)
{
        const char *name;
        name = mate_settings_plugin_info_get_location (info);
        g_debug ("MateSettingsManager: emitting plugin-deactivated %s", name);
        g_signal_emit (manager, signals [PLUGIN_DEACTIVATED], 0, name);
}

static gboolean
is_item_in_schema (char       **items,
                   const char  *item)
{
	while (*items) {
	       if (g_strcmp0 (*items++, item) == 0)
		       return TRUE;
	}
	return FALSE;
}

static gboolean
is_schema (const char *schema)
{
        GSettingsSchemaSource *source = NULL;
        gchar **non_relocatable = NULL;
        gchar **relocatable = NULL;
        gboolean in_schema;

        source = g_settings_schema_source_get_default ();
        if (!source)
                return FALSE;

        g_settings_schema_source_list_schemas (source, TRUE, &non_relocatable, &relocatable);

        in_schema = (is_item_in_schema (non_relocatable, schema) ||
                is_item_in_schema (relocatable, schema));


        g_strfreev (non_relocatable);
        g_strfreev (relocatable);

        return in_schema;
}

static void
_load_file (MateSettingsManager *manager,
            const char           *filename)
{
        MateSettingsPluginInfo  *info;
        char                    *schema;
        GSList                  *l;

        g_debug ("Loading plugin: %s", filename);
        mate_settings_profile_start ("%s", filename);

        info = mate_settings_plugin_info_new_from_file (filename);
        if (info == NULL) {
                goto out;
        }

        l = g_slist_find_custom (manager->priv->plugins,
                                 info,
                                 (GCompareFunc) compare_location);
        if (l != NULL) {
                goto out;
        }

        schema = g_strdup_printf ("%s.plugins.%s",
                                  DEFAULT_SETTINGS_PREFIX,
                                  mate_settings_plugin_info_get_location (info));

	/* Ignore unknown schemas or else we'll assert */
	if (is_schema (schema)) {
	       manager->priv->plugins = g_slist_prepend (manager->priv->plugins,
		                                         g_object_ref (info));

	       g_signal_connect (info, "activated",
		                 G_CALLBACK (on_plugin_activated), manager);
	       g_signal_connect (info, "deactivated",
		                 G_CALLBACK (on_plugin_deactivated), manager);

	       /* Also sets priority for plugins */
	       mate_settings_plugin_info_set_schema (info, schema);
	} else {
	       g_warning ("Ignoring unknown module '%s'", schema);
	}

        g_free (schema);

 out:
        if (info != NULL) {
                g_object_unref (info);
        }

        mate_settings_profile_end ("%s", filename);
}

static void
_load_dir (MateSettingsManager *manager,
           const char           *path)
{
        GError     *error;
        GDir       *d;
        const char *name;

        g_debug ("Loading settings plugins from dir: %s", path);
        mate_settings_profile_start (NULL);

        error = NULL;
        d = g_dir_open (path, 0, &error);
        if (d == NULL) {
                g_warning ("%s", error->message);
                g_error_free (error);
                return;
        }

        while ((name = g_dir_read_name (d))) {
                char *filename;

                if (!g_str_has_suffix (name, PLUGIN_EXT)) {
                        continue;
                }

                filename = g_build_filename (path, name, NULL);
                if (g_file_test (filename, G_FILE_TEST_IS_REGULAR)) {
                        _load_file (manager, filename);
                }
                g_free (filename);
        }

        g_dir_close (d);

        mate_settings_profile_end (NULL);
}

gboolean
mate_settings_manager_load (MateSettingsManager *manager,
                            GError             **error)
{
        gboolean ret = FALSE;
        mate_settings_profile_start (NULL);
        if (!g_module_supported ()) {
                g_warning ("mate-settings-daemon is not able to initialize the plugins.");
                g_set_error (error,
                             MATE_SETTINGS_MANAGER_ERROR,
                             MATE_SETTINGS_MANAGER_ERROR_GENERAL,
                             "Plugins not supported");
                goto out;
        }

        /* load system plugins */
        _load_dir (manager, MATE_SETTINGS_PLUGINDIR G_DIR_SEPARATOR_S);

        manager->priv->plugins = g_slist_sort (manager->priv->plugins, (GCompareFunc) compare_priority);
        g_slist_foreach (manager->priv->plugins, (GFunc) maybe_activate_plugin, manager);
        ret = TRUE;
out:
        mate_settings_profile_end (NULL);
        return ret;
}

static void
_unload_plugin (MateSettingsPluginInfo *info,
                gpointer user_data G_GNUC_UNUSED)
{
        if (mate_settings_plugin_info_get_enabled (info)) {
                mate_settings_plugin_info_deactivate (info);
        }
        g_object_unref (info);
}

static void
_unload_all (MateSettingsManager *manager)
{
         g_slist_foreach (manager->priv->plugins, (GFunc) _unload_plugin, NULL);
         g_slist_free (manager->priv->plugins);
         manager->priv->plugins = NULL;
}

/*
  Example:
  dbus-send --session --dest=org.mate.SettingsDaemon \
  --type=method_call --print-reply --reply-timeout=2000 \
  /org/mate/SettingsDaemon \
  org.mate.SettingsDaemon.Awake
*/
gboolean
mate_settings_manager_awake_handler (OrgMateSettingsDaemon *object,
                                     GDBusMethodInvocation *invocation,
                                     gpointer               user_data)
{
        gboolean ret;
        GError *error = NULL;
        MateSettingsManager *manager;

        manager = MATE_SETTINGS_MANAGER (user_data);

        g_debug ("Awake called");
        mate_settings_manager_set_init_flag (manager, PLUGIN_LOAD_ALL);
        ret = mate_settings_manager_load (manager, &error);
        if (!ret) {
            g_dbus_method_invocation_return_gerror (invocation, error);
        } else {
            org_mate_settings_daemon_complete_awake (object, invocation);
        }
        return ret;
}

gboolean
mate_settings_manager_start_handler (OrgMateSettingsDaemon *object,
                                     GDBusMethodInvocation *invocation,
                                     gpointer               user_data)
{
        gboolean ret;
        GError *error = NULL;
        MateSettingsManager *manager;

        g_debug ("Starting settings manager");

        manager = MATE_SETTINGS_MANAGER (user_data);

        mate_settings_profile_start (NULL);

        ret = mate_settings_manager_load (manager, &error);

        if (!ret) {
                g_dbus_method_invocation_return_gerror (invocation, error);
        } else {
                org_mate_settings_daemon_complete_start (object, invocation);
        }

        mate_settings_profile_end (NULL);

        return ret;
}

void
mate_settings_manager_stop (MateSettingsManager *manager)
{
        g_debug ("Stopping settings manager");

        _unload_all (manager);
}

static void
mate_settings_manager_dispose (GObject *object)
{
        MateSettingsManager *manager;

        manager = MATE_SETTINGS_MANAGER (object);

        mate_settings_manager_stop (manager);

        G_OBJECT_CLASS (mate_settings_manager_parent_class)->dispose (object);
}

static void
bus_acquired_handler_cb (GDBusConnection *connection,
                         const gchar     *name G_GNUC_UNUSED,
                         gpointer         user_data)
{
        MateSettingsManager *manager;

        GError *error = NULL;
        gboolean exported;

        manager = MATE_SETTINGS_MANAGER (user_data);

        g_signal_connect (manager->priv->skeleton,
                          "handle-awake",
                          G_CALLBACK (mate_settings_manager_awake_handler),
                          manager);
        g_signal_connect (manager->priv->skeleton,
                          "handle-start",
                          G_CALLBACK (mate_settings_manager_start_handler),
                          manager);

        exported = g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (manager->priv->skeleton),
                                                     connection,
                                                     MSD_MANAGER_DBUS_PATH,
                                                     &error);
        if (!exported)
        {
                g_warning ("Failed to export interface: %s", error->message);
                g_error_free (error);

                gtk_main_quit ();
        }
}

static void
name_lost_handler_cb (GDBusConnection *connection G_GNUC_UNUSED,
                      const gchar     *name G_GNUC_UNUSED,
                      gpointer         user_data)
{
        MateSettingsManager *manager;
        g_debug ("msd bus name lost\n");

        manager = MATE_SETTINGS_MANAGER (user_data);
        mate_settings_manager_stop (manager);
        gtk_main_quit ();
}


static void
mate_settings_manager_constructed (GObject *object)
{
        MateSettingsManager *manager;
        GBusNameOwnerFlags flags;

        manager = MATE_SETTINGS_MANAGER (object);

        G_OBJECT_CLASS (mate_settings_manager_parent_class)->constructed (object);

        flags = G_BUS_NAME_OWNER_FLAGS_ALLOW_REPLACEMENT;
        if (manager->priv->replace)
                flags |= G_BUS_NAME_OWNER_FLAGS_REPLACE;

        manager->priv->bus_name_id = g_bus_own_name (G_BUS_TYPE_SESSION,
                                                     DEFAULT_SETTINGS_PREFIX,
                                                     flags,
                                                     bus_acquired_handler_cb, NULL,
                                                     name_lost_handler_cb, manager, NULL);
}

void
mate_settings_manager_set_init_flag (MateSettingsManager *manager,
                                     gint                 load_init_flag)
{
        manager->priv->load_init_flag = load_init_flag;
}

static void
mate_settings_manager_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
        MateSettingsManager *manager;

        manager = MATE_SETTINGS_MANAGER (object);

        switch (prop_id)
        {
                case PROP_INIT_FLAG:
                        mate_settings_manager_set_init_flag (manager, g_value_get_int (value));
                        break;
                case PROP_REPLACE:
                        manager->priv->replace = g_value_get_boolean (value);
                        break;
                default:
                        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                        break;
        }
}


static void
mate_settings_manager_class_init (MateSettingsManagerClass *klass)
{
        GObjectClass   *object_class = G_OBJECT_CLASS (klass);

        object_class->constructed = mate_settings_manager_constructed;
        object_class->set_property = mate_settings_manager_set_property;

        object_class->dispose = mate_settings_manager_dispose;
        object_class->finalize = mate_settings_manager_finalize;

        properties[PROP_REPLACE] =
                g_param_spec_boolean ("replace",
                                      "replace",
                                      "replace",
                                      FALSE,
                                      G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE |
                                      G_PARAM_STATIC_STRINGS);

        properties[PROP_INIT_FLAG] =
                g_param_spec_int ("init-flag",
                                  "load init flag",
                                  "load init flag",
                                  PLUGIN_LOAD_ALL,
                                  PLUGIN_LOAD_DEFER,
                                  PLUGIN_LOAD_ALL,
                                  G_PARAM_WRITABLE);

        g_object_class_install_properties (object_class, LAST_PROP, properties);

        signals [PLUGIN_ACTIVATED] =
                g_signal_new ("plugin-activated",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (MateSettingsManagerClass, plugin_activated),
                              NULL,
                              NULL,
                              g_cclosure_marshal_VOID__STRING,
                              G_TYPE_NONE,
                              1, G_TYPE_STRING);
        signals [PLUGIN_DEACTIVATED] =
                g_signal_new ("plugin-deactivated",
                              G_TYPE_FROM_CLASS (object_class),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (MateSettingsManagerClass, plugin_deactivated),
                              NULL,
                              NULL,
                              g_cclosure_marshal_VOID__STRING,
                              G_TYPE_NONE,
                              1, G_TYPE_STRING);
}

static void
mate_settings_manager_init (MateSettingsManager *manager)
{
        char      *schema;
        GSettings *settings;

        manager->priv = mate_settings_manager_get_instance_private (manager);

        schema = g_strdup_printf ("%s.plugins", DEFAULT_SETTINGS_PREFIX);
        if (is_schema (schema)) {
                settings = g_settings_new (schema);
                manager->priv->init_load_priority = g_settings_get_int (settings, "init-load-priority");
        }
        manager->priv->load_init_flag = PLUGIN_LOAD_ALL;
        manager->priv->skeleton = org_mate_settings_daemon_skeleton_new ();
}

static void
mate_settings_manager_finalize (GObject *object)
{
        MateSettingsManager *manager;

        g_return_if_fail (object != NULL);
        g_return_if_fail (MATE_IS_SETTINGS_MANAGER (object));

        manager = MATE_SETTINGS_MANAGER (object);

        if (manager->priv->skeleton != NULL)
        {
                GDBusInterfaceSkeleton *skeleton;

                skeleton = G_DBUS_INTERFACE_SKELETON (manager->priv->skeleton);
                g_dbus_interface_skeleton_unexport (skeleton);
                g_clear_object (&manager->priv->skeleton);
        }

        if (manager->priv->bus_name_id > 0)
        {
                g_bus_unown_name (manager->priv->bus_name_id);
                manager->priv->bus_name_id = 0;
        }

        g_return_if_fail (manager->priv != NULL);

        G_OBJECT_CLASS (mate_settings_manager_parent_class)->finalize (object);
}

MateSettingsManager *
mate_settings_manager_new (gboolean replace)
{
        if (manager_object != NULL) {
                g_object_ref (manager_object);
        } else {
                manager_object = g_object_new (MATE_TYPE_SETTINGS_MANAGER,
                                               "replace",
                                               replace,
                                               NULL);
        }

        return MATE_SETTINGS_MANAGER (manager_object);
}
