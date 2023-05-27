##
# Copyright (C) Generations Linux <bugs@softcraft.org>
# src/common/Makefile.am for the GOULD project
#
MAINTAINERCLEANFILES = Makefile.in

PKG_CFLAGS = -D_GNU_SOURCE -Wall -Wno-deprecated-declarations -g -O -pipe
AM_CFLAGS  = $(PKG_CFLAGS) `pkg-config --cflags gtk+-2.0 libxml-2.0`
AM_LDFLAGS = -Wl,-export-dynamic `pkg-config --libs x11 libxml-2.0`

# libgould.a is needed by all the applications
lib_LTLIBRARIES = libgould.la

include_HEADERS = \
	module.h \
	dialog.h \
	docklet.h \
	print.h \
	window.h \
	grabber.h \
        iconbox.h \
	filechooser.h \
	xmlconfig.h \
	xpmglyphs.h \
	greenwindow.h \
	green.h \
	pager.h \
	tasklist.h \
	systray.h \
	xutil.h \
	util.h

libgould_SOURCES = \
        module.c \
	dialog.c \
	docklet.c \
	print.c \
	window.c \
	grabber.c \
        iconbox.c \
	filechooser.c \
	xmlconfig.c \
	xpmglyphs.c \
	greenwindow.c \
	green.c \
	pager.c \
	tasklist.c \
	systray.c \
	xutil.c \
	util.c

libgould_la_SOURCES = \
	$(include_HEADERS) \
	$(libgould_SOURCES)

libgould_la_LDFLAGS = \
	-version-info $(LIBGOULD_CURRENT):$(LIBGOULD_REVISION):$(LIBGOULD_AGE) \
	-no-undefined

# static libgould.a
libgould_OBJECTS = .libs/module.o .libs/dialog.o .libs/docklet.o .libs/print.o .libs/window.o .libs/grabber.o .libs/iconbox.o .libs/filechooser.o .libs/xmlconfig.o .libs/xpmglyphs.o .libs/greenwindow.o .libs/green.o .libs/pager.o .libs/tasklist.o .libs/systray.o .libs/xutil.o .libs/util.o

libgould.a: $(libgould_OBJECTS)
	ar -crs libgould.a $(libgould_OBJECTS)
#/
