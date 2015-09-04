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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
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
#include <math.h>

#include <locale.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gdk/gdkkeysyms.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>

#ifdef HAVE_X11_EXTENSIONS_XINPUT_H
#include <X11/extensions/XInput.h>
#include <X11/extensions/XIproto.h>
#endif
#include <gio/gio.h>

#include "mate-settings-profile.h"
#include "msd-mouse-manager.h"
#include "msd-input-helper.h"

#define MSD_MOUSE_MANAGER_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), MSD_TYPE_MOUSE_MANAGER, MsdMouseManagerPrivate))

#define MATE_MOUSE_SCHEMA                "org.mate.peripherals-mouse"
#define KEY_MOUSE_LEFT_HANDED            "left-handed"
#define KEY_MOUSE_MOTION_ACCELERATION    "motion-acceleration"
#define KEY_MOUSE_MOTION_THRESHOLD       "motion-threshold"
#define KEY_MOUSE_LOCATE_POINTER         "locate-pointer"
#define KEY_MIDDLE_BUTTON_EMULATION      "middle-button-enabled"

#define MATE_TOUCHPAD_SCHEMA             "org.mate.peripherals-touchpad"
#define KEY_TOUCHPAD_DISABLE_W_TYPING    "disable-while-typing"
#ifdef HAVE_X11_EXTENSIONS_XINPUT_H
#define KEY_TOUCHPAD_TAP_TO_CLICK        "tap-to-click"
#define KEY_TOUCHPAD_SCROLL_METHOD       "scroll-method"
#define KEY_TOUCHPAD_PAD_HORIZ_SCROLL    "horiz-scroll-enabled"
#define KEY_TOUCHPAD_ENABLED             "touchpad-enabled"
#endif

/* FIXME: currently there is no mate-mousetweaks, so I comment this
 *
 *#define MATE_MOUSE_A11Y_SCHEMA         "org.mate.accessibility-mouse"
 *#define KEY_MOUSE_A11Y_DWELL_ENABLE    "dwell-enable"
 *#define KEY_MOUSE_A11Y_DELAY_ENABLE    "delay-enable"
 */

struct MsdMouseManagerPrivate
{
        GSettings *settings_mouse;
        GSettings *settings_touchpad;

        gboolean mousetweaks_daemon_running;
        gboolean syndaemon_spawned;
        GPid syndaemon_pid;
        gboolean locate_pointer_spawned;
        GPid locate_pointer_pid;
};

static void     msd_mouse_manager_class_init  (MsdMouseManagerClass *klass);
static void     msd_mouse_manager_init        (MsdMouseManager      *mouse_manager);
static void     msd_mouse_manager_finalize    (GObject             *object);
static void     set_mouse_settings            (MsdMouseManager      *manager);
#ifdef HAVE_X11_EXTENSIONS_XINPUT_H
static int      set_tap_to_click              (gboolean state, gboolean left_handed);
#endif

G_DEFINE_TYPE (MsdMouseManager, msd_mouse_manager, G_TYPE_OBJECT)

static gpointer manager_object = NULL;

static void
msd_mouse_manager_set_property (GObject        *object,
                               guint           prop_id,
                               const GValue   *value,
                               GParamSpec     *pspec)
{
        MsdMouseManager *self;

        self = MSD_MOUSE_MANAGER (object);

        switch (prop_id) {
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static void
msd_mouse_manager_get_property (GObject        *object,
                               guint           prop_id,
                               GValue         *value,
                               GParamSpec     *pspec)
{
        MsdMouseManager *self;

        self = MSD_MOUSE_MANAGER (object);

        switch (prop_id) {
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static GObject *
msd_mouse_manager_constructor (GType                  type,
                              guint                  n_construct_properties,
                              GObjectConstructParam *construct_properties)
{
        MsdMouseManager      *mouse_manager;
        MsdMouseManagerClass *klass;

        klass = MSD_MOUSE_MANAGER_CLASS (g_type_class_peek (MSD_TYPE_MOUSE_MANAGER));

        mouse_manager = MSD_MOUSE_MANAGER (G_OBJECT_CLASS (msd_mouse_manager_parent_class)->constructor (type,
                                                                                                      n_construct_properties,
                                                                                                      construct_properties));

        return G_OBJECT (mouse_manager);
}

static void
msd_mouse_manager_dispose (GObject *object)
{
        MsdMouseManager *mouse_manager;

        mouse_manager = MSD_MOUSE_MANAGER (object);

        G_OBJECT_CLASS (msd_mouse_manager_parent_class)->dispose (object);
}

static void
msd_mouse_manager_class_init (MsdMouseManagerClass *klass)
{
        GObjectClass   *object_class = G_OBJECT_CLASS (klass);

        object_class->get_property = msd_mouse_manager_get_property;
        object_class->set_property = msd_mouse_manager_set_property;
        object_class->constructor = msd_mouse_manager_constructor;
        object_class->dispose = msd_mouse_manager_dispose;
        object_class->finalize = msd_mouse_manager_finalize;

        g_type_class_add_private (klass, sizeof (MsdMouseManagerPrivate));
}


#ifdef HAVE_X11_EXTENSIONS_XINPUT_H
static gboolean
supports_xinput_devices (void)
{
        gint op_code, event, error;

        return XQueryExtension (GDK_DISPLAY_XDISPLAY(gdk_display_get_default()),
                                "XInputExtension",
                                &op_code,
                                &event,
                                &error);
}
#endif

static void
configure_button_layout (guchar   *buttons,
                         gint      n_buttons,
                         gboolean  left_handed)
{
        const gint left_button = 1;
        gint right_button;
        gint i;

        /* if the button is higher than 2 (3rd button) then it's
         * probably one direction of a scroll wheel or something else
         * uninteresting
         */
        right_button = MIN (n_buttons, 3);

        /* If we change things we need to make sure we only swap buttons.
         * If we end up with multiple physical buttons assigned to the same
         * logical button the server will complain. This code assumes physical
         * button 0 is the physical left mouse button, and that the physical
         * button other than 0 currently assigned left_button or right_button
         * is the physical right mouse button.
         */

        /* check if the current mapping satisfies the above assumptions */
        if (buttons[left_button - 1] != left_button &&
            buttons[left_button - 1] != right_button)
                /* The current mapping is weird. Swapping buttons is probably not a
                 * good idea.
                 */
                return;

        /* check if we are left_handed and currently not swapped */
        if (left_handed && buttons[left_button - 1] == left_button) {
                /* find the right button */
                for (i = 0; i < n_buttons; i++) {
                        if (buttons[i] == right_button) {
                                buttons[i] = left_button;
                                break;
                        }
                }
                /* swap the buttons */
                buttons[left_button - 1] = right_button;
        }
        /* check if we are not left_handed but are swapped */
        else if (!left_handed && buttons[left_button - 1] == right_button) {
                /* find the right button */
                for (i = 0; i < n_buttons; i++) {
                        if (buttons[i] == left_button) {
                                buttons[i] = right_button;
                                break;
                        }
                }
                /* swap the buttons */
                buttons[left_button - 1] = left_button;
        }
}

#ifdef HAVE_X11_EXTENSIONS_XINPUT_H
static gboolean
xinput_device_has_buttons (XDeviceInfo *device_info)
{
        int i;
        XAnyClassInfo *class_info;

        class_info = device_info->inputclassinfo;
        for (i = 0; i < device_info->num_classes; i++) {
                if (class_info->class == ButtonClass) {
                        XButtonInfo *button_info;

                        button_info = (XButtonInfo *) class_info;
                        if (button_info->num_buttons > 0)
                                return TRUE;
                }

                class_info = (XAnyClassInfo *) (((guchar *) class_info) +
                                                class_info->length);
        }
        return FALSE;
}

static gboolean
touchpad_has_single_button (XDevice *device)
{
        Atom type, prop;
        int format;
        unsigned long nitems, bytes_after;
        unsigned char *data;
        gboolean is_single_button = FALSE;
        int rc;

        prop = XInternAtom (GDK_DISPLAY_XDISPLAY(gdk_display_get_default()), "Synaptics Capabilities", False);
        if (!prop)
                return FALSE;

        gdk_error_trap_push ();
        rc = XGetDeviceProperty (GDK_DISPLAY_XDISPLAY(gdk_display_get_default()), device, prop, 0, 1, False,
                                XA_INTEGER, &type, &format, &nitems,
                                &bytes_after, &data);
        if (rc == Success && type == XA_INTEGER && format == 8 && nitems >= 3)
                is_single_button = (data[0] == 1 && data[1] == 0 && data[2] == 0);

        if (rc == Success)
                XFree (data);

        gdk_error_trap_pop ();

        return is_single_button;
}


static void
set_xinput_devices_left_handed (gboolean left_handed)
{
        XDeviceInfo *device_info;
        gint n_devices;
        guchar *buttons;
        gsize buttons_capacity = 16;
        gint n_buttons;
        gint i;

        device_info = XListInputDevices (GDK_DISPLAY_XDISPLAY(gdk_display_get_default()), &n_devices);

        if (n_devices > 0)
                buttons = g_new (guchar, buttons_capacity);
        else
                buttons = NULL;

        for (i = 0; i < n_devices; i++) {
                XDevice *device = NULL;

                if ((device_info[i].use == IsXPointer) ||
                    (device_info[i].use == IsXKeyboard) ||
                    (g_strcmp0 ("Virtual core XTEST pointer", device_info[i].name) == 0) ||
                    (!xinput_device_has_buttons (&device_info[i])))
                        continue;

                /* If the device is a touchpad, swap tap buttons
                 * around too, otherwise a tap would be a right-click */
                device = device_is_touchpad (device_info[i]);
                if (device != NULL) {
                        GSettings *settings = g_settings_new (MATE_TOUCHPAD_SCHEMA);
                        gboolean tap = g_settings_get_boolean (settings, KEY_TOUCHPAD_TAP_TO_CLICK);
                        gboolean single_button = touchpad_has_single_button (device);

                        if (tap && !single_button)
                                set_tap_to_click (tap, left_handed);
                        XCloseDevice (GDK_DISPLAY_XDISPLAY(gdk_display_get_default()), device);
                        g_object_unref (settings);

                        if (single_button)
                            continue;
                }

                gdk_error_trap_push ();

                device = XOpenDevice (GDK_DISPLAY_XDISPLAY(gdk_display_get_default()), device_info[i].id);

                if ((gdk_error_trap_pop () != 0) ||
                    (device == NULL))
                        continue;

                n_buttons = XGetDeviceButtonMapping (GDK_DISPLAY_XDISPLAY(gdk_display_get_default()), device,
                                                     buttons,
                                                     buttons_capacity);

                while (n_buttons > buttons_capacity) {
                        buttons_capacity = n_buttons;
                        buttons = (guchar *) g_realloc (buttons,
                                                        buttons_capacity * sizeof (guchar));

                        n_buttons = XGetDeviceButtonMapping (GDK_DISPLAY_XDISPLAY(gdk_display_get_default()), device,
                                                             buttons,
                                                             buttons_capacity);
                }

                configure_button_layout (buttons, n_buttons, left_handed);

                XSetDeviceButtonMapping (GDK_DISPLAY_XDISPLAY(gdk_display_get_default()), device, buttons, n_buttons);
                XCloseDevice (GDK_DISPLAY_XDISPLAY(gdk_display_get_default()), device);
        }
        g_free (buttons);

        if (device_info != NULL)
                XFreeDeviceList (device_info);
}

static GdkFilterReturn
devicepresence_filter (GdkXEvent *xevent,
                       GdkEvent  *event,
                       gpointer   data)
{
        XEvent *xev = (XEvent *) xevent;
        XEventClass class_presence;
        int xi_presence;

        DevicePresence (gdk_x11_get_default_xdisplay (), xi_presence, class_presence);

        if (xev->type == xi_presence)
        {
            XDevicePresenceNotifyEvent *dpn = (XDevicePresenceNotifyEvent *) xev;
            if (dpn->devchange == DeviceEnabled)
                set_mouse_settings ((MsdMouseManager *) data);
        }
        return GDK_FILTER_CONTINUE;
}

static void
set_devicepresence_handler (MsdMouseManager *manager)
{
        Display *display;
        XEventClass class_presence;
        int xi_presence;

        if (!supports_xinput_devices ())
                return;

        display = gdk_x11_get_default_xdisplay ();

        gdk_error_trap_push ();
        DevicePresence (display, xi_presence, class_presence);
        XSelectExtensionEvent (display,
                               RootWindow (display, DefaultScreen (display)),
                               &class_presence, 1);

        gdk_flush ();
        if (!gdk_error_trap_pop ())
                gdk_window_add_filter (NULL, devicepresence_filter, manager);
}
#endif

static void
set_left_handed (MsdMouseManager *manager,
                 gboolean         left_handed)
{
        guchar *buttons ;
        gsize buttons_capacity = 16;
        gint n_buttons, i;

#ifdef HAVE_X11_EXTENSIONS_XINPUT_H
        if (supports_xinput_devices ()) {
                /* When XInput support is available, never set the
                 * button ordering on the core pointer as that would
                 * revert the changes we make on the devices themselves */
                set_xinput_devices_left_handed (left_handed);
                return;
        }
#endif

        buttons = g_new (guchar, buttons_capacity);
        n_buttons = XGetPointerMapping (GDK_DISPLAY_XDISPLAY(gdk_display_get_default()),
                                        buttons,
                                        (gint) buttons_capacity);
        while (n_buttons > buttons_capacity) {
                buttons_capacity = n_buttons;
                buttons = (guchar *) g_realloc (buttons,
                                                buttons_capacity * sizeof (guchar));

                n_buttons = XGetPointerMapping (GDK_DISPLAY_XDISPLAY(gdk_display_get_default()),
                                                buttons,
                                                (gint) buttons_capacity);
        }

        configure_button_layout (buttons, n_buttons, left_handed);

        /* X refuses to change the mapping while buttons are engaged,
         * so if this is the case we'll retry a few times
         */
        for (i = 0;
             i < 20 && XSetPointerMapping (GDK_DISPLAY_XDISPLAY(gdk_display_get_default()), buttons, n_buttons) == MappingBusy;
             ++i) {
                g_usleep (300);
        }

        g_free (buttons);
}

static void
set_motion_acceleration (MsdMouseManager *manager,
                         gfloat           motion_acceleration)
{
        gint numerator, denominator;

        if (motion_acceleration >= 1.0) {
                /* we want to get the acceleration, with a resolution of 0.5
                 */
                if ((motion_acceleration - floor (motion_acceleration)) < 0.25) {
                        numerator = floor (motion_acceleration);
                        denominator = 1;
                } else if ((motion_acceleration - floor (motion_acceleration)) < 0.5) {
                        numerator = ceil (2.0 * motion_acceleration);
                        denominator = 2;
                } else if ((motion_acceleration - floor (motion_acceleration)) < 0.75) {
                        numerator = floor (2.0 *motion_acceleration);
                        denominator = 2;
                } else {
                        numerator = ceil (motion_acceleration);
                        denominator = 1;
                }
        } else if (motion_acceleration < 1.0 && motion_acceleration > 0) {
                /* This we do to 1/10ths */
                numerator = floor (motion_acceleration * 10) + 1;
                denominator= 10;
        } else {
                numerator = -1;
                denominator = -1;
        }

        XChangePointerControl (GDK_DISPLAY_XDISPLAY(gdk_display_get_default()), True, False,
                               numerator, denominator,
                               0);
}

static void
set_motion_threshold (MsdMouseManager *manager,
                      int              motion_threshold)
{
        XChangePointerControl (GDK_DISPLAY_XDISPLAY(gdk_display_get_default()), False, True,
                               0, 0, motion_threshold);
}

static void
set_middle_button (MsdMouseManager *manager,
                   gboolean         middle_button)
{
        XDeviceInfo *device_info;
        gint n_devices;
        gint i;
        Atom prop;

        prop = XInternAtom (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()),
                            "Evdev Middle Button Emulation", True);

        if (!prop) /* no evdev devices */
                return;

        device_info = XListInputDevices (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), &n_devices);

        for (i = 0; i < n_devices; i++) {
                XDevice *device = NULL;
                Atom type;
                int format;
                unsigned long nitems, bytes_after;
                unsigned char *data;

                gdk_error_trap_push ();

                device = XOpenDevice (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), device_info[i].id);

                if ((gdk_error_trap_pop () != 0) ||
                    (device == NULL))
                        continue;

                gdk_error_trap_push ();

                XGetDeviceProperty (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()),
                                    device, prop, 0, 1, False, XA_INTEGER, &type, &format,
                                    &nitems, &bytes_after, &data);

                if ((gdk_error_trap_pop () != 0)) {
                        XCloseDevice (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), device);
                        continue;
                }

                if (format == 8 && type == XA_INTEGER && nitems == 1) {
                        data[0] = middle_button ? 1 : 0;

                        gdk_error_trap_push ();
                        XChangeDeviceProperty (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()),
                                               device, prop, type, format, PropModeReplace, data, nitems);

                        gdk_error_trap_pop ();
                }

                XFree (data);
                XCloseDevice (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), device);
        }

        if (device_info != NULL)
                XFreeDeviceList (device_info);
}

static gboolean
have_program_in_path (const char *name)
{
        gchar *path;
        gboolean result;

        path = g_find_program_in_path (name);
        result = (path != NULL);
        g_free (path);
        return result;
}

static int
set_disable_w_typing (MsdMouseManager *manager, gboolean state)
{

        if (state && touchpad_is_present ()) {
                GError *error = NULL;
                char *args[6];

                if (manager->priv->syndaemon_spawned)
                        return 0;

                args[0] = "syndaemon";
                args[1] = "-i";
                args[2] = "0.5";
                args[3] = "-K";
                args[4] = "-R";
                args[5] = NULL;

                if (!have_program_in_path (args[0]))
                        return 0;

                g_spawn_async (g_get_home_dir (), args, NULL,
                                G_SPAWN_SEARCH_PATH, NULL, NULL,
                                &manager->priv->syndaemon_pid, &error);

                manager->priv->syndaemon_spawned = (error == NULL);

                if (error) {
                        g_settings_set_boolean (manager->priv->settings_touchpad, KEY_TOUCHPAD_DISABLE_W_TYPING, FALSE);
                        g_error_free (error);
                }

        } else if (manager->priv->syndaemon_spawned)
        {
                kill (manager->priv->syndaemon_pid, SIGHUP);
                g_spawn_close_pid (manager->priv->syndaemon_pid);
                manager->priv->syndaemon_spawned = FALSE;
        }

        return 0;
}

#ifdef HAVE_X11_EXTENSIONS_XINPUT_H
static int
set_tap_to_click (gboolean state, gboolean left_handed)
{
        int numdevices, i, format, rc;
        unsigned long nitems, bytes_after;
        XDeviceInfo *devicelist = XListInputDevices (GDK_DISPLAY_XDISPLAY(gdk_display_get_default()), &numdevices);
        XDevice * device;
        unsigned char* data;
        Atom prop, type;

        if (devicelist == NULL)
                return 0;

        prop = XInternAtom (GDK_DISPLAY_XDISPLAY(gdk_display_get_default()), "Synaptics Tap Action", False);

        if (!prop)
                return 0;

        for (i = 0; i < numdevices; i++) {
                if ((device = device_is_touchpad (devicelist[i]))) {
                        gdk_error_trap_push ();
                        rc = XGetDeviceProperty (GDK_DISPLAY_XDISPLAY(gdk_display_get_default()), device, prop, 0, 2,
                                                False, XA_INTEGER, &type, &format, &nitems,
                                                &bytes_after, &data);

                        if (rc == Success && type == XA_INTEGER && format == 8 && nitems >= 7)
                        {
                                /* Set RLM mapping for 1/2/3 fingers*/
                                data[4] = (state) ? ((left_handed) ? 3 : 1) : 0;
                                data[5] = (state) ? ((left_handed) ? 1 : 3) : 0;
                                data[6] = (state) ? 2 : 0;
                                XChangeDeviceProperty (GDK_DISPLAY_XDISPLAY(gdk_display_get_default()), device, prop, XA_INTEGER, 8,
                                                        PropModeReplace, data, nitems);
                        }

                        if (rc == Success)
                                XFree (data);
                        XCloseDevice (GDK_DISPLAY_XDISPLAY(gdk_display_get_default()), device);
                        if (gdk_error_trap_pop ()) {
                                g_warning ("Error in setting tap to click on \"%s\"", devicelist[i].name);
                                continue;
                        }
                }
        }

        XFreeDeviceList (devicelist);
        return 0;
}

static int
set_horiz_scroll (gboolean state)
{
        int numdevices, i, rc;
        XDeviceInfo *devicelist = XListInputDevices (GDK_DISPLAY_XDISPLAY(gdk_display_get_default()), &numdevices);
        XDevice *device;
        Atom act_type, prop_edge, prop_twofinger;
        int act_format;
        unsigned long nitems, bytes_after;
        unsigned char *data;

        if (devicelist == NULL)
                return 0;

        prop_edge = XInternAtom (GDK_DISPLAY_XDISPLAY(gdk_display_get_default()), "Synaptics Edge Scrolling", False);
        prop_twofinger = XInternAtom (GDK_DISPLAY_XDISPLAY(gdk_display_get_default()), "Synaptics Two-Finger Scrolling", False);

        if (!prop_edge || !prop_twofinger)
                return 0;

        for (i = 0; i < numdevices; i++) {
                if ((device = device_is_touchpad (devicelist[i]))) {
                        gdk_error_trap_push ();
                        rc = XGetDeviceProperty (GDK_DISPLAY_XDISPLAY(gdk_display_get_default()), device,
                                                prop_edge, 0, 1, False,
                                                XA_INTEGER, &act_type, &act_format, &nitems,
                                                &bytes_after, &data);
                        if (rc == Success && act_type == XA_INTEGER &&
                                act_format == 8 && nitems >= 2) {
                                data[1] = (state && data[0]);
                                XChangeDeviceProperty (GDK_DISPLAY_XDISPLAY(gdk_display_get_default()), device,
                                                        prop_edge, XA_INTEGER, 8,
                                                        PropModeReplace, data, nitems);
                        }

                        XFree (data);

                        rc = XGetDeviceProperty (GDK_DISPLAY_XDISPLAY(gdk_display_get_default()), device,
                                                prop_twofinger, 0, 1, False,
                                                XA_INTEGER, &act_type, &act_format, &nitems,
                                                &bytes_after, &data);
                        if (rc == Success && act_type == XA_INTEGER &&
                                act_format == 8 && nitems >= 2) {
                                data[1] = (state && data[0]);
                                XChangeDeviceProperty (GDK_DISPLAY_XDISPLAY(gdk_display_get_default()), device,
                                                        prop_twofinger, XA_INTEGER, 8,
                                                        PropModeReplace, data, nitems);
                        }

                        XFree (data);
                        XCloseDevice (GDK_DISPLAY_XDISPLAY(gdk_display_get_default()), device);
                        if (gdk_error_trap_pop ()) {
                                g_warning ("Error in setting horiz scroll on \"%s\"", devicelist[i].name);
                                continue;
                        }
                }
        }

        XFreeDeviceList (devicelist);
        return 0;
}


/**
 * Scroll methods are: 0 - disabled, 1 - edge scrolling, 2 - twofinger
 * scrolling
 */
static int
set_edge_scroll (int method)
{
        int numdevices, i, rc;
        XDeviceInfo *devicelist = XListInputDevices (GDK_DISPLAY_XDISPLAY(gdk_display_get_default()), &numdevices);
        XDevice *device;
        Atom act_type, prop_edge, prop_twofinger;
        int act_format;
        unsigned long nitems, bytes_after;
        unsigned char *data;

        if (devicelist == NULL)
                return 0;

        prop_edge = XInternAtom (GDK_DISPLAY_XDISPLAY(gdk_display_get_default()), "Synaptics Edge Scrolling", False);
        prop_twofinger = XInternAtom (GDK_DISPLAY_XDISPLAY(gdk_display_get_default()), "Synaptics Two-Finger Scrolling", False);

        if (!prop_edge || !prop_twofinger)
                return 0;

        for (i = 0; i < numdevices; i++) {
                if ((device = device_is_touchpad (devicelist[i]))) {
                        gdk_error_trap_push ();
                        rc = XGetDeviceProperty (GDK_DISPLAY_XDISPLAY(gdk_display_get_default()), device,
                                                prop_edge, 0, 1, False,
                                                XA_INTEGER, &act_type, &act_format, &nitems,
                                                &bytes_after, &data);
                        if (rc == Success && act_type == XA_INTEGER &&
                                act_format == 8 && nitems >= 2) {
                                data[0] = (method == 1) ? 1 : 0;
                                XChangeDeviceProperty (GDK_DISPLAY_XDISPLAY(gdk_display_get_default()), device,
                                                        prop_edge, XA_INTEGER, 8,
                                                        PropModeReplace, data, nitems);
                        }

                        XFree (data);

                        rc = XGetDeviceProperty (GDK_DISPLAY_XDISPLAY(gdk_display_get_default()), device,
                                                prop_twofinger, 0, 1, False,
                                                XA_INTEGER, &act_type, &act_format, &nitems,
                                                &bytes_after, &data);
                        if (rc == Success && act_type == XA_INTEGER &&
                                act_format == 8 && nitems >= 2) {
                                data[0] = (method == 2) ? 1 : 0;
                                XChangeDeviceProperty (GDK_DISPLAY_XDISPLAY(gdk_display_get_default()), device,
                                                        prop_twofinger, XA_INTEGER, 8,
                                                        PropModeReplace, data, nitems);
                        }

                        XFree (data);
                        XCloseDevice (GDK_DISPLAY_XDISPLAY(gdk_display_get_default()), device);
                        if (gdk_error_trap_pop ()) {
                                g_warning ("Error in setting edge scroll on \"%s\"", devicelist[i].name);
                                continue;
                        }
                }
        }

        XFreeDeviceList (devicelist);
        return 0;
}

static int
set_touchpad_enabled (gboolean state)
{
        int numdevices, i;
        XDeviceInfo *devicelist = XListInputDevices (GDK_DISPLAY_XDISPLAY(gdk_display_get_default()), &numdevices);
        XDevice *device;
        Atom prop_enabled;

        if (devicelist == NULL)
                return 0;

        prop_enabled = XInternAtom (GDK_DISPLAY_XDISPLAY(gdk_display_get_default()), "Device Enabled", False);

        if (!prop_enabled)
            return 0;

        for (i = 0; i < numdevices; i++) {
                if ((device = device_is_touchpad (devicelist[i]))) {
                        unsigned char data = state;
                        gdk_error_trap_push ();
                        XChangeDeviceProperty (GDK_DISPLAY_XDISPLAY(gdk_display_get_default()), device,
                                               prop_enabled, XA_INTEGER, 8,
                                               PropModeReplace, &data, 1);
                        XCloseDevice (GDK_DISPLAY_XDISPLAY(gdk_display_get_default()), device);
                        gdk_flush ();
                        if (gdk_error_trap_pop ()) {
                                g_warning ("Error %s device \"%s\"",
                                           (state) ? "enabling" : "disabling",
                                           devicelist[i].name);
                                continue;
                        }
                }
        }

        XFreeDeviceList (devicelist);
        return 0;
}
#endif

static void
set_locate_pointer (MsdMouseManager *manager,
                    gboolean         state)
{
        if (state) {
                GError *error = NULL;
                char *args[2];

                if (manager->priv->locate_pointer_spawned)
                        return;

                args[0] = LIBEXECDIR "/msd-locate-pointer";
                args[1] = NULL;

                g_spawn_async (NULL, args, NULL,
                               0, NULL, NULL,
                               &manager->priv->locate_pointer_pid, &error);

                manager->priv->locate_pointer_spawned = (error == NULL);

                if (error) {
                        g_settings_set_boolean (manager->priv->settings_mouse, KEY_MOUSE_LOCATE_POINTER, FALSE);
                        g_error_free (error);
                }

        }
        else if (manager->priv->locate_pointer_spawned) {
                kill (manager->priv->locate_pointer_pid, SIGHUP);
                g_spawn_close_pid (manager->priv->locate_pointer_pid);
                manager->priv->locate_pointer_spawned = FALSE;
        }
}

static void
set_mousetweaks_daemon (MsdMouseManager *manager,
                        gboolean         dwell_enable,
                        gboolean         delay_enable)
{
        /* FIXME there is no mate-mousetweaks */
        return;
        
        
        GError *error = NULL;
        gchar *comm;
        gboolean run_daemon = dwell_enable || delay_enable;

        if (run_daemon || manager->priv->mousetweaks_daemon_running)
                comm = g_strdup_printf ("mousetweaks %s",
                                        run_daemon ? "" : "-s");
        else
                return;

        if (run_daemon)
                manager->priv->mousetweaks_daemon_running = TRUE;


        if (! g_spawn_command_line_async (comm, &error)) {
                if (error->code == G_SPAWN_ERROR_NOENT &&
                    (dwell_enable || delay_enable)) {
                        GtkWidget *dialog;
                        
                        /* uncomment this when (and if) we'll fork mousetweaks */
                        
                        /*
                        GSettings *settings;
                        settings = g_settings_new (MATE_MOUSE_A11Y_SCHEMA);
                        if (dwell_enable)
                                g_settings_set_boolean (settings,
                                                        MATE_MOUSE_A11Y_KEY_DWELL_ENABLE,
                                                        FALSE);
                        else if (delay_enable)
                                g_settings_set_boolean (settings,
                                                        MATE_MOUSE_A11Y_KEY_DELAY_ENABLE,
                                                        FALSE);
                        g_object_unref (settings);
                        */
                        
                        dialog = gtk_message_dialog_new (NULL, 0,
                                                         GTK_MESSAGE_WARNING,
                                                         GTK_BUTTONS_OK,
                                                         _("Could not enable mouse accessibility features"));
                        gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                                                  _("Mouse accessibility requires Mousetweaks "
                                                                    "to be installed on your system."));
                        gtk_window_set_title (GTK_WINDOW (dialog),
                                              _("Mouse Preferences"));
                        gtk_window_set_icon_name (GTK_WINDOW (dialog),
                                                  "input-mouse");
                        gtk_dialog_run (GTK_DIALOG (dialog));
                        gtk_widget_destroy (dialog);
                }
                g_error_free (error);
        }
        g_free (comm);
}

static void
set_mouse_settings (MsdMouseManager *manager)
{
        gboolean left_handed = g_settings_get_boolean (manager->priv->settings_mouse, KEY_MOUSE_LEFT_HANDED);

        set_left_handed (manager, left_handed);
        set_motion_acceleration (manager, g_settings_get_double (manager->priv->settings_mouse, KEY_MOUSE_MOTION_ACCELERATION));
        set_motion_threshold (manager, g_settings_get_int (manager->priv->settings_mouse, KEY_MOUSE_MOTION_THRESHOLD));
	set_middle_button (manager, g_settings_get_boolean (manager->priv->settings_mouse, KEY_MIDDLE_BUTTON_EMULATION));

        set_disable_w_typing (manager, g_settings_get_boolean (manager->priv->settings_touchpad, KEY_TOUCHPAD_DISABLE_W_TYPING));
#ifdef HAVE_X11_EXTENSIONS_XINPUT_H
        set_tap_to_click (g_settings_get_boolean (manager->priv->settings_touchpad, KEY_TOUCHPAD_TAP_TO_CLICK), left_handed);
        set_edge_scroll (g_settings_get_int (manager->priv->settings_touchpad, KEY_TOUCHPAD_SCROLL_METHOD));
        set_horiz_scroll (g_settings_get_boolean (manager->priv->settings_touchpad, KEY_TOUCHPAD_PAD_HORIZ_SCROLL));
        set_touchpad_enabled (g_settings_get_boolean (manager->priv->settings_touchpad, KEY_TOUCHPAD_ENABLED));
#endif
}

static void
mouse_callback (GSettings          *settings,
                const gchar        *key,
                MsdMouseManager    *manager)
{
        if (g_strcmp0 (key, KEY_MOUSE_LEFT_HANDED) == 0) {
                set_left_handed (manager, g_settings_get_boolean (settings, key));
        } else if (g_strcmp0 (key, KEY_MOUSE_MOTION_ACCELERATION) == 0) {
                set_motion_acceleration (manager, g_settings_get_double (settings, key));
        } else if (g_strcmp0 (key, KEY_MOUSE_MOTION_THRESHOLD) == 0) {
                set_motion_threshold (manager, g_settings_get_int (settings, key));
        } else if (g_strcmp0 (key, KEY_TOUCHPAD_DISABLE_W_TYPING) == 0) {
                set_disable_w_typing (manager, g_settings_get_boolean (settings, key));
	} else if (g_str_equal (key, KEY_MIDDLE_BUTTON_EMULATION)) {
	        set_middle_button (manager, g_settings_get_boolean (settings, key));
#ifdef HAVE_X11_EXTENSIONS_XINPUT_H
        } else if (g_strcmp0 (key, KEY_TOUCHPAD_TAP_TO_CLICK) == 0) {
                set_tap_to_click (g_settings_get_boolean (settings, key),
                                  g_settings_get_boolean (manager->priv->settings_mouse, KEY_MOUSE_LEFT_HANDED));
        } else if (g_strcmp0 (key, KEY_TOUCHPAD_SCROLL_METHOD) == 0) {
                set_edge_scroll (g_settings_get_int (settings, key));
                set_horiz_scroll (g_settings_get_boolean (settings, KEY_TOUCHPAD_PAD_HORIZ_SCROLL));
        } else if (g_strcmp0 (key, KEY_TOUCHPAD_PAD_HORIZ_SCROLL) == 0) {
                set_horiz_scroll (g_settings_get_boolean (settings, key));
#endif
        } else if (g_strcmp0 (key, KEY_MOUSE_LOCATE_POINTER) == 0) {
                set_locate_pointer (manager, g_settings_get_boolean (settings, key));
#ifdef HAVE_X11_EXTENSIONS_XINPUT_H
        } else if (g_strcmp0 (key, KEY_TOUCHPAD_ENABLED) == 0) {
                set_touchpad_enabled (g_settings_get_boolean (settings, key));
#endif
        } /*else if (g_strcmp0 (key, KEY_MOUSE_A11Y_DWELL_ENABLE) == 0) {
                set_mousetweaks_daemon (manager,
                                        g_settings_get_boolean (settings, key),
                                        g_settings_get_boolean (settings, KEY_MOUSE_A11Y_DELAY_ENABLE, NULL));
                }
        } else if (g_strcmp0 (key, KEY_MOUSE_A11Y_DELAY_ENABLE) == 0) {
                set_mousetweaks_daemon (manager,
                                        g_settings_get_boolean (settings, KEY_MOUSE_A11Y_DWELL_ENABLE, NULL),
                                        g_settings_get_boolean (settings, key));
        }*/
}

static void
msd_mouse_manager_init (MsdMouseManager *manager)
{
        manager->priv = MSD_MOUSE_MANAGER_GET_PRIVATE (manager);
}

static gboolean
msd_mouse_manager_idle_cb (MsdMouseManager *manager)
{
        mate_settings_profile_start (NULL);

        manager->priv->settings_mouse = g_settings_new (MATE_MOUSE_SCHEMA);
        manager->priv->settings_touchpad = g_settings_new (MATE_TOUCHPAD_SCHEMA);

        g_signal_connect (manager->priv->settings_mouse, "changed",
                          G_CALLBACK (mouse_callback), manager);
        g_signal_connect (manager->priv->settings_touchpad, "changed",
                          G_CALLBACK (mouse_callback), manager);

        manager->priv->syndaemon_spawned = FALSE;

#ifdef HAVE_X11_EXTENSIONS_XINPUT_H
        set_devicepresence_handler (manager);
#endif
        set_mouse_settings (manager);
        set_locate_pointer (manager, g_settings_get_boolean (manager->priv->settings_mouse, KEY_MOUSE_LOCATE_POINTER));
        /*
        set_mousetweaks_daemon (manager,
                                g_settings_get_boolean (manager->priv->settings_mouse_a11y, 
                                                        KEY_MOUSE_A11Y_DWELL_ENABLE),
                                g_settings_get_boolean (manager->priv->settings_mouse_a11y,
                                                        KEY_MOUSE_A11Y_DELAY_ENABLE));
        */

        set_disable_w_typing (manager, g_settings_get_boolean (manager->priv->settings_touchpad, KEY_TOUCHPAD_DISABLE_W_TYPING));
#ifdef HAVE_X11_EXTENSIONS_XINPUT_H
        set_tap_to_click (g_settings_get_boolean (manager->priv->settings_touchpad, KEY_TOUCHPAD_TAP_TO_CLICK),
                          g_settings_get_boolean (manager->priv->settings_mouse, KEY_MOUSE_LEFT_HANDED));
        set_edge_scroll (g_settings_get_int (manager->priv->settings_touchpad, KEY_TOUCHPAD_SCROLL_METHOD));
        set_horiz_scroll (g_settings_get_boolean (manager->priv->settings_touchpad, KEY_TOUCHPAD_PAD_HORIZ_SCROLL));
        set_touchpad_enabled (g_settings_get_boolean (manager->priv->settings_touchpad, KEY_TOUCHPAD_ENABLED));
#endif

        mate_settings_profile_end (NULL);

        return FALSE;
}

gboolean
msd_mouse_manager_start (MsdMouseManager *manager,
                         GError         **error)
{
        mate_settings_profile_start (NULL);

        g_idle_add ((GSourceFunc) msd_mouse_manager_idle_cb, manager);

        mate_settings_profile_end (NULL);

        return TRUE;
}

void
msd_mouse_manager_stop (MsdMouseManager *manager)
{
        MsdMouseManagerPrivate *p = manager->priv;

        g_debug ("Stopping mouse manager");

        if (p->settings_mouse != NULL) {
                g_object_unref(p->settings_mouse);
                p->settings_mouse = NULL;
        }

        if (p->settings_touchpad != NULL) {
                g_object_unref(p->settings_touchpad);
                p->settings_touchpad = NULL;
        }

        set_locate_pointer (manager, FALSE);

#ifdef HAVE_X11_EXTENSIONS_XINPUT_H
        gdk_window_remove_filter (NULL, devicepresence_filter, manager);
#endif
}

static void
msd_mouse_manager_finalize (GObject *object)
{
        MsdMouseManager *mouse_manager;

        g_return_if_fail (object != NULL);
        g_return_if_fail (MSD_IS_MOUSE_MANAGER (object));

        mouse_manager = MSD_MOUSE_MANAGER (object);

        g_return_if_fail (mouse_manager->priv != NULL);

        G_OBJECT_CLASS (msd_mouse_manager_parent_class)->finalize (object);
}

MsdMouseManager *
msd_mouse_manager_new (void)
{
        if (manager_object != NULL) {
                g_object_ref (manager_object);
        } else {
                manager_object = g_object_new (MSD_TYPE_MOUSE_MANAGER, NULL);
                g_object_add_weak_pointer (manager_object,
                                           (gpointer *) &manager_object);
        }

        return MSD_MOUSE_MANAGER (manager_object);
}
