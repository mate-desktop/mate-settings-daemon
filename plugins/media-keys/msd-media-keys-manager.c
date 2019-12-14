/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2001-2003 Bastien Nocera <hadess@hadess.net>
 * Copyright (C) 2006-2007 William Jon McCann <mccann@jhu.edu>
 * Copyright (C) 2014 Michal Ratajsky <michal.ratajsky@gmail.com>
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

#include <glib.h>
#include <glib/gi18n.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include <gio/gio.h>

#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>

#ifdef HAVE_LIBMATEMIXER
#include <libmatemixer/matemixer.h>
#endif

#ifdef HAVE_LIBCANBERRA
#include <canberra-gtk.h>
#endif

#include "mate-settings-profile.h"
#include "msd-marshal.h"
#include "msd-media-keys-manager.h"
#include "msd-media-keys-manager-glue.h"

#include "eggaccelerators.h"
#include "acme.h"
#include "msd-media-keys-window.h"
#include "msd-input-helper.h"

#define MSD_DBUS_PATH "/org/mate/SettingsDaemon"
#define MSD_DBUS_NAME "org.mate.SettingsDaemon"
#define MSD_MEDIA_KEYS_DBUS_PATH MSD_DBUS_PATH "/MediaKeys"
#define MSD_MEDIA_KEYS_DBUS_NAME MSD_DBUS_NAME ".MediaKeys"

#define TOUCHPAD_SCHEMA "org.mate.peripherals-touchpad"
#define TOUCHPAD_ENABLED_KEY "touchpad-enabled"

typedef struct {
        char   *application;
        guint32 time;
} MediaPlayer;

struct _MsdMediaKeysManagerPrivate
{
#ifdef HAVE_LIBMATEMIXER
        /* Volume bits */
        MateMixerContext       *context;
        MateMixerStream        *stream;
        MateMixerStream        *source_stream;
        MateMixerStreamControl *control;
        MateMixerStreamControl *source_control;
#endif
        GtkWidget        *dialog;
        GSettings        *settings;
        GVolumeMonitor   *volume_monitor;

        /* Multihead stuff */
        GdkScreen        *current_screen;
        GSList           *screens;

        /* RFKill stuff */
        guint            rfkill_watch_id;
        GDBusProxy      *rfkill_proxy;
        GCancellable    *rfkill_cancellable;

        GList            *media_players;

        DBusGConnection  *connection;
        guint             notify[HANDLED_KEYS];
};

enum {
        MEDIA_PLAYER_KEY_PRESSED,
        LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE_WITH_PRIVATE (MsdMediaKeysManager, msd_media_keys_manager, G_TYPE_OBJECT)

static gpointer manager_object = NULL;

static void
init_screens (MsdMediaKeysManager *manager)
{
        GdkDisplay *display;

        display = gdk_display_get_default ();

        GdkScreen *screen;

        screen = gdk_display_get_default_screen (display);

        if (screen != NULL) {
                manager->priv->screens = g_slist_append (manager->priv->screens, screen);
        }

        manager->priv->current_screen = manager->priv->screens->data;
}

static void
acme_error (char * msg)
{
        GtkWidget *error_dialog;

        error_dialog = gtk_message_dialog_new (NULL,
                                               GTK_DIALOG_MODAL,
                                               GTK_MESSAGE_ERROR,
                                               GTK_BUTTONS_OK,
                                               msg, NULL);
        gtk_dialog_set_default_response (GTK_DIALOG (error_dialog),
                                         GTK_RESPONSE_OK);
        gtk_widget_show (error_dialog);
        g_signal_connect (error_dialog,
                          "response",
                          G_CALLBACK (gtk_widget_destroy),
                          NULL);
}

static char *
get_term_command (MsdMediaKeysManager *manager)
{
	char *cmd_term, *cmd_args;
	char *cmd = NULL;
	GSettings *settings;

	settings = g_settings_new ("org.mate.applications-terminal");
	cmd_term = g_settings_get_string (settings, "exec");
	cmd_args = g_settings_get_string (settings, "exec-arg");

	if (cmd_term[0] != '\0') {
		cmd = g_strdup_printf ("%s %s -e", cmd_term, cmd_args);
	} else {
		cmd = g_strdup_printf ("mate-terminal -e");
	}

	g_free (cmd_args);
	g_free (cmd_term);
	g_object_unref (settings);

        return cmd;
}

static void
execute (MsdMediaKeysManager *manager,
         char                *cmd,
         gboolean             sync,
         gboolean             need_term)
{
        gboolean retval;
        char   **argv;
        int      argc;
        char    *exec;
        char    *term = NULL;

        retval = FALSE;

        if (need_term) {
                term = get_term_command (manager);
                if (term == NULL) {
                        acme_error (_("Could not get default terminal. Verify that your default "
                                      "terminal command is set and points to a valid application."));
                        return;
                }
        }

        if (term) {
                exec = g_strdup_printf ("%s %s", term, cmd);
                g_free (term);
        } else {
                exec = g_strdup (cmd);
        }

        if (g_shell_parse_argv (exec, &argc, &argv, NULL)) {
                if (sync != FALSE) {
                        retval = g_spawn_sync (g_get_home_dir (),
                                               argv,
                                               NULL,
                                               G_SPAWN_SEARCH_PATH,
                                               NULL,
                                               NULL,
                                               NULL,
                                               NULL,
                                               NULL,
                                               NULL);
                } else {
                        retval = g_spawn_async (g_get_home_dir (),
                                                argv,
                                                NULL,
                                                G_SPAWN_SEARCH_PATH,
                                                NULL,
                                                NULL,
                                                NULL,
                                                NULL);
                }
                g_strfreev (argv);
        }

        if (retval == FALSE) {
                char *msg;
                msg = g_strdup_printf (_("Couldn't execute command: %s\n"
                                         "Verify that this is a valid command."),
                                       exec);

                acme_error (msg);
                g_free (msg);
        }
        g_free (exec);
}

static void
dialog_init (MsdMediaKeysManager *manager)
{
        if (manager->priv->dialog != NULL
            && !msd_osd_window_is_valid (MSD_OSD_WINDOW (manager->priv->dialog))) {
                gtk_widget_destroy (manager->priv->dialog);
                manager->priv->dialog = NULL;
        }

        if (manager->priv->dialog == NULL) {
                manager->priv->dialog = msd_media_keys_window_new ();
        }
}

static gboolean
is_valid_shortcut (const char *string)
{
        if (string == NULL || string[0] == '\0') {
                return FALSE;
        }
        if (strcmp (string, "disabled") == 0) {
                return FALSE;
        }

        return TRUE;
}

static void
update_kbd_cb (GSettings           *settings,
               gchar               *settings_key,
               MsdMediaKeysManager *manager)
{
        int      i;
        GdkDisplay *dpy;
        gboolean need_flush = TRUE;

        g_return_if_fail (settings_key != NULL);

        dpy = gdk_display_get_default ();
        gdk_x11_display_error_trap_push (dpy);

        /* Find the key that was modified */
        for (i = 0; i < HANDLED_KEYS; i++) {
                if (g_strcmp0 (settings_key, keys[i].settings_key) == 0) {
                        char *tmp;
                        Key  *key;

                        if (keys[i].key != NULL) {
                                need_flush = TRUE;
                                grab_key_unsafe (keys[i].key, FALSE, manager->priv->screens);
                        }

                        g_free (keys[i].key);
                        keys[i].key = NULL;

                        /* We can't have a change in a hard-coded key */
                        g_assert (keys[i].settings_key != NULL);

                        tmp = g_settings_get_string (settings,
                                                     keys[i].settings_key);

                        if (is_valid_shortcut (tmp) == FALSE) {
                                g_free (tmp);
                                break;
                        }

                        key = g_new0 (Key, 1);
                        if (!egg_accelerator_parse_virtual (tmp, &key->keysym, &key->keycodes, &key->state)) {
                                g_free (tmp);
                                g_free (key);
                                break;
                        }

                        need_flush = TRUE;
                        grab_key_unsafe (key, TRUE, manager->priv->screens);
                        keys[i].key = key;

                        g_free (tmp);

                        break;
                }
        }

        if (need_flush)
                gdk_display_flush (dpy);
        if (gdk_x11_display_error_trap_pop (dpy))
                g_warning ("Grab failed for some keys, another application may already have access the them.");
}

static void init_kbd(MsdMediaKeysManager* manager)
{
	int i;
	GdkDisplay *dpy;
	gboolean need_flush = FALSE;

	mate_settings_profile_start(NULL);

	dpy = gdk_display_get_default ();
	gdk_x11_display_error_trap_push (dpy);

	for (i = 0; i < HANDLED_KEYS; i++)
	{
		char* tmp;
		Key* key;

		gchar* signal_name;
		signal_name = g_strdup_printf ("changed::%s", keys[i].settings_key);
		g_signal_connect (manager->priv->settings,
						  signal_name,
						  G_CALLBACK (update_kbd_cb),
						  manager);
		g_free (signal_name);

		if (keys[i].settings_key != NULL) {
			tmp = g_settings_get_string (manager->priv->settings, keys[i].settings_key);
		} else {
			tmp = g_strdup (keys[i].hard_coded);
		}

		if (!is_valid_shortcut(tmp))
		{
			g_debug("Not a valid shortcut: '%s'", tmp);
			g_free(tmp);
			continue;
		}

		key = g_new0(Key, 1);

		if (!egg_accelerator_parse_virtual(tmp, &key->keysym, &key->keycodes, &key->state))
		{
			g_debug("Unable to parse: '%s'", tmp);
			g_free(tmp);
			g_free(key);
			continue;
		}

		g_free(tmp);

		keys[i].key = key;

		need_flush = TRUE;
		grab_key_unsafe(key, TRUE, manager->priv->screens);
	}

	if (need_flush)
	{
		gdk_display_flush (dpy);
	}

	if (gdk_x11_display_error_trap_pop (dpy))
	{
		g_warning("Grab failed for some keys, another application may already have access the them.");
	}

	mate_settings_profile_end(NULL);
}

static void
ensure_cancellable (GCancellable **cancellable)
{
        if (*cancellable == NULL) {
                *cancellable = g_cancellable_new ();
                g_object_add_weak_pointer (G_OBJECT (*cancellable),
                                           (gpointer *)cancellable);
        } else {
                g_object_ref (*cancellable);
        }
}

static void
dialog_show (MsdMediaKeysManager *manager)
{
        int            orig_w;
        int            orig_h;
        int            screen_w;
        int            screen_h;
        int            x;
        int            y;
        GdkDisplay    *display;
        GdkSeat       *seat;
        GdkDevice     *pointer;
        int            pointer_x;
        int            pointer_y;
        GtkRequisition win_req;
        GdkScreen     *pointer_screen;
        GdkRectangle   geometry;
        GdkMonitor    *monitor;

        gtk_window_set_screen (GTK_WINDOW (manager->priv->dialog),
                               manager->priv->current_screen);

        /* Return if OSD notifications are disabled */
        if (!g_settings_get_boolean (manager->priv->settings, "enable-osd"))
                return;

        /*
         * get the window size
         * if the window hasn't been mapped, it doesn't necessarily
         * know its true size, yet, so we need to jump through hoops
         */
        gtk_window_get_default_size (GTK_WINDOW (manager->priv->dialog), &orig_w, &orig_h);
        gtk_widget_get_preferred_size (manager->priv->dialog, NULL, &win_req);

        if (win_req.width > orig_w) {
                orig_w = win_req.width;
        }
        if (win_req.height > orig_h) {
                orig_h = win_req.height;
        }

        pointer_screen = NULL;
        display = gdk_screen_get_display (manager->priv->current_screen);
        seat = gdk_display_get_default_seat (display);
        pointer = gdk_seat_get_pointer (seat);

        gdk_device_get_position (pointer,
                                 &pointer_screen,
                                 &pointer_x,
                                 &pointer_y);

        if (pointer_screen != manager->priv->current_screen) {
                /* The pointer isn't on the current screen, so just
                 * assume the default monitor
                 */
                monitor = gdk_display_get_monitor (display, 0);
        } else {
                monitor = gdk_display_get_monitor_at_point (display, pointer_x, pointer_y);
        }

        gdk_monitor_get_geometry (monitor, &geometry);

        screen_w = geometry.width;
        screen_h = geometry.height;

        x = ((screen_w - orig_w) / 2) + geometry.x;
        y = geometry.y + (screen_h / 2) + (screen_h / 2 - orig_h) / 2;

        gtk_window_move (GTK_WINDOW (manager->priv->dialog), x, y);

        gtk_widget_show (manager->priv->dialog);

        gdk_display_sync (gdk_screen_get_display (manager->priv->current_screen));
}

static void
do_url_action (MsdMediaKeysManager *manager,
               const gchar         *scheme)
{
        GError *error = NULL;
        GAppInfo *app_info;

        app_info = g_app_info_get_default_for_uri_scheme (scheme);

        if (app_info != NULL) {
           if (!g_app_info_launch (app_info, NULL, NULL, &error)) {
                g_warning ("Could not launch '%s': %s",
                    g_app_info_get_commandline (app_info),
                    error->message);
		g_object_unref (app_info);
                g_error_free (error);
            }
        }
        else {
            g_warning ("Could not find default application for '%s' scheme", scheme);
        }
}

static void
do_media_action (MsdMediaKeysManager *manager)
{
        GError *error = NULL;
        GAppInfo *app_info;

        app_info = g_app_info_get_default_for_type ("audio/x-vorbis+ogg", FALSE);

        if (app_info != NULL) {
           if (!g_app_info_launch (app_info, NULL, NULL, &error)) {
                g_warning ("Could not launch '%s': %s",
                    g_app_info_get_commandline (app_info),
                    error->message);
                g_error_free (error);
            }
        }
        else {
            g_warning ("Could not find default application for '%s' mime-type", "audio/x-vorbis+ogg");
        }
}

static void
do_calculator_action (MsdMediaKeysManager *manager)
{
        GSettings *settings;
        char *calc;

        settings = g_settings_new ("org.mate.applications-calculator");
        calc = g_settings_get_string (settings, "exec");

        if (calc)
                execute (manager, calc, FALSE, FALSE);

        g_free (calc);
        g_object_unref (settings);
}

static void
do_messenger_action (MsdMediaKeysManager *manager)
{
        GSettings *settings;
        char *messenger;

        settings = g_settings_new ("org.mate.applications-messenger");
        messenger = g_settings_get_string (settings, "exec");

        if (messenger)
                execute (manager, messenger, FALSE, FALSE);

        g_free (messenger);
        g_object_unref (settings);
}

static void
do_shutdown_action (MsdMediaKeysManager *manager)
{
        execute (manager, "mate-session-save --shutdown-dialog", FALSE, FALSE);
}

static void
do_logout_action (MsdMediaKeysManager *manager)
{
        execute (manager, "mate-session-save --logout-dialog", FALSE, FALSE);
}

static void
do_eject_action_cb (GDrive              *drive,
                    GAsyncResult        *res,
                    MsdMediaKeysManager *manager)
{
        g_drive_eject_with_operation_finish (drive, res, NULL);
}

#define NO_SCORE 0
#define SCORE_CAN_EJECT 50
#define SCORE_HAS_MEDIA 100
static void
do_eject_action (MsdMediaKeysManager *manager)
{
        GList *drives, *l;
        GDrive *fav_drive;
        guint score;

        /* Find the best drive to eject */
        fav_drive = NULL;
        score = NO_SCORE;
        drives = g_volume_monitor_get_connected_drives (manager->priv->volume_monitor);
        for (l = drives; l != NULL; l = l->next) {
                GDrive *drive = l->data;

                if (g_drive_can_eject (drive) == FALSE)
                        continue;
                if (g_drive_is_media_removable (drive) == FALSE)
                        continue;
                if (score < SCORE_CAN_EJECT) {
                        fav_drive = drive;
                        score = SCORE_CAN_EJECT;
                }
                if (g_drive_has_media (drive) == FALSE)
                        continue;
                if (score < SCORE_HAS_MEDIA) {
                        fav_drive = drive;
                        score = SCORE_HAS_MEDIA;
                        break;
                }
        }

        /* Show the dialogue */
        dialog_init (manager);
        msd_media_keys_window_set_action_custom (MSD_MEDIA_KEYS_WINDOW (manager->priv->dialog),
                                                 "media-eject",
                                                 NULL);
        dialog_show (manager);

        /* Clean up the drive selection and exit if no suitable
         * drives are found */
        if (fav_drive != NULL)
                fav_drive = g_object_ref (fav_drive);

        g_list_foreach (drives, (GFunc) g_object_unref, NULL);
        if (fav_drive == NULL)
                return;

        /* Eject! */
        g_drive_eject_with_operation (fav_drive, G_MOUNT_UNMOUNT_FORCE,
                                      NULL, NULL,
                                      (GAsyncReadyCallback) do_eject_action_cb,
                                      manager);
        g_object_unref (fav_drive);
}

static void
do_touchpad_osd_action (MsdMediaKeysManager *manager, gboolean state)
{
        dialog_init (manager);
        msd_media_keys_window_set_action_custom (MSD_MEDIA_KEYS_WINDOW (manager->priv->dialog),
                                                 state ? "input-touchpad" : "touchpad-disabled",
                                                 state ? _("Touchpad enabled") : _("Touchpad disabled"));
        dialog_show (manager);
}

static void
do_touchpad_action (MsdMediaKeysManager *manager)
{
        GSettings *settings = g_settings_new (TOUCHPAD_SCHEMA);
        gboolean state = g_settings_get_boolean (settings, TOUCHPAD_ENABLED_KEY);

        if (touchpad_is_present () == FALSE) {
                do_touchpad_osd_action (manager, FALSE);
                return;
        }

        do_touchpad_osd_action (manager, !state);

        g_settings_set_boolean (settings, TOUCHPAD_ENABLED_KEY, !state);
        g_object_unref (settings);
}

#ifdef HAVE_LIBMATEMIXER
static void
update_dialog (MsdMediaKeysManager *manager,
               guint                volume,
               gboolean             muted,
               gboolean             sound_changed,
               gboolean             quiet,
               gboolean             is_mic)
{
        if (muted)
                volume = 0.0;

        dialog_init (manager);

        if (is_mic)
                msd_media_keys_window_set_mic_muted (MSD_MEDIA_KEYS_WINDOW (manager->priv->dialog),
                                                     muted);
        else
                msd_media_keys_window_set_volume_muted (MSD_MEDIA_KEYS_WINDOW (manager->priv->dialog),
                                                        muted);

        msd_media_keys_window_set_volume_level (MSD_MEDIA_KEYS_WINDOW (manager->priv->dialog),
                                                volume);

        msd_media_keys_window_set_action (MSD_MEDIA_KEYS_WINDOW (manager->priv->dialog),
                                          MSD_MEDIA_KEYS_WINDOW_ACTION_VOLUME);
        dialog_show (manager);

#ifdef HAVE_LIBCANBERRA
        if (quiet == FALSE && sound_changed != FALSE && muted == FALSE && is_mic == FALSE)
                ca_gtk_play_for_widget (manager->priv->dialog, 0,
                                        CA_PROP_EVENT_ID, "audio-volume-change",
                                        CA_PROP_EVENT_DESCRIPTION, "Volume changed through key press",
                                        CA_PROP_APPLICATION_NAME, PACKAGE_NAME,
                                        CA_PROP_APPLICATION_VERSION, PACKAGE_VERSION,
                                        CA_PROP_APPLICATION_ID, "org.mate.SettingsDaemon",
                                        NULL);
#endif
}

static void
do_sound_action (MsdMediaKeysManager *manager,
                 int type,
                 gboolean quiet)
{
        gboolean muted;
        gboolean muted_last;
        gboolean sound_changed = FALSE;
        guint    volume;
        guint    volume_min, volume_max;
        guint    volume_step;
        guint    volume_last;
        MateMixerStreamControl *control;

        gboolean is_input_control =
                type == MIC_MUTE_KEY ? TRUE : FALSE;
        if (is_input_control)
                control = manager->priv->source_control;
        else
                control = manager->priv->control;
        if (control == NULL)
                return;

        /* Theoretically the volume limits might be different for different
         * streams, also the minimum might not always start at 0 */
        volume_min = mate_mixer_stream_control_get_min_volume (control);
        volume_max = mate_mixer_stream_control_get_normal_volume (control);

        volume_step = g_settings_get_int (manager->priv->settings, "volume-step");
        if (volume_step <= 0 || volume_step > 100) {
                GVariant *variant = g_settings_get_default_value (manager->priv->settings, "volume-step");
                volume_step = g_variant_get_int32 (variant);
                g_variant_unref (variant);
        }

        /* Scale the volume step size accordingly to the range used by the control */
        volume_step = (volume_max - volume_min) * volume_step / 100;

        volume = volume_last =
                mate_mixer_stream_control_get_volume (control);
        muted = muted_last =
                mate_mixer_stream_control_get_mute (control);

        switch (type) {
        case MUTE_KEY:
        case MIC_MUTE_KEY:
                muted = !muted;
                break;
        case VOLUME_DOWN_KEY:
                if (volume <= (volume_min + volume_step)) {
                        volume = volume_min;
                        muted  = TRUE;
                } else {
                        volume -= volume_step;
                        muted  = FALSE;
                }
                break;
        case VOLUME_UP_KEY:
                if (muted) {
                        muted = FALSE;
                        if (volume <= volume_min)
                               volume = volume_min + volume_step;
                } else
                        volume = CLAMP (volume + volume_step,
                                        volume_min,
                                        volume_max);
                break;
        }

        if (muted != muted_last) {
                if (mate_mixer_stream_control_set_mute (control, muted))
                        sound_changed = TRUE;
                else
                        muted = muted_last;
        }

        if (volume != mate_mixer_stream_control_get_volume (control)) {
                if (mate_mixer_stream_control_set_volume (control, volume))
                        sound_changed = TRUE;
                else
                        volume = volume_last;
        }

        update_dialog (manager,
                       CLAMP (100 * volume / (volume_max - volume_min), 0, 100),
                       muted,
                       sound_changed,
                       quiet,
                       is_input_control);
}

static void
update_default_output (MsdMediaKeysManager *manager)
{
        MateMixerStream        *stream;
        MateMixerStreamControl *control = NULL;

        stream = mate_mixer_context_get_default_output_stream (manager->priv->context);
        if (stream != NULL)
                control = mate_mixer_stream_get_default_control (stream);

        if (stream == manager->priv->stream)
                return;

        g_clear_object (&manager->priv->stream);
        g_clear_object (&manager->priv->control);

        if (control != NULL) {
                MateMixerStreamControlFlags flags = mate_mixer_stream_control_get_flags (control);

                /* Do not use the stream if it is not possible to mute it or
                 * change the volume */
                if (!(flags & MATE_MIXER_STREAM_CONTROL_MUTE_WRITABLE) &&
                    !(flags & MATE_MIXER_STREAM_CONTROL_VOLUME_WRITABLE))
                        return;

                manager->priv->stream  = g_object_ref (stream);
                manager->priv->control = g_object_ref (control);
                g_debug ("Default output stream updated to %s",
                         mate_mixer_stream_get_name (stream));
        } else
                g_debug ("Default output stream unset");
}

static void
update_default_input (MsdMediaKeysManager *manager)
{
        MateMixerStream        *stream;
        MateMixerStreamControl *control = NULL;

        stream = mate_mixer_context_get_default_input_stream (manager->priv->context);
        if (stream != NULL)
                control = mate_mixer_stream_get_default_control (stream);

        if (stream == manager->priv->source_stream)
                return;

        g_clear_object (&manager->priv->source_stream);
        g_clear_object (&manager->priv->source_control);

        if (control != NULL) {
                MateMixerStreamControlFlags flags = mate_mixer_stream_control_get_flags (control);

                /* Do not use the stream if it is not possible to mute it or
                 * change the volume */
                if (!(flags & MATE_MIXER_STREAM_CONTROL_MUTE_WRITABLE))
                        return;

                manager->priv->source_stream  = g_object_ref (stream);
                manager->priv->source_control = g_object_ref (control);
                g_debug ("Default input stream updated to %s",
                         mate_mixer_stream_get_name (stream));
        } else
                g_debug ("Default input stream unset");
}

static void
on_context_state_notify (MateMixerContext    *context,
                         GParamSpec          *pspec,
                         MsdMediaKeysManager *manager)
{
        update_default_output (manager);
        update_default_input (manager);
}

static void
on_context_default_output_notify (MateMixerContext    *context,
                                  GParamSpec          *pspec,
                                  MsdMediaKeysManager *manager)
{
        update_default_output (manager);
}

static void
on_context_default_input_notify (MateMixerContext    *context,
                                 GParamSpec          *pspec,
                                 MsdMediaKeysManager *manager)
{
        update_default_input (manager);
}

static void
on_context_stream_removed (MateMixerContext    *context,
                           const gchar         *name,
                           MsdMediaKeysManager *manager)
{
        if (manager->priv->stream != NULL) {
                MateMixerStream *stream =
                        mate_mixer_context_get_stream (manager->priv->context, name);

                if (stream == manager->priv->stream) {
                        g_clear_object (&manager->priv->stream);
                        g_clear_object (&manager->priv->control);
                }
        }
        if (manager->priv->source_stream != NULL) {
                MateMixerStream *stream =
                        mate_mixer_context_get_stream (manager->priv->context, name);

                if (stream == manager->priv->source_stream) {
                        g_clear_object (&manager->priv->source_stream);
                        g_clear_object (&manager->priv->source_control);
                }
        }
}
#endif /* HAVE_LIBMATEMIXER */

static gboolean
get_rfkill_property (MsdMediaKeysManager *manager,
                     const char          *property)
{
        GVariant *v;
        gboolean ret;

        v = g_dbus_proxy_get_cached_property (manager->priv->rfkill_proxy, property);
        if (!v)
                return FALSE;
        ret = g_variant_get_boolean (v);
        g_variant_unref (v);

        return ret;
}

typedef struct {
        MsdMediaKeysManager *manager;
        char *property;
        gboolean bluetooth;
        gboolean target_state;
} RfkillData;

static void
set_rfkill_complete (GObject      *object,
                     GAsyncResult *result,
                     gpointer      user_data)
{
        GError *error = NULL;
        GVariant *variant;
        RfkillData *data = user_data;

        variant = g_dbus_proxy_call_finish (G_DBUS_PROXY (object), result, &error);

        if (variant == NULL) {
                if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
                        g_warning ("Failed to set '%s' property: %s", data->property, error->message);
                g_error_free (error);
                goto out;
        }
        g_variant_unref (variant);

        g_debug ("Finished changing rfkill, property %s is now %s",
                 data->property, data->target_state ? "true" : "false");

        if (data->bluetooth){
                if (data->target_state)
                        msd_media_keys_window_set_action_custom (MSD_MEDIA_KEYS_WINDOW (data->manager->priv->dialog),
                                                                 "bluetooth-disabled-symbolic", _("Bluetooth disabled"));
                else
                        msd_media_keys_window_set_action_custom (MSD_MEDIA_KEYS_WINDOW (data->manager->priv->dialog),
                                                                 "bluetooth-active-symbolic", _("Bluetooth enabled"));
        } else {
                if (data->target_state)
                        msd_media_keys_window_set_action_custom (MSD_MEDIA_KEYS_WINDOW (data->manager->priv->dialog),
                                                                 "airplane-mode-symbolic", _("Airplane mode enabled"));
                else
                        msd_media_keys_window_set_action_custom (MSD_MEDIA_KEYS_WINDOW (data->manager->priv->dialog),
                                                                 "network-wireless-signal-excellent-symbolic", _("Airplane mode disabled"));
        }
        dialog_show (data->manager);
out:
        g_free (data->property);
        g_free (data);
}

static void
do_rfkill_action (MsdMediaKeysManager *manager,
                  gboolean             bluetooth)
{
        const char *has_mode, *hw_mode, *mode;
        gboolean new_state;
        RfkillData *data;

        dialog_init (manager);

        has_mode = bluetooth ? "BluetoothHasAirplaneMode" : "HasAirplaneMode";
        hw_mode = bluetooth ? "BluetoothHardwareAirplaneMode" : "HardwareAirplaneMode";
        mode = bluetooth ? "BluetoothAirplaneMode" : "AirplaneMode";

        if (manager->priv->rfkill_proxy == NULL)
                return;

        if (get_rfkill_property (manager, has_mode) == FALSE)
                return;

        if (get_rfkill_property (manager, hw_mode)) {
                msd_media_keys_window_set_action_custom (MSD_MEDIA_KEYS_WINDOW (manager->priv->dialog),
                                                        "airplane-mode-symbolic",
                                                        _("Hardware Airplane Mode"));
                dialog_show (manager);
                return;
        }

        new_state = !get_rfkill_property (manager, mode);
        data = g_new0 (RfkillData, 1);
        data->manager = manager;
        data->property = g_strdup (mode);
        data->bluetooth = bluetooth;
        data->target_state = new_state;
        g_dbus_proxy_call (manager->priv->rfkill_proxy,
                           "org.freedesktop.DBus.Properties.Set",
                           g_variant_new ("(ssv)",
                                          "org.mate.SettingsDaemon.Rfkill",
                                          data->property,
                                          g_variant_new_boolean (new_state)),
                           G_DBUS_CALL_FLAGS_NONE, -1,
                           manager->priv->rfkill_cancellable,
                           set_rfkill_complete, data);

        g_debug ("Setting rfkill property %s to %s",
                 data->property, new_state ? "true" : "false");
}

static void
do_display_osd_action (MsdMediaKeysManager *manager)
{
        GdkDisplay *display;
        int         n_monitors;

        display = gdk_display_get_default ();
        n_monitors = gdk_display_get_n_monitors (display);

        dialog_init (manager);
        if (n_monitors > 1)
                msd_media_keys_window_set_action_custom (MSD_MEDIA_KEYS_WINDOW (manager->priv->dialog),
                                                         "video-joined-displays-symbolic",
                                                         _("Changing Screen Layout"));
        else
                msd_media_keys_window_set_action_custom (MSD_MEDIA_KEYS_WINDOW (manager->priv->dialog),
                                                         "video-single-display-symbolic",
                                                         _("No External Display"));
        dialog_show (manager);
}

static gint
find_by_application (gconstpointer a,
                     gconstpointer b)
{
        return strcmp (((MediaPlayer *)a)->application, b);
}

static gint
find_by_time (gconstpointer a,
              gconstpointer b)
{
        return ((MediaPlayer *)a)->time < ((MediaPlayer *)b)->time;
}

/*
 * Register a new media player. Most applications will want to call
 * this with time = GDK_CURRENT_TIME. This way, the last registered
 * player will receive media events. In some cases, applications
 * may want to register with a lower priority (usually 1), to grab
 * events only nobody is interested.
 */
gboolean
msd_media_keys_manager_grab_media_player_keys (MsdMediaKeysManager *manager,
                                               const char          *application,
                                               guint32              time,
                                               GError             **error)
{
        GList       *iter;
        MediaPlayer *media_player;

        if (time == GDK_CURRENT_TIME) {
                GTimeVal tv;

                g_get_current_time (&tv);
                time = tv.tv_sec * 1000 + tv.tv_usec / 1000;
        }

        iter = g_list_find_custom (manager->priv->media_players,
                                   application,
                                   find_by_application);

        if (iter != NULL) {
                if (((MediaPlayer *)iter->data)->time < time) {
                        g_free (((MediaPlayer *)iter->data)->application);
                        g_free (iter->data);
                        manager->priv->media_players = g_list_delete_link (manager->priv->media_players, iter);
                } else {
                        return TRUE;
                }
        }

        g_debug ("Registering %s at %u", application, time);
        media_player = g_new0 (MediaPlayer, 1);
        media_player->application = g_strdup (application);
        media_player->time = time;

        manager->priv->media_players = g_list_insert_sorted (manager->priv->media_players,
                                                             media_player,
                                                             find_by_time);

        return TRUE;
}

gboolean
msd_media_keys_manager_release_media_player_keys (MsdMediaKeysManager *manager,
                                                  const char          *application,
                                                  GError             **error)
{
        GList *iter;

        iter = g_list_find_custom (manager->priv->media_players,
                                   application,
                                   find_by_application);

        if (iter != NULL) {
                g_debug ("Deregistering %s", application);
                g_free (((MediaPlayer *)iter->data)->application);
                g_free (iter->data);
                manager->priv->media_players = g_list_delete_link (manager->priv->media_players, iter);
        }

        return TRUE;
}

static gboolean
msd_media_player_key_pressed (MsdMediaKeysManager *manager,
                              const char          *key)
{
        const char *application = NULL;
        gboolean    have_listeners;

        have_listeners = (manager->priv->media_players != NULL);

        if (have_listeners) {
                application = ((MediaPlayer *)manager->priv->media_players->data)->application;
        }

        g_signal_emit (manager, signals[MEDIA_PLAYER_KEY_PRESSED], 0, application, key);

        return !have_listeners;
}

static gboolean
do_multimedia_player_action (MsdMediaKeysManager *manager,
                             const char          *key)
{
        return msd_media_player_key_pressed (manager, key);
}

static void
do_toggle_accessibility_key (const char *key)
{
        GSettings *settings;
        gboolean state;

        settings = g_settings_new ("org.gnome.desktop.a11y.applications");
        state = g_settings_get_boolean (settings, key);
        g_settings_set_boolean (settings, key, !state);
        g_object_unref (settings);
}

static void
do_magnifier_action (MsdMediaKeysManager *manager)
{
        do_toggle_accessibility_key ("screen-magnifier-enabled");
}

static void
do_screenreader_action (MsdMediaKeysManager *manager)
{
        do_toggle_accessibility_key ("screen-reader-enabled");
}

static void
do_on_screen_keyboard_action (MsdMediaKeysManager *manager)
{
        do_toggle_accessibility_key ("screen-keyboard-enabled");
}

static gboolean
do_action (MsdMediaKeysManager *manager,
           int                  type)
{
        char *cmd;
        char *path;

        switch (type) {
        case TOUCHPAD_KEY:
                do_touchpad_action (manager);
                break;
        case TOUCHPAD_ON_KEY:
                do_touchpad_osd_action(manager, TRUE);
                break;
        case TOUCHPAD_OFF_KEY:
                do_touchpad_osd_action(manager, FALSE);
                break;
        case MUTE_KEY:
        case VOLUME_DOWN_KEY:
        case VOLUME_UP_KEY:
        case MIC_MUTE_KEY:
#ifdef HAVE_LIBMATEMIXER
                do_sound_action (manager, type, FALSE);
#endif
                break;
        case MUTE_QUIET_KEY:
#ifdef HAVE_LIBMATEMIXER
                do_sound_action (manager, MUTE_KEY, TRUE);
#endif
                break;
        case VOLUME_DOWN_QUIET_KEY:
#ifdef HAVE_LIBMATEMIXER
                do_sound_action (manager, VOLUME_DOWN_KEY, TRUE);
#endif
                break;
        case VOLUME_UP_QUIET_KEY:
#ifdef HAVE_LIBMATEMIXER
                do_sound_action (manager, VOLUME_UP_KEY, TRUE);
#endif
                break;
        case POWER_KEY:
                do_shutdown_action (manager);
                break;
	case LOGOUT_KEY:
		do_logout_action (manager);
		break;
        case EJECT_KEY:
                do_eject_action (manager);
                break;
        case HOME_KEY:
                path = g_shell_quote (g_get_home_dir ());
                cmd = g_strconcat ("caja --no-desktop ", path, NULL);
                g_free (path);
                execute (manager, cmd, FALSE, FALSE);
                g_free (cmd);
                break;
        case SEARCH_KEY:
                cmd = NULL;
                if ((cmd = g_find_program_in_path ("beagle-search"))) {
                        execute (manager, "beagle-search", FALSE, FALSE);
                } else if ((cmd = g_find_program_in_path ("tracker-search-tool"))) {
                        execute (manager, "tracker-search-tool", FALSE, FALSE);
                } else {
                        execute (manager, "mate-search-tool", FALSE, FALSE);
                }
                g_free (cmd);
                break;
        case EMAIL_KEY:
                do_url_action (manager, "mailto");
                break;
        case CONTROL_CENTER_KEY:
                if ((cmd = g_find_program_in_path ("mate-control-center")))
                        execute (manager, "mate-control-center", FALSE, FALSE);

                g_free (cmd);
                break;
        case SCREENSAVER_KEY:
                if ((cmd = g_find_program_in_path ("mate-screensaver-command"))) {
                        execute (manager, "mate-screensaver-command --lock", FALSE, FALSE);
                } else {
                        execute (manager, "xscreensaver-command -lock", FALSE, FALSE);
                }

                g_free (cmd);
                break;
        case HELP_KEY:
                do_url_action (manager, "help");
                break;
        case WWW_KEY:
                do_url_action (manager, "http");
                break;
        case MEDIA_KEY:
                do_media_action (manager);
                break;
        case CALCULATOR_KEY:
                do_calculator_action (manager);
                break;
        case MESSENGER_KEY:
                do_messenger_action (manager);
                break;
        case PLAY_KEY:
                return do_multimedia_player_action (manager, "Play");
        case PAUSE_KEY:
                return do_multimedia_player_action (manager, "Pause");
        case STOP_KEY:
                return do_multimedia_player_action (manager, "Stop");
        case PREVIOUS_KEY:
                return do_multimedia_player_action (manager, "Previous");
        case NEXT_KEY:
                return do_multimedia_player_action (manager, "Next");
        case REWIND_KEY:
                return do_multimedia_player_action (manager, "Rewind");
        case FORWARD_KEY:
                return do_multimedia_player_action (manager, "FastForward");
        case REPEAT_KEY:
                return do_multimedia_player_action (manager, "Repeat");
        case RANDOM_KEY:
                return do_multimedia_player_action (manager, "Shuffle");
        case MAGNIFIER_KEY:
                do_magnifier_action (manager);
                break;
        case SCREENREADER_KEY:
                do_screenreader_action (manager);
                break;
        case ON_SCREEN_KEYBOARD_KEY:
                do_on_screen_keyboard_action (manager);
                break;
        case RFKILL_KEY:
                do_rfkill_action (manager, FALSE);
                break;
        case BLUETOOTH_RFKILL_KEY:
                do_rfkill_action (manager, TRUE);
                break;
        case DISPLAY_KEY:
                do_display_osd_action (manager);
                break;
        default:
                g_assert_not_reached ();
        }

        return FALSE;
}

static GdkScreen *
acme_get_screen_from_event (MsdMediaKeysManager *manager,
                            XAnyEvent           *xanyev)
{
        GdkWindow *window;
        GdkScreen *screen;
        GSList    *l;

        /* Look for which screen we're receiving events */
        for (l = manager->priv->screens; l != NULL; l = l->next) {
                screen = (GdkScreen *) l->data;
                window = gdk_screen_get_root_window (screen);

                if (GDK_WINDOW_XID (window) == xanyev->window) {
                        return screen;
                }
        }

        return NULL;
}

static GdkFilterReturn
acme_filter_events (GdkXEvent           *xevent,
                    GdkEvent            *event,
                    MsdMediaKeysManager *manager)
{
        XEvent    *xev = (XEvent *) xevent;
        XAnyEvent *xany = (XAnyEvent *) xevent;
        int        i;

        /* verify we have a key event */
        if (xev->type != KeyPress) {
                return GDK_FILTER_CONTINUE;
        }

        for (i = 0; i < HANDLED_KEYS; i++) {
                if (match_key (keys[i].key, xev)) {
                        switch (keys[i].key_type) {
                        case VOLUME_DOWN_KEY:
                        case VOLUME_UP_KEY:
                        case VOLUME_DOWN_QUIET_KEY:
                        case VOLUME_UP_QUIET_KEY:
                                /* auto-repeatable keys */
                                if (xev->type != KeyPress) {
                                        return GDK_FILTER_CONTINUE;
                                }
                                break;
                        }

                        manager->priv->current_screen = acme_get_screen_from_event (manager, xany);

                        if (do_action (manager, keys[i].key_type) == FALSE) {
                                return GDK_FILTER_REMOVE;
                        } else {
                                return GDK_FILTER_CONTINUE;
                        }
                }
        }

        return GDK_FILTER_CONTINUE;
}

static void
on_rfkill_proxy_ready (GObject      *source,
                       GAsyncResult *result,
                       gpointer      data)
{
        MsdMediaKeysManager *manager = data;

        manager->priv->rfkill_proxy =
                g_dbus_proxy_new_for_bus_finish (result, NULL);
}

static void
rfkill_appeared_cb (GDBusConnection *connection,
                    const gchar     *name,
                    const gchar     *name_owner,
                    gpointer         user_data)
{
        MsdMediaKeysManager *manager = user_data;

        g_dbus_proxy_new_for_bus (G_BUS_TYPE_SESSION,
                                  0, NULL,
                                  "org.mate.SettingsDaemon.Rfkill",
                                  "/org/mate/SettingsDaemon/Rfkill",
                                  "org.mate.SettingsDaemon.Rfkill",
                                  manager->priv->rfkill_cancellable,
                                  on_rfkill_proxy_ready, manager);
}

static gboolean
start_media_keys_idle_cb (MsdMediaKeysManager *manager)
{
        GSList *l;
        GdkDisplay *dpy;
        Display *xdpy;

        g_debug ("Starting media_keys manager");
        mate_settings_profile_start (NULL);

        dpy = gdk_display_get_default ();
        xdpy = GDK_DISPLAY_XDISPLAY (dpy);

        manager->priv->volume_monitor = g_volume_monitor_get ();
        manager->priv->settings = g_settings_new (BINDING_SCHEMA);

        ensure_cancellable (&manager->priv->rfkill_cancellable);

        init_screens (manager);
        init_kbd (manager);

        /* Start filtering the events */
        for (l = manager->priv->screens; l != NULL; l = l->next) {
                GdkWindow *window;
                Window xwindow;
                XWindowAttributes atts;

                mate_settings_profile_start ("gdk_window_add_filter");

                window = gdk_screen_get_root_window (l->data);
                xwindow = GDK_WINDOW_XID (window);

                g_debug ("adding key filter for screen: %d",
                         gdk_x11_screen_get_screen_number (l->data));

                gdk_window_add_filter (window,
                                       (GdkFilterFunc)acme_filter_events,
                                       manager);

                gdk_x11_display_error_trap_push (dpy);
                /* Add KeyPressMask to the currently reportable event masks */
                XGetWindowAttributes (xdpy, xwindow, &atts);
                XSelectInput (xdpy, xwindow, atts.your_event_mask | KeyPressMask);
                gdk_x11_display_error_trap_pop_ignored (dpy);

                mate_settings_profile_end ("gdk_window_add_filter");
        }

        manager->priv->rfkill_watch_id = g_bus_watch_name (G_BUS_TYPE_SESSION,
                                                           "org.mate.SettingsDaemon.Rfkill",
                                                           G_BUS_NAME_WATCHER_FLAGS_NONE,
                                                           rfkill_appeared_cb,
                                                           NULL,
                                                           manager, NULL);

        mate_settings_profile_end (NULL);

        return FALSE;
}

gboolean
msd_media_keys_manager_start (MsdMediaKeysManager *manager, GError **error)
{
        mate_settings_profile_start (NULL);

#ifdef HAVE_LIBMATEMIXER
        if (G_LIKELY (mate_mixer_is_initialized ())) {
                mate_settings_profile_start ("mate_mixer_context_new");

                manager->priv->context = mate_mixer_context_new ();

                g_signal_connect (manager->priv->context,
                                  "notify::state",
                                  G_CALLBACK (on_context_state_notify),
                                  manager);
                g_signal_connect (manager->priv->context,
                                  "notify::default-output-stream",
                                  G_CALLBACK (on_context_default_output_notify),
                                  manager);
                g_signal_connect (manager->priv->context,
                                  "notify::default-input-stream",
                                  G_CALLBACK (on_context_default_input_notify),
                                  manager);
                g_signal_connect (manager->priv->context,
                                  "stream-removed",
                                  G_CALLBACK (on_context_stream_removed),
                                  manager);

                mate_mixer_context_open (manager->priv->context);

                mate_settings_profile_end ("mate_mixer_context_new");
        }
#endif
        g_idle_add ((GSourceFunc) start_media_keys_idle_cb, manager);

        mate_settings_profile_end (NULL);

        return TRUE;
}

void
msd_media_keys_manager_stop (MsdMediaKeysManager *manager)
{
        MsdMediaKeysManagerPrivate *priv = manager->priv;
        GdkDisplay *dpy;
        GSList *ls;
        GList *l;
        int i;
        gboolean need_flush;

        g_debug ("Stopping media_keys manager");

        for (ls = priv->screens; ls != NULL; ls = ls->next) {
                gdk_window_remove_filter (gdk_screen_get_root_window (ls->data),
                                          (GdkFilterFunc) acme_filter_events,
                                          manager);
        }

        if (manager->priv->rfkill_watch_id > 0) {
                g_bus_unwatch_name (manager->priv->rfkill_watch_id);
                manager->priv->rfkill_watch_id = 0;
        }

        if (priv->settings != NULL) {
                g_object_unref (priv->settings);
                priv->settings = NULL;
        }

        if (priv->volume_monitor != NULL) {
                g_object_unref (priv->volume_monitor);
                priv->volume_monitor = NULL;
        }

        if (priv->connection != NULL) {
                dbus_g_connection_unref (priv->connection);
                priv->connection = NULL;
        }

        need_flush = FALSE;
        dpy = gdk_display_get_default ();
        gdk_x11_display_error_trap_push (dpy);

        for (i = 0; i < HANDLED_KEYS; ++i) {
                if (keys[i].key) {
                        need_flush = TRUE;
                        grab_key_unsafe (keys[i].key, FALSE, priv->screens);

                        g_free (keys[i].key->keycodes);
                        g_free (keys[i].key);
                        keys[i].key = NULL;
                }
        }

        if (need_flush)
                gdk_display_flush (dpy);

        gdk_x11_display_error_trap_pop_ignored (dpy);

        g_slist_free (priv->screens);
        priv->screens = NULL;

        if (priv->rfkill_cancellable != NULL) {
                g_cancellable_cancel (priv->rfkill_cancellable);
                g_clear_object (&priv->rfkill_cancellable);
        }

#ifdef HAVE_LIBMATEMIXER
        g_clear_object (&priv->stream);
        g_clear_object (&priv->source_stream);
        g_clear_object (&priv->control);
        g_clear_object (&priv->source_control);
        g_clear_object (&priv->context);
#endif

        if (priv->dialog != NULL) {
                gtk_widget_destroy (priv->dialog);
                priv->dialog = NULL;
        }

        for (l = priv->media_players; l; l = l->next) {
                MediaPlayer *mp = l->data;
                g_free (mp->application);
                g_free (mp);
        }
        g_list_free (priv->media_players);
        priv->media_players = NULL;
}

static void
msd_media_keys_manager_class_init (MsdMediaKeysManagerClass *klass)
{
        signals[MEDIA_PLAYER_KEY_PRESSED] =
                g_signal_new ("media-player-key-pressed",
                              G_OBJECT_CLASS_TYPE (klass),
                              G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (MsdMediaKeysManagerClass, media_player_key_pressed),
                              NULL,
                              NULL,
                              msd_marshal_VOID__STRING_STRING,
                              G_TYPE_NONE,
                              2,
                              G_TYPE_STRING,
                              G_TYPE_STRING);

        dbus_g_object_type_install_info (MSD_TYPE_MEDIA_KEYS_MANAGER, &dbus_glib_msd_media_keys_manager_object_info);
}

static void
msd_media_keys_manager_init (MsdMediaKeysManager *manager)
{
        manager->priv = msd_media_keys_manager_get_instance_private (manager);
}

static gboolean
register_manager (MsdMediaKeysManager *manager)
{
        GError *error = NULL;

        manager->priv->connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
        if (manager->priv->connection == NULL) {
                if (error != NULL) {
                        g_error ("Error getting session bus: %s", error->message);
                        g_error_free (error);
                }
                return FALSE;
        }

        dbus_g_connection_register_g_object (manager->priv->connection, MSD_MEDIA_KEYS_DBUS_PATH, G_OBJECT (manager));

        return TRUE;
}

MsdMediaKeysManager *
msd_media_keys_manager_new (void)
{
        if (manager_object != NULL) {
                g_object_ref (manager_object);
        } else {
                gboolean res;

                manager_object = g_object_new (MSD_TYPE_MEDIA_KEYS_MANAGER, NULL);
                g_object_add_weak_pointer (manager_object,
                                           (gpointer *) &manager_object);
                res = register_manager (manager_object);
                if (! res) {
                        g_object_unref (manager_object);
                        return NULL;
                }
        }

        return MSD_MEDIA_KEYS_MANAGER (manager_object);
}
