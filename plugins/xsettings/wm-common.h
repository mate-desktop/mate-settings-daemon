#ifndef WM_COMMON_H
#define WM_COMMON_H

#define WM_COMMON_MARCO       "Metacity (Marco)"
#define WM_COMMON_SAWFISH     "Sawfish"
#define WM_COMMON_METACITY    "Metacity"
#define WM_COMMON_COMPIZ      "Compiz"
#define WM_COMMON_COMPIZ_OLD  "compiz"
#define WM_COMMON_UNKNOWN     "Unknown"

gchar *wm_common_get_current_window_manager (void);
void   wm_common_update_window (void);

#endif /* WM_COMMON_H */

