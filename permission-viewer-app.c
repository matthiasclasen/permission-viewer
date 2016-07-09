#include <gtk/gtk.h>

#include "permission-viewer-app.h"
#include "permission-viewer-win.h"

struct _PermissionViewerApp
{
  GtkApplication parent;
};

struct _PermissionViewerAppClass
{
  GtkApplicationClass parent_class;
};

G_DEFINE_TYPE(PermissionViewerApp, permission_viewer_app, GTK_TYPE_APPLICATION)

static void
permission_viewer_app_init (PermissionViewerApp *app)
{
}

static void
permission_viewer_app_startup (GApplication *app)
{
  G_APPLICATION_CLASS (permission_viewer_app_parent_class)->startup (app);
}

static void
permission_viewer_app_activate (GApplication *app)
{
  GtkApplicationWindow *win;

  win = permission_viewer_win_new (PERMISSION_VIEWER_APP (app));
  gtk_window_present (GTK_WINDOW (win));
}

static void
permission_viewer_app_class_init (PermissionViewerAppClass *class)
{
  G_APPLICATION_CLASS (class)->startup = permission_viewer_app_startup;
  G_APPLICATION_CLASS (class)->activate = permission_viewer_app_activate;
}

GApplication *
permission_viewer_app_new (void)
{
  GApplication *app;

  app = g_object_new (permission_viewer_app_get_type (),
                      "application-id", "org.gnome.PermissionViewer",
                      NULL);

  return app;
}
