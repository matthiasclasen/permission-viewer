project('permission-viewer', 'c',
  version: '1',
  meson_version: '>= 0.36.0',
)

gnome = import('gnome')

flatpak_interfaces_dir = dependency('flatpak').get_pkgconfig_variable('interfaces_dir')
permission_store_xml = join_paths(flatpak_interfaces_dir,
                                  'org.freedesktop.impl.portal.PermissionStore.xml')

xdp_dbus = gnome.gdbus_codegen('xdp-dbus',
  permission_store_xml,
  interface_prefix: 'org.freedesktop.impl.portal.',
  namespace: 'Xdp'
)

resources = gnome.compile_resources('resources',
  'permission-viewer.gresource.xml',
  c_name: '_permission_viewer'
)

executable('permission-viewer',
  sources: [
    resources[0],
    xdp_dbus,
    'main.c',
    'permission-viewer-app.c',
    'permission-viewer-win.c',
  ],
  dependencies: dependency('gtk+-3.0'),
  install: true,
)

install_data('org.gnome.PermissionViewer.desktop',
  install_dir: join_paths(get_option('datadir'), 'applications')
)
