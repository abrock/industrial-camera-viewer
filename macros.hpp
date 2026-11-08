#ifndef MACROS_HPP
#define MACROS_HPP

#include "misc.h"

#define EXEC_AND_CHECK(code) \
  error = nullptr; \
  println("Trying command {} from {}: {}", #code, __FILE__, __LINE__); \
  code; \
  if (nullptr != error) { \
    Misc::println("Command: {}\nError message: {}", #code, error->message); \
  }

#define DEBUG(var) #var << var << std::endl

#endif  // MACROS_HPP
