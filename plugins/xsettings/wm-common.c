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
  int result;
  char *retval;
  int format;
  gulong nitems;
  gulong bytes_after;
  gchar *val;

  if (wm_window == None)
    return NULL;

  utf8_string = gdk_x11_get_xatom_by_name ("UTF8_STRING");

  gdk_error_trap_push ();

  val = NULL;
  result = XGetWindowProperty (GDK_DISPLAY_XDISPLAY(gdk_display_get_default()),
		  	       wm_window,
			       atom,
			       0, G_MAXLONG,
			       False, utf8_string,
			       &type, &format, &nitems,
			       &bytes_after, (guchar **) &val);

  if (gdk_error_trap_pop () || result != Success ||
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
  Window *xwindow;
  Atom type;
  gint format;
  gulong nitems;
  gulong bytes_after;

  XGetWindowProperty (GDK_DISPLAY_XDISPLAY(gdk_display_get_default()), GDK_ROOT_WINDOW (),
		      gdk_x11_get_xatom_by_name ("_NET_SUPPORTING_WM_CHECK"),
		      0, G_MAXLONG, False, XA_WINDOW, &type, &format,
		      &nitems, &bytes_after, (guchar **) &xwindow);

  if (type != XA_WINDOW)
    {
      wm_window = None;
     return;
    }

  gdk_error_trap_push ();
  XSelectInput (GDK_DISPLAY_XDISPLAY(gdk_display_get_default()), *xwindow, StructureNotifyMask | PropertyChangeMask);
  XSync (GDK_DISPLAY_XDISPLAY(gdk_display_get_default()), False);

  if (gdk_error_trap_pop ())
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
