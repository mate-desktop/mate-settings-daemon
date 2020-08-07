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

#include "config.h"

#include "msd-a11y-keyboard-atspi.h"

#include <glib-object.h>
#include <gdk/gdk.h>
#include <atspi/atspi.h>

struct _MsdA11yKeyboardAtspi
{
        GObject              parent;
        AtspiDeviceListener *listener;
        gboolean             listening;
};

G_DEFINE_TYPE (MsdA11yKeyboardAtspi, msd_a11y_keyboard_atspi, G_TYPE_OBJECT)

static void
msd_a11y_keyboard_atspi_finalize (GObject *obj)
{
        MsdA11yKeyboardAtspi *self = MSD_A11Y_KEYBOARD_ATSPI (obj);

        g_clear_object (&self->listener);
        self->listening = FALSE;

        G_OBJECT_CLASS (msd_a11y_keyboard_atspi_parent_class)->finalize (obj);
}

static void
msd_a11y_keyboard_atspi_class_init (MsdA11yKeyboardAtspiClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);

        object_class->finalize = msd_a11y_keyboard_atspi_finalize;
}

static gboolean
on_key_press_event (const AtspiDeviceEvent *event,
                    void                   *user_data G_GNUC_UNUSED)
{
        /* don't ring on capslock itself, that's taken care of by togglekeys
         * if the user want it. */
        if (event->id == GDK_KEY_Caps_Lock)
                return FALSE;

        gdk_display_beep (gdk_display_get_default ());

        return FALSE;
}

static void
msd_a11y_keyboard_atspi_init (MsdA11yKeyboardAtspi *self)
{
        self->listener = NULL;
        self->listening = FALSE;

#ifndef DESTROYING_ATSPI_LISTENER_DOES_NOT_CRASH
        /* init AT-SPI if needed */
        atspi_init ();

        self->listener = atspi_device_listener_new (on_key_press_event,
                                                    self, NULL);
        /* leak a reference so that this listener is *never* destroyed, to
         * prevent the crash even if our object gets destroyed.
         * See https://gitlab.gnome.org/GNOME/at-spi2-core/-/issues/22 */
        g_object_ref (self->listener);
#endif
}

static void
register_deregister_events (MsdA11yKeyboardAtspi *self,
                            gboolean              do_register)
{
        g_return_if_fail (MSD_IS_A11Y_KEYBOARD_ATSPI (self));
        g_return_if_fail (ATSPI_IS_DEVICE_LISTENER (self->listener));

        /* register listeners for all keys with CAPS_LOCK modifier */
        for (AtspiKeyMaskType mod_mask = 0; mod_mask < 256; mod_mask++)
        {
                if (! (mod_mask & (1 << ATSPI_MODIFIER_SHIFTLOCK)))
                        continue;

                if (do_register)
                        atspi_register_keystroke_listener (self->listener,
                                                           NULL,
                                                           mod_mask,
                                                           1 << ATSPI_KEY_PRESSED_EVENT,
                                                           ATSPI_KEYLISTENER_NOSYNC,
                                                           NULL);
                else
                        atspi_deregister_keystroke_listener (self->listener,
                                                             NULL,
                                                             mod_mask,
                                                             1 << ATSPI_KEY_PRESSED_EVENT,
                                                             NULL);
        }
}

void
msd_a11y_keyboard_atspi_start (MsdA11yKeyboardAtspi *self)
{
        g_return_if_fail (MSD_IS_A11Y_KEYBOARD_ATSPI (self));

        if (self->listening)
                return;

#ifdef DESTROYING_ATSPI_LISTENER_DOES_NOT_CRASH
        /* init AT-SPI if needed */
        atspi_init ();

        self->listener = atspi_device_listener_new (on_key_press_event,
                                                    self, NULL);
#endif
        register_deregister_events (self, TRUE);
        self->listening = TRUE;
}

void
msd_a11y_keyboard_atspi_stop (MsdA11yKeyboardAtspi *self)
{
        g_return_if_fail (MSD_IS_A11Y_KEYBOARD_ATSPI (self));

        if (! self->listening)
                return;

#ifdef DESTROYING_ATSPI_LISTENER_DOES_NOT_CRASH
        g_clear_object (&self->listener);
#else
        register_deregister_events (self, FALSE);
#endif
        self->listening = FALSE;
}

MsdA11yKeyboardAtspi *
msd_a11y_keyboard_atspi_new ()
{
        return g_object_new (MSD_TYPE_A11Y_KEYBOARD_ATSPI, NULL);
}
