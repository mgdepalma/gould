##
# Copyright (C) Generations Linux <bugs@softcraft.org>
# src/programs/Makefile.am for the GOULD project
#
MAINTAINERCLEANFILES = Makefile.in

ALL_CFLAGS = -Wno-deprecated-declarations
PKG_DEFINE = -D_GNU_SOURCE -DLOCALEDIR=\""$(datadir)/locale"\" 
PKG_DEFINE += -DCONFIG_FILE=\"@sysconfdir@/X11/@PACKAGE@.conf\"
PKG_CFLAGS = $(PKG_DEFINE) $(ALL_CFLAGS) -I../common -Wall -g -O -pipe

AM_CFLAGS  = $(PKG_CFLAGS) `pkg-config --cflags gtk+-2.0 libxml-2.0`
AM_LDFLAGS = -Wl,-export-dynamic `pkg-config --libs gtk+-2.0 libxml-2.0`
LIBS = -L../common -lgould -lX11

bin_PROGRAMS = \
gpanel \
gsession

noinst_PROGRAMS = \
gcontrol \
prova

# additional LDFLAGS needed by gpanel
gpanel_LDADD = -lgthread-2.0

# program source dependencies
gpanel_SOURCES = gpanel.h gpanel.c \
                          menu.c \
                          taskbar.c \
                          desktop.c \
                          setting.c \
                          wallpaper.c \
                          saver.c \
			  gsession.h

gcontrol_SOURCES  = gcontrol.h gcontrol.c

#/
