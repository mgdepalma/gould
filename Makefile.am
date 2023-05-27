##
# Generations Linux <bugs@softcraft.org>
# Makefile.am for the GOULD project
#
SUBDIRS = src data
#SUBDIRS = src po data

MAINTAINERCLEANFILES = \
Makefile.in \
aclocal.m4 \
config.h.in \
config.h.in~ \
configure \
config.guess \
config.sub \
depcomp \
install-sh \
intltool-extract \
intltool-extract.in \
intltool-merge \
intltool-merge.in \
intltool-update \
intltool-update.in \
ltmain.sh \
mkinstalldirs \
missing \
stamp-h.in

ACLOCAL_AMFLAGS =

docdir = ${prefix}/doc/gsnapshot
docfiles = \
	AUTHORS \
	COPYING \
	INSTALL \
	README \
	TODO

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libgould.pc

EXTRA_DIST = \
	$(docfiles) \
	libgould.pc

# Copy all the spec files. Only one is actually used.
dist-hook:
	for specfile in *.spec; do \
		if test -f $$specfile; then \
			cp -p $$specfile $(distdir); \
		fi \
	done
#/
