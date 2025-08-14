#pragma once

#include <deque>
#include <memory>
#include <spdlog/spdlog.h>

template <class T> class MultipleThreadQueue {
  std::deque<std::unique_ptr<T>> tasks_;
  std::mutex mutex_;
  std::condition_variable cv_;

public:
  void push(std::unique_ptr<T> task) {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      tasks_.push_back(std::move(task));
    }
    cv_.notify_all();
  }

  void push_all(std::deque<std::unique_ptr<T>> tasks) {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      tasks_.insert(tasks_.end(), std::make_move_iterator(tasks.begin()),
                    std::make_move_iterator(tasks.end()));
    }
    cv_.notify_all();
  }

  std::unique_ptr<T> pop() {
    std::unique_lock<std::mutex> lock(mutex_);
    cv_.wait(lock, [this] { return !tasks_.empty(); });
    auto task = std::move(tasks_.front());
    tasks_.pop_front();
    return task;
  }
  std::unique_ptr<T> try_pop() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (tasks_.empty()) {
      return nullptr;
    }
    auto task = std::move(tasks_.front());
    tasks_.pop_front();
    return task;
  }
  std::deque<std::unique_ptr<T>> pop_all() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::deque<std::unique_ptr<T>> tasks;
    tasks.swap(tasks_);
    return tasks;
  }
};
