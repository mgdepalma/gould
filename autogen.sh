#!/bin/sh
# $Id: autogen.sh,v 1.4 2008/11/07 02:36:27 mgdepalma Exp $
###
##
# autogen.sh - bootstrap GNU automake (maintainer)
#
# 2006/06/06 Generations Linux <info@softcraft.org>
##
###

if [ "x${ACLOCAL_DIR}" != "x" ]; then
  ACLOCAL_ARG=-I ${ACLOCAL_DIR}
fi

${ACLOCAL:-aclocal} ${ACLOCAL_ARG}
${AUTOHEADER:-autoheader}
${LIBTOOLIZE:-libtoolize} --automake --copy --force
#${INTLTOOLIZE:-intltoolize} --automake --copy --force
${AUTOMAKE:-automake} --add-missing --copy #--include-deps
${AUTOCONF:-autoconf}

# mkinstalldirs may not be correctly installed in some cases.
[ -e mkinstalldirs ] || cp -p /usr/share/automake/mkinstalldirs .

rm -rf autom4te.cache
echo "
./configure [--help] to setup build configuration files
"
