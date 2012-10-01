/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* acme-volume.c

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

#include "config.h"
#include "gvc-gstreamer-acme-vol.h"

#include <gst/gst.h>
#include <gst/audio/mixerutils.h>
#include <gst/interfaces/mixer.h>
#include <gst/interfaces/propertyprobe.h>

#include <gio/gio.h>

#include <string.h>

#define TIMEOUT	4

#define MATE_SOUND_SCHEMA          "org.mate.sound"
#define DEFAULT_MIXER_DEVICE_KEY   "default-mixer-device"
#define DEFAULT_MIXER_TRACKS_KEY   "default-mixer-tracks"

#define ACME_VOLUME_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), ACME_TYPE_VOLUME, AcmeVolumePrivate))

struct AcmeVolumePrivate {
	GstMixer     *mixer;
	GList        *mixer_tracks;
	guint         timer_id;
	gdouble       volume;
	gboolean      mute;
	GSettings    *settings;
};

G_DEFINE_TYPE (AcmeVolume, acme_volume, G_TYPE_OBJECT)

static gboolean acme_volume_open  (AcmeVolume *acme);
static void     acme_volume_close (AcmeVolume *acme);
static gboolean acme_volume_close_real (AcmeVolume *self);

static gpointer acme_volume_object = NULL;

static void
acme_volume_finalize (GObject *object)
{
	AcmeVolume *self;

	g_return_if_fail (object != NULL);
	g_return_if_fail (ACME_IS_VOLUME (object));

	self = ACME_VOLUME (object);

	if (self->_priv->timer_id != 0)
		g_source_remove (self->_priv->timer_id);
	acme_volume_close_real (self);

	if (self->_priv->settings != NULL) {
		g_object_unref (self->_priv->settings);
		self->_priv->settings = NULL;
	}

	G_OBJECT_CLASS (acme_volume_parent_class)->finalize (object);
}

void
acme_volume_set_mute (AcmeVolume *self, gboolean val)
{
	GList *t;

	g_return_if_fail(ACME_IS_VOLUME(self));
	g_return_if_fail(acme_volume_open(self));

	for (t = self->_priv->mixer_tracks; t != NULL; t = t->next) {
		GstMixerTrack *track = GST_MIXER_TRACK (t->data);
		gst_mixer_set_mute (self->_priv->mixer, track, val);
	}
	self->_priv->mute = val;
	acme_volume_close (self);
}

static void
update_state (AcmeVolume * self)
{
	gint *volumes, n;
	gdouble vol = 0;
	GstMixerTrack *track = GST_MIXER_TRACK (self->_priv->mixer_tracks->data);

	/* update mixer by getting volume */
	volumes = g_new0 (gint, track->num_channels);
	gst_mixer_get_volume (self->_priv->mixer, track, volumes);
	for (n = 0; n < track->num_channels; n++)
		vol += volumes[n];
	g_free (volumes);
	vol /= track->num_channels;
	vol = 100 * vol / (track->max_volume - track->min_volume);

	/* update mute flag, and volume if not muted */
	if (GST_MIXER_TRACK_HAS_FLAG (track, GST_MIXER_TRACK_MUTE))
		self->_priv->mute = TRUE;
	self->_priv->volume = vol;
}

gboolean
acme_volume_get_mute (AcmeVolume *self)
{
	g_return_val_if_fail(acme_volume_open(self), FALSE);

	update_state (self);
	acme_volume_close (self);

	return self->_priv->mute;
}

gint
acme_volume_get_volume (AcmeVolume *self)
{

	g_return_val_if_fail(acme_volume_open(self), 0);

	update_state (self);

	acme_volume_close (self);
	
	return (gint) (self->_priv->volume + 0.5);
}

void
acme_volume_set_volume (AcmeVolume *self, gint val)
{
	GList *t;

	g_return_if_fail(acme_volume_open(self));

	val = CLAMP (val, 0, 100);

	for (t = self->_priv->mixer_tracks; t != NULL; t = t->next) {
		GstMixerTrack *track = GST_MIXER_TRACK (t->data);
		gint *volumes, n;
		gdouble scale = (track->max_volume - track->min_volume) / 100.0;
		gint vol = (gint) (val * scale + track->min_volume + 0.5);

		volumes = g_new (gint, track->num_channels);
		for (n = 0; n < track->num_channels; n++)
			volumes[n] = vol;
		gst_mixer_set_volume (self->_priv->mixer, track, volumes);
		g_free (volumes);
	}

	/* update state */
	self->_priv->volume = val;

	acme_volume_close (self);
}

void
acme_volume_mute_toggle (AcmeVolume *self)
{
	gboolean muted;

	g_return_if_fail (self != NULL);
	g_return_if_fail (ACME_IS_VOLUME(self));

	muted = acme_volume_get_mute(self);
	acme_volume_set_mute(self, !muted);
}

gint
acme_volume_get_threshold (AcmeVolume *self)
{
	GList *t;
	gint steps = 101;

	g_return_val_if_fail(acme_volume_open(self), 1);

	for (t = self->_priv->mixer_tracks; t != NULL; t = t->next) {
		GstMixerTrack *track = GST_MIXER_TRACK (t->data);
		gint track_steps = track->max_volume - track->min_volume;
		if (track_steps > 0 && track_steps < steps)
			steps = track_steps;
	}

	acme_volume_close (self);

	return 100 / steps + 1;
}

static gboolean
acme_volume_close_real (AcmeVolume *self)
{
	if (self->_priv->mixer != NULL)
	{
		gst_element_set_state (GST_ELEMENT (self->_priv->mixer), GST_STATE_NULL);
		gst_object_unref (GST_OBJECT (self->_priv->mixer));
		g_list_foreach (self->_priv->mixer_tracks, (GFunc) g_object_unref, NULL);
		g_list_free (self->_priv->mixer_tracks);
		self->_priv->mixer = NULL;
		self->_priv->mixer_tracks = NULL;
	}

	self->_priv->timer_id = 0;
	return FALSE;
}

/*
 * _acme_set_mixer
 * @mixer  A pointer to mixer element
 * @data   A pointer to user data (AcmeVolume instance to be modified)
 * @return A gboolean indicating success if Master track was found, failed otherwises.
 */
static gboolean
_acme_set_mixer(GstMixer *mixer, gpointer user_data)
{
	const GList *tracks;

	for (tracks = gst_mixer_list_tracks (mixer); tracks != NULL; tracks = tracks->next) {
		GstMixerTrack *track = GST_MIXER_TRACK (tracks->data);

		if (GST_MIXER_TRACK_HAS_FLAG (track, GST_MIXER_TRACK_MASTER)) {
			AcmeVolume *self;

			self = ACME_VOLUME (user_data);

			self->_priv->mixer = mixer;
			self->_priv->mixer_tracks = g_list_append (self->_priv->mixer_tracks, g_object_ref (track));
			return TRUE;
		}

		continue;
	}

	return FALSE;
}

/* This is a modified version of code from gnome-media's gst-mixer */
static gboolean
acme_volume_open (AcmeVolume *self)
{
	gchar *mixer_device, **factory_and_device = NULL;
	GList *mixer_list;

	if (self->_priv->timer_id != 0) {
		g_source_remove (self->_priv->timer_id);
		self->_priv->timer_id = 0;
		return TRUE;
	}

	mixer_device = g_settings_get_string (self->_priv->settings, DEFAULT_MIXER_DEVICE_KEY);
	if (mixer_device != NULL)
		factory_and_device = g_strsplit (mixer_device, ":", 2);

	if (factory_and_device != NULL && factory_and_device[0] != NULL) {
		GstElement *element;

		element = gst_element_factory_make (factory_and_device[0], NULL);

		if (element != NULL) {
			if (factory_and_device[1] != NULL &&
			    g_object_class_find_property (G_OBJECT_GET_CLASS (element), "device"))
				g_object_set (G_OBJECT (element), "device", factory_and_device[1], NULL);
			gst_element_set_state (element, GST_STATE_READY);

			if (GST_IS_MIXER (element))
				self->_priv->mixer = GST_MIXER (element);
			else {
				gst_element_set_state (element, GST_STATE_NULL);
				gst_object_unref (element);
			}
		}
	}

	g_free (mixer_device);
	g_strfreev (factory_and_device);

	if (self->_priv->mixer != NULL) {
		const GList *m;
		GSList *tracks, *t;

		/* Try to use tracks saved in GSettings 
		   Note: errors need to be treated , for example if the user set a non type list for this key
		   or if the elements type_list are not "matched" */
		gchar **settings_list;
		settings_list = g_settings_get_strv (self->_priv->settings, DEFAULT_MIXER_TRACKS_KEY);
		if (settings_list != NULL) {
			gint i;
			for (i = 0; i < G_N_ELEMENTS (settings_list); i++) {
				if (settings_list[i] != NULL)
					tracks = g_slist_append (tracks, g_strdup (settings_list[i]));
			}
			g_strfreev (settings_list);
		}
		
		/* We use these tracks ONLY if they are supported on the system with the following mixer */
		for (m = gst_mixer_list_tracks (self->_priv->mixer); m != NULL; m = m->next) {
			GstMixerTrack *track = GST_MIXER_TRACK (m->data);

			for (t = tracks; t != NULL; t = t->next)
				if (!strcmp (t->data, track->label))
					self->_priv->mixer_tracks = g_list_append (self->_priv->mixer_tracks, g_object_ref (track));

		}

		g_slist_foreach (tracks, (GFunc)g_free, NULL);
		g_slist_free (tracks);

		/* If no track stored in GSettings is avaiable try to use Master track */
		if (self->_priv->mixer_tracks == NULL) {
			for (m = gst_mixer_list_tracks (self->_priv->mixer); m != NULL; m = m->next) {
				GstMixerTrack *track = GST_MIXER_TRACK (m->data);

				if (GST_MIXER_TRACK_HAS_FLAG (track, GST_MIXER_TRACK_MASTER)) {
					self->_priv->mixer_tracks = g_list_append (self->_priv->mixer_tracks, g_object_ref (track));
					break;
				}
			}
		}

		if (self->_priv->mixer_tracks != NULL)
			return TRUE;
		else {
			gst_element_set_state (GST_ELEMENT (self->_priv->mixer), GST_STATE_NULL);
			gst_object_unref (self->_priv->mixer);
		}
	}

	/* Go through all elements of a certain class and check whether
	 * they implement a mixer. If so, walk through the tracks and look
	 * for first one named "volume".
	 *
	 * We should probably do something intelligent if we don't find an
	 * appropriate mixer/track.  But now we do something stupid...
	 * everything just becomes a no-op.
	 */
	mixer_list = gst_audio_default_registry_mixer_filter (_acme_set_mixer,
			TRUE,
			self);

	if (mixer_list == NULL)
		return FALSE;

	/* do not unref the mixer as we keep the ref for self->priv->mixer */
	g_list_free (mixer_list);

	return TRUE;
}

static void
acme_volume_close (AcmeVolume *self)
{
	self->_priv->timer_id = g_timeout_add_seconds (TIMEOUT,
			(GSourceFunc) acme_volume_close_real, self);
}

static void
acme_volume_init (AcmeVolume *self)
{
	self->_priv = ACME_VOLUME_GET_PRIVATE (self);
	self->_priv->settings = g_settings_new (MATE_SOUND_SCHEMA);
}

static void
acme_volume_class_init (AcmeVolumeClass *klass)
{
	G_OBJECT_CLASS (klass)->finalize = acme_volume_finalize;

	gst_init (NULL, NULL);

	g_type_class_add_private (klass, sizeof (AcmeVolumePrivate));
}

/* acme_volume_new
 * @return A singleton instance of type AcmeVolume
 */
AcmeVolume *
acme_volume_new (void)
{
	if (acme_volume_object == NULL) {
		acme_volume_object = g_object_new (ACME_TYPE_VOLUME, NULL);
		return ACME_VOLUME(acme_volume_object);
	}
	g_object_ref(acme_volume_object);
	return ACME_VOLUME(acme_volume_object);
}
