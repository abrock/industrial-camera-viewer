#ifndef BUFFER_H
#define BUFFER_H

#include <arv.h>

#include <opencv2/core.hpp>

#include "cameramanager.h"

class CameraManager;

class Buffer {
  void make_img_8(ArvBuffer *buf);
  void make_img_12(ArvBuffer *buf);
  void make_img_16(ArvBuffer *buf);

 public:
  int width = 0;
  int height = 0;

  int64_t system_timestamp = 0;

  /**
   * @brief Number of bits a pixel value has.
   * Typical values are 8, 12, 16.
   */
  int bits = 0;

  /**
   * @brief Number of bytes in the ArvBuffer object
   */
  size_t size = 0;

  /**
   * @brief OpenCV image type for this image.
   */
  int cv_type = CV_8UC1;

  guint64 frame_id = 0;

  ArvPixelFormat pixel_format;

  /**
   * @brief isBayer returns true if the sensor has a Bayer pattern
   * @return
   */
  bool isBayer() const;

  std::string generateFilename() const;

  static cv::Mat1b clippingMask(cv::Mat1b const &img);

  static cv::Mat1b clippingMask(cv::Mat1w const &img);

  static cv::Mat1b clippingMask(cv::Mat const &img);

  /**
   * @brief exposureColoredColor produces an image with overexposure indication given an image from
   * a color camera
   * @return
   */
  cv::Mat3b exposureColoredColor() const;

  /**
   * @brief exposureColordMono produces an image with overexposure indication given an image from a
   * greyscale camera.
   * @return
   */
  cv::Mat3b exposureColordMono() const;

  /**
   * @brief exposureColored produces an image with overexposure indication
   * @return
   */
  cv::Mat3b exposureColored(CameraManager *manager = nullptr) const;

  /**
   * @brief Raw image provided by Aravis
   */
  cv::Mat img;

  void save(std::string const &fn) const;

  void save() const;

  static void savePtr(std::shared_ptr<Buffer> buf);

  void saveDNG(std::string const &fn) const;

  /**
   * @brief Constructor, reads the Aravis buffer and copies all data so the buffer can be given
   * back to Aravis.
   */
  Buffer(ArvBuffer *buf);
};

#endif  // BUFFER_H
