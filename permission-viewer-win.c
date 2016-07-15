#include <string.h>
#include <gtk/gtk.h>

#include "permission-viewer-app.h"
#include "permission-viewer-win.h"
#include "xdp-dbus.h"

struct _PermissionViewerWin
{
  GtkApplicationWindow parent;

  XdpPermissionStore *impl;
  GtkTreeStore *store;
  GtkTreeView *view;
};

struct _PermissionViewerWinClass
{
  GtkApplicationWindowClass parent_class;
};

G_DEFINE_TYPE (PermissionViewerWin, permission_viewer_win, GTK_TYPE_APPLICATION_WINDOW);

enum {
  COL_TABLE,
  COL_ID,
  COL_APP_ID,
  COL_NAME,
  COL_PERMISSIONS,
  COL_DATA,
  COL_EDITABLE
};

static void
tree_store_remove_children (GtkTreeStore *store,
                            GtkTreeIter *iter)
{
  GtkTreeIter child;

  while (gtk_tree_model_iter_children (GTK_TREE_MODEL (store), &child, iter))
    gtk_tree_store_remove (store, &child);
}

static void
add_permissions_to_store (PermissionViewerWin *win,
                          GtkTreeIter *iter,
                          const char *table,
                          const char *id,
                          GVariant *permissions)
{
  GtkTreeIter child;
  int j;
  GtkTreePath *path = NULL;
  gboolean expanded;
  gboolean editable;

  path = gtk_tree_model_get_path (GTK_TREE_MODEL (win->store), iter);
  expanded = gtk_tree_view_row_expanded (GTK_TREE_VIEW (win->view), path);

  tree_store_remove_children (win->store, iter);

  /* not dealing with geoclue yet */
  if (g_strcmp0 (table, "gnome") == 0)
    editable = FALSE;
  else
    editable = TRUE;

  for (j = 0; j < g_variant_n_children (permissions); j++)
    {
      const char *app;
      const char **perms;
      g_autofree char *value = NULL;

      g_variant_get_child (permissions, j, "{&s^a&s}", &app, &perms);
      value = g_strjoinv (", ", (char **)perms);

      gtk_tree_store_append (win->store, &child, iter);
      gtk_tree_store_set (win->store, &child,
                          COL_TABLE, table,
                          COL_ID, id,
                          COL_APP_ID, app,
                          COL_NAME, app,
                          COL_PERMISSIONS, value,
                          COL_EDITABLE, editable,
                          -1);
    }

  if (expanded)
    gtk_tree_view_expand_row (GTK_TREE_VIEW (win->view), path, TRUE);

  gtk_tree_path_free (path);
}

static void
add_table (PermissionViewerWin *win,
           const char *table)
{
  GtkTreeIter iter;
  GtkTreeIter parent;
  char **ids;
  int i;

  gtk_tree_store_append (win->store, &iter, NULL);
  gtk_tree_store_set (win->store, &iter,
                      COL_TABLE, table,
                      COL_NAME, table,
                      -1);

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
      gtk_tree_store_set (win->store, &iter,
                          COL_TABLE, table,
                          COL_ID, ids[i],
                          COL_NAME, ids[i],
                          COL_DATA, txt,
                          -1);

      child = iter;
      add_permissions_to_store (win, &iter, table, ids[i], permissions);
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
delete_row (GtkWidget *button, PermissionViewerWin *win)
{
  g_autofree char *table = NULL;
  g_autofree char *id = NULL;
  g_autofree char *app_id = NULL;
  GtkTreePath *path;
  GtkTreeIter iter;
  GtkWidget *popover;

  path = (GtkTreePath *) g_object_get_data (G_OBJECT (button), "path");
  gtk_tree_model_get_iter (GTK_TREE_MODEL (win->store), &iter, path);
  gtk_tree_model_get (GTK_TREE_MODEL (win->store), &iter,
                      COL_TABLE, &table,
                      COL_ID, &id,
                      COL_APP_ID, &app_id,
                      -1);

  if (app_id == NULL)
    {
      g_debug ("deleting item '%s' from table '%s'", id, table);
      xdp_permission_store_call_delete_sync (win->impl, table, id, NULL, NULL);
    }
  else
    {
      GVariantBuilder builder;
      g_autoptr(GVariant) perms = NULL;
      g_autoptr(GVariant) data = NULL;
      int i;

      g_debug ("deleting app '%s' in item '%s' from table '%s'", app_id, id, table);
      xdp_permission_store_call_lookup_sync (win->impl, table, id,
                                             &perms, &data,
                                             NULL, NULL);
      g_variant_builder_init (&builder, G_VARIANT_TYPE("a{sas}"));
      for (i = 0; i < g_variant_n_children (perms); i++)
        {
          const char *key;
          g_autoptr(GVariant) value = NULL;

          g_variant_get_child (perms, i, "{&s@as}", &key, &value);
          if (g_strcmp0 (key, app_id) != 0)
            g_variant_builder_add (&builder, "{s@as}", key, value);
        }
      xdp_permission_store_call_set_sync (win->impl, table, TRUE, id,
                                          g_variant_builder_end (&builder),
                                          data,
                                          NULL, NULL);
    }

  popover = gtk_widget_get_ancestor (button, GTK_TYPE_POPOVER);
  gtk_widget_hide (popover);
}

static void
add_row (GtkWidget *button, PermissionViewerWin *win)
{
  g_autofree char *table = NULL;
  g_autofree char *id = NULL;
  g_autofree char *app_id = NULL;
  GtkTreePath *path;
  GtkTreeIter iter;
  GtkTreeIter child;
  GtkWidget *popover;
  GVariantBuilder builder;
  GVariantBuilder pb;
  GVariant* perms = NULL;
  GVariant* newperms = NULL;
  GVariant* data = NULL;
  int i;

  path = (GtkTreePath *) g_object_get_data (G_OBJECT (button), "path");
  gtk_tree_model_get_iter (GTK_TREE_MODEL (win->store), &iter, path);
  gtk_tree_model_get (GTK_TREE_MODEL (win->store), &iter,
                      COL_TABLE, &table,
                      COL_ID, &id,
                      COL_APP_ID, &app_id,
                      -1);

  xdp_permission_store_call_lookup_sync (win->impl, table, id,
                                         &perms, &data,
                                         NULL, NULL);

  g_debug ("adding row in table '%s' for item '%s'", table, id);

  g_variant_builder_init (&builder, G_VARIANT_TYPE("a{sas}"));
  for (i = 0; perms && i < g_variant_n_children (perms); i++)
    {
      const char *key;
      g_autoptr(GVariant) value = NULL;

      g_variant_get_child (perms, i, "{&s@as}", &key, &value);
      g_variant_builder_add (&builder, "{s@as}", key, value);
    }

  g_variant_builder_init (&pb, G_VARIANT_TYPE_STRING_ARRAY);
  g_variant_builder_add (&pb, "s", "x");
  g_variant_builder_add (&builder, "{&s@as}", "x", g_variant_builder_end (&pb));
  newperms = g_variant_builder_end (&builder);
  xdp_permission_store_call_set_sync (win->impl, table, TRUE, id,
                                      newperms,
                                      data ? data : g_variant_new_byte (0),
                                      NULL, NULL);

  popover = gtk_widget_get_ancestor (button, GTK_TYPE_POPOVER);
  gtk_widget_hide (popover);
}

static gboolean
button_press_cb (GtkWidget *widget,
                 GdkEventButton *event,
                 PermissionViewerWin *win)
{
  GtkTreeView *tv = GTK_TREE_VIEW (widget);
  GtkTreePath *path;
  GtkTreeViewColumn *col;
  GtkTreeIter iter;
  GtkTreeIter parent;
  GdkRectangle rect;
  GtkWidget *button;
  GtkWidget *box;
  GtkWidget *popover;

  if (!gdk_event_triggers_context_menu ((GdkEvent *)event) ||
      !gtk_tree_view_get_path_at_pos (tv, event->x, event->y, &path, &col, NULL, NULL))
    return FALSE;

  gtk_tree_model_get_iter (GTK_TREE_MODEL (win->store), &iter, path);
  gtk_tree_view_get_cell_area (tv, path, col, &rect);
  gtk_tree_view_convert_bin_window_to_widget_coords (tv, rect.x, rect.y, &rect.x, &rect.y);

  if (gtk_tree_path_get_depth (path) == 1)
    return FALSE;

  popover = gtk_popover_new (GTK_WIDGET (tv));
  gtk_popover_set_pointing_to (GTK_POPOVER (popover), &rect);
  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_widget_show (box);
  gtk_container_add (GTK_CONTAINER (popover), box);


  button = gtk_button_new_with_label ("Delete");
  g_object_set_data_full (G_OBJECT (button), "path", gtk_tree_path_copy (path), (GDestroyNotify)gtk_tree_path_free);
  g_signal_connect (button, "clicked", G_CALLBACK (delete_row), win);

  gtk_widget_show (button);
  gtk_container_add (GTK_CONTAINER (box), button);

  if (gtk_tree_path_get_depth (path) == 2)
    {
      button = gtk_button_new_with_label ("Add");
      g_object_set_data_full (G_OBJECT (button), "path", gtk_tree_path_copy (path), (GDestroyNotify)gtk_tree_path_free);
      g_signal_connect (button, "clicked", G_CALLBACK (add_row), win);

      gtk_widget_show (button);
      gtk_container_add (GTK_CONTAINER (box), button);
    }

  gtk_widget_show (popover);

  g_signal_connect (popover, "unmap", G_CALLBACK (gtk_widget_destroy), NULL);

  gtk_tree_path_free (path);
}

static void
apply_change (PermissionViewerWin *win,
              const char *table,
              const char *id,
              const char *app_id,
              const char *new_app_id,
              GVariant *new_perms)
{
  g_autoptr(GVariant) perms = NULL;
  g_autoptr(GVariant) data = NULL;
  GVariantBuilder builder;
  int i;

  xdp_permission_store_call_lookup_sync (win->impl, table, id,
                                         &perms, &data,
                                         NULL, NULL);

  g_variant_builder_init (&builder, G_VARIANT_TYPE("a{sas}"));
  for (i = 0; i < g_variant_n_children (perms); i++)
    {
      const char *key;
      g_autoptr(GVariant) value = NULL;

      g_variant_get_child (perms, i, "{&s@as}", &key, &value);
      if (g_strcmp0 (key, app_id) == 0)
        g_variant_builder_add (&builder, "{s@as}",
                               new_app_id ? new_app_id : key,
                               new_perms ? new_perms : value);
      else
        g_variant_builder_add (&builder, "{s@as}", key, value);
    }

  xdp_permission_store_call_set_sync (win->impl, table, TRUE, id,
                                      g_variant_builder_end (&builder),
                                      data,
                                      NULL, NULL);
}

static void
name_edited (GtkCellRendererText *renderer,
             const char *path_text,
             const char *new_text,
             PermissionViewerWin *win)
{
  GtkTreePath *path;
  GtkTreeIter iter;
  g_autofree char *table = NULL;
  g_autofree char *id = NULL;
  g_autofree char *app_id = NULL;

  path = gtk_tree_path_new_from_string (path_text);
  gtk_tree_model_get_iter (GTK_TREE_MODEL (win->store), &iter, path);
  gtk_tree_path_free (path);
  gtk_tree_model_get (GTK_TREE_MODEL (win->store), &iter,
                      COL_TABLE, &table,
                      COL_ID, &id,
                      COL_APP_ID, &app_id,
                      -1);

  g_debug ("changing app id %s -> %s", app_id, new_text);

  apply_change (win, table, id, app_id, new_text, NULL);
}

static GVariant *
parse_permissions (PermissionViewerWin *win,
                   const char *table,
                   const char *text)
{
  g_auto(GStrv) strv = NULL;
  int length;
  int i;

  strv = g_strsplit (text, ",", -1);
  length = g_strv_length (strv);
  for (i = 0; strv[i]; i++)
    strv[i] = g_strstrip (strv[i]);

  if (g_strcmp0 (table, "documents") == 0)
    {
      const char * const valid[] = { "read", "write", "grant-permissions", "delete", NULL };
      for (i = 0; strv[i]; i++)
        {
           if (!g_strv_contains (valid, strv[i]))
             {
               g_warning ("%s not a valid permission for documents", strv[i]);
               return NULL;
             }
        }
    }
  else if (g_strcmp0 (table, "desktop-used-apps") == 0)
    {
      gint64 num;
      char *end;

      if (length != 2 && length != 3)
        {
          g_warning ("permissions must have 2 or 3 members for desktop-used-apps");
          return NULL;
        }
      if (strlen (strv[0]) == 0)
        {
          g_warning ("app name can't be empty in desktop-used-apps");
          return NULL;
        }
      end = NULL;
      num = g_ascii_strtoll (strv[1], &end, 10);
      if (end && end[0] != 0)
        {
          g_warning ("count invalid in desktop-used-apps");
          return NULL;
        }
      if (num < 0)
        {
          g_warning ("count negative in desktop-used-apps");
          return NULL;
        }

      if (length == 3)
        {
          end = NULL;
          num = g_ascii_strtoll (strv[2], &end, 10);
          if (end && end[0] != 0)
            {
              g_warning ("threshold invalid in desktop-used-apps");
              return NULL;
            }
          if (num < 0)
            {
              g_warning ("threshold negative in desktop-used-apps");
              return NULL;
            }
        }
    }

  return g_variant_ref_sink (g_variant_new_strv ((const char *const *)strv, -1));
}

static void
permissions_edited (GtkCellRendererText *renderer,
                    const char *path_text,
                    const char *new_text,
                    PermissionViewerWin *win)
{
  GtkTreePath *path;
  GtkTreeIter iter;
  g_autofree char *table = NULL;
  g_autofree char *id = NULL;
  g_autofree char *app_id = NULL;
  g_autofree char *permissions = NULL;
  g_autoptr(GVariant) new_perms = NULL;

  path = gtk_tree_path_new_from_string (path_text);
  gtk_tree_model_get_iter (GTK_TREE_MODEL (win->store), &iter, path);
  gtk_tree_path_free (path);
  gtk_tree_model_get (GTK_TREE_MODEL (win->store), &iter,
                      COL_TABLE, &table,
                      COL_ID, &id,
                      COL_APP_ID, &app_id,
                      COL_PERMISSIONS, &permissions,
                      -1);

  new_perms = parse_permissions (win, table, new_text);
  if (new_perms == NULL)
    return;

  g_debug ("changing permissions %s -> %s", permissions, new_text);

  apply_change (win, table, id, app_id, NULL, new_perms);
}

static void
data_edited (GtkCellRendererText *renderer,
             const char *path,
             const char *new_text,
             PermissionViewerWin *win)
{
  g_debug ("new data: %s", new_text);
}

static void
changed_cb (XdpPermissionStore *object,
            const char *table,
            const char *id,
            gboolean deleted,
            GVariant *data,
            GVariant *permissions,
            PermissionViewerWin *win)
{
  GtkTreeIter iter, child;

  g_debug ("%s: table: %s, item %s, permissions %s", deleted ? "deleted" : "changed", table, id, g_variant_print (permissions, 0));

  gtk_tree_model_iter_children (GTK_TREE_MODEL (win->store), &iter, NULL);
  do {
    g_autofree char *t = NULL;
    gtk_tree_model_get (GTK_TREE_MODEL (win->store), &iter,
                        COL_TABLE, &t,
                        -1);
    if (g_strcmp0 (table, t) == 0)
      {
        gtk_tree_model_iter_children (GTK_TREE_MODEL (win->store), &child, &iter);
        do {
          g_autofree char *i = NULL;
          gtk_tree_model_get (GTK_TREE_MODEL (win->store), &child,
                              COL_ID, &i,
                              -1);
          if (g_strcmp0 (id, i) == 0)
            {
              if (deleted)
                gtk_tree_store_remove (win->store, &child);
              else
                add_permissions_to_store (win, &child, table, id, permissions);
              return;
            }
        } while (gtk_tree_model_iter_next (GTK_TREE_MODEL (win->store), &child));
      }
  } while (gtk_tree_model_iter_next (GTK_TREE_MODEL (win->store), &iter));
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
  g_signal_connect (win->impl, "changed", G_CALLBACK (changed_cb), win);
  populate (win);
}

static void
permission_viewer_win_class_init (PermissionViewerWinClass *class)
{
  gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (class),
                                               "/org/gtk/permission-viewer/permission-viewer-win.ui");
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), PermissionViewerWin, store);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), PermissionViewerWin, view);
  gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (class), button_press_cb);
  gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (class), name_edited);
  gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (class), permissions_edited);
  gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (class), data_edited);
}

GtkApplicationWindow *
permission_viewer_win_new (PermissionViewerApp *app)
{
  return g_object_new (permission_viewer_win_get_type (), "application", app, NULL);
}
