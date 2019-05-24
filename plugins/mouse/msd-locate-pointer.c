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
  GdkRGBA color;
  gdouble progress, circle_progress;
  gint width, height, i;
  GtkStyleContext *style;

  color.red = color.green = color.blue = 0.7;
  color.alpha = 0.;

  progress = data->progress;

  width = gdk_window_get_width (data->window);
  height = gdk_window_get_height (data->window);

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
				 color.red,
				 color.green,
				 color.blue,
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
}

static void
timeline_frame_cb (MsdTimeline *timeline,
		   gdouble      progress,
		   gpointer     user_data)
{
  MsdLocatePointerData *data = (MsdLocatePointerData *) user_data;
  GdkDisplay *display = gdk_window_get_display (data->window);
  GdkScreen *screen = gdk_display_get_default_screen (display);
  GdkSeat *seat;
  GdkDevice *pointer;
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

  seat = gdk_display_get_default_seat (display);
  pointer = gdk_seat_get_pointer (seat);
  gdk_device_get_position (pointer,
                           NULL,
                           &cursor_x,
                           &cursor_y);

  gtk_window_move (data->widget,
                   cursor_x - WINDOW_SIZE / 2,
                   cursor_y - WINDOW_SIZE / 2);
}

static void
set_transparent_shape (GdkWindow *window)
{
  cairo_t *cr;
  cairo_surface_t *mask;
  cairo_region_t *region;

  mask = gdk_window_create_similar_image_surface (window,
                                                  CAIRO_FORMAT_A1,
                                                  WINDOW_SIZE,
                                                  WINDOW_SIZE,
                                                  0);
  cr = cairo_create (mask);

  cairo_set_source_rgba (cr, 1., 1., 1., 0.);
  cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
  cairo_paint (cr);

  region = gdk_cairo_region_create_from_surface (mask);

  gdk_window_shape_combine_region (window, region, 0, 0);

  cairo_region_destroy (region);
  cairo_destroy (cr);
  cairo_surface_destroy (mask);
}

static void
unset_transparent_shape (GdkWindow *window)
{
  gdk_window_shape_combine_region (window, NULL, 0, 0);
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
      gtk_widget_unregister_window (GTK_WIDGET (data->widget),
                                    data->window);
      gdk_window_destroy (data->window);
    }
  data->window = NULL;
}

static void
locate_pointer_realize_cb (GtkWidget            *widget,
                           MsdLocatePointerData *data)
{
  GdkDisplay *display;
  GdkScreen *screen;
  GdkVisual *visual;
  GdkWindowAttr attributes;
  gint attributes_mask;

  display = gtk_widget_get_display (GTK_WIDGET (data->widget));
  screen = gdk_display_get_default_screen (display);
  visual = gdk_screen_get_rgba_visual (screen);

  if (visual == NULL)
    visual = gdk_screen_get_system_visual (screen);

  locate_pointer_unrealize_cb (GTK_WIDGET (data->widget), data);

  attributes_mask = GDK_WA_X | GDK_WA_Y;
  if (visual != NULL)
    {
      attributes_mask |= GDK_WA_VISUAL;
    }

  attributes.window_type = GDK_WINDOW_TEMP;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.visual = visual;
  attributes.event_mask = GDK_VISIBILITY_NOTIFY_MASK | GDK_EXPOSURE_MASK;
  attributes.width = 1;
  attributes.height = 1;

  data->window = gdk_window_new (gdk_screen_get_root_window (screen),
				 &attributes,
				 attributes_mask);

  gtk_widget_set_window (GTK_WIDGET (data->widget),
                         data->window);
  gtk_widget_register_window (GTK_WIDGET (data->widget),
                              data->window);
}

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

static MsdLocatePointerData *
msd_locate_pointer_data_new (void)
{
  MsdLocatePointerData *data;

  data = g_new0 (MsdLocatePointerData, 1);

  data->widget = GTK_WINDOW (gtk_window_new (GTK_WINDOW_POPUP));

  g_signal_connect (GTK_WIDGET (data->widget), "unrealize",
                    G_CALLBACK (locate_pointer_unrealize_cb),
                    data);
  g_signal_connect (GTK_WIDGET (data->widget), "realize",
                    G_CALLBACK (locate_pointer_realize_cb),
                    data);
  g_signal_connect (GTK_WIDGET (data->widget), "draw",
                    G_CALLBACK (locate_pointer_draw_cb),
                    data);

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
			    GdkDisplay           *display)
{
  GdkSeat *seat;
  GdkDevice *pointer;
  gint cursor_x, cursor_y;
  cairo_t *cr;
  cairo_surface_t *mask;
  cairo_region_t *region;

  seat = gdk_display_get_default_seat (display);
  pointer = gdk_seat_get_pointer (seat);
  gdk_device_get_position (pointer,
                           NULL,
                           &cursor_x,
                           &cursor_y);

  gtk_window_move (data->widget,
                   cursor_x - WINDOW_SIZE / 2,
                   cursor_y - WINDOW_SIZE / 2);
  gtk_window_resize (data->widget,
                     WINDOW_SIZE, WINDOW_SIZE);

  mask = gdk_window_create_similar_image_surface (data->window,
                                                  CAIRO_FORMAT_A1,
                                                  WINDOW_SIZE,
                                                  WINDOW_SIZE,
                                                  0);
  cr = cairo_create (mask);

  cairo_set_source_rgba (cr, 0., 0., 0., 0.);
  cairo_paint (cr);

  region = gdk_cairo_region_create_from_surface (mask);

  gdk_window_input_shape_combine_region (data->window, region, 0, 0);

  cairo_region_destroy (region);
  cairo_destroy (cr);
  cairo_surface_destroy (mask);
}

void
msd_locate_pointer (GdkDisplay *display)
{
  GdkScreen *screen = gdk_display_get_default_screen (display);

  if (data == NULL)
    {
      data = msd_locate_pointer_data_new ();
    }

  msd_timeline_pause (data->timeline);
  msd_timeline_rewind (data->timeline);

  data->progress = 0.;

  g_signal_connect (screen, "composited-changed",
                    G_CALLBACK (composited_changed), data);

  move_locate_pointer_window (data, display);
  composited_changed (screen, data);
  gtk_widget_show (GTK_WIDGET (data->widget));

  msd_timeline_start (data->timeline);
}


#define KEYBOARD_GROUP_SHIFT 13
#define KEYBOARD_GROUP_MASK ((1 << 13) | (1 << 14))

/* Owen magic */
static GdkFilterReturn
event_filter (GdkXEvent *gdkxevent,
              GdkEvent  *event,
              gpointer   user_data)
{
  XEvent *xevent = (XEvent *) gdkxevent;
  GdkDisplay *display = (GdkDisplay *) user_data;

  if (xevent->xany.type == KeyPress || xevent->xany.type == KeyRelease)
    {
      guint keyval;
      gint group;

      /* get the keysym */
      group = (xevent->xkey.state & KEYBOARD_GROUP_MASK) >> KEYBOARD_GROUP_SHIFT;
      gdk_keymap_translate_keyboard_state (gdk_keymap_get_for_display (display),
                                           xevent->xkey.keycode,
                                           xevent->xkey.state,
                                           group,
                                           &keyval,
                                           NULL, NULL, NULL);

      if (keyval == GDK_KEY_Control_L || keyval == GDK_KEY_Control_R)
        {
          if (xevent->xany.type == KeyRelease)
            {
              XAllowEvents (xevent->xany.display,
                            AsyncKeyboard,
                            xevent->xkey.time);
              msd_locate_pointer (display);
            }
          else
            {
              XAllowEvents (xevent->xany.display,
                            SyncKeyboard,
                            xevent->xkey.time);
            }
        }
      else
        {
          XAllowEvents (xevent->xany.display,
                        ReplayKeyboard,
                        xevent->xkey.time);
          XUngrabButton (xevent->xany.display,
                         AnyButton,
                         AnyModifier,
                         xevent->xany.window);
          XUngrabKeyboard (xevent->xany.display,
                           xevent->xkey.time);
        }
    }
  else if (xevent->xany.type == ButtonPress)
    {
      XAllowEvents (xevent->xany.display,
                    ReplayPointer,
                    xevent->xbutton.time);
      XUngrabButton (xevent->xany.display,
                     AnyButton,
                     AnyModifier,
                     xevent->xany.window);
      XUngrabKeyboard (xevent->xany.display,
                       xevent->xbutton.time);
    }

  return GDK_FILTER_CONTINUE;
}

static void
set_locate_pointer (void)
{
  GdkKeymapKey *keys;
  GdkDisplay *display;
  GdkScreen *screen;
  int n_keys;
  gboolean has_entries = FALSE;
  static const guint keyvals[] = { GDK_KEY_Control_L, GDK_KEY_Control_R };
  unsigned int i, j;

  display = gdk_display_get_default ();
  screen = gdk_display_get_default_screen (display);

  for (i = 0; i < G_N_ELEMENTS (keyvals); ++i)
    {
      if (gdk_keymap_get_entries_for_keyval (gdk_keymap_get_for_display (display),
                                             keyvals[i],
                                             &keys,
                                             &n_keys))
        {
          has_entries = TRUE;
          for (j = 0; j < n_keys; ++j)
            {
              Window xroot;

              xroot = GDK_WINDOW_XID (gdk_screen_get_root_window (screen));

              XGrabKey (GDK_DISPLAY_XDISPLAY (display),
                        keys[j].keycode,
                        0,
                        xroot,
                        False,
                        GrabModeAsync,
                        GrabModeSync);
              XGrabKey (GDK_DISPLAY_XDISPLAY (display),
                        keys[j].keycode,
                        LockMask,
                        xroot,
                        False,
                        GrabModeAsync,
                        GrabModeSync);
              XGrabKey (GDK_DISPLAY_XDISPLAY (display),
                        keys[j].keycode,
                        Mod2Mask,
                        xroot,
                        False,
                        GrabModeAsync,
                        GrabModeSync);
              XGrabKey (GDK_DISPLAY_XDISPLAY (display),
                        keys[j].keycode,
                        Mod4Mask,
                        xroot,
                        False,
                        GrabModeAsync,
                        GrabModeSync);
            }
          g_free (keys);
        }
    }

  if (has_entries)
    {
      gdk_window_add_filter (gdk_screen_get_root_window (screen),
                             (GdkFilterFunc) event_filter,
                             display);
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

