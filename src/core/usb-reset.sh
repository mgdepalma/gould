#!/bin/sh
##
# ./usb-reset wrapper
#
usbus=$(lsusb | grep ${1:-"Mouse"} |\
awk '{printf "/dev/bus/usb/%s/%s\n", $2, $4}' | sed -e 's,:,,')
[ -n "$usbus" ] || exit 1

COMMAND=usb-reset
which $COMMAND >/dev/null || COMMAND=$(dirname $0)/$COMMAND

set -x
exec sudo $COMMAND ${usbus}
