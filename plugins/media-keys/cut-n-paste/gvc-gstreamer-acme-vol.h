/* acme-volume.h

   Copyright (C) 2002, 2003 Bastien Nocera
   Copyright (C) 2004 Novell, Inc.
   Copyright (C) 2009 PERIER Romain <mrpouet@tuxfamily.org>
   Copyright (C) 2011 Stefano Karapetsas <stefano@karapetsas.com>

   The Mate Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Mate Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Mate Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Author: Bastien Nocera <hadess@hadess.net>
           Jon Trowbridge <trow@ximian.com>
 */

#include <glib-object.h>

#define ACME_TYPE_VOLUME                        (acme_volume_get_type ())
#define ACME_VOLUME(obj)		        (G_TYPE_CHECK_INSTANCE_CAST ((obj), ACME_TYPE_VOLUME, AcmeVolume))
#define ACME_VOLUME_CLASS(klass)	        (G_TYPE_CHECK_CLASS_CAST ((klass),  ACME_TYPE_VOLUME, AcmeVolumeClass))
#define ACME_IS_VOLUME(obj)	                (G_TYPE_CHECK_INSTANCE_TYPE ((obj), ACME_TYPE_VOLUME))
#define ACME_VOLUME_GET_CLASS(obj)	        (G_TYPE_INSTANCE_GET_CLASS ((obj), ACME_TYPE_VOLUME, AcmeVolumeClass))

typedef struct AcmeVolume AcmeVolume;
typedef struct AcmeVolumeClass AcmeVolumeClass;
typedef struct AcmeVolumePrivate AcmeVolumePrivate;

struct AcmeVolume {
	GObject parent;
	AcmeVolumePrivate *_priv;
};

struct AcmeVolumeClass {
	GObjectClass parent;
};

GType       acme_volume_get_type      (void);
AcmeVolume *acme_volume_new           (void);
void        acme_volume_set_mute      (AcmeVolume *self, gboolean val);
void        acme_volume_mute_toggle   (AcmeVolume *self);
gboolean    acme_volume_get_mute      (AcmeVolume *self);
void        acme_volume_set_volume    (AcmeVolume *self, gint val);
gint        acme_volume_get_volume    (AcmeVolume *self);
gint        acme_volume_get_threshold (AcmeVolume *self);

