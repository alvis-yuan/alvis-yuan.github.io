/**
 * @file timer_demo.c
 * @brief timer_heap 与 epoll_wait 集成演示 (含子线程所有权转移)
 */
#define _GNU_SOURCE

#include "timer.h"
#include "raii.h"

#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/signalfd.h>
#include <sys/eventfd.h>

enum {
    TIMER_DEMO_MAX_EVENTS     = 8,
    TIMER_DEMO_CONN1_DELAY_MS = 1500,
    TIMER_DEMO_CONN2_DELAY_MS = 4000,
    TIMER_DEMO_BUF_SIZE       = 4096,
    TIMER_TASK_Q_MAX          = 32,
};

#define TIMER_DEMO_MS_TO_NS(ms) ((uint64_t)(ms) * 1000000ULL)

typedef struct timer_demo_conn {
    int                     fd;
    char                    buf[TIMER_DEMO_BUF_SIZE];
    struct timer_node       timeout_timer;
    struct timer_node       keepalive_timer;
    struct timer_context   *timer_ctx;
    struct timer_demo_conn *peer;
} timer_demo_conn_t;

enum timer_task_type {
    TIMER_TASK_ADD,
    TIMER_TASK_CANCEL
};

typedef struct timer_task {
    enum timer_task_type type;
    struct timer_node   *node;
    uint64_t             delay_ns;
    void               (*cb)(struct timer_node *);
} timer_task_t;

typedef struct timer_task_queue {
    pthread_mutex_t lock;
    timer_task_t    tasks[TIMER_TASK_Q_MAX];
    int             head;
    int             tail;
} timer_task_queue_t;

typedef struct timer_demo_ctx {
    int                  epfd;
    int                  signal_fd;
    int                  event_fd;
    struct timer_context timer_ctx;
    timer_demo_conn_t    conn1;
    timer_demo_conn_t    conn2;
    timer_task_queue_t   task_queue;
    bool                 running;
    pthread_t            worker_tid;
} timer_demo_ctx_t;

static void
timer_demo_on_conn_timeout(struct timer_node *node)
{
    timer_demo_conn_t *conn = TIMER_ENTRY(node, timer_demo_conn_t, timeout_timer);
    LogInfo("connection fd=%d timeout, would close", conn->fd);

    if (conn->fd == 5 && conn->peer != NULL && conn->timer_ctx != NULL) {
        timer_cancel(conn->timer_ctx, &conn->peer->timeout_timer);
        LogInfo("cancelled pending timer on fd=%d", conn->peer->fd);
    }
}

static void
worker_timer_cb(struct timer_node *node)
{
    (void)node;
    LogInfo("worker_timer_cb: timer submitted from sub-thread just fired in main thread!");
}

static bool
enqueue_timer_task(timer_demo_ctx_t *ctx, enum timer_task_type type,
                   struct timer_node *node, uint64_t delay_ns,
                   void (*cb)(struct timer_node *))
{
    timer_task_queue_t *q = &ctx->task_queue;
    uint64_t val = 1;

    pthread_mutex_lock(&q->lock);
    if (q->tail - q->head >= TIMER_TASK_Q_MAX) {
        pthread_mutex_unlock(&q->lock);
        LogError("task queue full");
        return false;
    }
    timer_task_t *t = &q->tasks[q->tail % TIMER_TASK_Q_MAX];
    t->type     = type;
    t->node     = node;
    t->delay_ns = delay_ns;
    t->cb       = cb;
    q->tail++;
    pthread_mutex_unlock(&q->lock);

    if (write(ctx->event_fd, &val, sizeof(val)) != sizeof(val)) {
        LogError("eventfd write failed: %s", strerror(errno));
        return false;
    }
    return true;
}

static void
drain_task_queue(timer_demo_ctx_t *ctx)
{
    timer_task_queue_t *q = &ctx->task_queue;
    uint64_t val;
    int i;

    if (read(ctx->event_fd, &val, sizeof(val)) == -1 && errno != EAGAIN) {
         LogError("eventfd read failed: %s", strerror(errno));
    }

    pthread_mutex_lock(&q->lock);
    int count = q->tail - q->head;
    timer_task_t tasks_copy[TIMER_TASK_Q_MAX];
    for (i = 0; i < count; i++) {
        tasks_copy[i] = q->tasks[(q->head + i) % TIMER_TASK_Q_MAX];
    }
    q->head += count;
    pthread_mutex_unlock(&q->lock);

    for (i = 0; i < count; i++) {
        timer_task_t *t = &tasks_copy[i];
        if (t->type == TIMER_TASK_ADD) {
            timer_add(&ctx->timer_ctx, t->node, t->delay_ns, t->cb);
        } else if (t->type == TIMER_TASK_CANCEL) {
            timer_cancel(&ctx->timer_ctx, t->node);
        }
    }
}

static void *
worker_thread_fn(void *arg)
{
    timer_demo_ctx_t *ctx = arg;
    LogInfo("worker_thread: sleeping 1s before submitting a timer");
    sleep(1);

    LogInfo("worker_thread: submitting TIMER_TASK_ADD via eventfd");
    enqueue_timer_task(ctx, TIMER_TASK_ADD, &ctx->conn1.keepalive_timer,
                       TIMER_DEMO_MS_TO_NS(2000), worker_timer_cb);
    return NULL;
}

static int
timer_demo_epoll_wait_ms(const struct timer_context *timer_ctx)
{
    int64_t next_ms = timer_next_ms(timer_ctx);
    if (next_ms < 0) return -1;
    if (next_ms > INT32_MAX) return INT32_MAX;
    return (int)next_ms;
}

static int
timer_demo_drain_signalfd(int signal_fd, bool *running)
{
    struct signalfd_siginfo info;
    ssize_t nread;
    for (;;) {
        nread = read(signal_fd, &info, sizeof(info));
        if (nread == (ssize_t)sizeof(info)) {
            if (info.ssi_signo == SIGINT) {
                LogInfo("SIGINT received, exiting");
                *running = false;
                return 0;
            }
            continue;
        }
        if (nread == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) return 0;
            if (errno == EINTR) continue;
            return -1;
        }
        return -1;
    }
}

static int
timer_demo_setup_signalfd(sigset_t *mask, int *signal_fd)
{
    sigemptyset(mask);
    sigaddset(mask, SIGINT);
    pthread_sigmask(SIG_BLOCK, mask, NULL);
    *signal_fd = signalfd(-1, mask, SFD_CLOEXEC | SFD_NONBLOCK);
    return (*signal_fd < 0) ? -1 : 0;
}

static int
timer_demo_epoll_add(int epfd, int fd)
{
    struct epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.events  = EPOLLIN;
    ev.data.fd = fd;
    return epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);
}

static int
timer_demo_setup_connections(timer_demo_ctx_t *ctx)
{
    timer_context_init(&ctx->timer_ctx);
    memset(&ctx->conn1, 0, sizeof(ctx->conn1));
    memset(&ctx->conn2, 0, sizeof(ctx->conn2));

    ctx->conn1.fd = 5; ctx->conn1.timer_ctx = &ctx->timer_ctx; ctx->conn1.peer = &ctx->conn2;
    ctx->conn2.fd = 7; ctx->conn2.timer_ctx = &ctx->timer_ctx; ctx->conn2.peer = &ctx->conn1;

    timer_node_init(&ctx->conn1.timeout_timer);
    timer_node_init(&ctx->conn1.keepalive_timer);
    timer_node_init(&ctx->conn2.timeout_timer);
    timer_node_init(&ctx->conn2.keepalive_timer);

    timer_add(&ctx->timer_ctx, &ctx->conn1.timeout_timer,
              TIMER_DEMO_MS_TO_NS(TIMER_DEMO_CONN1_DELAY_MS), timer_demo_on_conn_timeout);
    timer_add(&ctx->timer_ctx, &ctx->conn2.timeout_timer,
              TIMER_DEMO_MS_TO_NS(TIMER_DEMO_CONN2_DELAY_MS), timer_demo_on_conn_timeout);

    LogInfo("timers armed: fd=5 in %dms, fd=7 in %dms", TIMER_DEMO_CONN1_DELAY_MS, TIMER_DEMO_CONN2_DELAY_MS);
    return 0;
}

static int
timer_demo_run_loop(timer_demo_ctx_t *ctx)
{
    struct epoll_event events[TIMER_DEMO_MAX_EVENTS];
    int i, ready, wait_ms;

    while (ctx->running) {
        wait_ms = timer_demo_epoll_wait_ms(&ctx->timer_ctx);
        ready = epoll_wait(ctx->epfd, events, TIMER_DEMO_MAX_EVENTS, wait_ms);
        if (ready < 0) {
            if (errno == EINTR) continue;
            return -1;
        }

        for (i = 0; i < ready; i++) {
            if (events[i].data.fd == ctx->signal_fd && (events[i].events & EPOLLIN)) {
                timer_demo_drain_signalfd(ctx->signal_fd, &ctx->running);
            } else if (events[i].data.fd == ctx->event_fd && (events[i].events & EPOLLIN)) {
                drain_task_queue(ctx);
            }
        }

        timer_process(&ctx->timer_ctx);

        if (ctx->timer_ctx.count == 0) {
            LogInfo("all timers handled, exiting");
            break;
        }
    }
    return 0;
}

static void
timer_demo_teardown(timer_demo_ctx_t *ctx, sigset_t *mask)
{
    ctx->running = false;
    uint64_t val = 1;
    if (ctx->event_fd >= 0) {
        if (write(ctx->event_fd, &val, sizeof(val))) {}; // Wake up worker if any
    }
    if (ctx->worker_tid != 0) {
        pthread_join(ctx->worker_tid, NULL);
    }
    if (ctx->epfd >= 0) close(ctx->epfd);
    if (ctx->signal_fd >= 0) close(ctx->signal_fd);
    if (ctx->event_fd >= 0) close(ctx->event_fd);
    pthread_mutex_destroy(&ctx->task_queue.lock);
    if (mask != NULL) pthread_sigmask(SIG_UNBLOCK, mask, NULL);
}

int
main(void)
{
    timer_demo_ctx_t ctx;
    sigset_t mask;

    memset(&ctx, 0, sizeof(ctx));
    ctx.epfd = -1;
    ctx.signal_fd = -1;
    ctx.event_fd = -1;
    ctx.running = true;
    pthread_mutex_init(&ctx.task_queue.lock, NULL);

    ctx.epfd = epoll_create1(EPOLL_CLOEXEC);
    if (ctx.epfd < 0) goto out;

    timer_demo_setup_signalfd(&mask, &ctx.signal_fd);
    timer_demo_epoll_add(ctx.epfd, ctx.signal_fd);

    ctx.event_fd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
    if (ctx.event_fd < 0) goto out_teardown;
    timer_demo_epoll_add(ctx.epfd, ctx.event_fd);

    if (timer_demo_setup_connections(&ctx) != 0) goto out_teardown;

    if (pthread_create(&ctx.worker_tid, NULL, worker_thread_fn, &ctx) != 0) {
        goto out_teardown;
    }

    LogInfo("timer_demo pid=%d: SIGINT to exit", (int)getpid());

    timer_demo_run_loop(&ctx);

out_teardown:
    timer_demo_teardown(&ctx, &mask);
out:
    return 0;
}
