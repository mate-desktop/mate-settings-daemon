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

#ifdef HAVE_LIBATSPI
#include <gdk/gdk.h>
#include <atspi/atspi.h>


/* ugly workaround https://gitlab.gnome.org/GNOME/at-spi2-core/-/issues/22 */
static AtspiDeviceListener *
WORKAROUND_atspi_device_listener_new (AtspiDeviceListenerCB callback,
                                      void                 *user_data,
                                      GDestroyNotify        callback_destroyed)
{
        AtspiDeviceListener *listener;

        listener = atspi_device_listener_new (callback, user_data,
                                              callback_destroyed);

        /* Yes, we do leak a reference.  But we have to, because there's a
         * nasty bug in libatspi [1] where if a listener is destroyed it leads
         * to invalid memory access and potentially a crash.
         * [1] https://gitlab.gnome.org/GNOME/at-spi2-core/-/issues/22 */
        return g_object_ref (listener);
}
#define atspi_device_listener_new WORKAROUND_atspi_device_listener_new


struct MsdA11yKeyboardAtspiPrivate
{
        AtspiDeviceListener *listener;
        gboolean             listening;
};

G_DEFINE_TYPE_WITH_PRIVATE (MsdA11yKeyboardAtspi, msd_a11y_keyboard_atspi, G_TYPE_OBJECT)

static void
msd_a11y_keyboard_atspi_finalize (GObject *obj)
{
        MsdA11yKeyboardAtspi *self = MSD_A11Y_KEYBOARD_ATSPI (obj);

        g_clear_object (&self->priv->listener);
        self->priv->listening = FALSE;

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
        /* don't ring when disabling capslock */
        if (event->id == GDK_KEY_Caps_Lock &&
            event->modifiers & (1 << ATSPI_MODIFIER_SHIFTLOCK))
                return FALSE;

        gdk_display_beep (gdk_display_get_default ());

        return FALSE;
}

static void
msd_a11y_keyboard_atspi_init (MsdA11yKeyboardAtspi *self)
{
        self->priv = msd_a11y_keyboard_atspi_get_instance_private (self);

        /* init AT-SPI if needed */
        atspi_init ();

        /* Ideally we'd create the listener in start() only -- we don't need
         * it otherwise.  But there's a bug [1] in libatspi where destroying a
         * listener leads to a crash, so we have a workaround keeping the
         * listener alive, and thus try and avoid creating (and not destroying)
         * listener more often than absolutely necessary.
         * [1] https://gitlab.gnome.org/GNOME/at-spi2-core/-/issues/22 */
        self->priv->listener = atspi_device_listener_new (on_key_press_event,
                                                          self, NULL);
        self->priv->listening = FALSE;
}

static void
register_deregister_events (MsdA11yKeyboardAtspi *self,
                            gboolean              do_register)
{
        AtspiKeyDefinition shiftlock_key;
        GArray *shiftlock_key_set;

        g_return_if_fail (MSD_IS_A11Y_KEYBOARD_ATSPI (self));
        g_return_if_fail (ATSPI_IS_DEVICE_LISTENER (self->priv->listener));

        /* register listeners for CAPS_LOCK with any modifier but CAPS_LOCK,
         * and all keys with CAPS_LOCK modifier, so we grab when CAPS_LOCK is
         * active or gets activated */
        shiftlock_key.keycode = 0;
        shiftlock_key.keysym = GDK_KEY_Caps_Lock;
        shiftlock_key.keystring = NULL;

        shiftlock_key_set = g_array_new (FALSE, FALSE, sizeof shiftlock_key);
        g_array_append_val (shiftlock_key_set, shiftlock_key);

        for (AtspiKeyMaskType mod_mask = 0; mod_mask < 256; mod_mask++)
        {
                GArray *key_set;

                if (mod_mask & (1 << ATSPI_MODIFIER_SHIFTLOCK))
                        key_set = NULL;
                else
                        key_set = shiftlock_key_set;

                if (do_register)
                        atspi_register_keystroke_listener (self->priv->listener,
                                                           key_set,
                                                           mod_mask,
                                                           1 << ATSPI_KEY_PRESSED_EVENT,
                                                           ATSPI_KEYLISTENER_NOSYNC,
                                                           NULL);
                else
                        atspi_deregister_keystroke_listener (self->priv->listener,
                                                             key_set,
                                                             mod_mask,
                                                             1 << ATSPI_KEY_PRESSED_EVENT,
                                                             NULL);
        }

        g_array_unref (shiftlock_key_set);

        self->priv->listening = do_register;
}

void
msd_a11y_keyboard_atspi_start (MsdA11yKeyboardAtspi *self)
{
        g_return_if_fail (MSD_IS_A11Y_KEYBOARD_ATSPI (self));

        if (! self->priv->listening)
                register_deregister_events (self, TRUE);
}

void
msd_a11y_keyboard_atspi_stop (MsdA11yKeyboardAtspi *self)
{
        g_return_if_fail (MSD_IS_A11Y_KEYBOARD_ATSPI (self));

        if (self->priv->listening)
                register_deregister_events (self, FALSE);
}

#else /* ! defined(HAVE_LIBATSPI): AT-SPI is not available, provide stubs */

G_DEFINE_TYPE (MsdA11yKeyboardAtspi, msd_a11y_keyboard_atspi, G_TYPE_OBJECT)

static void
msd_a11y_keyboard_atspi_class_init (MsdA11yKeyboardAtspiClass *klass G_GNUC_UNUSED)
{
}

static void
msd_a11y_keyboard_atspi_init (MsdA11yKeyboardAtspi *self G_GNUC_UNUSED)
{
}

void
msd_a11y_keyboard_atspi_start (MsdA11yKeyboardAtspi *self G_GNUC_UNUSED)
{
}

void
msd_a11y_keyboard_atspi_stop (MsdA11yKeyboardAtspi *self G_GNUC_UNUSED)
{
}

#endif /* ! defined(HAVE_LIBATSPI) */

MsdA11yKeyboardAtspi *
msd_a11y_keyboard_atspi_new ()
{
        return g_object_new (MSD_TYPE_A11Y_KEYBOARD_ATSPI, NULL);
}
