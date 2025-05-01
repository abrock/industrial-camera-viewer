#ifndef MACROS_HPP
#define MACROS_HPP

#include "misc.h"

#define EXEC_AND_CHECK(code) \
  error = nullptr; \
  code; \
  if (nullptr != error) { \
    Misc::println("Command: {}\nError message: {}", #code, error->message); \
  }

#endif  // MACROS_HPP
