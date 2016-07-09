#pragma once

#include <gtk/gtk.h>
#include "permission-viewer-app.h"

G_DECLARE_FINAL_TYPE(PermissionViewerWin, permission_viewer_win, PERMISSION, VIEWER_WIN, GtkApplicationWindow)

GType                    permission_viewer_win_get_type       (void);
GtkApplicationWindow    *permission_viewer_win_new            (PermissionViewerApp *app);
void                     permission_viewer_win_ack            (PermissionViewerWin *win);
