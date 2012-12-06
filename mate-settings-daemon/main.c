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
#include <gtk/gtk.h>

#include <gio/gio.h>

#include "mate-settings-manager.h"
#include "mate-settings-profile.h"

#define MSD_DBUS_NAME         "org.mate.SettingsDaemon"

#define MATE_SESSION_DBUS_NAME      "org.mate.SessionManager"
#define MATE_SESSION_DBUS_OBJECT    "/org/mate/SessionManager"
#define MATE_SESSION_DBUS_INTERFACE "org.mate.SessionManager"

static gboolean   no_daemon    = FALSE;
static gboolean   debug        = FALSE;
static gboolean   do_timed_exit = FALSE;
static int        daemon_pipe_fds[2];
static int        term_signal_pipe_fds[2];
static guint      name_id       = 0;
static MateSettingsManager *manager = NULL;

static GOptionEntry entries[] = {
        {"debug", 0, 0, G_OPTION_ARG_NONE, &debug, N_("Enable debugging code"), NULL },
        {"no-daemon", 0, 0, G_OPTION_ARG_NONE, &no_daemon, N_("Don't become a daemon"), NULL },
        { "timed-exit", 0, 0, G_OPTION_ARG_NONE, &do_timed_exit, N_("Exit after a time (for debugging)"), NULL },
        {NULL}
};

static gboolean
timed_exit_cb (void)
{
        gtk_main_quit ();
        return FALSE;
}

static void
on_session_over (GDBusProxy *proxy,
                 gchar      *sender_name,
                 gchar      *signal_name,
                 GVariant   *parameters,
                 gpointer    user_data)
{
        if (g_strcmp0 (signal_name, "SessionOver") == 0) {
                mate_settings_manager_stop (manager);
                gtk_main_quit ();
        }
}

static void
got_session_proxy (GObject      *source_object,
                   GAsyncResult *res,
                   gpointer      user_data)
{
        GDBusProxy *proxy;
        GError *error = NULL;

        proxy = g_dbus_proxy_new_finish (res, &error);
        if (proxy == NULL) {
                g_debug ("Could not connect to the Session manager: %s", error->message);
                g_error_free (error);
        } else {
                g_signal_connect (G_OBJECT (proxy), "g-signal",
                                  G_CALLBACK (on_session_over), NULL);
        }
}

static gboolean
on_term_signal_pipe_closed (GIOChannel   *source,
                            GIOCondition  condition,
                            gpointer      data)
{
        MateSettingsManager *manager;

        manager = MATE_SETTINGS_MANAGER (data);

        term_signal_pipe_fds[0] = -1;

        /* Got SIGTERM, time to clean up and get out
         */
        gtk_main_quit ();

        return FALSE;
}

static void
on_term_signal (int signal)
{
        /* Wake up main loop to tell it to shutdown */
        close (term_signal_pipe_fds[1]);
        term_signal_pipe_fds[1] = -1;
}

static void
watch_for_term_signal (MateSettingsManager *manager)
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

        signal (SIGTERM, on_term_signal);

}

static void
set_session_over_handler (GDBusConnection *bus)
{
        g_assert (bus != NULL);

        g_dbus_proxy_new (bus,
                          G_DBUS_PROXY_FLAGS_NONE,
                          NULL,
                          MATE_SESSION_DBUS_NAME,
                          MATE_SESSION_DBUS_OBJECT,
                          MATE_SESSION_DBUS_INTERFACE,
                          NULL,
                          (GAsyncReadyCallback) got_session_proxy,
                          NULL);

        watch_for_term_signal (manager);
}

static void
name_acquired_handler (GDBusConnection *connection,
                       const gchar     *name,
                       gpointer         user_data)
{
        set_session_over_handler (connection);
}

static void
name_lost_handler (GDBusConnection *connection,
                   const gchar     *name,
                   gpointer         user_data)
{
        /* Name was already taken, or the bus went away */
        gtk_main_quit ();
}

static void
bus_register (void)
{
        name_id = g_bus_own_name (G_BUS_TYPE_SESSION,
                                  MSD_DBUS_NAME,
                                  G_BUS_NAME_OWNER_FLAGS_NONE,
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


/* We want the parent process to quit after initializing all plugins,
 * but we have to do all the work in the child process.  We can't
 * initialize in parent and then fork here: that is not clean with
 * X display and DBUS where we would make the connection from one
 * process and continue using it from the other. So, we just make the
 * parent to fork early and wait. */

static void
daemon_start (void)
{
        int child_pid;
        char buf[1];

        if (no_daemon)
                return;

        mate_settings_profile_msg ("forking daemon");

        signal (SIGPIPE, SIG_IGN);
        if (-1 == pipe (daemon_pipe_fds)) {
                g_error ("Could not create pipe: %s", g_strerror (errno));
                exit (EXIT_FAILURE);
        }

        child_pid = fork ();

        switch (child_pid) {
        case -1:
                g_error ("Could not daemonize: %s", g_strerror (errno));
                exit (EXIT_FAILURE);

        case 0:
                /* child */

                close (daemon_pipe_fds[0]);

                return;

         default:
                /* parent */

                close (daemon_pipe_fds[1]);

                /* Wait for child to signal that we are good to go. */
                read (daemon_pipe_fds[0], buf, 1);

                exit (EXIT_SUCCESS);
        }
}

static void
daemon_detach (void)
{
        if (no_daemon)
                return;

        mate_settings_profile_msg ("detaching daemon");

        /* disconnect */
        setsid ();
        close (0);
        close (1);
        open ("/dev/null", O_RDONLY);
        open ("/dev/null", O_WRONLY);

        /* get outta the way */
        chdir ("/");
}

static void
daemon_terminate_parent (void)
{
        if (no_daemon)
                return;

        mate_settings_profile_msg ("terminating parent");

        write (daemon_pipe_fds[1], "1", 1);
        close (daemon_pipe_fds[1]);
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

int
main (int argc, char *argv[])
{
        gboolean              res;
        GError               *error;

        manager = NULL;

        if (!g_thread_supported ()) {
                g_thread_init (NULL);
        }

        mate_settings_profile_start (NULL);

        bindtextdomain (GETTEXT_PACKAGE, MATE_SETTINGS_LOCALEDIR);
        bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
        textdomain (GETTEXT_PACKAGE);
        setlocale (LC_ALL, "");

        parse_args (&argc, &argv);

        daemon_start ();

        g_type_init ();

        mate_settings_profile_start ("opening gtk display");
        if (! gtk_init_check (NULL, NULL)) {
                g_warning ("Unable to initialize GTK+");
                daemon_terminate_parent ();
                exit (EXIT_FAILURE);
        }
        mate_settings_profile_end ("opening gtk display");

        daemon_detach ();

        g_log_set_default_handler (msd_log_default_handler, NULL);

        bus_register();

        mate_settings_profile_start ("mate_settings_manager_new");
        manager = mate_settings_manager_new ();
        mate_settings_profile_end ("mate_settings_manager_new");
        if (manager == NULL) {
                g_warning ("Unable to register object");
                goto out;
        }

        /* If we aren't started by dbus then load the plugins
           automatically.  Otherwise, wait for an Awake etc. */
        if (g_getenv ("DBUS_STARTER_BUS_TYPE") == NULL) {
                error = NULL;
                res = mate_settings_manager_start (manager, &error);
                if (! res) {
                        g_warning ("Unable to start: %s", error->message);
                        g_error_free (error);
                        goto out;
                }
        }

        daemon_terminate_parent ();

        if (do_timed_exit) {
                g_timeout_add_seconds (30, (GSourceFunc) timed_exit_cb, NULL);
        }

        gtk_main ();

out:
        if (name_id > 0) {
                g_bus_unown_name (name_id);
                name_id = 0;
        }

        if (manager != NULL) {
                g_object_unref (manager);
        }

        g_debug ("SettingsDaemon finished");
        mate_settings_profile_end (NULL);

        return 0;
}
