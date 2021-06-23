/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007 William Jon McCann <mccann@jhu.edu>
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

#ifndef MSD_BACKGROUND_MANAGER_H
#define MSD_BACKGROUND_MANAGER_H

#include <glib-object.h>

G_BEGIN_DECLS

#define MSD_TYPE_BACKGROUND_MANAGER         (msd_background_manager_get_type())

G_DECLARE_FINAL_TYPE (MsdBackgroundManager, msd_background_manager, MSD, BACKGROUND_MANAGER, GObject)

MsdBackgroundManager * msd_background_manager_new   (void);

gboolean               msd_background_manager_start (MsdBackgroundManager  *manager,
                                                     GError               **error);

void                   msd_background_manager_stop  (MsdBackgroundManager  *manager);

G_END_DECLS

#endif /* MSD_BACKGROUND_MANAGER_H */
