#include <chrono>
#include <iostream>
#include <string>

#include <filesystem>
namespace fs = std::filesystem;

#include <opencv2/opencv.hpp>
#include <tiffio.h>

#include <runningstats/runningstats.h>
namespace rs = runningstats;

bool saveMatToTiffZstd(const cv::Mat &mat,
                       const std::string &output_path,
                       bool const zstd,
                       int const level,
                       int const rows_per_strip)
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

    // Doesn't exist in OpenCV 5 ?
    //params.push_back(cv::IMWRITE_TIFF_COMPRESSION_LEVEL);
    //params.push_back(level);
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

  uint32_t width = mat.cols;
  uint32_t height = mat.rows;
  uint16_t channels = mat.channels();
  uint16_t bits_per_sample = mat.elemSize1() * 8;

  // Set standard TIFF metadata tags
  TIFFSetField(out, TIFFTAG_IMAGEWIDTH, width);
  TIFFSetField(out, TIFFTAG_IMAGELENGTH, height);
  TIFFSetField(out, TIFFTAG_BITSPERSAMPLE, bits_per_sample);
  TIFFSetField(out, TIFFTAG_SAMPLESPERPIXEL, channels);
  TIFFSetField(out, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
  TIFFSetField(out, TIFFTAG_IMAGEDESCRIPTION, "my little comment in the imagedescription field");

  TIFFSetField(out, TIFFTAG_ROWSPERSTRIP, rows_per_strip);

  if (channels == 1) {
    TIFFSetField(out, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK);
  }
  else {
    TIFFSetField(out, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);
  }

  // Enable Zstd Compression and Horizontal Differencing Predictor
  if (zstd) {
    TIFFSetField(out, TIFFTAG_COMPRESSION, COMPRESSION_ZSTD);
    TIFFSetField(out, TIFFTAG_ZSTD_LEVEL, level);
  }
  else {
    TIFFSetField(out, TIFFTAG_COMPRESSION, COMPRESSION_LZW);
  }
  TIFFSetField(out, TIFFTAG_PREDICTOR, PREDICTOR_HORIZONTAL);

  // Write image row by row
  for (uint32_t row = 0; row < height; ++row) {
    // Create a deep copy of the row so libtiff's in-place predictor
    // doesn't corrupt the original cv::Mat data in the loop.
    cv::Mat row_clone = mat.row(row).clone();

    // Note: row_clone.data is a pointer, not a function (no parentheses)
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

int run_test(cv::Mat const &img,
             std::string const &output_path,
             bool const zstd,
             int const level,
             int const rows_per_strip,
             std::ostream &times,
             std::ostream &sizes)
{
  rs::RunningStats stats;
  for (size_t ii = 0; ii < 20; ++ii) {
    fs::remove(output_path);
    // --- 2. Write Action (using libtiff + Zstd + Predictor) ---
    auto start_write = std::chrono::high_resolution_clock::now();

    bool success = saveMatToTiffZstd(img, output_path, zstd, level, rows_per_strip);

    auto end_write = std::chrono::high_resolution_clock::now();

    if (!success) {
      std::cerr << "Error: Failed to write output TIFF file.\n";
      return 1;
    }

    // --- Calculate and Print Durations ---
    std::chrono::duration<double, std::milli> write_duration = end_write - start_write;

    // std::cout << "Write action took: " << write_duration.count() << " ms" << std::endl;
    stats.push_unsafe(write_duration.count());
  }
  std::cout << "ZSTD: " << zstd << ", level: " << level << ", rows per strip: " << rows_per_strip
            << std::endl;
  std::cout << "Write duration stats: " << stats.print() << std::endl;
  std::cout << "Size in bytes: " << fs::file_size(output_path) << std::endl;
  sizes << fs::file_size(output_path) << "\t";
  times << stats.getMean() << "\t";
  return 0;
}

int main(int argc, char *argv[])
{
  if (argc < 3) {
    std::cerr << "Usage: " << argv[0] << " <input.tif> <output.tif>\n";
    return 1;
  }

  std::string input_path = argv[1];
  std::string output_path = argv[2];

  // --- 1. Read Action (using OpenCV) ---
  auto start_read = std::chrono::high_resolution_clock::now();

  // cv::IMREAD_UNCHANGED preserves bit depth (e.g., 16-bit) and multi-channel configurations
  cv::Mat img = cv::imread(input_path, cv::IMREAD_UNCHANGED);

  auto end_read = std::chrono::high_resolution_clock::now();

  std::chrono::duration<double, std::milli> read_duration = end_read - start_read;
  std::cout << "Read action took:  " << read_duration.count() << " ms\n";

  if (img.empty()) {
    std::cerr << "Error: Failed to read input image with OpenCV: " << input_path << "\n";
    return 1;
  }

  std::stringstream times;
  std::stringstream sizes;

  int const default_rows_per_strip = 32;

#if 1  // Compare LZW against ZSTD
  run_test(img, output_path, false, 0, default_rows_per_strip, times, sizes);
  for (int ii = 1; ii <= 7; ++ii) {
    run_test(img, output_path, true, ii, default_rows_per_strip, times, sizes);
    std::cout << "Times: " << times.str() << std::endl;
    std::cout << "Sizes: " << sizes.str() << std::endl;
    std::cout << std::endl;
  }
#endif
#if 0  // compare different values for rows per strip
  for (int ii = 1; ii <= 8192; ii *= 2) {
    run_test(img, output_path, false, 1, ii, times, sizes);
    std::cout << "Times:\t" << times.str() << std::endl;
    std::cout << "Sizes:\t" << sizes.str() << std::endl;
    std::cout << std::endl;
  }
#endif

  return 0;
}
