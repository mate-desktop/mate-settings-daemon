/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2001 Bastien Nocera <hadess@hadess.net>
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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301,
 * USA.
 */

#ifndef __ACME_H__
#define __ACME_H__

#include "msd-keygrab.h"

#define BINDING_SCHEMA "org.mate.SettingsDaemon.plugins.media-keys"

enum {
        TOUCHPAD_KEY,
        TOUCHPAD_ON_KEY,
        TOUCHPAD_OFF_KEY,
        MUTE_KEY,
        VOLUME_DOWN_KEY,
        VOLUME_UP_KEY,
        MUTE_QUIET_KEY,
        VOLUME_DOWN_QUIET_KEY,
        VOLUME_UP_QUIET_KEY,
        MIC_MUTE_KEY,
        POWER_KEY,
        EJECT_KEY,
        HOME_KEY,
        MEDIA_KEY,
        CALCULATOR_KEY,
        MESSENGER_KEY,
        SEARCH_KEY,
        EMAIL_KEY,
        CONTROL_CENTER_KEY,
        SCREENSAVER_KEY,
        HELP_KEY,
        WWW_KEY,
        PLAY_KEY,
        PAUSE_KEY,
        STOP_KEY,
        PREVIOUS_KEY,
        NEXT_KEY,
        REWIND_KEY,
        FORWARD_KEY,
        REPEAT_KEY,
        RANDOM_KEY,
        MAGNIFIER_KEY,
        SCREENREADER_KEY,
        ON_SCREEN_KEYBOARD_KEY,
        LOGOUT_KEY,
        RFKILL_KEY,
        BLUETOOTH_RFKILL_KEY,
        DISPLAY_KEY,
        HANDLED_KEYS,
};

static struct {
        int key_type;
        const char *settings_key;
        const char *hard_coded;
        Key *key;
} keys[HANDLED_KEYS] = {
        { TOUCHPAD_KEY, "touchpad", NULL, NULL },
        { TOUCHPAD_ON_KEY, NULL, "XF86TouchpadOn", NULL },
        { TOUCHPAD_OFF_KEY, NULL, "XF86TouchpadOff", NULL },
        { MUTE_KEY, "volume-mute", NULL, NULL },
        { VOLUME_DOWN_KEY, "volume-down", NULL, NULL },
        { VOLUME_UP_KEY, "volume-up", NULL, NULL },
        { MUTE_QUIET_KEY, "volume-mute-quiet", NULL, NULL },
        { VOLUME_DOWN_QUIET_KEY, "volume-down-quiet", NULL, NULL },
        { VOLUME_UP_QUIET_KEY, "volume-up-quiet", NULL, NULL },
        { MIC_MUTE_KEY, "mic-mute", NULL, NULL },
        { POWER_KEY, "power", NULL, NULL },
        { EJECT_KEY, "eject", NULL, NULL },
        { HOME_KEY, "home", NULL, NULL },
        { MEDIA_KEY, "media", NULL, NULL },
        { CALCULATOR_KEY, "calculator", NULL, NULL },
        { MESSENGER_KEY, "messenger", NULL, NULL },
        { SEARCH_KEY, "search", NULL, NULL },
        { EMAIL_KEY, "email", NULL, NULL },
        { CONTROL_CENTER_KEY, "control-center", NULL, NULL },
        { SCREENSAVER_KEY, "screensaver", NULL, NULL },
        { HELP_KEY, "help", NULL, NULL },
        { WWW_KEY, "www", NULL, NULL },
        { PLAY_KEY, "play", NULL, NULL },
        { PAUSE_KEY, "pause", NULL, NULL },
        { STOP_KEY, "stop", NULL, NULL },
        { PREVIOUS_KEY, "previous", NULL, NULL },
        { NEXT_KEY, "next", NULL, NULL },
        /* Those are not configurable in the UI */
        { REWIND_KEY, NULL, "XF86AudioRewind", NULL },
        { FORWARD_KEY, NULL, "XF86AudioForward", NULL },
        { REPEAT_KEY, NULL, "XF86AudioRepeat", NULL },
        { RANDOM_KEY, NULL, "XF86AudioRandomPlay", NULL },
        { MAGNIFIER_KEY, "magnifier", NULL, NULL },
        { SCREENREADER_KEY, "screenreader", NULL, NULL },
        { ON_SCREEN_KEYBOARD_KEY, "on-screen-keyboard", NULL, NULL },
        { LOGOUT_KEY, "logout", NULL, NULL },
        { RFKILL_KEY, NULL, "XF86WLAN", NULL },
        { BLUETOOTH_RFKILL_KEY, NULL, "XF86Bluetooth", NULL },
        { DISPLAY_KEY, NULL, "XF86Display", NULL }
};

#endif /* __ACME_H__ */
