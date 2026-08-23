#ifndef V4L2_UTILS_H
#define V4L2_UTILS_H

#include <assert.h>
#include <chrono>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <getopt.h> /* getopt_long() */

#include <errno.h>
#include <fcntl.h> /* low-level i/o */
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <libv4l2.h>
#include <linux/videodev2.h>

std::string fcc2s(__u32 val);

int xioctl(int fh, int request, void *arg);

std::string frmtype2s(unsigned type);

bool valid_pixel_format(int fd, __u32 pixelformat, bool output, bool mplane);

__u32 find_pixel_format(int _fd, unsigned index, bool output, bool mplane);

#endif  // V4L2_UTILS_H
