/**
 * @file event_demo.c
 * @brief 主线程 epoll 事件循环：signalfd 收 SIGINT/SIGUSR1，timer_heap 定时器
 *
 * 交互：
 *   SIGINT  — 退出
 *   SIGUSR1 — 打印 glibc malloc 诊断（mem_frag_diag）
 *
 * 编译运行：make event && out/bin/event
 */

#define _GNU_SOURCE

#include "mem_frag_diag.h"
#include "timer.h"

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
    EVENT_DEMO_MAX_EVENTS = 16,
    EVENT_DEMO_TICK_MS    = 2000,
};

/** 纳秒：周期性 tick 间隔 */
#define EVENT_DEMO_TICK_NS  ((uint64_t)EVENT_DEMO_TICK_MS * 1000000ULL)

/** 嵌入 timer_node 的演示定时器 */
typedef struct demo_timer {
    struct timer_node node;
    struct timer_context     *heap;
    const char            *name;
} demo_timer_t;

/** 事件循环上下文（单线程，仅 main 访问） */
typedef struct event_demo_ctx {
    int              epfd;
    int              signal_fd;
    struct timer_context timers;
    demo_timer_t     tick_timer;
    bool             running;
} event_demo_ctx_t;

static void
event_tick_cb(struct timer_node *node)
{
    demo_timer_t *timer = NULL;

    timer = TIMER_ENTRY(node, demo_timer_t, node);
    LogInfo("periodic timer '%s' fired", timer->name);

    /* 一次性 pop 后需重新入堆以实现周期触发 */
    if (!timer_add(timer->heap, node, EVENT_DEMO_TICK_NS, event_tick_cb)) {
        LogError("timer_add re-arm failed for '%s'", timer->name);
    }
}

static void
event_dump_malloc_info(void)
{
    int rc = 0;

    if (!mem_frag_diag_is_supported()) {
        LogError("mem_frag_diag not supported on this platform");
        return;
    }

    (void)fprintf(stdout, "\n=== SIGUSR1: malloc diagnostics ===\n");
    rc = mem_frag_diag_run(stdout, MEM_FRAG_DIAG_F_STATS_STDERR);
    if (rc != 0) {
        LogError("mem_frag_diag_run failed: rc=%d", rc);
    }
}

static int
event_handle_signal_info(const struct signalfd_siginfo *info, bool *running)
{
    if (info == NULL || running == NULL) {
        LogError("invalid argument: info=%p running=%p",
                 (const void *)info, (void *)running);
        return -1;
    }

    if (info->ssi_signo == SIGINT) {
        LogInfo("SIGINT received, exiting");
        *running = false;
        return 0;
    }

    if (info->ssi_signo == SIGUSR1) {
        event_dump_malloc_info();
        return 0;
    }

    LogInfo("ignored signal: signo=%u", info->ssi_signo);
    return 0;
}

static int
event_drain_signalfd(int signal_fd, bool *running)
{
    struct signalfd_siginfo info;
    ssize_t                 nread = 0;

    if (running == NULL) {
        LogError("event_drain_signalfd: running is NULL");
        return -1;
    }

    for (;;) {
        nread = read(signal_fd, &info, sizeof(info));
        if (nread == (ssize_t)sizeof(info)) {
            if (event_handle_signal_info(&info, running) != 0) {
                return -1;
            }
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
event_epoll_wait_ms(const struct timer_context *timers)
{
    int64_t next_ms = 0;

    next_ms = timer_next_ms(timers);
    if (next_ms < 0) {
        return -1;
    }
    if (next_ms > INT32_MAX) {
        return INT32_MAX;
    }
    return (int)next_ms;
}

static int
event_setup_signalfd(sigset_t *mask, int *signal_fd)
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
    sigaddset(mask, SIGUSR1);

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
event_epoll_add(int epfd, int fd)
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
event_setup_timers(event_demo_ctx_t *ctx)
{
    if (ctx == NULL) {
        LogError("event_setup_timers: ctx is NULL");
        return -1;
    }

    timer_context_init(&ctx->timers);
    timer_node_init(&ctx->tick_timer.node);

    ctx->tick_timer.heap = &ctx->timers;
    ctx->tick_timer.name = "tick";

    if (!timer_add(&ctx->timers,
                         &ctx->tick_timer.node,
                         EVENT_DEMO_TICK_NS,
                         event_tick_cb)) {
        LogError("timer_add initial tick failed");
        return -1;
    }
    return 0;
}

static int
event_run_loop(event_demo_ctx_t *ctx)
{
    struct epoll_event events[EVENT_DEMO_MAX_EVENTS];
    int                i = 0;
    int                ready = 0;
    int                wait_ms = 0;

    if (ctx == NULL) {
        LogError("event_run_loop: ctx is NULL");
        return -1;
    }

    while (ctx->running) {
        wait_ms = event_epoll_wait_ms(&ctx->timers);
        ready = epoll_wait(ctx->epfd, events, EVENT_DEMO_MAX_EVENTS, wait_ms);
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
            if (event_drain_signalfd(ctx->signal_fd, &ctx->running) != 0) {
                return -1;
            }
        }

        /* 超时或 epoll 返回后处理到期定时器（与 timer_demo 相同模式） */
        timer_process(&ctx->timers);

        if (!ctx->running) {
            break;
        }
    }

    return 0;
}

static void
event_ctx_teardown(event_demo_ctx_t *ctx, sigset_t *mask)
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
    event_demo_ctx_t ctx;
    sigset_t         mask;
    int              rc = EXIT_FAILURE;
    void            *probe = NULL;

    memset(&ctx, 0, sizeof(ctx));
    ctx.epfd       = -1;
    ctx.signal_fd  = -1;
    ctx.running    = true;

    /* 分配少量内存，使 SIGUSR1 诊断输出更有参考意义 */
    probe = malloc(4096);
    if (probe == NULL) {
        LogError("malloc probe block failed");
        goto out;
    }

    ctx.epfd = epoll_create1(EPOLL_CLOEXEC);
    if (ctx.epfd < 0) {
        LogError("epoll_create1 failed: %s", strerror(errno));
        goto out_free_probe;
    }

    if (event_setup_signalfd(&mask, &ctx.signal_fd) != 0) {
        goto out_teardown;
    }

    if (event_epoll_add(ctx.epfd, ctx.signal_fd) != 0) {
        goto out_teardown;
    }

    if (event_setup_timers(&ctx) != 0) {
        goto out_teardown;
    }

    LogInfo("event_demo pid=%d: SIGINT to exit, SIGUSR1 for malloc stats",
            (int)getpid());

    if (event_run_loop(&ctx) != 0) {
        goto out_teardown;
    }

    rc = EXIT_SUCCESS;

out_teardown:
    event_ctx_teardown(&ctx, &mask);
out_free_probe:
    free(probe);
    probe = NULL;
out:
    return rc;
}
