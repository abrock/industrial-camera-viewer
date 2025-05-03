#include "misc.h"

#include <opencv2/imgproc.hpp>
#include <opencv2/photo.hpp>

namespace Misc {

std::vector<std::string> splitString(const std::string &in, const char s)
{
  std::string current;
  std::vector<std::string> result;
  for (size_t ii = 0; ii < in.size(); ++ii) {
    if (in[ii] == s) {
      if (current.size() > 0) {
        result.push_back(current);
      }
      current.clear();
    }
    else {
      current += in[ii];
    }
  }
  if (current.size() > 0) {
    result.push_back(current);
  }
  return result;
}

void unpack12_16(uchar *dst, const uchar *src, const size_t input_size)
{
  size_t ii_out = 0;
  for (size_t ii = 0; ii < input_size; ii += 3, ii_out += 4) {
    uchar const first_low = src[ii] << 4;
    uchar const first_high = (src[ii + 1] << 4) | (src[ii] >> 4);
    dst[ii_out + 0] = first_low;
    dst[ii_out + 1] = first_high;

    uchar const second_low = src[ii + 1] & uchar(0xf0);
    uchar const second_high = src[ii + 2];
    dst[ii_out + 2] = second_low;
    dst[ii_out + 3] = second_high;
  }
}

std::string type2str(int type)
{
  std::string r;

  uchar depth = type & CV_MAT_DEPTH_MASK;
  uchar channels = 1 + (type >> CV_CN_SHIFT);

  switch (depth) {
    case CV_8U:
      r = "8U";
      break;
    case CV_8S:
      r = "8S";
      break;
    case CV_16U:
      r = "16U";
      break;
    case CV_16S:
      r = "16S";
      break;
    case CV_32S:
      r = "32S";
      break;
    case CV_32F:
      r = "32F";
      break;
    case CV_64F:
      r = "64F";
      break;
    default:
      r = "User";
      break;
  }

  r += "C";
  r += (channels + '0');

  return r;
}

std::string envVar(const std::string &key)
{
  const char *val = std::getenv(key.c_str());
  return nullptr == val ? "" : val;
}

double compress_16_8_gamma_float(const double val, const double gamma)
{
  // Transform from 12bit which was scaled to 16bit to range 0-1
  double result = val / (4095 * 16);
  result = std::pow(result, gamma);

  // Transform from 0-1 to 0-255
  return result * 255;
}

double uncompress_16_8_gamma_float(const double val, const double gamma)
{
  // Transform from 0-255 to 0-1
  double result = val / 255;
  result = std::pow(result, 1.0 / gamma);

  // Transform from 0-1 to 0-(12bit scaled to 16bit)
  return result * (4095 * 16);
}

uint8_t compress_16_8_gamma_int(const int val, const double gamma)
{
  return cv::saturate_cast<uint8_t>(std::round(compress_16_8_gamma_float(val, gamma)));
}

uint16_t uncompress_16_8_gamma_int(const int val, const double gamma)
{
  return cv::saturate_cast<uint16_t>(std::round(uncompress_16_8_gamma_float(val, gamma)));
}

cv::Mat_<uint8_t> compress_16_8_gamma(const cv::Mat_<uint16_t> &img, const double gamma)
{
  if (img.depth() == CV_8S || img.depth() == CV_8U) {
    return img;
  }
  cv::Mat_<uint8_t> result(img.size());
  for (int row = 0; row < img.rows; ++row) {
    for (int col = 0; col < img.cols; ++col) {
      result(row, col) = std::round(compress_16_8_gamma_float(img(row, col), gamma));
    }
  }
  return result;
}

std::vector<uint8_t> get_gamma_lut(const double gamma)
{
  std::vector<uint8_t> result(4096);
  for (size_t ii = 0; ii < result.size(); ++ii) {
    result[ii] = compress_16_8_gamma_int(ii * 16, gamma);
  }
  return result;
}

cv::Mat_<uint8_t> compress_16_8_lut(const cv::Mat_<uint16_t> &img, const std::vector<uint8_t> &lut)
{
  if (img.depth() == CV_8S || img.depth() == CV_8U) {
    return img;
  }
  cv::Mat_<uint8_t> result(img.size());
  for (int row = 0; row < img.rows; ++row) {
    for (int col = 0; col < img.cols; ++col) {
      result(row, col) = lut.at(img(row, col) / 16);
    }
  }
  return result;
}

cv::Mat1b apply_gamma(const cv::Mat &input, const double gamma)
{
  if (input.channels() > 1) {
    std::vector<cv::Mat> splitted;
    cv::split(input, splitted);
    for (cv::Mat &channel : splitted) {
      channel = apply_gamma(channel, gamma);
    }
    cv::Mat result;
    cv::merge(splitted, result);
    return result;
  }
  if (input.depth() == 2) {
    std::vector<uint8_t> lut = get_gamma_lut(gamma);
    return compress_16_8_lut(input, lut);
  }
  return apply_gamma_8(input, gamma);
}

cv::Mat1b apply_gamma_8(const cv::Mat1b &input, const double gamma)
{
  cv::Mat1b result(input);
  std::vector<uint8_t> lut;
  for (size_t ii = 0; ii <= 255; ++ii) {
    lut.push_back(std::sqrt(double(ii)) * std::sqrt(255));
  }
  for (int row = 0; row < result.rows; ++row) {
    for (int col = 0; col < result.cols; ++col) {
      result(row, col) = lut.at(result(row, col));
    }
  }
  return result;
}

cv::Mat3b denoiseValue(const cv::Mat3b &img, int const size)
{
  cv::Mat hsv;
  cv::cvtColor(img, hsv, cv::COLOR_BGR2HSV_FULL);
  std::vector<cv::Mat> splitted;
  cv::split(hsv, splitted);
  cv::medianBlur(splitted[0], splitted[0], ceil_odd(size));
  cv::medianBlur(splitted[1], splitted[1], ceil_odd(size));
  cv::merge(splitted, hsv);
  cv::cvtColor(hsv, hsv, cv::COLOR_HSV2BGR_FULL);
  return hsv;
}

int ceil_odd(const int val)
{
  if (val < 3) {
    return 3;
  }
  if (val % 2 == 0) {
    return val + 1;
  }
  return val;
}

}  // namespace Misc
