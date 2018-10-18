/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007 William Jon McCann <mccann@jhu.edu>
 * Copyright (C) 2010 Red Hat, Inc.
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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301,
 * USA.
 */

#ifndef __MSD_RFKILL_MANAGER_H
#define __MSD_RFKILL_MANAGER_H

#include <glib-object.h>

G_BEGIN_DECLS

#define MSD_TYPE_RFKILL_MANAGER         (msd_rfkill_manager_get_type ())
#define MSD_RFKILL_MANAGER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), MSD_TYPE_RFKILL_MANAGER, MsdRfkillManager))
#define MSD_RFKILL_MANAGER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), MSD_TYPE_RFKILL_MANAGER, MsdRfkillManagerClass))
#define MSD_IS_RFKILL_MANAGER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), MSD_TYPE_RFKILL_MANAGER))
#define MSD_IS_RFKILL_MANAGER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), MSD_TYPE_RFKILL_MANAGER))
#define MSD_RFKILL_MANAGER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), MSD_TYPE_RFKILL_MANAGER, MsdRfkillManagerClass))

typedef struct MsdRfkillManagerPrivate MsdRfkillManagerPrivate;

typedef struct
{
        GObject                     parent;
        MsdRfkillManagerPrivate *priv;
} MsdRfkillManager;

typedef struct
{
        GObjectClass   parent_class;
} MsdRfkillManagerClass;

GType                   msd_rfkill_manager_get_type            (void);

MsdRfkillManager *       msd_rfkill_manager_new                 (void);
gboolean                msd_rfkill_manager_start               (MsdRfkillManager *manager,
                                                               GError         **error);
void                    msd_rfkill_manager_stop                (MsdRfkillManager *manager);

G_END_DECLS

#endif /* __MSD_RFKILL_MANAGER_H */
