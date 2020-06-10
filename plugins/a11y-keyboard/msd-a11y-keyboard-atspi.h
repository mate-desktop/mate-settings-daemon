/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2020 Colomban Wendling <cwendling@hypra.fr>
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
 */

#ifndef __MSD_A11Y_KEYBOARD_ATSPI_H
#define __MSD_A11Y_KEYBOARD_ATSPI_H

#include <glib-object.h>

G_BEGIN_DECLS

#define MSD_TYPE_A11Y_KEYBOARD_ATSPI            (msd_a11y_keyboard_atspi_get_type ())
#define MSD_A11Y_KEYBOARD_ATSPI(o)              (G_TYPE_CHECK_INSTANCE_CAST ((o), MSD_TYPE_A11Y_KEYBOARD_ATSPI, MsdA11yKeyboardAtspi))
#define MSD_A11Y_KEYBOARD_ATSPI_CLASS(k)        (G_TYPE_CHECK_CLASS_CAST((k), MSD_TYPE_A11Y_KEYBOARD_ATSPI, MsdA11yKeyboardAtspiClass))
#define MSD_IS_A11Y_KEYBOARD_ATSPI(o)           (G_TYPE_CHECK_INSTANCE_TYPE ((o), MSD_TYPE_A11Y_KEYBOARD_ATSPI))
#define MSD_IS_A11Y_KEYBOARD_ATSPI_CLASS(k)     (G_TYPE_CHECK_CLASS_TYPE ((k), MSD_TYPE_A11Y_KEYBOARD_ATSPI))
#define MSD_A11Y_KEYBOARD_ATSPI_GET_CLASS(o)    (G_TYPE_INSTANCE_GET_CLASS ((o), MSD_TYPE_A11Y_KEYBOARD_ATSPI, MsdA11yKeyboardAtspiClass))

typedef struct MsdA11yKeyboardAtspiPrivate MsdA11yKeyboardAtspiPrivate;

typedef struct
{
        GObject                         parent;
        MsdA11yKeyboardAtspiPrivate    *priv;
} MsdA11yKeyboardAtspi;

typedef struct
{
        GObjectClass   parent_class;
} MsdA11yKeyboardAtspiClass;

GType                   msd_a11y_keyboard_atspi_get_type        (void);

MsdA11yKeyboardAtspi   *msd_a11y_keyboard_atspi_new             (void);
void                    msd_a11y_keyboard_atspi_start           (MsdA11yKeyboardAtspi *self);
void                    msd_a11y_keyboard_atspi_stop            (MsdA11yKeyboardAtspi *self);

G_END_DECLS

#endif
