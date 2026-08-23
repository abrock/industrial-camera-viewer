#include "time.h"

#include <chrono>

int Time::hour()
{
  auto now = std::chrono::system_clock::now();
  auto zt = std::chrono::zoned_time{std::chrono::current_zone(), now};
  auto local_time = zt.get_local_time();

  // Extract the time-of-day duration since midnight
  auto tod = local_time - std::chrono::floor<std::chrono::days>(local_time);

  // Cast to hours (24-hour format: 0–23)
  return std::chrono::duration_cast<std::chrono::hours>(tod).count();
}

int Time::minute()
{
  auto now = std::chrono::system_clock::now();
  auto zt = std::chrono::zoned_time{std::chrono::current_zone(), now};
  auto local_time = zt.get_local_time();

  auto tod = local_time - std::chrono::floor<std::chrono::days>(local_time);

  // Extract minutes modulo hours (0–59)
  return std::chrono::duration_cast<std::chrono::minutes>(tod % std::chrono::hours(1)).count();
}

int Time::second()
{
  auto now = std::chrono::system_clock::now();
  auto zt = std::chrono::zoned_time{std::chrono::current_zone(), now};
  auto local_time = zt.get_local_time();

  auto tod = local_time - std::chrono::floor<std::chrono::days>(local_time);

  // Extract seconds modulo minutes (0–59)
  return std::chrono::duration_cast<std::chrono::seconds>(tod % std::chrono::minutes(1)).count();
}
