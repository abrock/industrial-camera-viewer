#ifndef BLOCKING_QUEUE_HPP
#define BLOCKING_QUEUE_HPP

#include <mutex>
#include <queue>

template<class T> class BQueue {
 private:
  std::mutex mutex;
  std::queue<T> queue;

 public:
  T pop()
  {
    std::lock_guard guard(mutex);
    T value = queue.front();
    queue.pop();
    return value;
  }

  void push(T value)
  {
    std::lock_guard guard(mutex);
    queue.push(value);
  }

  bool empty()
  {
    std::lock_guard guard(mutex);
    return queue.empty();
  }

  size_t size()
  {
    std::lock_guard guard(mutex);
    return queue.size();
  }
};

#endif  // BLOCKING_QUEUE_HPP
