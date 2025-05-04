#ifndef WHITEBALANCE_H
#define WHITEBALANCE_H

#include <opencv2/core.hpp>

class WhiteBalance {
 private:
  float inputMin = 0;
  float inputMax = 255;
  float outputMin = 0;
  float outputMax = 255;
  float p = 2;

 public:
  float getInputMin() const;
  void setInputMin(float val);

  float getInputMax() const;
  void setInputMax(float val);

  float getOutputMin() const;
  void setOutputMin(float val);

  float getOutputMax() const;
  void setOutputMax(float val);

  float getP() const;
  void setP(float val);

  void calculateParameters(cv::InputArray _src,
                           float &min0,
                           float &max0,
                           float &min1,
                           float &max1,
                           float &min2,
                           float &max2);

  void applyParameters(cv::InputArray src,
                       cv::OutputArray dst,
                       const float min0,
                       const float max0,
                       const float min1,
                       const float max1,
                       const float min2,
                       const float max2);
};

#endif  // WHITEBALANCE_H
