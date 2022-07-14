#!/bin/sh -f
##
# redpill ... or how to detect virtualization (ex: [ redpill ] || echo "running virtual")
#
# 2014-11-21 Generations Linux <bugs@softcraft.org>
#
dmesg | grep -i virtual > /dev/null
exit $?
