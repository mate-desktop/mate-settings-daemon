/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2026 MATE Developers
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

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

#include <glib.h>
#include <glib-unix.h>
#include <gio/gio.h>
#include <wayland-client.h>

#include "ext-data-control-v1-client.h"
#include "msd-clipboard-manager-wayland.h"

typedef enum
{
        MSD_WAYLAND_SELECTION_CLIPBOARD,
        MSD_WAYLAND_SELECTION_PRIMARY
} SelectionKind;

typedef struct
{
        gchar      *mime;
        GByteArray *data;
} ClipEntry;

typedef struct _MsdClipboardManagerWayland MsdClipboardManagerWayland;

typedef struct
{
        MsdClipboardManagerWayland *wl;
        SelectionKind               kind;
        gint                        fd;
        guint                       source_id;
        gchar                      *mime;
        GByteArray                 *data;
        gboolean                    done;
} ClipReceive;

typedef struct
{
        MsdClipboardManagerWayland           *wl;
        SelectionKind                         kind;
        struct ext_data_control_offer_v1     *offer;
        GPtrArray                            *cache;
        GPtrArray                            *receives;
        struct ext_data_control_source_v1    *source;
        gboolean                              reoffer_pending;
} SelectionState;

struct _MsdClipboardManagerWayland
{
        struct wl_display                   *display;
        struct wl_registry                  *registry;
        struct ext_data_control_manager_v1  *dc_manager;
        struct wl_seat                      *seat;
        struct ext_data_control_device_v1   *device;
        guint                                display_source;

        struct ext_data_control_offer_v1    *pending_offer;
        GPtrArray                           *pending_mimes;

        SelectionState                       clipboard;
        SelectionState                       primary;
};

static void    selection_state_clear      (SelectionState *state);

static void
clip_entry_free (gpointer data)
{
        ClipEntry *entry = data;

        if (entry == NULL)
                return;

        g_free (entry->mime);
        if (entry->data != NULL)
                g_byte_array_free (entry->data, TRUE);
        g_free (entry);
}

static SelectionState *
state_from_kind (MsdClipboardManagerWayland *wl,
                 SelectionKind               kind)
{
        if (kind == MSD_WAYLAND_SELECTION_PRIMARY)
                return &wl->primary;

        return &wl->clipboard;
}

static void
start_reoffer (SelectionState *state);

static void
finalize_cache (SelectionState *state)
{
        GPtrArray *new_cache;
        guint      i;

        if (state->receives == NULL)
                return;

        new_cache = g_ptr_array_new_with_free_func (clip_entry_free);

        for (i = 0; i < state->receives->len; i++) {
                ClipReceive *recv;
                ClipEntry   *entry;

                recv = state->receives->pdata[i];

                entry = g_new0 (ClipEntry, 1);
                entry->mime = recv->mime;
                entry->data = recv->data;
                recv->mime = NULL;
                recv->data = NULL;

                g_ptr_array_add (new_cache, entry);
        }

        if (state->cache != NULL)
                g_ptr_array_free (state->cache, TRUE);
        state->cache = new_cache;

        for (i = 0; i < state->receives->len; i++) {
                ClipReceive *recv;

                recv = state->receives->pdata[i];
                if (recv->fd != -1)
                        close (recv->fd);
                g_free (recv);
        }
        g_ptr_array_free (state->receives, FALSE);
        state->receives = NULL;
}

static void
receive_finish (ClipReceive *recv)
{
        SelectionState *state;
        guint           i;

        if (recv->done)
                return;
        recv->done = TRUE;

        if (recv->fd != -1) {
                close (recv->fd);
                recv->fd = -1;
        }

        state = state_from_kind (recv->wl, recv->kind);

        if (state->receives == NULL)
                return;

        for (i = 0; i < state->receives->len; i++) {
                ClipReceive *other;

                other = state->receives->pdata[i];
                if (!other->done)
                        return;
        }

        finalize_cache (state);

        if (state->reoffer_pending) {
                state->reoffer_pending = FALSE;
                start_reoffer (state);
        }
}

static void
receive_cancel (ClipReceive *recv)
{
        if (recv->source_id != 0)
                g_source_remove (recv->source_id);
        if (recv->fd != -1)
                close (recv->fd);
        g_free (recv->mime);
        if (recv->data != NULL)
                g_byte_array_free (recv->data, TRUE);
        g_free (recv);
}

static gboolean
receive_dispatch_cb (gint         fd,
                     GIOCondition condition,
                     gpointer     user_data)
{
        ClipReceive *recv = user_data;
        guchar       buf[4096];
        gssize       n;

        n = read (fd, buf, sizeof (buf));
        if (n > 0) {
                g_byte_array_append (recv->data, buf, n);
                return G_SOURCE_CONTINUE;
        }
        if (n == 0) {
                receive_finish (recv);
                return G_SOURCE_REMOVE;
        }
        if (n < 0 && errno == EINTR)
                return G_SOURCE_CONTINUE;
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                if (condition & (G_IO_HUP | G_IO_ERR)) {
                        receive_finish (recv);
                        return G_SOURCE_REMOVE;
                }
                return G_SOURCE_CONTINUE;
        }

        receive_finish (recv);
        return G_SOURCE_REMOVE;
}

static void
start_receives (SelectionState                          *state,
                struct ext_data_control_offer_v1       *offer,
                GPtrArray                              *mimes)
{
        guint i;

        if (state->receives != NULL) {
                for (i = 0; i < state->receives->len; i++)
                        receive_cancel (state->receives->pdata[i]);
                g_ptr_array_free (state->receives, FALSE);
                state->receives = NULL;
        }

        state->receives = g_ptr_array_new ();

        if (mimes == NULL)
                return;

        for (i = 0; i < mimes->len; i++) {
                ClipReceive *recv;
                int          fds[2];

                if (pipe (fds) < 0)
                        continue;

                ext_data_control_offer_v1_receive (offer, mimes->pdata[i], fds[1]);
                close (fds[1]);

                recv = g_new0 (ClipReceive, 1);
                recv->wl = state->wl;
                recv->kind = state->kind;
                recv->fd = fds[0];
                recv->mime = g_strdup (mimes->pdata[i]);
                recv->data = g_byte_array_new ();
                fcntl (recv->fd, F_SETFL, O_NONBLOCK);
                recv->source_id = g_unix_fd_add (recv->fd,
                                                 G_IO_IN | G_IO_HUP | G_IO_ERR,
                                                 receive_dispatch_cb,
                                                 recv);
                g_ptr_array_add (state->receives, recv);
        }
}

static void
source_write_all (gint                fd,
                  const guint8       *data,
                  gsize               len)
{
        gsize          total = 0;
        gint           flags;

        flags = fcntl (fd, F_GETFL, 0);
        if (flags != -1)
                fcntl (fd, F_SETFL, flags | O_NONBLOCK);

        while (total < len) {
                gssize n;

                n = write (fd, data + total, len - total);
                if (n > 0) {
                        total += n;
                        continue;
                }
                if (n < 0 && errno == EINTR)
                        continue;
                if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                        GPollFD pfd;

                        pfd.fd = fd;
                        pfd.events = G_IO_OUT;
                        if (g_poll (&pfd, 1, 10000) <= 0)
                                break;
                        continue;
                }
                break;
        }

        close (fd);
}

static void
handle_source_send (void                                 *data,
                    struct ext_data_control_source_v1    *source,
                    const char                           *mime_type,
                    int32_t                               fd)
{
        SelectionState *state = data;
        guint           i;

        if (state->cache == NULL) {
                close (fd);
                return;
        }

        for (i = 0; i < state->cache->len; i++) {
                ClipEntry *entry;

                entry = state->cache->pdata[i];
                if (g_strcmp0 (entry->mime, mime_type) == 0) {
                        source_write_all (fd, entry->data->data, entry->data->len);
                        return;
                }
        }

        close (fd);
}

static void
handle_source_cancelled (void                              *data,
                         struct ext_data_control_source_v1 *source)
{
        SelectionState *state = data;

        if (state->source == source) {
                ext_data_control_source_v1_destroy (source);
                state->source = NULL;
        }
}

static const struct ext_data_control_source_v1_listener source_listener = {
        .send = handle_source_send,
        .cancelled = handle_source_cancelled,
};

static void
start_reoffer (SelectionState *state)
{
        struct ext_data_control_source_v1 *source;
        guint                              i;

        if (state->cache == NULL || state->cache->len == 0)
                return;
        if (state->wl->dc_manager == NULL || state->wl->device == NULL)
                return;
        if (state->source != NULL)
                return;

        source = ext_data_control_manager_v1_create_data_source (state->wl->dc_manager);
        ext_data_control_source_v1_add_listener (source, &source_listener, state);

        for (i = 0; i < state->cache->len; i++) {
                ClipEntry *entry;

                entry = state->cache->pdata[i];
                ext_data_control_source_v1_offer (source, entry->mime);
        }

        state->source = source;

        if (state->kind == MSD_WAYLAND_SELECTION_PRIMARY)
                ext_data_control_device_v1_set_primary_selection (state->wl->device, source);
        else
                ext_data_control_device_v1_set_selection (state->wl->device, source);

        wl_display_flush (state->wl->display);
}

static void
handle_offer (void                             *data,
              struct ext_data_control_offer_v1 *offer,
              const char                       *mime_type)
{
        MsdClipboardManagerWayland *wl = data;

        if (wl->pending_offer == offer && wl->pending_mimes != NULL)
                g_ptr_array_add (wl->pending_mimes, g_strdup (mime_type));
}

static const struct ext_data_control_offer_v1_listener offer_listener = {
        .offer = handle_offer,
};

static void
handle_selection_for (MsdClipboardManagerWayland      *wl,
                      SelectionState                  *state,
                      struct ext_data_control_offer_v1 *offer)
{
        if (offer != NULL && state->source != NULL) {
                /* The compositor is echoing our own set_selection back to
                 * us: state->source is still the current selection (it was
                 * not cancelled), so this offer describes our own data.
                 * Keep the source alive and ignore the echoed offer.
                 */
                if (state->offer != NULL && state->offer != offer)
                        ext_data_control_offer_v1_destroy (state->offer);
                state->offer = offer;

                if (wl->pending_offer != NULL)
                        wl->pending_offer = NULL;
                if (wl->pending_mimes != NULL) {
                        g_ptr_array_free (wl->pending_mimes, TRUE);
                        wl->pending_mimes = NULL;
                }
                return;
        }

        if (state->source != NULL) {
                ext_data_control_source_v1_destroy (state->source);
                state->source = NULL;
        }

        if (state->offer != NULL && state->offer != offer) {
                ext_data_control_offer_v1_destroy (state->offer);
                state->offer = NULL;
        }

        if (offer == NULL) {
                state->offer = NULL;

                if (state->receives != NULL && state->receives->len > 0)
                        state->reoffer_pending = TRUE;
                else
                        start_reoffer (state);

                return;
        }

        state->offer = offer;
        state->reoffer_pending = FALSE;

        if (wl->pending_offer == offer && wl->pending_mimes != NULL) {
                GPtrArray *mimes;

                mimes = wl->pending_mimes;
                wl->pending_mimes = NULL;
                wl->pending_offer = NULL;

                start_receives (state, offer, mimes);
                g_ptr_array_free (mimes, TRUE);
        } else {
                start_receives (state, offer, NULL);
        }
}

static void
handle_selection (void                              *data,
                  struct ext_data_control_device_v1 *device,
                  struct ext_data_control_offer_v1  *offer)
{
        MsdClipboardManagerWayland *wl = data;

        handle_selection_for (wl, &wl->clipboard, offer);
}

static void
handle_primary_selection (void                              *data,
                          struct ext_data_control_device_v1 *device,
                          struct ext_data_control_offer_v1  *offer)
{
        MsdClipboardManagerWayland *wl = data;

        handle_selection_for (wl, &wl->primary, offer);
}

static void
handle_data_offer (void                              *data,
                   struct ext_data_control_device_v1 *device,
                   struct ext_data_control_offer_v1  *offer)
{
        MsdClipboardManagerWayland *wl = data;

        if (wl->pending_offer != NULL)
                ext_data_control_offer_v1_destroy (wl->pending_offer);
        if (wl->pending_mimes != NULL)
                g_ptr_array_free (wl->pending_mimes, TRUE);

        wl->pending_offer = offer;
        wl->pending_mimes = g_ptr_array_new_with_free_func (g_free);
        ext_data_control_offer_v1_add_listener (offer, &offer_listener, wl);
}

static void
handle_finished (void                              *data,
                 struct ext_data_control_device_v1 *device)
{
        MsdClipboardManagerWayland *wl = data;

        ext_data_control_device_v1_destroy (device);
        wl->device = NULL;
}

static const struct ext_data_control_device_v1_listener device_listener = {
        .data_offer = handle_data_offer,
        .selection = handle_selection,
        .finished = handle_finished,
        .primary_selection = handle_primary_selection,
};

static void
registry_handle_global (void             *data,
                        struct wl_registry *registry,
                        uint32_t          name,
                        const char       *interface,
                        uint32_t          version)
{
        MsdClipboardManagerWayland *wl = data;

        if (g_strcmp0 (interface, "ext_data_control_manager_v1") == 0) {
                wl->dc_manager = wl_registry_bind (registry,
                                                   name,
                                                   &ext_data_control_manager_v1_interface,
                                                   MIN (version, 1u));
        } else if (g_strcmp0 (interface, "wl_seat") == 0) {
                wl->seat = wl_registry_bind (registry,
                                             name,
                                             &wl_seat_interface,
                                             MIN (version, 5u));
        }
}

static void
registry_handle_global_remove (void             *data,
                               struct wl_registry *registry,
                               uint32_t          name)
{
}

static const struct wl_registry_listener registry_listener = {
        .global = registry_handle_global,
        .global_remove = registry_handle_global_remove,
};

static gboolean
display_dispatch_cb (gint         fd,
                     GIOCondition condition,
                     gpointer     user_data)
{
        MsdClipboardManagerWayland *wl = user_data;

        if (condition & (G_IO_HUP | G_IO_ERR)) {
                g_warning ("Clipboard manager: Wayland connection lost");
                return G_SOURCE_REMOVE;
        }

        if (wl_display_dispatch (wl->display) == -1) {
                g_warning ("Clipboard manager: Failed to dispatch Wayland events");
                return G_SOURCE_REMOVE;
        }

        wl_display_flush (wl->display);

        return G_SOURCE_CONTINUE;
}

static void
selection_state_clear (SelectionState *state)
{
        guint i;

        if (state->source != NULL) {
                ext_data_control_source_v1_destroy (state->source);
                state->source = NULL;
        }

        if (state->offer != NULL) {
                ext_data_control_offer_v1_destroy (state->offer);
                state->offer = NULL;
        }

        if (state->receives != NULL) {
                for (i = 0; i < state->receives->len; i++)
                        receive_cancel (state->receives->pdata[i]);
                g_ptr_array_free (state->receives, FALSE);
                state->receives = NULL;
        }

        if (state->cache != NULL) {
                g_ptr_array_free (state->cache, TRUE);
                state->cache = NULL;
        }
}

MsdClipboardManagerWayland *
msd_clipboard_manager_wayland_new (GError **error)
{
        MsdClipboardManagerWayland *wl;

        wl = g_new0 (MsdClipboardManagerWayland, 1);

        wl->display = wl_display_connect (NULL);
        if (wl->display == NULL) {
                g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                             "Could not connect to the Wayland compositor");
                g_free (wl);
                return NULL;
        }

        wl->clipboard.wl = wl;
        wl->clipboard.kind = MSD_WAYLAND_SELECTION_CLIPBOARD;
        wl->primary.wl = wl;
        wl->primary.kind = MSD_WAYLAND_SELECTION_PRIMARY;

        wl->registry = wl_display_get_registry (wl->display);
        wl_registry_add_listener (wl->registry, &registry_listener, wl);

        wl_display_roundtrip (wl->display);

        if (wl->dc_manager == NULL || wl->seat == NULL) {
                g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                             "The compositor does not support the ext-data-control protocol");
                msd_clipboard_manager_wayland_destroy (wl);
                return NULL;
        }

        wl->device = ext_data_control_manager_v1_get_data_device (wl->dc_manager, wl->seat);
        ext_data_control_device_v1_add_listener (wl->device, &device_listener, wl);

        wl->display_source = g_unix_fd_add (wl_display_get_fd (wl->display),
                                            G_IO_IN | G_IO_HUP | G_IO_ERR,
                                            display_dispatch_cb,
                                            wl);

        wl_display_roundtrip (wl->display);

        return wl;
}

void
msd_clipboard_manager_wayland_destroy (MsdClipboardManagerWayland *wl)
{
        if (wl == NULL)
                return;

        if (wl->display_source != 0) {
                g_source_remove (wl->display_source);
                wl->display_source = 0;
        }

        selection_state_clear (&wl->clipboard);
        selection_state_clear (&wl->primary);

        if (wl->pending_offer != NULL) {
                ext_data_control_offer_v1_destroy (wl->pending_offer);
                wl->pending_offer = NULL;
        }
        if (wl->pending_mimes != NULL) {
                g_ptr_array_free (wl->pending_mimes, TRUE);
                wl->pending_mimes = NULL;
        }

        if (wl->device != NULL) {
                ext_data_control_device_v1_destroy (wl->device);
                wl->device = NULL;
        }
        if (wl->dc_manager != NULL) {
                ext_data_control_manager_v1_destroy (wl->dc_manager);
                wl->dc_manager = NULL;
        }
        if (wl->seat != NULL) {
                wl_seat_destroy (wl->seat);
                wl->seat = NULL;
        }
        if (wl->registry != NULL) {
                wl_registry_destroy (wl->registry);
                wl->registry = NULL;
        }
        if (wl->display != NULL) {
                wl_display_disconnect (wl->display);
                wl->display = NULL;
        }

        g_free (wl);
}
