 #include <X11/Xatom.h>
#include <gdk/gdkx.h>
#include <gdk/gdk.h>
#include <string.h>
#include <glib.h>
#include <glib-object.h>
#include "wm-common.h"

/* Our WM Window */
static Window wm_window = None;

static char *
wm_common_get_window_manager_property (Atom atom)
{
  Atom utf8_string, type;
  GdkDisplay *display;
  int result;
  char *retval;
  int format;
  gulong nitems;
  gulong bytes_after;
  gchar *val;

  if (wm_window == None)
    return NULL;

  utf8_string = gdk_x11_get_xatom_by_name ("UTF8_STRING");

  display = gdk_display_get_default ();

  gdk_x11_display_error_trap_push (display);

  val = NULL;
  result = XGetWindowProperty (GDK_DISPLAY_XDISPLAY (display),
		  	       wm_window,
			       atom,
			       0, G_MAXLONG,
			       False, utf8_string,
			       &type, &format, &nitems,
			       &bytes_after, (guchar **) &val);

  if (gdk_x11_display_error_trap_pop (display) || result != Success ||
      type != utf8_string || format != 8 || nitems == 0 ||
      !g_utf8_validate (val, nitems, NULL))
    {
      retval = NULL;
    }
  else
    {
      retval = g_strndup (val, nitems);
    }

  if (val)
    XFree (val);

  return retval;
}

char*
wm_common_get_current_window_manager (void)
{
  Atom atom = gdk_x11_get_xatom_by_name ("_NET_WM_NAME");
  char *result;

  result = wm_common_get_window_manager_property (atom);
  if (result)
    return result;
  else
    return g_strdup (WM_COMMON_UNKNOWN);
}

static void
update_wm_window (void)
{
  GdkDisplay *display;
  Window *xwindow;
  Atom type;
  gint format;
  gulong nitems;
  gulong bytes_after;

  display = gdk_display_get_default ();

  XGetWindowProperty (GDK_DISPLAY_XDISPLAY (display), GDK_ROOT_WINDOW (),
		      gdk_x11_get_xatom_by_name ("_NET_SUPPORTING_WM_CHECK"),
		      0, G_MAXLONG, False, XA_WINDOW, &type, &format,
		      &nitems, &bytes_after, (guchar **) &xwindow);

  if (type != XA_WINDOW)
    {
      wm_window = None;
     return;
    }

  gdk_x11_display_error_trap_push (display);
  XSelectInput (GDK_DISPLAY_XDISPLAY (display), *xwindow, StructureNotifyMask | PropertyChangeMask);
  XSync (GDK_DISPLAY_XDISPLAY (display), False);

  if (gdk_x11_display_error_trap_pop (display))
    {
       XFree (xwindow);
       wm_window = None;
       return;
    }

    wm_window = *xwindow;
    XFree (xwindow);
}

void
wm_common_update_window ()
{
  update_wm_window();
}
