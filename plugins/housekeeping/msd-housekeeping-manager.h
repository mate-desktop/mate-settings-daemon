/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2008 Michael J. Chudobiak <mjc@avtechpulse.com>
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

#ifndef __MSD_HOUSEKEEPING_MANAGER_H
#define __MSD_HOUSEKEEPING_MANAGER_H

#include <glib-object.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MSD_TYPE_HOUSEKEEPING_MANAGER         (msd_housekeeping_manager_get_type ())
#define MSD_HOUSEKEEPING_MANAGER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), MSD_TYPE_HOUSEKEEPING_MANAGER, MsdHousekeepingManager))
#define MSD_HOUSEKEEPING_MANAGER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), MSD_TYPE_HOUSEKEEPING_MANAGER, MsdHousekeepingManagerClass))
#define MSD_IS_HOUSEKEEPING_MANAGER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), MSD_TYPE_HOUSEKEEPING_MANAGER))
#define MSD_IS_HOUSEKEEPING_MANAGER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), MSD_TYPE_HOUSEKEEPING_MANAGER))
#define MSD_HOUSEKEEPING_MANAGER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), MSD_TYPE_HOUSEKEEPING_MANAGER, MsdHousekeepingManagerClass))

typedef struct MsdHousekeepingManagerPrivate MsdHousekeepingManagerPrivate;

typedef struct {
        GObject                        parent;
        MsdHousekeepingManagerPrivate *priv;
} MsdHousekeepingManager;

typedef struct {
        GObjectClass   parent_class;
} MsdHousekeepingManagerClass;

GType                    msd_housekeeping_manager_get_type      (void);

MsdHousekeepingManager * msd_housekeeping_manager_new           (void);
gboolean                 msd_housekeeping_manager_start         (MsdHousekeepingManager  *manager,
                                                                 GError                 **error);
void                     msd_housekeeping_manager_stop          (MsdHousekeepingManager  *manager);

#ifdef __cplusplus
}
#endif

#endif /* __MSD_HOUSEKEEPING_MANAGER_H */
