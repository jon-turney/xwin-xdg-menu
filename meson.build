project('xwin-xdg-menu', 'c', default_options: ['warning_level=2'], meson_version: '>=0.47.0')
add_global_arguments('-Wno-unused-parameter', language: 'c')

cc = meson.get_compiler('c')
gdi32 = cc.find_library('gdi32')
gmenu = dependency('libgnome-menu-3.0')
gtk = dependency('gtk+-2.0')

convert = find_program('convert')
X_ico = custom_target('X.ico',
                      input: 'X.svg',
                      output: 'X.ico',
                      command: [convert, '-background', 'transparent', '@INPUT@',
                                '-trim', '-define', 'icon:auto-resize', '@OUTPUT@'])

version_h = vcs_tag('version.h',
                    input: 'version.h.in',
                    output: 'version.h',
                    command: ['git', 'describe', '--long', '--dirty', '--always'])

windows = import('windows')
rc = find_program('windres')
res = files('resource.rc')
res_deps = files('cygwinx.ico', 'resource.h', 'xwin-xdg-menu.exe.manifest')
resource_o = windows.compile_resources(res,
                                       depends: [X_ico, version_h],
                                       depend_files: res_deps)

srcs = files('main.c',
             'execute.c', 'execute.h',
             'menu.c', 'menu.h',
             'msgwindow.c', 'msgwindow.h',
             'trayicon.c', 'trayicon.h')
exe = executable('xwin-xdg-menu', srcs, resource_o,
                 c_args: '-D_GNU_SOURCE',
                 dependencies: [gtk, gmenu, gdi32],
                 install: true)

install_data('X-Cygwin-Settings.directory',
             install_dir: join_paths(get_option('datadir'), 'desktop-directories'))
install_data('xwin-applications.menu',
             install_dir: join_paths(get_option('sysconfdir'), 'xdg', 'menus'))
install_man('xwin-xdg-menu.1')
