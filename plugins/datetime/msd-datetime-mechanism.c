/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007 David Zeuthen <david@fubar.dk>
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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <errno.h>
#include <sys/time.h>

#include <glib.h>
#include <glib-object.h>

#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>

#include <polkit/polkit.h>

#include "system-timezone.h"

#include "msd-datetime-mechanism.h"
#include "msd-datetime-mechanism-glue.h"

static gboolean
do_exit (gpointer user_data)
{
        g_debug ("Exiting due to inactivity");
        exit (1);
        return FALSE;
}

static void
reset_killtimer (void)
{
        static guint timer_id = 0;

        if (timer_id > 0) {
                g_source_remove (timer_id);
        }
        g_debug ("Setting killtimer to 30 seconds...");
        timer_id = g_timeout_add_seconds (30, do_exit, NULL);
}

struct MsdDatetimeMechanismPrivate
{
        DBusGConnection *system_bus_connection;
        DBusGProxy      *system_bus_proxy;
        PolkitAuthority *auth;
};

static void     msd_datetime_mechanism_finalize    (GObject     *object);

G_DEFINE_TYPE (MsdDatetimeMechanism, msd_datetime_mechanism, G_TYPE_OBJECT)

#define MSD_DATETIME_MECHANISM_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), MSD_DATETIME_TYPE_MECHANISM, MsdDatetimeMechanismPrivate))

GQuark
msd_datetime_mechanism_error_quark (void)
{
        static GQuark ret = 0;

        if (ret == 0) {
                ret = g_quark_from_static_string ("msd_datetime_mechanism_error");
        }

        return ret;
}


#define ENUM_ENTRY(NAME, DESC) { NAME, "" #NAME "", DESC }

GType
msd_datetime_mechanism_error_get_type (void)
{
        static GType etype = 0;
        
        if (etype == 0)
        {
                static const GEnumValue values[] =
                        {
                                ENUM_ENTRY (MSD_DATETIME_MECHANISM_ERROR_GENERAL, "GeneralError"),
                                ENUM_ENTRY (MSD_DATETIME_MECHANISM_ERROR_NOT_PRIVILEGED, "NotPrivileged"),
                                ENUM_ENTRY (MSD_DATETIME_MECHANISM_ERROR_INVALID_TIMEZONE_FILE, "InvalidTimezoneFile"),
                                { 0, 0, 0 }
                        };
                
                g_assert (MSD_DATETIME_MECHANISM_NUM_ERRORS == G_N_ELEMENTS (values) - 1);
                
                etype = g_enum_register_static ("MsdDatetimeMechanismError", values);
        }
        
        return etype;
}


static GObject *
msd_datetime_mechanism_constructor (GType                  type,
                                    guint                  n_construct_properties,
                                    GObjectConstructParam *construct_properties)
{
        MsdDatetimeMechanism      *mechanism;

        mechanism = MSD_DATETIME_MECHANISM (G_OBJECT_CLASS (msd_datetime_mechanism_parent_class)->constructor (
                                                type,
                                                n_construct_properties,
                                                construct_properties));

        return G_OBJECT (mechanism);
}

static void
msd_datetime_mechanism_class_init (MsdDatetimeMechanismClass *klass)
{
        GObjectClass   *object_class = G_OBJECT_CLASS (klass);

        object_class->constructor = msd_datetime_mechanism_constructor;
        object_class->finalize = msd_datetime_mechanism_finalize;

        g_type_class_add_private (klass, sizeof (MsdDatetimeMechanismPrivate));

        dbus_g_object_type_install_info (MSD_DATETIME_TYPE_MECHANISM, &dbus_glib_msd_datetime_mechanism_object_info);

        dbus_g_error_domain_register (MSD_DATETIME_MECHANISM_ERROR, NULL, MSD_DATETIME_MECHANISM_TYPE_ERROR);

}

static void
msd_datetime_mechanism_init (MsdDatetimeMechanism *mechanism)
{
        mechanism->priv = MSD_DATETIME_MECHANISM_GET_PRIVATE (mechanism);

}

static void
msd_datetime_mechanism_finalize (GObject *object)
{
        MsdDatetimeMechanism *mechanism;

        g_return_if_fail (object != NULL);
        g_return_if_fail (MSD_DATETIME_IS_MECHANISM (object));

        mechanism = MSD_DATETIME_MECHANISM (object);

        g_return_if_fail (mechanism->priv != NULL);

        g_object_unref (mechanism->priv->system_bus_proxy);

        G_OBJECT_CLASS (msd_datetime_mechanism_parent_class)->finalize (object);
}

static gboolean
register_mechanism (MsdDatetimeMechanism *mechanism)
{
        GError *error = NULL;

        mechanism->priv->auth = polkit_authority_get_sync (NULL, &error);
        if (mechanism->priv->auth == NULL) {
                if (error != NULL) {
                        g_critical ("error getting system bus: %s", error->message);
                        g_error_free (error);
                }
                goto error;
        }

        mechanism->priv->system_bus_connection = dbus_g_bus_get (DBUS_BUS_SYSTEM, &error);
        if (mechanism->priv->system_bus_connection == NULL) {
                if (error != NULL) {
                        g_critical ("error getting system bus: %s", error->message);
                        g_error_free (error);
                }
                goto error;
        }

        dbus_g_connection_register_g_object (mechanism->priv->system_bus_connection, "/", 
                                             G_OBJECT (mechanism));

        mechanism->priv->system_bus_proxy = dbus_g_proxy_new_for_name (mechanism->priv->system_bus_connection,
                                                                      DBUS_SERVICE_DBUS,
                                                                      DBUS_PATH_DBUS,
                                                                      DBUS_INTERFACE_DBUS);

        reset_killtimer ();

        return TRUE;

error:
        return FALSE;
}


MsdDatetimeMechanism *
msd_datetime_mechanism_new (void)
{
        GObject *object;
        gboolean res;

        object = g_object_new (MSD_DATETIME_TYPE_MECHANISM, NULL);

        res = register_mechanism (MSD_DATETIME_MECHANISM (object));
        if (! res) {
                g_object_unref (object);
                return NULL;
        }

        return MSD_DATETIME_MECHANISM (object);
}

static gboolean
_check_polkit_for_action (MsdDatetimeMechanism *mechanism, DBusGMethodInvocation *context, const char *action)
{
        const char *sender;
        GError *error;
        PolkitSubject *subject;
        PolkitAuthorizationResult *result;

        error = NULL;

        /* Check that caller is privileged */
        sender = dbus_g_method_get_sender (context);
        subject = polkit_system_bus_name_new (sender);

        result = polkit_authority_check_authorization_sync (mechanism->priv->auth,
                                                            subject,
                                                            action,
                                                            NULL,
                                                            POLKIT_CHECK_AUTHORIZATION_FLAGS_ALLOW_USER_INTERACTION,
                                                            NULL, &error);
        g_object_unref (subject);

        if (error) {
                dbus_g_method_return_error (context, error);
                g_error_free (error);

                return FALSE;
        }

        if (!polkit_authorization_result_get_is_authorized (result)) {
                error = g_error_new (MSD_DATETIME_MECHANISM_ERROR,
                                     MSD_DATETIME_MECHANISM_ERROR_NOT_PRIVILEGED,
                                     "Not Authorized for action %s", action);
                dbus_g_method_return_error (context, error);
                g_error_free (error);
                g_object_unref (result);

                return FALSE;
        }

        g_object_unref (result);

        return TRUE;
}


static gboolean
_set_time (MsdDatetimeMechanism  *mechanism,
           const struct timeval  *tv,
           DBusGMethodInvocation *context)
{
        GError *error;

        if (!_check_polkit_for_action (mechanism, context, "org.mate.settingsdaemon.datetimemechanism.settime"))
                return FALSE;

        if (settimeofday (tv, NULL) != 0) {
                error = g_error_new (MSD_DATETIME_MECHANISM_ERROR,
                                     MSD_DATETIME_MECHANISM_ERROR_GENERAL,
                                     "Error calling settimeofday({%lld,%lld}): %s", 
                                     (gint64) tv->tv_sec, (gint64) tv->tv_usec,
                                     strerror (errno));
                dbus_g_method_return_error (context, error);
                g_error_free (error);
                return FALSE;
        }

        if (g_file_test ("/sbin/hwclock", 
                         G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR | G_FILE_TEST_IS_EXECUTABLE)) {
                int exit_status;
                if (!g_spawn_command_line_sync ("/sbin/hwclock --systohc", NULL, NULL, &exit_status, &error)) {
                        GError *error2;
                        error2 = g_error_new (MSD_DATETIME_MECHANISM_ERROR,
                                              MSD_DATETIME_MECHANISM_ERROR_GENERAL,
                                              "Error spawning /sbin/hwclock: %s", error->message);
                        g_error_free (error);
                        dbus_g_method_return_error (context, error2);
                        g_error_free (error2);
                        return FALSE;
                }
                if (WEXITSTATUS (exit_status) != 0) {
                        error = g_error_new (MSD_DATETIME_MECHANISM_ERROR,
                                             MSD_DATETIME_MECHANISM_ERROR_GENERAL,
                                             "/sbin/hwclock returned %d", exit_status);
                        dbus_g_method_return_error (context, error);
                        g_error_free (error);
                        return FALSE;
                }
        }

        dbus_g_method_return (context);
        return TRUE;
}

static gboolean
_rh_update_etc_sysconfig_clock (DBusGMethodInvocation *context, const char *key, const char *value)
{
        /* On Red Hat / Fedora, the /etc/sysconfig/clock file needs to be kept in sync */
        if (g_file_test ("/etc/sysconfig/clock", G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR)) {
                char **lines;
                int n;
                gboolean replaced;
                char *data;
                gsize len;
                GError *error;
                
                error = NULL;

                if (!g_file_get_contents ("/etc/sysconfig/clock", &data, &len, &error)) {
                        GError *error2;
                        error2 = g_error_new (MSD_DATETIME_MECHANISM_ERROR,
                                              MSD_DATETIME_MECHANISM_ERROR_GENERAL,
                                              "Error reading /etc/sysconfig/clock file: %s", error->message);
                        g_error_free (error);
                        dbus_g_method_return_error (context, error2);
                        g_error_free (error2);
                        return FALSE;
                }
                replaced = FALSE;
                lines = g_strsplit (data, "\n", 0);
                g_free (data);

                for (n = 0; lines[n] != NULL; n++) {
                        if (g_str_has_prefix (lines[n], key)) {
                                g_free (lines[n]);
                                lines[n] = g_strdup_printf ("%s%s", key, value);
                                replaced = TRUE;
                        }
                }
                if (replaced) {
                        GString *str;

                        str = g_string_new (NULL);
                        for (n = 0; lines[n] != NULL; n++) {
                                g_string_append (str, lines[n]);
                                if (lines[n + 1] != NULL)
                                        g_string_append_c (str, '\n');
                        }
                        data = g_string_free (str, FALSE);
                        len = strlen (data);
                        if (!g_file_set_contents ("/etc/sysconfig/clock", data, len, &error)) {
                                GError *error2;
                                error2 = g_error_new (MSD_DATETIME_MECHANISM_ERROR,
                                                      MSD_DATETIME_MECHANISM_ERROR_GENERAL,
                                                      "Error updating /etc/sysconfig/clock: %s", error->message);
                                g_error_free (error);
                                dbus_g_method_return_error (context, error2);
                                g_error_free (error2);
                                g_free (data);
                                return FALSE;
                        }
                        g_free (data);
                }
                g_strfreev (lines);
        }

        return TRUE;
}

/* exported methods */

gboolean
msd_datetime_mechanism_set_time (MsdDatetimeMechanism  *mechanism,
                                 gint64                 seconds_since_epoch,
                                 DBusGMethodInvocation *context)
{
        struct timeval tv;

        reset_killtimer ();
        g_debug ("SetTime(%lld) called", seconds_since_epoch);

        tv.tv_sec = (time_t) seconds_since_epoch;
        tv.tv_usec = 0;
        return _set_time (mechanism, &tv, context);
}

gboolean
msd_datetime_mechanism_adjust_time (MsdDatetimeMechanism  *mechanism,
                                    gint64                 seconds_to_add,
                                    DBusGMethodInvocation *context)
{
        struct timeval tv;

        reset_killtimer ();
        g_debug ("AdjustTime(%lld) called", seconds_to_add);

        if (gettimeofday (&tv, NULL) != 0) {
                GError *error;
                error = g_error_new (MSD_DATETIME_MECHANISM_ERROR,
                                     MSD_DATETIME_MECHANISM_ERROR_GENERAL,
                                     "Error calling gettimeofday(): %s", strerror (errno));
                dbus_g_method_return_error (context, error);
                g_error_free (error);
                return FALSE;
        }

        tv.tv_sec += (time_t) seconds_to_add;
        return _set_time (mechanism, &tv, context);        
}


gboolean
msd_datetime_mechanism_set_timezone (MsdDatetimeMechanism  *mechanism,
                                     const char            *zone_file,
                                     DBusGMethodInvocation *context)
{
        GError *error;

        reset_killtimer ();
        g_debug ("SetTimezone('%s') called", zone_file);

        if (!_check_polkit_for_action (mechanism, context, "org.mate.settingsdaemon.datetimemechanism.settimezone"))
                return FALSE;

        error = NULL;

        if (!system_timezone_set_from_file (zone_file, &error)) {
                GError *error2;
                int     code;

                if (error->code == SYSTEM_TIMEZONE_ERROR_INVALID_TIMEZONE_FILE)
                        code = MSD_DATETIME_MECHANISM_ERROR_INVALID_TIMEZONE_FILE;
                else 
                        code = MSD_DATETIME_MECHANISM_ERROR_GENERAL;

                error2 = g_error_new (MSD_DATETIME_MECHANISM_ERROR,
                                      code, "%s", error->message);

                g_error_free (error);

                dbus_g_method_return_error (context, error2);
                g_error_free (error2);

                return FALSE;
        }

        dbus_g_method_return (context);
        return TRUE;
}


gboolean
msd_datetime_mechanism_get_timezone (MsdDatetimeMechanism   *mechism,
                                     DBusGMethodInvocation  *context)
{
  gchar *timezone;

  reset_killtimer ();

  timezone = system_timezone_find ();

  dbus_g_method_return (context, timezone);

  return TRUE;
}

gboolean
msd_datetime_mechanism_get_hardware_clock_using_utc (MsdDatetimeMechanism  *mechanism,
                                                     DBusGMethodInvocation *context)
{
        char **lines;
        char *data;
        gsize len;
        GError *error;
        gboolean is_utc;

        error = NULL;

        if (!g_file_get_contents ("/etc/adjtime", &data, &len, &error)) {
                GError *error2;
                error2 = g_error_new (MSD_DATETIME_MECHANISM_ERROR,
                                      MSD_DATETIME_MECHANISM_ERROR_GENERAL,
                                      "Error reading /etc/adjtime file: %s", error->message);
                g_error_free (error);
                dbus_g_method_return_error (context, error2);
                g_error_free (error2);
                return FALSE;
        }

        lines = g_strsplit (data, "\n", 0);
        g_free (data);

        if (g_strv_length (lines) < 3) {
                error = g_error_new (MSD_DATETIME_MECHANISM_ERROR,
                                     MSD_DATETIME_MECHANISM_ERROR_GENERAL,
                                     "Cannot parse /etc/adjtime");
                dbus_g_method_return_error (context, error);
                g_error_free (error);
                g_strfreev (lines);
                return FALSE;
        }

        if (strcmp (lines[2], "UTC") == 0) {
                is_utc = TRUE;
        } else if (strcmp (lines[2], "LOCAL") == 0) {
                is_utc = FALSE;
        } else {
                error = g_error_new (MSD_DATETIME_MECHANISM_ERROR,
                                     MSD_DATETIME_MECHANISM_ERROR_GENERAL,
                                     "Expected UTC or LOCAL at line 3 of /etc/adjtime; found '%s'", lines[2]);
                dbus_g_method_return_error (context, error);
                g_error_free (error);
                g_strfreev (lines);
                return FALSE;
        }
        g_strfreev (lines);
        dbus_g_method_return (context, is_utc);
        return TRUE;
}

gboolean
msd_datetime_mechanism_set_hardware_clock_using_utc (MsdDatetimeMechanism  *mechanism,
                                                     gboolean               using_utc,
                                                     DBusGMethodInvocation *context)
{
        GError *error;

        error = NULL;

        if (!_check_polkit_for_action (mechanism, context,
                                       "org.mate.settingsdaemon.datetimemechanism.configurehwclock"))
                return FALSE;

        if (g_file_test ("/sbin/hwclock", 
                         G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR | G_FILE_TEST_IS_EXECUTABLE)) {
                int exit_status;
                char *cmd;
                cmd = g_strdup_printf ("/sbin/hwclock %s --systohc", using_utc ? "--utc" : "--localtime");
                if (!g_spawn_command_line_sync (cmd, NULL, NULL, &exit_status, &error)) {
                        GError *error2;
                        error2 = g_error_new (MSD_DATETIME_MECHANISM_ERROR,
                                              MSD_DATETIME_MECHANISM_ERROR_GENERAL,
                                              "Error spawning /sbin/hwclock: %s", error->message);
                        g_error_free (error);
                        dbus_g_method_return_error (context, error2);
                        g_error_free (error2);
                        g_free (cmd);
                        return FALSE;
                }
                g_free (cmd);
                if (WEXITSTATUS (exit_status) != 0) {
                        error = g_error_new (MSD_DATETIME_MECHANISM_ERROR,
                                             MSD_DATETIME_MECHANISM_ERROR_GENERAL,
                                             "/sbin/hwclock returned %d", exit_status);
                        dbus_g_method_return_error (context, error);
                        g_error_free (error);
                        return FALSE;
                }

                if (!_rh_update_etc_sysconfig_clock (context, "UTC=", using_utc ? "true" : "false"))
                        return FALSE;

        }
        dbus_g_method_return (context);
        return TRUE;
}

static void
check_can_do (MsdDatetimeMechanism  *mechanism,
              const char            *action,
              DBusGMethodInvocation *context)
{
        const char *sender;
        PolkitSubject *subject;
        PolkitAuthorizationResult *result;
        GError *error;

        /* Check that caller is privileged */
        sender = dbus_g_method_get_sender (context);
        subject = polkit_system_bus_name_new (sender);

        error = NULL;
        result = polkit_authority_check_authorization_sync (mechanism->priv->auth,
                                                            subject,
                                                            action,
                                                            NULL,
                                                            0,
                                                            NULL,
                                                            &error);
        g_object_unref (subject);

        if (error) {
                dbus_g_method_return_error (context, error);
                g_error_free (error);
                return;
        }

        if (polkit_authorization_result_get_is_authorized (result)) {
                dbus_g_method_return (context, 2);
        }
        else if (polkit_authorization_result_get_is_challenge (result)) {
                dbus_g_method_return (context, 1);
        }
        else {
                dbus_g_method_return (context, 0);
        }

        g_object_unref (result);
}


gboolean
msd_datetime_mechanism_can_set_time (MsdDatetimeMechanism  *mechanism,
                                     DBusGMethodInvocation *context)
{
        check_can_do (mechanism,
                      "org.mate.settingsdaemon.datetimemechanism.settime",
                      context);

        return TRUE;
}

gboolean
msd_datetime_mechanism_can_set_timezone (MsdDatetimeMechanism  *mechanism,
                                         DBusGMethodInvocation *context)
{
        check_can_do (mechanism,
                      "org.mate.settingsdaemon.datetimemechanism.settimezone",
                      context);

        return TRUE;
}
