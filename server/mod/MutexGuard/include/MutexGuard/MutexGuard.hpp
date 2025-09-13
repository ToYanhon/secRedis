#pragma once
#include <pthread.h>

class MutexGuard {
   public:
    MutexGuard(pthread_mutex_t& mutex) : mutex_(mutex) {
        pthread_mutex_lock(&mutex_);
    }
    ~MutexGuard() { pthread_mutex_unlock(&mutex_); }

    MutexGuard(const MutexGuard&) = delete;
    MutexGuard& operator=(const MutexGuard&) = delete;
    MutexGuard(MutexGuard&&) = delete;
    MutexGuard& operator=(MutexGuard&&) = delete;

   private:
    pthread_mutex_t& mutex_;
};