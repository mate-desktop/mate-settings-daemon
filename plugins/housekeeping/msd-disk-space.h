/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 * vim: set et sw=8 ts=8:
 *
 * Copyright (c) 2008, Novell, Inc.
 *
 * Authors: Vincent Untz <vuntz@gnome.org>
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

#ifndef __MSD_DISK_SPACE_H
#define __MSD_DISK_SPACE_H

#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif

void msd_ldsm_setup (gboolean check_now);
void msd_ldsm_clean (void);

#ifdef __cplusplus
}
#endif

#endif /* __MSD_DISK_SPACE_H */
