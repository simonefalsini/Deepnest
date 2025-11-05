#pragma once

#include <boost/thread.hpp>
#include <functional>
#include <queue>
#include <vector>

namespace deepnest {

class ThreadPool {
 public:
  explicit ThreadPool(int thread_count = 1);
  ~ThreadPool();

  void Enqueue(const std::function<void()>& task);

 private:
  void WorkerLoop();

  boost::mutex mutex_;
  boost::condition_variable condition_;
  bool stop_;
  std::queue<std::function<void()>> tasks_;
  std::vector<boost::thread> workers_;
};

}  // namespace deepnest
