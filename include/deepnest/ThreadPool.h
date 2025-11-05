#pragma once

#include <boost/thread.hpp>
#include <boost/thread/future.hpp>

#include <functional>
#include <memory>
#include <queue>
#include <stdexcept>
#include <type_traits>
#include <vector>

namespace deepnest {

class ThreadPool {
 public:
  explicit ThreadPool(int thread_count = 1);
  ~ThreadPool();

  void Enqueue(const std::function<void()>& task);

  template <typename Function>
  auto Submit(Function&& task)
      -> boost::unique_future<std::invoke_result_t<Function>>;

 private:
  void WorkerLoop();

  boost::mutex mutex_;
  boost::condition_variable condition_;
  bool stop_;
  std::queue<std::function<void()>> tasks_;
  std::vector<boost::thread> workers_;
};

}  // namespace deepnest

namespace deepnest {

template <typename Function>
auto ThreadPool::Submit(Function&& task)
    -> boost::unique_future<std::invoke_result_t<Function>> {
  using ResultType = std::invoke_result_t<Function>;

  auto packaged_task = std::make_shared<boost::packaged_task<ResultType()>>(
      std::forward<Function>(task));
  boost::unique_future<ResultType> future = packaged_task->get_future();

  {
    boost::unique_lock<boost::mutex> lock(mutex_);
    if (stop_) {
      throw std::runtime_error("ThreadPool is shutting down");
    }
    tasks_.emplace([packaged_task]() { (*packaged_task)(); });
  }

  condition_.notify_one();
  return future;
}

}  // namespace deepnest
