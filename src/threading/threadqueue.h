#ifndef F264_THREADQUEUE_H_
#define F264_THREADQUEUE_H_

/**
 * \file threadqueue.h
 * \brief Multithreading job queue and threadpool architecture (Kvazaar-compatible).
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct threadqueue_job_t threadqueue_job_t;
typedef struct threadqueue_queue_t threadqueue_queue_t;

/**
 * \brief Initialize threadqueue with the specified number of worker threads.
 * \param thread_count Number of worker threads (0 or 1 runs synchronously or minimal).
 * \return Pointer to the allocated threadqueue.
 */
threadqueue_queue_t * f264_threadqueue_init(int thread_count);

/**
 * \brief Create a new job with a function pointer and argument.
 */
threadqueue_job_t * f264_threadqueue_job_create(void (*fptr)(void *arg), void *arg);

/**
 * \brief Submit a job to the threadqueue.
 * If all dependencies are satisfied, it is queued for immediate execution.
 */
int f264_threadqueue_submit(threadqueue_queue_t *tq, threadqueue_job_t *job);

/**
 * \brief Add a dependency: `job` cannot start until `dependency` is DONE.
 */
int f264_threadqueue_job_dep_add(threadqueue_job_t *job, threadqueue_job_t *dependency);

/**
 * \brief Increment reference count of a job.
 */
threadqueue_job_t * f264_threadqueue_copy_ref(threadqueue_job_t *job);

/**
 * \brief Decrement reference count and free job if zero.
 */
void f264_threadqueue_free_job(threadqueue_job_t **job_ptr);

/**
 * \brief Wait until the specified job has completed execution.
 */
int f264_threadqueue_waitfor(threadqueue_queue_t *tq, threadqueue_job_t *job);

/**
 * \brief Stop all workers in the threadqueue.
 */
int f264_threadqueue_stop(threadqueue_queue_t *tq);

/**
 * \brief Free and destroy the threadqueue.
 */
void f264_threadqueue_free(threadqueue_queue_t *tq);

#ifdef __cplusplus
}
#endif

#endif // F264_THREADQUEUE_H_
