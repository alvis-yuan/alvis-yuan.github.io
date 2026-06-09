/**
 * @file timer.h
 * @brief 用户态侵入式最小堆定时器（timer_heap.ctx 设计）
 *
 * 设计参考：
 *   Linux include/linux/list.ctx    (侵入式链表范式)
 *   Linux include/linux/min_heap.ctx (最小堆 API)
 *   Documentation/core-api/min_heap.rst (Kuan-Wei Chiu)
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef TIMER_H
#define TIMER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

#include "utils.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ─── 时钟辅助 ─── */
static inline uint64_t
timer_now_ns(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

/* ─── 侵入式节点（类比 struct list_head）─── */
struct timer_node {
    uint64_t expires;                               /* 绝对过期时刻（ns, CLOCK_MONOTONIC） */
    void (*callback)(struct timer_node *);     /* 超时回调 */
    int      internal_idx;                              /* 在堆中的下标（-1 表示不在堆中） */
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
static inline void
timer_node_init(struct timer_node *n)
{
    n->expires  = 0;
    n->callback = NULL;
    n->internal_idx = -1;
}

/* ─── 堆管理结构体 ─── */
#define TIMER_MAX 4096

struct timer_context {
    struct timer_node *data[TIMER_MAX]; /* 指针数组 */
    int                     count;              /* 当前节点数 */
};

/* ─── 核心 API 声明 ─── */

/**
 * @brief 初始化堆（清零 count 与 data）
 * @param ctx 堆对象；不可为 NULL
 */
void timer_context_init(struct timer_context *ctx);

/**
 * @brief 插入定时器，O(log n)
 *
 * 参照 min_heap_push_inline：放末尾，sift_up。
 *
 * @return true 成功；false 堆满或参数无效
 */
bool timer_add(struct timer_context *ctx,
                     struct timer_node *node,
                     uint64_t delay_ns,
                     void (*cb)(struct timer_node *));

/**
 * @brief 取消任意定时器，O(log n)
 *
 * 参照 min_heap_del_inline：与末尾交换，sift_up + sift_down。
 * 节点不在堆中时幂等返回。
 */
void timer_cancel(struct timer_context *ctx,
                       struct timer_node *node);

/**
 * @brief 返回距堆顶到期的毫秒数（参照 min_heap_peek）
 * @return 毫秒数；无定时器时 -1；参数无效时 -1
 */
int64_t timer_next_ms(const struct timer_context *ctx);

/**
 * @brief 处理所有已到期节点（类比 min_heap_pop 循环）
 */
void timer_process(struct timer_context *ctx);

#ifdef __cplusplus
}
#endif

#endif /* TIMER_H */
