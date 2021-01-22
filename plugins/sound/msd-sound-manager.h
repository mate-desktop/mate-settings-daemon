/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2008 Lennart Poettering <lennart@poettering.net>
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

#ifndef MSD_SOUND_MANAGER_H
#define MSD_SOUND_MANAGER_H

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define MSD_TYPE_SOUND_MANAGER (msd_sound_manager_get_type ())

G_DECLARE_FINAL_TYPE (MsdSoundManager, msd_sound_manager,
                      MSD, SOUND_MANAGER, GObject)

MsdSoundManager * msd_sound_manager_new   (void);

gboolean          msd_sound_manager_start (MsdSoundManager  *manager,
                                           GError          **error);

void              msd_sound_manager_stop  (MsdSoundManager *manager);

G_END_DECLS

#endif /* MSD_SOUND_MANAGER_H */
