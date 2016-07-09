BUILT_SOURCES =
EXTRA_DIST =

resource_files = $(shell $(GLIB_COMPILE_RESOURCES)  --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/permission-viewer.gresource.xml)
EXTRA_DIST += $(resource_files)

resources.h: permission-viewer.gresource.xml $(resource_files)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) $< 	\
		--target=$@ 				\
		--sourcedir=$(srcdir) 			\
		--c-name _permission_viewer 			\
		--generate-header

resources.c: permission-viewer.gresource.xml $(resource_files)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) $< 	\
		--target=$@ 				\
		--sourcedir=$(srcdir) 			\
		--c-name _permission_viewer 			\
		--generate-source

BUILT_SOURCES += $(resource_files)

dbus_built_sources = xdp-dbus.c xdp-dbus.h
BUILT_SOURCES += $(dbus_built_sources)

$(dbus_built_sources): Makefile.am
	$(AM_V_GEN) $(GDBUS_CODEGEN)                    			\
		--interface-prefix org.freedesktop.impl.portal. 		\
		--c-namespace Xdp                       			\
		--generate-c-code xdp-dbus			               	\
		$(FLATPAK_INTERFACES_DIR)/org.freedesktop.impl.portal.PermissionStore.xml 	\
		$(NULL)

bin_PROGRAMS = permission-viewer
permission_viewer_CFLAGS = $(PORTAL_TEST_CFLAGS)
permission_viewer_LDADD = $(PORTAL_TEST_LIBS)
permission_viewer_SOURCES = 				\
	main.c						\
	permission-viewer-app.h				\
	permission-viewer-app.c				\
	permission-viewer-win.h				\
	permission-viewer-win.c				\
	xdp-dbus.h					\
	xdp-dbus.c					\
	resources.h					\
	resources.c

desktopdir = $(datadir)/applications
desktop_DATA = org.gnome.PermissionViewer.desktop
