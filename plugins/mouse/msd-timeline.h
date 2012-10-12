/* msdtimeline.c
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

#ifndef __MSD_TIMELINE_H__
#define __MSD_TIMELINE_H__

#include <glib-object.h>
#include <gdk/gdk.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MSD_TYPE_TIMELINE_DIRECTION       (msd_timeline_direction_get_type ())
#define MSD_TYPE_TIMELINE_PROGRESS_TYPE   (msd_timeline_progress_type_get_type ())
#define MSD_TYPE_TIMELINE                 (msd_timeline_get_type ())
#define MSD_TIMELINE(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), MSD_TYPE_TIMELINE, MsdTimeline))
#define MSD_TIMELINE_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass),  MSD_TYPE_TIMELINE, MsdTimelineClass))
#define MSD_IS_TIMELINE(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MSD_TYPE_TIMELINE))
#define MSD_IS_TIMELINE_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass),  MSD_TYPE_TIMELINE))
#define MSD_TIMELINE_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj),  MSD_TYPE_TIMELINE, MsdTimelineClass))

typedef enum {
  MSD_TIMELINE_DIRECTION_FORWARD,
  MSD_TIMELINE_DIRECTION_BACKWARD
} MsdTimelineDirection;

typedef enum {
  MSD_TIMELINE_PROGRESS_LINEAR,
  MSD_TIMELINE_PROGRESS_SINUSOIDAL,
  MSD_TIMELINE_PROGRESS_EXPONENTIAL
} MsdTimelineProgressType;

typedef struct MsdTimeline      MsdTimeline;
typedef struct MsdTimelineClass MsdTimelineClass;

struct MsdTimeline
{
  GObject parent_instance;
};

struct MsdTimelineClass
{
  GObjectClass parent_class;

  void (* started)           (MsdTimeline *timeline);
  void (* finished)          (MsdTimeline *timeline);
  void (* paused)            (MsdTimeline *timeline);

  void (* frame)             (MsdTimeline *timeline,
			      gdouble      progress);

  void (* __msd_reserved1) (void);
  void (* __msd_reserved2) (void);
  void (* __msd_reserved3) (void);
  void (* __msd_reserved4) (void);
};

typedef gdouble (*MsdTimelineProgressFunc) (gdouble progress);


GType                   msd_timeline_get_type           (void) G_GNUC_CONST;
GType                   msd_timeline_direction_get_type (void) G_GNUC_CONST;
GType                   msd_timeline_progress_type_get_type (void) G_GNUC_CONST;

MsdTimeline            *msd_timeline_new                (guint                    duration);
MsdTimeline            *msd_timeline_new_for_screen     (guint                    duration,
							 GdkScreen               *screen);

void                    msd_timeline_start              (MsdTimeline             *timeline);
void                    msd_timeline_pause              (MsdTimeline             *timeline);
void                    msd_timeline_rewind             (MsdTimeline             *timeline);

gboolean                msd_timeline_is_running         (MsdTimeline             *timeline);

guint                   msd_timeline_get_fps            (MsdTimeline             *timeline);
void                    msd_timeline_set_fps            (MsdTimeline             *timeline,
							 guint                    fps);

gboolean                msd_timeline_get_loop           (MsdTimeline             *timeline);
void                    msd_timeline_set_loop           (MsdTimeline             *timeline,
							 gboolean                 loop);

guint                   msd_timeline_get_duration       (MsdTimeline             *timeline);
void                    msd_timeline_set_duration       (MsdTimeline             *timeline,
							 guint                    duration);

GdkScreen              *msd_timeline_get_screen         (MsdTimeline             *timeline);
void                    msd_timeline_set_screen         (MsdTimeline             *timeline,
							 GdkScreen               *screen);

MsdTimelineDirection    msd_timeline_get_direction      (MsdTimeline             *timeline);
void                    msd_timeline_set_direction      (MsdTimeline             *timeline,
							 MsdTimelineDirection     direction);

MsdTimelineProgressType msd_timeline_get_progress_type  (MsdTimeline             *timeline);
void                    msd_timeline_set_progress_type  (MsdTimeline             *timeline,
							 MsdTimelineProgressType  type);
void                    msd_timeline_get_progress_func  (MsdTimeline             *timeline);

void                    msd_timeline_set_progress_func  (MsdTimeline             *timeline,
							 MsdTimelineProgressFunc  progress_func);

gdouble                 msd_timeline_get_progress       (MsdTimeline             *timeline);


#ifdef __cplusplus
}
#endif

#endif /* __MSD_TIMELINE_H__ */
