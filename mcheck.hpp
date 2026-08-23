#ifndef MCHECK_HPP
#define MCHECK_HPP

#include <cmath>
#include <cstdlib>
#include <iostream>

namespace detail {

// Helper class that outputs the error context and message to std::cout
class CheckLogger {
 public:
  CheckLogger(const char *file, int line, const char *condition_str)
  {
    std::cout << "[CHECK FAILED] " << file << ":" << line << " - (" << condition_str << ") ";
  }

  // Destructor fires at the end of the full-expression statement (after all << operations)
  ~CheckLogger()
  {
    std::cout << std::endl;
    // Optionally terminate on failure like standard Abseil CHECKs:
    // std::abort();
  }

  // Overload operator<< to accept any streamable type
  template<typename T> CheckLogger &operator<<(const T &val)
  {
    std::cout << val;
    return *this;
  }
};

// Helper struct to handle the stream expression when condition is TRUE
struct VoidStreamer {
  template<typename T> void operator&(const T &) const {}
};

// Equality & Comparison Helper to print values when an assertion fails
template<typename T1, typename T2> class BinaryCheckLogger {
 public:
  BinaryCheckLogger(const char *file, int line, const char *expr, const T1 &v1, const T2 &v2)
      : logger_(file, line, expr)
  {
    logger_ << " (evaluated as '" << v1 << "' vs '" << v2 << "') ";
  }

  template<typename T> BinaryCheckLogger &operator<<(const T &val)
  {
    logger_ << val;
    return *this;
  }

 private:
  CheckLogger logger_;
};

}  // namespace detail

// Primary Condition Macro
#define MCHECK(condition) \
  (condition) ? (void)0 : \
                ::detail::VoidStreamer() & ::detail::CheckLogger(__FILE__, __LINE__, #condition)

// Pointer Nullness Check
#define MCHECK_NOT_NULL(ptr) \
  ((ptr) != nullptr) ? \
      (void)0 : \
      ::detail::VoidStreamer() & ::detail::CheckLogger(__FILE__, __LINE__, #ptr " != nullptr")

// Comparison Macros
#define MCHECK_BINARY(val1, val2, op) \
  ((val1)op(val2)) ? \
      (void)0 : \
      ::detail::VoidStreamer() & ::detail::BinaryCheckLogger( \
                                     __FILE__, __LINE__, #val1 " " #op " " #val2, (val1), (val2))

#define MCHECK_EQ(val1, val2) MCHECK_BINARY(val1, val2, ==)
#define MCHECK_NE(val1, val2) MCHECK_BINARY(val1, val2, !=)
#define MCHECK_LT(val1, val2) MCHECK_BINARY(val1, val2, <)
#define MCHECK_LE(val1, val2) MCHECK_BINARY(val1, val2, <=)
#define MCHECK_GT(val1, val2) MCHECK_BINARY(val1, val2, >)
#define MCHECK_GE(val1, val2) MCHECK_BINARY(val1, val2, >=)

// Floating-point Near Check
#define MCHECK_NEAR(val1, val2, margin) \
  (val1 <= val2 + margin && val2 <= val1 + margin) ? \
      (void)0 : \
      ::detail::VoidStreamer() & \
          ::detail::CheckLogger(__FILE__, __LINE__, "|" #val1 " - " #val2 "| <= " #margin)

#endif  // MCHECK_HPP
