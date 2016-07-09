#include <gio/gio.h>

int
main (int argc, char *argv[])
{
  GDBusConnection *bus;

  bus = g_dbus_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);

}
