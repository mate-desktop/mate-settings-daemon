/* msd-locate-pointer.c
 *
 * Copyright (C) 2008 Carlos Garnacho  <carlos@imendio.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include <gtk/gtk.h>
#include "msd-timeline.h"
#include "msd-locate-pointer.h"

#include <gdk/gdkkeysyms.h>
#include <gdk/gdkx.h>
#include <X11/keysym.h>

#define ANIMATION_LENGTH 750
#define WINDOW_SIZE 101
#define N_CIRCLES 4

/* All circles are supposed to be moving when progress
 * reaches 0.5, and each of them are supposed to long
 * for half of the progress, hence the need of 0.5 to
 * get the circles interval, and the multiplication
 * by 2 to know a circle progress */
#define CIRCLES_PROGRESS_INTERVAL (0.5 / N_CIRCLES)
#define CIRCLE_PROGRESS(p) (MIN (1., ((gdouble) (p) * 2.)))

typedef struct MsdLocatePointerData MsdLocatePointerData;

struct MsdLocatePointerData
{
  MsdTimeline *timeline;
  GtkWindow *widget;
  GdkWindow *window;

  gdouble progress;
};

static MsdLocatePointerData *data = NULL;

static void
locate_pointer_paint (MsdLocatePointerData *data,
		      cairo_t              *cr,
		      gboolean              composited)
{
#if GTK_CHECK_VERSION (3, 0, 0)
  GdkRGBA color;
  gdouble progress, circle_progress;
  gint width, height, i;
  GtkStyleContext *style;

  color.red = color.green = color.blue = 0.7;
  color.alpha = 0.;
#else
  GdkColor color;
  gdouble progress, circle_progress;
  gint width, height, i;
  GtkStyle *style;

  color.red = color.green = color.blue = 0xAAAA;
#endif

  progress = data->progress;

  width = gdk_window_get_width (data->window);
  height = gdk_window_get_height (data->window);

#if GTK_CHECK_VERSION (3, 0, 0)
  style = gtk_widget_get_style_context (GTK_WIDGET (data->widget));
  gtk_style_context_save (style);
  gtk_style_context_set_state (style, GTK_STATE_FLAG_SELECTED);
  gtk_style_context_add_class (style, GTK_STYLE_CLASS_VIEW);
  gtk_style_context_get_background_color (style,
                                          gtk_style_context_get_state (style),
                                          &color);
  if (color.alpha == 0.)
    {
      gtk_style_context_remove_class (style, GTK_STYLE_CLASS_VIEW);
      gtk_style_context_get_background_color (style,
                                              gtk_style_context_get_state (style),
                                              &color);
    }
  gtk_style_context_restore (style);
#else
  style = gtk_widget_get_style (GTK_WIDGET (data->widget));
  color = style->bg[GTK_STATE_SELECTED];
#endif

  cairo_save (cr);
  cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
  cairo_set_source_rgba (cr, 1., 1., 1., 0.);
  cairo_paint (cr);

  for (i = 0; i <= N_CIRCLES; i++)
    {
      if (progress < 0.)
	break;

      circle_progress = MIN (1., (progress * 2));
      progress -= CIRCLES_PROGRESS_INTERVAL;

      if (circle_progress >= 1.)
	continue;

      if (composited)
	{
	  cairo_set_source_rgba (cr,
#if GTK_CHECK_VERSION (3, 0, 0)
				 color.red,
				 color.green,
				 color.blue,
#else
				 color.red / 65535.,
				 color.green / 65535.,
				 color.blue / 65535.,
#endif
				 1 - circle_progress);
	  cairo_arc (cr,
		     width / 2,
		     height / 2,
		     circle_progress * width / 2,
		     0, 2 * G_PI);

	  cairo_fill (cr);
	  cairo_stroke (cr);
	}
      else
	{
	  cairo_set_source_rgb (cr, 0., 0., 0.);
	  cairo_set_line_width (cr, 3.);
	  cairo_arc (cr,
		     width / 2,
		     height / 2,
		     circle_progress * width / 2,
		     0, 2 * G_PI);
	  cairo_stroke (cr);

	  cairo_set_source_rgb (cr, 1., 1., 1.);
	  cairo_set_line_width (cr, 1.);
	  cairo_arc (cr,
		     width / 2,
		     height / 2,
		     circle_progress * width / 2,
		     0, 2 * G_PI);
	  cairo_stroke (cr);
	}
    }
  cairo_restore (cr);
}

static void
update_shape (MsdLocatePointerData *data)
{
  cairo_t *cr;
#if GTK_CHECK_VERSION (3, 0, 0)
  cairo_surface_t *mask;
  cairo_region_t *region;

  mask = gdk_window_create_similar_image_surface (data->window,
                                                  CAIRO_FORMAT_A1,
                                                  WINDOW_SIZE,
                                                  WINDOW_SIZE,
                                                  0);
  cr = cairo_create (mask);

  locate_pointer_paint (data, cr, FALSE);

  region = gdk_cairo_region_create_from_surface (mask);

  gdk_window_shape_combine_region (data->window, region, 0, 0);

  cairo_region_destroy (region);
  cairo_destroy (cr);
  cairo_surface_destroy (mask);
#else
  GdkBitmap *mask;

  mask = gdk_pixmap_new (data->window, WINDOW_SIZE, WINDOW_SIZE, 1);
  cr = gdk_cairo_create (mask);

  locate_pointer_paint (data, cr, FALSE);

  gdk_window_shape_combine_mask (data->window, mask, 0, 0);

  cairo_destroy (cr);
  g_object_unref (mask);
#endif
}

static void
timeline_frame_cb (MsdTimeline *timeline,
		   gdouble      progress,
		   gpointer     user_data)
{
  MsdLocatePointerData *data = (MsdLocatePointerData *) user_data;
  GdkScreen *screen = gdk_window_get_screen (data->window);
#if GTK_CHECK_VERSION (3, 20, 0)
  GdkDisplay *display = gdk_window_get_display (data->window);
  GdkSeat *seat;
  GdkDevice *pointer;
#elif GTK_CHECK_VERSION (3, 0, 0)
  GdkDisplay *display = gdk_window_get_display (data->window);
  GdkDeviceManager *device_manager;
  GdkDevice *pointer;
#endif
  gint cursor_x, cursor_y;

  if (gdk_screen_is_composited (screen))
    {
      gtk_widget_queue_draw (GTK_WIDGET (data->widget));
      data->progress = progress;
    }
  else if (progress >= data->progress + CIRCLES_PROGRESS_INTERVAL)
    {
      /* only invalidate window each circle interval */
      update_shape (data);
      gtk_widget_queue_draw (GTK_WIDGET (data->widget));
      data->progress += CIRCLES_PROGRESS_INTERVAL;
    }

#if GTK_CHECK_VERSION (3, 0, 0)
#if GTK_CHECK_VERSION (3, 20, 0)
  seat = gdk_display_get_default_seat (display);
  pointer = gdk_seat_get_pointer (seat);
#else
  device_manager = gdk_display_get_device_manager (display);
  pointer = gdk_device_manager_get_client_pointer (device_manager);
#endif
  gdk_device_get_position (pointer,
                           NULL,
                           &cursor_x,
                           &cursor_y);
#else
  gdk_window_get_pointer (gdk_screen_get_root_window (screen),
                          &cursor_x, &cursor_y, NULL);
#endif
  gtk_window_move (data->widget,
                   cursor_x - WINDOW_SIZE / 2,
                   cursor_y - WINDOW_SIZE / 2);
}

static void
set_transparent_shape (GdkWindow *window)
{
#if GTK_CHECK_VERSION (3, 0, 0)
  cairo_t *cr;
  cairo_surface_t *mask;
  cairo_region_t *region;

  mask = gdk_window_create_similar_image_surface (window,
                                                  CAIRO_FORMAT_A1,
                                                  WINDOW_SIZE,
                                                  WINDOW_SIZE,
                                                  0);
  cr = cairo_create (mask);
#else
  cairo_t *cr;
  GdkBitmap *mask;

  mask = gdk_pixmap_new (data->window, WINDOW_SIZE, WINDOW_SIZE, 1);
  cr = gdk_cairo_create (mask);
#endif

  cairo_set_source_rgba (cr, 1., 1., 1., 0.);
  cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
  cairo_paint (cr);

#if GTK_CHECK_VERSION (3, 0, 0)
  region = gdk_cairo_region_create_from_surface (mask);

  gdk_window_shape_combine_region (window, region, 0, 0);

  cairo_region_destroy (region);
  cairo_destroy (cr);
  cairo_surface_destroy (mask);
#else
  gdk_window_shape_combine_mask (data->window, mask, 0, 0);

  cairo_destroy (cr);
  g_object_unref (mask);
#endif
}

static void
unset_transparent_shape (GdkWindow *window)
{
#if GTK_CHECK_VERSION (3, 0, 0)
  gdk_window_shape_combine_region (window, NULL, 0, 0);
#else
  gdk_window_shape_combine_mask (window, NULL, 0, 0);
#endif
}

static void
composited_changed (GdkScreen            *screen,
                    MsdLocatePointerData *data)
{
  if (gdk_screen_is_composited (screen))
    {
      unset_transparent_shape (data->window);
    }
  else
    {
      set_transparent_shape (data->window);
    }
}

static void
timeline_finished_cb (MsdTimeline *timeline,
		      gpointer     user_data)
{
  MsdLocatePointerData *data = (MsdLocatePointerData *) user_data;
  GdkScreen *screen = gdk_window_get_screen (data->window);

  /* set transparent shape and hide window */
  if (!gdk_screen_is_composited (screen))
    {
      set_transparent_shape (data->window);
    }

  gtk_widget_hide (GTK_WIDGET (data->widget));
}

static void
locate_pointer_unrealize_cb (GtkWidget            *widget,
                             MsdLocatePointerData *data)
{
  if (data->window != NULL)
    {
#if GTK_CHECK_VERSION (3, 0, 0)
      gtk_widget_unregister_window (GTK_WIDGET (data->widget),
                                    data->window);
#else
      gdk_window_set_user_data (data->window, NULL);
#endif
      gdk_window_destroy (data->window);
    }
  data->window = NULL;
}

static void
locate_pointer_realize_cb (GtkWidget            *widget,
                           MsdLocatePointerData *data)
{
  GdkScreen *screen;
#if !GTK_CHECK_VERSION (3, 0, 0)
  GdkColormap *colormap;
#endif
  GdkVisual *visual;
  GdkWindowAttr attributes;
  gint attributes_mask;

  screen = gtk_window_get_screen (data->widget);
#if !GTK_CHECK_VERSION (3, 0, 0)
  colormap = gdk_screen_get_rgba_colormap (screen);
#endif
  visual = gdk_screen_get_rgba_visual (screen);

#if !GTK_CHECK_VERSION (3, 0, 0)
  if (colormap == NULL)
    colormap = gdk_screen_get_system_colormap (screen);
#endif
  if (visual == NULL)
    visual = gdk_screen_get_system_visual (screen);

  locate_pointer_unrealize_cb (GTK_WIDGET (data->widget), data);

  attributes_mask = GDK_WA_X | GDK_WA_Y;
#if !GTK_CHECK_VERSION (3, 0, 0)
  if (colormap != NULL)
    {
      attributes_mask |= GDK_WA_COLORMAP;
    }
#endif
  if (visual != NULL)
    {
      attributes_mask |= GDK_WA_VISUAL;
    }

  attributes.window_type = GDK_WINDOW_TEMP;
  attributes.wclass = GDK_INPUT_OUTPUT;
#if !GTK_CHECK_VERSION (3, 0, 0)
  attributes.colormap = colormap;
#endif
  attributes.visual = visual;
  attributes.event_mask = GDK_VISIBILITY_NOTIFY_MASK | GDK_EXPOSURE_MASK;
  attributes.width = 1;
  attributes.height = 1;

  data->window = gdk_window_new (gdk_screen_get_root_window (screen),
				 &attributes,
				 attributes_mask);

  gtk_widget_set_window (GTK_WIDGET (data->widget),
                         data->window);
#if GTK_CHECK_VERSION (3, 0, 0)
  gtk_widget_register_window (GTK_WIDGET (data->widget),
                              data->window);
#else
  gdk_window_set_user_data (data->window, GTK_WIDGET (data->widget));
#endif
}

#if GTK_CHECK_VERSION (3, 0, 0)
static gboolean
locate_pointer_draw_cb (GtkWidget      *widget,
                        cairo_t        *cr,
                        gpointer        user_data)
{
  MsdLocatePointerData *data = (MsdLocatePointerData *) user_data;
  GdkScreen *screen = gdk_window_get_screen (data->window);

  if (gtk_cairo_should_draw_window (cr, data->window))
    {
      locate_pointer_paint (data, cr, gdk_screen_is_composited (screen));
    }

  return TRUE;
}
#else
static gboolean
locate_pointer_expose_event_cb (GtkWidget      *widget,
                                GdkEventExpose *event,
                                gpointer        user_data)
{
  MsdLocatePointerData *data = (MsdLocatePointerData *) user_data;
  GdkScreen *screen = gdk_window_get_screen (data->window);
  cairo_t *cr;

  if (event->window != data->window)
    {
      return FALSE;
    }

  cr = gdk_cairo_create (data->window);

  gdk_cairo_rectangle (cr, &event->area);
  cairo_clip (cr);

  locate_pointer_paint (data, cr, gdk_screen_is_composited (screen));

  cairo_destroy (cr);
  return TRUE;
}
#endif

static MsdLocatePointerData *
msd_locate_pointer_data_new (GdkScreen *screen)
{
  MsdLocatePointerData *data;

  data = g_new0 (MsdLocatePointerData, 1);

  /* this widget will never be shown, it's
   * mainly used to get signals/events from
   */
  data->widget = GTK_WINDOW (gtk_window_new (GTK_WINDOW_POPUP));

  g_signal_connect (GTK_WIDGET (data->widget), "unrealize",
                    G_CALLBACK (locate_pointer_unrealize_cb),
                    data);
  g_signal_connect (GTK_WIDGET (data->widget), "realize",
                    G_CALLBACK (locate_pointer_realize_cb),
                    data);
#if GTK_CHECK_VERSION (3, 0, 0)
  g_signal_connect (G_OBJECT (data->widget), "draw",
                    G_CALLBACK (locate_pointer_draw_cb),
                    data);
#else
  g_signal_connect (G_OBJECT (data->widget), "expose-event",
                    G_CALLBACK (locate_pointer_expose_event_cb),
                    data);
#endif

  gtk_window_set_screen (data->widget, screen);
  gtk_widget_set_app_paintable (GTK_WIDGET (data->widget), TRUE);
  gtk_widget_realize (GTK_WIDGET (data->widget));

  data->timeline = msd_timeline_new (ANIMATION_LENGTH);
  g_signal_connect (data->timeline, "frame",
		    G_CALLBACK (timeline_frame_cb), data);
  g_signal_connect (data->timeline, "finished",
		    G_CALLBACK (timeline_finished_cb), data);

  return data;
}

static void
move_locate_pointer_window (MsdLocatePointerData *data,
			    GdkScreen            *screen)
{
#if GTK_CHECK_VERSION (3, 20, 0)
  GdkDisplay *display;
  GdkSeat *seat;
  GdkDevice *pointer;
#elif GTK_CHECK_VERSION (3, 0, 0)
  GdkDisplay *display;
  GdkDeviceManager *device_manager;
  GdkDevice *pointer;
#endif
  gint cursor_x, cursor_y;
  cairo_t *cr;
#if GTK_CHECK_VERSION (3, 0, 0)
  cairo_surface_t *mask;
  cairo_region_t *region;
#else
  GdkBitmap *mask;
#endif

#if GTK_CHECK_VERSION (3, 0, 0)
  display = gdk_screen_get_display (screen);
#if GTK_CHECK_VERSION (3, 20, 0)
  seat = gdk_display_get_default_seat (display);
  pointer = gdk_seat_get_pointer (seat);
#else
  device_manager = gdk_display_get_device_manager (display);
  pointer = gdk_device_manager_get_client_pointer (device_manager);
#endif
  gdk_device_get_position (pointer,
                           NULL,
                           &cursor_x,
                           &cursor_y);
#else
  gdk_window_get_pointer (gdk_screen_get_root_window (screen),
                          &cursor_x,
                          &cursor_y,
                          NULL);
#endif

  gtk_window_move (data->widget,
                   cursor_x - WINDOW_SIZE / 2,
                   cursor_y - WINDOW_SIZE / 2);
  gtk_window_resize (data->widget,
                     WINDOW_SIZE, WINDOW_SIZE);

#if GTK_CHECK_VERSION (3, 0, 0)
  mask = gdk_window_create_similar_image_surface (data->window,
                                                  CAIRO_FORMAT_A1,
                                                  WINDOW_SIZE,
                                                  WINDOW_SIZE,
                                                  0);
  cr = cairo_create (mask);

  cairo_set_source_rgba (cr, 0., 0., 0., 0.);
  cairo_paint (cr);

  region = gdk_cairo_region_create_from_surface (mask);

  /* allow events to happen through the window */
  gdk_window_input_shape_combine_region (data->window, region, 0, 0);

  cairo_region_destroy (region);
  cairo_destroy (cr);
  cairo_surface_destroy (mask);
#else
  mask = gdk_pixmap_new (data->window, WINDOW_SIZE, WINDOW_SIZE, 1);
  cr = gdk_cairo_create (mask);

  cairo_set_source_rgba (cr, 0., 0., 0., 0.);
  cairo_paint (cr);

  /* allow events to happen through the window */
  gdk_window_input_shape_combine_mask (data->window, mask, 0, 0);

  cairo_destroy (cr);
  g_object_unref (mask);
#endif
}

void
msd_locate_pointer (GdkScreen *screen)
{
  if (data == NULL)
    {
      data = msd_locate_pointer_data_new (screen);
    }

  msd_timeline_pause (data->timeline);
  msd_timeline_rewind (data->timeline);

  /* Create again the window if it is not for the current screen */
  if (gdk_screen_get_number (screen) != gdk_screen_get_number (gdk_window_get_screen (data->window)))
    {
      gtk_widget_unrealize (GTK_WIDGET (data->widget));
      gtk_window_set_screen (data->widget, screen);
      gtk_widget_realize (GTK_WIDGET (data->widget));
    }

  data->progress = 0.;

  g_signal_connect (screen, "composited-changed",
                    G_CALLBACK (composited_changed), data);

  move_locate_pointer_window (data, screen);
  composited_changed (screen, data);
  gtk_widget_show (GTK_WIDGET (data->widget));

  msd_timeline_start (data->timeline);
}


#define KEYBOARD_GROUP_SHIFT 13
#define KEYBOARD_GROUP_MASK ((1 << 13) | (1 << 14))

/* Owen magic */
static GdkFilterReturn
filter (GdkXEvent *xevent,
        GdkEvent  *event,
        gpointer   data)
{
  GdkScreen *screen;
  GdkDisplay *display;
  XEvent *xev = (XEvent *) xevent;
  guint keyval;
  gint group;

  screen = (GdkScreen *)data;
  display = gdk_screen_get_display (screen);

  if (xev->type == KeyPress || xev->type == KeyRelease)
    {
      /* get the keysym */
      group = (xev->xkey.state & KEYBOARD_GROUP_MASK) >> KEYBOARD_GROUP_SHIFT;
      gdk_keymap_translate_keyboard_state (gdk_keymap_get_default (),
                                           xev->xkey.keycode,
                                           xev->xkey.state,
                                           group,
                                           &keyval,
                                           NULL, NULL, NULL);
      if (keyval == GDK_KEY_Control_L || keyval == GDK_KEY_Control_R)
        {
          if (xev->type == KeyPress)
            {
              XAllowEvents (xev->xkey.display,
                            SyncKeyboard,
                            xev->xkey.time);
            }
          else
            {
              XAllowEvents (xev->xkey.display,
                            AsyncKeyboard,
                            xev->xkey.time);
              msd_locate_pointer (screen);
            }
        }
      else
        {
          XAllowEvents (xev->xkey.display,
                        ReplayKeyboard,
                        xev->xkey.time);
          XUngrabKeyboard (GDK_DISPLAY_XDISPLAY (display),
                           xev->xkey.time);
        }
    }

  return GDK_FILTER_CONTINUE;
}

static void
set_locate_pointer (void)
{
  GdkKeymapKey *keys;
  GdkDisplay *display;
  int n_screens;
  int n_keys;
  gboolean has_entries;
  static const guint keyvals[] = { GDK_KEY_Control_L, GDK_KEY_Control_R };
  unsigned int j;

  display = gdk_display_get_default ();
  n_screens = gdk_display_get_n_screens (display);

  for (j = 0 ; j < G_N_ELEMENTS (keyvals) ; j++)
    {
      has_entries = gdk_keymap_get_entries_for_keyval (gdk_keymap_get_default (),
                                                       keyvals[j],
                                                       &keys,
                                                       &n_keys);
      if (has_entries)
        {
          gint i, j;
          for (i = 0; i < n_keys; i++)
            {
              for (j = 0; j < n_screens; j++)
                {
                  GdkScreen *screen;
                  Window xroot;

                  screen = gdk_display_get_screen (display, j);
                  xroot = GDK_WINDOW_XID (gdk_screen_get_root_window (screen));

                  XGrabKey (GDK_DISPLAY_XDISPLAY (display),
                            keys[i].keycode,
                            0,
                            xroot,
                            False,
                            GrabModeAsync,
                            GrabModeSync);
                  XGrabKey (GDK_DISPLAY_XDISPLAY (display),
                            keys[i].keycode,
                            LockMask,
                            xroot,
                            False,
                            GrabModeAsync,
                            GrabModeSync);
                  XGrabKey (GDK_DISPLAY_XDISPLAY (display),
                            keys[i].keycode,
                            Mod2Mask,
                            xroot,
                            False,
                            GrabModeAsync,
                            GrabModeSync);
                  XGrabKey (GDK_DISPLAY_XDISPLAY (display),
                            keys[i].keycode,
                            Mod4Mask,
                            xroot,
                            False,
                            GrabModeAsync,
                            GrabModeSync);
                }
            }

          g_free (keys);

          for (i = 0; i < n_screens; i++)
            {
              GdkScreen *screen;

              screen = gdk_display_get_screen (display, i);
              gdk_window_add_filter (gdk_screen_get_root_window (screen),
                                     filter,
                                     screen);
            }
        }
    }
}


int
main (int argc, char *argv[])
{
  gtk_init (&argc, &argv);

  set_locate_pointer ();

  gtk_main ();

  return 0;
}

