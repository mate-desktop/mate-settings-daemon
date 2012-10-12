/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007 David Zeuthen <david@fubar.dk>
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

#ifndef MSD_DATETIME_MECHANISM_H
#define MSD_DATETIME_MECHANISM_H

#include <glib-object.h>
#include <dbus/dbus-glib.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MSD_DATETIME_TYPE_MECHANISM         (msd_datetime_mechanism_get_type ())
#define MSD_DATETIME_MECHANISM(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), MSD_DATETIME_TYPE_MECHANISM, MsdDatetimeMechanism))
#define MSD_DATETIME_MECHANISM_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), MSD_DATETIME_TYPE_MECHANISM, MsdDatetimeMechanismClass))
#define MSD_DATETIME_IS_MECHANISM(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), MSD_DATETIME_TYPE_MECHANISM))
#define MSD_DATETIME_IS_MECHANISM_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), MSD_DATETIME_TYPE_MECHANISM))
#define MSD_DATETIME_MECHANISM_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), MSD_DATETIME_TYPE_MECHANISM, MsdDatetimeMechanismClass))

typedef struct MsdDatetimeMechanismPrivate MsdDatetimeMechanismPrivate;

typedef struct
{
        GObject        parent;
        MsdDatetimeMechanismPrivate *priv;
} MsdDatetimeMechanism;

typedef struct
{
        GObjectClass   parent_class;
} MsdDatetimeMechanismClass;

typedef enum
{
        MSD_DATETIME_MECHANISM_ERROR_GENERAL,
        MSD_DATETIME_MECHANISM_ERROR_NOT_PRIVILEGED,
        MSD_DATETIME_MECHANISM_ERROR_INVALID_TIMEZONE_FILE,
        MSD_DATETIME_MECHANISM_NUM_ERRORS
} MsdDatetimeMechanismError;

#define MSD_DATETIME_MECHANISM_ERROR msd_datetime_mechanism_error_quark ()

GType msd_datetime_mechanism_error_get_type (void);
#define MSD_DATETIME_MECHANISM_TYPE_ERROR (msd_datetime_mechanism_error_get_type ())


GQuark                     msd_datetime_mechanism_error_quark         (void);
GType                      msd_datetime_mechanism_get_type            (void);
MsdDatetimeMechanism      *msd_datetime_mechanism_new                 (void);

/* exported methods */
gboolean            msd_datetime_mechanism_get_timezone (MsdDatetimeMechanism   *mechanism,
                                                         DBusGMethodInvocation  *context);
gboolean            msd_datetime_mechanism_set_timezone (MsdDatetimeMechanism   *mechanism,
                                                         const char             *zone_file,
                                                         DBusGMethodInvocation  *context);

gboolean            msd_datetime_mechanism_can_set_timezone (MsdDatetimeMechanism  *mechanism,
                                                             DBusGMethodInvocation *context);

gboolean            msd_datetime_mechanism_set_time     (MsdDatetimeMechanism  *mechanism,
                                                         gint64                 seconds_since_epoch,
                                                         DBusGMethodInvocation *context);

gboolean            msd_datetime_mechanism_can_set_time (MsdDatetimeMechanism  *mechanism,
                                                         DBusGMethodInvocation *context);

gboolean            msd_datetime_mechanism_adjust_time  (MsdDatetimeMechanism  *mechanism,
                                                         gint64                 seconds_to_add,
                                                         DBusGMethodInvocation *context);

gboolean            msd_datetime_mechanism_get_hardware_clock_using_utc  (MsdDatetimeMechanism  *mechanism,
                                                                          DBusGMethodInvocation *context);

gboolean            msd_datetime_mechanism_set_hardware_clock_using_utc  (MsdDatetimeMechanism  *mechanism,
                                                                          gboolean               using_utc,
                                                                          DBusGMethodInvocation *context);

#ifdef __cplusplus
}
#endif

#endif /* MSD_DATETIME_MECHANISM_H */
