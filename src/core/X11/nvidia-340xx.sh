##
# /etc/profile.d/nvidia-340xx additions (bourne)
#
NVIDIA_LIBDIR=/usr/lib64/nvidia-340xx

if [ -d $NVIDIA_LIBDIR ]; then
  if [ -n "$LD_LIBRARY_PATH" ]; then
    echo $LD_LIBRARY_PATH | grep -q $NVIDIA_LIBDIR || \
    LD_LIBRARY_PATH=$NVIDIA_LIBDIR/tls:$NVIDIA_LIBDIR:$LD_LIBRARY_PATH
  else
    LD_LIBRARY_PATH=$NVIDIA_LIBDIR/tls:$NVIDIA_LIBDIR
  fi
  export LD_LIBRARY_PATH
fi
#/
