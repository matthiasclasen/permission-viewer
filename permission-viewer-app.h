#pragma once

#include <gtk/gtk.h>

G_DECLARE_FINAL_TYPE(PermissionViewerApp, permission_viewer_app, PERMISSION, VIEWER_APP, GtkApplication)

GType            permission_viewer_app_get_type       (void);
GApplication    *permission_viewer_app_new            (void);
