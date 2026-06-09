#include "timer_heap.h"
#include <stdio.h>
#include <sys/epoll.h>

/* ── 宿主结构体：HTTP 连接，嵌入 timer_heap_node ── */
struct http_conn {
    int  fd;
    char buf[4096];
    struct timer_heap_node timeout_timer;  /* ← 侵入式嵌入 */
    struct timer_heap_node keepalive_timer; /* 同一结构体可有多个 */
};

/* ── 超时回调：通过 TIMER_ENTRY 取回宿主指针 ── */
static void on_conn_timeout(struct timer_heap_node *node)
{
    /* 等价于 list_entry(node, struct http_conn, timeout_timer) */
    struct http_conn *conn =
        TIMER_ENTRY(node, struct http_conn, timeout_timer);

    printf("连接 fd=%d 超时，强制关闭\n", conn->fd);
    /* close(conn->fd); free(conn); */
}

int main(void)
{
    struct timer_heap heap;
    timer_heap_init(&heap);

    /* 创建 HTTP 连接（从内存池分配，非示意） */
    struct http_conn conn1 = { .fd = 5 };
    struct http_conn conn2 = { .fd = 7 };

    /* 初始化嵌入的节点（类比 INIT_LIST_HEAD）*/
    timer_heap_node_init(&conn1.timeout_timer);
    timer_heap_node_init(&conn2.timeout_timer);

    /* 注册定时器（30s、60s 后超时）*/
    timer_heap_push(&heap, &conn1.timeout_timer,
                    30000000000ULL, on_conn_timeout);   /* 30s */
    timer_heap_push(&heap, &conn2.timeout_timer,
                    60000000000ULL, on_conn_timeout);   /* 60s */

    int epfd = epoll_create1(0);

    /* ── 事件循环 ── */
    while (1) {
        int64_t timeout_ms = timer_next_ms(&heap);
        struct epoll_event evs[64];

        /* epoll_wait 超时时间 = 距最近定时器到期的毫秒数 */
        int n = epoll_wait(epfd, evs, 64, (int)timeout_ms);

        /* 处理 I/O 事件... */
        (void)n;

        /* 处理所有到期定时器 */
        timer_process(&heap);
    }
}

/* 取消一个定时器（O(log n)）：
 *   timer_heap_cancel(&heap, &conn1.timeout_timer);
 *
 * 重置定时器（先取消再重新注册）：
 *   timer_heap_cancel(&heap, &conn1.timeout_timer);
 *   timer_heap_push(&heap, &conn1.timeout_timer, new_delay_ns, cb);
 */
