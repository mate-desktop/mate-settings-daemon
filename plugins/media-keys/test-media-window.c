/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007 William Jon McCann <mccann@jhu.edu>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 *
 */

#include "config.h"

#include <stdlib.h>

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "msd-media-keys-window.h"

static gboolean
update_state (GtkWidget *window)
{
        static int count = 0;

        count++;

        switch (count) {
        case 1:
                msd_media_keys_window_set_volume_level (MSD_MEDIA_KEYS_WINDOW (window),
                                                        50);
                msd_media_keys_window_set_action (MSD_MEDIA_KEYS_WINDOW (window),
                                                  MSD_MEDIA_KEYS_WINDOW_ACTION_VOLUME);

                gtk_widget_show (window);
                break;
        case 2:
                msd_media_keys_window_set_volume_level (MSD_MEDIA_KEYS_WINDOW (window),
                                                        100);
                msd_media_keys_window_set_action (MSD_MEDIA_KEYS_WINDOW (window),
                                                  MSD_MEDIA_KEYS_WINDOW_ACTION_VOLUME);

                gtk_widget_show (window);
                break;
        case 3:
                msd_media_keys_window_set_volume_muted (MSD_MEDIA_KEYS_WINDOW (window),
                                                        TRUE);
                msd_media_keys_window_set_action (MSD_MEDIA_KEYS_WINDOW (window),
                                                  MSD_MEDIA_KEYS_WINDOW_ACTION_VOLUME);

                gtk_widget_show (window);
                break;
        case 4:
                msd_media_keys_window_set_action_custom (MSD_MEDIA_KEYS_WINDOW (window),
                                                         "media-eject",
                                                         FALSE);

                gtk_widget_show (window);
                break;
        case 5:
                msd_media_keys_window_set_volume_level (MSD_MEDIA_KEYS_WINDOW (window),
                                                        0);
                msd_media_keys_window_set_action_custom (MSD_MEDIA_KEYS_WINDOW (window),
                                                         "gpm-brightness-lcd",
                                                         TRUE);

                gtk_widget_show (window);
                break;
        case 6:
                msd_media_keys_window_set_volume_level (MSD_MEDIA_KEYS_WINDOW (window),
                                                        50);
                msd_media_keys_window_set_action_custom (MSD_MEDIA_KEYS_WINDOW (window),
                                                         "gpm-brightness-lcd",
                                                         TRUE);

                gtk_widget_show (window);
                break;
        case 7:
                msd_media_keys_window_set_volume_level (MSD_MEDIA_KEYS_WINDOW (window),
                                                        100);
                msd_media_keys_window_set_action_custom (MSD_MEDIA_KEYS_WINDOW (window),
                                                         "gpm-brightness-lcd",
                                                         TRUE);

                gtk_widget_show (window);
                break;
        default:
                gtk_main_quit ();
                break;
        }

        return TRUE;
}

static void
test_window (void)
{
        GtkWidget *window;

        window = msd_media_keys_window_new ();
        gtk_window_set_position (GTK_WINDOW (window), GTK_WIN_POS_CENTER_ALWAYS);

        msd_media_keys_window_set_volume_level (MSD_MEDIA_KEYS_WINDOW (window),
                                                0);
        msd_media_keys_window_set_action (MSD_MEDIA_KEYS_WINDOW (window),
                                          MSD_MEDIA_KEYS_WINDOW_ACTION_VOLUME);

        gtk_widget_show (window);

        g_timeout_add (3000, (GSourceFunc) update_state, window);
}

int
main (int    argc,
      char **argv)
{
        GError *error = NULL;

#ifdef ENABLE_NLS
        bindtextdomain (GETTEXT_PACKAGE, MATE_SETTINGS_LOCALEDIR);
# ifdef HAVE_BIND_TEXTDOMAIN_CODESET
        bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
# endif
        textdomain (GETTEXT_PACKAGE);
#endif

        if (! gtk_init_with_args (&argc, &argv, NULL, NULL, NULL, &error)) {
                fprintf (stderr, "%s", error->message);
                g_error_free (error);
                exit (1);
        }

        gtk_icon_theme_append_search_path (gtk_icon_theme_get_default (),
                                           DATADIR G_DIR_SEPARATOR_S "mate-power-manager" G_DIR_SEPARATOR_S "icons");

        test_window ();

        gtk_main ();

        return 0;
}
