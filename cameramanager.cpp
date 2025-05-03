#include "cameramanager.h"

#include <glog/logging.h>

#include <opencv2/imgproc.hpp>

#include "misc.h"
using namespace Misc;

#include <runningstats/runningstats.h>
namespace rs = runningstats;

#include <filesystem>
namespace fs = std::filesystem;

#include <fstream>

#include "macros.hpp"

cv::Mat CameraManager::downscale_if_neccessary(const cv::Mat &input, size_t max_width)
{
  if (input.size().width <= max_width) {
    return input;
  }
  cv::Mat result;
  double const factor = double(input.size().width) / max_width;
  cv::resize(input, result, cv::Size(), 1.0 / factor, 1.0 / factor, cv::INTER_AREA);
  return result;
}

template<class T, class U> size_t get_idx(std::vector<T> const &vec, U const &elem)
{
  size_t result = 0;
  for (; result < vec.size(); ++result) {
    if (elem == vec[result]) {
      return result;
    }
  }
  return result;
}

void CameraManager::auto_assign_depth()
{
  guint n_pixel_formats = 0;
  EXEC_AND_CHECK(
      gint64 *formats = arv_camera_dup_available_pixel_formats(camera, &n_pixel_formats, &error));
  EXEC_AND_CHECK(
      const char **format_strings = arv_camera_dup_available_pixel_formats_as_display_names(
          camera, &n_pixel_formats, &error));
  if (0 == n_pixel_formats) {
    delete[] formats;
    delete[] format_strings;
    return;
  }
  EXEC_AND_CHECK(pixel_format = arv_camera_get_pixel_format(camera, &error));
  std::vector<ArvPixelFormat> preferences{
      ARV_PIXEL_FORMAT_BAYER_GR_12_PACKED,
      ARV_PIXEL_FORMAT_BAYER_RG_12_PACKED,
      ARV_PIXEL_FORMAT_BAYER_GB_12_PACKED,
      ARV_PIXEL_FORMAT_BAYER_BG_12_PACKED,
      ARV_PIXEL_FORMAT_BAYER_BG_12P,
      ARV_PIXEL_FORMAT_BAYER_GB_12P,
      ARV_PIXEL_FORMAT_BAYER_GR_12P,
      ARV_PIXEL_FORMAT_BAYER_RG_12P,
      ARV_PIXEL_FORMAT_MONO_12_PACKED,

      ARV_PIXEL_FORMAT_BAYER_GR_12,
      ARV_PIXEL_FORMAT_BAYER_RG_12,
      ARV_PIXEL_FORMAT_BAYER_GB_12,
      ARV_PIXEL_FORMAT_BAYER_BG_12,

      ARV_PIXEL_FORMAT_BAYER_GR_16,
      ARV_PIXEL_FORMAT_BAYER_RG_16,
      ARV_PIXEL_FORMAT_BAYER_GB_16,
      ARV_PIXEL_FORMAT_BAYER_BG_16,
      ARV_PIXEL_FORMAT_MONO_16,
      ARV_PIXEL_FORMAT_MONO_14,
      ARV_PIXEL_FORMAT_MONO_12,

      ARV_PIXEL_FORMAT_BAYER_GR_10_PACKED,
      ARV_PIXEL_FORMAT_BAYER_RG_10_PACKED,
      ARV_PIXEL_FORMAT_BAYER_GB_10_PACKED,
      ARV_PIXEL_FORMAT_BAYER_BG_10_PACKED,
      ARV_PIXEL_FORMAT_BAYER_BG_10P,
      ARV_PIXEL_FORMAT_BAYER_GB_10P,
      ARV_PIXEL_FORMAT_BAYER_GR_10P,
      ARV_PIXEL_FORMAT_BAYER_RG_10P,
      ARV_PIXEL_FORMAT_MONO_10_PACKED,
      ARV_PIXEL_FORMAT_MONO_10,

      ARV_PIXEL_FORMAT_BAYER_GR_10,
      ARV_PIXEL_FORMAT_BAYER_RG_10,
      ARV_PIXEL_FORMAT_BAYER_GB_10,
      ARV_PIXEL_FORMAT_BAYER_BG_10,

      ARV_PIXEL_FORMAT_MONO_8,
      ARV_PIXEL_FORMAT_MONO_8_SIGNED,

      ARV_PIXEL_FORMAT_BAYER_GR_8,
      ARV_PIXEL_FORMAT_BAYER_RG_8,
      ARV_PIXEL_FORMAT_BAYER_GB_8,
      ARV_PIXEL_FORMAT_BAYER_BG_8,
  };
  Misc::println("Available pixel formats: ");
  size_t best_position = get_idx(preferences, pixel_format);
  for (guint idx = 0; idx < n_pixel_formats; ++idx) {
    size_t const position = get_idx(preferences, formats[idx]);
    if (position < best_position) {
      best_position = position;
    }
    Misc::println("#{:>2}: {:>8}, {}", idx, formats[idx], format_strings[idx]);
  }
  if (best_position < preferences.size()) {
    EXEC_AND_CHECK(
        std::string const old_format = arv_camera_get_pixel_format_as_string(camera, &error));
    EXEC_AND_CHECK(arv_camera_set_pixel_format(camera, preferences.at(best_position), &error));
    EXEC_AND_CHECK(
        std::string const new_format = arv_camera_get_pixel_format_as_string(camera, &error));
    println("Changed pixel format from {} to {}", old_format, new_format);
  }
  delete[] formats;
  delete[] format_strings;
}

CameraManager::CameraManager()
{
  if ("i3" == Misc::envVar("XDG_CURRENT_DESKTOP") || "i3" == Misc::envVar("GDMSESSION")) {
    Misc::println("Found i3, preview window gets full resolution");
    max_width_shown = 10'000;
  }
}

void CameraManager::runCamera()
{
  println("Running CameraManager::runCamera");

  camera = arv_camera_new(NULL, &error);

  if (!ARV_IS_CAMERA(camera)) {
    cv::Mat_<uint8_t> img(100, 100, uint8_t(0));
    std::mt19937_64 rng(std::random_device{}());
    while (true) {
      usleep(500'000);
    }
    return;
  }

  CHECK(ARV_IS_CAMERA(camera));

  ArvStream *stream = NULL;

  println("Found camera '{}'", arv_camera_get_model_name(camera, NULL));

  auto_assign_depth();

  EXEC_AND_CHECK(arv_camera_get_gain_bounds(camera, &min_gain, &max_gain, &error));

  arv_camera_set_acquisition_mode(camera, ARV_ACQUISITION_MODE_CONTINUOUS, &error);
  println("arv_camera_set_acquisition_mode done.");

  /* Create the stream object without callback */
  stream = arv_camera_create_stream(camera, NULL, NULL, &error);
  CHECK_EQ(nullptr, error) << error->message;

  CHECK(ARV_IS_STREAM(stream));

  size_t payload;

  /* Retrieve the payload size for buffer creation */
  payload = arv_camera_get_payload(camera, &error);
  CHECK_EQ(nullptr, error) << error->message;

  /* Insert some buffers in the stream buffer pool */
  for (int i = 0; i < 5; i++)
    arv_stream_push_buffer(stream, arv_buffer_new(payload, NULL));

  arv_camera_start_acquisition(camera, &error);
  CHECK_EQ(nullptr, error) << error->message;

  camera_running = true;
  setExposure(requested_exposure);
  setGain(requested_gain);

  for (size_t ii = 0; !stopped; ++ii) {
    usleep(10'000);
    std::lock_guard guard(arv_mutex);
    ArvBuffer *buffer;

    buffer = arv_stream_pop_buffer(stream);
    if (ARV_IS_BUFFER(buffer)) {
      std::shared_ptr<Buffer> buf = std::make_shared<Buffer>(buffer);
      std::thread(&CameraManager::process_image, this, buf).detach();

      /* Don't destroy the buffer, but put it back into the buffer pool */
      arv_stream_push_buffer(stream, buffer);
    }
  }
  cv::destroyAllWindows();

  CHECK_EQ(nullptr, error) << error->message;
  arv_camera_stop_acquisition(camera, &error);
  CHECK_EQ(nullptr, error) << error->message;

  /* Destroy the stream object */
  g_clear_object(&stream);

  /* Destroy the camera instance */
  g_clear_object(&camera);
}

void CameraManager::process_image(std::shared_ptr<Buffer> buf)
{
  cv::Mat3b colored = buf->exposureColored();
  if (crosshairs) {
    drawCrosshairs(colored);
    if (crosshair_window) {
      cv::Point center(colored.cols / 2, colored.rows / 2);
      int const size = std::min(colored.cols, colored.rows) / 20;
      cv::Rect roi(center.x - size, center.y - size, 2 * size, 2 * size);
      cv::imshow(crosshair_window_name, colored(roi));
    }
    else {
      cv::destroyWindow(crosshair_window_name);
    }
  }
  cv::imshow(window_name, downscale_if_neccessary(colored, max_width_shown));
  if (save_images > 0) {
    save_images--;
    std::thread(&Buffer::savePtr, buf).detach();
  }
}

void CameraManager::drawCrosshairs(cv::Mat3b &img)
{
  cv::Vec3d const red(0, 0, 255);
  for (int row = 0; row < img.rows; ++row) {
    img(row, img.cols / 2) = red;
  }
  for (int col = 0; col < img.cols; ++col) {
    img(img.rows / 2, col) = red;
  }
}

void CameraManager::runWaitKey()
{
  while (!stopped) {
    int const key = cv::waitKey(100);
    switch (char(key)) {
      case '+':
      case 'w':
        increaseExposureTime();
        break;
      case '-':
      case 's':
        decreaseExposureTime();
        break;
      case 'e':
        increaseGain();
        break;
      case 'd':
        decreaseGain();
        break;
      case 'q':
        stopped = true;
        cv::destroyAllWindows();
        break;
      case ' ':
        save_images++;
    }
    if (key >= 0) {
      println("Key: {} ({})", key, int(key));
    }
  }
}

void CameraManager::increaseExposureTime()
{
  println("Increasing exposure time...");
  println("Current exp: {}", requested_exposure);
  double const old_exp = requested_exposure;
  double const new_exp = std::max(1.0, requested_exposure * std::pow(2.0, 1.0 / 4.0));
  setExposure(new_exp);
  println("Increased exposure from {} to {}", old_exp, requested_exposure);
}

void CameraManager::decreaseExposureTime()
{
  println("Decreasing exposure time...");
  println("Current exp: {}", requested_exposure);
  double const old_exp = requested_exposure;
  double const new_exp = std::max(1.0, requested_exposure * std::pow(2.0, -1.0 / 4.0));
  setExposure(new_exp);
  println("Decreased exposure from {} to {}", old_exp, requested_exposure);
}

void CameraManager::increaseGain()
{
  println("Increasing gain...");
  println("Current gain: {}", requested_gain);
  double const old_gain = requested_gain;
  double const new_gain = std::clamp(requested_gain + 1, min_gain, max_gain);
  setGain(new_gain);
  println("Increased exposure from {} to {}", old_gain, new_gain);
}

void CameraManager::decreaseGain()
{
  println("Decreasing gain...");
  println("Current gain: {}", requested_gain);
  double const old_gain = requested_gain;
  double const new_gain = std::clamp(requested_gain - 1, min_gain, max_gain);
  setGain(new_gain);
  println("Decreased exposure from {} to {}", old_gain, new_gain);
}

void CameraManager::makeWindow()
{
  cv::namedWindow(window_name);
}

void CameraManager::setExposure(double const exposure_us)
{
  requested_exposure = exposure_us;
  emit requestedExposure(exposure_us);
  if (!camera_running) {
    println("camera not running");
    return;
  }
  if (!ARV_IS_CAMERA(camera)) {
    println("camera not valid");
    return;
  }
  GError *error = nullptr;
  std::lock_guard guard(arv_mutex);
  arv_camera_set_exposure_time(camera, exposure_us, &error);
  CHECK_EQ(nullptr, error) << error->message;
  println("Set exposure time to {}", exposure_us);
}

void CameraManager::setGain(const double gain)
{
  requested_gain = std::clamp(gain, min_gain, max_gain);
  emit requestedGain(requested_gain);
  if (!camera_running) {
    println("camera not running");
    return;
  }
  if (!ARV_IS_CAMERA(camera)) {
    println("camera not valid");
    return;
  }
  GError *error = nullptr;
  std::lock_guard guard(arv_mutex);
  arv_camera_set_gain(camera, requested_gain, &error);
  CHECK_EQ(nullptr, error) << error->message;
  println("Set gain time to {}", requested_gain);
}

void CameraManager::setCrosshairs(const bool val)
{
  crosshairs = val;
}

void CameraManager::setCrosshairWindow(const bool val)
{
  crosshair_window = val;
}

void CameraManager::stop()
{
  stopped = true;
}
