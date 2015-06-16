AC_INIT(xwin-xdg-menu, 0.0)
AM_INIT_AUTOMAKE([dist-xz no-dist-gzip -Wall -Werror foreign])
AM_SILENT_RULES([yes])
AC_PROG_CC
AC_CHECK_TOOL(WINDRES, windres)
PKG_CHECK_MODULES(GMENU, [libgnome-menu-3.0 gtk+-2.0])
AC_CONFIG_FILES([Makefile])
AC_OUTPUT
