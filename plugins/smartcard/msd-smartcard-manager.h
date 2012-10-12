/* msd-smartcard-manager.h - object for monitoring smartcard insertion and
 *                           removal events
 *
 * Copyright (C) 2006, 2009 Red Hat, Inc.
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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 * Written by: Ray Strode
 */
#ifndef MSD_SMARTCARD_MANAGER_H
#define MSD_SMARTCARD_MANAGER_H

#define MSD_SMARTCARD_ENABLE_INTERNAL_API
#include "msd-smartcard.h"

#include <glib.h>
#include <glib-object.h>

#ifdef __cplusplus
extern "C" {
#endif
#define MSD_TYPE_SMARTCARD_MANAGER            (msd_smartcard_manager_get_type ())
#define MSD_SMARTCARD_MANAGER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), MSD_TYPE_SMARTCARD_MANAGER, MsdSmartcardManager))
#define MSD_SMARTCARD_MANAGER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), MSD_TYPE_SMARTCARD_MANAGER, MsdSmartcardManagerClass))
#define MSD_IS_SMARTCARD_MANAGER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SC_TYPE_SMARTCARD_MANAGER))
#define MSD_IS_SMARTCARD_MANAGER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), SC_TYPE_SMARTCARD_MANAGER))
#define MSD_SMARTCARD_MANAGER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), MSD_TYPE_SMARTCARD_MANAGER, MsdSmartcardManagerClass))
#define MSD_SMARTCARD_MANAGER_ERROR           (msd_smartcard_manager_error_quark ())
typedef struct _MsdSmartcardManager MsdSmartcardManager;
typedef struct _MsdSmartcardManagerClass MsdSmartcardManagerClass;
typedef struct _MsdSmartcardManagerPrivate MsdSmartcardManagerPrivate;
typedef enum _MsdSmartcardManagerError MsdSmartcardManagerError;

struct _MsdSmartcardManager {
    GObject parent;

    /*< private > */
    MsdSmartcardManagerPrivate *priv;
};

struct _MsdSmartcardManagerClass {
        GObjectClass parent_class;

        /* Signals */
        void (*smartcard_inserted) (MsdSmartcardManager *manager,
                                    MsdSmartcard        *token);
        void (*smartcard_removed) (MsdSmartcardManager *manager,
                                   MsdSmartcard        *token);
        void (*error) (MsdSmartcardManager *manager,
                       GError              *error);
};

enum _MsdSmartcardManagerError {
    MSD_SMARTCARD_MANAGER_ERROR_GENERIC = 0,
    MSD_SMARTCARD_MANAGER_ERROR_WITH_NSS,
    MSD_SMARTCARD_MANAGER_ERROR_LOADING_DRIVER,
    MSD_SMARTCARD_MANAGER_ERROR_WATCHING_FOR_EVENTS,
    MSD_SMARTCARD_MANAGER_ERROR_REPORTING_EVENTS
};

GType msd_smartcard_manager_get_type (void) G_GNUC_CONST;
GQuark msd_smartcard_manager_error_quark (void) G_GNUC_CONST;

MsdSmartcardManager *msd_smartcard_manager_new (const char *module);

gboolean msd_smartcard_manager_start (MsdSmartcardManager  *manager,
                                      GError              **error);

void msd_smartcard_manager_stop (MsdSmartcardManager *manager);

char *msd_smartcard_manager_get_module_path (MsdSmartcardManager *manager);
gboolean msd_smartcard_manager_login_card_is_inserted (MsdSmartcardManager *manager);

#ifdef __cplusplus
}
#endif
#endif                                /* MSD_SMARTCARD_MANAGER_H */
