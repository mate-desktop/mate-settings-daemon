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

#ifndef __MSD_A11Y_KEYBOARD_MANAGER_H
#define __MSD_A11Y_KEYBOARD_MANAGER_H

#include <glib-object.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MSD_TYPE_A11Y_KEYBOARD_MANAGER         (msd_a11y_keyboard_manager_get_type ())
#define MSD_A11Y_KEYBOARD_MANAGER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), MSD_TYPE_A11Y_KEYBOARD_MANAGER, MsdA11yKeyboardManager))
#define MSD_A11Y_KEYBOARD_MANAGER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), MSD_TYPE_A11Y_KEYBOARD_MANAGER, MsdA11yKeyboardManagerClass))
#define MSD_IS_A11Y_KEYBOARD_MANAGER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), MSD_TYPE_A11Y_KEYBOARD_MANAGER))
#define MSD_IS_A11Y_KEYBOARD_MANAGER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), MSD_TYPE_A11Y_KEYBOARD_MANAGER))
#define MSD_A11Y_KEYBOARD_MANAGER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), MSD_TYPE_A11Y_KEYBOARD_MANAGER, MsdA11yKeyboardManagerClass))

typedef struct MsdA11yKeyboardManagerPrivate MsdA11yKeyboardManagerPrivate;

typedef struct
{
        GObject                     parent;
        MsdA11yKeyboardManagerPrivate *priv;
} MsdA11yKeyboardManager;

typedef struct
{
        GObjectClass   parent_class;
} MsdA11yKeyboardManagerClass;

GType                   msd_a11y_keyboard_manager_get_type            (void);

MsdA11yKeyboardManager *msd_a11y_keyboard_manager_new                 (void);
gboolean                msd_a11y_keyboard_manager_start               (MsdA11yKeyboardManager *manager,
                                                                       GError                **error);
void                    msd_a11y_keyboard_manager_stop                (MsdA11yKeyboardManager *manager);

#ifdef __cplusplus
}
#endif

#endif /* __MSD_A11Y_KEYBOARD_MANAGER_H */
