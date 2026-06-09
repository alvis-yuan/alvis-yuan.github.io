/**
 * @file wq_demo.c
 * @brief 演示 wq 全部公开 API 的多线程用法
 *
 * 覆盖接口：
 *   WQ_ENTRY / WQ_INIT_WORK / WQ_DECLARE_WORK
 *   wq_create / wq_queue_work / wq_flush / wq_cancel_work_sync / wq_destroy
 *
 * 编译运行：make wq && out/bin/wq
 */

#define _GNU_SOURCE

#include "wq.h"

#include <pthread.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "raii.h"

enum {
    DEMO_PRODUCER_COUNT   = 4,
    DEMO_TASKS_PER_THREAD = 8,
    DEMO_WORKER_COUNT     = 4,
};

/** 慢任务 sleep 时长（微秒），给 cancel 演示留出窗口 */
enum {
    DEMO_SLOW_SLEEP_US       = 300000,
    DEMO_CANCEL_WINDOW_US    = 20000,
    DEMO_RUNNING_PICKUP_US   = 50000,
};

/** 已完成工作项计数（多 worker 并发更新） */
static atomic_t g_completed_count;

/** 演示任务：wq_work 必须以值嵌入，回调里用 WQ_ENTRY 反推 */
typedef struct demo_task {
    wq_work_t work;
    int       id;
    int       sleep_us;
} demo_task_t;

typedef struct producer_arg {
    struct wq   *wq;
    demo_task_t *tasks;
    int          thread_idx;
} producer_arg_t;

static void demo_static_handler(wq_work_t *work);

WQ_DECLARE_WORK(g_static_work, demo_static_handler);

/** demo 进度输出（stdout 演示轨迹，非 LogError 日志通道） */
static void
demo_trace(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    (void)vprintf(fmt, ap);
    va_end(ap);
}

static void
demo_task_handler(wq_work_t *work)
{
    demo_task_t *task = NULL;
    uintptr_t    tid = 0;

    task = WQ_ENTRY(work, demo_task_t, work);
    tid  = (uintptr_t)pthread_self();

    if (task->sleep_us > 0) {
        usleep((useconds_t)task->sleep_us);
    }

    atomic_add(1, &g_completed_count);
    demo_trace("  [handler] task #%d done on thread %ju\n",
                task->id, (uintmax_t)tid);
}

static void
demo_static_handler(wq_work_t *work)
{
    (void)work;
    atomic_add(1, &g_completed_count);
    demo_trace("  [handler] WQ_DECLARE_WORK static item done\n");
}

static void
demo_slow_handler(wq_work_t *work)
{
    demo_task_t *task = NULL;

    task = WQ_ENTRY(work, demo_task_t, work);
    demo_trace("  [handler] slow task #%d started (sleep %dms)\n",
               task->id, DEMO_SLOW_SLEEP_US / 1000);
    usleep((useconds_t)DEMO_SLOW_SLEEP_US);
    atomic_add(1, &g_completed_count);
    demo_trace("  [handler] slow task #%d finished\n", task->id);
}

static void *
producer_thread(void *arg)
{
    producer_arg_t *ctx = NULL;
    int             i = 0;
    int             base_id = 0;

    ctx = (producer_arg_t *)arg;
    if (ctx == NULL) {
        LogError("producer_thread: arg is NULL");
        return NULL;
    }

    base_id = ctx->thread_idx * DEMO_TASKS_PER_THREAD;
    demo_trace("[producer-%d] start (tid=%ju)\n",
               ctx->thread_idx, (uintmax_t)(uintptr_t)pthread_self());

    for (i = 0; i < DEMO_TASKS_PER_THREAD; i++) {
        demo_task_t *task = NULL;
        bool         queued = false;
        bool         dup = false;

        task = &ctx->tasks[i];
        task->id       = base_id + i;
        task->sleep_us = 1000 + (task->id % 5) * 500;

        WQ_INIT_WORK(&task->work, demo_task_handler);

        queued = wq_queue_work(ctx->wq, &task->work);
        demo_trace("[producer-%d] wq_queue_work task #%d -> %s\n",
                   ctx->thread_idx, task->id, queued ? "queued" : "rejected");

        /* 同一 work 重复入队应被拒绝（内核 queue_work 幂等语义） */
        dup = wq_queue_work(ctx->wq, &task->work);
        demo_trace("[producer-%d] wq_queue_work duplicate task #%d -> %s (expect rejected)\n",
                   ctx->thread_idx, task->id, dup ? "queued" : "rejected");
    }

    demo_trace("[producer-%d] done\n", ctx->thread_idx);
    return NULL;
}

static int
demo_join_producers(pthread_t producers[], int count)
{
    int i = 0;
    int rc = 0;

    for (i = 0; i < count; i++) {
        int err = pthread_join(producers[i], NULL);

        if (err != 0) {
            LogError("pthread_join producer[%d] failed: %s", i, strerror(err));
            rc = -1;
        }
    }
    return rc;
}

static int
demo_run_concurrent_producers(void)
{
    struct wq      *wq = NULL;
    demo_task_t    *tasks = NULL;
    producer_arg_t  args[DEMO_PRODUCER_COUNT];
    pthread_t       producers[DEMO_PRODUCER_COUNT];
    int             i = 0;
    int             spawned = 0;
    int             total_tasks = 0;
    int             err = 0;

    demo_trace("\n=== [1] wq_create + multi-producer wq_queue_work ===\n");

    wq = wq_create("demo-wq", DEMO_WORKER_COUNT);
    if (wq == NULL) {
        LogError("wq_create(demo-wq) failed");
        return -1;
    }
    demo_trace("wq_create(\"demo-wq\", %d) ok\n", DEMO_WORKER_COUNT);

    total_tasks = DEMO_PRODUCER_COUNT * DEMO_TASKS_PER_THREAD;
    tasks = (demo_task_t *)calloc((size_t)total_tasks, sizeof(*tasks));
    if (tasks == NULL) {
        LogError("calloc(%zu) failed for demo tasks", sizeof(*tasks) * (size_t)total_tasks);
        wq_destroy(wq);
        return -1;
    }

    for (i = 0; i < DEMO_PRODUCER_COUNT; i++) {
        args[i].wq         = wq;
        args[i].thread_idx = i;
        args[i].tasks      = &tasks[i * DEMO_TASKS_PER_THREAD];

        err = pthread_create(&producers[i], NULL, producer_thread, &args[i]);
        if (err != 0) {
            int j = 0;

            LogError("pthread_create producer[%d] failed: %s", i, strerror(err));
            for (j = 0; j < spawned; j++) {
                (void)pthread_join(producers[j], NULL);
            }
            wq_destroy(wq);
            free(tasks);
            tasks = NULL;
            return -1;
        }
        spawned++;
    }

    if (demo_join_producers(producers, DEMO_PRODUCER_COUNT) != 0) {
        wq_destroy(wq);
        free(tasks);
        return -1;
    }

    demo_trace("\n=== [2] WQ_DECLARE_WORK + wq_queue_work ===\n");
    if (!wq_queue_work(wq, &g_static_work)) {
        LogError("wq_queue_work(g_static_work) rejected");
        wq_destroy(wq);
        free(tasks);
        return -1;
    }
    demo_trace("wq_queue_work(g_static_work) ok\n");

    demo_trace("\n=== [3] wq_flush ===\n");
    wq_flush(wq);
    demo_trace("wq_flush done, completed=%d (expect %d + 1 static)\n",
               atomic_read(&g_completed_count), total_tasks);

    demo_trace("\n=== [4] wq_destroy ===\n");
    wq_destroy(wq);
    wq = NULL;
    demo_trace("wq_destroy ok\n");

    wq_flush(NULL);
    wq_destroy(NULL);
    demo_trace("wq_flush(NULL) / wq_destroy(NULL) ok (NULL-safe)\n");

    free(tasks);
    tasks = NULL;
    return 0;
}

static int
demo_run_cancel(void)
{
    struct wq   *wq = NULL;
    demo_task_t  blocker;
    demo_task_t  victim;
    demo_task_t  solo;
    bool         cancelled = false;

    demo_trace("\n=== [5] wq_cancel_work_sync ===\n");

    /* 5a：单 worker 下，队首慢任务占用 worker 时取消队中 victim */
    wq = wq_create("cancel-wq", 1);
    if (wq == NULL) {
        LogError("wq_create(cancel-wq) failed");
        return -1;
    }

    blocker.id       = 100;
    blocker.sleep_us = DEMO_SLOW_SLEEP_US;
    WQ_INIT_WORK(&blocker.work, demo_slow_handler);

    victim.id       = 101;
    victim.sleep_us = 0;
    WQ_INIT_WORK(&victim.work, demo_task_handler);

    (void)wq_queue_work(wq, &blocker.work);
    (void)wq_queue_work(wq, &victim.work);
    demo_trace("[5a] queued slow#100 then victim#101 (single worker, FIFO)\n");

    usleep((useconds_t)DEMO_CANCEL_WINDOW_US);
    cancelled = wq_cancel_work_sync(wq, &victim.work);
    demo_trace("[5a] wq_cancel_work_sync(victim#101) -> %s\n",
               cancelled ? "cancelled" : "not cancelled");
    wq_flush(wq);
    wq_destroy(wq);
    wq = NULL;

    /* 5b：已在执行的工作项无法从队列移除，cancel 返回 false 并 sync 等待 */
    wq = wq_create("cancel-run-wq", 1);
    if (wq == NULL) {
        LogError("wq_create(cancel-run-wq) failed");
        return -1;
    }

    solo.id       = 200;
    solo.sleep_us = DEMO_SLOW_SLEEP_US;
    WQ_INIT_WORK(&solo.work, demo_slow_handler);

    (void)wq_queue_work(wq, &solo.work);
    demo_trace("[5b] queued slow#200, wait until worker picks it up\n");
    usleep((useconds_t)DEMO_RUNNING_PICKUP_US);

    cancelled = wq_cancel_work_sync(wq, &solo.work);
    demo_trace("[5b] wq_cancel_work_sync(slow#200 running) -> %s (expect not cancelled)\n",
               cancelled ? "cancelled" : "not cancelled");

    wq_destroy(wq);
    return 0;
}

static int
demo_run_create_invalid(void)
{
    struct wq *wq = NULL;

    demo_trace("\n=== [6] wq_create parameter validation ===\n");

    wq = wq_create("bad-wq", 0);
    if (wq == NULL) {
        demo_trace("wq_create(nr_workers=0) -> NULL (expected)\n");
    } else {
        LogError("wq_create(nr_workers=0) should fail");
        wq_destroy(wq);
        return -1;
    }

    wq = wq_create(NULL, 1);
    if (wq == NULL) {
        LogError("wq_create(NULL name, 1) failed unexpectedly");
        return -1;
    }
    demo_trace("wq_create(NULL name, 1) ok (default name \"wq\")\n");
    wq_destroy(wq);
    return 0;
}

int
main(void)
{
    int rc = 0;

    atomic_set(&g_completed_count, 0);

    demo_trace("wq demo — cover all APIs in wq.h\n");
    (void)fflush(stdout);

    rc = demo_run_create_invalid();
    if (rc != 0) {
        return EXIT_FAILURE;
    }

    rc = demo_run_concurrent_producers();
    if (rc != 0) {
        return EXIT_FAILURE;
    }

    rc = demo_run_cancel();
    if (rc != 0) {
        return EXIT_FAILURE;
    }

    demo_trace("\nAll demos finished.\n");
    return EXIT_SUCCESS;
}
