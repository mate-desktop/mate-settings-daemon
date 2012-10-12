/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2008 Jens Granseuer <jensgr@gmx.net>
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
 */

#ifndef __MSD_COMMON_KEYGRAB_H
#define __MSD_COMMON_KEYGRAB_H

#ifdef __cplusplus
extern "C" {
#endif

#include <glib.h>
#include <X11/keysym.h>

typedef struct {
        guint keysym;
        guint state;
        guint *keycodes;
} Key;


void	        grab_key_unsafe	(Key     *key,
		        	 gboolean grab,
			         GSList  *screens);

gboolean        match_key       (Key     *key,
                                 XEvent  *event);

gboolean        key_uses_keycode (const Key *key,
                                  guint keycode);

#ifdef __cplusplus
}
#endif

#endif /* __MSD_COMMON_KEYGRAB_H */
