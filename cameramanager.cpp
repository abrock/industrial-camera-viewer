#include "cameramanager.h"

#include <glog/logging.h>

#include <opencv2/imgproc.hpp>
#include <opencv2/videostab/deblurring.hpp>

#include <ParallelTime/paralleltime.h>

#include "misc.h"
using namespace Misc;

#include <runningstats/runningstats.h>
namespace rs = runningstats;

#include <filesystem>
namespace fs = std::filesystem;

#include <fstream>

#include "macros.hpp"
#include "setfalseondestruct.h"
#include "whitebalance.h"

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

void CameraManager::save_video_frame(const cv::Mat &img)
{
  if (!video_writer.isOpened()) {
    int ii = 0;
    std::string filename;
    do {
      filename = fmt::format("cameramanager-rec-{:04}.avi", ii++);
    } while (fs::exists(filename));
    int codec = cv::VideoWriter::fourcc(
        'M',
        'J',
        'P',
        'G');           // select desired codec (must be available at runtime)
    double fps = 25.0;  // framerate of the created video stream
    video_writer.open(filename, codec, fps, img.size(), img.channels() > 1);
  }
  if (!video_writer.isOpened()) {
    std::cerr << "Could not open the output video file for write" << std::endl;
    return;
  }
  video_writer << img;
}

#if 0  // Parallel version
int CameraManager::videoWriteWorker()
{
  std::lock_guard guard(video_writer_mutex);
  ParallelTime t;
  std::vector<std::shared_ptr<Buffer>> buffers;
  while (!save_video_queue.empty()) {
    buffers.push_back(save_video_queue.pop());
  }

  std::vector<cv::Mat3b> images(buffers.size());
#  pragma omp parallel for schedule(dynamic, 1)
  for (size_t ii = 0; ii < buffers.size(); ++ii) {
    std::shared_ptr<Buffer> buf = buffers[ii];
    cv::Mat3b &img = images[ii];
    img = buf->videoImage(this);
    if (!img.empty()) {
      img = downscale_if_neccessary(img, max_width_shown);
    }
  }

  for (size_t ii = 0; ii < images.size(); ++ii) {
    save_video_frame(images[ii]);
  }
  if (images.size() > 0) {
    println("Video write worker pushed {} frames, time: {}", images.size(), t.print());
  }
  return images.size();
}
#endif

int CameraManager::videoWriteWorker()
{
  std::lock_guard guard(video_writer_mutex);
  ParallelTime t;
  int count = 0;
  bool save_video_was_false = false;
  while (!save_video_queue.empty()) {
    auto buf = save_video_queue.pop();
    cv::Mat3b img = buf->videoImage(this);
    if (!img.empty()) {
      count++;
      save_video_frame(img);
      if (!save_video && !save_video_was_false) {
        save_video_was_false = true;
        std::cout << std::string(save_video_queue.size(), '+') << std::endl;
      }
      if (!save_video) {
        std::cout << "-" << std::flush;
      }
    }
  }

  if (count > 0) {
    std::cout << std::endl;
    println("Video write worker pushed {} frames, time: {}", count, t.print());
  }
  if (!save_video) {
    video_writer.release();
  }
  return count;
}

CameraManager::CameraManager()
{
  if ("i3" == Misc::envVar("XDG_CURRENT_DESKTOP") || "i3" == Misc::envVar("GDMSESSION")) {
    Misc::println("Found i3, preview window gets full resolution");
    max_width_shown = 10'000;
  }
}

void CameraManager::runCameraMainThread()
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

  arv_camera_set_exposure_time_auto(camera, ARV_AUTO_OFF, &error);
  CHECK_EQ(nullptr, error) << error->message;

  arv_camera_set_gain_auto(camera, ARV_AUTO_OFF, &error);
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

void new_buffer_cb(ArvStream *stream, void *user_data)
{
  ArvBuffer *buffer;

  /* This code is called from the stream receiving thread, which means all the time spent there is
   * less time available for the reception of incoming packets */

  buffer = arv_stream_pop_buffer(stream);

  if (ARV_IS_BUFFER(buffer)) {
    std::shared_ptr<Buffer> buf = std::make_shared<Buffer>(buffer);
    std::thread(&CameraManager::process_image, static_cast<CameraManager *>(user_data), buf)
        .detach();

    /* Don't destroy the buffer, but put it back into the buffer pool */
    arv_stream_push_buffer(stream, buffer);
  }
}

void CameraManager::runCameraCallback()
{
  GError *error = nullptr;

  main_loop = g_main_loop_new(nullptr, FALSE);

  /* Connect to the first available camera */
  camera = arv_camera_new(nullptr, &error);

  if (ARV_IS_CAMERA(camera)) {
    ArvStream *stream = nullptr;

    printf("Found camera '%s'\n", arv_camera_get_model_name(camera, nullptr));

    EXEC_AND_CHECK(
        arv_camera_set_acquisition_mode(camera, ARV_ACQUISITION_MODE_CONTINUOUS, &error));

    if (error == nullptr)
      /* Create the stream object without callback */
      EXEC_AND_CHECK(stream = arv_camera_create_stream(camera, nullptr, nullptr, &error));

    if (ARV_IS_STREAM(stream)) {
      size_t payload;

      /* Retrieve the payload size for buffer creation */
      EXEC_AND_CHECK(payload = arv_camera_get_payload(camera, &error));
      if (error == nullptr) {
        /* Insert some buffers in the stream buffer pool */
        for (int ii = 0; ii < 5; ii++)
          arv_stream_push_buffer(stream, arv_buffer_new(payload, nullptr));
      }

      g_signal_connect(stream, "new-buffer", G_CALLBACK(new_buffer_cb), this);
      arv_stream_set_emit_signals(stream, TRUE);

      // 1. Set the trigger source (e.g., "Line0")
      EXEC_AND_CHECK(arv_camera_set_trigger_source(camera, "Line0", &error));

      // 2. Enable external trigger mode on that source
      EXEC_AND_CHECK(arv_camera_set_trigger(camera, "Line0", &error));

      // Optional: Configure activation edge via GenICam device feature access
      ArvDevice *device = arv_camera_get_device(camera);
      EXEC_AND_CHECK(
          arv_device_set_string_feature_value(device, "TriggerSelector", "FrameStart", &error));
      EXEC_AND_CHECK(
          arv_device_set_string_feature_value(device, "TriggerActivation", "RisingEdge", &error));

      if (error == nullptr)
        /* Start the acquisition */
        EXEC_AND_CHECK(arv_camera_start_acquisition(camera, &error));

      EXEC_AND_CHECK(arv_camera_set_exposure_time_auto(camera, ARV_AUTO_OFF, &error));
      EXEC_AND_CHECK(arv_camera_set_gain_auto(camera, ARV_AUTO_OFF, &error));

      camera_running = true;

      setExposure(requested_exposure);
      setGain(requested_gain);

      if (error == nullptr)
        g_main_loop_run(main_loop);

      if (error == nullptr)
        /* Stop the acquisition */
        EXEC_AND_CHECK(arv_camera_stop_acquisition(camera, &error));

      arv_stream_set_emit_signals(stream, FALSE);

      /* Destroy the stream object */
      g_clear_object(&stream);
    }

    /* Destroy the camera instance */
    g_clear_object(&camera);
  }

  g_main_loop_unref(main_loop);

  if (error != nullptr) {
    /* En error happened, display the correspdonding message */
    printf("Error: %s\n", error->message);
  }
}

void CameraManager::process_image(std::shared_ptr<Buffer> buf)
{
  if (save_images > 0) {
    save_images--;
    std::thread(&Buffer::savePtr, buf).detach();
  }
  if (save_video) {
    save_video_queue.push(buf);
  }
#if 0
  for (int ii = 0; ii < 5; ++ii) {
      cv::Mat3b img = buf->exposureColored(this);
      cv::imwrite("test-" + std::to_string(ii) + ".jpg", img);
  }
  abort();
#endif
  static bool process_running = false;
  if (process_running) {
    return;
  }
  PARALLELTIME_FUNCTION();
  process_running = true;
  SetFalseOnDestruct set_false(process_running);
  cv::Mat3b colored = buf->exposureColored(this);
  if (colored.empty()) {
    return;
  }
  if (denoise) {
    colored = Misc::denoiseValue(colored, denoise_scale);
  }
  if (crosshairs) {
    drawCrosshairs(colored);
    if (crosshair_window) {
      cv::Point center(colored.cols / 2, colored.rows / 2);
      int const size = std::min(colored.cols, colored.rows) / 20;
      cv::Rect roi(center.x - size, center.y - size, 2 * size, 2 * size);
      cv::imshow(crosshair_window_name, colored(roi));
    }
  }
  int const x = (colored.size().width * (100 - crop)) / 200;
  int const y = (colored.size().height * (100 - crop)) / 200;
  int const width = colored.size().width - 2 * x;
  int const height = colored.size().height - 2 * y;
  cv::Rect const roi(x, y, width, height);
  if (100 != crop) {
    colored = colored(roi);
  }
  if (show_sharpness) {
    double const sharpness = 100 *
                             (1.0 - 1000 * cv::videostab::calcBlurriness(buf->get_raw_8()(roi)));
    Misc::println("Sharpness: {:3.3f}", sharpness);
  }
  cv::Mat3b downscaled = downscale_if_neccessary(colored, max_width_shown);
  cv::imshow(window_name, downscaled);
}

void CameraManager::drawCrosshairs(cv::Mat3b &img)
{
  cv::Vec3d const red(0, 0, 255);
  int const dash_length = 2;
  int const row_center = img.rows / 2;
  int const col_center = img.cols / 2;
  for (int row = 0; row < img.rows; ++row) {
    int const center_dist = std::abs(row - row_center);
    if (1 == (((center_dist) / dash_length) % 2)) {
      img(row, col_center) = red;
    }
  }
  for (int col = 0; col < img.cols; ++col) {
    int const center_dist = std::abs(col - col_center);
    if (1 == (((center_dist) / dash_length) % 2)) {
      img(row_center, col) = red;
    }
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

void CameraManager::runVideoWorker()
{
  while (!stopped) {
    if (videoWriteWorker() < 1) {
      sleep(1);
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

void CameraManager::setWBS1(int val)
{
  balance.setS1(val);
}

void CameraManager::setWBS2(int val)
{
  balance.setS2(val);
}

void CameraManager::setAutoWB(const bool val)
{
  auto_wb = val;
  println("Setting auto-WB to {}", auto_wb);
}

void CameraManager::setDenoise(const bool val)
{
  denoise = val;
}

void CameraManager::setDenoiseScale(const int val)
{
  denoise_scale = val;
}

void CameraManager::setSharpness(const bool val)
{
  show_sharpness = val;
}

void CameraManager::setSaveVideo(const bool val)
{
  save_video = val;
  println("Save video: {}", save_video);
  if (!val) {
    std::lock_guard guard(video_writer_mutex);
  }
}

void CameraManager::setCrop(const int val)
{
  crop = std::clamp(val, 1, 100);
}

void CameraManager::setTriggerSource(const QString &str)
{
  std::string val = str.toStdString();
  println("Trying to set trigger source {}", val);
  GError *error = nullptr;
  std::lock_guard guard(arv_mutex);
  EXEC_AND_CHECK(arv_camera_set_trigger_source(camera, val.c_str(), &error));
  if (nullptr != error) {
    println("Error: {}", error->message);
  }
  EXEC_AND_CHECK(
      println("Trigger source was set to {}", arv_camera_get_trigger_source(camera, &error)));
}

void CameraManager::setModeContinuous()
{
  println("Setting acquisition mode to continuous");
  EXEC_AND_CHECK(arv_camera_set_acquisition_mode(camera, ARV_ACQUISITION_MODE_CONTINUOUS, &error));
}

void CameraManager::setModeSingle()
{
  println("Setting acquisition mode to single");
  EXEC_AND_CHECK(
      arv_camera_set_acquisition_mode(camera, ARV_ACQUISITION_MODE_SINGLE_FRAME, &error));
}

void CameraManager::handleWhiteBalance(cv::Mat3b &img, bool skip_auto_wb)
{
  if (auto_wb && !skip_auto_wb) {
    balance.calculateParameters(img, wb_min0, wb_max0, wb_min1, wb_max1, wb_min2, wb_max2);
    emit requestedWBmin0(wb_min0);
    emit requestedWBmin1(wb_min1);
    emit requestedWBmin2(wb_min2);
    emit requestedWBmax0(wb_max0);
    emit requestedWBmax1(wb_max1);
    emit requestedWBmax2(wb_max2);
    println("WB parameters: \n{}\t{}\n{}\t{}\n{}\t{}",
            wb_min0,
            wb_max0,
            wb_min1,
            wb_max1,
            wb_min2,
            wb_max2);
  }
  balance.applyParameters(img, img, wb_min0, wb_max0, wb_min1, wb_max1, wb_min2, wb_max2);
}

void CameraManager::stop()
{
  stopped = true;
}
