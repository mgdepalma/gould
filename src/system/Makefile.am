##
# Generations Linux <bugs@softcraft.org>
# src/system/Makefile.am for the GOULD project
#
MAINTAINERCLEANFILES = Makefile.in

DEPKGS = "gtk+-2.0 libxml-2.0 rpm"
PKG_DEFINE = -D_GNU_SOURCE -DLOCALEDIR=\""/usr/share/locale"\"
PKG_CFLAGS = $(PKG_DEFINE) -I../common -Wall -g -O -pipe

AM_CFLAGS  = $(PKG_CFLAGS) `pkg-config --cflags $(DEPKGS)`
AM_LDFLAGS = -Wl,-export-dynamic

#LIBS = -lrpmbuild -lrpm -lrpmio -lpopt -llzma -L../common -lgould
LIBS = `pkg-config --libs $(DEPKGS)` -lgould

include_HEADERS = package.h interface.h genesis.h

bin_PROGRAMS = genesis gpackage

# program source dependencies
genesis_SOURCES = $(HEADERS) \
                  package.c \
                  navigation.c \
                  interface.c \
                  installer.c \
                  genesis.c

gpackage_SOURCES = package.h \
                   package.c \
                   navigation.c \
                   gpackage.c

