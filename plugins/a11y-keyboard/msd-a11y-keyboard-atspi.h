/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2020 Colomban Wendling <cwendling@hypra.fr>
 * Copyright (C) 2012-2021 MATE Developers
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

#define MSD_TYPE_A11Y_KEYBOARD_ATSPI (msd_a11y_keyboard_atspi_get_type ())
G_DECLARE_FINAL_TYPE (MsdA11yKeyboardAtspi, msd_a11y_keyboard_atspi,
                      MSD, A11Y_KEYBOARD_ATSPI, GObject)

MsdA11yKeyboardAtspi   *msd_a11y_keyboard_atspi_new             (void);
void                    msd_a11y_keyboard_atspi_start           (MsdA11yKeyboardAtspi *self);
void                    msd_a11y_keyboard_atspi_stop            (MsdA11yKeyboardAtspi *self);

G_END_DECLS

#endif
