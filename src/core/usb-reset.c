/**
* usb-reset aims to perform same as unplugging and plugging a USB device.
*
*  Build: cc {OPTS} usb-reset.c -o usb-reset
*  Usage: sudo ./usb-reset /dev/bus/usb/006/002
*/
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <linux/usbdevice_fs.h>

void usage(const char *prog)
{
  printf("usage: %s <usb bus (ex: /dev/bus/usb/006/002)>\n", prog);
  _exit(5);
}

int main(int argc, char **argv)
{
  const char *filename = argv[1];

  if (filename) {
    int fd = open(filename, O_WRONLY);
    ioctl(fd, USBDEVFS_RESET, 0);
    close(fd);
  }
  else {
    usage(argv[0]);
  }
  return 0;
}
