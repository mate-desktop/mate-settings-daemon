/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007 William Jon McCann <mccann@jhu.edu>
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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"

#include <stdlib.h>
#include <unistd.h>
#include <libintl.h>
#include <errno.h>
#include <locale.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/wait.h>

#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <glib-unix.h>
#include <gio/gio.h>
#include <gtk/gtk.h>

#ifdef HAVE_LIBNOTIFY
#include <libnotify/notify.h>
#endif /* HAVE_LIBNOTIFY */

#include "mate-settings-manager.h"
#include "mate-settings-profile.h"

#include <libmate-desktop/mate-gsettings.h>

#define MSD_DBUS_NAME         "org.mate.SettingsDaemon"

#define DEBUG_KEY             "mate-settings-daemon"
#define DEBUG_SCHEMA          "org.mate.debug"

#define MATE_SESSION_DBUS_NAME      "org.gnome.SessionManager"
#define MATE_SESSION_DBUS_OBJECT    "/org/gnome/SessionManager"
#define MATE_SESSION_DBUS_INTERFACE "org.gnome.SessionManager"
#define MATE_SESSION_PRIVATE_DBUS_INTERFACE "org.gnome.SessionManager.ClientPrivate"

/* this is kept only for compatibility with custom .desktop files */
static gboolean   replace      = FALSE;
static gboolean   debug        = FALSE;
static gboolean   do_timed_exit = FALSE;
static guint      name_id      = 0;
static int        term_signal_pipe_fds[2];
static MateSettingsManager *manager = NULL;

static GOptionEntry entries[] = {
        { "debug", 0, 0, G_OPTION_ARG_NONE, &debug, N_("Enable debugging code"), NULL },
        { "replace", 0, 0, G_OPTION_ARG_NONE, &replace, N_("Replace the current daemon"), NULL },
        { "timed-exit", 0, 0, G_OPTION_ARG_NONE, &do_timed_exit, N_("Exit after a time (for debugging)"), NULL },
        { NULL, 0, 0, G_OPTION_ARG_NONE, NULL, NULL, NULL }
};

static gboolean
timed_exit_cb (void)
{
        g_debug ("Doing timed exit");
        gtk_main_quit ();
        return G_SOURCE_REMOVE;
}

static void
stop_manager (void)
{
        gtk_main_quit ();
}

static void
on_session_over (GDBusProxy *proxy,
                 gchar      *sender_name,
                 gchar      *signal_name,
                 GVariant   *parameters,
                 gpointer    user_data)
{
        if (g_strcmp0 (signal_name, "SessionOver") == 0) {
                g_debug ("Got a SessionOver signal - stopping");
                stop_manager ();
        }
}

static void
respond_to_end_session (GDBusProxy *proxy)
{
        /* we must answer with "EndSessionResponse" */
        g_dbus_proxy_call (proxy, "EndSessionResponse",
                           g_variant_new ("(bs)",
                                          TRUE, ""),
                           G_DBUS_CALL_FLAGS_NONE,
                           -1, NULL, NULL, NULL);
}

static void
client_proxy_signal_cb (GDBusProxy *proxy,
                        gchar *sender_name,
                        gchar *signal_name,
                        GVariant *parameters,
                        gpointer user_data)
{
        if (g_strcmp0 (signal_name, "QueryEndSession") == 0) {
                g_debug ("Got QueryEndSession signal");
                respond_to_end_session (proxy);
        } else if (g_strcmp0 (signal_name, "EndSession") == 0) {
                g_debug ("Got EndSession signal");
                respond_to_end_session (proxy);
        } else if (g_strcmp0 (signal_name, "Stop") == 0) {
                g_debug ("Got Stop signal");
                stop_manager ();
        }
}

static void
got_client_proxy (GObject *object,
                  GAsyncResult *res,
                  gpointer user_data)
{
        GDBusProxy *client_proxy;
        GError *error = NULL;

        client_proxy = g_dbus_proxy_new_for_bus_finish (res, &error);

        if (error != NULL) {
                g_debug ("Unable to get the session client proxy: %s", error->message);
                g_error_free (error);
                return;
        }

        g_signal_connect (client_proxy, "g-signal",
                          G_CALLBACK (client_proxy_signal_cb), manager);
}

static void
start_settings_manager (void)
{
        gboolean res;
        GError *error;

        mate_settings_profile_start ("mate_settings_manager_new");
        manager = mate_settings_manager_new ();
        mate_settings_profile_end ("mate_settings_manager_new");
        if (manager == NULL) {
                g_warning ("Unable to register object");
                gtk_main_quit ();
        }

        /* If we aren't started by dbus then load the plugins automatically during the
         * Initialization phase. Otherwise, wait for an Awake etc. */
        if (g_getenv ("DBUS_STARTER_BUS_TYPE") == NULL) {
                error = NULL;
                res = mate_settings_manager_start (manager, PLUGIN_LOAD_INIT, &error);
                if (! res) {
                        g_warning ("Unable to start: %s", error->message);
                        g_error_free (error);
                }
        }
}

static void
on_client_registered (GObject             *source_object,
                      GAsyncResult        *res,
                      gpointer             user_data)
{
        GVariant *variant;
        GError *error = NULL;
        gchar *object_path = NULL;

        variant = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object), res, &error);
        if (error != NULL) {
                g_warning ("Unable to register client: %s", error->message);
                g_error_free (error);
        } else {
                g_variant_get (variant, "(o)", &object_path);

                g_debug ("Registered client at path %s", object_path);

                g_dbus_proxy_new_for_bus (G_BUS_TYPE_SESSION, 0, NULL,
                                          MATE_SESSION_DBUS_NAME,
                                          object_path,
                                          MATE_SESSION_PRIVATE_DBUS_INTERFACE,
                                          NULL,
                                          got_client_proxy,
                                          manager);

                g_free (object_path);
                g_variant_unref (variant);
        }
}

static void
register_with_mate_session (void)
{
        const char *startup_id;
        GError *error = NULL;
        GDBusProxy *proxy;

        proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
                                               G_DBUS_PROXY_FLAGS_NONE,
                                               NULL,
                                               MATE_SESSION_DBUS_NAME,
                                               MATE_SESSION_DBUS_OBJECT,
                                               MATE_SESSION_DBUS_INTERFACE,
                                               NULL,
                                               &error);

        if (proxy == NULL) {
                g_warning ("Unable to contact session manager daemon: %s\n", error->message);
                g_error_free (error);
        }
        g_signal_connect (G_OBJECT (proxy), "g-signal",
                          G_CALLBACK (on_session_over), NULL);
        startup_id = g_getenv ("DESKTOP_AUTOSTART_ID");
        g_dbus_proxy_call (proxy,
                           "RegisterClient",
                           g_variant_new ("(ss)", "mate-settings-daemon", startup_id ? startup_id : ""),
                           G_DBUS_CALL_FLAGS_NONE,
                           -1,
                           NULL,
                           (GAsyncReadyCallback) on_client_registered,
                           manager);
}

static gboolean
on_term_signal (gpointer signal)
{
        /* Wake up main loop to tell it to shutdown */
        close (term_signal_pipe_fds[1]);
        term_signal_pipe_fds[1] = -1;
        return G_SOURCE_REMOVE;
}

static gboolean
on_term_signal_pipe_closed (GIOChannel *source,
                            GIOCondition condition,
                            gpointer data)
{
        term_signal_pipe_fds[0] = -1;

        /* Got SIGTERM, time to clean up and get out
         */
        gtk_main_quit ();

        return FALSE;
}

static void
watch_for_term_signal (void)
{
        GIOChannel *channel;

        if (-1 == pipe (term_signal_pipe_fds) ||
            -1 == fcntl (term_signal_pipe_fds[0], F_SETFD, FD_CLOEXEC) ||
            -1 == fcntl (term_signal_pipe_fds[1], F_SETFD, FD_CLOEXEC)) {
                g_error ("Could not create pipe: %s", g_strerror (errno));
                exit (EXIT_FAILURE);
        }

        channel = g_io_channel_unix_new (term_signal_pipe_fds[0]);
        g_io_channel_set_encoding (channel, NULL, NULL);
        g_io_channel_set_buffered (channel, FALSE);
        g_io_add_watch (channel, G_IO_HUP, on_term_signal_pipe_closed, manager);
        g_io_channel_unref (channel);

        g_unix_signal_add (SIGTERM, on_term_signal, manager);
}

static void
name_acquired_handler (GDBusConnection *connection,
                       const gchar *name,
                       gpointer user_data)
{
        gboolean res;
        g_autoptr (GError) error = NULL;

        start_settings_manager ();
        register_with_mate_session ();

        /* If we aren't started by dbus then load the plugins automatically after
         * mate-settings-daemon has registered itself. Otherwise, wait for an Awake etc. */
        if (g_getenv ("DBUS_STARTER_BUS_TYPE") == NULL) {
                error = NULL;
                res = mate_settings_manager_start (manager, PLUGIN_LOAD_DEFER, &error);
                if (! res) {
                        g_warning ("Unable to start: %s", error->message);
                        g_error_free (error);
                        gtk_main_quit ();
                }
        }

        watch_for_term_signal ();
}

static void
name_lost_handler (GDBusConnection *connection,
                   const gchar *name,
                   gpointer user_data)
{
        /* Name was already taken, or the bus went away */

        g_warning ("Name taken or bus went away - shutting down");

        gtk_main_quit ();

}

static void
bus_register (void)
{
        GBusNameOwnerFlags flags;

        flags = G_BUS_NAME_OWNER_FLAGS_ALLOW_REPLACEMENT;

        if (replace)
                flags |= G_BUS_NAME_OWNER_FLAGS_REPLACE;

        name_id = g_bus_own_name (G_BUS_TYPE_SESSION,
                                  MSD_DBUS_NAME,
                                  flags,
                                  NULL,
                                  (GBusNameAcquiredCallback) name_acquired_handler,
                                  (GBusNameLostCallback) name_lost_handler,
                                  NULL,
                                  NULL);
}

static void
msd_log_default_handler (const gchar   *log_domain,
                         GLogLevelFlags log_level,
                         const gchar   *message,
                         gpointer       unused_data)
{
        /* filter out DEBUG messages if debug isn't set */
        if ((log_level & G_LOG_LEVEL_MASK) == G_LOG_LEVEL_DEBUG
            && ! debug) {
                return;
        }

        g_log_default_handler (log_domain,
                               log_level,
                               message,
                               unused_data);
}

static void
parse_args (int *argc, char ***argv)
{
        GError *error;
        GOptionContext *context;

        mate_settings_profile_start (NULL);

        context = g_option_context_new (NULL);

        g_option_context_add_main_entries (context, entries, NULL);
        g_option_context_add_group (context, gtk_get_option_group (FALSE));

        error = NULL;
        if (!g_option_context_parse (context, argc, argv, &error)) {
                if (error != NULL) {
                        g_warning ("%s", error->message);
                        g_error_free (error);
                } else {
                        g_warning ("Unable to initialize GTK+");
                }
                exit (EXIT_FAILURE);
        }

        g_option_context_free (context);

        mate_settings_profile_end (NULL);

        if (debug)
            g_setenv ("G_MESSAGES_DEBUG", "all", FALSE);
}

static void debug_changed (GSettings *settings,
                           gchar     *key,
                           gpointer   user_data G_GNUC_UNUSED)
{
        debug = g_settings_get_boolean (settings, key);
        if (debug) {
            g_warning ("Enable DEBUG");
            g_setenv ("G_MESSAGES_DEBUG", "all", FALSE);
        } else {
            g_warning ("Disable DEBUG");
            g_unsetenv ("G_MESSAGES_DEBUG");
        }
}

int
main (int argc, char *argv[])
{
        GSettings            *debug_settings = NULL;

        manager = NULL;

        mate_settings_profile_start (NULL);

        bindtextdomain (GETTEXT_PACKAGE, MATE_SETTINGS_LOCALEDIR);
        bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
        textdomain (GETTEXT_PACKAGE);
        setlocale (LC_ALL, "");

        parse_args (&argc, &argv);

        /* Allows to enable/disable debug from GSettings only if it is not set from argument */
        if (mate_gsettings_schema_exists (DEBUG_SCHEMA))
        {
                debug_settings = g_settings_new (DEBUG_SCHEMA);
                debug = g_settings_get_boolean (debug_settings, DEBUG_KEY);
                g_signal_connect (debug_settings, "changed::" DEBUG_KEY, G_CALLBACK (debug_changed), NULL);

		if (debug) {
		    g_setenv ("G_MESSAGES_DEBUG", "all", FALSE);
		}
        }

        mate_settings_profile_start ("opening gtk display");
        if (! gtk_init_check (NULL, NULL)) {
                g_warning ("Unable to initialize GTK+");
                exit (EXIT_FAILURE);
        }
        mate_settings_profile_end ("opening gtk display");

        g_log_set_default_handler (msd_log_default_handler, NULL);

        bus_register ();

#ifdef HAVE_LIBNOTIFY
        notify_init ("mate-settings-daemon");
#endif /* HAVE_LIBNOTIFY */


        if (do_timed_exit) {
                g_timeout_add_seconds (30, G_SOURCE_FUNC (timed_exit_cb), NULL);
        }

        gtk_main ();

        g_debug ("Shutting down");

        if (name_id > 0) {
                g_bus_unown_name (name_id);
                name_id = 0;
        }
        if (manager != NULL) {
                g_object_unref (manager);
        }

        if (debug_settings != NULL) {
                g_object_unref (debug_settings);
        }

#ifdef HAVE_LIBNOTIFY
        if (notify_is_initted ())
            notify_uninit ();
#endif /* HAVE_LIBNOTIFY */

        g_debug ("SettingsDaemon finished");
        mate_settings_profile_end (NULL);

        return 0;
}
