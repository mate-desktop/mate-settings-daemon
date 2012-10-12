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

#ifndef __MSD_KEYBOARD_MANAGER_H
#define __MSD_KEYBOARD_MANAGER_H

#include <glib-object.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MSD_TYPE_KEYBOARD_MANAGER         (msd_keyboard_manager_get_type ())
#define MSD_KEYBOARD_MANAGER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), MSD_TYPE_KEYBOARD_MANAGER, MsdKeyboardManager))
#define MSD_KEYBOARD_MANAGER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), MSD_TYPE_KEYBOARD_MANAGER, MsdKeyboardManagerClass))
#define MSD_IS_KEYBOARD_MANAGER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), MSD_TYPE_KEYBOARD_MANAGER))
#define MSD_IS_KEYBOARD_MANAGER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), MSD_TYPE_KEYBOARD_MANAGER))
#define MSD_KEYBOARD_MANAGER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), MSD_TYPE_KEYBOARD_MANAGER, MsdKeyboardManagerClass))

typedef struct MsdKeyboardManagerPrivate MsdKeyboardManagerPrivate;

typedef struct
{
        GObject                     parent;
        MsdKeyboardManagerPrivate *priv;
} MsdKeyboardManager;

typedef struct
{
        GObjectClass   parent_class;
} MsdKeyboardManagerClass;

GType                   msd_keyboard_manager_get_type            (void);

MsdKeyboardManager *       msd_keyboard_manager_new                 (void);
gboolean                msd_keyboard_manager_start               (MsdKeyboardManager *manager,
                                                               GError         **error);
void                    msd_keyboard_manager_stop                (MsdKeyboardManager *manager);
void                    msd_keyboard_manager_apply_settings      (MsdKeyboardManager *manager);

#ifdef __cplusplus
}
#endif

#endif /* __MSD_KEYBOARD_MANAGER_H */
