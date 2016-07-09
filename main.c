#include <gtk/gtk.h>

#include "permission-viewer-app.h"

int
main (int argc, char *argv[])
{
  return g_application_run (permission_viewer_app_new (), argc, argv);
}
