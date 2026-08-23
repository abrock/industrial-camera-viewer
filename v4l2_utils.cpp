#include "v4l2_utils.h"

std::string fcc2s(__u32 val)
{
  std::string s;

  s += val & 0x7f;
  s += (val >> 8) & 0x7f;
  s += (val >> 16) & 0x7f;
  s += (val >> 24) & 0x7f;
  if (val & (1U << 31))
    s += "-BE";
  return s;
}

int xioctl(int fh, int request, void *arg)
{
  int r;

  do {
    r = ioctl(fh, request, arg);
  } while (-1 == r && EINTR == errno);

  return r;
}

std::string frmtype2s(unsigned int type)
{
  static constexpr const char *types[] = {"Unknown", "Discrete", "Continuous", "Stepwise"};

  if (type > 3)
    type = 0;
  return types[type];
}

bool valid_pixel_format(int fd, __u32 pixelformat, bool output, bool mplane)
{
  struct v4l2_fmtdesc fmt = {};

  if (output)
    fmt.type = mplane ? V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE : V4L2_BUF_TYPE_VIDEO_OUTPUT;
  else
    fmt.type = mplane ? V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE : V4L2_BUF_TYPE_VIDEO_CAPTURE;

  while (!ioctl(fd, VIDIOC_ENUM_FMT, &fmt)) {
    if (fmt.pixelformat == pixelformat)
      return true;
    fmt.index++;
  }
  return false;
}

__u32 find_pixel_format(int _fd, unsigned int index, bool output, bool mplane)
{
  struct v4l2_fmtdesc fmt = {};

  fmt.index = index;
  if (output)
    fmt.type = mplane ? V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE : V4L2_BUF_TYPE_VIDEO_OUTPUT;
  else
    fmt.type = mplane ? V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE : V4L2_BUF_TYPE_VIDEO_CAPTURE;

  if (ioctl(_fd, VIDIOC_ENUM_FMT, &fmt))
    return 0;
  return fmt.pixelformat;
}
