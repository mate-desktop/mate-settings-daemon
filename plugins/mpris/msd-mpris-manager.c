/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2013 Stefano Karapetsas <stefano@karapetsas.com>
 *               2013 Steve Zesch <stevezesch2@gmail.com>
 *               2007 William Jon McCann <mccann@jhu.edu>
 *               2007 Jan Arne Petersen <jap@gnome.org>
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
 * Authors:
 *      Stefano Karapetsas <stefano@karapetsas.com>
 *      Steve Zesch <stevezesch2@gmail.com>
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
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>

#include "mate-settings-profile.h"
#include "msd-mpris-manager.h"

#define MPRIS_OBJECT_PATH  "/org/mpris/MediaPlayer2"
#define MPRIS_INTERFACE    "org.mpris.MediaPlayer2.Player"
#define MPRIS_PREFIX       "org.mpris.MediaPlayer2."

/* Number of media players supported.
 * Correlates to the number of elements in BUS_NAMES */
static const gint NUM_BUS_NAMES = 16;

/* Names to we want to watch */
static const gchar *BUS_NAMES[] = {"org.mpris.MediaPlayer2.audacious",
                                   "org.mpris.MediaPlayer2.clementine",
                                   "org.mpris.MediaPlayer2.vlc",
                                   "org.mpris.MediaPlayer2.mpd",
                                   "org.mpris.MediaPlayer2.exaile",
                                   "org.mpris.MediaPlayer2.banshee",
                                   "org.mpris.MediaPlayer2.rhythmbox",
                                   "org.mpris.MediaPlayer2.pragha",
                                   "org.mpris.MediaPlayer2.quodlibet",
                                   "org.mpris.MediaPlayer2.guayadeque",
                                   "org.mpris.MediaPlayer2.amarok",
                                   "org.mpris.MediaPlayer2.nuvolaplayer",
                                   "org.mpris.MediaPlayer2.xbmc",
                                   "org.mpris.MediaPlayer2.xnoise",
                                   "org.mpris.MediaPlayer2.gmusicbrowser",
                                   "org.mpris.MediaPlayer2.spotify"};

#define MSD_MPRIS_MANAGER_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), MSD_TYPE_MPRIS_MANAGER, MsdMprisManagerPrivate))

struct MsdMprisManagerPrivate
{
        GQueue       *media_player_queue;
        GDBusProxy   *media_keys_proxy;
        guint         watch_id;
};

enum {
        PROP_0,
};

static void     msd_mpris_manager_class_init  (MsdMprisManagerClass *klass);
static void     msd_mpris_manager_init        (MsdMprisManager      *mpris_manager);
static void     msd_mpris_manager_finalize    (GObject              *object);

G_DEFINE_TYPE (MsdMprisManager, msd_mpris_manager, G_TYPE_OBJECT)

static gpointer manager_object = NULL;

/* Returns the name of the media player.
 * User must free. */
static gchar*
get_player_name(const gchar *name)
{
    gchar **tokens;
    gchar *player_name;

    /* max_tokens is 4 because a player could have additional instances,
     * like org.mpris.MediaPlayer2.vlc.instance7389 */
    tokens = g_strsplit (name, ".", 4);
    player_name = g_strdup (tokens[3]);
    g_strfreev (tokens);

    return player_name;
}

/* A media player was just run and should be
 * added to the head of media_player_queue. */
static void
mp_name_appeared (GDBusConnection  *connection,
                  const gchar      *name,
                  const gchar      *name_owner,
                  MsdMprisManager  *manager)
{
    gchar *player_name;

    g_debug ("MPRIS Name acquired: %s\n", name);

    player_name = get_player_name(name);
    g_queue_push_head (manager->priv->media_player_queue,
                       player_name);
}

/* A media player quit running and should be
 * removed from media_player_queue. */
static void
mp_name_vanished (GDBusConnection *connection,
                  const gchar     *name,
                  MsdMprisManager *manager)
{
    gchar *player_name;

    if (g_queue_is_empty (manager->priv->media_player_queue))
        return;

    g_debug ("MPRIS Name vanished: %s\n", name);

    player_name = get_player_name(name);

    g_queue_remove (manager->priv->media_player_queue, player_name);

    g_free (player_name);
}

/* Code copied from Totem media player
 * src/plugins/media-player-keys/totem-media-player-keys.c */
static void
on_media_player_key_pressed (MsdMprisManager  *manager,
                             const gchar      *key)
{
    GDBusProxyFlags flags = G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START;
    GDBusProxy *proxy = NULL;
    GError *error = NULL;
    const char *mpris_key = NULL;
    const char *mpris_head = NULL;
    char *mpris_name = NULL;

    if (g_queue_is_empty (manager->priv->media_player_queue))
        return;

    if (strcmp ("Play", key) == 0)
        mpris_key = "PlayPause";
    else if (strcmp ("Pause", key) == 0)
        mpris_key = "Pause";
    else if (strcmp ("Previous", key) == 0)
        mpris_key = "Previous";
    else if (strcmp ("Next", key) == 0)
        mpris_key = "Next";
    else if (strcmp ("Stop", key) == 0)
        mpris_key = "Stop";

    if (mpris_key != NULL)
    {
        mpris_head = g_queue_peek_head (manager->priv->media_player_queue);
        mpris_name = g_strdup_printf (MPRIS_PREFIX "%s", mpris_head);
        
        g_debug ("MPRIS Sending '%s' to '%s'!", mpris_key, mpris_head);

        proxy = g_dbus_proxy_new_for_bus_sync(G_BUS_TYPE_SESSION,
                                              flags,
                                              NULL,
                                              mpris_name,
                                              MPRIS_OBJECT_PATH,
                                              MPRIS_INTERFACE,
                                              NULL,
                                              &error);
        g_free (mpris_name);
        if (proxy == NULL)
        {
            g_printerr("Error creating proxy: %s\n", error->message);
            g_error_free(error);
            return;
        }

        g_dbus_proxy_call (proxy, mpris_key, NULL,
                           G_DBUS_CALL_FLAGS_NONE,
                           -1, NULL, NULL, NULL);
        g_object_unref (proxy);

    }
}

static void
grab_media_player_keys_cb (GDBusProxy       *proxy,
                           GAsyncResult     *res,
                           MsdMprisManager  *manager)
{
    GVariant *variant;
    GError *error = NULL;

    variant = g_dbus_proxy_call_finish (proxy, res, &error);

    if (variant == NULL) {
        if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
            g_warning ("Failed to call \"GrabMediaPlayerKeys\": %s", error->message);
        g_error_free (error);
        return;
    }
    g_variant_unref (variant);
}

static void
grab_media_player_keys (MsdMprisManager *manager)
{
    if (manager->priv->media_keys_proxy == NULL)
        return;

    g_dbus_proxy_call (manager->priv->media_keys_proxy,
                      "GrabMediaPlayerKeys",
                      g_variant_new ("(su)", "MsdMpris", 0),
                      G_DBUS_CALL_FLAGS_NONE,
                      -1, NULL,
                      (GAsyncReadyCallback) grab_media_player_keys_cb,
                      manager);
}

static void
key_pressed (GDBusProxy          *proxy,
             gchar               *sender_name,
             gchar               *signal_name,
             GVariant            *parameters,
             MsdMprisManager     *manager)
{
    char *app, *cmd;

    if (g_strcmp0 (signal_name, "MediaPlayerKeyPressed") != 0)
        return;
    g_variant_get (parameters, "(ss)", &app, &cmd);
    if (g_strcmp0 (app, "MsdMpris") == 0) {
        on_media_player_key_pressed (manager, cmd);
    }
    g_free (app);
    g_free (cmd);
}

static void
got_proxy_cb (GObject           *source_object,
              GAsyncResult      *res,
              MsdMprisManager   *manager)
{
    GError *error = NULL;

    manager->priv->media_keys_proxy = g_dbus_proxy_new_for_bus_finish (res, &error);

    if (manager->priv->media_keys_proxy == NULL) {
        if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
            g_warning ("Failed to contact settings daemon: %s", error->message);
        g_error_free (error);
        return;
    }

    grab_media_player_keys (manager);

    g_signal_connect (G_OBJECT (manager->priv->media_keys_proxy), "g-signal",
                      G_CALLBACK (key_pressed), manager);
}

static void
msd_name_appeared (GDBusConnection     *connection,
                   const gchar         *name,
                   const gchar         *name_owner,
                   MsdMprisManager     *manager)
{
    g_dbus_proxy_new_for_bus (G_BUS_TYPE_SESSION,
                              G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES |
                              G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START,
                              NULL,
                              "org.mate.SettingsDaemon",
                              "/org/mate/SettingsDaemon/MediaKeys",
                              "org.mate.SettingsDaemon.MediaKeys",
                              NULL,
                              (GAsyncReadyCallback) got_proxy_cb,
                              manager);
}

static void
msd_name_vanished (GDBusConnection   *connection,
                   const gchar       *name,
                   MsdMprisManager   *manager)
{
    if (manager->priv->media_keys_proxy != NULL) {
        g_object_unref (manager->priv->media_keys_proxy);
        manager->priv->media_keys_proxy = NULL;
    }
}


gboolean
msd_mpris_manager_start (MsdMprisManager   *manager,
                         GError           **error)
{
    GBusNameWatcherFlags flags = G_BUS_NAME_WATCHER_FLAGS_NONE;
    int i;
    
    g_debug ("Starting mpris manager");
    mate_settings_profile_start (NULL);

    manager->priv->media_player_queue = g_queue_new();

    /* Register all the names we wish to watch.*/
    for (i = 0; i < NUM_BUS_NAMES; i++)
    {
        g_bus_watch_name(G_BUS_TYPE_SESSION,
                         BUS_NAMES[i],
                         flags,
                         mp_name_appeared,
                         mp_name_vanished,
                         manager,
                         NULL);
    }


    manager->priv->watch_id = g_bus_watch_name (G_BUS_TYPE_SESSION,
                                                "org.mate.SettingsDaemon",
                                                G_BUS_NAME_WATCHER_FLAGS_NONE,
                                                (GBusNameAppearedCallback) msd_name_appeared,
                                                (GBusNameVanishedCallback) msd_name_vanished,
                                                manager, NULL);

    mate_settings_profile_end (NULL);
    return TRUE;
}

void
msd_mpris_manager_stop (MsdMprisManager *manager)
{
    g_debug ("Stopping mpris manager");

    if (manager->priv->media_keys_proxy != NULL) {
        g_object_unref (manager->priv->media_keys_proxy);
        manager->priv->media_keys_proxy = NULL;
    }

    if (manager->priv->watch_id != 0) {
        g_bus_unwatch_name (manager->priv->watch_id);
        manager->priv->watch_id = 0;
    }
}

static void
msd_mpris_manager_set_property (GObject        *object,
                                guint           prop_id,
                                const GValue   *value,
                                GParamSpec     *pspec)
{
    MsdMprisManager *self;

    self = MSD_MPRIS_MANAGER (object);

    switch (prop_id) {
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
msd_mpris_manager_get_property (GObject        *object,
                                guint           prop_id,
                                GValue         *value,
                                GParamSpec     *pspec)
{
    MsdMprisManager *self;

    self = MSD_MPRIS_MANAGER (object);

    switch (prop_id) {
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static GObject *
msd_mpris_manager_constructor (GType                  type,
                               guint                  n_construct_properties,
                               GObjectConstructParam *construct_properties)
{
    MsdMprisManager      *mpris_manager;
    MsdMprisManagerClass *klass;

    klass = MSD_MPRIS_MANAGER_CLASS (g_type_class_peek (MSD_TYPE_MPRIS_MANAGER));

    mpris_manager = MSD_MPRIS_MANAGER (G_OBJECT_CLASS (msd_mpris_manager_parent_class)->constructor (type,
                                                                                                     n_construct_properties,
                                                                                                     construct_properties));

    return G_OBJECT (mpris_manager);
}

static void
msd_mpris_manager_dispose (GObject *object)
{
    MsdMprisManager *mpris_manager;

    mpris_manager = MSD_MPRIS_MANAGER (object);

    G_OBJECT_CLASS (msd_mpris_manager_parent_class)->dispose (object);
}

static void
msd_mpris_manager_class_init (MsdMprisManagerClass *klass)
{
    GObjectClass   *object_class = G_OBJECT_CLASS (klass);

    object_class->get_property = msd_mpris_manager_get_property;
    object_class->set_property = msd_mpris_manager_set_property;
    object_class->constructor = msd_mpris_manager_constructor;
    object_class->dispose = msd_mpris_manager_dispose;
    object_class->finalize = msd_mpris_manager_finalize;

    g_type_class_add_private (klass, sizeof (MsdMprisManagerPrivate));
}

static void
msd_mpris_manager_init (MsdMprisManager *manager)
{
        manager->priv = MSD_MPRIS_MANAGER_GET_PRIVATE (manager);

}

static void
msd_mpris_manager_finalize (GObject *object)
{
    MsdMprisManager *mpris_manager;

    g_return_if_fail (object != NULL);
    g_return_if_fail (MSD_IS_MPRIS_MANAGER (object));

    mpris_manager = MSD_MPRIS_MANAGER (object);

    g_return_if_fail (mpris_manager->priv != NULL);

    G_OBJECT_CLASS (msd_mpris_manager_parent_class)->finalize (object);
}

MsdMprisManager *
msd_mpris_manager_new (void)
{
    if (manager_object != NULL) {
        g_object_ref (manager_object);
    } else {
        manager_object = g_object_new (MSD_TYPE_MPRIS_MANAGER, NULL);
        g_object_add_weak_pointer (manager_object,
                                   (gpointer *) &manager_object);
    }

    return MSD_MPRIS_MANAGER (manager_object);
}
