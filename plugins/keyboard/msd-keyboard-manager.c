/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright © 2001 Ximian, Inc.
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

#include "config.h"

#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include <locale.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>

#ifdef HAVE_X11_EXTENSIONS_XKB_H
#include <X11/XKBlib.h>
#include <X11/keysym.h>
#endif

#include "mate-settings-profile.h"
#include "msd-keyboard-manager.h"

#include "msd-keyboard-xkb.h"

#define MSD_KEYBOARD_SCHEMA "org.mate.peripherals-keyboard"

#define KEY_REPEAT         "repeat"
#define KEY_CLICK          "click"
#define KEY_RATE           "rate"
#define KEY_DELAY          "delay"
#define KEY_CLICK_VOLUME   "click-volume"

#define KEY_BELL_PITCH     "bell-pitch"
#define KEY_BELL_DURATION  "bell-duration"
#define KEY_BELL_MODE      "bell-mode"

#define KEY_NUMLOCK_STATE    "numlock-state"
#define KEY_NUMLOCK_REMEMBER "remember-numlock-state"

struct MsdKeyboardManagerPrivate {
	gboolean    have_xkb;
	gint        xkb_event_base;
	GSettings  *settings;
};

static void     msd_keyboard_manager_finalize    (GObject *object);

G_DEFINE_TYPE_WITH_PRIVATE (MsdKeyboardManager, msd_keyboard_manager, G_TYPE_OBJECT)

static gpointer manager_object = NULL;


#ifdef HAVE_X11_EXTENSIONS_XKB_H
static gboolean xkb_set_keyboard_autorepeat_rate(int delay, int rate)
{
	int interval = (rate <= 0) ? 1000000 : 1000/rate;

	if (delay <= 0)
	{
		delay = 1;
	}

	return XkbSetAutoRepeatRate(GDK_DISPLAY_XDISPLAY(gdk_display_get_default()), XkbUseCoreKbd, delay, interval);
}
#endif

#ifdef HAVE_X11_EXTENSIONS_XKB_H

typedef enum {
        NUMLOCK_STATE_OFF = 0,
        NUMLOCK_STATE_ON = 1,
        NUMLOCK_STATE_UNKNOWN = 2
} NumLockState;

static void
numlock_xkb_init (MsdKeyboardManager *manager)
{
        Display *dpy = GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());
        gboolean have_xkb;
        int opcode, error_base, major, minor;

        have_xkb = XkbQueryExtension (dpy,
                                      &opcode,
                                      &manager->priv->xkb_event_base,
                                      &error_base,
                                      &major,
                                      &minor)
                && XkbUseExtension (dpy, &major, &minor);

        if (have_xkb) {
                XkbSelectEventDetails (dpy,
                                       XkbUseCoreKbd,
                                       XkbStateNotifyMask,
                                       XkbModifierLockMask,
                                       XkbModifierLockMask);
        } else {
                g_warning ("XKB extension not available");
        }

        manager->priv->have_xkb = have_xkb;
}

static unsigned
numlock_NumLock_modifier_mask (void)
{
        Display *dpy = GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());
        return XkbKeysymToModifiers (dpy, XK_Num_Lock);
}

static void
numlock_set_xkb_state (NumLockState new_state)
{
        unsigned int num_mask;
        Display *dpy = GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());
        if (new_state != NUMLOCK_STATE_ON && new_state != NUMLOCK_STATE_OFF)
                return;
        num_mask = numlock_NumLock_modifier_mask ();
        XkbLockModifiers (dpy, XkbUseCoreKbd, num_mask, new_state ? num_mask : 0);
}

static NumLockState
numlock_get_settings_state (GSettings *settings)
{
        int          curr_state;
        curr_state = g_settings_get_enum (settings, KEY_NUMLOCK_STATE);
        return curr_state;
}

static void numlock_set_settings_state(GSettings *settings, NumLockState new_state)
{
        if (new_state != NUMLOCK_STATE_ON && new_state != NUMLOCK_STATE_OFF) {
                return;
        }
        g_settings_set_enum (settings, KEY_NUMLOCK_STATE, new_state);
}

static GdkFilterReturn
numlock_xkb_callback (GdkXEvent *xev_,
                      GdkEvent *gdkev_,
                      gpointer xkb_event_code)
{
        XEvent *xev = (XEvent *) xev_;

        if (xev->type == GPOINTER_TO_INT (xkb_event_code)) {
                XkbEvent *xkbev = (XkbEvent *)xev;
                if (xkbev->any.xkb_type == XkbStateNotify)
                if (xkbev->state.changed & XkbModifierLockMask) {
                        unsigned num_mask = numlock_NumLock_modifier_mask ();
                        unsigned locked_mods = xkbev->state.locked_mods;
                        int numlock_state = !! (num_mask & locked_mods);
                        GSettings *settings = g_settings_new (MSD_KEYBOARD_SCHEMA);
                        numlock_set_settings_state (settings, numlock_state);
                        g_object_unref (settings);
                }
        }
        return GDK_FILTER_CONTINUE;
}

static void
numlock_install_xkb_callback (MsdKeyboardManager *manager)
{
        if (!manager->priv->have_xkb)
                return;

        gdk_window_add_filter (NULL,
                               numlock_xkb_callback,
                               GINT_TO_POINTER (manager->priv->xkb_event_base));
}

#endif /* HAVE_X11_EXTENSIONS_XKB_H */

static void
apply_settings (GSettings          *settings,
                gchar              *key,
                MsdKeyboardManager *manager)
{
        XKeyboardControl kbdcontrol;
        gboolean         repeat;
        gboolean         click;
        int              rate;
        int              delay;
        int              click_volume;
        int              bell_volume;
        int              bell_pitch;
        int              bell_duration;
        char            *volume_string;
        GdkDisplay      *display;
#ifdef HAVE_X11_EXTENSIONS_XKB_H
        gboolean         rnumlock;
#endif /* HAVE_X11_EXTENSIONS_XKB_H */

        repeat        = g_settings_get_boolean  (settings, KEY_REPEAT);
        click         = g_settings_get_boolean  (settings, KEY_CLICK);
        rate          = g_settings_get_int   (settings, KEY_RATE);
        delay         = g_settings_get_int   (settings, KEY_DELAY);
        click_volume  = g_settings_get_int   (settings, KEY_CLICK_VOLUME);
        bell_pitch    = g_settings_get_int   (settings, KEY_BELL_PITCH);
        bell_duration = g_settings_get_int   (settings, KEY_BELL_DURATION);

        volume_string = g_settings_get_string (settings, KEY_BELL_MODE);
        bell_volume   = (volume_string && !strcmp (volume_string, "on")) ? 50 : 0;
        g_free (volume_string);

        display = gdk_display_get_default ();
        gdk_x11_display_error_trap_push (display);
        if (repeat) {
                gboolean rate_set = FALSE;

                XAutoRepeatOn (GDK_DISPLAY_XDISPLAY (display));
                /* Use XKB in preference */
#ifdef HAVE_X11_EXTENSIONS_XKB_H
                rate_set = xkb_set_keyboard_autorepeat_rate (delay, rate);
#endif
                if (!rate_set)
                        g_warning ("Neither XKeyboard not Xfree86's keyboard extensions are available,\n"
                                   "no way to support keyboard autorepeat rate settings");
        } else {
                XAutoRepeatOff (GDK_DISPLAY_XDISPLAY (display));
        }

        /* as percentage from 0..100 inclusive */
        if (click_volume < 0) {
                click_volume = 0;
        } else if (click_volume > 100) {
                click_volume = 100;
        }
        kbdcontrol.key_click_percent = click ? click_volume : 0;
        kbdcontrol.bell_percent = bell_volume;
        kbdcontrol.bell_pitch = bell_pitch;
        kbdcontrol.bell_duration = bell_duration;
        XChangeKeyboardControl (GDK_DISPLAY_XDISPLAY(gdk_display_get_default()),
                                KBKeyClickPercent | KBBellPercent | KBBellPitch | KBBellDuration,
                                &kbdcontrol);

#ifdef HAVE_X11_EXTENSIONS_XKB_H
        rnumlock = g_settings_get_boolean (settings, KEY_NUMLOCK_REMEMBER);

        if (rnumlock == 0 || key == NULL) {
                if (manager->priv->have_xkb && rnumlock) {
                        numlock_set_xkb_state (numlock_get_settings_state (settings));
                }
        }
#endif /* HAVE_X11_EXTENSIONS_XKB_H */

        XSync (GDK_DISPLAY_XDISPLAY (display), FALSE);
        gdk_x11_display_error_trap_pop_ignored (display);
}

void
msd_keyboard_manager_apply_settings (MsdKeyboardManager *manager)
{
        apply_settings (manager->priv->settings, NULL, manager);
}

static gboolean
start_keyboard_idle_cb (MsdKeyboardManager *manager)
{
        mate_settings_profile_start (NULL);

        g_debug ("Starting keyboard manager");

        manager->priv->have_xkb = 0;
        manager->priv->settings = g_settings_new (MSD_KEYBOARD_SCHEMA);

        /* Essential - xkb initialization should happen before */
        msd_keyboard_xkb_init (manager);

#ifdef HAVE_X11_EXTENSIONS_XKB_H
        numlock_xkb_init (manager);
#endif /* HAVE_X11_EXTENSIONS_XKB_H */

        /* apply current settings before we install the callback */
        msd_keyboard_manager_apply_settings (manager);

        g_signal_connect (manager->priv->settings, "changed", G_CALLBACK (apply_settings), manager);

#ifdef HAVE_X11_EXTENSIONS_XKB_H
        numlock_install_xkb_callback (manager);
#endif /* HAVE_X11_EXTENSIONS_XKB_H */

        mate_settings_profile_end (NULL);

        return FALSE;
}

gboolean
msd_keyboard_manager_start (MsdKeyboardManager *manager,
                            GError            **error)
{
        mate_settings_profile_start (NULL);

        g_idle_add ((GSourceFunc) start_keyboard_idle_cb, manager);

        mate_settings_profile_end (NULL);

        return TRUE;
}

void
msd_keyboard_manager_stop (MsdKeyboardManager *manager)
{
        MsdKeyboardManagerPrivate *p = manager->priv;

        g_debug ("Stopping keyboard manager");

        if (p->settings != NULL) {
                g_object_unref (p->settings);
                p->settings = NULL;
        }

#if HAVE_X11_EXTENSIONS_XKB_H
        if (p->have_xkb) {
                gdk_window_remove_filter (NULL,
                                          numlock_xkb_callback,
                                          GINT_TO_POINTER (p->xkb_event_base));
        }
#endif /* HAVE_X11_EXTENSIONS_XKB_H */

        msd_keyboard_xkb_shutdown ();
}

static void
msd_keyboard_manager_class_init (MsdKeyboardManagerClass *klass)
{
        GObjectClass   *object_class = G_OBJECT_CLASS (klass);

        object_class->finalize = msd_keyboard_manager_finalize;
}

static void
msd_keyboard_manager_init (MsdKeyboardManager *manager)
{
        manager->priv = msd_keyboard_manager_get_instance_private (manager);
}

static void
msd_keyboard_manager_finalize (GObject *object)
{
        MsdKeyboardManager *keyboard_manager;

        g_return_if_fail (object != NULL);
        g_return_if_fail (MSD_IS_KEYBOARD_MANAGER (object));

        keyboard_manager = MSD_KEYBOARD_MANAGER (object);

        g_return_if_fail (keyboard_manager->priv != NULL);

        G_OBJECT_CLASS (msd_keyboard_manager_parent_class)->finalize (object);
}

MsdKeyboardManager *
msd_keyboard_manager_new (void)
{
        if (manager_object != NULL) {
                g_object_ref (manager_object);
        } else {
                manager_object = g_object_new (MSD_TYPE_KEYBOARD_MANAGER, NULL);
                g_object_add_weak_pointer (manager_object,
                                           (gpointer *) &manager_object);
        }

        return MSD_KEYBOARD_MANAGER (manager_object);
}
