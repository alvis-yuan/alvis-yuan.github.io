/**
 * @file timer_demo.c
 * @brief timer_heap 与 epoll_wait 集成演示
 *
 * 覆盖接口：
 *   timer_heap_init / timer_heap_node_init / timer_heap_push
 *   timer_heap_cancel / timer_process / timer_next_ms / TIMER_ENTRY
 *
 * 场景：
 *   - 两个 HTTP 连接各自嵌入 timeout_timer（30s/60s 缩为 1.5s/4s 便于观察）
 *   - conn1 超时后取消 conn2 定时器，演示 timer_heap_cancel
 *   - epoll_wait 超时取自 timer_next_ms
 *   - SIGINT 退出
 *
 * 编译运行：make timer && out/bin/timer
 */

#define _GNU_SOURCE

#include "timer_heap.h"

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

#include "raii.h"

enum {
    TIMER_DEMO_MAX_EVENTS    = 8,
    TIMER_DEMO_CONN1_DELAY_MS = 1500,
    TIMER_DEMO_CONN2_DELAY_MS = 4000,
    TIMER_DEMO_BUF_SIZE      = 4096,
};

#define TIMER_DEMO_MS_TO_NS(ms)  ((uint64_t)(ms) * 1000000ULL)

/**
 * 宿主结构体：模拟 HTTP 连接，同一结构体可嵌入多个 timer_heap_node
 * （keepalive_timer 仅初始化，演示多节点嵌入，本 demo 未注册）
 */
typedef struct timer_demo_conn {
    int                     fd;
    char                    buf[TIMER_DEMO_BUF_SIZE];
    struct timer_heap_node  timeout_timer;
    struct timer_heap_node  keepalive_timer;
    struct timer_heap      *heap;
    struct timer_demo_conn *peer;
} timer_demo_conn_t;

typedef struct timer_demo_ctx {
    int                epfd;
    int                signal_fd;
    struct timer_heap  heap;
    timer_demo_conn_t  conn1;
    timer_demo_conn_t  conn2;
    bool               running;
} timer_demo_ctx_t;

static int
timer_demo_epoll_wait_ms(const struct timer_heap *heap)
{
    int64_t next_ms = 0;

    next_ms = timer_next_ms(heap);
    if (next_ms < 0) {
        return -1;
    }
    if (next_ms > INT32_MAX) {
        return INT32_MAX;
    }
    return (int)next_ms;
}

static void
timer_demo_on_conn_timeout(struct timer_heap_node *node)
{
    timer_demo_conn_t *conn = NULL;

    conn = TIMER_ENTRY(node, timer_demo_conn_t, timeout_timer);
    LogInfo("connection fd=%d timeout, would close", conn->fd);

    if (conn->fd == 5 && conn->peer != NULL && conn->heap != NULL) {
        /* conn1 超时后取消 conn2，演示 O(log n) cancel */
        timer_heap_cancel(conn->heap, &conn->peer->timeout_timer);
        LogInfo("cancelled pending timer on fd=%d", conn->peer->fd);
    }
}

static int
timer_demo_drain_signalfd(int signal_fd, bool *running)
{
    struct signalfd_siginfo info;
    ssize_t                 nread = 0;

    if (running == NULL) {
        LogError("timer_demo_drain_signalfd: running is NULL");
        return -1;
    }

    for (;;) {
        nread = read(signal_fd, &info, sizeof(info));
        if (nread == (ssize_t)sizeof(info)) {
            if (info.ssi_signo == SIGINT) {
                LogInfo("SIGINT received, exiting");
                *running = false;
                return 0;
            }
            LogInfo("ignored signal: signo=%u", info.ssi_signo);
            continue;
        }

        if (nread == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return 0;
            }
            if (errno == EINTR) {
                continue;
            }
            LogError("signalfd read failed: %s", strerror(errno));
            return -1;
        }

        LogError("signalfd read short: got %zd bytes", nread);
        return -1;
    }
}

static int
timer_demo_setup_signalfd(sigset_t *mask, int *signal_fd)
{
    int err = 0;
    int fd = -1;

    if (mask == NULL || signal_fd == NULL) {
        LogError("invalid argument: mask=%p signal_fd=%p",
                 (void *)mask, (void *)signal_fd);
        return -1;
    }

    sigemptyset(mask);
    sigaddset(mask, SIGINT);

    err = pthread_sigmask(SIG_BLOCK, mask, NULL);
    if (err != 0) {
        LogError("pthread_sigmask(SIG_BLOCK) failed: %s", strerror(err));
        return -1;
    }

    fd = signalfd(-1, mask, SFD_CLOEXEC | SFD_NONBLOCK);
    if (fd < 0) {
        int saved = errno;

        (void)pthread_sigmask(SIG_UNBLOCK, mask, NULL);
        LogError("signalfd failed: %s", strerror(saved));
        return -1;
    }

    *signal_fd = fd;
    return 0;
}

static int
timer_demo_epoll_add(int epfd, int fd)
{
    struct epoll_event ev;

    memset(&ev, 0, sizeof(ev));
    ev.events  = EPOLLIN;
    ev.data.fd = fd;

    if (epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev) != 0) {
        LogError("epoll_ctl(ADD, fd=%d) failed: %s", fd, strerror(errno));
        return -1;
    }
    return 0;
}

static int
timer_demo_setup_connections(timer_demo_ctx_t *ctx)
{
    if (ctx == NULL) {
        LogError("timer_demo_setup_connections: ctx is NULL");
        return -1;
    }

    timer_heap_init(&ctx->heap);

    memset(&ctx->conn1, 0, sizeof(ctx->conn1));
    memset(&ctx->conn2, 0, sizeof(ctx->conn2));

    ctx->conn1.fd   = 5;
    ctx->conn1.heap = &ctx->heap;
    ctx->conn1.peer = &ctx->conn2;

    ctx->conn2.fd   = 7;
    ctx->conn2.heap = &ctx->heap;
    ctx->conn2.peer = &ctx->conn1;

    timer_heap_node_init(&ctx->conn1.timeout_timer);
    timer_heap_node_init(&ctx->conn1.keepalive_timer);
    timer_heap_node_init(&ctx->conn2.timeout_timer);
    timer_heap_node_init(&ctx->conn2.keepalive_timer);

    if (!timer_heap_push(&ctx->heap,
                         &ctx->conn1.timeout_timer,
                         TIMER_DEMO_MS_TO_NS(TIMER_DEMO_CONN1_DELAY_MS),
                         timer_demo_on_conn_timeout)) {
        LogError("timer_heap_push conn1 failed");
        return -1;
    }

    if (!timer_heap_push(&ctx->heap,
                         &ctx->conn2.timeout_timer,
                         TIMER_DEMO_MS_TO_NS(TIMER_DEMO_CONN2_DELAY_MS),
                         timer_demo_on_conn_timeout)) {
        LogError("timer_heap_push conn2 failed");
        return -1;
    }

    LogInfo("timers armed: fd=5 in %dms, fd=7 in %dms (fd=7 will be cancelled)",
            TIMER_DEMO_CONN1_DELAY_MS, TIMER_DEMO_CONN2_DELAY_MS);
    return 0;
}

static int
timer_demo_run_loop(timer_demo_ctx_t *ctx)
{
    struct epoll_event events[TIMER_DEMO_MAX_EVENTS];
    int                i = 0;
    int                ready = 0;
    int                wait_ms = 0;

    if (ctx == NULL) {
        LogError("timer_demo_run_loop: ctx is NULL");
        return -1;
    }

    while (ctx->running) {
        wait_ms = timer_demo_epoll_wait_ms(&ctx->heap);
        ready = epoll_wait(ctx->epfd, events, TIMER_DEMO_MAX_EVENTS, wait_ms);
        if (ready < 0) {
            if (errno == EINTR) {
                continue;
            }
            LogError("epoll_wait failed: %s", strerror(errno));
            return -1;
        }

        for (i = 0; i < ready; i++) {
            if (events[i].data.fd != ctx->signal_fd) {
                continue;
            }
            if ((events[i].events & EPOLLIN) == 0) {
                continue;
            }
            if (timer_demo_drain_signalfd(ctx->signal_fd, &ctx->running) != 0) {
                return -1;
            }
        }

        /* epoll 超时或返回后统一处理到期定时器 */
        timer_process(&ctx->heap);

        if (ctx->heap.count == 0) {
            LogInfo("all timers handled, exiting");
            break;
        }
    }

    return 0;
}

static void
timer_demo_teardown(timer_demo_ctx_t *ctx, sigset_t *mask)
{
    if (ctx == NULL) {
        return;
    }

    if (ctx->epfd >= 0) {
        close(ctx->epfd);
        ctx->epfd = -1;
    }
    if (ctx->signal_fd >= 0) {
        close(ctx->signal_fd);
        ctx->signal_fd = -1;
    }
    if (mask != NULL) {
        (void)pthread_sigmask(SIG_UNBLOCK, mask, NULL);
    }
}

int
main(void)
{
    timer_demo_ctx_t ctx;
    sigset_t         mask;
    int              rc = EXIT_FAILURE;

    memset(&ctx, 0, sizeof(ctx));
    ctx.epfd      = -1;
    ctx.signal_fd = -1;
    ctx.running   = true;

    ctx.epfd = epoll_create1(EPOLL_CLOEXEC);
    if (ctx.epfd < 0) {
        LogError("epoll_create1 failed: %s", strerror(errno));
        goto out;
    }

    if (timer_demo_setup_signalfd(&mask, &ctx.signal_fd) != 0) {
        goto out_teardown;
    }

    if (timer_demo_epoll_add(ctx.epfd, ctx.signal_fd) != 0) {
        goto out_teardown;
    }

    if (timer_demo_setup_connections(&ctx) != 0) {
        goto out_teardown;
    }

    LogInfo("timer_demo pid=%d: SIGINT to exit", (int)getpid());

    if (timer_demo_run_loop(&ctx) != 0) {
        goto out_teardown;
    }

    rc = EXIT_SUCCESS;

out_teardown:
    timer_demo_teardown(&ctx, &mask);
out:
    return rc;
}
