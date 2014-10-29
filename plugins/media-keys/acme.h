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
        MUTE_KEY,
        VOLUME_DOWN_KEY,
        VOLUME_UP_KEY,
        POWER_KEY,
        EJECT_KEY,
        HOME_KEY,
        MEDIA_KEY,
        CALCULATOR_KEY,
        SEARCH_KEY,
        EMAIL_KEY,
        SCREENSAVER_KEY,
        HELP_KEY,
        WWW_KEY,
        PLAY_KEY,
        PAUSE_KEY,
        STOP_KEY,
        PREVIOUS_KEY,
        NEXT_KEY,
        LOGOUT_KEY,
        HANDLED_KEYS,
};

static struct {
        int key_type;
        const char *settings_key;
        Key *key;
} keys[HANDLED_KEYS] = {
        { TOUCHPAD_KEY, "touchpad", NULL },
        { MUTE_KEY, "volume-mute",NULL },
        { VOLUME_DOWN_KEY, "volume-down", NULL },
        { VOLUME_UP_KEY, "volume-up", NULL },
        { POWER_KEY, "power", NULL },
        { EJECT_KEY, "eject", NULL },
        { HOME_KEY, "home", NULL },
        { MEDIA_KEY, "media", NULL },
        { CALCULATOR_KEY, "calculator", NULL },
        { SEARCH_KEY, "search", NULL },
        { EMAIL_KEY, "email", NULL },
        { SCREENSAVER_KEY, "screensaver", NULL },
        { HELP_KEY, "help", NULL },
        { WWW_KEY, "www", NULL },
        { PLAY_KEY, "play", NULL },
        { PAUSE_KEY, "pause", NULL },
        { STOP_KEY, "stop", NULL },
        { PREVIOUS_KEY, "previous", NULL },
        { NEXT_KEY, "next", NULL },
        { LOGOUT_KEY, "logout", NULL },
};

#endif /* __ACME_H__ */
