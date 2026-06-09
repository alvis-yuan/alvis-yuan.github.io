/**
 * @file wq.h
 * @brief 用户态工作队列（仿 Linux kernel workqueue / work_struct）
 *
 * 设计参考：
 *   https://www.kernel.org/doc/html/latest/core-api/workqueue.html
 *
 * 用法：将 wq_work 以值方式嵌入自定义结构体，回调中通过 list_entry() 反推宿主指针。
 *
 * 线程安全：
 *   - wq_queue_work / wq_flush / wq_destroy / wq_cancel_work_sync：线程安全
 *   - wq_work 同一实例同一时刻仅允许挂在一个 wq 上
 *
 * 编译：gcc -std=gnu99 -pthread wq.c your_app.c
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef WQ_H
#define WQ_H

#include <stdbool.h>
#include <stddef.h>

#include "compile_checks.h"
#include "atomic.h"
#include "list.h"
#include "utils.h"

#ifdef __cplusplus
extern "C" {
#endif

/** 最大 worker 线程数（对标 alloc_workqueue @max_active 上限） */
#define WQ_MAX_WORKERS  64

/** 队列名称最大长度（含 '\0'） */
#define WQ_NAME_LEN     64

struct wq;

struct wq_work;

/**
 * @brief 工作项回调，对标内核 work_func_t
 * @param work 嵌入在用户结构体中的 wq_work 指针
 */
typedef void (*wq_func_t)(struct wq_work *work);

/**
 * @brief 工作项，对标 struct work_struct
 *
 * entry 链入 struct wq::queue；pending / on_queue 由 wq 模块维护，应用勿直接修改。
 */
typedef struct wq_work {
    struct list_head entry; /**< 侵入式链表节点 */
    wq_func_t        func;  /**< 执行回调 */
    atomic_t         pending; /**< 1=已入队或正在执行 */
    bool             on_queue; /**< 内部：是否在 queue 链表中（受 wq->lock 保护） */
} wq_work_t;

/**
 * @brief 从 wq_work 成员指针获取宿主结构体指针
 * 
 * @note 此宏仅用于从 wq_work 成员指针获取宿主结构体指针，生产者在回调中使用。
 *
 * 等价于 list_entry(ptr, type, member)。
 */
#define WQ_ENTRY(ptr, type, member) \
    list_entry(ptr, type, member)

/** 运行时初始化工作项，对标 INIT_WORK 
 * 
 * @note 此宏仅用于初始化工作项，生产者调用。
 */
#define WQ_INIT_WORK(work, handler) \
    do { \
        INIT_LIST_HEAD(&(work)->entry); \
        (work)->func     = (handler); \
        atomic_set(&(work)->pending, 0); \
        (work)->on_queue = false; \
    } while (0)

/** 静态声明并初始化工作项，对标 DECLARE_WORK
 * 
 * @note 此宏仅用于初始化工作项，生产者调用。
 */
#define WQ_DECLARE_WORK(var, handler) \
    wq_work_t var = { \
        .entry    = LIST_HEAD_INIT((var).entry), \
        .func     = (handler), \
        .pending  = ATOMIC_INIT(0), \
        .on_queue = false, \
    }

/**
 * @brief 创建工作队列
 *
 * @param name        队列名（可为 NULL，默认 "wq"）
 * @param nr_workers  worker 数量；1 对标 create_singlethread_workqueue，>1 对标 alloc_workqueue
 * @return 成功返回队列指针；失败返回 NULL
 * 
 * @note 此函数仅用于创建工作队列，生产者调用。在初始化阶段调用。
 */
struct wq *wq_create(const char *name, int nr_workers) __must_check;

/**
 * @brief 销毁工作队列（先 flush 再停止 worker）
 * @param wq 由 wq_create 创建；NULL 安全
 * 
 * @note 此函数仅用于销毁工作队列，生产者调用。在退出阶段调用。
 */
void wq_destroy(struct wq *wq);

/**
 * @brief 将工作项提交到队列
 *
 * @return true 成功入队；false 已在 pending（幂等，与内核 queue_work 一致）或参数无效
 * 
 * @note 此函数仅用于提交工作项，生产者调用。在需要执行工作项时调用。
 */
bool wq_queue_work(struct wq *wq, wq_work_t *work);

/**
 * @brief 等待队列中所有已提交工作完成
 * @param wq 由 wq_create 创建；NULL 安全
 * 
 * @note 此函数仅用于等待工作队列中所有已提交工作完成，生产者调用。在退出阶段调用。
 */
void wq_flush(struct wq *wq);

/**
 * @brief 取消尚未开始执行的工作项；若已在执行则等待其完成
 *
 * @return true 从队列中移除；false 未在队列中（可能正在执行或已空闲）
 * 
 * @note 此函数仅用于取消工作项，生产者调用。在需要取消工作项时调用。
 */
bool wq_cancel_work_sync(struct wq *wq, wq_work_t *work);

#ifdef __cplusplus
}
#endif

#endif /* WQ_H */
