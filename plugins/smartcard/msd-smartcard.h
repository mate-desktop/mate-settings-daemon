/* securitycard.h - api for reading and writing data to a security card
 *
 * Copyright (C) 2006 Ray Strode
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
 */
#ifndef MSD_SMARTCARD_H
#define MSD_SMARTCARD_H

#include <glib.h>
#include <glib-object.h>

#include <secmod.h>

#ifdef __cplusplus
extern "C" {
#endif
#define MSD_TYPE_SMARTCARD            (msd_smartcard_get_type ())
#define MSD_SMARTCARD(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), MSD_TYPE_SMARTCARD, MsdSmartcard))
#define MSD_SMARTCARD_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), MSD_TYPE_SMARTCARD, MsdSmartcardClass))
#define MSD_IS_SMARTCARD(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MSD_TYPE_SMARTCARD))
#define MSD_IS_SMARTCARD_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), MSD_TYPE_SMARTCARD))
#define MSD_SMARTCARD_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), MSD_TYPE_SMARTCARD, MsdSmartcardClass))
#define MSD_SMARTCARD_ERROR           (msd_smartcard_error_quark ())
typedef struct _MsdSmartcardClass MsdSmartcardClass;
typedef struct _MsdSmartcard MsdSmartcard;
typedef struct _MsdSmartcardPrivate MsdSmartcardPrivate;
typedef enum _MsdSmartcardError MsdSmartcardError;
typedef enum _MsdSmartcardState MsdSmartcardState;

typedef struct _MsdSmartcardRequest MsdSmartcardRequest;

struct _MsdSmartcard {
    GObject parent;

    /*< private > */
    MsdSmartcardPrivate *priv;
};

struct _MsdSmartcardClass {
    GObjectClass parent_class;

    void (* inserted) (MsdSmartcard *card);
    void (* removed)  (MsdSmartcard *card);
};

enum _MsdSmartcardError {
    MSD_SMARTCARD_ERROR_GENERIC = 0,
};

enum _MsdSmartcardState {
    MSD_SMARTCARD_STATE_INSERTED = 0,
    MSD_SMARTCARD_STATE_REMOVED,
};

GType msd_smartcard_get_type (void) G_GNUC_CONST;
GQuark msd_smartcard_error_quark (void) G_GNUC_CONST;

CK_SLOT_ID msd_smartcard_get_slot_id (MsdSmartcard *card);
gint msd_smartcard_get_slot_series (MsdSmartcard *card);
MsdSmartcardState msd_smartcard_get_state (MsdSmartcard *card);

char *msd_smartcard_get_name (MsdSmartcard *card);
gboolean msd_smartcard_is_login_card (MsdSmartcard *card);

gboolean msd_smartcard_unlock (MsdSmartcard *card,
                               const char   *password);

/* don't under any circumstances call these functions */
#ifdef MSD_SMARTCARD_ENABLE_INTERNAL_API

MsdSmartcard *_msd_smartcard_new (SECMODModule *module,
                                  CK_SLOT_ID    slot_id,
                                  gint          slot_series);
MsdSmartcard *_msd_smartcard_new_from_name (SECMODModule *module,
                                            const char   *name);

void _msd_smartcard_set_state (MsdSmartcard      *card,
                               MsdSmartcardState  state);
#endif

#ifdef __cplusplus
}
#endif
#endif                                /* MSD_SMARTCARD_H */
