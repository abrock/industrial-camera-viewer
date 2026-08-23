/*
 *  V4L2 video capture example
 *
 *  This program can be used and distributed without restrictions.
 *
 *      This program is provided with the V4L2 API
 * see https://linuxtv.org/docs.php for more information
 */

#include <chrono>
#include <exception>
#include <memory>

#include <filesystem>
#include <omp.h>
namespace fs = std::filesystem;

std::error_code ignore_error_code;

#include <assert.h>
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

#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>

#include <tclap/CmdLine.h>

#include <QApplication>
#include <QObject>
#include <QtDBus>

#include <ParallelTime/paralleltime.h>

#include "captureinterface.hpp"

#include "misc.h"
#include "time.h"

cv::VideoWriter writer;

// Width and height of the camera image are auto-detected.
static int width = 2592;
static int height = 1944;

static const std::string window_name = "image";

static bool sqrt_preview = true;

static bool verbose = false;

cv::Mat next_img;
bool show_next_image = false;

struct GuiCheck : public QObject {
  Q_OBJECT
 public:
  GuiCheck()
  {
    QTimer *timer = new QTimer(this);
    timer->setInterval(200);
    connect(timer, SIGNAL(timeout()), this, SLOT(check()));
    timer->start();
    std::cout << "GuiCheck was set up" << std::endl;
  }

 public Q_SLOTS:
  void check()
  {
    printf("~");
    if (show_next_image) {
      printf("*");
      show_next_image = false;
      cv::imshow(window_name, next_img);
      cv::waitKey(1);
    }
    fflush(stdout);
  }
};

#include "misc.h"

#include "v4l2_utils.h"

#define CLEAR(x) memset(&(x), 0, sizeof(x))

struct buffer {
  void *start;
  size_t length;
};

/**
 * @brief If DO_SLEEP is true, the program sleeps every 10th frame to test the buffering.
 */
#define DO_SLEEP 0

static bool store_images = false;
static bool show_images = false;

static bool quit = false;

void waitkey_thread()
{
  while (true) {
    char key = cv::waitKey(0);
    std::cout << "Got key: " << key << std::endl;
    switch (key) {
      case 'q':
        quit = true;
        return;
      case 'c':
        sqrt_preview = !sqrt_preview;
        break;
      case 'r':
        store_images = true;
        break;
      case 'p':
        show_images = !show_images;
        break;
    }
  }
}

template<class Duration>
using sys_time = std::chrono::time_point<std::chrono::system_clock, Duration>;
using sys_nanoseconds = sys_time<std::chrono::nanoseconds>;

double duration_ms(sys_nanoseconds const first, sys_nanoseconds const second)
{
  return 1e-6 * double(std::chrono::nanoseconds(second - first).count());
}

struct LImage {
  cv::Mat1w img;

  sys_nanoseconds timestamp;

  LImage(const void *p, const size_t size)
  {
    timestamp = std::chrono::system_clock::now();
    assert(size == 2 * width * height);
    img = cv::Mat1w(cv::Size(width, height));
    memcpy(img.data, p, size);
    img *= (1 << 6);
  }
};

static std::vector<LImage> images;

void save_video_frame(cv::Mat const &img)
{
  if (!writer.isOpened()) {
    int ii = 0;
    std::string filename;
    do {
      filename = fmt::format("v4l2-rec-{:04}.avi", ii++);
    } while (fs::exists(filename));
    int codec = cv::VideoWriter::fourcc(
        'M', 'J', 'P', 'G');  // select desired codec (must be available at runtime)
    double fps = 25.0;        // framerate of the created video stream
    writer.open(filename, codec, fps, img.size(), img.channels() > 1);
  }
  if (!writer.isOpened()) {
    std::cerr << "Could not open the output video file for write" << std::endl;
    return;
  }
  writer << img;
}

void show_image_sub(const void *p, const size_t size)
{
  // Avoid running this function in parallel, skip frames instead.
  static bool is_running = false;
  if (is_running || show_next_image) {
    return;
  }
  static std::mutex mutex;
  std::lock_guard guard(mutex);

  std::string const time = fmt::format(
      "{:02}:{:02}:{:02}", Time::hour(), Time::minute(), Time::second());

  std::cout << "Time: " << time << std::endl;

  is_running = true;

  LImage img(p, size);
  cv::resize(img.img, img.img, {}, .5, .5, cv::INTER_AREA);
  if (sqrt_preview) {
    next_img = Misc::compress_16_8_gamma(img.img, .5);
  }
  else {
    next_img = img.img;
  }
  cv::Point const text_orig(10, next_img.rows - 10);
  double const text_size = 2;
  cv::putText(next_img, time, text_orig, cv::FONT_HERSHEY_SIMPLEX, text_size, {0}, 4, cv::LINE_AA);
  cv::putText(
      next_img, time, text_orig, cv::FONT_HERSHEY_SIMPLEX, text_size, {255}, 2, cv::LINE_AA);
  show_next_image = true;
  save_video_frame(next_img);
  is_running = false;
}

void show_image(const void *p, const size_t size)
{
  static int last_second = Time::second();
  int const second = Time::second();
  if (second == last_second) {
    return;
  }
  last_second = second;

  std::thread(show_image_sub, p, size).detach();
}

static char *dev_name;
static int fd = -1;
struct buffer *buffers;
static unsigned int n_buffers;
FILE *out_buf;
static int force_format;

static int queued_buffers;
static int frame_count;
static struct timespec last_frame;

static double diff_timespec(struct timespec *prev, struct timespec *next)
{
  return (next->tv_sec - prev->tv_sec) + (next->tv_nsec - prev->tv_nsec) / 1000000000.0;
}

static void errno_exit(const char *s)
{
  fprintf(stderr, "%s error %d, %s\\n", s, errno, strerror(errno));
  exit(EXIT_FAILURE);
}

static size_t frames_since_pause = 0;

static void read_frame(void)
{
  struct v4l2_buffer buf;

  CLEAR(buf);

  buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  buf.memory = V4L2_MEMORY_MMAP;
  buf.flags = V4L2_BUF_FLAG_TIMECODE;

  if (-1 == xioctl(fd, VIDIOC_DQBUF, &buf))
    errno_exit("VIDIOC_DQBUF");
  queued_buffers -= 1;
  assert(buf.index < n_buffers);

  struct timespec now;
  clock_gettime(CLOCK_MONOTONIC_RAW, &now);

  bool marked_for_saving = false;

  const double ms = diff_timespec(&last_frame, &now) * 1000;

  show_image(buffers[buf.index].start, buf.bytesused);

  if (-1 == xioctl(fd, VIDIOC_QBUF, &buf))
    errno_exit("VIDIOC_QBUF");
  queued_buffers += 1;

  ++frame_count;
  fflush(stdout);
}

static void mainloop(void)
{
  for (;;) {
    read_frame();
    if (quit) {
      return;
    }
  }
}

static void stop_capturing(void)
{
  enum v4l2_buf_type type;
  type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  if (-1 == xioctl(fd, VIDIOC_STREAMOFF, &type))
    errno_exit("VIDIOC_STREAMOFF");
}

static void start_capturing(void)
{
  unsigned int i;
  enum v4l2_buf_type type;
  queued_buffers = 0;
  for (i = 0; i < n_buffers; ++i) {
    struct v4l2_buffer buf;

    CLEAR(buf);
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = i;

    if (-1 == xioctl(fd, VIDIOC_QBUF, &buf))
      errno_exit("VIDIOC_QBUF");
    queued_buffers += 1;
  }
  type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  if (-1 == xioctl(fd, VIDIOC_STREAMON, &type))
    errno_exit("VIDIOC_STREAMON");
}

static void uninit_device(void)
{
  unsigned int i;
  for (i = 0; i < n_buffers; ++i)
    if (-1 == munmap(buffers[i].start, buffers[i].length))
      errno_exit("munmap");
  free(buffers);
}

static void get_frame_size(const struct v4l2_frmsizeenum &frmsize, const char *prefix)
{
  printf("%s\tSize: %s ", prefix, frmtype2s(frmsize.type).c_str());
  if (frmsize.type == V4L2_FRMSIZE_TYPE_DISCRETE) {
    printf("%ux%u", frmsize.discrete.width, frmsize.discrete.height);
    width = frmsize.discrete.width;
    height = frmsize.discrete.height;
  }
  else if (frmsize.type == V4L2_FRMSIZE_TYPE_CONTINUOUS) {
    printf("%ux%u - %ux%u",
           frmsize.stepwise.min_width,
           frmsize.stepwise.min_height,
           frmsize.stepwise.max_width,
           frmsize.stepwise.max_height);
    width = frmsize.stepwise.max_width;
    height = frmsize.stepwise.max_height;
  }
  else if (frmsize.type == V4L2_FRMSIZE_TYPE_STEPWISE) {
    printf("%ux%u - %ux%u with step %u/%u",
           frmsize.stepwise.min_width,
           frmsize.stepwise.min_height,
           frmsize.stepwise.max_width,
           frmsize.stepwise.max_height,
           frmsize.stepwise.step_width,
           frmsize.stepwise.step_height);
    width = frmsize.stepwise.max_width;
    height = frmsize.stepwise.max_height;
  }
  printf("\n");
  assert(width > 0);
  assert(height > 0);
}

static void init_mmap(void)
{
  struct v4l2_requestbuffers req;

  CLEAR(req);

  req.count = 32;
  req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  req.memory = V4L2_MEMORY_MMAP;

  if (-1 == xioctl(fd, VIDIOC_REQBUFS, &req)) {
    if (EINVAL == errno) {
      fprintf(stderr,
              "%s does not support "
              "memory mappingn",
              dev_name);
      exit(EXIT_FAILURE);
    }
    else {
      errno_exit("VIDIOC_REQBUFS");
    }
  }

  if (req.count < 32) {
    fprintf(stderr, "Insufficient buffer memory on %s\\n", dev_name);
    // Don't exit if we detect the fake-video0 debug input
    if (width != 1280 && height != 720) {
      exit(EXIT_FAILURE);
    }
  }

  buffers = static_cast<buffer *>(calloc(req.count, sizeof(*buffers)));

  if (!buffers) {
    fprintf(stderr, "Out of memory\\n");
    exit(EXIT_FAILURE);
  }

  for (n_buffers = 0; n_buffers < req.count; ++n_buffers) {
    struct v4l2_buffer buf;

    CLEAR(buf);

    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = n_buffers;

    if (-1 == xioctl(fd, VIDIOC_QUERYBUF, &buf))
      errno_exit("VIDIOC_QUERYBUF");

    buffers[n_buffers].length = buf.length;
    buffers[n_buffers].start = mmap(NULL /* start anywhere */,
                                    buf.length,
                                    PROT_READ | PROT_WRITE /* required */,
                                    MAP_SHARED /* recommended */,
                                    fd,
                                    buf.m.offset);

    if (MAP_FAILED == buffers[n_buffers].start)
      errno_exit("mmap");
  }
}

static void init_device(void)
{
  struct v4l2_capability cap;
  struct v4l2_cropcap cropcap;
  struct v4l2_crop crop;
  struct v4l2_format fmt;
  unsigned int min;

  if (-1 == xioctl(fd, VIDIOC_QUERYCAP, &cap)) {
    if (EINVAL == errno) {
      fprintf(stderr, "%s is no V4L2 device\\n", dev_name);
      exit(EXIT_FAILURE);
    }
    else {
      errno_exit("VIDIOC_QUERYCAP");
    }
  }

  if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
    fprintf(stderr, "%s is no video capture device\\n", dev_name);
    exit(EXIT_FAILURE);
  }

  if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
    fprintf(stderr, "%s does not support streaming i/o\\n", dev_name);
    exit(EXIT_FAILURE);
  }

  /* Select video input, video standard and tune here. */

  CLEAR(cropcap);

  cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

  if (0 == xioctl(fd, VIDIOC_CROPCAP, &cropcap)) {
    crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    crop.c = cropcap.defrect; /* reset to default */

    if (-1 == xioctl(fd, VIDIOC_S_CROP, &crop)) {
      switch (errno) {
        case EINVAL:
          /* Cropping not supported. */
          break;
        default:
          /* Errors ignored. */
          break;
      }
    }
  }
  else {
    /* Errors ignored. */
  }

  CLEAR(fmt);

  fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  if (force_format) {
    fmt.fmt.pix.width = 640;
    fmt.fmt.pix.height = 480;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;

    if (-1 == xioctl(fd, VIDIOC_S_FMT, &fmt))
      errno_exit("VIDIOC_S_FMT");

    /* Note VIDIOC_S_FMT may change width and height. */
  }
  else {
    /* Preserve original settings as set by v4l2-ctl for example */
    if (-1 == xioctl(fd, VIDIOC_G_FMT, &fmt))
      errno_exit("VIDIOC_G_FMT");
  }

  /* Buggy driver paranoia. */
  min = fmt.fmt.pix.width * 2;
  if (fmt.fmt.pix.bytesperline < min)
    fmt.fmt.pix.bytesperline = min;
  min = fmt.fmt.pix.bytesperline * fmt.fmt.pix.height;
  if (fmt.fmt.pix.sizeimage < min)
    fmt.fmt.pix.sizeimage = min;

  bool is_multiplanar = false;
  v4l2_frmsizeenum frmsize;
  frmsize.index = 0;
  if (frmsize.pixel_format < 256) {
    frmsize.pixel_format = find_pixel_format(fd, frmsize.pixel_format, false, is_multiplanar);
    if (!frmsize.pixel_format) {
      fprintf(stderr, "The pixelformat index was invalid\n");
      std::exit(EXIT_FAILURE);
    }
  }
  if (!valid_pixel_format(fd, frmsize.pixel_format, false, is_multiplanar) &&
      !valid_pixel_format(fd, frmsize.pixel_format, true, is_multiplanar))
  {
    fprintf(stderr, "The pixelformat '%s' is invalid\n", fcc2s(frmsize.pixel_format).c_str());
    // std::exit(EXIT_FAILURE);
  }
  printf("ioctl: VIDIOC_ENUM_FRAMESIZES\n");
  while (ioctl(fd, VIDIOC_ENUM_FRAMESIZES, &frmsize) >= 0) {
    get_frame_size(frmsize, "");
    frmsize.index++;
  }

  printf("Final size: %d x %d\n", width, height);

  assert(width > 0);
  assert(height > 0);

  init_mmap();
}

static void close_device(void)
{
  if (-1 == close(fd))
    errno_exit("close");

  fd = -1;
}

static void open_device(void)
{
  struct stat st;

  if (-1 == stat(dev_name, &st)) {
    fprintf(stderr, "Cannot identify '%s': %d, %s\\n", dev_name, errno, strerror(errno));
    exit(EXIT_FAILURE);
  }

  if (!S_ISCHR(st.st_mode)) {
    fprintf(stderr, "%s is no devicen", dev_name);
    exit(EXIT_FAILURE);
  }

  fd = open(dev_name, O_RDWR /* required */ /* | O_NONBLOCK */, 0);

  if (-1 == fd) {
    fprintf(stderr, "Cannot open '%s': %d, %s\\n", dev_name, errno, strerror(errno));
    exit(EXIT_FAILURE);
  }
}

static void usage(FILE *fp, int argc, char **argv)
{
  fprintf(fp,
          "Usage: %s [options]\\n\\n"
          "Version 1.3\\n"
          "Options:\\n"
          "-d | --device name   Video device name [%s]n"
          "-h | --help          Print this messagen"
          "-o | --output        Outputs stream to stdoutn"
          "-f | --format        Force format to 640x480 YUYVn"
          "",
          argv[0],
          dev_name);
}

static const char short_options[] = "d:hof:";

static const struct option long_options[] = {{"device", required_argument, NULL, 'd'},
                                             {"help", no_argument, NULL, 'h'},
                                             {"output", no_argument, NULL, 'o'},
                                             {"format", no_argument, NULL, 'f'},
                                             {0, 0, 0, 0}};

void run_v4l2()
{
  open_device();
  init_device();
  start_capturing();
  mainloop();
  stop_capturing();
  uninit_device();
  close_device();
  fprintf(stderr, "\\n");
}

int main(int argc, char **argv)
{
  clock_getres(CLOCK_MONOTONIC_RAW, &last_frame);
  assert(last_frame.tv_sec == 0);
  assert(last_frame.tv_nsec == 1);
  clock_gettime(CLOCK_MONOTONIC_RAW, &last_frame);

  dev_name = "/dev/video2";

  // std::thread(waitkey_thread).detach();

  for (;;) {
    int idx;
    int c;

    c = getopt_long(argc, argv, short_options, long_options, &idx);

    if (-1 == c)
      break;

    switch (c) {
      case 0: /* getopt_long() flag */
        break;

      case 'd':
        dev_name = optarg;
        break;

      case 'h':
        usage(stdout, argc, argv);
        exit(EXIT_SUCCESS);

      case 'o':
        out_buf++;
        break;

      case 'f':
        force_format++;
        break;

      default:
        usage(stderr, argc, argv);
        exit(EXIT_FAILURE);
    }
  }

  int fake_argc = 0;
  QApplication app(fake_argc, nullptr);

  GuiCheck checker;

  std::thread t(run_v4l2);

  app.exec();

  t.join();

  return 0;
}

#include "v4l2-recording.moc"

// vim: set ts=8 sts=8 sw=8 et:
