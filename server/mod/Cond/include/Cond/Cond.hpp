#pragma once
#include <pthread.h>

class Cond {
   public:
    Cond() { pthread_cond_init(&cond_, nullptr); }
    ~Cond() { pthread_cond_destroy(&cond_); }

    void wait(pthread_mutex_t& mutex) { pthread_cond_wait(&cond_, &mutex); }
    void notify_one() { pthread_cond_signal(&cond_); }
    void notify_all() { pthread_cond_broadcast(&cond_); }

   private:
    pthread_cond_t cond_;
};