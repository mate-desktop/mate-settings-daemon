/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright © 2001 Ximian, Inc.
 * Copyright (C) 2007 William Jon McCann <mccann@jhu.edu>
 * Copyright 2007 Red Hat, Inc.
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
#include <gio/gio.h>

#define MATE_DESKTOP_USE_UNSTABLE_API
#include <libmateui/mate-bg.h>
#include <X11/Xatom.h>

#include "mate-settings-profile.h"
#include "msd-background-manager.h"

#if !GTK_CHECK_VERSION(3, 0, 0)
#define cairo_surface_t		GdkPixmap
#define cairo_surface_destroy	g_object_unref
#define mate_bg_create_surface				mate_bg_create_pixmap
#define mate_bg_set_surface_as_root			mate_bg_set_pixmap_as_root
#define mate_bg_set_surface_as_root_with_crossfade	mate_bg_set_pixmap_as_root_with_crossfade
#endif

#define MATE_SESSION_MANAGER_DBUS_NAME "org.mate.SessionManager"
#define MATE_SESSION_MANAGER_DBUS_PATH "/org/mate/SessionManager"

#define MSD_BACKGROUND_MANAGER_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), MSD_TYPE_BACKGROUND_MANAGER, MsdBackgroundManagerPrivate))

struct MsdBackgroundManagerPrivate {
	GSettings      *settings;
	MateBG         *bg;
	guint           timeout_id;

	MateBGCrossfade *fade;

	GDBusProxy     *proxy;
	guint           proxy_signal_id;
};

static void
msd_background_manager_class_init (MsdBackgroundManagerClass* klass);

static void

msd_background_manager_init (MsdBackgroundManager* background_manager);

static void
msd_background_manager_finalize (GObject* object);

static void setup_bg (MsdBackgroundManager *manager);
static void connect_screen_signals (MsdBackgroundManager *manager);

G_DEFINE_TYPE(MsdBackgroundManager, msd_background_manager, G_TYPE_OBJECT)

static gpointer manager_object = NULL;

static gboolean
do_draw_background (MsdBackgroundManager *manager)
{
	return g_settings_get_boolean (manager->priv->settings,
					MATE_BG_KEY_DRAW_BACKGROUND);
}

static gboolean
do_crossfade_background (MsdBackgroundManager *manager)
{
	return g_settings_get_boolean (manager->priv->settings,
					MATE_BG_KEY_BACKGROUND_FADE);
}

static gboolean
caja_is_drawing_background (MsdBackgroundManager *manager)
{
	Atom           window_id_atom;
	Window         caja_xid;
	Atom           actual_type;
	int            actual_format;
	unsigned long  nitems;
	unsigned long  bytes_after;
	unsigned char* data;
	Atom           wmclass_atom;
	gboolean       running;
	gint           error;
	gboolean       show_desktop_icons;

	show_desktop_icons = g_settings_get_boolean (manager->priv->settings,
						     MATE_BG_KEY_SHOW_DESKTOP);
	if (!show_desktop_icons) {
	       return FALSE;
	}

	window_id_atom = XInternAtom(GDK_DISPLAY_XDISPLAY(gdk_display_get_default()),
	                             "CAJA_DESKTOP_WINDOW_ID", True);

	if (window_id_atom == None) {
		return FALSE;
	}

	XGetWindowProperty (GDK_DISPLAY_XDISPLAY(gdk_display_get_default()),
			    GDK_ROOT_WINDOW(),
			    window_id_atom,
			    0,
			    1,
			    False,
			    XA_WINDOW,
			    &actual_type,
			    &actual_format,
			    &nitems,
			    &bytes_after,
			    &data);

	if (data != NULL)
	{
		caja_xid = *(Window*) data;
		XFree(data);
	}
	else
	{
		return FALSE;
	}

	if (actual_type != XA_WINDOW)
	{
		return FALSE;
	}

	if (actual_format != 32)
	{
		return FALSE;
	}

	wmclass_atom = XInternAtom(GDK_DISPLAY_XDISPLAY(gdk_display_get_default()), "WM_CLASS", False);

	gdk_error_trap_push();

	XGetWindowProperty (GDK_DISPLAY_XDISPLAY(gdk_display_get_default()),
			    caja_xid,
			    wmclass_atom,
			    0,
			    20,
			    False,
			    XA_STRING,
			    &actual_type,
			    &actual_format,
			    &nitems,
			    &bytes_after,
			    &data);

	error = gdk_error_trap_pop();

	if (error == BadWindow)
	{
		return FALSE;
	}

	if (actual_type == XA_STRING &&
		nitems == 20 &&
		bytes_after == 0 &&
		actual_format == 8 &&
		data != NULL &&
		!strcmp((char*) data, "desktop_window") &&
		!strcmp((char*) data + strlen((char*) data) + 1, "Caja"))
	{
		running = TRUE;
	}
	else
	{
		running = FALSE;
	}

	if (data != NULL)
	{
		XFree(data);
	}

	return running;
}

static void
free_fade (MsdBackgroundManager *manager)
{
	if (manager->priv->fade != NULL) {
		g_object_unref (manager->priv->fade);
		manager->priv->fade = NULL;
	}
}

static void
draw_background (MsdBackgroundManager *manager,
                 gboolean              use_crossfade)
{
	GdkDisplay *display;
	int         n_screens;
	int         i;

	if (caja_is_drawing_background (manager) || !do_draw_background (manager))
	{
		return;
	}

	mate_settings_profile_start(NULL);

	display = gdk_display_get_default();
	n_screens = gdk_display_get_n_screens(display);

	for (i = 0; i < n_screens; ++i)
	{
		GdkScreen *screen;
		cairo_surface_t *surface;

		screen = gdk_display_get_screen(display, i);

		surface = mate_bg_create_surface (manager->priv->bg,
						  gdk_screen_get_root_window (screen),
						  gdk_screen_get_width (screen),
						  gdk_screen_get_height (screen),
						  TRUE);

		if (use_crossfade && do_crossfade_background (manager))
		{
			if (manager->priv->fade != NULL)
				g_object_unref (manager->priv->fade);

			manager->priv->fade = mate_bg_set_surface_as_root_with_crossfade (screen,
											  surface);
			g_signal_connect_swapped (manager->priv->fade, "finished",
						  G_CALLBACK (free_fade),
						  manager);
		} else {
			mate_bg_set_surface_as_root (screen, surface);
		}

		cairo_surface_destroy (surface);
		surface = NULL;
	}

	mate_settings_profile_end(NULL);
}

static void
on_bg_transitioned (MateBG               *bg,
                    MsdBackgroundManager *manager)
{
	draw_background (manager, FALSE);
}

static gboolean
settings_change_event_idle_cb (MsdBackgroundManager *manager)
{
	mate_bg_load_from_gsettings (manager->priv->bg,
	                             manager->priv->settings);

	return FALSE;   /* remove from the list of event sources */
}

static gboolean
settings_change_event_cb (GSettings            *settings,
                          gpointer              keys,
                          gint                  n_keys,
                          MsdBackgroundManager *manager)
{
	/* Defer signal processing to avoid making the dconf backend deadlock */
	g_idle_add ((GSourceFunc) settings_change_event_idle_cb, manager);

	return FALSE;   /* let the event propagate further */
}

static void
on_screen_size_changed (GdkScreen            *screen,
                        MsdBackgroundManager *manager)
{
	draw_background (manager, FALSE);
}

static void
on_bg_changed (MateBG               *bg,
               MsdBackgroundManager *manager)
{
	draw_background (manager, TRUE);
}

static void
setup_bg (MsdBackgroundManager *manager)
{
	g_return_if_fail (manager->priv->bg == NULL);

	manager->priv->bg = mate_bg_new();

	g_signal_connect(manager->priv->bg,
			 "changed",
			 G_CALLBACK (on_bg_changed),
			 manager);

	g_signal_connect(manager->priv->bg,
			 "transitioned",
			 G_CALLBACK (on_bg_transitioned),
			 manager);

	connect_screen_signals (manager);

	mate_bg_load_from_gsettings (manager->priv->bg,
	                             manager->priv->settings);

    /* Connect to "change-event" signal to receive *groups of changes* before
     * they are split out into multiple emissions of the "changed" signal.
     */
	g_signal_connect (manager->priv->settings,
			  "change-event",
			  G_CALLBACK (settings_change_event_cb),
			  manager);
}

static gboolean
queue_draw_background (MsdBackgroundManager *manager)
{
	manager->priv->timeout_id = 0;

        if (manager->priv->bg == NULL)
	        setup_bg (manager);

	draw_background (manager, FALSE);

	return FALSE;
}

static void
queue_timeout (MsdBackgroundManager *manager)
{
	if (manager->priv->timeout_id > 0)
		return;

	/* If the session finishes then check if caja is
	 * running and if not, set the background.
	 *
	 * We wait a few seconds after the session is up
	 * because caja tells the session manager that its
	 * ready before it sets the background.
	 */
	manager->priv->timeout_id = g_timeout_add_seconds(8,
	                                                  (GSourceFunc) queue_draw_background,
	                                                  manager);
}

static void
disconnect_session_manager_listener (MsdBackgroundManager* manager)
{
	if (manager->priv->proxy && manager->priv->proxy_signal_id) {
		g_signal_handler_disconnect (manager->priv->proxy,
					     manager->priv->proxy_signal_id);
		manager->priv->proxy_signal_id = 0;
	}
}

static void
on_session_manager_signal (GDBusProxy   *proxy,
			   const gchar  *sender_name,
			   const gchar  *signal_name,
			   GVariant     *parameters,
			   gpointer      user_data)
{
	MsdBackgroundManager *manager = MSD_BACKGROUND_MANAGER (user_data);

	if (g_strcmp0 (signal_name, "SessionRunning") == 0) {
		queue_timeout (manager);
		disconnect_session_manager_listener (manager);
	}
}

static void
draw_background_after_session_loads (MsdBackgroundManager *manager)
{
	GError *error = NULL;
	GDBusProxyFlags flags;

	flags = G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES |
		G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START;
	manager->priv->proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
							      flags,
							      NULL, /* GDBusInterfaceInfo */
							      MATE_SESSION_MANAGER_DBUS_NAME,
							      MATE_SESSION_MANAGER_DBUS_PATH,
							      MATE_SESSION_MANAGER_DBUS_NAME,
							      NULL, /* GCancellable */
							      &error);
	if (manager->priv->proxy == NULL) {
		g_warning ("Could not listen to session manager: %s",
			   error->message);
		g_error_free (error);
		return;
	}

	manager->priv->proxy_signal_id = g_signal_connect (manager->priv->proxy,
							   "g-signal",
							   G_CALLBACK (on_session_manager_signal),
							   manager);
}

static void
disconnect_screen_signals (MsdBackgroundManager* manager)
{
	GdkDisplay* display;
	int         i;
	int         n_screens;

	display = gdk_display_get_default();
	n_screens = gdk_display_get_n_screens(display);

	for (i = 0; i < n_screens; ++i)
	{
		GdkScreen *screen;

		screen = gdk_display_get_screen(display, i);

		g_signal_handlers_disconnect_by_func(screen,
		                                     G_CALLBACK(on_screen_size_changed),
		                                     manager);
	}
}

static void
connect_screen_signals (MsdBackgroundManager* manager)
{
	GdkDisplay* display;
	int         i;
	int         n_screens;

	display = gdk_display_get_default();
	n_screens = gdk_display_get_n_screens(display);

	for (i = 0; i < n_screens; ++i)
	{
		GdkScreen* screen;
		screen = gdk_display_get_screen(display, i);
		g_signal_connect(screen,
		                 "monitors-changed",
		                 G_CALLBACK(on_screen_size_changed),
		                 manager);

		g_signal_connect(screen,
		                 "size-changed",
		                 G_CALLBACK(on_screen_size_changed),
		                 manager);
	}
}

static void
background_handling_changed (GSettings            *settings,
			     const char           *key,
			     MsdBackgroundManager *manager)
{
	if (do_draw_background (manager) &&
	    !caja_is_drawing_background (manager))
	{
		queue_timeout (manager);
	}
}

gboolean
msd_background_manager_start (MsdBackgroundManager  *manager,
                              GError               **error)
{
	gboolean show_desktop_icons;

	g_debug("Starting background manager");
	mate_settings_profile_start(NULL);

	manager->priv->settings = g_settings_new (MATE_BG_SCHEMA);
	g_signal_connect (manager->priv->settings, "changed::" MATE_BG_KEY_DRAW_BACKGROUND,
			  G_CALLBACK (background_handling_changed), manager);
	g_signal_connect (manager->priv->settings, "changed::" MATE_BG_KEY_SHOW_DESKTOP,
			  G_CALLBACK (background_handling_changed), manager);

	/* If this is set, caja will draw the background and is
	 * almost definitely in our session.  however, it may not be
	 * running yet (so is_caja_running() will fail).  so, on
	 * startup, just don't do anything if this key is set so we
	 * don't waste time setting the background only to have
	 * caja overwrite it.
	 */
	show_desktop_icons = g_settings_get_boolean (manager->priv->settings,
						     MATE_BG_KEY_SHOW_DESKTOP);

	if (!show_desktop_icons)
	{
		setup_bg(manager);
	}
	else
	{
		draw_background_after_session_loads(manager);
	}

	mate_settings_profile_end(NULL);

	return TRUE;
}

void
msd_background_manager_stop (MsdBackgroundManager *manager)
{
	MsdBackgroundManagerPrivate *p = manager->priv;

	g_debug("Stopping background manager");

	disconnect_screen_signals(manager);

	if (manager->priv->proxy)
	{
		disconnect_session_manager_listener (manager);
		g_object_unref (manager->priv->proxy);
	}

	g_signal_handlers_disconnect_by_func (manager->priv->settings,
					      settings_change_event_cb,
					      manager);

	if (p->settings != NULL)
	{
		g_object_unref (G_OBJECT (p->settings));
		p->settings = NULL;
	}

	if (p->timeout_id != 0)
	{
		g_source_remove (p->timeout_id);
		p->timeout_id = 0;
	}

	if (p->bg != NULL)
	{
		g_object_unref (G_OBJECT (p->bg));
		p->bg = NULL;
	}

	free_fade (manager);
}

static GObject*
msd_background_manager_constructor (GType                  type,
                                    guint                  n_construct_properties,
                                    GObjectConstructParam* construct_properties)
{
	MsdBackgroundManager*      background_manager;

	background_manager = MSD_BACKGROUND_MANAGER(G_OBJECT_CLASS(msd_background_manager_parent_class)->constructor(type,
	                                                                                                             n_construct_properties,
	                                                                                                             construct_properties));
	return G_OBJECT(background_manager);
}

static void
msd_background_manager_class_init (MsdBackgroundManagerClass* klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS(klass);

	object_class->constructor = msd_background_manager_constructor;
	object_class->finalize = msd_background_manager_finalize;

	g_type_class_add_private(klass, sizeof(MsdBackgroundManagerPrivate));
}

static void
msd_background_manager_init (MsdBackgroundManager* manager)
{
	manager->priv = MSD_BACKGROUND_MANAGER_GET_PRIVATE(manager);
}

static void
msd_background_manager_finalize (GObject* object)
{
	g_return_if_fail (object != NULL);
	g_return_if_fail (MSD_IS_BACKGROUND_MANAGER (object));

	MsdBackgroundManager *manager = MSD_BACKGROUND_MANAGER (object);

	g_return_if_fail (manager->priv != NULL);

	G_OBJECT_CLASS(msd_background_manager_parent_class)->finalize(object);
}

MsdBackgroundManager*
msd_background_manager_new (void)
{
	if (manager_object != NULL)
	{
		g_object_ref(manager_object);
	}
	else
	{
		manager_object = g_object_new(MSD_TYPE_BACKGROUND_MANAGER, NULL);

		g_object_add_weak_pointer(manager_object, (gpointer*) &manager_object);
	}

	return MSD_BACKGROUND_MANAGER(manager_object);
}
