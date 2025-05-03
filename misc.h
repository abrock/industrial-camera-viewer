#ifndef MISC_H
#define MISC_H

#include <fmt/core.h>
#include <iostream>

#include <opencv2/core.hpp>

namespace Misc {
template<typename... Args> inline void println(fmt::format_string<Args...> s, Args &&...args)
{
  std::cout << fmt::format(s, std::forward<Args>(args)...) << std::endl;
}

template<class T, int N> bool is_finite(cv::Vec<T, N> const &val)
{
  for (size_t ii = 0; ii < N; ++ii) {
    if (!std::isfinite(val[ii])) {
      return false;
    }
  }
  return true;
}

std::vector<std::string> splitString(const std::string &in, const char s);

void unpack12_16(uchar *dst, const uchar *src, size_t const input_size);

/**
 * @brief type2str takes an OpenCV pixel type and returns a human-readable string.
 * @param type
 * @return
 */
std::string type2str(int type);

std::string envVar(std::string const &key);

double compress_16_8_gamma_float(const double val, const double gamma);

double uncompress_16_8_gamma_float(const double val, const double gamma);

uint8_t compress_16_8_gamma_int(const int val, const double gamma);

uint16_t uncompress_16_8_gamma_int(const int val, const double gamma);

cv::Mat_<uint8_t> compress_16_8_gamma(const cv::Mat_<uint16_t> &img, const double gamma);

std::vector<uint8_t> get_gamma_lut(const double gamma);

cv::Mat_<uint8_t> compress_16_8_lut(const cv::Mat_<uint16_t> &img,
                                    const std::vector<uint8_t> &lut);

cv::Mat1b apply_gamma_8(cv::Mat1b const &input, double const gamma);

cv::Mat1b apply_gamma(cv::Mat const &input, double const gamma);

int ceil_odd(int const val);

cv::Mat3b denoiseValue(cv::Mat3b const &img, const int size = 10);

}  // namespace Misc

#endif  // MISC_H
