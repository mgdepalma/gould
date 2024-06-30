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
gdisplay \
grdesktop \
gsnapshot

gdisplay_SOURCES  = gdisplay.h gdisplay.c
grdesktop_SOURCES = grdesktop.h grdesktop.c
gsnapshot_SOURCES = gsnapshot.h gsnapshot.c

gsnapshot_CFLAGS = $(AM_CFLAGS) `pkg-config --cflags gtk+-unix-print-2.0`
gsnapshot_LDFLAGS = $(AM_LDFLAGS) `pkg-config --libs  gtk+-unix-print-2.0`

gsnapshot_LDADD = jpeg2pdf.o

jpeg2pdf.o: ../common/jpeg2pdf.c ../common/jpeg2pdf.h
	$(CC) $(AM_CFLAGS) -c ../common/jpeg2pdf.c -o $@
#/
