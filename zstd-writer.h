#ifndef ZSTD_WRITER_H
#define ZSTD_WRITER_H

#include <opencv2/core.hpp>

struct zstd_options {
  /**
   * @brief compression level for ZSTD.
   * Valid values are 1-22, values > 3 give diminishing returns for compression ratio with rapidly
   * rising compute time.
   */
  int level = 1;

  /**
   * @brief rows per strip. There is very little influence on the resulting size, so keeping the
   * default makes sense.
   */
  int rows_per_strip = 32;

  /**
   * @brief custom value for image description tag
   */
  std::string desc = "";
};

bool zstd_writer(const std::string &output_path, const cv::Mat &mat, zstd_options const &opt = {});

#endif  // ZSTD_WRITER_H
