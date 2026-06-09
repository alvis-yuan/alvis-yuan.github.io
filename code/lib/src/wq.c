/**
 * @file wq.c
 * @brief 用户态工作队列实现
 */

#define _GNU_SOURCE

#include "wq.h"

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "raii.h"

/** worker 线程上下文 */
typedef struct wq_worker {
    pthread_t  tid;
    struct wq *wq;
} wq_worker_t;

/** 工作队列，对标 struct workqueue_struct */
struct wq {
    pthread_mutex_t lock;
    pthread_cond_t  work_avail;
    pthread_cond_t  work_done;
    struct list_head queue;
    int             nr_workers;
    int             nr_running;
    int             nr_queued;
    int             shutdown;
    wq_worker_t     workers[WQ_MAX_WORKERS];
    char            name[WQ_NAME_LEN];
};

static void *
wq_worker_thread(void *arg)
{
    wq_worker_t *self = (wq_worker_t *)arg;
    struct wq   *wq   = self->wq;
    wq_work_t   *work = NULL;

    pthread_mutex_lock(&wq->lock);
    while (1) {
        while (list_empty(&wq->queue) && !wq->shutdown) {
            pthread_cond_wait(&wq->work_avail, &wq->lock);
        }

        if (wq->shutdown && list_empty(&wq->queue)) {
            break;
        }

        work = list_first_entry(&wq->queue, wq_work_t, entry);
        list_del_init(&work->entry);
        work->on_queue = false;
        wq->nr_queued--;
        wq->nr_running++;
        pthread_mutex_unlock(&wq->lock);

        work->func(work); // 处理工作期间，不需要加锁

        pthread_mutex_lock(&wq->lock);
        atomic_set(&work->pending, 0);
        wq->nr_running--;
        if (wq->nr_running == 0 && list_empty(&wq->queue)) {
            pthread_cond_broadcast(&wq->work_done);
        }
    }
    pthread_mutex_unlock(&wq->lock);
    return NULL;
}

static void
wq_sync_destroy(struct wq *wq)
{
    if (wq == NULL) {
        return;
    }

    pthread_cond_destroy(&wq->work_done);
    pthread_cond_destroy(&wq->work_avail);
    pthread_mutex_destroy(&wq->lock);
    free(wq);
}

struct wq *
wq_create(const char *name, int nr_workers)
{
    struct wq *wq = NULL;
    int        i = 0;
    int        err = 0;

    if (nr_workers <= 0 || nr_workers > WQ_MAX_WORKERS) {
        LogError("invalid nr_workers: %d (valid: 1..%d)",
                 nr_workers, WQ_MAX_WORKERS);
        return NULL;
    }

    wq = (struct wq *)calloc(1, sizeof(*wq));
    if (wq == NULL) {
        LogError("calloc(%zu) failed", sizeof(*wq));
        return NULL;
    }

    err = pthread_mutex_init(&wq->lock, NULL);
    if (err != 0) {
        LogError("pthread_mutex_init failed: %s", strerror(err));
        free(wq);
        return NULL;
    }

    err = pthread_cond_init(&wq->work_avail, NULL);
    if (err != 0) {
        LogError("pthread_cond_init(work_avail) failed: %s", strerror(err));
        pthread_mutex_destroy(&wq->lock);
        free(wq);
        return NULL;
    }

    err = pthread_cond_init(&wq->work_done, NULL);
    if (err != 0) {
        LogError("pthread_cond_init(work_done) failed: %s", strerror(err));
        pthread_cond_destroy(&wq->work_avail);
        pthread_mutex_destroy(&wq->lock);
        free(wq);
        return NULL;
    }

    INIT_LIST_HEAD(&wq->queue);
    wq->nr_workers = nr_workers;
    wq->shutdown   = 0;

    if (name != NULL && name[0] != '\0') {
        (void)snprintf(wq->name, sizeof(wq->name), "%s", name);
    } else {
        (void)snprintf(wq->name, sizeof(wq->name), "%s", "wq");
    }

    for (i = 0; i < nr_workers; i++) {
        wq->workers[i].wq = wq;
        err = pthread_create(&wq->workers[i].tid, NULL,
                             wq_worker_thread, &wq->workers[i]);
        if (err != 0) {
            int j = 0;

            LogError("pthread_create worker[%d] failed: %s", i, strerror(err));
            pthread_mutex_lock(&wq->lock);
            wq->shutdown = 1;
            pthread_cond_broadcast(&wq->work_avail);
            pthread_mutex_unlock(&wq->lock);

            for (j = 0; j < i; j++) {
                (void)pthread_join(wq->workers[j].tid, NULL);
            }
            wq_sync_destroy(wq);
            return NULL;
        }
    }

    return wq;
}

bool
wq_queue_work(struct wq *wq, wq_work_t *work)
{
    if (wq == NULL || work == NULL) {
        LogError("invalid argument: wq=%p work=%p", (void *)wq, (void *)work);
        return false;
    }
    if (work->func == NULL) {
        LogError("work->func is NULL");
        return false;
    }

    pthread_mutex_lock(&wq->lock);
    if (atomic_read(&work->pending) != 0) {
        pthread_mutex_unlock(&wq->lock);
        return false;
    }

    atomic_set(&work->pending, 1);
    work->on_queue = true;
    list_add_tail(&work->entry, &wq->queue);
    wq->nr_queued++;
    pthread_cond_signal(&wq->work_avail);
    pthread_mutex_unlock(&wq->lock);
    return true;
}

void
wq_flush(struct wq *wq)
{
    if (wq == NULL) {
        LogError("invalid argument: wq=%p", (void *)wq);
        return;
    }

    pthread_mutex_lock(&wq->lock);
    while (wq->nr_running > 0 || !list_empty(&wq->queue)) {
        pthread_cond_wait(&wq->work_done, &wq->lock);
    }
    pthread_mutex_unlock(&wq->lock);
}

void
wq_destroy(struct wq *wq)
{
    int i = 0;

    if (wq == NULL) {
        return;
    }

    wq_flush(wq);

    pthread_mutex_lock(&wq->lock);
    wq->shutdown = 1;
    pthread_cond_broadcast(&wq->work_avail);
    pthread_mutex_unlock(&wq->lock);

    for (i = 0; i < wq->nr_workers; i++) {
        int join_err = pthread_join(wq->workers[i].tid, NULL);

        if (join_err != 0) {
            LogError("pthread_join worker[%d] failed: %s", i, strerror(join_err));
        }
    }

    wq_sync_destroy(wq);
}

bool
wq_cancel_work_sync(struct wq *wq, wq_work_t *work)
{
    bool cancelled = false;

    if (wq == NULL || work == NULL) {
        LogError("invalid argument: wq=%p work=%p", (void *)wq, (void *)work);
        return false;
    }

    pthread_mutex_lock(&wq->lock);
    if (work->on_queue) {
        list_del_init(&work->entry);
        wq->nr_queued--;
        work->on_queue = false;
        atomic_set(&work->pending, 0);
        cancelled = true;
    }
    pthread_mutex_unlock(&wq->lock);

    wq_flush(wq);
    return cancelled;
}
