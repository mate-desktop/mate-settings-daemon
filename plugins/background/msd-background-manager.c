/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2001 Ximian, Inc.
 * Copyright (C) 2007 William Jon McCann <mccann@jhu.edu>
 * Copyright (C) 2007 Red Hat, Inc.
 * Copyright (C) 2012 Jasmine Hassan <jasmine.aura@gmail.com>
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
#include <libmate-desktop/mate-bg.h>
#include <X11/Xatom.h>

#include "mate-settings-profile.h"
#include "msd-background-manager.h"

struct _MsdBackgroundManager {
	GObject          parent;

	GSettings       *settings;
	MateBG          *bg;
	cairo_surface_t *surface;

	gboolean         draw_in_progress;
};

G_DEFINE_TYPE (MsdBackgroundManager, msd_background_manager, G_TYPE_OBJECT)

static gpointer manager_object = NULL;

/* Check if any window with _NET_WM_WINDOW_TYPE_DESKTOP exists */
static gboolean
desktop_window_exists (void)
{
	Display       *display = gdk_x11_get_default_xdisplay ();
	Window         window = gdk_x11_get_default_root_xwindow ();
	Atom           client_list, desktop_type, window_type, type;
	int            format;
	unsigned long  nitems, after;
	unsigned char *data;
	GdkDisplay    *gdk_display;
	gboolean       running = FALSE;

	/* This atom will be present if a window owns the desktop (e.g. caja) */
	desktop_type = XInternAtom (display, "_NET_WM_WINDOW_TYPE_DESKTOP", True);
	if (desktop_type == None)
		return FALSE;

	/* This atom will contain the list of all windows managed by the window manager */
	client_list = XInternAtom (display, "_NET_CLIENT_LIST", True);
	if (client_list == None)
		return FALSE;

	XGetWindowProperty (display, window, client_list, 0, 1024, False,
			    XA_WINDOW, &type, &format, &nitems, &after, &data);

	if (data == NULL)
		return FALSE;

	if (type != XA_WINDOW || format != 32) {
		XFree (data);
		return FALSE;
	}

	window_type = XInternAtom (display, "_NET_WM_WINDOW_TYPE", True);
	if (window_type == None) {
		XFree (data);
		return FALSE;
	}

	gdk_display = gdk_display_get_default ();
	Window *windows = (Window *) data;

	/* Read all the windows and determine if any of them own the desktop */
	for (unsigned long i = 0; i < nitems && !running; i++) {
		Atom           wm_type;
		int            wm_format;
		unsigned long  wm_nitems, wm_after;
		unsigned char *wm_data;

		gdk_x11_display_error_trap_push (gdk_display);

		XGetWindowProperty (display, windows[i], window_type, 0, 32, False,
				    XA_ATOM, &wm_type, &wm_format, &wm_nitems, &wm_after, &wm_data);

		if (wm_data != NULL) {
			Atom *types = (Atom *) wm_data;
			for (unsigned long j = 0; j < wm_nitems; j++) {
				/* Found a window managing the desktop. Bail out. */
				if (types[j] == desktop_type) {
					running = TRUE;
					break;
				}
			}
			XFree (wm_data);
		}

		gdk_x11_display_error_trap_pop_ignored (gdk_display);
	}
	XFree (data);

	return running;
}

static void
free_bg_surface (MsdBackgroundManager *manager)
{
	if (manager->surface != NULL) {
		cairo_surface_destroy (manager->surface);
		manager->surface = NULL;
	}
}

static void
draw_background (MsdBackgroundManager *manager)
{
	if (manager->draw_in_progress || desktop_window_exists ())
		return;

	GdkDisplay *display = gdk_display_get_default ();
	GdkScreen *screen = gdk_display_get_default_screen (display);
	GdkWindow *window = gdk_screen_get_root_window (screen);
	gint scale   = gdk_window_get_scale_factor (window);
	gint width   = WidthOfScreen (gdk_x11_screen_get_xscreen (screen)) / scale;
	gint height  = HeightOfScreen (gdk_x11_screen_get_xscreen (screen)) / scale;

	manager->draw_in_progress = TRUE;

	free_bg_surface (manager);
	manager->surface = mate_bg_create_surface_scale (manager->bg, window, width, height, scale, TRUE);
	mate_bg_set_surface_as_root (screen, manager->surface);

	manager->draw_in_progress = FALSE;
}

static gboolean
queue_draw_background (MsdBackgroundManager *manager)
{
	draw_background (manager);
	return FALSE;
}

static void
on_bg_changed (MateBG               *bg G_GNUC_UNUSED,
	       MsdBackgroundManager *manager)
{
	g_idle_add ((GSourceFunc) queue_draw_background, manager);
}

static void
on_bg_transitioned (MateBG               *bg G_GNUC_UNUSED,
		    MsdBackgroundManager *manager)
{
	g_idle_add ((GSourceFunc) queue_draw_background, manager);
}

static void
on_screen_size_changed (GdkScreen            *screen G_GNUC_UNUSED,
			MsdBackgroundManager *manager)
{
	g_idle_add ((GSourceFunc) queue_draw_background, manager);
}

static void
disconnect_screen_signals (MsdBackgroundManager *manager)
{
	GdkDisplay *display   = gdk_display_get_default();

	g_signal_handlers_disconnect_by_func
		(gdk_display_get_default_screen (display),
		 G_CALLBACK (on_screen_size_changed), manager);
}

static void
connect_screen_signals (MsdBackgroundManager *manager)
{
	GdkDisplay *display   = gdk_display_get_default();

	GdkScreen *screen = gdk_display_get_default_screen (display);

	g_signal_connect (screen, "monitors-changed",
			  G_CALLBACK (on_screen_size_changed), manager);
	g_signal_connect (screen, "size-changed",
			  G_CALLBACK (on_screen_size_changed), manager);
}

static gboolean
settings_change_event_idle_cb (MsdBackgroundManager *manager)
{
	mate_bg_load_from_preferences (manager->bg);
	return FALSE;
}

static gboolean
settings_change_event_cb (GSettings            *settings G_GNUC_UNUSED,
			  gpointer              keys G_GNUC_UNUSED,
			  gint                  n_keys G_GNUC_UNUSED,
			  MsdBackgroundManager *manager)
{
	g_idle_add ((GSourceFunc) settings_change_event_idle_cb, manager);
	return FALSE;
}

gboolean
msd_background_manager_start (MsdBackgroundManager  *manager,
			      GError               **error)
{
	g_debug ("Starting background manager");
	mate_settings_profile_start (NULL);

	manager->settings = g_settings_new (MATE_BG_SCHEMA);

	manager->bg = mate_bg_new();

	g_signal_connect(manager->bg, "changed", G_CALLBACK (on_bg_changed), manager);

	g_signal_connect(manager->bg, "transitioned", G_CALLBACK (on_bg_transitioned), manager);

	mate_bg_load_from_gsettings (manager->bg, manager->settings);

	connect_screen_signals (manager);

	g_signal_connect (manager->settings, "change-event",
			  G_CALLBACK (settings_change_event_cb), manager);

	mate_settings_profile_end (NULL);
	return TRUE;
}

void
msd_background_manager_stop (MsdBackgroundManager *manager)
{
	g_debug ("Stopping background manager");

	disconnect_screen_signals (manager);

	if (manager->settings != NULL) {
		g_signal_handlers_disconnect_by_func (manager->settings, settings_change_event_cb, manager);
		g_object_unref (manager->settings);
		manager->settings = NULL;
	}

	if (manager->bg != NULL) {
		g_object_unref (manager->bg);
		manager->bg = NULL;
	}

	free_bg_surface (manager);
}

static GObject*
msd_background_manager_constructor (GType                  type,
				    guint                  n_construct_properties,
				    GObjectConstructParam* construct_properties)
{
	MsdBackgroundManager *manager =
	   MSD_BACKGROUND_MANAGER (
	      G_OBJECT_CLASS (msd_background_manager_parent_class)->constructor (
				type, n_construct_properties, construct_properties));

	return G_OBJECT(manager);
}

static void
msd_background_manager_finalize (GObject *object)
{
	g_return_if_fail (object != NULL);
	g_return_if_fail (MSD_IS_BACKGROUND_MANAGER (object));

	G_OBJECT_CLASS(msd_background_manager_parent_class)->finalize(object);
}

static void
msd_background_manager_init (MsdBackgroundManager* manager)
{
}

static void
msd_background_manager_class_init (MsdBackgroundManagerClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);

	object_class->constructor = msd_background_manager_constructor;
	object_class->finalize = msd_background_manager_finalize;
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
