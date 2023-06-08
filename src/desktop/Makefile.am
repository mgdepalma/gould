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
PKG_LDFLAGS = $(ALL_LDFLAGS)

AM_CFLAGS  = $(PKG_CFLAGS) `pkg-config --cflags gtk+-2.0 libxml-2.0`
AM_LDFLAGS = $(PKG_LDFLAGS) `pkg-config --libs gtk+-2.0 libxml-2.0`
#AM_CFLAGS  = $(PKG_CFLAGS) `pkg-config --cflags libgtkhtml-3.14 libxml-2.0`
#AM_LDFLAGS = $(PKG_LDFLAGS) `pkg-config --libs libgtkhtml-3.14 libxml-2.0`
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
gpanel_SOURCES = gpanel.h gsession.h \
		 gpanel.c \
		 desktop.c \
		 taskbar.c \
		 settings.c \
		 wallpaper.c \
		 dialog.c \
		 menus.c \
		 saver.c

gscreen_SOURCES  = gscreen.h gscreen.c

#/
