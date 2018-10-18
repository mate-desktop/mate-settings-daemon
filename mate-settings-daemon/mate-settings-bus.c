/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2006-2011 Richard Hughes <richard@hughsie.com>
 *
 * Licensed under the GNU General Public License Version 2
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"

#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <glib.h>
#include <gio/gio.h>

#include "mate-settings-bus.h"

char *
mate_settings_get_chassis_type (void)
{
        char *ret = NULL;
        GError *error = NULL;
        GVariant *inner;
        GVariant *variant = NULL;
        GDBusConnection *connection;

        connection = g_bus_get_sync (G_BUS_TYPE_SYSTEM,
                                     NULL,
                                     &error);
        if (connection == NULL) {
                g_warning ("system bus not available: %s", error->message);
                g_error_free (error);
                goto out;
        }

        variant = g_dbus_connection_call_sync (connection,
                                               "org.freedesktop.hostname1",
                                               "/org/freedesktop/hostname1",
                                               "org.freedesktop.DBus.Properties",
                                               "Get",
                                               g_variant_new ("(ss)",
                                                              "org.freedesktop.hostname1",
                                                              "Chassis"),
                                               NULL,
                                               G_DBUS_CALL_FLAGS_NONE,
                                               -1,
                                               NULL,
                                               &error);
        if (variant == NULL) {
                g_debug ("Failed to get property '%s': %s", "Chassis", error->message);
                g_error_free (error);
                goto out;
        }

        g_variant_get (variant, "(v)", &inner);
        ret = g_variant_dup_string (inner, NULL);
        g_debug ("Get property '%s': %s", "Chassis", ret);
        g_variant_unref (inner);
out:
        g_clear_object (&connection);
        g_clear_pointer (&variant, g_variant_unref);
        return ret;
}
