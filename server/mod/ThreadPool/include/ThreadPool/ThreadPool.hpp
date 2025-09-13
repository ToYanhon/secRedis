#pragma once
#include <pthread.h>

#include <atomic>
#include <functional>

#include "Cond/Cond.hpp"

namespace yanhon {

class ThreadPool {
public:
  ThreadPool(int num_threads);
  ~ThreadPool();

  void enqueue(std::function<void()> task);
  void stop();

  ThreadPool(const ThreadPool &tp) = delete;
  ThreadPool operator=(const ThreadPool &tp) = delete;

private:
  static void *worker(void *arg);

  pthread_t *threads;
  int num_threads;
  std::atomic<bool> stop_flag;
  pthread_mutex_t queue_mutex;
  Cond condition_var;

  struct TaskNode {
    std::function<void()> task;
    void *res;
    TaskNode *next;
  };

  TaskNode *task_queue_head;
  TaskNode *task_queue_tail;
};

} // namespace yanhon
