#include "zstd-writer.h"

#include <iostream>

#include <tiffio.h>

bool zstd_writer(const std::string &output_path, const cv::Mat &mat, const zstd_options &opt)
{
#if 0
  std::vector<int> params;
  params.push_back(cv::IMWRITE_TIFF_PREDICTOR);
  params.push_back(cv::IMWRITE_TIFF_PREDICTOR_HORIZONTAL);
  params.push_back(cv::IMWRITE_TIFF_ROWSPERSTRIP);
  params.push_back(rows_per_strip);
  if (zstd) {
    params.push_back(cv::IMWRITE_TIFF_COMPRESSION);
    params.push_back(cv::IMWRITE_TIFF_COMPRESSION_ZSTD);

    params.push_back(cv::IMWRITE_TIFF_COMPRESSION_LEVEL);
    params.push_back(level);
  } else {
    params.push_back(cv::IMWRITE_TIFF_COMPRESSION);
    params.push_back(cv::IMWRITE_TIFF_COMPRESSION_LZW);
  }
  cv::imwrite(output_path, mat, params);
  return true;
#else
  TIFF *out = TIFFOpen(output_path.c_str(), "w");
  if (!out) {
    std::cerr << "Error: Failed to open output TIFF file: " << output_path << "\n";
    return false;
  }

  uint32_t const width = mat.cols;
  uint32_t const height = mat.rows;
  uint16_t const channels = mat.channels();
  uint16_t const bits_per_sample = mat.elemSize1() * 8;

  // Set standard TIFF metadata tags
  TIFFSetField(out, TIFFTAG_IMAGEWIDTH, width);
  TIFFSetField(out, TIFFTAG_IMAGELENGTH, height);
  TIFFSetField(out, TIFFTAG_BITSPERSAMPLE, bits_per_sample);
  TIFFSetField(out, TIFFTAG_SAMPLESPERPIXEL, channels);
  TIFFSetField(out, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);

  if (!opt.desc.empty()) {
    TIFFSetField(out, TIFFTAG_IMAGEDESCRIPTION, opt.desc);
  }

  TIFFSetField(out, TIFFTAG_ROWSPERSTRIP, opt.rows_per_strip);

  if (channels == 1) {
    TIFFSetField(out, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK);
  }
  else {
    TIFFSetField(out, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);
  }

  // Enable Zstd Compression and Horizontal Differencing Predictor
  TIFFSetField(out, TIFFTAG_COMPRESSION, COMPRESSION_ZSTD);
  TIFFSetField(out, TIFFTAG_ZSTD_LEVEL, opt.level);
  TIFFSetField(out, TIFFTAG_PREDICTOR, PREDICTOR_HORIZONTAL);

  // Write image row by row
  for (uint32_t row = 0; row < height; ++row) {
    // Create a deep copy of the row so libtiff's in-place predictor
    // doesn't corrupt the original cv::Mat data in the loop.
    cv::Mat row_clone = mat.row(row).clone();

    if (TIFFWriteScanline(out, row_clone.data, row, 0) < 0) {
      std::cerr << "Error writing scanline at row " << row << "\n";
      TIFFClose(out);
      return false;
    }
  }

  TIFFClose(out);
  return true;
#endif
}
