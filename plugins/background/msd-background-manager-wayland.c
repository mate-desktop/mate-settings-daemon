/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2026 MATE Developers
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
#include <gdk/gdkwayland.h>
#include <gtk/gtk.h>
#include <wayland-client.h>

#include <gtk-layer-shell.h>

#define MATE_DESKTOP_USE_UNSTABLE_API
#include <libmate-desktop/mate-bg.h>

#include "msd-background-manager-wayland.h"

struct _MsdBackgroundManagerWayland
{
	GdkDisplay   *display;
	GSettings    *settings;
	MateBG       *bg;
	GPtrArray    *monitor_windows;
	gboolean      caja_desktop_active;
};

typedef struct
{
	GtkWidget                 *window;
	GdkMonitor                *monitor;
	MsdBackgroundManagerWayland *manager;
	cairo_surface_t           *surface;
	cairo_surface_t           *fading_surface;
	cairo_surface_t           *end_surface;
	guint                      fade_timeout_id;
	gdouble                    fade_start_time;
	gdouble                    fade_total_duration;
	gboolean                   fade_is_first_frame;
	int                        width;
	int                        height;
} MonitorWindow;

static void
free_monitor_window_surface (MonitorWindow *mw)
{
	if (mw->surface != NULL) {
		cairo_surface_destroy (mw->surface);
		mw->surface = NULL;
	}
}

static gdouble
get_current_time (void)
{
	return g_get_monotonic_time () / 1000000.0;
}

/* Check the GSettings key that indicates whether Caja is configured to
 * draw desktop icons. When this is enabled Caja draws the desktop itself
 * so we must not create our own layer-shell background windows.
 */
static gboolean
caja_desktop_is_showing (MsdBackgroundManagerWayland *manager)
{
	return g_settings_get_boolean (manager->settings, MATE_BG_KEY_SHOW_DESKTOP);
}

static gboolean
animations_are_disabled (MonitorWindow *mw G_GNUC_UNUSED)
{
	GtkSettings *settings;
	gboolean    enabled;

	settings = gtk_settings_get_default ();
	if (settings == NULL)
		return TRUE;

	g_object_get (settings, "gtk-enable-animations", &enabled, NULL);

	return !enabled;
}

static void
cancel_fade (MonitorWindow *mw)
{
	if (mw->fade_timeout_id == 0)
		return;

	g_source_remove (mw->fade_timeout_id);
	mw->fade_timeout_id = 0;
}

static void
on_fade_finished (MonitorWindow *mw)
{
	cairo_t *cr;

	mw->fade_timeout_id = 0;

	if (mw->fading_surface == NULL || mw->end_surface == NULL) {
		if (mw->end_surface != NULL) {
			cairo_surface_destroy (mw->end_surface);
			mw->end_surface = NULL;
		}
		return;
	}

	cr = cairo_create (mw->fading_surface);
	cairo_set_source_surface (cr, mw->end_surface, 0, 0);
	cairo_paint (cr);
	cairo_destroy (cr);

	cairo_surface_destroy (mw->end_surface);
	mw->end_surface = NULL;

	free_monitor_window_surface (mw);
	mw->surface = mw->fading_surface;
	mw->fading_surface = NULL;

	gtk_widget_queue_draw (mw->window);
}

static gboolean
on_fade_tick (MonitorWindow *mw)
{
	gdouble         now, percent_done;
	cairo_t        *cr;
	cairo_status_t  status;

	if (mw->fading_surface == NULL || mw->end_surface == NULL)
		return FALSE;

	now = get_current_time ();
	percent_done = (now - mw->fade_start_time) / mw->fade_total_duration;
	percent_done = CLAMP (percent_done, 0.0, 1.0);

	/* If it's taking a long time to get to the first frame,
	 * then lengthen the duration, so the user will get to see
	 * the effect.
	 */
	if (mw->fade_is_first_frame && percent_done > .33) {
		mw->fade_is_first_frame = FALSE;
		mw->fade_total_duration *= 1.5;
		return TRUE;
	}

	if (animations_are_disabled (mw))
		return FALSE;

	/* Accumulate the end surface in place, so the fade is
	 * exponential rather than linear (looks good, matches X11).
	 */
	cr = cairo_create (mw->fading_surface);
	cairo_set_source_surface (cr, mw->end_surface, 0, 0);
	cairo_paint_with_alpha (cr, percent_done);
	status = cairo_status (cr);
	cairo_destroy (cr);

	if (status == CAIRO_STATUS_SUCCESS)
		gtk_widget_queue_draw (mw->window);

	return percent_done <= .99;
}

static void
set_surface_instantly (MonitorWindow *mw, cairo_surface_t *new_surface)
{
	if (mw->fade_timeout_id != 0) {
		if (mw->fading_surface != NULL) {
			cairo_surface_destroy (mw->fading_surface);
			mw->fading_surface = NULL;
		}
		if (mw->end_surface != NULL) {
			cairo_surface_destroy (mw->end_surface);
			mw->end_surface = NULL;
		}
		g_source_remove (mw->fade_timeout_id);
		mw->fade_timeout_id = 0;
	}
	free_monitor_window_surface (mw);
	mw->surface = new_surface;
	gtk_widget_queue_draw (mw->window);
}

static void
start_or_continue_fade (MsdBackgroundManagerWayland *manager,
			MonitorWindow               *mw,
			cairo_surface_t             *new_surface)
{
	gboolean fade_enabled;
	gboolean animations_enabled;
	cairo_t *cr;
	GSource *source;

	fade_enabled = g_settings_get_boolean (manager->settings,
					       MATE_BG_KEY_BACKGROUND_FADE);
	animations_enabled = !animations_are_disabled (mw);

	if (!fade_enabled || !animations_enabled || mw->surface == NULL) {
		/* No fade: swap instantly. This also covers the first render
		 * after startup.
		 */
		set_surface_instantly (mw, new_surface);
		return;
	}

	/* If a fade is already running, restart from the current blended
	 * state (its completion commits the blend into mw->surface).
	 */
	cancel_fade (mw);

	mw->fading_surface = cairo_surface_create_similar (mw->surface,
							   cairo_surface_get_content (mw->surface),
							   mw->width, mw->height);
	cr = cairo_create (mw->fading_surface);
	cairo_set_source_surface (cr, mw->surface, 0, 0);
	cairo_paint (cr);
	cairo_destroy (cr);

	mw->end_surface = new_surface;
	mw->fade_start_time = get_current_time ();
	mw->fade_total_duration = 0.75;
	mw->fade_is_first_frame = TRUE;

	source = g_timeout_source_new (1000 / 60.0);
	g_source_set_callback (source, (GSourceFunc) on_fade_tick, mw,
			       (GDestroyNotify) on_fade_finished);
	mw->fade_timeout_id = g_source_attach (source, NULL);
	g_source_unref (source);

	gtk_widget_queue_draw (mw->window);
}

static void
render_surface_at_size (MsdBackgroundManagerWayland *manager,
			MonitorWindow               *mw,
			int                          width,
			int                          height,
			gboolean                     animate)
{
	GdkPixbuf       *pixbuf;
	cairo_surface_t *new_surface;
	cairo_t         *cr;

	if (width <= 0 || height <= 0)
		return;

	pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8,
				 width, height);
	if (pixbuf == NULL) {
		g_warning ("msd-background-wayland: unable to create %dx%d pixbuf",
			   width, height);
		return;
	}

	mate_bg_draw (manager->bg, pixbuf, NULL, FALSE);

	new_surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
						  width, height);
	cr = cairo_create (new_surface);
	gdk_cairo_set_source_pixbuf (cr, pixbuf, 0, 0);
	cairo_paint (cr);
	cairo_destroy (cr);

	g_object_unref (pixbuf);

	mw->width = width;
	mw->height = height;

	if (animate)
		start_or_continue_fade (manager, mw, new_surface);
	else
		set_surface_instantly (mw, new_surface);
}

static void
render_surface_for_monitor (MsdBackgroundManagerWayland *manager,
			    MonitorWindow               *mw,
			    gboolean                     animate)
{
	GdkRectangle rect;

	gdk_monitor_get_geometry (mw->monitor, &rect);
	render_surface_at_size (manager, mw, rect.width, rect.height, animate);
}

static void
on_window_size_allocate (GtkWidget       *widget G_GNUC_UNUSED,
			 GtkAllocation   *alloc,
			 MonitorWindow   *mw)
{
	if (alloc->width <= 0 || alloc->height <= 0)
		return;

	if (mw->surface == NULL)
		return;

	/* The layer surface is anchored to the output edges, so when the
	 * output resolution or scale changes the compositor resizes the
	 * window. Re-render at the new size so the wallpaper adapts even
	 * if no monitors-changed event arrives.
	 */
	if (alloc->width == mw->width && alloc->height == mw->height)
		return;

	g_debug ("msd-background-wayland: window resized to %dx%d, re-rendering",
		 alloc->width, alloc->height);
	render_surface_at_size (mw->manager, mw,
				alloc->width, alloc->height, FALSE);
}

static gboolean
on_window_draw (GtkWidget     *widget,
		cairo_t       *cr,
		MonitorWindow *mw)
{
	GtkAllocation   alloc;
	cairo_surface_t *src;
	gdouble         sx, sy;

	if (mw->surface == NULL && mw->fading_surface == NULL)
		return FALSE;

	src = (mw->fading_surface != NULL) ? mw->fading_surface : mw->surface;

	gtk_widget_get_allocation (widget, &alloc);
	if (alloc.width <= 0 || alloc.height <= 0)
		return FALSE;

	sx = alloc.width / (gdouble) cairo_image_surface_get_width (src);
	sy = alloc.height / (gdouble) cairo_image_surface_get_height (src);
	cairo_scale (cr, sx, sy);
	cairo_set_source_surface (cr, src, 0, 0);
	cairo_paint (cr);

	return FALSE;
}

static void
redraw_all (MsdBackgroundManagerWayland *manager)
{
	guint i;

	for (i = 0; i < manager->monitor_windows->len; i++) {
		MonitorWindow *mw = g_ptr_array_index (manager->monitor_windows, i);
		render_surface_for_monitor (manager, mw, TRUE);
		gtk_widget_queue_draw (mw->window);
	}
}

static void
on_window_destroy (GtkWidget     *widget G_GNUC_UNUSED,
		   MonitorWindow *mw)
{
	if (mw->fade_timeout_id != 0) {
		/* Make on_fade_finished a no-op, then drop the source */
		if (mw->fading_surface != NULL) {
			cairo_surface_destroy (mw->fading_surface);
			mw->fading_surface = NULL;
		}
		if (mw->end_surface != NULL) {
			cairo_surface_destroy (mw->end_surface);
			mw->end_surface = NULL;
		}
		g_source_remove (mw->fade_timeout_id);
		mw->fade_timeout_id = 0;
	}
	free_monitor_window_surface (mw);
	g_free (mw);
}

static void
set_input_region_empty (GtkWidget *window)
{
	struct wl_surface *wl_surface;
	GdkWindow         *gdk_window;

	gdk_window = gtk_widget_get_window (window);
	if (gdk_window == NULL)
		return;

	wl_surface = gdk_wayland_window_get_wl_surface (gdk_window);
	if (wl_surface != NULL)
		wl_surface_set_input_region (wl_surface, NULL);
}

static MonitorWindow *
create_monitor_window (MsdBackgroundManagerWayland *manager G_GNUC_UNUSED,
		       GdkMonitor                  *monitor)
{
	MonitorWindow *mw;
	GtkWidget     *window;
	GdkRectangle   rect;
	guint          i;

	gdk_monitor_get_geometry (monitor, &rect);

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_widget_set_app_paintable (window, TRUE);
	gtk_widget_set_size_request (window, rect.width, rect.height);
	gtk_window_set_default_size (GTK_WINDOW (window), rect.width, rect.height);
	gtk_window_set_resizable (GTK_WINDOW (window), FALSE);

	g_debug ("msd-background-wayland: monitor %dx%d at %d,%d", rect.width, rect.height,
		 rect.x, rect.y);

	gtk_layer_init_for_window (GTK_WINDOW (window));
	gtk_layer_set_layer (GTK_WINDOW (window), GTK_LAYER_SHELL_LAYER_BACKGROUND);
	gtk_layer_set_namespace (GTK_WINDOW (window), "mate-background");
	gtk_layer_set_monitor (GTK_WINDOW (window), monitor);
	for (i = 0; i < GTK_LAYER_SHELL_EDGE_ENTRY_NUMBER; i++)
		gtk_layer_set_anchor (GTK_WINDOW (window), i, TRUE);
	gtk_layer_set_keyboard_mode (GTK_WINDOW (window), GTK_LAYER_SHELL_KEYBOARD_MODE_NONE);

	mw = g_new0 (MonitorWindow, 1);
	mw->window = window;
	mw->monitor = monitor;
	mw->manager = manager;
	mw->width = rect.width;
	mw->height = rect.height;

	g_signal_connect (window, "draw", G_CALLBACK (on_window_draw), mw);
	g_signal_connect (window, "size-allocate", G_CALLBACK (on_window_size_allocate), mw);
	g_signal_connect (window, "destroy", G_CALLBACK (on_window_destroy), mw);

	gtk_widget_show (window);

	set_input_region_empty (window);

	return mw;
}

static void
destroy_all_monitor_windows (MsdBackgroundManagerWayland *manager)
{
	guint i;

	for (i = 0; i < manager->monitor_windows->len; i++) {
		MonitorWindow *mw = g_ptr_array_index (manager->monitor_windows, i);
		gtk_widget_destroy (mw->window);
	}
	g_ptr_array_set_size (manager->monitor_windows, 0);
}

static void
rebuild_monitor_windows (MsdBackgroundManagerWayland *manager)
{
	GdkDisplay *display = manager->display;
	int         n_monitors;
	int         i;

	destroy_all_monitor_windows (manager);

	/* Caja's desktop window (layer "desktop", BACKGROUND) owns the
	 * wallpaper and icons. When the "show-desktop-icons" setting is
	 * enabled we must not map our own layer windows, or we would
	 * cover it.
	 */
	if (caja_desktop_is_showing (manager)) {
		manager->caja_desktop_active = TRUE;
		g_debug ("msd-background-wayland: caja desktop detected, background disabled");
		return;
	}
	manager->caja_desktop_active = FALSE;

	n_monitors = gdk_display_get_n_monitors (display);
	for (i = 0; i < n_monitors; i++) {
		GdkMonitor    *monitor;
		MonitorWindow *mw;

		monitor = gdk_display_get_monitor (display, i);
		mw = create_monitor_window (manager, monitor);
		g_ptr_array_add (manager->monitor_windows, mw);
	}
}

static void
on_show_desktop_icons_changed (GSettings                  *settings G_GNUC_UNUSED,
			       const gchar                *key G_GNUC_UNUSED,
			       MsdBackgroundManagerWayland *manager)
{
	gboolean active;

	active = caja_desktop_is_showing (manager);
	if (active == manager->caja_desktop_active)
		return;

	manager->caja_desktop_active = active;

	if (active) {
		g_debug ("msd-background-wayland: caja desktop icons enabled, hiding background");
		destroy_all_monitor_windows (manager);
	} else {
		g_debug ("msd-background-wayland: caja desktop icons disabled, showing background");
		rebuild_monitor_windows (manager);
		redraw_all (manager);
	}
}

static void
on_monitors_changed (GdkScreen                     *screen G_GNUC_UNUSED,
		     MsdBackgroundManagerWayland *manager)
{
	guint n_monitors = gdk_display_get_n_monitors (manager->display);
	guint i;

	/* A monitor set change (added/removed) requires recreating the
	 * layer windows. A pure geometry/scale change (e.g. the compositor
	 * output is being resized) keeps the same window count, so just
	 * re-render in place at the new size - destroying and recreating
	 * windows mid-resize races with the compositor and leaves the
	 * wallpaper missing.
	 */
	if (n_monitors != manager->monitor_windows->len) {
		g_debug ("msd-background-wayland: monitors changed, rebuilding (%u monitors)",
			 n_monitors);
		rebuild_monitor_windows (manager);
		redraw_all (manager);
		return;
	}

	g_debug ("msd-background-wayland: monitors changed, re-rendering in place (%u monitors)",
		 n_monitors);

	for (i = 0; i < manager->monitor_windows->len; i++) {
		MonitorWindow *mw = g_ptr_array_index (manager->monitor_windows, i);
		render_surface_for_monitor (manager, mw, FALSE);
		gtk_widget_queue_draw (mw->window);
	}
}

static void
on_bg_changed (MateBG                       *bg G_GNUC_UNUSED,
	       MsdBackgroundManagerWayland *manager)
{
	if (manager->caja_desktop_active)
		return;

	g_debug ("msd-background-wayland: background changed, redrawing");
	redraw_all (manager);
}

static gboolean
settings_change_event_idle_cb (MsdBackgroundManagerWayland *manager)
{
	g_debug ("msd-background-wayland: applying settings change");
	mate_bg_load_from_preferences (manager->bg);
	return FALSE;
}

static gboolean
settings_change_event_cb (GSettings                  *settings G_GNUC_UNUSED,
			  gpointer                    keys G_GNUC_UNUSED,
			  gint                        n_keys G_GNUC_UNUSED,
			  MsdBackgroundManagerWayland *manager)
{
	g_idle_add ((GSourceFunc) settings_change_event_idle_cb, manager);
	return FALSE;
}

MsdBackgroundManagerWayland *
msd_background_manager_wayland_new (GError **error)
{
	MsdBackgroundManagerWayland *manager;
	GdkDisplay                  *display;

	display = gdk_display_get_default ();
	if (display == NULL || !GDK_IS_WAYLAND_DISPLAY (display)) {
		g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
			     "A Wayland display is required for the Wayland background backend");
		return NULL;
	}

	manager = g_new0 (MsdBackgroundManagerWayland, 1);
	manager->display = display;
	manager->monitor_windows = g_ptr_array_new ();

	manager->settings = g_settings_new (MATE_BG_SCHEMA);
	manager->bg = mate_bg_new ();

	g_signal_connect (manager->bg, "changed",
			  G_CALLBACK (on_bg_changed), manager);
	g_signal_connect (manager->bg, "transitioned",
			  G_CALLBACK (on_bg_changed), manager);
	g_signal_connect (manager->settings, "change-event",
			  G_CALLBACK (settings_change_event_cb), manager);
	g_signal_connect (gdk_screen_get_default (), "monitors-changed",
			  G_CALLBACK (on_monitors_changed), manager);

	g_debug ("msd-background-wayland: initializing on %d monitors",
		 gdk_display_get_n_monitors (display));

	mate_bg_load_from_gsettings (manager->bg, manager->settings);

	rebuild_monitor_windows (manager);
	redraw_all (manager);

	g_signal_connect (manager->settings, "changed::show-desktop-icons",
			  G_CALLBACK (on_show_desktop_icons_changed), manager);

	return manager;
}

void
msd_background_manager_wayland_destroy (MsdBackgroundManagerWayland *manager)
{
	if (manager == NULL)
		return;

	g_signal_handlers_disconnect_by_func (gdk_screen_get_default (),
					      G_CALLBACK (on_monitors_changed), manager);

	destroy_all_monitor_windows (manager);
	g_ptr_array_free (manager->monitor_windows, TRUE);

	if (manager->settings != NULL) {
		g_signal_handlers_disconnect_by_func (manager->settings,
						      G_CALLBACK (settings_change_event_cb), manager);
		g_signal_handlers_disconnect_by_func (manager->settings,
						      G_CALLBACK (on_show_desktop_icons_changed), manager);
		g_object_unref (manager->settings);
	}

	if (manager->bg != NULL) {
		g_signal_handlers_disconnect_by_func (manager->bg,
						      G_CALLBACK (on_bg_changed), manager);
		g_object_unref (manager->bg);
	}

	g_free (manager);
}
