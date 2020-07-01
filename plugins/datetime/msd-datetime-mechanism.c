/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 * vim: set ts=8 sts=8 sw=8 expandtab:
 *
 * Copyright (C) 2007 David Zeuthen <david@fubar.dk>
 * Copyright (C) 2012-2021 MATE Developers
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
#include <polkit/polkit.h>

#include "system-timezone.h"
#include "msd-datetime-mechanism.h"
#include "msd-datetime-generated.h"

#define MSD_DATETIME_DBUS_NAME "org.mate.SettingsDaemon.DateTimeMechanism"
#define MSD_DATETIME_DBUS_PATH "/"

enum {
        PROP_0,
        PROP_LOOP,
        LAST_PROP
};

struct MsdDatetimeMechanismPrivate
{
        MateSettingsDateTimeMechanism *skeleton;
        guint                          bus_name_id;
        GMainLoop                     *loop;
        PolkitAuthority               *auth;
};

static GParamSpec *properties[LAST_PROP] = { NULL };

static void     msd_datetime_mechanism_dispose (GObject     *object);
static gboolean msd_datetime_mechanism_can_set_timezone_handler (MateSettingsDateTimeMechanism *object,
                                                                 GDBusMethodInvocation         *invocation,
                                                                 gpointer                       user_data);
static gboolean msd_datetime_mechanism_get_timezone_handler (MateSettingsDateTimeMechanism *object,
                                                             GDBusMethodInvocation         *invocation,
                                                             gpointer                       user_data);
static gboolean msd_datetime_mechanism_set_timezone_handler (MateSettingsDateTimeMechanism *object,
                                                             GDBusMethodInvocation         *invocation,
                                                             const gchar                   *arg_zonefile,
                                                             gpointer                       user_data);

static gboolean msd_datetime_mechanism_adjust_time_handler (MateSettingsDateTimeMechanism *object,
                                                            GDBusMethodInvocation         *invocation,
                                                            gint64                         arg_seconds_to_add,
                                                            gpointer                       user_data);
static gboolean msd_datetime_mechanism_can_set_time_handler (MateSettingsDateTimeMechanism *object,
                                                             GDBusMethodInvocation         *invocation,
                                                             gpointer                       user_data);
static gboolean msd_datetime_mechanism_get_hardware_clock_using_utc_handler (MateSettingsDateTimeMechanism *object,
                                                                             GDBusMethodInvocation         *invocation,
                                                                             gpointer                       user_data);
static gboolean msd_datetime_mechanism_set_hardware_clock_using_utc_handler (MateSettingsDateTimeMechanism *object,
                                                                             GDBusMethodInvocation         *invocation,
                                                                             gboolean                       arg_is_using_utc,
                                                                             gpointer                       user_data);
static gboolean msd_datetime_mechanism_set_time_handler (MateSettingsDateTimeMechanism *object,
                                                         GDBusMethodInvocation         *invocation,
                                                         gint64                         arg_seconds_since_epoch,
                                                         gpointer                       user_data);
static gboolean _set_time (MsdDatetimeMechanism  *mechanism,
                           const struct timeval  *tv,
                           GError               **error);
static gboolean _check_polkit_for_action (MsdDatetimeMechanism *mechanism,
                                          const char           *action,
                                          const char           *sender,
                                          GError              **error);
static gboolean _rh_update_etc_sysconfig_clock (const char *key,
                                                const char *value,
                                                GError    **error);

G_DEFINE_TYPE_WITH_PRIVATE (MsdDatetimeMechanism, msd_datetime_mechanism, G_TYPE_OBJECT)

static gboolean
do_exit (gpointer user_data)
{
        GMainLoop *loop;

        loop = (GMainLoop*)user_data;
        g_debug ("Exiting due to inactivity");
        g_main_loop_quit (loop);
        return FALSE;
}

static void
reset_killtimer (GMainLoop *loop)
{
        static guint timer_id = 0;

        if (timer_id > 0) {
                g_source_remove (timer_id);
        }
        g_debug ("Setting killtimer to 30 seconds...");
        timer_id = g_timeout_add_seconds (30, do_exit, loop);
}

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

static gboolean
msd_datetime_mechanism_adjust_time_handler (MateSettingsDateTimeMechanism *object,
                                            GDBusMethodInvocation         *invocation,
                                            gint64                         seconds_to_add,
                                            gpointer                       user_data)
{
        struct timeval tv;
        gboolean ret = FALSE;
        GError *error = NULL;
        MsdDatetimeMechanism *mechanism;

        mechanism = MSD_DATETIME_MECHANISM (user_data);

        reset_killtimer (mechanism->priv->loop);
        g_debug ("AdjustTime(%ld) called", seconds_to_add);

        if (gettimeofday (&tv, NULL) != 0) {
                error = g_error_new (MSD_DATETIME_MECHANISM_ERROR,
                                     MSD_DATETIME_MECHANISM_ERROR_GENERAL,
                                     "Error calling gettimeofday(): %s", strerror (errno));
                g_dbus_method_invocation_return_gerror (invocation, error);
                g_error_free (error);
                return FALSE;
        }

        if (!_check_polkit_for_action (mechanism,
                                       "org.mate.settingsdaemon.datetimemechanism.settime",
                                       g_dbus_method_invocation_get_sender (invocation),
                                       &error)) {
                g_dbus_method_invocation_return_gerror (invocation, error);
                g_error_free (error);
                return FALSE;
        }

        tv.tv_sec += (time_t) seconds_to_add;
        ret = _set_time (mechanism, &tv, &error);
        if (!ret) {
                g_dbus_method_invocation_return_gerror (invocation, error);
                g_error_free (error);
        } else {
                mate_settings_date_time_mechanism_complete_adjust_time (object, invocation);
        }

        return ret;
}

static gint
check_can_do (MsdDatetimeMechanism  *mechanism,
              const char            *action,
              const char            *sender,
              GError               **error)
{
        gint value = -1;
        PolkitSubject *subject;
        PolkitAuthorizationResult *result;

        /* Check that caller is privileged */
        subject = polkit_system_bus_name_new (sender);
        result = polkit_authority_check_authorization_sync (mechanism->priv->auth,
                                                            subject,
                                                            action,
                                                            NULL,
                                                            0,
                                                            NULL,
                                                            error);
        g_object_unref (subject);

        if (*error != NULL) {
                return value;
        }

        if (polkit_authorization_result_get_is_authorized (result)) {
                value = 2;
        }
        else if (polkit_authorization_result_get_is_challenge (result)) {
                value = 1;
        }
        else {
                value = 0;
        }

        g_object_unref (result);
        return value;
}


static gboolean
msd_datetime_mechanism_can_set_time_handler (MateSettingsDateTimeMechanism *object,
                                             GDBusMethodInvocation         *invocation,
                                             gpointer                       user_data)
{
        gboolean ret = FALSE;
        gint value;
        GError *error = NULL;
        MsdDatetimeMechanism *mechanism;

        mechanism = MSD_DATETIME_MECHANISM (user_data);

        value = check_can_do (mechanism,
                              "org.mate.settingsdaemon.datetimemechanism.settime",
                              g_dbus_method_invocation_get_sender (invocation),
                              &error);
        if (error != NULL) {
                g_dbus_method_invocation_return_gerror (invocation, error);
                g_error_free (error);
                ret = FALSE;
        } else {
                mate_settings_date_time_mechanism_complete_can_set_time (object, invocation, value);
                ret = TRUE;
        }

        return ret;
}

static gboolean
msd_datetime_mechanism_can_set_timezone_handler (MateSettingsDateTimeMechanism *object,
                                                 GDBusMethodInvocation         *invocation,
                                                 gpointer                       user_data)
{
        gboolean ret = FALSE;
        gint value;
        GError *error = NULL;
        MsdDatetimeMechanism *mechanism;

        mechanism = MSD_DATETIME_MECHANISM (user_data);
        value = check_can_do (mechanism,
                              "org.mate.settingsdaemon.datetimemechanism.settimezone",
                              g_dbus_method_invocation_get_sender (invocation),
                              &error);
        if (error != NULL) {
                g_dbus_method_invocation_return_gerror (invocation, error);
                g_error_free (error);
                ret = FALSE;
        } else {
                mate_settings_date_time_mechanism_complete_can_set_timezone (object, invocation, value);
                ret = TRUE;
        }

        return ret;
}

static gboolean
msd_datetime_mechanism_get_hardware_clock_using_utc_handler (MateSettingsDateTimeMechanism *object,
                                                             GDBusMethodInvocation         *invocation,
                                                             gpointer                       user_data G_GNUC_UNUSED)
{
        char **lines;
        char *data;
        gsize len;
        gboolean is_utc;
        GError *error = NULL;

        if (!g_file_get_contents ("/etc/adjtime", &data, &len, &error)) {
                GError *error2;
                error2 = g_error_new (MSD_DATETIME_MECHANISM_ERROR,
                                      MSD_DATETIME_MECHANISM_ERROR_GENERAL,
                                      "Error reading /etc/adjtime file: %s", error->message);
                g_error_free (error);
                g_dbus_method_invocation_return_gerror (invocation, error2);
                g_error_free (error2);
                return FALSE;
        }

        lines = g_strsplit (data, "\n", 0);
        g_free (data);

        if (g_strv_length (lines) < 3) {
                error = g_error_new (MSD_DATETIME_MECHANISM_ERROR,
                                     MSD_DATETIME_MECHANISM_ERROR_GENERAL,
                                     "Cannot parse /etc/adjtime");
                g_dbus_method_invocation_return_gerror (invocation, error);
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
                                     "Expected UTC or LOCAL at line 3 of /etc/adjtime; found '%s'",
                                     lines[2]);
                g_dbus_method_invocation_return_gerror (invocation, error);
                g_error_free (error);
                g_strfreev (lines);
                return FALSE;
        }
        g_strfreev (lines);
        mate_settings_date_time_mechanism_complete_get_hardware_clock_using_utc (object, invocation, is_utc);
        return TRUE;
}

static gboolean
msd_datetime_mechanism_get_timezone_handler (MateSettingsDateTimeMechanism *object,
                                             GDBusMethodInvocation         *invocation,
                                             gpointer                       user_data G_GNUC_UNUSED)
{
        gchar *tz;
        MsdDatetimeMechanism *mechanism;

        mechanism = MSD_DATETIME_MECHANISM (user_data);

        reset_killtimer (mechanism->priv->loop);

        tz = system_timezone_find ();

        mate_settings_date_time_mechanism_complete_get_timezone (object, invocation, tz);

        return TRUE;
}

static gboolean
msd_datetime_mechanism_set_hardware_clock_using_utc_handler (MateSettingsDateTimeMechanism *object,
                                                             GDBusMethodInvocation         *invocation,
                                                             gboolean                       using_utc,
                                                             gpointer                       user_data)
{
        GError *error = NULL;
        MsdDatetimeMechanism *mechanism;

        mechanism = MSD_DATETIME_MECHANISM (user_data);
        if (!_check_polkit_for_action (mechanism,
                                       "org.mate.settingsdaemon.datetimemechanism.configurehwclock",
                                       g_dbus_method_invocation_get_sender (invocation),
                                       &error)) {
                g_dbus_method_invocation_return_gerror (invocation, error);
                g_error_free (error);
                return FALSE;
        }

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
                        g_dbus_method_invocation_return_gerror (invocation, error2);
                        g_error_free (error2);
                        g_free (cmd);
                        return FALSE;
                }
                g_free (cmd);
                if (WEXITSTATUS (exit_status) != 0) {
                        error = g_error_new (MSD_DATETIME_MECHANISM_ERROR,
                                             MSD_DATETIME_MECHANISM_ERROR_GENERAL,
                                             "/sbin/hwclock returned %d", exit_status);
                        g_dbus_method_invocation_return_gerror (invocation, error);
                        g_error_free (error);
                        return FALSE;
                }

                if (!_rh_update_etc_sysconfig_clock ("UTC=", using_utc ? "true" : "false", &error)) {
                        g_dbus_method_invocation_return_gerror (invocation, error);
                        g_error_free (error);
                        return FALSE;
                }

        }
        mate_settings_date_time_mechanism_complete_set_hardware_clock_using_utc (object, invocation);
        return TRUE;
}

static gboolean
msd_datetime_mechanism_set_time_handler (MateSettingsDateTimeMechanism *object,
                                         GDBusMethodInvocation         *invocation,
                                         gint64                         arg_seconds_since_epoch,
                                         gpointer                       user_data)
{
        gboolean ret = FALSE;
        struct timeval tv;
        GError *error = NULL;
        MsdDatetimeMechanism *mechanism;

        mechanism = MSD_DATETIME_MECHANISM (user_data);

        reset_killtimer (mechanism->priv->loop);
        g_debug ("SetTime(%ld) called", arg_seconds_since_epoch);

        if (!_check_polkit_for_action (mechanism,
                                       "org.mate.settingsdaemon.datetimemechanism.settime",
                                       g_dbus_method_invocation_get_sender (invocation),
                                       &error)) {
                g_dbus_method_invocation_return_gerror (invocation, error);
                g_error_free (error);
                return FALSE;
        }

        tv.tv_sec = (time_t) arg_seconds_since_epoch;
        tv.tv_usec = 0;

        ret = _set_time (mechanism, &tv, &error);
        if (ret == FALSE) {
                g_dbus_method_invocation_return_gerror (invocation, error);
                g_error_free (error);
        } else {
                mate_settings_date_time_mechanism_complete_set_time (object, invocation);
        }
        return ret;
}

static gboolean
msd_datetime_mechanism_set_timezone_handler (MateSettingsDateTimeMechanism *object,
                                             GDBusMethodInvocation         *invocation,
                                             const gchar                   *zonefile,
                                             gpointer                       user_data)
{
        GError *error = NULL;
        MsdDatetimeMechanism *mechanism;

        mechanism = MSD_DATETIME_MECHANISM (user_data);
        reset_killtimer (mechanism->priv->loop);
        g_debug ("SetTimezone('%s') called", zonefile);

        if (!_check_polkit_for_action (mechanism,
                                       "org.mate.settingsdaemon.datetimemechanism.settimezone",
                                       g_dbus_method_invocation_get_sender (invocation),
                                       &error)) {
                g_dbus_method_invocation_return_gerror (invocation, error);
                g_error_free (error);
                return FALSE;
        }

        if (!system_timezone_set_from_file (zonefile, &error)) {
                GError *error2;
                int     code;

                if (error->code == SYSTEM_TIMEZONE_ERROR_INVALID_TIMEZONE_FILE)
                        code = MSD_DATETIME_MECHANISM_ERROR_INVALID_TIMEZONE_FILE;
                else
                        code = MSD_DATETIME_MECHANISM_ERROR_GENERAL;

                error2 = g_error_new (MSD_DATETIME_MECHANISM_ERROR,
                                      code, "%s", error->message);


                g_error_free (error);

                g_dbus_method_invocation_return_gerror (invocation, error2);
                g_error_free (error2);
                return FALSE;
        }

        mate_settings_date_time_mechanism_complete_set_timezone (object, invocation);
        return TRUE;
}

static void
bus_acquired_handler_cb (GDBusConnection *connection,
                         const gchar     *name G_GNUC_UNUSED,
                         gpointer         user_data)
{
        MsdDatetimeMechanism *mechanism;
        GError *error = NULL;
        gboolean exported;

        mechanism = MSD_DATETIME_MECHANISM (user_data);

        g_signal_connect (mechanism->priv->skeleton,
                          "handle-can-set-timezone",
                          G_CALLBACK (msd_datetime_mechanism_can_set_timezone_handler),
                          mechanism);
        g_signal_connect (mechanism->priv->skeleton,
                          "handle-set-timezone",
                          G_CALLBACK (msd_datetime_mechanism_set_timezone_handler),
                          mechanism);
        g_signal_connect (mechanism->priv->skeleton,
                          "handle-get-timezone",
                          G_CALLBACK (msd_datetime_mechanism_get_timezone_handler),
                          mechanism);

        g_signal_connect (mechanism->priv->skeleton,
                          "handle-can-set-time",
                          G_CALLBACK (msd_datetime_mechanism_can_set_time_handler),
                          mechanism);
        g_signal_connect (mechanism->priv->skeleton,
                          "handle-set-time",
                          G_CALLBACK (msd_datetime_mechanism_set_time_handler),
                          mechanism);

        g_signal_connect (mechanism->priv->skeleton,
                          "handle-adjust-time",
                          G_CALLBACK (msd_datetime_mechanism_adjust_time_handler),
                          mechanism);

        g_signal_connect (mechanism->priv->skeleton,
                          "handle-get-hardware-clock-using-utc",
                          G_CALLBACK (msd_datetime_mechanism_get_hardware_clock_using_utc_handler),
                          mechanism);
        g_signal_connect (mechanism->priv->skeleton,
                          "handle-set-hardware-clock-using-utc",
                          G_CALLBACK (msd_datetime_mechanism_set_hardware_clock_using_utc_handler),
                          mechanism);

        exported = g_dbus_interface_skeleton_export (
                        G_DBUS_INTERFACE_SKELETON (mechanism->priv->skeleton),
                                                   connection,
                                                   MSD_DATETIME_DBUS_PATH,
                                                   &error);
        if (!exported)
        {
                g_warning ("Failed to export interface: %s", error->message);
                g_error_free (error);
                g_main_loop_quit (mechanism->priv->loop);
        }
}

static void
name_lost_handler_cb (GDBusConnection *connection G_GNUC_UNUSED,
                      const gchar     *name G_GNUC_UNUSED,
                      gpointer         user_data)
{
        MsdDatetimeMechanism *mechanism;

        mechanism = MSD_DATETIME_MECHANISM (user_data);
        g_debug("bus name lost\n");

        g_main_loop_quit (mechanism->priv->loop);
}

static void
msd_datetime_mechanism_constructed (GObject *object)
{
        MsdDatetimeMechanism *mechanism;

        mechanism = MSD_DATETIME_MECHANISM (object);

        G_OBJECT_CLASS (msd_datetime_mechanism_parent_class)->constructed (object);

        mechanism->priv->bus_name_id = g_bus_own_name (G_BUS_TYPE_SYSTEM,
                                                       MSD_DATETIME_DBUS_NAME,
                                                       G_BUS_NAME_OWNER_FLAGS_NONE,
                                                       bus_acquired_handler_cb,
                                                       NULL,
                                                       name_lost_handler_cb, mechanism, NULL);
}

static void
msd_datetime_mechanism_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
        MsdDatetimeMechanism *mechanism;

        mechanism = MSD_DATETIME_MECHANISM (object);

        switch (prop_id)
        {
                case PROP_LOOP:
                        mechanism->priv->loop = g_value_get_pointer (value);
                        break;
                default:
                        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                        break;
        }
 }

static void
msd_datetime_mechanism_class_init (MsdDatetimeMechanismClass *klass)
{
        GObjectClass   *object_class = G_OBJECT_CLASS (klass);

        object_class->constructed = msd_datetime_mechanism_constructed;
        object_class->set_property = msd_datetime_mechanism_set_property;
        object_class->dispose = msd_datetime_mechanism_dispose;

        properties[PROP_LOOP] =
            g_param_spec_pointer("loop",
                                 "loop",
                                 "loop",
                                 G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE |
                                 G_PARAM_STATIC_STRINGS);
        g_object_class_install_properties (object_class, LAST_PROP, properties);
}

static void
msd_datetime_mechanism_init (MsdDatetimeMechanism *mechanism)
{
        mechanism->priv = msd_datetime_mechanism_get_instance_private (mechanism);
        mechanism->priv->skeleton = mate_settings_date_time_mechanism_skeleton_new ();
}

static void
msd_datetime_mechanism_dispose (GObject *object)
{
        MsdDatetimeMechanism *mechanism;

        g_return_if_fail (object != NULL);
        g_return_if_fail (MSD_DATETIME_IS_MECHANISM (object));

        mechanism = MSD_DATETIME_MECHANISM (object);

        g_return_if_fail (mechanism->priv != NULL);

        if (mechanism->priv->skeleton != NULL)
        {
                GDBusInterfaceSkeleton *skeleton;

                skeleton = G_DBUS_INTERFACE_SKELETON (mechanism->priv->skeleton);
                g_dbus_interface_skeleton_unexport (skeleton);
                g_clear_object (&mechanism->priv->skeleton);
        }

        if (mechanism->priv->bus_name_id > 0)
        {
                g_bus_unown_name (mechanism->priv->bus_name_id);
                mechanism->priv->bus_name_id = 0;
        }


        G_OBJECT_CLASS (msd_datetime_mechanism_parent_class)->dispose (object);
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

        reset_killtimer (mechanism->priv->loop);

        return TRUE;

error:
        return FALSE;
}

MsdDatetimeMechanism *
msd_datetime_mechanism_new (GMainLoop *loop)
{
        GObject *object;
        gboolean res;

        object = g_object_new (MSD_DATETIME_TYPE_MECHANISM, "loop", loop, NULL);

        res = register_mechanism (MSD_DATETIME_MECHANISM (object));
        if (! res) {
                g_object_unref (object);
                return NULL;
        }

        return MSD_DATETIME_MECHANISM (object);
}

static gboolean
_check_polkit_for_action (MsdDatetimeMechanism  *mechanism,
                          const char            *action,
                          const char            *sender,
                          GError               **error)
{
        PolkitSubject *subject;
        PolkitAuthorizationResult *result;

        /* Check that caller is privileged */
        subject = polkit_system_bus_name_new (sender);
        result = polkit_authority_check_authorization_sync (mechanism->priv->auth,
                                                            subject,
                                                            action,
                                                            NULL,
                                                            POLKIT_CHECK_AUTHORIZATION_FLAGS_ALLOW_USER_INTERACTION,
                                                            NULL, error);
        g_object_unref (subject);

        if (*error != NULL) {
                return FALSE;
        }

        if (!polkit_authorization_result_get_is_authorized (result)) {
                *error = g_error_new (MSD_DATETIME_MECHANISM_ERROR,
                                      MSD_DATETIME_MECHANISM_ERROR_NOT_PRIVILEGED,
                                      "Not Authorized for action %s", action);
                g_object_unref (result);

                return FALSE;
        }

        g_object_unref (result);

        return TRUE;
}

static gboolean
_set_time (MsdDatetimeMechanism  *mechanism G_GNUC_UNUSED,
           const struct timeval  *tv,
           GError               **error)
{
        if (settimeofday (tv, NULL) != 0) {
                *error = g_error_new (MSD_DATETIME_MECHANISM_ERROR,
                                      MSD_DATETIME_MECHANISM_ERROR_GENERAL,
                                      "Error calling settimeofday({%ld,%ld}): %s",
                                      (gint64) tv->tv_sec, (gint64) tv->tv_usec,
                                      strerror (errno));
                return FALSE;
        }

        if (g_file_test ("/sbin/hwclock",
                         G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR | G_FILE_TEST_IS_EXECUTABLE)) {
                int exit_status;
                if (!g_spawn_command_line_sync ("/sbin/hwclock --systohc", NULL, NULL, &exit_status, error)) {
                        GError *error2;
                        error2 = g_error_new (MSD_DATETIME_MECHANISM_ERROR,
                                              MSD_DATETIME_MECHANISM_ERROR_GENERAL,
                                              "Error spawning /sbin/hwclock: %s", (*error)->message);
                        g_error_free (*error);
                        g_propagate_error (error, error2);
                        return FALSE;
                }
                if (WEXITSTATUS (exit_status) != 0) {
                        *error = g_error_new (MSD_DATETIME_MECHANISM_ERROR,
                                              MSD_DATETIME_MECHANISM_ERROR_GENERAL,
                                              "/sbin/hwclock returned %d", exit_status);
                        return FALSE;
                }
        }

        return TRUE;
}

static gboolean
_rh_update_etc_sysconfig_clock (const char *key,
                                const char *value,
                                GError    **error)
{
        /* On Red Hat / Fedora, the /etc/sysconfig/clock file needs to be kept in sync */
        if (g_file_test ("/etc/sysconfig/clock", G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR)) {
                char **lines;
                int n;
                gboolean replaced;
                char *data;
                gsize len;

                if (!g_file_get_contents ("/etc/sysconfig/clock", &data, &len, error)) {
                        GError *error2;
                        error2 = g_error_new (MSD_DATETIME_MECHANISM_ERROR,
                                              MSD_DATETIME_MECHANISM_ERROR_GENERAL,
                                              "Error reading /etc/sysconfig/clock file: %s", (*error)->message);
                        g_error_free (*error);
                        g_propagate_error (error, error2);
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
                        if (!g_file_set_contents ("/etc/sysconfig/clock", data, len, error)) {
                                GError *error2;
                                error2 = g_error_new (MSD_DATETIME_MECHANISM_ERROR,
                                                MSD_DATETIME_MECHANISM_ERROR_GENERAL,
                                                "Error updating /etc/sysconfig/clock: %s", (*error)->message);
                                g_error_free (*error);
                                g_propagate_error (error, error2);
                                g_free (data);
                                return FALSE;
                        }
                        g_free (data);
                }
                g_strfreev (lines);
        }

        return TRUE;
}
