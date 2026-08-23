#include <gtest/gtest.h>

#include "mcheck.hpp"

namespace {
int side_effect_counter = 0;
int get_sec()
{
  return ++side_effect_counter;
}
int reset_sec()
{
  int const val = side_effect_counter;
  side_effect_counter = 0;
  return val;
}

}  // namespace

#define EXPECT_SE(statement) \
  statement << get_sec(); \
  EXPECT_EQ(1, reset_sec());

#define EXPECT_NO_SE(statement) \
  statement << get_sec(); \
  EXPECT_EQ(0, reset_sec());

TEST(MCHECK, ALL)
{
  side_effect_counter = 0;

  EXPECT_EQ(0, side_effect_counter);

  MCHECK(true) << get_sec();
  EXPECT_EQ(0, reset_sec());

  MCHECK(false) << get_sec();
  EXPECT_EQ(1, reset_sec());

  MCHECK_NOT_NULL(&side_effect_counter) << get_sec();
  EXPECT_EQ(0, reset_sec());

  MCHECK_NOT_NULL(nullptr) << get_sec();
  EXPECT_EQ(1, reset_sec());

  MCHECK_EQ(1, 1) << get_sec();
  EXPECT_EQ(0, reset_sec());

  double const fnan = std::numeric_limits<float>::quiet_NaN();
  double const dnan = std::numeric_limits<double>::quiet_NaN();

  EXPECT_SE(MCHECK_EQ(1, 0));
  EXPECT_SE(MCHECK_EQ(1, 1.5));
  EXPECT_SE(MCHECK_EQ(1, fnan));
  EXPECT_SE(MCHECK_EQ(1, dnan));
  EXPECT_SE(MCHECK_EQ(fnan, fnan));
  EXPECT_SE(MCHECK_EQ(dnan, dnan));

  EXPECT_NO_SE(MCHECK_EQ(1, 1));
  EXPECT_NO_SE(MCHECK_LE(1, 1));
  EXPECT_NO_SE(MCHECK_GE(1, 1));
  EXPECT_NO_SE(MCHECK_NE(1, 2));

  EXPECT_SE(MCHECK_LT(1, 1));
  EXPECT_SE(MCHECK_GT(1, 1));
  EXPECT_SE(MCHECK_NE(1, 1));

  for (float val : {-1.0f,
                    -0.5f,
                    0.0f,
                    1.0f,
                    9999.999f,
                    std::numeric_limits<float>::max(),
                    std::numeric_limits<float>::min()})
  {
    EXPECT_SE(MCHECK_EQ(val, fnan));
    EXPECT_SE(MCHECK_EQ(fnan, val));
    EXPECT_SE(MCHECK_LE(val, fnan));
    EXPECT_SE(MCHECK_LE(fnan, val));
    EXPECT_SE(MCHECK_LT(val, fnan));
    EXPECT_SE(MCHECK_LT(fnan, val));
    EXPECT_SE(MCHECK_GE(val, fnan));
    EXPECT_SE(MCHECK_GE(fnan, val));
    EXPECT_SE(MCHECK_GT(val, fnan));
    EXPECT_SE(MCHECK_GT(fnan, val));
    EXPECT_NO_SE(MCHECK_NE(fnan, val));
    EXPECT_NO_SE(MCHECK_NE(val, fnan));
  }

  for (double val : {-1.0,
                     -0.5,
                     0.0,
                     1.0,
                     9999.999,
                     std::numeric_limits<double>::max(),
                     std::numeric_limits<double>::min()})
  {
    EXPECT_SE(MCHECK_EQ(val, dnan));
    EXPECT_SE(MCHECK_EQ(dnan, val));
    EXPECT_SE(MCHECK_LE(val, dnan));
    EXPECT_SE(MCHECK_LE(dnan, val));
    EXPECT_SE(MCHECK_LT(val, dnan));
    EXPECT_SE(MCHECK_LT(dnan, val));
    EXPECT_SE(MCHECK_GE(val, dnan));
    EXPECT_SE(MCHECK_GE(dnan, val));
    EXPECT_SE(MCHECK_GT(val, dnan));
    EXPECT_SE(MCHECK_GT(dnan, val));
    EXPECT_NO_SE(MCHECK_NE(dnan, val));
    EXPECT_NO_SE(MCHECK_NE(val, dnan));
  }

  std::cout << "Note: This test produces a lot of output lines starting with [CHECK FAILED] but "
               "that is ok, because this test tests the MCHECK* macros."
            << std::endl;
}
