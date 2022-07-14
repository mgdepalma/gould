##
# Generations Linux <bugs@softcraft.org>
# src/programs/Makefile.am for the GOULD project
#
SUBDIRS =
MAINTAINERCLEANFILES = Makefile.in

PKG_DEFINE = -D_GNU_SOURCE
PKG_CFLAGS = $(PKG_DEFINE) -Wall -g -O -pipe
PKG_LIBS = `pkg-config --libs gtk+-2.0 x11 dbus-1`

GTK_CFLAGS = `pkg-config --cflags gtk+-2.0 libxml-2.0`


bin_PROGRAMS = gould greeter

gould_CFLAGS = \
	$(GTK_CFLAGS) \
	-DGOULD_DATA_DIR=@datadir@/@PACKAGE@ \
	-DCONFIG_FILE=\"@sysconfdir@/X11/@PACKAGE@.conf\" \
	-DXSESSIONS_DIR=\"/usr/share/xsessions\" \
	-Werror-implicit-function-declaration \
	-Wall

gould_LDFLAGS = $(PKG_LIBS)

gould_SOURCES = \
	gould.c gould.h \
	interface.c \
	util.c util.h \
	$(NULL)

greeter_CFLAGS = \
	$(GTK_CFLAGS) \
	-DGOULD_DATA_DIR=\"@datadir@/@PACKAGE@\" \
	-DCONFIG_FILE=\"@sysconfdir@/X11/@PACKAGE@.conf\" \
	-DXSESSIONS_DIR=\"/usr/share/xsessions\" \
	-DLIBLOCALEDIR=\"/usr/lib/locale\" \
	-DLOCALEDIR=\"/usr/share/locale\" \
	-DISO_CODES_PREFIX=\"/usr\" \
	-Werror-implicit-function-declaration

greeter_LDFLAGS = `pkg-config --libs gtk+-2.0 x11`

greeter_SOURCES = \
	greeter.c \
	lang.c lang.h \
	gdm/gdm-languages.c gdm/gdm-languages.h \
	gdm/locarchive.h \
	util.c util.h \
	$(NULL)
