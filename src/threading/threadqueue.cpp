/**
 * \file threadqueue.cpp
 * \brief Implementation of C++20 threadpool and job queue inspired by Kvazaar.
 */

#include "threadqueue.h"

#include <vector>
#include <deque>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <cassert>
#include <cstdlib>

typedef enum {
  THREADQUEUE_JOB_STATE_CREATED,
  THREADQUEUE_JOB_STATE_WAITING,
  THREADQUEUE_JOB_STATE_READY,
  THREADQUEUE_JOB_STATE_RUNNING,
  THREADQUEUE_JOB_STATE_DONE
} threadqueue_job_state;

struct threadqueue_job_t {
  std::mutex lock;
  std::condition_variable cv_done;

  threadqueue_job_state state{THREADQUEUE_JOB_STATE_CREATED};

  int ndepends{0};
  std::vector<threadqueue_job_t*> rdepends;

  std::atomic<int> refcount{1};

  void (*fptr)(void *arg){nullptr};
  void *arg{nullptr};

  threadqueue_queue_t *queue{nullptr};
};

struct threadqueue_queue_t {
  std::mutex lock;
  std::condition_variable cv_available;
  std::condition_variable cv_done;

  std::deque<threadqueue_job_t*> ready_jobs;
  std::vector<std::thread> workers;

  int thread_count{0};
  bool stop{false};
  int running_count{0};
};

static void worker_loop(threadqueue_queue_t *tq)
{
  while (true) {
    threadqueue_job_t *job = nullptr;

    {
      std::unique_lock<std::mutex> lock(tq->lock);
      tq->cv_available.wait(lock, [&]() {
        return tq->stop || !tq->ready_jobs.empty();
      });

      if (tq->stop && tq->ready_jobs.empty()) {
        break;
      }

      job = tq->ready_jobs.front();
      tq->ready_jobs.pop_front();
      tq->running_count++;
    }

    // Execute job
    {
      std::unique_lock<std::mutex> job_lock(job->lock);
      job->state = THREADQUEUE_JOB_STATE_RUNNING;
    }

    if (job->fptr) {
      job->fptr(job->arg);
    }

    // Mark done and notify reverse dependencies
    std::vector<threadqueue_job_t*> ready_rdepends;
    std::vector<threadqueue_job_t*> rdepends_to_release;
    {
      std::unique_lock<std::mutex> job_lock(job->lock);
      job->state = THREADQUEUE_JOB_STATE_DONE;
      job->cv_done.notify_all();

      for (auto *rdep : job->rdepends) {
        std::unique_lock<std::mutex> rdep_lock(rdep->lock);
        rdep->ndepends--;
        if (rdep->ndepends == 0 && rdep->state == THREADQUEUE_JOB_STATE_WAITING) {
          rdep->state = THREADQUEUE_JOB_STATE_READY;
          ready_rdepends.push_back(rdep);
        }
        rdepends_to_release.push_back(rdep);
      }
      job->rdepends.clear();
    }

    for (auto *rdep : rdepends_to_release) {
      f264_threadqueue_free_job(&rdep);
    }

    {
      std::unique_lock<std::mutex> lock(tq->lock);
      tq->running_count--;
      for (auto *ready_job : ready_rdepends) {
        tq->ready_jobs.push_back(ready_job);
      }
      if (!ready_rdepends.empty()) {
        if (ready_rdepends.size() > 1) {
          tq->cv_available.notify_all();
        } else {
          tq->cv_available.notify_one();
        }
      }
      tq->cv_done.notify_all();
    }

    // Release job reference from the queue execution
    f264_threadqueue_free_job(&job);
  }
}

extern "C" {

threadqueue_queue_t * f264_threadqueue_init(int thread_count)
{
  auto *tq = new threadqueue_queue_t();
  tq->thread_count = thread_count;
  tq->stop = false;
  tq->running_count = 0;

  if (thread_count > 0) {
    tq->workers.reserve(thread_count);
    for (int i = 0; i < thread_count; ++i) {
      tq->workers.emplace_back(worker_loop, tq);
    }
  }

  return tq;
}

threadqueue_job_t * f264_threadqueue_job_create(void (*fptr)(void *arg), void *arg)
{
  auto *job = new threadqueue_job_t();
  job->fptr = fptr;
  job->arg = arg;
  job->state = THREADQUEUE_JOB_STATE_CREATED;
  job->ndepends = 0;
  job->refcount = 1;
  job->queue = nullptr;
  return job;
}

int f264_threadqueue_job_dep_add(threadqueue_job_t *job, threadqueue_job_t *dependency)
{
  if (!job || !dependency) return 0;

  std::unique_lock<std::mutex> dep_lock(dependency->lock);
  std::unique_lock<std::mutex> job_lock(job->lock);

  if (dependency->state != THREADQUEUE_JOB_STATE_DONE) {
    job->ndepends++;
    dependency->rdepends.push_back(job);
    f264_threadqueue_copy_ref(job); // Hold reference until dependency finishes
  }

  return 1;
}

threadqueue_job_t * f264_threadqueue_copy_ref(threadqueue_job_t *job)
{
  if (job) {
    job->refcount.fetch_add(1, std::memory_order_relaxed);
  }
  return job;
}

void f264_threadqueue_free_job(threadqueue_job_t **job_ptr)
{
  if (!job_ptr || !*job_ptr) return;
  threadqueue_job_t *job = *job_ptr;
  *job_ptr = nullptr;

  if (job->refcount.fetch_sub(1, std::memory_order_acq_rel) == 1) {
    delete job;
  }
}

int f264_threadqueue_submit(threadqueue_queue_t *tq, threadqueue_job_t *job)
{
  if (!tq || !job) return 0;

  job->queue = tq;
  f264_threadqueue_copy_ref(job); // Queue takes a reference

  if (tq->thread_count <= 0) {
    // Synchronous execution mode if 0 threads
    job->state = THREADQUEUE_JOB_STATE_RUNNING;
    if (job->fptr) {
      job->fptr(job->arg);
    }
    job->state = THREADQUEUE_JOB_STATE_DONE;
    f264_threadqueue_free_job(&job);
    return 1;
  }

  bool is_ready = false;
  {
    std::unique_lock<std::mutex> job_lock(job->lock);
    if (job->ndepends == 0) {
      job->state = THREADQUEUE_JOB_STATE_READY;
      is_ready = true;
    } else {
      job->state = THREADQUEUE_JOB_STATE_WAITING;
    }
  }

  if (is_ready) {
    std::unique_lock<std::mutex> queue_lock(tq->lock);
    tq->ready_jobs.push_back(job);
    tq->cv_available.notify_one();
  }

  return 1;
}

int f264_threadqueue_waitfor(threadqueue_queue_t *tq, threadqueue_job_t *job)
{
  if (!job) return 0;

  std::unique_lock<std::mutex> job_lock(job->lock);
  job->cv_done.wait(job_lock, [&]() {
    return job->state == THREADQUEUE_JOB_STATE_DONE;
  });

  return 1;
}

int f264_threadqueue_job_is_done(threadqueue_job_t *job)
{
  if (!job) return 1;
  std::unique_lock<std::mutex> job_lock(job->lock);
  return job->state == THREADQUEUE_JOB_STATE_DONE ? 1 : 0;
}

int f264_threadqueue_stop(threadqueue_queue_t *tq)
{
  if (!tq) return 0;

  {
    std::unique_lock<std::mutex> lock(tq->lock);
    tq->stop = true;
  }
  tq->cv_available.notify_all();

  for (auto &worker : tq->workers) {
    if (worker.joinable()) {
      worker.join();
    }
  }
  tq->workers.clear();

  return 1;
}

void f264_threadqueue_free(threadqueue_queue_t *tq)
{
  if (!tq) return;

  f264_threadqueue_stop(tq);

  {
    std::unique_lock<std::mutex> lock(tq->lock);
    while (!tq->ready_jobs.empty()) {
      auto *job = tq->ready_jobs.front();
      tq->ready_jobs.pop_front();
      f264_threadqueue_free_job(&job);
    }
  }

  delete tq;
}

} // extern "C"
