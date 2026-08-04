/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2026 MATE Developers
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
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

#ifndef __MSD_WLRANDR_MANAGER_H
#define __MSD_WLRANDR_MANAGER_H

#include <glib-object.h>

G_BEGIN_DECLS

#define MSD_TYPE_WLRANDR_MANAGER         (msd_wlrandr_manager_get_type ())
#define MSD_WLRANDR_MANAGER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), MSD_TYPE_WLRANDR_MANAGER, MsdWlrandrManager))
#define MSD_WLRANDR_MANAGER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), MSD_TYPE_WLRANDR_MANAGER, MsdWlrandrManagerClass))
#define MSD_IS_WLRANDR_MANAGER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), MSD_TYPE_WLRANDR_MANAGER))
#define MSD_IS_WLRANDR_MANAGER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), MSD_TYPE_WLRANDR_MANAGER))
#define MSD_WLRANDR_MANAGER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), MSD_TYPE_WLRANDR_MANAGER, MsdWlrandrManagerClass))

typedef struct MsdWlrandrManagerPrivate MsdWlrandrManagerPrivate;

typedef struct
{
        GObject                     parent;
        MsdWlrandrManagerPrivate *priv;
} MsdWlrandrManager;

typedef struct
{
        GObjectClass   parent_class;
} MsdWlrandrManagerClass;

GType                   msd_wlrandr_manager_get_type            (void);

MsdWlrandrManager *     msd_wlrandr_manager_new                 (void);
gboolean                msd_wlrandr_manager_start               (MsdWlrandrManager *manager,
                                                                 GError         **error);
void                    msd_wlrandr_manager_stop                (MsdWlrandrManager *manager);

G_END_DECLS

#endif /* __MSD_WLRANDR_MANAGER_H */
