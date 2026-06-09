/**
 * @file fifo_demo.c
 * @brief 演示 ufifo 全部公开 API 的单线程与多线程用法
 *
 * 覆盖接口：
 *   ufifo_create / ufifo_destroy
 *   ufifo_size / ufifo_len / ufifo_avail / ufifo_is_empty / ufifo_is_full
 *   ufifo_in / ufifo_out / ufifo_peek
 *   ufifo_put / ufifo_get
 *
 * 编译运行：make fifo && out/bin/fifo
 */

#define _GNU_SOURCE

#include "fifo.h"

#include <pthread.h>
#include <sched.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "atomic.h"
#include "utils.h"

enum {
    FIFO_DEMO_STATIC_SIZE = 4,
    FIFO_DEMO_THREAD_SIZE = 8,
    FIFO_DEMO_ITEM_COUNT = 64,
};

typedef struct fifo_demo_thread_ctx {
    ufifo_t     *fifo;
    atomic_t      stop;
    int           produced_count;
    int           consumed_count;
    int           error;
} fifo_demo_thread_ctx_t;

static int fifo_demo_check(bool cond, const char *msg)
{
    if (!cond) {
        LogError("%s", msg);
        return -1;
    }

    return 0;
}

static int fifo_demo_expect_int(const char *name, int actual, int expected)
{
    if (actual != expected) {
        LogError("%s mismatch: actual=%d expected=%d", name, actual, expected);
        return -1;
    }

    return 0;
}

static int fifo_demo_run_single_thread_api(void)
{
    ufifo_t     *fifo = NULL;
    int          input[FIFO_DEMO_STATIC_SIZE] = {10, 20, 30, 40};
    int          peeked[2] = {0};
    int          output[2] = {0};
    int          value = 0;

    LogInfo("=== [1] single-thread API coverage ===");

    fifo = ufifo_create(FIFO_DEMO_STATIC_SIZE, sizeof(input[0]));
    if (fifo == NULL) {
        LogError("ufifo_create failed");
        return -1;
    }

    if (fifo_demo_expect_int("ufifo_size", (int)ufifo_size(fifo), FIFO_DEMO_STATIC_SIZE) != 0) {
        goto out_destroy;
    }
    if (fifo_demo_expect_int("ufifo_len(empty)", (int)ufifo_len(fifo), 0) != 0) {
        goto out_destroy;
    }
    if (fifo_demo_expect_int("ufifo_avail(empty)", (int)ufifo_avail(fifo), FIFO_DEMO_STATIC_SIZE) != 0) {
        goto out_destroy;
    }
    if (fifo_demo_check(ufifo_is_empty(fifo), "ufifo_is_empty should be true") != 0) {
        goto out_destroy;
    }
    if (fifo_demo_check(!ufifo_is_full(fifo), "ufifo_is_full should be false") != 0) {
        goto out_destroy;
    }

    if (fifo_demo_expect_int("ufifo_in", (int)ufifo_in(fifo, input, 3), 3) != 0) {
        goto out_destroy;
    }
    if (fifo_demo_expect_int("ufifo_len(after in)", (int)ufifo_len(fifo), 3) != 0) {
        goto out_destroy;
    }
    if (fifo_demo_expect_int("ufifo_avail(after in)", (int)ufifo_avail(fifo), 1) != 0) {
        goto out_destroy;
    }

    if (fifo_demo_expect_int("ufifo_peek", (int)ufifo_peek(fifo, peeked, 2), 2) != 0) {
        goto out_destroy;
    }
    if (fifo_demo_expect_int("peeked[0]", peeked[0], 10) != 0) {
        goto out_destroy;
    }
    if (fifo_demo_expect_int("peeked[1]", peeked[1], 20) != 0) {
        goto out_destroy;
    }
    if (fifo_demo_expect_int("ufifo_len(after peek)", (int)ufifo_len(fifo), 3) != 0) {
        goto out_destroy;
    }

    if (fifo_demo_expect_int("ufifo_out", (int)ufifo_out(fifo, output, 2), 2) != 0) {
        goto out_destroy;
    }
    if (fifo_demo_expect_int("output[0]", output[0], 10) != 0) {
        goto out_destroy;
    }
    if (fifo_demo_expect_int("output[1]", output[1], 20) != 0) {
        goto out_destroy;
    }

    value = 50;
    if (fifo_demo_expect_int("ufifo_put", (int)ufifo_put(fifo, &value), 1) != 0) {
        goto out_destroy;
    }
    if (fifo_demo_expect_int("ufifo_put fill", (int)ufifo_put(fifo, &input[3]), 1) != 0) {
        goto out_destroy;
    }
    value = 60;
    if (fifo_demo_expect_int("ufifo_put full", (int)ufifo_put(fifo, &value), 1) != 0) {
        goto out_destroy;
    }
    if (fifo_demo_check(ufifo_is_full(fifo), "ufifo_is_full should be true") != 0) {
        goto out_destroy;
    }

    if (fifo_demo_expect_int("ufifo_get", (int)ufifo_get(fifo, &value), 1) != 0) {
        goto out_destroy;
    }
    if (fifo_demo_expect_int("ufifo_get value", value, 30) != 0) {
        goto out_destroy;
    }

    ufifo_destroy(fifo);
    LogInfo("single-thread API coverage passed");
    return 0;

out_destroy:
    ufifo_destroy(fifo);
    return -1;
}

static void *fifo_demo_producer(void *arg)
{
    fifo_demo_thread_ctx_t *ctx = NULL;
    int                     value = 0;
    int                     i = 0;

    ctx = (fifo_demo_thread_ctx_t *)arg;
    if (ctx == NULL || ctx->fifo == NULL) {
        LogError("producer argument is invalid");
        return NULL;
    }

    for (i = 0; i < FIFO_DEMO_ITEM_COUNT; i++) {
        value = i + 1;
        while (ufifo_put(ctx->fifo, &value) != 1) {
            if (atomic_read_acquire(&ctx->stop)) {
                LogError("producer stopped before completion");
                return NULL;
            }
            sched_yield();
        }
        ctx->produced_count++;
    }

    LogInfo("producer done: produced=%d", ctx->produced_count);
    return NULL;
}

static void *fifo_demo_consumer(void *arg)
{
    fifo_demo_thread_ctx_t *ctx = NULL;
    int                     value = 0;
    int                     expected = 0;

    ctx = (fifo_demo_thread_ctx_t *)arg;
    if (ctx == NULL || ctx->fifo == NULL) {
        LogError("consumer argument is invalid");
        return NULL;
    }

    for (expected = 1; expected <= FIFO_DEMO_ITEM_COUNT; expected++) {
        while (ufifo_get(ctx->fifo, &value) != 1) {
            if (atomic_read_acquire(&ctx->stop)) {
                LogError("consumer stopped before completion");
                return NULL;
            }
            sched_yield();
        }
        if (value != expected) {
            LogError("consumer value mismatch: actual=%d expected=%d", value, expected);
            ctx->error = -1;
            atomic_set_release(&ctx->stop, 1);
            return NULL;
        }
        ctx->consumed_count++;
    }

    LogInfo("consumer done: consumed=%d", ctx->consumed_count);
    return NULL;
}

static int fifo_demo_join_thread(pthread_t thread, const char *name)
{
    int err = 0;

    err = pthread_join(thread, NULL);
    if (err != 0) {
        LogError("pthread_join(%s) failed: %s", name, strerror(err));
        return -1;
    }

    return 0;
}

static int fifo_demo_run_thread_api(void)
{
    ufifo_t                *fifo = NULL;
    fifo_demo_thread_ctx_t  ctx;
    pthread_t               producer;
    pthread_t               consumer;
    int                     err = 0;
    int                     producer_created = 0;
    int                     consumer_created = 0;
    int                     rc = -1;

    memset(&ctx, 0, sizeof(ctx));
    atomic_set(&ctx.stop, 0);

    LogInfo("=== [2] pthread SPSC API coverage ===");

    fifo = ufifo_create(FIFO_DEMO_THREAD_SIZE, sizeof(int));
    if (fifo == NULL) {
        LogError("ufifo_create failed");
        return -1;
    }
    ctx.fifo = fifo;

    err = pthread_create(&consumer, NULL, fifo_demo_consumer, &ctx);
    if (err != 0) {
        LogError("pthread_create(consumer) failed: %s", strerror(err));
        goto out_free_fifo;
    }
    consumer_created = 1;

    err = pthread_create(&producer, NULL, fifo_demo_producer, &ctx);
    if (err != 0) {
        LogError("pthread_create(producer) failed: %s", strerror(err));
        atomic_set_release(&ctx.stop, 1);
        goto out_join_consumer;
    }
    producer_created = 1;

    if (fifo_demo_join_thread(producer, "producer") != 0) {
        atomic_set_release(&ctx.stop, 1);
        goto out_join_consumer;
    }
    producer_created = 0;

    if (fifo_demo_join_thread(consumer, "consumer") != 0) {
        goto out_free_fifo;
    }
    consumer_created = 0;

    if (ctx.error != 0) {
        LogError("thread demo reported data error");
        goto out_free_fifo;
    }
    if (fifo_demo_expect_int("produced_count", ctx.produced_count, FIFO_DEMO_ITEM_COUNT) != 0) {
        goto out_free_fifo;
    }
    if (fifo_demo_expect_int("consumed_count", ctx.consumed_count, FIFO_DEMO_ITEM_COUNT) != 0) {
        goto out_free_fifo;
    }
    if (fifo_demo_check(ufifo_is_empty(fifo), "thread fifo should be empty") != 0) {
        goto out_free_fifo;
    }

    LogInfo("pthread SPSC coverage passed");
    rc = 0;

out_join_consumer:
    if (producer_created) {
        (void)pthread_join(producer, NULL);
    }
    if (consumer_created) {
        (void)pthread_join(consumer, NULL);
    }
out_free_fifo:
    ufifo_destroy(fifo);
    return rc;
}

int main(void)
{
    if (fifo_demo_run_single_thread_api() != 0) {
        return EXIT_FAILURE;
    }
    if (fifo_demo_run_thread_api() != 0) {
        return EXIT_FAILURE;
    }

    LogInfo("fifo demo passed");
    return EXIT_SUCCESS;
}