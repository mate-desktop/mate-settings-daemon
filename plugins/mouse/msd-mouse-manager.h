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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#ifndef __MSD_MOUSE_MANAGER_H
#define __MSD_MOUSE_MANAGER_H

#include <glib-object.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MSD_TYPE_MOUSE_MANAGER         (msd_mouse_manager_get_type ())
#define MSD_MOUSE_MANAGER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), MSD_TYPE_MOUSE_MANAGER, MsdMouseManager))
#define MSD_MOUSE_MANAGER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), MSD_TYPE_MOUSE_MANAGER, MsdMouseManagerClass))
#define MSD_IS_MOUSE_MANAGER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), MSD_TYPE_MOUSE_MANAGER))
#define MSD_IS_MOUSE_MANAGER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), MSD_TYPE_MOUSE_MANAGER))
#define MSD_MOUSE_MANAGER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), MSD_TYPE_MOUSE_MANAGER, MsdMouseManagerClass))

typedef struct MsdMouseManagerPrivate MsdMouseManagerPrivate;

typedef struct
{
        GObject                     parent;
        MsdMouseManagerPrivate *priv;
} MsdMouseManager;

typedef struct
{
        GObjectClass   parent_class;
} MsdMouseManagerClass;

GType                   msd_mouse_manager_get_type            (void);

MsdMouseManager *       msd_mouse_manager_new                 (void);
gboolean                msd_mouse_manager_start               (MsdMouseManager *manager,
                                                               GError         **error);
void                    msd_mouse_manager_stop                (MsdMouseManager *manager);

#ifdef __cplusplus
}
#endif

#endif /* __MSD_MOUSE_MANAGER_H */
