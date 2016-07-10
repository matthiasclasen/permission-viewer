#include <gtk/gtk.h>

#include "permission-viewer-app.h"
#include "permission-viewer-win.h"
#include "xdp-dbus.h"

struct _PermissionViewerWin
{
  GtkApplicationWindow parent;

  XdpPermissionStore *impl;
  GtkTreeStore *store;
};

struct _PermissionViewerWinClass
{
  GtkApplicationWindowClass parent_class;
};

G_DEFINE_TYPE (PermissionViewerWin, permission_viewer_win, GTK_TYPE_APPLICATION_WINDOW);

static void
add_table (PermissionViewerWin *win,
           const char *table)
{
  GtkTreeIter iter;
  GtkTreeIter parent;
  char **ids;
  int i;

  gtk_tree_store_append (win->store, &iter, NULL);
  gtk_tree_store_set (win->store, &iter, 0, table, -1);

  xdp_permission_store_call_list_sync (win->impl, table, &ids, NULL, NULL);
  parent = iter;
  for (i = 0; ids[i]; i++)
    {
      GtkTreeIter child;
      g_autoptr(GVariant) permissions = NULL;
      g_autoptr(GVariant) data = NULL;
      g_autoptr(GVariant) d = NULL;
      g_autofree char *txt = NULL;
      int j;

      xdp_permission_store_call_lookup_sync (win->impl, table, ids[i], &permissions, &data, NULL, NULL);

      d = g_variant_get_child_value (data, 0);
      txt = g_variant_print (d, 0);
      gtk_tree_store_append (win->store, &iter, &parent);
      gtk_tree_store_set (win->store, &iter, 0, ids[i], 2, txt, -1);

      child = iter;
      for (j = 0; j < g_variant_n_children (permissions); j++)
        {
          const char *app;
          const char **perms;
          g_autofree char *value = NULL;
          g_variant_get_child (permissions, j, "{&s^a&s}", &app, &perms);
          value = g_strjoinv (", ", (char **)perms);
          gtk_tree_store_append (win->store, &iter, &child);
          gtk_tree_store_set (win->store, &iter, 0, app, 1, value, -1);
        }
    }
}

static void
populate (PermissionViewerWin *win)
{
  g_autofree char *path = NULL;
  g_autoptr(GError) error = NULL;
  GDir *dir;
  const char *name;

  path = g_build_filename (g_get_user_data_dir (), "flatpak/db", NULL);
  dir = g_dir_open (path, 0, &error);
  if (dir == NULL)
    {
      g_warning ("No databases found: %s", error->message);
      return;
    }

  while ((name = g_dir_read_name (dir)) != NULL)
    add_table (win, name);

  g_dir_close (dir);
}

static void
permission_viewer_win_init (PermissionViewerWin *win)
{
  gtk_widget_init_template (GTK_WIDGET (win));

  win->impl = xdp_permission_store_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
                                                           G_DBUS_PROXY_FLAGS_NONE,
                                                           "org.freedesktop.impl.portal.PermissionStore",
                                                           "/org/freedesktop/impl/portal/PermissionStore",
                                                           NULL, NULL);
  populate (win);
}

static void
permission_viewer_win_class_init (PermissionViewerWinClass *class)
{
  gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (class),
                                               "/org/gtk/permission-viewer/permission-viewer-win.ui");
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), PermissionViewerWin, store);
}

GtkApplicationWindow *
permission_viewer_win_new (PermissionViewerApp *app)
{
  return g_object_new (permission_viewer_win_get_type (), "application", app, NULL);
}
