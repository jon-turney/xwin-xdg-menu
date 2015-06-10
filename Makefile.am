bin_PROGRAMS = xwin-xdg-menu

AM_CFLAGS = -Wall -Wextra
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
	$(AM_V_GEN)$(WINDRES) -i $< --input-format=rc -o $@ -O coff --use-temp-file -DVERSION=\"@VERSION@\"

resource.o: resource.rc X.ico resource.h

X.ico: X.svg
	$(AM_V_GEN)convert -background transparent $(srcdir)/X.svg -trim -define icon:auto-resize X.ico

menudir = $(sysconfdir)/xdg/menus

menu_DATA = \
	xwin-applications.menu

EXTRA_DIST = \
	version \
	X.svg \
	$(menu_DATA)

CLEANFILES = \
	X.ico
