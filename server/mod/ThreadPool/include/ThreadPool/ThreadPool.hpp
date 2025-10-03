#pragma once
#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>

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

  std::thread *threads;
  int num_threads;
  std::atomic<bool> stop_flag;
  std::mutex queue_mutex;
  std::condition_variable condition_var;

  struct TaskNode {
    std::function<void()> task;
    void *res;
    TaskNode *next;
  };

  TaskNode *task_queue_head;
  TaskNode *task_queue_tail;
};

} // namespace yanhon
