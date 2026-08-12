/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2026 MATE Developers
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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"

#include <stdlib.h>
#include <string.h>

#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <gdk/gdkwayland.h>
#include <libnotify/notify.h>

#define MATE_DESKTOP_USE_UNSTABLE_API
#include <libmate-desktop/mate-desktop-utils.h>

#include <libayatana-appindicator/app-indicator.h>

#include <wayland-client.h>
#include "wlr-output-management-unstable-v1-client-protocol.h"

#include "msd-wlrandr-manager.h"

#define CONF_SCHEMA                                    "org.mate.SettingsDaemon.plugins.wlrandr"
#define CONF_KEY_SHOW_NOTIFICATION_ICON                "show-notification-icon"
#define CONF_KEY_TURN_ON_EXTERNAL_MONITORS_AT_STARTUP  "turn-on-external-monitors-at-startup"
#define CONF_KEY_DEFAULT_CONFIGURATION_FILE            "default-configuration-file"

/* Number of seconds that the confirmation dialog will last before it resets the
 * RANDR configuration to its old state.
 */
#define CONFIRMATION_DIALOG_SECONDS 30

/* name of the icon files (msd-wlrandr.svg, etc.) */
#define MSD_WLRANDR_ICON_NAME   "msd-wlrandr"

/* executable of the control center's display configuration capplet */
#define MSD_WLRANDR_DISPLAY_CAPPLET  "mate-display-properties"

#define CONFIG_INTENDED_BASENAME "monitors-wayland.xml"
#define CONFIG_BACKUP_BASENAME   "monitors-wayland.xml.backup"

#define WLR_OUTPUT_MANAGER_VERSION 4

#define MSD_DBUS_PATH  "/org/mate/SettingsDaemon"
#define MSD_DBUS_NAME  "org.mate.SettingsDaemon"

#define MSD_WLRANDR_DBUS_NAME  MSD_DBUS_NAME ".WLRANDR"
#define MSD_WLRANDR_DBUS_NAME_2  MSD_WLRANDR_DBUS_NAME "_2"
#define MSD_WLRANDR_DBUS_PATH  MSD_DBUS_PATH "/WLRANDR"

static const gchar introspection_xml[] =
"<node>"
"  <interface name='org.mate.SettingsDaemon.WLRANDR'>"
"    <method name='GetConfiguration'>"
"      <arg name='configuration' type='a(ssbiiidbb(iiii)a(iiii))' direction='out'/>"
"    </method>"
"    <method name='SetConfiguration'>"
"      <arg name='configuration' type='a(sbiiiiidib)' direction='in'/>"
"    </method>"
"    <method name='ApplyConfiguration'>"
"    </method>"
"    <signal name='ConfigurationChanged'>"
"    </signal>"
"  </interface>"
""
"  <interface name='org.mate.SettingsDaemon.WLRANDR_2'>"
"    <method name='GetConfiguration'>"
"      <arg name='configuration' type='a(ssbiiidbb(iiii)a(iiii))' direction='out'/>"
"    </method>"
"    <method name='SetConfiguration'>"
"      <arg name='configuration' type='a(sbiiiiidib)' direction='in'/>"
"    </method>"
"    <method name='ApplyConfiguration'>"
"      <arg name='parent_window_id' type='x' direction='in'/>"
"      <arg name='timestamp' type='x' direction='in'/>"
"    </method>"
"    <signal name='ConfigurationChanged'>"
"    </signal>"
"  </interface>"
"</node>";

typedef struct {
        char     *name;
        char     *vendor;
        char     *product;
        char     *serial;
        gboolean  enabled;
        int       width;
        int       height;
        int       rate;
        int       x;
        int       y;
        double    scale;
        int       transform;
        gboolean  adaptive_sync;
} WlrOutputInfo;

typedef struct {
        WlrOutputInfo **outputs;
} WlrConfig;

typedef struct _WlOutputHead WlOutputHead;
typedef struct _WlOutputMode WlOutputMode;

typedef struct _WlOutputMode {
        int       width;
        int       height;
        int       refresh;
        gboolean  preferred;
        struct zwlr_output_mode_v1 *proxy;
        WlOutputHead *head;
} WlOutputMode;

typedef struct _WlOutputHead {
        char    *name;
        char    *description;
        char    *make;
        char    *model;
        char    *serial_number;
        gboolean  enabled;
        int       x;
        int       y;
        double    scale;
        int       transform;
        gboolean  adaptive_sync;
        int       adaptive_sync_supported; /* -1 unknown, 0 no, 1 yes */
        GList     *modes;
        WlOutputMode *current_mode;
        struct zwlr_output_head_v1 *proxy;
        MsdWlrandrManager *manager;
} WlOutputHead;

struct MsdWlrandrManagerPrivate {
        GSettings               *settings;
        struct wl_display       *display;
        struct wl_registry      *registry;
        struct zwlr_output_manager_v1 *manager;
        uint32_t                 manager_version;
        uint32_t                 last_serial;
        GList                   *heads;
        gboolean                 started;
        gboolean                 initial_config_applied;
        struct zwlr_output_configuration_v1 *pending_config;
        gboolean                 probing;
        AppIndicator            *indicator;
        GtkWidget               *menu;
        GtkWidget               *confirm_dialog;
        guint                    icon_changed_id;

        GDBusConnection         *connection;
        GDBusNodeInfo           *introspection_data;
        GCancellable            *bus_cancellable;
        guint                    owner_id;
        guint                    registration_id;
        guint                    registration_id_2;
};

static void
rebuild_menu (MsdWlrandrManager *manager);

static void
emit_configuration_changed (MsdWlrandrManager *manager);

static void
save_current_configuration (MsdWlrandrManager *manager, WlrConfig *config);

static void
queue_rollback_confirmation (MsdWlrandrManager *manager, WlrConfig *previous);

static void
probe_adaptive_sync_support (MsdWlrandrManager *manager);

static gboolean
probe_idle_cb (gpointer data)
{
        probe_adaptive_sync_support (data);
        return G_SOURCE_REMOVE;
}

static WlrConfig *
snapshot_config (MsdWlrandrManager *manager);

static WlrOutputInfo *
find_output_in_config (WlrConfig *config, const char *name);

static struct zwlr_output_mode_v1 *
find_mode_for_output (WlOutputHead *head, WlrOutputInfo *info);

static WlrOutputInfo *
wlr_output_info_new (const char *name)
{
        WlrOutputInfo *info = g_new0 (WlrOutputInfo, 1);
        info->name = g_strdup (name);
        info->scale = 1.0;
        return info;
}

static void
wlr_output_info_free (WlrOutputInfo *info)
{
        if (info == NULL)
                return;
        g_free (info->name);
        g_free (info->vendor);
        g_free (info->product);
        g_free (info->serial);
        g_free (info);
}

static void
wlr_config_free (WlrConfig *config)
{
        int i;

        if (config == NULL)
                return;

        for (i = 0; config->outputs != NULL && config->outputs[i] != NULL; i++)
                wlr_output_info_free (config->outputs[i]);
        g_free (config->outputs);
        g_free (config);
}

static WlOutputMode *
wlr_output_mode_new (struct zwlr_output_mode_v1 *proxy)
{
        WlOutputMode *mode = g_new0 (WlOutputMode, 1);
        mode->proxy = proxy;
        return mode;
}

static void
wlr_output_mode_free (WlOutputMode *mode)
{
        g_free (mode);
}

static WlOutputHead *
wlr_output_head_new (struct zwlr_output_head_v1 *proxy)
{
        WlOutputHead *head = g_new0 (WlOutputHead, 1);
        head->proxy = proxy;
        head->scale = 1.0;
        head->adaptive_sync_supported = -1;
        return head;
}

static void
wlr_output_head_free (WlOutputHead *head)
{
        GList *l;

        if (head == NULL)
                return;

        for (l = head->modes; l != NULL; l = l->next)
                wlr_output_mode_free (l->data);
        g_list_free (head->modes);

        g_free (head->name);
        g_free (head->description);
        g_free (head->make);
        g_free (head->model);
        g_free (head->serial_number);
        g_free (head);
}

static WlOutputMode *
find_mode_by_proxy (WlOutputHead *head, struct zwlr_output_mode_v1 *proxy)
{
        GList *l;

        for (l = head->modes; l != NULL; l = l->next) {
                WlOutputMode *mode = l->data;
                if (mode->proxy == proxy)
                        return mode;
        }

        return NULL;
}

/* ------------------------------------------------------------------ *
 *  wlr-output-management protocol listeners                           *
 * ------------------------------------------------------------------ */

static void
head_name_handler (void *data, struct zwlr_output_head_v1 *proxy, const char *name)
{
        WlOutputHead *head = data;
        g_free (head->name);
        head->name = g_strdup (name);
}

static void
head_description_handler (void *data, struct zwlr_output_head_v1 *proxy, const char *description)
{
        WlOutputHead *head = data;
        g_free (head->description);
        head->description = g_strdup (description);
}

static void
head_make_handler (void *data, struct zwlr_output_head_v1 *proxy, const char *make)
{
        WlOutputHead *head = data;
        g_free (head->make);
        head->make = g_strdup (make);
}

static void
head_model_handler (void *data, struct zwlr_output_head_v1 *proxy, const char *model)
{
        WlOutputHead *head = data;
        g_free (head->model);
        head->model = g_strdup (model);
}

static void
head_serial_number_handler (void *data, struct zwlr_output_head_v1 *proxy, const char *serial_number)
{
        WlOutputHead *head = data;
        g_free (head->serial_number);
        head->serial_number = g_strdup (serial_number);
}

static void
head_physical_size_handler (void *data, struct zwlr_output_head_v1 *proxy, int32_t width, int32_t height)
{
}

static void
mode_size_handler (void *data, struct zwlr_output_mode_v1 *mode, int32_t width, int32_t height)
{
        WlOutputMode *wm = data;
        wm->width = width;
        wm->height = height;
}

static void
mode_refresh_handler (void *data, struct zwlr_output_mode_v1 *mode, int32_t refresh)
{
        WlOutputMode *wm = data;
        wm->refresh = refresh;
}

static void
mode_preferred_handler (void *data, struct zwlr_output_mode_v1 *mode)
{
        WlOutputMode *wm = data;
        wm->preferred = TRUE;
}

static void
mode_finished_handler (void *data, struct zwlr_output_mode_v1 *mode)
{
        WlOutputMode *wm = data;
        WlOutputHead *head = wm->head;
        GList *l;

        for (l = head->modes; l != NULL; l = l->next) {
                if (l->data == wm) {
                        head->modes = g_list_delete_link (head->modes, l);
                        break;
                }
        }

        if (head->current_mode == wm)
                head->current_mode = NULL;

        wlr_output_mode_free (wm);
}

static const struct zwlr_output_mode_v1_listener mode_listener = {
        .size = mode_size_handler,
        .refresh = mode_refresh_handler,
        .preferred = mode_preferred_handler,
        .finished = mode_finished_handler,
};

static void
head_mode_handler (void *data, struct zwlr_output_head_v1 *proxy, struct zwlr_output_mode_v1 *mode)
{
        WlOutputHead *head = data;
        WlOutputMode *wm;

        wm = wlr_output_mode_new (mode);
        wm->head = head;
        head->modes = g_list_append (head->modes, wm);
        zwlr_output_mode_v1_add_listener (mode, &mode_listener, wm);
}

static void
head_enabled_handler (void *data, struct zwlr_output_head_v1 *proxy, int32_t enabled)
{
        WlOutputHead *head = data;
        head->enabled = enabled != 0;
}

static void
head_current_mode_handler (void *data, struct zwlr_output_head_v1 *proxy, struct zwlr_output_mode_v1 *mode)
{
        WlOutputHead *head = data;
        head->current_mode = find_mode_by_proxy (head, mode);
}

static void
head_position_handler (void *data, struct zwlr_output_head_v1 *proxy, int32_t x, int32_t y)
{
        WlOutputHead *head = data;
        head->x = x;
        head->y = y;
}

static void
head_transform_handler (void *data, struct zwlr_output_head_v1 *proxy, int32_t transform)
{
        WlOutputHead *head = data;
        head->transform = transform;
}

static void
head_scale_handler (void *data, struct zwlr_output_head_v1 *proxy, wl_fixed_t scale)
{
        WlOutputHead *head = data;
        head->scale = wl_fixed_to_double (scale);
}

static void
head_adaptive_sync_handler (void *data, struct zwlr_output_head_v1 *proxy, uint32_t state)
{
        WlOutputHead *head = data;
        head->adaptive_sync = state != 0;
}

static void
head_finished_handler (void *data, struct zwlr_output_head_v1 *proxy)
{
        MsdWlrandrManager *manager = ((WlOutputHead *) data)->manager;
        WlOutputHead *head = data;

        manager->priv->heads = g_list_remove (manager->priv->heads, head);
        wlr_output_head_free (head);
        rebuild_menu (manager);
}

static const struct zwlr_output_head_v1_listener head_listener = {
        .name = head_name_handler,
        .description = head_description_handler,
        .physical_size = head_physical_size_handler,
        .mode = head_mode_handler,
        .enabled = head_enabled_handler,
        .current_mode = head_current_mode_handler,
        .position = head_position_handler,
        .transform = head_transform_handler,
        .scale = head_scale_handler,
        .finished = head_finished_handler,
        .make = head_make_handler,
        .model = head_model_handler,
        .serial_number = head_serial_number_handler,
        .adaptive_sync = head_adaptive_sync_handler,
};

static void
manager_head_handler (void *data, struct zwlr_output_manager_v1 *manager_obj, struct zwlr_output_head_v1 *proxy)
{
        MsdWlrandrManager *manager = data;
        WlOutputHead *head;

        head = wlr_output_head_new (proxy);
        head->manager = manager;
        manager->priv->heads = g_list_append (manager->priv->heads, head);

        zwlr_output_head_v1_add_listener (proxy, &head_listener, head);
}

static void
apply_initial_config (MsdWlrandrManager *manager);

static void
manager_done_handler (void *data, struct zwlr_output_manager_v1 *manager_obj, uint32_t serial)
{
        MsdWlrandrManager *manager = data;

        manager->priv->last_serial = serial;
        g_debug ("msd-wlrandr: output manager done, serial %u", serial);

        rebuild_menu (manager);
        emit_configuration_changed (manager);

        if (!manager->priv->initial_config_applied) {
                manager->priv->initial_config_applied = TRUE;
                apply_initial_config (manager);
        }

        probe_adaptive_sync_support (manager);
}

static void
manager_finished_handler (void *data, struct zwlr_output_manager_v1 *manager_obj)
{
        MsdWlrandrManager *manager = data;

        g_debug ("msd-wlrandr: output manager finished");

        if (manager->priv->manager != NULL) {
                zwlr_output_manager_v1_destroy (manager->priv->manager);
                manager->priv->manager = NULL;
        }
}

static const struct zwlr_output_manager_v1_listener manager_listener = {
        .head = manager_head_handler,
        .done = manager_done_handler,
        .finished = manager_finished_handler,
};

static void
registry_handle_global (void *data, struct wl_registry *registry, uint32_t name, const char *interface, uint32_t version)
{
        MsdWlrandrManager *manager = data;

        if (strcmp (interface, zwlr_output_manager_v1_interface.name) == 0) {
                manager->priv->manager_version = MIN (version, WLR_OUTPUT_MANAGER_VERSION);
                manager->priv->manager = wl_registry_bind (registry, name,
                                                           &zwlr_output_manager_v1_interface,
                                                           manager->priv->manager_version);
                zwlr_output_manager_v1_add_listener (manager->priv->manager, &manager_listener, manager);
                g_debug ("msd-wlrandr: bound zwlr_output_manager_v1 version %u", manager->priv->manager_version);
        }
}

static void
registry_handle_global_remove (void *data, struct wl_registry *registry, uint32_t name)
{
}

static const struct wl_registry_listener registry_listener = {
        .global = registry_handle_global,
        .global_remove = registry_handle_global_remove,
};

/* ------------------------------------------------------------------ *
 *  Configuration application                                          *
 * ------------------------------------------------------------------ */

typedef struct {
        MsdWlrandrManager *manager;
        gboolean  save;
        gboolean  confirm_rollback;
        WlrConfig *previous;
        WlrConfig *config;
        WlOutputHead *adaptive_sync_head;
} ApplyData;

static void
notify_config_error (const char *message)
{
        NotifyNotification *n;

        g_warning ("msd-wlrandr: %s", message);

        if (notify_is_initted ()) {
                n = notify_notification_new (_("Display Settings"), message, MSD_WLRANDR_ICON_NAME);
                notify_notification_set_timeout (n, 3000);
                notify_notification_show (n, NULL);
                g_object_unref (n);
        }
}

static void
config_succeeded_handler (void *data, struct zwlr_output_configuration_v1 *config)
{
        ApplyData *ad = data;

        g_debug ("msd-wlrandr: output configuration applied");

        if (ad->save)
                save_current_configuration (ad->manager, ad->config);

        if (ad->confirm_rollback)
                queue_rollback_confirmation (ad->manager, ad->previous);
        else
                wlr_config_free (ad->previous);

        ad->manager->priv->pending_config = NULL;
        g_idle_add (probe_idle_cb, ad->manager);
        zwlr_output_configuration_v1_destroy (config);
        wlr_config_free (ad->config);
        g_free (ad);
}

static void
config_failed_handler (void *data, struct zwlr_output_configuration_v1 *config)
{
        ApplyData *ad = data;

        if (ad->adaptive_sync_head != NULL &&
            g_list_find (ad->manager->priv->heads, ad->adaptive_sync_head) != NULL) {
                WlOutputHead *head = ad->adaptive_sync_head;

                g_debug ("msd-wlrandr: adaptive sync rejected by the compositor on %s", head->name);
                head->adaptive_sync_supported = 0;
                head->adaptive_sync = 0;

                /* The configuration was rejected, so nothing was applied and
                 * the live head state is unchanged; persist it as is.
                 */
                save_current_configuration (ad->manager, NULL);
                rebuild_menu (ad->manager);
        } else {
                notify_config_error (_("Could not apply the display configuration"));
        }

        wlr_config_free (ad->previous);
        ad->manager->priv->pending_config = NULL;
        g_idle_add (probe_idle_cb, ad->manager);
        zwlr_output_configuration_v1_destroy (config);
        wlr_config_free (ad->config);
        g_free (ad);
}

static void
config_cancelled_handler (void *data, struct zwlr_output_configuration_v1 *config)
{
        ApplyData *ad = data;

        g_debug ("msd-wlrandr: output configuration cancelled");

        wlr_config_free (ad->previous);
        ad->manager->priv->pending_config = NULL;
        g_idle_add (probe_idle_cb, ad->manager);
        zwlr_output_configuration_v1_destroy (config);
        wlr_config_free (ad->config);
        g_free (ad);
}

static const struct zwlr_output_configuration_v1_listener config_listener = {
        .succeeded = config_succeeded_handler,
        .failed = config_failed_handler,
        .cancelled = config_cancelled_handler,
};

typedef struct {
        MsdWlrandrManager *manager;
        WlOutputHead *head;
        guint stage;
        gboolean original;
} ProbeData;

static const struct zwlr_output_configuration_v1_listener probe_listener;

static void
probe_confirm (ProbeData *pd, gboolean adaptive_sync)
{
        struct MsdWlrandrManagerPrivate *priv = pd->manager->priv;
        struct zwlr_output_configuration_v1 *config_obj;
        struct zwlr_output_configuration_head_v1 *head_obj;
        WlrConfig *config;
        GList *l;

        if (priv->manager == NULL || g_list_find (priv->heads, pd->head) == NULL) {
                g_free (pd);
                return;
        }

        config = snapshot_config (pd->manager);
        config_obj = zwlr_output_manager_v1_create_configuration (priv->manager, priv->last_serial);

        for (l = priv->heads; l != NULL; l = l->next) {
                WlOutputHead *head = l->data;
                WlrOutputInfo *info = find_output_in_config (config, head->name);

                if (info != NULL && info->enabled) {
                        struct zwlr_output_mode_v1 *mode = find_mode_for_output (head, info);

                        head_obj = zwlr_output_configuration_v1_enable_head (config_obj, head->proxy);

                        if (mode != NULL)
                                zwlr_output_configuration_head_v1_set_mode (head_obj, mode);
                        else
                                zwlr_output_configuration_head_v1_set_custom_mode (head_obj,
                                                                                   info->width,
                                                                                   info->height,
                                                                                   info->rate);

                        zwlr_output_configuration_head_v1_set_position (head_obj, info->x, info->y);
                        zwlr_output_configuration_head_v1_set_transform (head_obj, info->transform);
                        zwlr_output_configuration_head_v1_set_scale (head_obj, wl_fixed_from_double (info->scale));

                        if (priv->manager_version >= 4 && head == pd->head)
                                zwlr_output_configuration_head_v1_set_adaptive_sync (head_obj,
                                                                                    adaptive_sync ? 1 : 0);
                } else {
                        zwlr_output_configuration_v1_disable_head (config_obj, head->proxy);
                }
        }

        wlr_config_free (config);

        zwlr_output_configuration_v1_add_listener (config_obj, &probe_listener, pd);
        zwlr_output_configuration_v1_apply (config_obj);
        wl_display_flush (priv->display);
}

static void
probe_succeeded_handler (void *data, struct zwlr_output_configuration_v1 *config)
{
        ProbeData *pd = data;
        struct MsdWlrandrManagerPrivate *priv = pd->manager->priv;

        if (pd->stage == 0) {
                if (g_list_find (priv->heads, pd->head) != NULL) {
                        g_debug ("msd-wlrandr: adaptive sync toggle accepted on %s", pd->head->name);
                        pd->stage = 1;
                        probe_confirm (pd, pd->original);
                        zwlr_output_configuration_v1_destroy (config);
                        return;
                }
        } else if (g_list_find (priv->heads, pd->head) != NULL) {
                g_debug ("msd-wlrandr: adaptive sync confirmed on %s", pd->head->name);
                pd->head->adaptive_sync_supported = 1;
                pd->head->adaptive_sync = pd->original;
                rebuild_menu (pd->manager);
        }

        priv->probing = FALSE;
        probe_adaptive_sync_support (pd->manager);
        zwlr_output_configuration_v1_destroy (config);
        g_free (pd);
}

static void
probe_failed_handler (void *data, struct zwlr_output_configuration_v1 *config)
{
        ProbeData *pd = data;
        struct MsdWlrandrManagerPrivate *priv = pd->manager->priv;

        if (pd->stage == 0 && g_list_find (priv->heads, pd->head) != NULL) {
                g_debug ("msd-wlrandr: adaptive sync not supported on %s", pd->head->name);
                pd->head->adaptive_sync_supported = 0;
                rebuild_menu (pd->manager);
        } else if (g_list_find (priv->heads, pd->head) != NULL) {
                g_debug ("msd-wlrandr: adaptive sync restore rejected on %s", pd->head->name);
                pd->head->adaptive_sync_supported = 0;
                pd->head->adaptive_sync = pd->original;
                rebuild_menu (pd->manager);
        }

        priv->probing = FALSE;
        probe_adaptive_sync_support (pd->manager);
        zwlr_output_configuration_v1_destroy (config);
        g_free (pd);
}

static void
probe_cancelled_handler (void *data, struct zwlr_output_configuration_v1 *config)
{
        ProbeData *pd = data;

        g_debug ("msd-wlrandr: adaptive sync probe cancelled on %s", pd->head->name);

        pd->manager->priv->probing = FALSE;
        probe_adaptive_sync_support (pd->manager);
        zwlr_output_configuration_v1_destroy (config);
        g_free (pd);
}

static const struct zwlr_output_configuration_v1_listener probe_listener = {
        .succeeded = probe_succeeded_handler,
        .failed = probe_failed_handler,
        .cancelled = probe_cancelled_handler,
};

static gboolean
head_is_wayland_backend (WlOutputHead *head)
{
        return g_strcmp0 (head->make, "wayland") == 0 &&
               g_strcmp0 (head->model, "wayland") == 0;
}

static void
probe_adaptive_sync_support (MsdWlrandrManager *manager)
{
        struct MsdWlrandrManagerPrivate *priv = manager->priv;
        ProbeData *pd;
        GList *l;

        if (priv->manager == NULL || priv->manager_version < 4)
                return;

        if (priv->pending_config != NULL || priv->probing)
                return;

        for (l = priv->heads; l != NULL; l = l->next) {
                WlOutputHead *head = l->data;

                if (!head->enabled || head->adaptive_sync_supported != -1)
                        continue;

                /* The wlroots Wayland backend cannot enable VRR on its
                 * nested output, and the compositor's reply is not reliable
                 * for it.
                 */
                if (head_is_wayland_backend (head)) {
                        g_debug ("msd-wlrandr: adaptive sync not supported on nested output %s", head->name);
                        head->adaptive_sync_supported = 0;
                        continue;
                }

                if (head->current_mode == NULL)
                        continue;

                pd = g_new0 (ProbeData, 1);
                pd->manager = manager;
                pd->head = head;
                pd->stage = 0;
                pd->original = head->adaptive_sync;
                priv->probing = TRUE;

                probe_confirm (pd, !head->adaptive_sync);

                return;
        }
}

static WlrOutputInfo *
find_output_in_config (WlrConfig *config, const char *name)
{
        int i;

        if (config == NULL || config->outputs == NULL)
                return NULL;

        for (i = 0; config->outputs[i] != NULL; i++) {
                if (g_strcmp0 (config->outputs[i]->name, name) == 0)
                        return config->outputs[i];
        }

        return NULL;
}

static struct zwlr_output_mode_v1 *
find_mode_for_output (WlOutputHead *head, WlrOutputInfo *info)
{
        GList *l;
        struct zwlr_output_mode_v1 *fallback = NULL;

        for (l = head->modes; l != NULL; l = l->next) {
                WlOutputMode *wm = l->data;

                if (wm->width == info->width && wm->height == info->height) {
                        if (fallback == NULL)
                                fallback = wm->proxy;
                        if (info->rate <= 0 || wm->refresh == info->rate)
                                return wm->proxy;
                }
        }

        return fallback;
}

static void
fill_output_mode (WlOutputHead *head, WlrOutputInfo *info)
{
        GList *l;
        WlOutputMode *wm;

        if (info->width > 0 && info->height > 0)
                return;

        if (head->current_mode != NULL) {
                info->width = head->current_mode->width;
                info->height = head->current_mode->height;
                info->rate = head->current_mode->refresh;
                return;
        }

        for (l = head->modes; l != NULL; l = l->next) {
                wm = l->data;
                if (wm->preferred || info->width <= 0) {
                        info->width = wm->width;
                        info->height = wm->height;
                        info->rate = wm->refresh;
                        if (wm->preferred)
                                return;
                }
        }
}

/* Deep copy of a configuration.  Used to keep a reference to the
 * configuration that was actually sent to the compositor, so that the file
 * is saved with the intended values rather than whatever the compositor
 * last reported (which may not have been updated yet when the
 * "succeeded" event arrives).
 */
static WlrConfig *
wlr_config_copy (const WlrConfig *config)
{
        WlrConfig *copy;
        GPtrArray *outputs;
        int i;

        if (config == NULL)
                return NULL;

        outputs = g_ptr_array_new ();
        for (i = 0; config->outputs != NULL && config->outputs[i] != NULL; i++) {
                WlrOutputInfo *src = config->outputs[i];
                WlrOutputInfo *dst = wlr_output_info_new (src->name);

                dst->vendor = g_strdup (src->vendor);
                dst->product = g_strdup (src->product);
                dst->serial = g_strdup (src->serial);
                dst->enabled = src->enabled;
                dst->width = src->width;
                dst->height = src->height;
                dst->rate = src->rate;
                dst->x = src->x;
                dst->y = src->y;
                dst->scale = src->scale;
                dst->transform = src->transform;
                dst->adaptive_sync = src->adaptive_sync;

                g_ptr_array_add (outputs, dst);
        }
        g_ptr_array_add (outputs, NULL);

        copy = g_new0 (WlrConfig, 1);
        copy->outputs = (WlrOutputInfo **) g_ptr_array_free (outputs, FALSE);

        return copy;
}

static gboolean
apply_configuration (MsdWlrandrManager *manager,
                     WlrConfig         *config,
                     gboolean           save,
                     gboolean           confirm_rollback,
                     WlrConfig         *previous,
                     WlOutputHead      *adaptive_sync_head)
{
        struct MsdWlrandrManagerPrivate *priv = manager->priv;
        struct zwlr_output_configuration_v1 *config_obj;
        GList *l;
        ApplyData *ad;

        if (priv->manager == NULL || priv->pending_config != NULL)
                return FALSE;

        g_debug ("msd-wlrandr: applying output configuration");

        config_obj = zwlr_output_manager_v1_create_configuration (priv->manager, priv->last_serial);

        for (l = priv->heads; l != NULL; l = l->next) {
                WlOutputHead *head = l->data;
                WlrOutputInfo *info = find_output_in_config (config, head->name);
                struct zwlr_output_configuration_head_v1 *head_obj;

                if (info != NULL && info->enabled) {
                        fill_output_mode (head, info);

                        head_obj = zwlr_output_configuration_v1_enable_head (config_obj, head->proxy);

                        {
                                struct zwlr_output_mode_v1 *mode = find_mode_for_output (head, info);

                                if (mode != NULL)
                                        zwlr_output_configuration_head_v1_set_mode (head_obj, mode);
                                else
                                        zwlr_output_configuration_head_v1_set_custom_mode (head_obj,
                                                                                           info->width,
                                                                                           info->height,
                                                                                           info->rate);
                        }

                        zwlr_output_configuration_head_v1_set_position (head_obj, info->x, info->y);
                        zwlr_output_configuration_head_v1_set_transform (head_obj, info->transform);
                        zwlr_output_configuration_head_v1_set_scale (head_obj, wl_fixed_from_double (info->scale));

                        if (priv->manager_version >= 4 && head == adaptive_sync_head)
                                zwlr_output_configuration_head_v1_set_adaptive_sync (head_obj,
                                                                                    info->adaptive_sync ? 1 : 0);
                } else {
                        zwlr_output_configuration_v1_disable_head (config_obj, head->proxy);
                }
        }

        ad = g_new0 (ApplyData, 1);
        ad->manager = manager;
        ad->save = save;
        ad->confirm_rollback = confirm_rollback;
        ad->previous = previous;
        ad->adaptive_sync_head = adaptive_sync_head;

        ad->config = wlr_config_copy (config);

        /* Enrich the copy with the head identity fields; configurations that
         * arrive over D-Bus do not carry them and they are useful when the
         * file is inspected by hand.
         */
        for (l = priv->heads; l != NULL; l = l->next) {
                WlOutputHead *head = l->data;
                WlrOutputInfo *info = find_output_in_config (ad->config, head->name);

                if (info == NULL)
                        continue;
                if (info->vendor == NULL || *info->vendor == '\0')
                        info->vendor = g_strdup (head->make);
                if (info->product == NULL || *info->product == '\0')
                        info->product = g_strdup (head->model);
                if (info->serial == NULL || *info->serial == '\0')
                        info->serial = g_strdup (head->serial_number);
        }

        zwlr_output_configuration_v1_add_listener (config_obj, &config_listener, ad);
        priv->pending_config = config_obj;
        zwlr_output_configuration_v1_apply (config_obj);
        wl_display_flush (priv->display);

        return TRUE;
}

static int
enabled_head_count (MsdWlrandrManager *manager)
{
        GList *l;
        int count = 0;

        for (l = manager->priv->heads; l != NULL; l = l->next) {
                WlOutputHead *head = l->data;
                if (head->enabled)
                        count++;
        }

        return count;
}

/* ------------------------------------------------------------------ *
 *  Confirmation / rollback of risky changes                           *
 * ------------------------------------------------------------------ */

typedef struct {
        MsdWlrandrManager *manager;
        WlrConfig *previous;
        GtkWidget *dialog;
        GtkLabel  *countdown_label;
        guint      countdown;
        guint      timeout_id;
} ConfirmationData;

static void
restore_previous_configuration (MsdWlrandrManager *manager, WlrConfig *previous)
{
        g_debug ("msd-wlrandr: restoring previous configuration");

        if (!apply_configuration (manager, previous, TRUE, FALSE, NULL, NULL))
                g_warning ("msd-wlrandr: could not restore previous configuration");
}

static gboolean
confirmation_timeout_cb (gpointer data)
{
        ConfirmationData *cd = data;
        char *markup;

        if (cd->countdown > 1) {
                cd->countdown--;
                markup = g_markup_printf_escaped (_("Reverting in %u second%s…"),
                                                  cd->countdown,
                                                  cd->countdown == 1 ? "" : "s");
                gtk_label_set_markup (cd->countdown_label, markup);
                g_free (markup);
                return G_SOURCE_CONTINUE;
        }

        cd->timeout_id = 0;
        gtk_dialog_response (GTK_DIALOG (cd->dialog), GTK_RESPONSE_CANCEL);
        return G_SOURCE_REMOVE;
}

static void
confirmation_dialog_response_cb (GtkDialog *dialog, gint response_id, gpointer data)
{
        ConfirmationData *cd = data;

        if (response_id != GTK_RESPONSE_ACCEPT)
                restore_previous_configuration (cd->manager, cd->previous);

        gtk_widget_destroy (cd->dialog);
}

static void
confirmation_dialog_destroy_cb (GtkWidget *widget, gpointer data)
{
        ConfirmationData *cd = data;

        cd->manager->priv->confirm_dialog = NULL;

        if (cd->timeout_id > 0) {
                g_source_remove (cd->timeout_id);
                cd->timeout_id = 0;
        }

        wlr_config_free (cd->previous);
        g_free (cd);
}

static void
queue_rollback_confirmation (MsdWlrandrManager *manager, WlrConfig *previous)
{
        ConfirmationData *cd;
        GtkWidget *dialog;
        GtkWidget *content;
        GtkWidget *countdown_label;
        char *markup;

        if (manager->priv->confirm_dialog != NULL) {
                wlr_config_free (previous);
                return;
        }

        cd = g_new0 (ConfirmationData, 1);
        cd->manager = manager;
        cd->previous = previous;
        cd->countdown = CONFIRMATION_DIALOG_SECONDS;

        dialog = gtk_message_dialog_new (NULL,
                                         GTK_DIALOG_MODAL,
                                         GTK_MESSAGE_QUESTION,
                                         GTK_BUTTONS_NONE,
                                         _("Does the display look OK?"));
        gtk_window_set_icon_name (GTK_WINDOW (dialog), "preferences-desktop-display");
        gtk_dialog_add_button (GTK_DIALOG (dialog), _("_Restore Previous Configuration"), GTK_RESPONSE_CANCEL);
        gtk_dialog_add_button (GTK_DIALOG (dialog), _("_Keep This Configuration"), GTK_RESPONSE_ACCEPT);
        gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT);

        countdown_label = gtk_label_new (NULL);
        markup = g_markup_printf_escaped (_("Reverting in %u seconds…"), cd->countdown);
        gtk_label_set_markup (GTK_LABEL (countdown_label), markup);
        g_free (markup);
        gtk_widget_show (countdown_label);

        content = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
        gtk_container_add (GTK_CONTAINER (content), countdown_label);

        cd->dialog = dialog;
        cd->countdown_label = GTK_LABEL (countdown_label);

        g_signal_connect (dialog, "response", G_CALLBACK (confirmation_dialog_response_cb), cd);
        g_signal_connect (dialog, "destroy", G_CALLBACK (confirmation_dialog_destroy_cb), cd);

        manager->priv->confirm_dialog = dialog;
        gtk_widget_show_all (dialog);

        cd->timeout_id = g_timeout_add (1000, confirmation_timeout_cb, cd);
}

/* ------------------------------------------------------------------ *
 *  Configuration snapshots                                            *
 * ------------------------------------------------------------------ */

static WlrOutputInfo *
snapshot_head (WlOutputHead *head)
{
        WlrOutputInfo *info = wlr_output_info_new (head->name);

        info->vendor = g_strdup (head->make);
        info->product = g_strdup (head->model);
        info->serial = g_strdup (head->serial_number);
        info->enabled = head->enabled;
        info->x = head->x;
        info->y = head->y;
        info->scale = head->scale;
        info->transform = head->transform;
        info->adaptive_sync = head->adaptive_sync;

        if (head->enabled && head->current_mode != NULL) {
                info->width = head->current_mode->width;
                info->height = head->current_mode->height;
                info->rate = head->current_mode->refresh;
        } else if (head->enabled) {
                fill_output_mode (head, info);
        }

        return info;
}

static WlrConfig *
snapshot_config (MsdWlrandrManager *manager)
{
        WlrConfig *config;
        GList *l;
        GPtrArray *outputs;

        outputs = g_ptr_array_new ();
        for (l = manager->priv->heads; l != NULL; l = l->next)
                g_ptr_array_add (outputs, snapshot_head (l->data));
        g_ptr_array_add (outputs, NULL);

        config = g_new0 (WlrConfig, 1);
        config->outputs = (WlrOutputInfo **) g_ptr_array_free (outputs, FALSE);

        return config;
}

/* ------------------------------------------------------------------ *
 *  Config file storage (mirrors monitors.xml)                         *
 * ------------------------------------------------------------------ */

typedef struct {
        WlrOutputInfo *output;
        GPtrArray    *outputs;
        GPtrArray    *configs;
        GString      *text;
        char         *current_element;
} WlrConfigParser;

static gboolean
yes_no_to_boolean (const char *text)
{
        if (text == NULL)
                return FALSE;

        if (g_ascii_strcasecmp (text, "yes") == 0 ||
            g_ascii_strcasecmp (text, "true") == 0 ||
            strcmp (text, "1") == 0)
                return TRUE;

        return FALSE;
}

/* Read a scale value from the configuration file.  g_ascii_strtod() is
 * locale-independent and only accepts '.' as the decimal separator, so
 * normalize any ',' left in the file by older versions of the plugin that
 * wrote it with a locale-dependent "%f".  Without this, e.g. "1,500000"
 * would silently come back as 1.0.
 */
static double
parse_scale (const char *text)
{
        char *normalized;
        char *p;
        double value;

        if (text == NULL)
                return 1.0;

        normalized = g_strdup (text);
        for (p = normalized; *p != '\0'; p++) {
                if (*p == ',')
                        *p = '.';
        }

        value = g_ascii_strtod (normalized, NULL);
        g_free (normalized);

        return value;
}

static const char *
transform_to_name (int transform)
{
        switch (transform) {
        case WL_OUTPUT_TRANSFORM_NORMAL:
                return "normal";
        case WL_OUTPUT_TRANSFORM_90:
                return "90";
        case WL_OUTPUT_TRANSFORM_180:
                return "180";
        case WL_OUTPUT_TRANSFORM_270:
                return "270";
        case WL_OUTPUT_TRANSFORM_FLIPPED:
                return "flipped";
        case WL_OUTPUT_TRANSFORM_FLIPPED_90:
                return "flipped-90";
        case WL_OUTPUT_TRANSFORM_FLIPPED_180:
                return "flipped-180";
        case WL_OUTPUT_TRANSFORM_FLIPPED_270:
                return "flipped-270";
        default:
                return "normal";
        }
}

static int
transform_from_name (const char *name)
{
        if (name == NULL)
                return WL_OUTPUT_TRANSFORM_NORMAL;

        if (strcmp (name, "normal") == 0)
                return WL_OUTPUT_TRANSFORM_NORMAL;
        if (strcmp (name, "90") == 0)
                return WL_OUTPUT_TRANSFORM_90;
        if (strcmp (name, "180") == 0)
                return WL_OUTPUT_TRANSFORM_180;
        if (strcmp (name, "270") == 0)
                return WL_OUTPUT_TRANSFORM_270;
        if (strcmp (name, "flipped") == 0)
                return WL_OUTPUT_TRANSFORM_FLIPPED;
        if (strcmp (name, "flipped-90") == 0)
                return WL_OUTPUT_TRANSFORM_FLIPPED_90;
        if (strcmp (name, "flipped-180") == 0)
                return WL_OUTPUT_TRANSFORM_FLIPPED_180;
        if (strcmp (name, "flipped-270") == 0)
                return WL_OUTPUT_TRANSFORM_FLIPPED_270;

        return WL_OUTPUT_TRANSFORM_NORMAL;
}

static void
parser_start_element (GMarkupParseContext *context,
                      const gchar          *element_name,
                      const gchar         **attribute_names,
                      const gchar         **attribute_values,
                      gpointer              user_data,
                      GError              **error)
{
        WlrConfigParser *parser = user_data;
        const char *name = NULL;
        int i;

        if (strcmp (element_name, "configuration") == 0) {
                parser->outputs = g_ptr_array_new ();
                return;
        }

        if (strcmp (element_name, "head") == 0) {
                for (i = 0; attribute_names[i] != NULL; i++) {
                        if (strcmp (attribute_names[i], "name") == 0) {
                                name = attribute_values[i];
                                break;
                        }
                }
                parser->output = wlr_output_info_new (name);
                if (parser->outputs != NULL)
                        g_ptr_array_add (parser->outputs, parser->output);
                return;
        }

        g_free (parser->current_element);
        parser->current_element = g_ascii_strdown (element_name, -1);
        if (parser->text != NULL)
                g_string_truncate (parser->text, 0);
        else
                parser->text = g_string_new (NULL);
}

static void
parser_end_element (GMarkupParseContext *context,
                    const gchar          *element_name,
                    gpointer              user_data,
                    GError              **error)
{
        WlrConfigParser *parser = user_data;
        WlrOutputInfo *output = parser->output;
        const char *text = parser->text != NULL ? parser->text->str : "";

        if (strcmp (element_name, "configuration") == 0) {
                WlrConfig *config;

                if (parser->outputs != NULL) {
                        g_ptr_array_add (parser->outputs, NULL);
                        config = g_new0 (WlrConfig, 1);
                        config->outputs = (WlrOutputInfo **) g_ptr_array_free (parser->outputs, FALSE);
                        parser->outputs = NULL;
                        g_ptr_array_add (parser->configs, config);
                }
                return;
        }

        if (strcmp (element_name, "head") == 0) {
                parser->output = NULL;
                return;
        }

        if (parser->current_element == NULL || output == NULL)
                return;

        if (strcmp (parser->current_element, "vendor") == 0) {
                g_free (output->vendor);
                output->vendor = g_strdup (text);
        } else if (strcmp (parser->current_element, "product") == 0) {
                g_free (output->product);
                output->product = g_strdup (text);
        } else if (strcmp (parser->current_element, "serial") == 0) {
                g_free (output->serial);
                output->serial = g_strdup (text);
        } else if (strcmp (parser->current_element, "enabled") == 0) {
                output->enabled = yes_no_to_boolean (text);
        } else if (strcmp (parser->current_element, "width") == 0) {
                output->width = atoi (text);
        } else if (strcmp (parser->current_element, "height") == 0) {
                output->height = atoi (text);
        } else if (strcmp (parser->current_element, "rate") == 0) {
                output->rate = atoi (text);
        } else if (strcmp (parser->current_element, "x") == 0) {
                output->x = atoi (text);
        } else if (strcmp (parser->current_element, "y") == 0) {
                output->y = atoi (text);
        } else if (strcmp (parser->current_element, "scale") == 0) {
                output->scale = parse_scale (text);
        } else if (strcmp (parser->current_element, "transform") == 0) {
                output->transform = transform_from_name (text);
        } else if (strcmp (parser->current_element, "adaptive_sync") == 0) {
                output->adaptive_sync = yes_no_to_boolean (text);
        }
}

static void
parser_text (GMarkupParseContext *context,
             const gchar          *text,
             gsize                 text_len,
             gpointer              user_data,
             GError              **error)
{
        WlrConfigParser *parser = user_data;

        if (parser->text != NULL)
                g_string_append_len (parser->text, text, text_len);
}

static WlrConfigParser *
parser_new (void)
{
        WlrConfigParser *parser = g_new0 (WlrConfigParser, 1);
        parser->configs = g_ptr_array_new ();
        return parser;
}

static WlrConfig **
configs_load_from_file (const char *filename)
{
        GMarkupParseContext *context;
        WlrConfigParser *parser;
        gchar *contents = NULL;
        GError *error = NULL;
        WlrConfig **result = NULL;

        if (!g_file_get_contents (filename, &contents, NULL, &error)) {
                g_debug ("msd-wlrandr: cannot read config file %s: %s", filename, error->message);
                g_error_free (error);
                return NULL;
        }

        parser = parser_new ();

        context = g_markup_parse_context_new (&(GMarkupParser) {
                .start_element = parser_start_element,
                .end_element = parser_end_element,
                .text = parser_text,
        }, 0, parser, NULL);

        if (g_markup_parse_context_parse (context, contents, -1, &error) &&
            g_markup_parse_context_end_parse (context, &error)) {
                g_ptr_array_add (parser->configs, NULL);
                result = (WlrConfig **) g_ptr_array_free (parser->configs, FALSE);
        } else {
                g_warning ("msd-wlrandr: failed to parse config file %s: %s", filename, error->message);
                g_error_free (error);
        }

        g_markup_parse_context_free (context);
        g_free (contents);
        g_free (parser);

        return result;
}

static void
free_config_list (WlrConfig **configs)
{
        int i;

        if (configs == NULL)
                return;

        for (i = 0; configs[i] != NULL; i++)
                wlr_config_free (configs[i]);
        g_free (configs);
}

static gboolean
config_matches_heads (WlrConfig *config, GList *heads)
{
        GPtrArray *config_names;
        GPtrArray *head_names;
        GList *l;
        int i, j;
        gboolean match;

        config_names = g_ptr_array_new ();
        for (i = 0; config->outputs != NULL && config->outputs[i] != NULL; i++)
                g_ptr_array_add (config_names, config->outputs[i]->name);

        head_names = g_ptr_array_new ();
        for (l = heads; l != NULL; l = l->next)
                g_ptr_array_add (head_names, ((WlOutputHead *) l->data)->name);

        match = config_names->len == head_names->len;
        if (match) {
                for (i = 0; i < config_names->len && match; i++) {
                        gboolean found = FALSE;
                        for (j = 0; j < head_names->len; j++) {
                                if (g_strcmp0 (g_ptr_array_index (config_names, i), g_ptr_array_index (head_names, j)) == 0) {
                                        found = TRUE;
                                        break;
                                }
                        }
                        match = found;
                }
        }

        g_ptr_array_free (config_names, FALSE);
        g_ptr_array_free (head_names, FALSE);

        return match;
}

static int
config_enabled_head_count (WlrConfig *config)
{
        int i;
        int count = 0;

        for (i = 0; config->outputs != NULL && config->outputs[i] != NULL; i++) {
                if (config->outputs[i]->enabled)
                        count++;
        }

        return count;
}

static void
emit_configuration (WlrConfig *config, GString *string)
{
        int i;

        g_string_append (string, "  <configuration>\n");

        for (i = 0; config->outputs != NULL && config->outputs[i] != NULL; i++) {
                WlrOutputInfo *output = config->outputs[i];
                char *escaped;

                g_string_append_printf (string, "      <head name=\"%s\">\n", output->name);

                if (output->vendor != NULL && *output->vendor != '\0') {
                        escaped = g_markup_escape_text (output->vendor, -1);
                        g_string_append_printf (string, "          <vendor>%s</vendor>\n", escaped);
                        g_free (escaped);
                }
                if (output->product != NULL && *output->product != '\0') {
                        escaped = g_markup_escape_text (output->product, -1);
                        g_string_append_printf (string, "          <product>%s</product>\n", escaped);
                        g_free (escaped);
                }
                if (output->serial != NULL && *output->serial != '\0') {
                        escaped = g_markup_escape_text (output->serial, -1);
                        g_string_append_printf (string, "          <serial>%s</serial>\n", escaped);
                        g_free (escaped);
                }

                if (output->enabled && output->width > 0) {
                        g_string_append (string, "          <enabled>yes</enabled>\n");
                        g_string_append_printf (string, "          <width>%d</width>\n", output->width);
                        g_string_append_printf (string, "          <height>%d</height>\n", output->height);
                        g_string_append_printf (string, "          <rate>%d</rate>\n", output->rate);
                        g_string_append_printf (string, "          <x>%d</x>\n", output->x);
                        g_string_append_printf (string, "          <y>%d</y>\n", output->y);

                        {
                                /* "%f" is locale-dependent and would write
                                 * e.g. "1,500000" in comma-decimal locales,
                                 * while the parser always reads with the
                                 * locale-independent g_ascii_strtod().  Use
                                 * g_ascii_formatd() so the file is always
                                 * written with a '.' decimal separator.
                                 */
                                char scale_buf[G_ASCII_DTOSTR_BUF_SIZE];
                                g_ascii_formatd (scale_buf, sizeof (scale_buf), "%f", output->scale);
                                g_string_append_printf (string, "          <scale>%s</scale>\n", scale_buf);
                        }
                        g_string_append_printf (string, "          <transform>%s</transform>\n", transform_to_name (output->transform));
                } else {
                        g_string_append (string, "          <enabled>no</enabled>\n");
                }

                g_string_append_printf (string, "          <adaptive_sync>%s</adaptive_sync>\n",
                                        output->adaptive_sync ? "yes" : "no");

                g_string_append (string, "      </head>\n");
        }

        g_string_append (string, "  </configuration>\n");
}

/* Persist the current output configuration to monitors-wayland.xml.  If
 * `config` is non-NULL it is used as-is; otherwise the live head state is
 * snapshotted (only used when nothing was applied and the state is known to
 * be accurate, e.g. after a rejected adaptive sync toggle).
 */
static void
save_current_configuration (MsdWlrandrManager *manager, WlrConfig *config)
{
        struct MsdWlrandrManagerPrivate *priv = manager->priv;
        WlrConfig *snapshot;
        WlrConfig **existing;
        GString *string;
        char *intended;
        char *backup;
        int i;
        GError *error = NULL;

        if (config == NULL)
                config = snapshot = snapshot_config (manager);
        else
                snapshot = NULL;

        intended = g_build_filename (g_get_user_config_dir (), CONFIG_INTENDED_BASENAME, NULL);
        backup = g_build_filename (g_get_user_config_dir (), CONFIG_BACKUP_BASENAME, NULL);

        string = g_string_new ("<monitors version=\"1\">\n");

        existing = configs_load_from_file (intended);
        if (existing != NULL) {
                for (i = 0; existing[i] != NULL; i++) {
                        if (config_matches_heads (existing[i], priv->heads))
                                continue;
                        emit_configuration (existing[i], string);
                }
                free_config_list (existing);
        }

        emit_configuration (config, string);
        g_string_append (string, "</monitors>\n");

        g_rename (intended, backup);

        if (!g_file_set_contents (intended, string->str, -1, &error)) {
                g_warning ("msd-wlrandr: could not save config to %s: %s", intended, error->message);
                g_error_free (error);
        } else {
                g_debug ("msd-wlrandr: saved configuration to %s", intended);
        }

        g_string_free (string, TRUE);
        g_free (intended);
        g_free (backup);
        if (snapshot != NULL)
                wlr_config_free (snapshot);
}

static gboolean
apply_configurations_from_list (MsdWlrandrManager *manager, WlrConfig **configs)
{
        int i;

        if (configs == NULL)
                return FALSE;

        for (i = 0; configs[i] != NULL; i++) {
                if (config_matches_heads (configs[i], manager->priv->heads)) {
                        if (config_enabled_head_count (configs[i]) == 0) {
                                g_debug ("msd-wlrandr: skipping configuration that would disable all heads");
                                return FALSE;
                        }
                        g_debug ("msd-wlrandr: found matching configuration, applying");
                        apply_configuration (manager, configs[i], FALSE, FALSE, NULL, NULL);
                        return TRUE;
                }
        }

        return FALSE;
}

static gboolean
apply_stored_configuration_at_startup (MsdWlrandrManager *manager)
{
        gboolean res;
        char *backup;
        char *intended;
        WlrConfig **configs;

        backup = g_build_filename (g_get_user_config_dir (), CONFIG_BACKUP_BASENAME, NULL);
        intended = g_build_filename (g_get_user_config_dir (), CONFIG_INTENDED_BASENAME, NULL);

        res = FALSE;

        configs = configs_load_from_file (intended);
        if (apply_configurations_from_list (manager, configs))
                res = TRUE;
        free_config_list (configs);

        if (!res) {
                configs = configs_load_from_file (backup);
                if (apply_configurations_from_list (manager, configs))
                        res = TRUE;
                free_config_list (configs);
        }

        g_free (backup);
        g_free (intended);

        return res;
}

static gboolean
apply_default_configuration_from_file (MsdWlrandrManager *manager)
{
        char *filename;
        WlrConfig **configs;
        gboolean res;

        filename = g_settings_get_string (manager->priv->settings, CONF_KEY_DEFAULT_CONFIGURATION_FILE);
        configs = configs_load_from_file (filename);
        res = apply_configurations_from_list (manager, configs);
        free_config_list (configs);
        g_free (filename);

        return res;
}

static void
auto_enable_external_monitors (MsdWlrandrManager *manager)
{
        WlrConfig *config;
        GList *l;
        gboolean changed = FALSE;

        if (!g_settings_get_boolean (manager->priv->settings, CONF_KEY_TURN_ON_EXTERNAL_MONITORS_AT_STARTUP))
                return;

        config = snapshot_config (manager);

        for (l = manager->priv->heads; l != NULL; l = l->next) {
                WlOutputHead *head = l->data;
                WlrOutputInfo *info = find_output_in_config (config, head->name);

                if (info != NULL && !info->enabled) {
                        info->enabled = TRUE;
                        fill_output_mode (head, info);
                        changed = TRUE;
                }
        }

        if (changed)
                apply_configuration (manager, config, FALSE, FALSE, NULL, NULL);

        wlr_config_free (config);
}

static void
apply_initial_config (MsdWlrandrManager *manager)
{
        g_debug ("msd-wlrandr: applying initial configuration");

        if (apply_stored_configuration_at_startup (manager))
                return;

        if (apply_default_configuration_from_file (manager))
                return;

        auto_enable_external_monitors (manager);
}

/* ------------------------------------------------------------------ *
 *  DBus interface                                                     *
 * ------------------------------------------------------------------ */

#define HEAD_INFO_TYPE  "a(ssbiiidbb(iiii)a(iiii))"
#define HEAD_SET_TYPE   "a(sbiiiiidib)"
#define HEAD_INFO_FORMAT  "(ssbiiidbb(iiii)a(iiii))"
#define HEAD_SET_FORMAT   "(sbiiiiidib)"
#define MODE_FORMAT       "(iiii)"

/* GVariant type strings cannot contain whitespace, so the compact forms
 * above are required on the wire.  These enums document what each field
 * means so the code below never has to be decoded from the string. */

/* One head as reported by GetConfiguration:
 * (name, description, enabled, x, y, transform, scale,
 *  adaptive_sync_supported, adaptive_sync,
 *  current_mode (width, height, refresh, preferred),
 *  modes (width, height, refresh, preferred) ...) */
typedef enum {
        HEAD_INFO_FIELD_NAME,                   /* s      output name */
        HEAD_INFO_FIELD_DESCRIPTION,            /* s      human-readable description */
        HEAD_INFO_FIELD_ENABLED,                /* b      output enabled */
        HEAD_INFO_FIELD_X,                      /* i      x position on the logical screen */
        HEAD_INFO_FIELD_Y,                      /* i      y position on the logical screen */
        HEAD_INFO_FIELD_TRANSFORM,              /* i      rotation (wl_output_transform) */
        HEAD_INFO_FIELD_SCALE,                  /* d      fractional scale factor */
        HEAD_INFO_FIELD_ADAPTIVE_SYNC_SUPPORTED, /* b     hardware supports adaptive sync */
        HEAD_INFO_FIELD_ADAPTIVE_SYNC,          /* b      adaptive sync currently active */
        HEAD_INFO_FIELD_CURRENT_MODE,           /* (iiii) the mode currently in use */
        HEAD_INFO_FIELD_MODES,                  /* a(iiii) all modes the output supports */
        HEAD_INFO_N_FIELDS
} HeadInfoField;

/* One entry of the modes array. */
typedef enum {
        MODE_FIELD_WIDTH,                       /* i      width in pixels */
        MODE_FIELD_HEIGHT,                      /* i      height in pixels */
        MODE_FIELD_REFRESH,                     /* i      refresh rate in mHz */
        MODE_FIELD_PREFERRED,                   /* i      1 if this is the preferred mode */
        MODE_N_FIELDS
} ModeField;

/* One head as sent to SetConfiguration:
 * (name, enabled, width, height, refresh, x, y, scale, transform,
 *  adaptive_sync) */
typedef enum {
        HEAD_SET_FIELD_NAME,                    /* s      output name */
        HEAD_SET_FIELD_ENABLED,                 /* b      output enabled */
        HEAD_SET_FIELD_WIDTH,                   /* i      desired width in pixels */
        HEAD_SET_FIELD_HEIGHT,                  /* i      desired height in pixels */
        HEAD_SET_FIELD_RATE,                    /* i      desired refresh rate in mHz */
        HEAD_SET_FIELD_X,                       /* i      desired x position */
        HEAD_SET_FIELD_Y,                       /* i      desired y position */
        HEAD_SET_FIELD_SCALE,                   /* d      desired fractional scale */
        HEAD_SET_FIELD_TRANSFORM,               /* i      desired rotation */
        HEAD_SET_FIELD_ADAPTIVE_SYNC,           /* b      desired adaptive sync state */
        HEAD_SET_N_FIELDS
} HeadSetField;

/* Notify interested parties (e.g. the display capplet) that the current
 * output configuration may have changed.  The same object path is
 * registered twice, once per interface, so the signal is emitted on both.
 */
static void
emit_configuration_changed (MsdWlrandrManager *manager)
{
        GDBusConnection *connection;

        if (manager->priv->connection == NULL)
                return;

        connection = manager->priv->connection;

        g_dbus_connection_emit_signal (connection,
                                       NULL,
                                       MSD_WLRANDR_DBUS_PATH,
                                       MSD_WLRANDR_DBUS_NAME,
                                       "ConfigurationChanged",
                                       NULL, NULL);
        g_dbus_connection_emit_signal (connection,
                                       NULL,
                                       MSD_WLRANDR_DBUS_PATH,
                                       MSD_WLRANDR_DBUS_NAME_2,
                                       "ConfigurationChanged",
                                       NULL, NULL);
}

/* Serialize the current state of the outputs so the display capplet can
 * render and edit it.  Field order and meaning are documented by the
 * HeadInfoField and ModeField enums above.
 */
static GVariant *
build_configuration_variant (MsdWlrandrManager *manager)
{
        GVariantBuilder builder;
        GList *l;

        g_variant_builder_init (&builder, G_VARIANT_TYPE (HEAD_INFO_TYPE));

        for (l = manager->priv->heads; l != NULL; l = l->next) {
                WlOutputHead *head = l->data;
                GVariantBuilder modes_builder;
                GList *m;
                int cur_w = 0, cur_h = 0, cur_rate = 0, cur_preferred = 0;

                g_variant_builder_init (&modes_builder, G_VARIANT_TYPE ("a" MODE_FORMAT));

                for (m = head->modes; m != NULL; m = m->next) {
                        WlOutputMode *mode = m->data;

                        g_variant_builder_add (&modes_builder, MODE_FORMAT,
                                               mode->width,
                                               mode->height,
                                               mode->refresh,
                                               mode->preferred ? 1 : 0);
                }

                if (head->enabled && head->current_mode != NULL) {
                        cur_w = head->current_mode->width;
                        cur_h = head->current_mode->height;
                        cur_rate = head->current_mode->refresh;
                        cur_preferred = head->current_mode->preferred ? 1 : 0;
                }

                g_variant_builder_add (&builder, HEAD_INFO_FORMAT,
                                       head->name != NULL ? head->name : "",
                                       head->description != NULL ? head->description : "",
                                       head->enabled,
                                       head->x,
                                       head->y,
                                       head->transform,
                                       head->scale,
                                       head->adaptive_sync_supported == 1,
                                       head->adaptive_sync,
                                       cur_w, cur_h, cur_rate, cur_preferred,
                                       &modes_builder);
        }

        return g_variant_builder_end (&builder);
}

/* Deserialize a configuration sent by the display capplet.  Field order
 * and meaning are documented by the HeadSetField enum above.
 */
static WlrConfig *
parse_configuration_from_variant (GVariant *configuration)
{
        WlrConfig *config;
        GVariantIter iter;
        GPtrArray *outputs;

        if (configuration == NULL ||
            !g_variant_is_of_type (configuration, G_VARIANT_TYPE (HEAD_SET_TYPE)))
                return NULL;

        outputs = g_ptr_array_new ();

        g_variant_iter_init (&iter, configuration);
        while (TRUE) {
                GVariant *head_var;
                const char *name;
                gboolean enabled;
                gboolean adaptive_sync;
                int width, height, rate, x, y, transform;
                double scale;
                WlrOutputInfo *info;

                head_var = g_variant_iter_next_value (&iter);
                if (head_var == NULL)
                        break;

                g_variant_get_child (head_var, HEAD_SET_FIELD_NAME,
                                     "&s", &name);
                g_variant_get_child (head_var, HEAD_SET_FIELD_ENABLED,
                                     "b", &enabled);
                g_variant_get_child (head_var, HEAD_SET_FIELD_WIDTH,
                                     "i", &width);
                g_variant_get_child (head_var, HEAD_SET_FIELD_HEIGHT,
                                     "i", &height);
                g_variant_get_child (head_var, HEAD_SET_FIELD_RATE,
                                     "i", &rate);
                g_variant_get_child (head_var, HEAD_SET_FIELD_X,
                                     "i", &x);
                g_variant_get_child (head_var, HEAD_SET_FIELD_Y,
                                     "i", &y);
                g_variant_get_child (head_var, HEAD_SET_FIELD_SCALE,
                                     "d", &scale);
                g_variant_get_child (head_var, HEAD_SET_FIELD_TRANSFORM,
                                     "i", &transform);
                g_variant_get_child (head_var, HEAD_SET_FIELD_ADAPTIVE_SYNC,
                                     "b", &adaptive_sync);

                /* wlr_output_info_new () duplicates the name, so it is
                 * safe to release the tuple afterwards. */
                info = wlr_output_info_new (name);
                info->enabled = enabled;
                info->width = width;
                info->height = height;
                info->rate = rate;
                info->x = x;
                info->y = y;
                info->scale = scale;
                info->transform = transform;
                info->adaptive_sync = adaptive_sync;

                g_variant_unref (head_var);

                g_ptr_array_add (outputs, info);
        }

        if (outputs->len == 0) {
                g_ptr_array_free (outputs, TRUE);
                return NULL;
        }

        g_ptr_array_add (outputs, NULL);

        config = g_new0 (WlrConfig, 1);
        config->outputs = (WlrOutputInfo **) g_ptr_array_free (outputs, FALSE);

        return config;
}

/* Pick the head whose adaptive sync state the caller wants to change, if
 * any.  The zwlr-output-management protocol only lets the plugin set
 * adaptive sync on the heads it explicitly marks, so only the head whose
 * requested value differs from the current one is applied.
 */
static WlOutputHead *
find_adaptive_sync_head_to_set (MsdWlrandrManager *manager,
                                WlrConfig         *config)
{
        GList *l;

        for (l = manager->priv->heads; l != NULL; l = l->next) {
                WlOutputHead *head = l->data;
                WlrOutputInfo *info = find_output_in_config (config, head->name);

                if (info != NULL && info->enabled &&
                    info->adaptive_sync != head->adaptive_sync)
                        return head;
        }

        return NULL;
}

static gboolean
msd_wlrandr_manager_set_configuration (MsdWlrandrManager *manager,
                                       GVariant          *configuration,
                                       GError           **error)
{
        WlrConfig *config;
        WlrConfig *previous;
        WlOutputHead *adaptive_sync_head;
        gboolean res;

        config = parse_configuration_from_variant (configuration);
        if (config == NULL) {
                g_set_error (error, G_DBUS_ERROR, G_DBUS_ERROR_INVALID_ARGS,
                             "The supplied configuration is invalid");
                return FALSE;
        }

        if (config_enabled_head_count (config) == 0) {
                g_set_error (error, G_DBUS_ERROR, G_DBUS_ERROR_FAILED,
                             "The last active output cannot be turned off");
                wlr_config_free (config);
                return FALSE;
        }

        previous = snapshot_config (manager);

        adaptive_sync_head = find_adaptive_sync_head_to_set (manager, config);

        res = apply_configuration (manager, config, TRUE, TRUE, previous, adaptive_sync_head);
        if (!res) {
                g_set_error (error, G_DBUS_ERROR, G_DBUS_ERROR_FAILED,
                             "Could not apply the display configuration");
                wlr_config_free (config);
                wlr_config_free (previous);
                return FALSE;
        }

        wlr_config_free (config);

        return TRUE;
}

static gboolean
msd_wlrandr_manager_apply_configuration (MsdWlrandrManager *manager,
                                         GError          **error)
{
        if (!apply_stored_configuration_at_startup (manager)) {
                g_set_error (error, G_DBUS_ERROR, G_DBUS_ERROR_FAILED,
                             "No stored configuration applies to the current outputs");
                return FALSE;
        }

        return TRUE;
}

static void
handle_method_call (GDBusConnection       *connection,
                    const gchar           *sender,
                    const gchar           *object_path,
                    const gchar           *interface_name,
                    const gchar           *method_name,
                    GVariant              *parameters,
                    GDBusMethodInvocation *invocation,
                    gpointer               user_data)
{
        MsdWlrandrManager *manager = (MsdWlrandrManager *) user_data;
        g_autoptr (GError) error = NULL;

        g_debug ("Calling method '%s' for wlrandr", method_name);

        if (g_strcmp0 (method_name, "GetConfiguration") == 0) {
                GVariant *configuration;

                configuration = build_configuration_variant (manager);
                g_dbus_method_invocation_return_value (invocation,
                                                       g_variant_new ("(@a(ssbiiidbb(iiii)a(iiii)))",
                                                                      configuration));
        } else if (g_strcmp0 (method_name, "SetConfiguration") == 0) {
                g_autoptr (GVariant) configuration = NULL;

                g_variant_get (parameters, "(@a(sbiiiiidib))", &configuration);
                if (!msd_wlrandr_manager_set_configuration (manager, configuration, &error))
                        g_dbus_method_invocation_return_gerror (invocation, error);
                else
                        g_dbus_method_invocation_return_value (invocation, NULL);
        } else if (g_strcmp0 (method_name, "ApplyConfiguration") == 0) {
                if (!msd_wlrandr_manager_apply_configuration (manager, &error))
                        g_dbus_method_invocation_return_gerror (invocation, error);
                else
                        g_dbus_method_invocation_return_value (invocation, NULL);
        }
}

static void
handle_method_call2 (GDBusConnection       *connection,
                     const gchar           *sender,
                     const gchar           *object_path,
                     const gchar           *interface_name,
                     const gchar           *method_name,
                     GVariant              *parameters,
                     GDBusMethodInvocation *invocation,
                     gpointer               user_data)
{
        MsdWlrandrManager *manager = (MsdWlrandrManager *) user_data;
        g_autoptr (GError) error = NULL;

        g_debug ("Calling method '%s' for wlrandr", method_name);

        if (g_strcmp0 (method_name, "GetConfiguration") == 0) {
                GVariant *configuration;

                configuration = build_configuration_variant (manager);
                g_dbus_method_invocation_return_value (invocation,
                                                       g_variant_new ("(@a(ssbiiidbb(iiii)a(iiii)))",
                                                                      configuration));
        } else if (g_strcmp0 (method_name, "SetConfiguration") == 0) {
                g_autoptr (GVariant) configuration = NULL;

                g_variant_get (parameters, "(@a(sbiiiiidib))", &configuration);
                if (!msd_wlrandr_manager_set_configuration (manager, configuration, &error))
                        g_dbus_method_invocation_return_gerror (invocation, error);
                else
                        g_dbus_method_invocation_return_value (invocation, NULL);
        } else if (g_strcmp0 (method_name, "ApplyConfiguration") == 0) {
                gint64 parent_window_id;
                gint64 timestamp;

                g_variant_get (parameters, "(xx)", &parent_window_id, &timestamp);
                /* Wayland has no X window ids or X timestamps; the arguments
                 * are accepted only for API compatibility with xrandr.
                 */
                (void) parent_window_id;
                (void) timestamp;

                if (!msd_wlrandr_manager_apply_configuration (manager, &error))
                        g_dbus_method_invocation_return_gerror (invocation, error);
                else
                        g_dbus_method_invocation_return_value (invocation, NULL);
        }
}

static const GDBusInterfaceVTable interface_vtable =
{
        .method_call = handle_method_call,
};

static const GDBusInterfaceVTable interface_vtable2 =
{
        .method_call = handle_method_call2,
};

static void
on_bus_gotten (GObject           *source_object,
               GAsyncResult      *res,
               MsdWlrandrManager *manager)
{
        GDBusConnection *connection;
        GError *error = NULL;

        connection = g_bus_get_finish (res, &error);
        if (connection == NULL) {
                if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
                        g_warning ("Could not get session bus: %s", error->message);
                g_error_free (error);
                return;
        }
        manager->priv->connection = connection;

        if (manager->priv->introspection_data == NULL) {
                g_warning ("Could not parse wlrandr D-Bus introspection XML");
                return;
        }

        manager->priv->registration_id =
                g_dbus_connection_register_object (connection,
                                                   MSD_WLRANDR_DBUS_PATH,
                                                   manager->priv->introspection_data->interfaces[0],
                                                   &interface_vtable,
                                                   manager,
                                                   NULL,
                                                   NULL);
        manager->priv->registration_id_2 =
                g_dbus_connection_register_object (connection,
                                                   MSD_WLRANDR_DBUS_PATH,
                                                   manager->priv->introspection_data->interfaces[1],
                                                   &interface_vtable2,
                                                   manager,
                                                   NULL,
                                                   NULL);

        manager->priv->owner_id = g_bus_own_name_on_connection (manager->priv->connection,
                                                                MSD_DBUS_NAME,
                                                                G_BUS_NAME_OWNER_FLAGS_ALLOW_REPLACEMENT,
                                                                NULL, NULL, NULL, NULL);
}

static void
register_manager_dbus (MsdWlrandrManager *manager)
{
        manager->priv->introspection_data = g_dbus_node_info_new_for_xml (introspection_xml, NULL);
        if (manager->priv->introspection_data == NULL) {
                g_warning ("Could not parse wlrandr D-Bus introspection XML");
                return;
        }
        manager->priv->bus_cancellable = g_cancellable_new ();
        g_bus_get (G_BUS_TYPE_SESSION,
                   manager->priv->bus_cancellable,
                   (GAsyncReadyCallback) on_bus_gotten,
                   manager);
}

/* ------------------------------------------------------------------ *
 *  Menu actions                                                       *
 * ------------------------------------------------------------------ */

static void
monitor_activate_cb (GtkCheckMenuItem *item, MsdWlrandrManager *manager)
{
        WlOutputHead *head = g_object_get_data (G_OBJECT (item), "head");
        gboolean on = gtk_check_menu_item_get_active (item);
        WlrConfig *config;
        WlrOutputInfo *info;

        if (head == NULL)
                return;

        if (head->enabled == on)
                return;

        if (!on && enabled_head_count (manager) <= 1) {
                gulong activate_id;

                g_warning ("msd-wlrandr: refusing to disable the last output");
                notify_config_error (_("The last active output cannot be turned off"));

                activate_id = (gulong) GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (item), "activate-id"));
                if (activate_id > 0) {
                        g_signal_handler_block (item, activate_id);
                        gtk_check_menu_item_set_active (item, TRUE);
                        g_signal_handler_unblock (item, activate_id);
                }
                return;
        }

        config = snapshot_config (manager);
        info = find_output_in_config (config, head->name);
        if (info == NULL) {
                wlr_config_free (config);
                return;
        }

        info->enabled = on;
        if (on) {
                fill_output_mode (head, info);
                if (info->x == 0 && info->y == 0) {
                        GList *l;
                        int max_x = 0;

                        for (l = manager->priv->heads; l != NULL; l = l->next) {
                                WlOutputHead *h = l->data;
                                if (h->enabled && h->current_mode != NULL) {
                                        int right = h->x + h->current_mode->width;
                                        if (right > max_x)
                                                max_x = right;
                                }
                        }
                        info->x = max_x;
                        info->y = 0;
                }
        }

        apply_configuration (manager, config, TRUE, FALSE, NULL, NULL);
        wlr_config_free (config);
}

static void
mode_activate_cb (GtkCheckMenuItem *item, MsdWlrandrManager *manager)
{
        WlOutputHead *head = g_object_get_data (G_OBJECT (item), "head");
        WlOutputMode *mode = g_object_get_data (G_OBJECT (item), "mode");
        WlrConfig *config;
        WlrOutputInfo *info;

        if (head == NULL || mode == NULL)
                return;

        if (!gtk_check_menu_item_get_active (item))
                return;

        config = snapshot_config (manager);
        info = find_output_in_config (config, head->name);
        if (info == NULL) {
                wlr_config_free (config);
                return;
        }

        info->width = mode->width;
        info->height = mode->height;
        info->rate = mode->refresh;

        apply_configuration (manager, config, TRUE, FALSE, NULL, NULL);
        wlr_config_free (config);
}

static void
rotation_activate_cb (GtkCheckMenuItem *item, MsdWlrandrManager *manager)
{
        WlOutputHead *head = g_object_get_data (G_OBJECT (item), "head");
        int transform = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (item), "transform"));
        WlrConfig *config;
        WlrOutputInfo *info;

        if (head == NULL)
                return;

        if (!gtk_check_menu_item_get_active (item))
                return;

        config = snapshot_config (manager);
        info = find_output_in_config (config, head->name);
        if (info == NULL) {
                wlr_config_free (config);
                return;
        }

        info->transform = transform;

        apply_configuration (manager, config, TRUE, FALSE, NULL, NULL);
        wlr_config_free (config);
}

static void
adaptive_sync_activate_cb (GtkCheckMenuItem *item, MsdWlrandrManager *manager)
{
        WlOutputHead *head = g_object_get_data (G_OBJECT (item), "head");
        gboolean on = gtk_check_menu_item_get_active (item);
        WlrConfig *config;
        WlrOutputInfo *info;

        if (head == NULL)
                return;

        if (head->adaptive_sync_supported != 1) {
                gulong activate_id;

                g_debug ("msd-wlrandr: adaptive sync support not confirmed on %s", head->name);

                activate_id = (gulong) GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (item), "activate-id"));
                if (activate_id > 0) {
                        g_signal_handler_block (item, activate_id);
                        gtk_check_menu_item_set_active (item, head->adaptive_sync);
                        g_signal_handler_unblock (item, activate_id);
                }
                return;
        }

        config = snapshot_config (manager);
        info = find_output_in_config (config, head->name);
        if (info == NULL) {
                wlr_config_free (config);
                return;
        }

        info->adaptive_sync = on;

        apply_configuration (manager, config, TRUE, FALSE, NULL, head);
        wlr_config_free (config);
}

static void
run_display_capplet (MsdWlrandrManager *manager)
{
        GError *error = NULL;

        if (!mate_gdk_spawn_command_line_on_screen (gdk_screen_get_default (),
                                                    MSD_WLRANDR_DISPLAY_CAPPLET, &error)) {
                g_warning ("Could not spawn %s: %s", MSD_WLRANDR_DISPLAY_CAPPLET, error->message);
                g_error_free (error);
        }
}

static void
configure_display_cb (GtkMenuItem *item, MsdWlrandrManager *manager)
{
        run_display_capplet (manager);
}

/* ------------------------------------------------------------------ *
 *  SNI menu                                                           *
 * ------------------------------------------------------------------ */

static void
append_output_title_item (GtkMenu *menu, WlOutputHead *head)
{
        GtkWidget *item;
        GtkWidget *box;
        GtkWidget *image;
        GtkWidget *label;
        char *markup;
        char *display_name;

        item = gtk_menu_item_new ();
        box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
        image = gtk_image_new_from_icon_name ("computer", GTK_ICON_SIZE_MENU);
        gtk_container_add (GTK_CONTAINER (box), image);

        display_name = head->description != NULL && *head->description != '\0'
                ? g_strdup (head->description) : g_strdup (head->name);

        markup = g_markup_printf_escaped ("<b>%s</b>", display_name);
        label = gtk_label_new (NULL);
        gtk_label_set_markup (GTK_LABEL (label), markup);
        gtk_widget_set_margin_start (label, 6);
        gtk_widget_set_margin_end (label, 6);
        gtk_container_add (GTK_CONTAINER (box), label);

        gtk_container_add (GTK_CONTAINER (item), box);
        gtk_widget_set_sensitive (item, FALSE);
        gtk_widget_show_all (item);
        gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

        g_free (markup);
        g_free (display_name);
}

static void
append_enable_item (GtkMenu *menu, MsdWlrandrManager *manager, WlOutputHead *head)
{
        GtkWidget *item;
        gulong activate_id;

        item = gtk_check_menu_item_new ();

        if (head->enabled) {
                gtk_menu_item_set_label (GTK_MENU_ITEM (item), _("ON"));
                gtk_widget_set_tooltip_text (item, _("Turn this monitor off"));
        } else {
                gtk_menu_item_set_label (GTK_MENU_ITEM (item), _("OFF"));
                gtk_widget_set_tooltip_text (item, _("Turn this monitor on"));
        }

        g_object_set_data (G_OBJECT (item), "head", head);

        activate_id = g_signal_connect (item, "activate", G_CALLBACK (monitor_activate_cb), manager);
        g_object_set_data (G_OBJECT (item), "activate-id", GUINT_TO_POINTER ((guint) activate_id));

        g_signal_handler_block (item, activate_id);
        gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (item), head->enabled);
        g_signal_handler_unblock (item, activate_id);

        if (head->enabled && enabled_head_count (manager) <= 1)
                gtk_widget_set_sensitive (item, FALSE);

        gtk_widget_show_all (item);
        gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
}

static void
append_resolution_submenu (GtkMenu *menu, MsdWlrandrManager *manager, WlOutputHead *head)
{
        GtkWidget *submenu;
        GtkWidget *item;
        GList *l;
        GSList *group = NULL;

        submenu = gtk_menu_new ();

        for (l = head->modes; l != NULL; l = l->next) {
                WlOutputMode *mode = l->data;
                gboolean swapped = head->transform == WL_OUTPUT_TRANSFORM_90 ||
                                   head->transform == WL_OUTPUT_TRANSFORM_270;
                int width = swapped ? mode->height : mode->width;
                int height = swapped ? mode->width : mode->height;
                char *label;
                gulong activate_id;

                if (mode->refresh > 0)
                        label = g_strdup_printf ("%dx%d @ %g Hz", width, height, mode->refresh / 1000.0);
                else
                        label = g_strdup_printf ("%dx%d", width, height);

                item = gtk_radio_menu_item_new_with_label (group, label);
                group = gtk_radio_menu_item_get_group (GTK_RADIO_MENU_ITEM (item));
                g_free (label);

                g_object_set_data (G_OBJECT (item), "head", head);
                g_object_set_data (G_OBJECT (item), "mode", mode);

                if (mode->preferred)
                        gtk_widget_set_tooltip_text (item, _("Preferred resolution"));

                if (head->current_mode == mode) {
                        activate_id = g_signal_connect (item, "toggled", G_CALLBACK (mode_activate_cb), manager);
                        g_signal_handler_block (item, activate_id);
                        gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (item), TRUE);
                        g_signal_handler_unblock (item, activate_id);
                } else {
                        g_signal_connect (item, "toggled", G_CALLBACK (mode_activate_cb), manager);
                }

                gtk_widget_show_all (item);
                gtk_menu_shell_append (GTK_MENU_SHELL (submenu), item);
        }

        item = gtk_menu_item_new_with_label (_("Resolution"));
        gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), submenu);
        gtk_widget_show (item);
        gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
}

static void
append_rotation_submenu (GtkMenu *menu, MsdWlrandrManager *manager, WlOutputHead *head)
{
        typedef struct {
                int          transform;
                const char  *name;
        } RotationInfo;

        static const RotationInfo rotations[] = {
                { WL_OUTPUT_TRANSFORM_NORMAL, N_("Normal") },
                { WL_OUTPUT_TRANSFORM_90,     N_("Left") },
                { WL_OUTPUT_TRANSFORM_270,    N_("Right") },
                { WL_OUTPUT_TRANSFORM_180,    N_("Upside Down") },
        };

        GtkWidget *submenu;
        GtkWidget *item;
        GSList *group = NULL;
        int i;

        submenu = gtk_menu_new ();

        for (i = 0; i < G_N_ELEMENTS (rotations); i++) {
                gulong activate_id;

                item = gtk_radio_menu_item_new_with_label (group, _(rotations[i].name));
                group = gtk_radio_menu_item_get_group (GTK_RADIO_MENU_ITEM (item));

                if (!head->enabled)
                        gtk_widget_set_sensitive (item, FALSE);

                g_object_set_data (G_OBJECT (item), "head", head);
                g_object_set_data (G_OBJECT (item), "transform", GINT_TO_POINTER (rotations[i].transform));

                if (head->transform == rotations[i].transform) {
                        activate_id = g_signal_connect (item, "toggled", G_CALLBACK (rotation_activate_cb), manager);
                        g_signal_handler_block (item, activate_id);
                        gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (item), TRUE);
                        g_signal_handler_unblock (item, activate_id);
                } else {
                        g_signal_connect (item, "toggled", G_CALLBACK (rotation_activate_cb), manager);
                }

                gtk_widget_show_all (item);
                gtk_menu_shell_append (GTK_MENU_SHELL (submenu), item);
        }

        item = gtk_menu_item_new_with_label (_("Rotation"));
        gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), submenu);
        gtk_widget_show (item);
        gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
}

static void
append_adaptive_sync_item (GtkMenu *menu, MsdWlrandrManager *manager, WlOutputHead *head)
{
        GtkWidget *item;
        gulong activate_id;
        gboolean unavailable;

        if (head_is_wayland_backend (head) && head->adaptive_sync_supported == -1)
                head->adaptive_sync_supported = 0;

        unavailable = !head->enabled || head_is_wayland_backend (head) || head->adaptive_sync_supported != 1;

        item = gtk_check_menu_item_new_with_label (_("Adaptive Sync (VRR)"));
        g_object_set_data (G_OBJECT (item), "head", head);

        activate_id = g_signal_connect (item, "activate", G_CALLBACK (adaptive_sync_activate_cb), manager);
        g_object_set_data (G_OBJECT (item), "activate-id", GUINT_TO_POINTER ((guint) activate_id));

        g_signal_handler_block (item, activate_id);
        if (unavailable)
                gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (item), FALSE);
        else
                gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (item), head->adaptive_sync);
        g_signal_handler_unblock (item, activate_id);

        if (unavailable)
                gtk_widget_set_sensitive (item, FALSE);

        gtk_widget_show_all (item);
        gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
}

static void
append_configure_item (GtkMenu *menu, MsdWlrandrManager *manager)
{
        GtkWidget *item;
        GtkWidget *box;
        GtkWidget *image;
        GtkWidget *label;
        GSettings *icon_settings;

        item = gtk_menu_item_new ();
        box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
        image = gtk_image_new_from_icon_name ("preferences-system", GTK_ICON_SIZE_MENU);
        label = gtk_label_new_with_mnemonic (_("_Configure Display Settings…"));

        icon_settings = g_settings_new ("org.mate.interface");
        if (g_settings_get_boolean (icon_settings, "menus-have-icons"))
                gtk_container_add (GTK_CONTAINER (box), image);
        g_object_unref (icon_settings);

        gtk_container_add (GTK_CONTAINER (box), label);
        gtk_container_add (GTK_CONTAINER (item), box);

        g_signal_connect (item, "activate", G_CALLBACK (configure_display_cb), manager);

        gtk_widget_show_all (item);
        gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
}

static void
rebuild_menu (MsdWlrandrManager *manager)
{
        struct MsdWlrandrManagerPrivate *priv = manager->priv;
        GtkWidget *menu;
        GList *l;

        if (priv->indicator == NULL)
                return;

        menu = gtk_menu_new ();

        for (l = priv->heads; l != NULL; l = l->next) {
                WlOutputHead *head = l->data;

                append_output_title_item (GTK_MENU (menu), head);
                append_enable_item (GTK_MENU (menu), manager, head);
                append_resolution_submenu (GTK_MENU (menu), manager, head);
                append_rotation_submenu (GTK_MENU (menu), manager, head);
                append_adaptive_sync_item (GTK_MENU (menu), manager, head);
        }

        gtk_menu_shell_append (GTK_MENU_SHELL (menu), gtk_separator_menu_item_new ());

        append_configure_item (GTK_MENU (menu), manager);

        gtk_widget_show_all (menu);

        if (priv->menu != NULL)
                gtk_widget_destroy (priv->menu);
        priv->menu = menu;

        app_indicator_set_menu (priv->indicator, GTK_MENU (menu));
}

/* ------------------------------------------------------------------ *
 *  GSettings callbacks                                                *
 * ------------------------------------------------------------------ */

static void
start_or_stop_icon (MsdWlrandrManager *manager)
{
        gboolean show = g_settings_get_boolean (manager->priv->settings, CONF_KEY_SHOW_NOTIFICATION_ICON);

        if (show) {
                app_indicator_set_status (manager->priv->indicator, APP_INDICATOR_STATUS_ACTIVE);
                rebuild_menu (manager);
        } else {
                app_indicator_set_status (manager->priv->indicator, APP_INDICATOR_STATUS_PASSIVE);
        }
}

static void
on_config_changed (GSettings *settings, gchar *key, MsdWlrandrManager *manager)
{
        if (strcmp (key, CONF_KEY_SHOW_NOTIFICATION_ICON) == 0)
                start_or_stop_icon (manager);
}

/* ------------------------------------------------------------------ *
 *  Object lifecycle                                                    *
 * ------------------------------------------------------------------ */

G_DEFINE_TYPE_WITH_PRIVATE (MsdWlrandrManager, msd_wlrandr_manager, G_TYPE_OBJECT)

static void
msd_wlrandr_manager_finalize (GObject *object)
{
        MsdWlrandrManager *manager = MSD_WLRANDR_MANAGER (object);
        struct MsdWlrandrManagerPrivate *priv = manager->priv;
        GList *l;

        if (priv->owner_id > 0) {
                g_bus_unown_name (priv->owner_id);
                priv->owner_id = 0;
        }

        if (priv->connection != NULL) {
                if (priv->registration_id > 0)
                        g_dbus_connection_unregister_object (priv->connection, priv->registration_id);
                if (priv->registration_id_2 > 0)
                        g_dbus_connection_unregister_object (priv->connection, priv->registration_id_2);
                g_object_unref (priv->connection);
                priv->connection = NULL;
        }

        if (priv->introspection_data != NULL) {
                g_dbus_node_info_unref (priv->introspection_data);
                priv->introspection_data = NULL;
        }

        if (priv->bus_cancellable != NULL) {
                g_cancellable_cancel (priv->bus_cancellable);
                g_object_unref (priv->bus_cancellable);
                priv->bus_cancellable = NULL;
        }

        if (priv->settings != NULL) {
                if (priv->icon_changed_id > 0)
                        g_signal_handler_disconnect (priv->settings, priv->icon_changed_id);
                g_object_unref (priv->settings);
        }

        if (priv->indicator != NULL)
                g_object_unref (priv->indicator);
        if (priv->menu != NULL)
                gtk_widget_destroy (priv->menu);
        if (priv->confirm_dialog != NULL)
                gtk_widget_destroy (priv->confirm_dialog);

        for (l = priv->heads; l != NULL; l = l->next)
                wlr_output_head_free (l->data);
        g_list_free (priv->heads);

        if (priv->manager != NULL)
                zwlr_output_manager_v1_destroy (priv->manager);
        if (priv->registry != NULL)
                wl_registry_destroy (priv->registry);

        if (notify_is_initted ())
                notify_uninit ();

        G_OBJECT_CLASS (msd_wlrandr_manager_parent_class)->finalize (object);
}

static void
msd_wlrandr_manager_init (MsdWlrandrManager *manager)
{
        manager->priv = msd_wlrandr_manager_get_instance_private (manager);
}

static void
msd_wlrandr_manager_class_init (MsdWlrandrManagerClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);

        object_class->finalize = msd_wlrandr_manager_finalize;
}

MsdWlrandrManager *
msd_wlrandr_manager_new (void)
{
        return g_object_new (MSD_TYPE_WLRANDR_MANAGER, NULL);
}

gboolean
msd_wlrandr_manager_start (MsdWlrandrManager *manager, GError **error)
{
        struct MsdWlrandrManagerPrivate *priv = manager->priv;
        GdkDisplay *gdk_display;
        GSettings *icon_settings;

        g_return_val_if_fail (MSD_IS_WLRANDR_MANAGER (manager), FALSE);

        if (priv->started)
                return TRUE;

        if (!GDK_IS_WAYLAND_DISPLAY (gdk_display_get_default ())) {
                g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                             "The wlrandr plugin requires a Wayland display");
                return FALSE;
        }

        gdk_display = gdk_display_get_default ();
        priv->display = gdk_wayland_display_get_wl_display (gdk_display);
        if (priv->display == NULL) {
                g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                             "Unable to get the Wayland display");
                return FALSE;
        }

        g_debug ("Starting wlrandr manager");

        priv->settings = g_settings_new (CONF_SCHEMA);

        notify_init ("mate-settings-daemon");

        priv->registry = wl_display_get_registry (priv->display);
        wl_registry_add_listener (priv->registry, &registry_listener, manager);
        wl_display_flush (priv->display);

        priv->indicator = app_indicator_new ("org.mate.settingsdaemon.wlrandr",
                                             MSD_WLRANDR_ICON_NAME,
                                             APP_INDICATOR_CATEGORY_APPLICATION_STATUS);
        app_indicator_set_title (priv->indicator, _("Configure display settings"));

        icon_settings = g_settings_new ("org.mate.interface");
        if (g_settings_get_boolean (icon_settings, "menus-have-icons"))
                app_indicator_set_icon_theme_path (priv->indicator, NULL);
        g_object_unref (icon_settings);

        priv->icon_changed_id = g_signal_connect (priv->settings, "changed::" CONF_KEY_SHOW_NOTIFICATION_ICON,
                                                  G_CALLBACK (on_config_changed), manager);

        start_or_stop_icon (manager);

        register_manager_dbus (manager);

        priv->started = TRUE;

        return TRUE;
}

void
msd_wlrandr_manager_stop (MsdWlrandrManager *manager)
{
        struct MsdWlrandrManagerPrivate *priv;
        GList *l;

        g_return_if_fail (MSD_IS_WLRANDR_MANAGER (manager));

        if (!manager->priv->started)
                return;

        g_debug ("Stopping wlrandr manager");

        priv = manager->priv;

        if (priv->owner_id > 0) {
                g_bus_unown_name (priv->owner_id);
                priv->owner_id = 0;
        }

        if (priv->connection != NULL) {
                if (priv->registration_id > 0) {
                        g_dbus_connection_unregister_object (priv->connection, priv->registration_id);
                        priv->registration_id = 0;
                }
                if (priv->registration_id_2 > 0) {
                        g_dbus_connection_unregister_object (priv->connection, priv->registration_id_2);
                        priv->registration_id_2 = 0;
                }
                g_object_unref (priv->connection);
                priv->connection = NULL;
        }

        if (priv->introspection_data != NULL) {
                g_dbus_node_info_unref (priv->introspection_data);
                priv->introspection_data = NULL;
        }

        if (priv->bus_cancellable != NULL) {
                g_cancellable_cancel (priv->bus_cancellable);
                g_object_unref (priv->bus_cancellable);
                priv->bus_cancellable = NULL;
        }

        if (priv->settings != NULL) {
                if (priv->icon_changed_id > 0)
                        g_signal_handler_disconnect (priv->settings, priv->icon_changed_id);
                g_object_unref (priv->settings);
                priv->settings = NULL;
        }

        if (priv->indicator != NULL) {
                app_indicator_set_status (priv->indicator, APP_INDICATOR_STATUS_PASSIVE);
                g_object_unref (priv->indicator);
                priv->indicator = NULL;
        }

        if (priv->menu != NULL) {
                gtk_widget_destroy (priv->menu);
                priv->menu = NULL;
        }

        if (priv->confirm_dialog != NULL) {
                gtk_widget_destroy (priv->confirm_dialog);
                priv->confirm_dialog = NULL;
        }

        if (priv->manager != NULL) {
                zwlr_output_manager_v1_stop (priv->manager);
                zwlr_output_manager_v1_destroy (priv->manager);
                priv->manager = NULL;
        }

        if (priv->registry != NULL) {
                wl_registry_destroy (priv->registry);
                priv->registry = NULL;
        }

        for (l = priv->heads; l != NULL; l = l->next)
                wlr_output_head_free (l->data);
        g_list_free (priv->heads);
        priv->heads = NULL;

        if (notify_is_initted ())
                notify_uninit ();

        priv->started = FALSE;
}
