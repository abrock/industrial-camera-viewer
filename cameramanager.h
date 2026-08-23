#ifndef CAMERAMANAGER_H
#define CAMERAMANAGER_H

#include <arv.h>

#include <stdio.h>
#include <stdlib.h>

#include <opencv2/highgui.hpp>

#include <QLabel>
#include <QMouseEvent>
#include <QObject>

#include "blocking-queue.hpp"
#include "buffer.h"
#include "dbusreceiver.h"
#include "whitebalance.h"

class Buffer;

class CameraManager : public QObject {
  Q_OBJECT

  std::mutex arv_mutex;

  ArvCamera *camera = nullptr;
  GError *error = nullptr;

  ArvPixelFormat pixel_format;

  bool stopped = false;

  std::string window_name = "img";

  std::mutex video_writer_mutex;

  cv::VideoWriter video_writer;

  bool camera_running = false;

  double requested_exposure = 10'000;

  double requested_gain = 0;

  WhiteBalance balance;

  /**
   * @brief Minimum gain value supported by the camera, typically zero.
   */
  double min_gain = 0;

  /**
   * @brief Maximum gain value supported by the camera.
   */
  double max_gain = 30;

  /**
   * @brief Maximum width of images shown to the user using cv::imshow
   */
  size_t max_width_shown = 1600;

  cv::Rect current_roi;

  /**
   * @brief downscale_if_neccessary scales an image down if its width is larger than a given
   * maximum width. If the width is smaller it is returned unchanged.
   * @param input
   * @param max_width
   * @return
   */
  static cv::Mat downscale_if_neccessary(cv::Mat const &input, size_t max_width);

  DBusReceiver *dbus = nullptr;

  void auto_assign_depth();

  /**
   * @brief Number of incoming images which should be saved.
   * After each saved image the counter is reduced until it reaches zero.
   */
  int save_images = 0;

  void save_video_frame(cv::Mat const &img);

  int videoWriteWorker();

  bool save_video = false;
  BQueue<std::shared_ptr<Buffer>> save_video_queue;
  bool save_all_video_frames();

 public:
  CameraManager();

  void runCameraMainThread();

  GMainLoop *main_loop;

  void runCameraCallback();

  void process_image(std::shared_ptr<Buffer> buf);

  void drawCrosshairs(cv::Mat3b &img);

  void stop();

  void runWaitKey();

  void runVideoWorker();

  void increaseExposureTime();
  void decreaseExposureTime();

  void increaseGain();
  void decreaseGain();

  void makeWindow();

  Q_INVOKABLE void setExposure(const double exposure_us);

  Q_INVOKABLE void setGain(const double gain);

  bool crosshairs = false;
  Q_INVOKABLE void setCrosshairs(const bool val);

  std::string crosshair_window_name = "Crosshair";
  bool crosshair_window = false;
  Q_INVOKABLE void setCrosshairWindow(const bool val);

  float wb_min0 = 0;
  Q_INVOKABLE void setWBmin0(double val)
  {
    wb_min0 = val;
  }
  float wb_min1 = 0;
  Q_INVOKABLE void setWBmin1(double val)
  {
    wb_min1 = val;
  }
  float wb_min2 = 0;
  Q_INVOKABLE void setWBmin2(double val)
  {
    wb_min2 = val;
  }
  float wb_max0 = 0;
  Q_INVOKABLE void setWBmax0(double val)
  {
    wb_max0 = val;
  }
  float wb_max1 = 0;
  Q_INVOKABLE void setWBmax1(double val)
  {
    wb_max1 = val;
  }
  float wb_max2 = 0;
  Q_INVOKABLE void setWBmax2(double val)
  {
    wb_max2 = val;
  }

  Q_INVOKABLE void setWBS1(int val);
  Q_INVOKABLE void setWBS2(int val);

  bool auto_wb = true;
  Q_INVOKABLE void setAutoWB(bool const val);

  bool denoise = true;
  Q_INVOKABLE void setDenoise(bool const val);

  int denoise_scale = 5;
  Q_INVOKABLE void setDenoiseScale(int const val);

  bool show_sharpness = false;
  Q_INVOKABLE void setSharpness(const bool val);

  Q_INVOKABLE void setSaveVideo(const bool val);

  int crop = 100;
  Q_INVOKABLE void setCrop(int const val);

  Q_INVOKABLE void setTriggerSource(QString const &str);

  Q_INVOKABLE void setModeContinuous();

  Q_INVOKABLE void setModeSingle();

  void handleWhiteBalance(cv::Mat3b &img, bool skip_auto_wb = false);

 signals:

  void requestedExposure(int val);

  void requestedGain(int val);

  void requestedWBmin0(double val);
  void requestedWBmin1(double val);
  void requestedWBmin2(double val);
  void requestedWBmax0(double val);
  void requestedWBmax1(double val);
  void requestedWBmax2(double val);
};

#endif  // CAMERAMANAGER_H
