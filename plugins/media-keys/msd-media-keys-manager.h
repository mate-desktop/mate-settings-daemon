/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007 William Jon McCann <mccann@jhu.edu>
 * Copyright (C) 2014 Michal Ratajsky <michal.ratajsky@gmail.com>
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

#ifndef __MSD_MEDIA_KEYS_MANAGER_H
#define __MSD_MEDIA_KEYS_MANAGER_H

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define MSD_TYPE_MEDIA_KEYS_MANAGER         (msd_media_keys_manager_get_type ())
#define MSD_MEDIA_KEYS_MANAGER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), MSD_TYPE_MEDIA_KEYS_MANAGER, MsdMediaKeysManager))
#define MSD_MEDIA_KEYS_MANAGER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), MSD_TYPE_MEDIA_KEYS_MANAGER, MsdMediaKeysManagerClass))
#define MSD_IS_MEDIA_KEYS_MANAGER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), MSD_TYPE_MEDIA_KEYS_MANAGER))
#define MSD_IS_MEDIA_KEYS_MANAGER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), MSD_TYPE_MEDIA_KEYS_MANAGER))
#define MSD_MEDIA_KEYS_MANAGER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), MSD_TYPE_MEDIA_KEYS_MANAGER, MsdMediaKeysManagerClass))

typedef struct _MsdMediaKeysManager         MsdMediaKeysManager;
typedef struct _MsdMediaKeysManagerClass    MsdMediaKeysManagerClass;
typedef struct _MsdMediaKeysManagerPrivate  MsdMediaKeysManagerPrivate;

struct _MsdMediaKeysManager
{
        GObject                     parent;
        MsdMediaKeysManagerPrivate *priv;
};

struct _MsdMediaKeysManagerClass
{
        GObjectClass   parent_class;
        void          (* media_player_key_pressed) (MsdMediaKeysManager *manager,
                                                    const char          *application,
                                                    const char          *key);
};

GType                 msd_media_keys_manager_get_type                  (void);

MsdMediaKeysManager * msd_media_keys_manager_new                       (void);
gboolean              msd_media_keys_manager_start                     (MsdMediaKeysManager *manager,
                                                                        GError             **error);
void                  msd_media_keys_manager_stop                      (MsdMediaKeysManager *manager);

gboolean              msd_media_keys_manager_grab_media_player_keys    (MsdMediaKeysManager *manager,
                                                                        const char          *application,
                                                                        guint32              time,
                                                                        GError             **error);
gboolean              msd_media_keys_manager_release_media_player_keys (MsdMediaKeysManager *manager,
                                                                        const char          *application,
                                                                        GError             **error);

G_END_DECLS

#endif /* __MSD_MEDIA_KEYS_MANAGER_H */
