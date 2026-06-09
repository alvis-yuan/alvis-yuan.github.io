/* timer_heap.h — 用户态侵入式最小堆定时器
 *
 * 设计参考：
 *   Linux include/linux/list.h    (侵入式链表范式)
 *   Linux include/linux/min_heap.h (最小堆 API)
 *   Documentation/core-api/min_heap.rst (Kuan-Wei Chiu)
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <time.h>

/* ─── container_of（同 Linux kernel include/linux/container_of.h）─── */
#define container_of(ptr, type, member) \
    ((type *)((char *)(ptr) - __builtin_offsetof(type, member)))

/* ─── 时钟辅助 ─── */
static inline uint64_t timer_now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

/* ─── 侵入式节点（类比 struct list_head）─── */
struct timer_heap_node {
    uint64_t  expires;    /* 绝对过期时刻（ns, CLOCK_MONOTONIC） */
    void    (*callback)(struct timer_heap_node *); /* 超时回调 */
    int       heap_idx;   /* 在堆中的下标（-1 表示不在堆中） */
};

/**
 * TIMER_ENTRY - 从侵入式节点指针获取宿主结构体指针
 * @ptr:    &宿主.timer 的地址
 * @type:   宿主结构体类型
 * @member: timer_heap_node 在宿主结构体中的字段名
 *
 * 与 list_entry(ptr, type, member) 完全相同的模式：
 *   #define list_entry(ptr,type,member) container_of(ptr,type,member)
 */
#define TIMER_ENTRY(ptr, type, member) \
    container_of(ptr, type, member)

/* ─── 初始化节点（类比 INIT_LIST_HEAD）─── */
static inline void timer_heap_node_init(struct timer_heap_node *n) {
    n->expires   = 0;
    n->callback  = NULL;
    n->heap_idx  = -1;
}

/* ─── 堆管理结构体 ─── */
#define TIMER_HEAP_MAX 4096

struct timer_heap {
    struct timer_heap_node *data[TIMER_HEAP_MAX]; /* 指针数组 */
    int count;   /* 当前节点数 */
};

/* ─── 核心 API 声明 ─── */
void timer_heap_init(struct timer_heap *h);

bool timer_heap_push(struct timer_heap *h,
                      struct timer_heap_node *node,
                      uint64_t delay_ns,
                      void (*cb)(struct timer_heap_node *));

void timer_heap_cancel(struct timer_heap *h,
                        struct timer_heap_node *node);

int64_t  timer_next_ms(const struct timer_heap *h); /* 距堆顶到期的毫秒数 */
void     timer_process(struct timer_heap *h);       /* 处理所有到期节点 */
