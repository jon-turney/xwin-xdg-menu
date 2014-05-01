AC_INIT(xwin-xdm-menu, m4_esyscmd([cat version | tr -d '\n']))
AM_INIT_AUTOMAKE([-Wall -Werror foreign])
AM_SILENT_RULES([yes])
AC_PROG_CC
AC_CHECK_TOOL(WINDRES, windres)
PKG_CHECK_MODULES(GMENU, [libgnome-menu-3.0 gtk+-2.0])
AC_CONFIG_FILES([Makefile])
AC_OUTPUT
