##
# Copyright (C) Generations Linux <bugs@softcraft.org>
# src/programs/Makefile.am for the GOULD project
#
MAINTAINERCLEANFILES = Makefile.in

ALL_CFLAGS  = -Wno-deprecated-declarations
ALL_LDFLAGS = -Wl,-export-dynamic

PKG_DEFINE  = -D_GNU_SOURCE -DLOCALEDIR=\""$(datadir)/locale"\" 
PKG_DEFINE += -DCONFIG_FILE=\"@sysconfdir@/X11/@PACKAGE@.conf\"
PKG_CFLAGS  = $(PKG_DEFINE) $(ALL_CFLAGS) -I../common -Wall -g -O -pipe

AM_CFLAGS  = $(PKG_CFLAGS) `pkg-config --cflags gtk+-2.0 libxml-2.0`
AM_LDFLAGS = $(ALL_LDFLAGS) `pkg-config --libs gtk+-2.0 libxml-2.0`

GLIB_COMPILE_RESOURCES = /usr/bin/glib-compile-resources
GCRARGS = --sourcedir=$(srcdir) --generate-source

LIBS = -L../common -lgould -lX11

bin_PROGRAMS = \
gpanel \
gsession

noinst_PROGRAMS = \
gscreen \
prova

# additional LDFLAGS needed by gpanel
gpanel_LDADD = -lgthread-2.0

# program source dependencies
gpanel_SOURCES = gsession.h \
		 gpanel.h \
		 gpanel.c \
		 desktop.c \
		 taskbar.c \
		 settings.c \
		 screensaver.c \
		 wallpaper.c \
		 dialog.c \
		 menus.c

gscreen_SOURCES  = gscreen.h gscreen.c

gresources.c: gresources.xml screensaver.ui
	$(GLIB_COMPILE_RESOURCES) $(srcdir)/gresources.xml --target=$@ $(GCRARGS)
#/
