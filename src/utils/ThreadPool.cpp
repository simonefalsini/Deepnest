#include "deepnest/ThreadPool.h"

namespace deepnest {

ThreadPool::ThreadPool(int thread_count) : stop_(false) {
  for (int i = 0; i < thread_count; ++i) {
    workers_.emplace_back(&ThreadPool::WorkerLoop, this);
  }
}

ThreadPool::~ThreadPool() {
  {
    boost::unique_lock<boost::mutex> lock(mutex_);
    stop_ = true;
  }
  condition_.notify_all();
  for (auto& worker : workers_) {
    worker.join();
  }
}

void ThreadPool::Enqueue(const std::function<void()>& task) {
  {
    boost::unique_lock<boost::mutex> lock(mutex_);
    tasks_.push(task);
  }
  condition_.notify_one();
}

void ThreadPool::WorkerLoop() {
  while (true) {
    std::function<void()> task;
    {
      boost::unique_lock<boost::mutex> lock(mutex_);
      condition_.wait(lock, [this] { return stop_ || !tasks_.empty(); });
      if (stop_ && tasks_.empty()) {
        return;
      }
      task = tasks_.front();
      tasks_.pop();
    }
    task();
  }
}

}  // namespace deepnest
