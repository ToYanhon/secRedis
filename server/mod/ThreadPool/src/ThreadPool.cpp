#include "ThreadPool/ThreadPool.hpp"

#include "MutexGuard/MutexGuard.hpp"

namespace yanhon {

ThreadPool::ThreadPool(int num_threads)
    : num_threads(num_threads), stop_flag(false), task_queue_head(nullptr),
      task_queue_tail(nullptr) {
  threads = new pthread_t[num_threads];
  pthread_mutex_init(&queue_mutex, nullptr);

  for (int i = 0; i < num_threads; ++i) {
    pthread_create(&threads[i], nullptr, worker, this);
  }
}

ThreadPool::~ThreadPool() { stop(); }

void ThreadPool::enqueue(std::function<void()> task) {
  if (stop_flag)
    return;

  TaskNode *new_task = new TaskNode{std::move(task), nullptr, nullptr};

  MutexGuard lock(queue_mutex);
  if (task_queue_tail) {
    task_queue_tail->next = new_task;
    task_queue_tail = new_task;
  } else {
    task_queue_head = task_queue_tail = new_task;
  }
  condition_var.notify_one();
}

void ThreadPool::stop() {
  stop_flag = true;
  condition_var.notify_all();

  for (int i = 0; i < num_threads; ++i) {
    pthread_join(threads[i], nullptr);
  }

  delete[] threads;

  while (task_queue_head) {
    TaskNode *temp = task_queue_head;
    task_queue_head = task_queue_head->next;
    delete temp;
  }
}

void *ThreadPool::worker(void *arg) {
  ThreadPool *pool = static_cast<ThreadPool *>(arg);

  while (true) {
    TaskNode *task_node;
    {
      MutexGuard lock(pool->queue_mutex);

      while (!pool->stop_flag && !pool->task_queue_head) {
        pool->condition_var.wait(pool->queue_mutex);
      }

      if (pool->stop_flag) {
        break;
      }

      task_node = pool->task_queue_head;
      if (task_node) {
        pool->task_queue_head = task_node->next;
        if (!pool->task_queue_head) {
          pool->task_queue_tail = nullptr;
        }
      }
    }

    if (task_node) {
      task_node->task();
      delete task_node;
    }
  }

  return nullptr;
}

} // namespace yanhon
