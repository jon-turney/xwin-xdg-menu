bin_PROGRAMS = xwin-xdg-menu

AM_CFLAGS = -Wall -Wextra -Wno-unused-parameter
AM_CPPFLAGS = $(GMENU_CFLAGS)
LDADD = $(GMENU_LIBS) -lgdi32

xwin_xdg_menu_SOURCES = \
	execute.c \
	execute.h \
	main.c \
	menu.c \
	menu.h \
	msgwindow.c \
	msgwindow.h \
	resource.rc \
	resource.h \
	trayicon.c \
	trayicon.h

.rc.o:
	$(AM_V_GEN)$(WINDRES) -i $< --input-format=rc -o $@ -O coff --use-temp-file -I$(builddir)

resource.o: resource.rc X.ico hippo.ico resource.h version.h xwin-xdg-menu.exe.manifest

X.ico: X.svg
	$(AM_V_GEN)convert -background transparent $(srcdir)/X.svg -trim -define icon:auto-resize X.ico

menudir = $(sysconfdir)/xdg/menus

menu_DATA = \
	xwin-applications.menu

dist_man1_MANS = xwin-xdg-menu.man

EXTRA_DIST = \
	update-version \
	version.h \
	X.svg \
	hippo.ico \
	xwin-xdg-menu.exe.manifest \
	$(menu_DATA)

CLEANFILES = \
	X.ico

DISTCLEANFILES = \
	version.h

version.h: update-version-if-changed
	$(AM_V_GEN)

.PHONY: force
update-version-if-changed: force
	@$(srcdir)/update-version
