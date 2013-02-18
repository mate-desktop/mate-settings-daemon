/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007 William Jon McCann <mccann@jhu.edu>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */

#include "config.h"

#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include <locale.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include <X11/keysym.h>
#include <gio/gio.h>
#include <dconf.h>

#include "mate-settings-profile.h"
#include "msd-keybindings-manager.h"
#include "dconf-util.h"

#include "msd-keygrab.h"
#include "eggaccelerators.h"

#define GSETTINGS_KEYBINDINGS_DIR "/org/mate/desktop/keybindings/"
#define CUSTOM_KEYBINDING_SCHEMA "org.mate.control-center.keybinding"

#define MSD_KEYBINDINGS_MANAGER_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), MSD_TYPE_KEYBINDINGS_MANAGER, MsdKeybindingsManagerPrivate))

typedef struct {
        char *binding_str;
        char *action;
        char *settings_path;
        Key   key;
        Key   previous_key;
} Binding;

struct MsdKeybindingsManagerPrivate
{
        DConfClient *client;
        GSList      *binding_list;
        GSList      *screens;
};

static void     msd_keybindings_manager_class_init  (MsdKeybindingsManagerClass *klass);
static void     msd_keybindings_manager_init        (MsdKeybindingsManager      *keybindings_manager);
static void     msd_keybindings_manager_finalize    (GObject             *object);

G_DEFINE_TYPE (MsdKeybindingsManager, msd_keybindings_manager, G_TYPE_OBJECT)

static gpointer manager_object = NULL;

static GSList *
get_screens_list (void)
{
        GdkDisplay *display = gdk_display_get_default();
        int         n_screens;
        GSList     *list = NULL;
        int         i;

        n_screens = gdk_display_get_n_screens (display);

        if (n_screens == 1) {
                list = g_slist_append (list, gdk_screen_get_default ());
        } else {
                for (i = 0; i < n_screens; i++) {
                        GdkScreen *screen;

                        screen = gdk_display_get_screen (display, i);
                        if (screen != NULL) {
                                list = g_slist_prepend (list, screen);
                        }
                }
                list = g_slist_reverse (list);
        }

        return list;
}

static gboolean
parse_binding (Binding *binding)
{
        gboolean success;

        g_return_val_if_fail (binding != NULL, FALSE);

        binding->key.keysym = 0;
        binding->key.state = 0;
        g_free (binding->key.keycodes);
        binding->key.keycodes = NULL;

        if (binding->binding_str == NULL ||
            binding->binding_str[0] == '\0' ||
            g_strcmp0 (binding->binding_str, "Disabled") == 0 ||
            g_strcmp0 (binding->binding_str, "disabled") == 0 ) {
                return FALSE;
        }

        success = egg_accelerator_parse_virtual (binding->binding_str,
                                                 &binding->key.keysym,
                                                 &binding->key.keycodes,
                                                 &binding->key.state);

        if (!success)
            g_warning (_("Key binding (%s) is invalid"), binding->settings_path);

        return success;
}

static gint
compare_bindings (gconstpointer a,
                  gconstpointer b)
{
        Binding *key_a = (Binding *) a;
        char    *key_b = (char *) b;

        return g_strcmp0 (key_b, key_a->settings_path);
}

static gboolean
bindings_get_entry (MsdKeybindingsManager *manager,
                    const char            *settings_path)
{
        GSettings *settings;
        Binding   *new_binding;
        GSList    *tmp_elem;
        char      *action = NULL;
        char      *key = NULL;

        if (!settings_path) {
                return FALSE;
        }

        /* Get entries for this binding */
        settings = g_settings_new_with_path (CUSTOM_KEYBINDING_SCHEMA, settings_path);
        action = g_settings_get_string (settings, "action");
        key = g_settings_get_string (settings, "binding");
        g_object_unref (settings);

        if (!action || !key) {
                g_warning (_("Key binding (%s) is incomplete"), settings_path);
                g_free (action);
                g_free (key);
                return FALSE;
        }

        g_debug ("keybindings: get entries from '%s' (action: '%s', key: '%s')", settings_path, action, key);

        tmp_elem = g_slist_find_custom (manager->priv->binding_list,
                                        settings_path,
                                        compare_bindings);

        if (!tmp_elem) {
                new_binding = g_new0 (Binding, 1);
        } else {
                new_binding = (Binding *) tmp_elem->data;
                g_free (new_binding->binding_str);
                g_free (new_binding->action);
                g_free (new_binding->settings_path);

                new_binding->previous_key.keysym = new_binding->key.keysym;
                new_binding->previous_key.state = new_binding->key.state;
                new_binding->previous_key.keycodes = new_binding->key.keycodes;
                new_binding->key.keycodes = NULL;
        }

        new_binding->binding_str = key;
        new_binding->action = action;
        new_binding->settings_path = g_strdup (settings_path);

        if (parse_binding (new_binding)) {
                if (!tmp_elem)
                        manager->priv->binding_list = g_slist_prepend (manager->priv->binding_list, new_binding);
        } else {
                g_free (new_binding->binding_str);
                g_free (new_binding->action);
                g_free (new_binding->settings_path);
                g_free (new_binding->previous_key.keycodes);
                g_free (new_binding);

                if (tmp_elem)
                        manager->priv->binding_list = g_slist_delete_link (manager->priv->binding_list, tmp_elem);
                return FALSE;
        }

        return TRUE;
}

static void
bindings_clear (MsdKeybindingsManager *manager)
{
        MsdKeybindingsManagerPrivate *p = manager->priv;
        GSList *l;

        if (p->binding_list != NULL)
        {
                for (l = p->binding_list; l; l = l->next) {
                        Binding *b = l->data;
                        g_free (b->binding_str);
                        g_free (b->action);
                        g_free (b->settings_path);
                        g_free (b->previous_key.keycodes);
                        g_free (b->key.keycodes);
                        g_free (b);
                }
                g_slist_free (p->binding_list);
                p->binding_list = NULL;
        }
}

static void
bindings_get_entries (MsdKeybindingsManager *manager)
{
        gchar **custom_list = NULL;
        gint i;

        bindings_clear (manager);

        custom_list = dconf_util_list_subdirs (GSETTINGS_KEYBINDINGS_DIR, FALSE);

        if (custom_list != NULL)
        {
                for (i = 0; custom_list[i] != NULL; i++)
                {
                        gchar *settings_path;
                        settings_path = g_strdup_printf("%s%s", GSETTINGS_KEYBINDINGS_DIR, custom_list[i]);
                        bindings_get_entry (manager, settings_path);
                        g_free (settings_path);
                }
                g_strfreev (custom_list);
        }

}

static gboolean
same_keycode (const Key *key, const Key *other)
{
        if (key->keycodes != NULL && other->keycodes != NULL) {
                guint *c;

                for (c = key->keycodes; *c; ++c) {
                        if (key_uses_keycode (other, *c))
                                return TRUE;
                }
        }
        return FALSE;
}

static gboolean
same_key (const Key *key, const Key *other)
{
        if (key->state == other->state) {
                if (key->keycodes != NULL && other->keycodes != NULL) {
                        guint *c1, *c2;

                        for (c1 = key->keycodes, c2 = other->keycodes;
                             *c1 || *c2; ++c1, ++c2) {
                                     if (*c1 != *c2)
                                        return FALSE;
                        }
                } else if (key->keycodes != NULL || other->keycodes != NULL)
                        return FALSE;


                return TRUE;
        }

        return FALSE;
}

static gboolean
key_already_used (MsdKeybindingsManager *manager,
                  Binding               *binding)
{
        GSList *li;

        for (li = manager->priv->binding_list; li != NULL; li = li->next) {
                Binding *tmp_binding =  (Binding*) li->data;

                if (tmp_binding != binding &&
                    same_keycode (&tmp_binding->key, &binding->key) &&
                    tmp_binding->key.state == binding->key.state) {
                        return TRUE;
                }
        }

        return FALSE;
}

static void
binding_unregister_keys (MsdKeybindingsManager *manager)
{
        GSList *li;
        gboolean need_flush = FALSE;

        gdk_error_trap_push ();

        for (li = manager->priv->binding_list; li != NULL; li = li->next) {
                Binding *binding = (Binding *) li->data;

                if (binding->key.keycodes) {
                        need_flush = TRUE;
                        grab_key_unsafe (&binding->key, FALSE, manager->priv->screens);
                }
        }

        if (need_flush)
                gdk_flush ();
        gdk_error_trap_pop ();
}

static void
binding_register_keys (MsdKeybindingsManager *manager)
{
        GSList *li;
        gboolean need_flush = FALSE;

        gdk_error_trap_push ();

        /* Now check for changes and grab new key if not already used */
        for (li = manager->priv->binding_list; li != NULL; li = li->next) {
                Binding *binding = (Binding *) li->data;

                if (!same_key (&binding->previous_key, &binding->key)) {
                        /* Ungrab key if it changed and not clashing with previously set binding */
                        if (!key_already_used (manager, binding)) {
                                gint i;

                                need_flush = TRUE;
                                if (binding->previous_key.keycodes) {
                                        grab_key_unsafe (&binding->previous_key, FALSE, manager->priv->screens);
                                }
                                grab_key_unsafe (&binding->key, TRUE, manager->priv->screens);

                                binding->previous_key.keysym = binding->key.keysym;
                                binding->previous_key.state = binding->key.state;
                                g_free (binding->previous_key.keycodes);
                                for (i = 0; binding->key.keycodes[i]; ++i);
                                binding->previous_key.keycodes = g_new0 (guint, i);
                                for (i = 0; binding->key.keycodes[i]; ++i)
                                        binding->previous_key.keycodes[i] = binding->key.keycodes[i];
                        } else
                                g_warning ("Key binding (%s) is already in use", binding->binding_str);
                }
        }

        if (need_flush)
                gdk_flush ();
        if (gdk_error_trap_pop ())
                g_warning ("Grab failed for some keys, another application may already have access the them.");

}

extern char **environ;

static char *
screen_exec_display_string (GdkScreen *screen)
{
        GString    *str;
        const char *old_display;
        char       *p;

        g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL);

        old_display = gdk_display_get_name (gdk_screen_get_display (screen));

        str = g_string_new ("DISPLAY=");
        g_string_append (str, old_display);

        p = strrchr (str->str, '.');
        if (p && p >  strchr (str->str, ':')) {
                g_string_truncate (str, p - str->str);
        }

        g_string_append_printf (str, ".%d", gdk_screen_get_number (screen));

        return g_string_free (str, FALSE);
}

/**
 * get_exec_environment:
 *
 * Description: Modifies the current program environment to
 * ensure that $DISPLAY is set such that a launched application
 * inheriting this environment would appear on screen.
 *
 * Returns: a newly-allocated %NULL-terminated array of strings or
 * %NULL on error. Use g_strfreev() to free it.
 *
 * mainly ripped from egg_screen_exec_display_string in
 * mate-panel/egg-screen-exec.c
 **/
static char **
get_exec_environment (XEvent *xevent)
{
        char     **retval = NULL;
        int        i;
        int        display_index = -1;
        GdkScreen *screen = NULL;
        GdkWindow *window = gdk_xid_table_lookup (xevent->xkey.root);

        if (window) {
                screen = gdk_drawable_get_screen (GDK_DRAWABLE (window));
        }

        g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL);

        for (i = 0; environ [i]; i++) {
                if (!strncmp (environ [i], "DISPLAY", 7)) {
                        display_index = i;
                }
        }

        if (display_index == -1) {
                display_index = i++;
        }

        retval = g_new (char *, i + 1);

        for (i = 0; environ [i]; i++) {
                if (i == display_index) {
                        retval [i] = screen_exec_display_string (screen);
                } else {
                        retval [i] = g_strdup (environ [i]);
                }
        }

        retval [i] = NULL;

        return retval;
}

static GdkFilterReturn
keybindings_filter (GdkXEvent             *gdk_xevent,
                    GdkEvent              *event,
                    MsdKeybindingsManager *manager)
{
        XEvent *xevent = (XEvent *) gdk_xevent;
        GSList *li;

        if (xevent->type != KeyPress) {
                return GDK_FILTER_CONTINUE;
        }

        for (li = manager->priv->binding_list; li != NULL; li = li->next) {
                Binding *binding = (Binding *) li->data;

                if (match_key (&binding->key, xevent)) {
                        GError  *error = NULL;
                        gboolean retval;
                        gchar  **argv = NULL;
                        gchar  **envp = NULL;

                        g_return_val_if_fail (binding->action != NULL, GDK_FILTER_CONTINUE);

                        if (!g_shell_parse_argv (binding->action,
                                                 NULL, &argv,
                                                 &error)) {
                                return GDK_FILTER_CONTINUE;
                        }

                        envp = get_exec_environment (xevent);

                        retval = g_spawn_async (NULL,
                                                argv,
                                                envp,
                                                G_SPAWN_SEARCH_PATH,
                                                NULL,
                                                NULL,
                                                NULL,
                                                &error);
                        g_strfreev (argv);
                        g_strfreev (envp);

                        if (!retval) {
                                GtkWidget *dialog = gtk_message_dialog_new (NULL, 0, GTK_MESSAGE_WARNING,
                                                                            GTK_BUTTONS_CLOSE,
                                                                            _("Error while trying to run (%s)\n"\
                                                                              "which is linked to the key (%s)"),
                                                                            binding->action,
                                                                            binding->binding_str);
                                g_signal_connect (dialog,
                                                  "response",
                                                  G_CALLBACK (gtk_widget_destroy),
                                                  NULL);
                                gtk_widget_show (dialog);
                        }
                        return GDK_FILTER_REMOVE;
                }
        }
        return GDK_FILTER_CONTINUE;
}

static void
bindings_callback (DConfClient           *client,
        #ifdef HAVE_DCONF_0_13
                   gchar                 *prefix,
                   GStrv                  changes,
        #else
                   const gchar           *path,
                   const gchar * const   *items,
                   gint                   n_items,
        #endif
                   gchar                 *tag,
                   MsdKeybindingsManager *manager)
{
        g_debug ("keybindings: received 'changed' signal from dconf");

        binding_unregister_keys (manager);

        bindings_get_entries (manager);

        binding_register_keys (manager);
}

gboolean
msd_keybindings_manager_start (MsdKeybindingsManager *manager,
                               GError               **error)
{
        GdkDisplay  *dpy;
        GdkScreen   *screen;
        int          screen_num;
        int          i;

        g_debug ("Starting keybindings manager");
        mate_settings_profile_start (NULL);

        dpy = gdk_display_get_default ();
        screen_num = gdk_display_get_n_screens (dpy);

        for (i = 0; i < screen_num; i++) {
                screen = gdk_display_get_screen (dpy, i);
                gdk_window_add_filter (gdk_screen_get_root_window (screen),
                                       (GdkFilterFunc) keybindings_filter,
                                       manager);
        }
        manager->priv->screens = get_screens_list ();

        manager->priv->binding_list = NULL;
        bindings_get_entries (manager);
        binding_register_keys (manager);

        /* DConf has different API between versions:
         * http://developer.gnome.org/dconf/0.12/DConfClient.html
         * http://developer.gnome.org/dconf/0.14/DConfClient.html
         */
        #ifdef HAVE_DCONF_0_13
        manager->priv->client = dconf_client_new ();
        dconf_client_watch_fast (manager->priv->client, GSETTINGS_KEYBINDINGS_DIR);
        g_signal_connect (manager->priv->client, "changed", G_CALLBACK (bindings_callback), manager);
        #else
        manager->priv->client = dconf_client_new (NULL, (DConfWatchFunc) bindings_callback, manager, NULL);
        dconf_client_watch (manager->priv->client, GSETTINGS_KEYBINDINGS_DIR, NULL, NULL);
        #endif

        mate_settings_profile_end (NULL);

        return TRUE;
}

void
msd_keybindings_manager_stop (MsdKeybindingsManager *manager)
{
        MsdKeybindingsManagerPrivate *p = manager->priv;
        GSList *l;

        g_debug ("Stopping keybindings manager");

        if (p->client != NULL) {
                g_object_unref (p->client);
                p->client = NULL;
        }

        for (l = p->screens; l; l = l->next) {
                GdkScreen *screen = l->data;
                gdk_window_remove_filter (gdk_screen_get_root_window (screen),
                                          (GdkFilterFunc) keybindings_filter,
                                          manager);
        }

        binding_unregister_keys (manager);
        bindings_clear (manager);

        g_slist_free (p->screens);
        p->screens = NULL;
}

static void
msd_keybindings_manager_set_property (GObject        *object,
                               guint           prop_id,
                               const GValue   *value,
                               GParamSpec     *pspec)
{
        MsdKeybindingsManager *self;

        self = MSD_KEYBINDINGS_MANAGER (object);

        switch (prop_id) {
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static void
msd_keybindings_manager_get_property (GObject        *object,
                               guint           prop_id,
                               GValue         *value,
                               GParamSpec     *pspec)
{
        MsdKeybindingsManager *self;

        self = MSD_KEYBINDINGS_MANAGER (object);

        switch (prop_id) {
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static GObject *
msd_keybindings_manager_constructor (GType                  type,
                              guint                  n_construct_properties,
                              GObjectConstructParam *construct_properties)
{
        MsdKeybindingsManager      *keybindings_manager;
        MsdKeybindingsManagerClass *klass;

        klass = MSD_KEYBINDINGS_MANAGER_CLASS (g_type_class_peek (MSD_TYPE_KEYBINDINGS_MANAGER));

        keybindings_manager = MSD_KEYBINDINGS_MANAGER (G_OBJECT_CLASS (msd_keybindings_manager_parent_class)->constructor (type,
                                                                                                      n_construct_properties,
                                                                                                      construct_properties));

        return G_OBJECT (keybindings_manager);
}

static void
msd_keybindings_manager_dispose (GObject *object)
{
        MsdKeybindingsManager *keybindings_manager;

        keybindings_manager = MSD_KEYBINDINGS_MANAGER (object);

        G_OBJECT_CLASS (msd_keybindings_manager_parent_class)->dispose (object);
}

static void
msd_keybindings_manager_class_init (MsdKeybindingsManagerClass *klass)
{
        GObjectClass   *object_class = G_OBJECT_CLASS (klass);

        object_class->get_property = msd_keybindings_manager_get_property;
        object_class->set_property = msd_keybindings_manager_set_property;
        object_class->constructor = msd_keybindings_manager_constructor;
        object_class->dispose = msd_keybindings_manager_dispose;
        object_class->finalize = msd_keybindings_manager_finalize;

        g_type_class_add_private (klass, sizeof (MsdKeybindingsManagerPrivate));
}

static void
msd_keybindings_manager_init (MsdKeybindingsManager *manager)
{
        manager->priv = MSD_KEYBINDINGS_MANAGER_GET_PRIVATE (manager);

}

static void
msd_keybindings_manager_finalize (GObject *object)
{
        MsdKeybindingsManager *keybindings_manager;

        g_return_if_fail (object != NULL);
        g_return_if_fail (MSD_IS_KEYBINDINGS_MANAGER (object));

        keybindings_manager = MSD_KEYBINDINGS_MANAGER (object);

        g_return_if_fail (keybindings_manager->priv != NULL);

        G_OBJECT_CLASS (msd_keybindings_manager_parent_class)->finalize (object);
}

MsdKeybindingsManager *
msd_keybindings_manager_new (void)
{
        if (manager_object != NULL) {
                g_object_ref (manager_object);
        } else {
                manager_object = g_object_new (MSD_TYPE_KEYBINDINGS_MANAGER, NULL);
                g_object_add_weak_pointer (manager_object,
                                           (gpointer *) &manager_object);
        }

        return MSD_KEYBINDINGS_MANAGER (manager_object);
}
