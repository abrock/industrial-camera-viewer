#include "buffer.h"

#include "misc.h"

#include "arv_ext.h"

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/xphoto/white_balance.hpp>

#include <filesystem>
namespace fs = std::filesystem;

void Buffer::make_img_8(ArvBuffer *buf)
{
  img.create(height, width, cv_type);
  const void *data = arv_buffer_get_data(buf, &size);
  memcpy(img.data, data, size);
}

void Buffer::make_img_12(ArvBuffer *buf)
{
  img.create(height, width, cv_type);
  const void *data = arv_buffer_get_data(buf, &size);
  Misc::unpack12_16(img.data, static_cast<const unsigned char *>(data), size);
}

void Buffer::make_img_16(ArvBuffer *buf)
{
  img.create(height, width, cv_type);
  const void *data = arv_buffer_get_data(buf, &size);
  memcpy(img.data, data, size);
  img *= 16;
}

bool Buffer::isBayer() const
{
  return ArvExt::isBayer(pixel_format);
}

std::string Buffer::generateFilename() const
{
  static int counter = 0;
  std::string fn;
  std::string const extension = isBayer() ? "dng" : "tif";
  do {
    fn = fmt::format("cv-{}-{}-{}.{}", system_timestamp, frame_id, counter, extension);
    counter++;
  } while (fs::exists(fn));
  return fn;
}

cv::Mat1b Buffer::clippingMask(const cv::Mat1b &img)
{
  cv::Mat1b result(img.size(), uint8_t(0));
  for (int row = 0; row < img.rows; ++row) {
    for (int col = 0; col < img.cols; ++col) {
      if (img(row, col) >= 255) {
        result(row, col) = 255;
      }
    }
  }
  return result;
}

cv::Mat1b Buffer::clippingMask(const cv::Mat1w &img)
{
  cv::Mat1b result(img.size(), uint8_t(0));
  for (int row = 0; row < img.rows; ++row) {
    for (int col = 0; col < img.cols; ++col) {
      if (img(row, col) >= 4095 * 16) {
        result(row, col) = 255;
      }
    }
  }
  return result;
}

cv::Mat1b Buffer::clippingMask(const cv::Mat &img)
{
  switch (img.type()) {
    case CV_8UC1:
      return clippingMask(cv::Mat1b(img));
    case CV_16UC1:
      return clippingMask(cv::Mat1w(img));
  }
  throw std::runtime_error("Function not implemented for given type");
}

void maskExposure(cv::Mat3b &img, cv::Mat1b const &mask, int const offset)
{
  int const width = 20;
  for (int row = 0; row < img.rows; ++row) {
    for (int col = 0; col < img.cols; ++col) {
      if (mask(row, col) < 127) {
        continue;
      }
      int const position = (row - col + 2 * offset) % (2 * width);
      img(row, col) = position > width ? cv::Vec3b(0, 0, 0) : cv::Vec3b(255, 255, 255);
    }
  }
}

cv::Mat3b Buffer::exposureColored() const
{
  cv::Mat1b const over_exposed = clippingMask(img);
  cv::Mat1b tmp = Misc::apply_gamma(img, 1.0 / 2.0);
  cv::Mat3b result;
  if (isBayer()) {
    cv::demosaicing(tmp, result, ArvExt::demosaicingVNG(pixel_format));
    cv::cvtColor(result, result, cv::COLOR_BGR2RGB);
    cv::xphoto::createSimpleWB()->balanceWhite(result, result);
  }
  else {
    cv::merge(std::vector<cv::Mat>{tmp, tmp, tmp}, result);
  }

  maskExposure(result, over_exposed, frame_id);

  return result;
}

void Buffer::save(const std::string &fn) const
{
  if (isBayer()) {
    saveDNG(fn);
    return;
  }
  cv::imwrite(fn, img, {cv::IMWRITE_TIFF_COMPRESSION, cv::IMWRITE_TIFF_COMPRESSION_LZW});
}

void Buffer::save() const
{
  save(generateFilename());
}

void Buffer::savePtr(std::shared_ptr<Buffer> buf)
{
  buf->save();
}

#define TINY_DNG_WRITER_IMPLEMENTATION
#include "tiny_dng_writer.h"

bool const big_endian = false;

void setup_image(tinydngwriter::DNGImage &dng_image, cv::Mat const &img)
{
  dng_image.SetBigEndian(big_endian);
  unsigned int image_width = img.size().width;
  unsigned int image_height = img.size().height;
  dng_image.SetSubfileType(false, false, false);
  dng_image.SetImageWidth(image_width);
  dng_image.SetImageLength(image_height);
  // We do not yet support multiple strips per image, so must set RowsPerStrip to image.height.
  // TODO: https://github.com/syoyo/tinydng/issues/40
  dng_image.SetRowsPerStrip(image_height);
  dng_image.SetPlanarConfig(tinydngwriter::PLANARCONFIG_CONTIG);
  dng_image.SetCompression(tinydngwriter::COMPRESSION_NONE);
  dng_image.SetPhotometric(tinydngwriter::PHOTOMETRIC_CFA);
  dng_image.SetXResolution(300.0);
  dng_image.SetYResolution(300.0);
  dng_image.SetOrientation(tinydngwriter::ORIENTATION_TOPLEFT);
  dng_image.SetResolutionUnit(tinydngwriter::RESUNIT_NONE);
  dng_image.SetImageDescription("Some image description");
  dng_image.SetUniqueCameraModel("Some camera model");

  dng_image.SetDNGVersion(1, 2, 0, 0);

  dng_image.SetCFARepeatPatternDim(2, 2);

  uint8_t cfa_pattern[4] = {0, 1, 1, 2};
  dng_image.SetCFAPattern(4, cfa_pattern);
}

void finalize_write(tinydngwriter::DNGImage &dng_image, std::string const fn)
{
  tinydngwriter::DNGWriter dng_writer(big_endian);
  bool ret = dng_writer.AddImage(&dng_image);
  if (!ret) {
    std::cout << "AddImage failed for " << fn << std::endl;
    return;
  }

  std::string err;
  ret = dng_writer.WriteToFile(fn.c_str(), &err);

  if (!err.empty()) {
    std::cerr << err;
  }

  if (!ret) {
    std::cout << "WriteToFile failed for " << fn << std::endl;
    return;
  }
}

void write_uint8_t(tinydngwriter::DNGImage &dng_image, cv::Mat const &img)
{
  dng_image.SetSamplesPerPixel(1);
  uint16_t bps[1] = {8};
  dng_image.SetBitsPerSample(1, bps);

  dng_image.SetImageData(img.data, img.size().area() * sizeof(uint8_t));
}

void write_uint16_t(tinydngwriter::DNGImage &dng_image, cv::Mat const &img)
{
  dng_image.SetSamplesPerPixel(1);
  uint16_t bps[1] = {16};
  dng_image.SetBitsPerSample(1, bps);

  dng_image.SetImageData(img.data, img.size().area() * sizeof(uint16_t));
}

void Buffer::saveDNG(const std::string &fn) const
{
  tinydngwriter::DNGImage dng_image;
  setup_image(dng_image, img);

  switch (img.depth()) {
    case CV_8U:
      write_uint8_t(dng_image, img);
      break;
    case CV_16U:
      write_uint16_t(dng_image, img);
      break;
    default:
      throw std::runtime_error("saveDNG is not implemented for the given depth");
  }
  finalize_write(dng_image, fn);
}

Buffer::Buffer(ArvBuffer *buf)
{
  height = arv_buffer_get_image_height(buf);
  width = arv_buffer_get_image_width(buf);
  pixel_format = arv_buffer_get_image_pixel_format(buf);
  bits = ARV_PIXEL_FORMAT_BIT_PER_PIXEL(pixel_format);
  cv_type = bits > 8 ? CV_16UC1 : CV_8UC1;
  system_timestamp = arv_buffer_get_system_timestamp(buf);

  frame_id = arv_buffer_get_frame_id(buf);

  switch (bits) {
    case 8:
      make_img_8(buf);
      break;
    case 12:
      make_img_12(buf);
      break;
    case 16:
      make_img_16(buf);
      break;
    default:
      Misc::println("Couldn't make an image for pixel format {}", pixel_format);
      return;
  }

  double min = 0;
  double max = 0;
  cv::minMaxIdx(img, &min, &max);

  /*
  Misc::println("Acquired {}×{} buffer #{}, bpp {}, size {} (=> {}bpp, value range {}-{})",
                width,
                height,
                frame_id,
                bits,
                size,
                double(8 * size) / (width * height),
                min,
                max);
                */
}
