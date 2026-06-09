/**
 * user_wq.h — 应用层工作队列（仿 Linux kernel work_struct）
 *
 * 设计参考（官方原文）：
 *   https://www.kernel.org/doc/html/latest/core-api/workqueue.html
 *   https://www.kernel.org/doc/Documentation/driver-model/design-patterns.txt
 *   https://linux-kernel-labs.github.io/refs/heads/master/labs/deferred_work.html
 *
 * 核心思想：
 *   将 uwq_work_t 以值方式嵌入用户自定义结构体（如 struct foo），
 *   worker 线程回调时仅传递 uwq_work_t * 指针，
 *   通过 UWQ_CONTAINER_OF 宏反推出 struct foo * 指针，
 *   彻底避免 void* 强制类型转换，保留 C 类型安全。
 *
 * 编译：gcc -std=c99 -pthread your_file.c
 */

#ifndef USER_WQ_H
#define USER_WQ_H

#include <stddef.h>     /* offsetof */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <assert.h>

/* ================================================================
 * 1. container_of  —  对标 <linux/kernel.h> container_of()
 *    原文（design-patterns.txt）：
 *    "obtain a pointer to the containing struct from a pointer to a member
 *     by a simple subtraction using the offsetof() macro from standard C"
 * ================================================================ */

#ifndef UWQ_CONTAINER_OF
#define UWQ_CONTAINER_OF(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))
#endif

/* ================================================================
 * 2. 侵入式双向链表  —  对标 struct list_head（include/linux/list.h）
 *    内核原文：work_struct.entry 是 struct list_head 类型
 * ================================================================ */

typedef struct uwq_list_head {
    struct uwq_list_head *next;
    struct uwq_list_head *prev;
} uwq_list_head_t;

#define UWQ_LIST_HEAD_INIT(name) { &(name), &(name) }

static inline void uwq_list_init(uwq_list_head_t *h) {
    h->next = h; h->prev = h;
}

static inline int uwq_list_empty(const uwq_list_head_t *h) {
    return h->next == h;
}

static inline void __uwq_list_add(uwq_list_head_t *n,
                                    uwq_list_head_t *prev,
                                    uwq_list_head_t *next) {
    next->prev = n; n->next = next;
    n->prev = prev; prev->next = n;
}

/* 尾部插入（FIFO 队列语义，与内核 list_add_tail 一致） */
static inline void uwq_list_add_tail(uwq_list_head_t *n,
                                       uwq_list_head_t *head) {
    __uwq_list_add(n, head->prev, head);
}

static inline void uwq_list_del(uwq_list_head_t *n) {
    n->prev->next = n->next;
    n->next->prev = n->prev;
    n->next = NULL;
    n->prev = NULL;
}

/* list_first_entry 等价宏 */
#define UWQ_LIST_FIRST_ENTRY(head, type, member) \
    UWQ_CONTAINER_OF((head)->next, type, member)

/* ================================================================
 * 3. work_func_t  —  对标 typedef void (*work_func_t)(struct work_struct *)
 *    原文（workqueue.h）：
 *    "typedef void (*work_func_t)(struct work_struct *work);"
 * ================================================================ */

struct uwq_work;  /* 前向声明 */
typedef void (*uwq_func_t)(struct uwq_work *work);

/* ================================================================
 * 4. uwq_work_t  —  对标 struct work_struct
 *    原文（workqueue.h）：
 *    "struct work_struct { atomic_long_t data; struct list_head entry;
 *     work_func_t func; };"
 *    应用层去除 atomic_long_t data（仅内核用于池指针/状态编码）
 * ================================================================ */

typedef struct uwq_work {
    uwq_list_head_t  entry;    /* 侵入式链表节点，链入 user_wq_t.queue */
    uwq_func_t       func;     /* 待执行的回调函数 */
    volatile int      pending;  /* 1=已入队或正在执行，0=空闲 */
} uwq_work_t;

/* ================================================================
 * 5. INIT_WORK 等价宏  —  对标内核 INIT_WORK / DECLARE_WORK
 *    原文（workqueue.h）：
 *    "#define INIT_WORK(_work, _func) do {
 *       INIT_LIST_HEAD(&(_work)->entry);
 *       (_work)->func = (_func);
 *     } while (0)"
 * ================================================================ */

#define UWQ_INIT_WORK(_work, _func) \
    do { \
        uwq_list_init(&(_work)->entry); \
        (_work)->func    = (_func); \
        (_work)->pending = 0; \
    } while (0)

/* ================================================================
 * 6. user_wq_t  —  对标 struct workqueue_struct
 *    原文（cmwq 文档）：
 *    "workqueue serves as a domain for forward progress guarantee,
 *     flush and work item attributes"
 * ================================================================ */

#define UWQ_MAX_WORKERS  64
#define UWQ_NAME_LEN     64

typedef struct uwq_worker {
    pthread_t       tid;
    struct user_wq *wq;  /* 反向指向所属队列 */
} uwq_worker_t;

typedef struct user_wq {
    pthread_mutex_t  lock;        /* 保护 queue 链表和各状态字段 */
    pthread_cond_t   work_avail; /* 有新工作时 broadcast */
    pthread_cond_t   work_done;  /* flush 等待所有工作完成 */
    uwq_list_head_t  queue;       /* 待执行工作项的侵入式链表 */
    int              nr_workers;  /* 工作者线程数量 */
    int              nr_running;  /* 当前正在执行的工作数 */
    int              nr_queued;   /* 队列中等待的工作数 */
    int              shutdown;    /* 置1后 worker 线程退出 */
    uwq_worker_t     workers[UWQ_MAX_WORKERS];
    char             name[UWQ_NAME_LEN]; /* 队列名（对标 create_workqueue 的 name 参数）*/
} user_wq_t;

/* ================================================================
 * 7. worker 线程主循环  —  对标内核 kworker 线程逻辑
 *    原文（cmwq 文档）：
 *    "worker executes the functions associated with the work items one
 *     after the other. When there is no work item left on the workqueue
 *     the worker becomes idle."
 * ================================================================ */

static void *uwq_worker_thread(void *arg)
{
    uwq_worker_t *self = (uwq_worker_t *)arg;
    user_wq_t    *wq   = self->wq;

    pthread_mutex_lock(&wq->lock);
    while (1) {
        /* 等待有工作 或 shutdown */
        while (uwq_list_empty(&wq->queue) && !wq->shutdown)
            pthread_cond_wait(&wq->work_avail, &wq->lock);

        if (wq->shutdown && uwq_list_empty(&wq->queue))
            break;

        /* 取出链表头部工作项（FIFO） */
        uwq_work_t *work = UWQ_LIST_FIRST_ENTRY(&wq->queue, uwq_work_t, entry);
        uwq_list_del(&work->entry);
        wq->nr_queued--;
        wq->nr_running++;
        pthread_mutex_unlock(&wq->lock);

        /* 执行工作项回调——仅传递 uwq_work_t* 指针，
         * 用户通过 UWQ_CONTAINER_OF 取得外层结构体，无需 void* */
        work->func(work);

        pthread_mutex_lock(&wq->lock);
        work->pending = 0;
        wq->nr_running--;
        /* 唤醒 uwq_flush() 等待者 */
        if (wq->nr_running == 0 && uwq_list_empty(&wq->queue))
            pthread_cond_broadcast(&wq->work_done);
    }
    pthread_mutex_unlock(&wq->lock);
    return NULL;
}

/* ================================================================
 * 8. alloc_workqueue / create_singlethread_workqueue 等价
 *    原文（cmwq 文档）：
 *    "alloc_workqueue() takes three arguments — @name, @flags and @max_active"
 *    "create_singlethread_workqueue() uses a single thread"
 *
 * uwq_alloc(name, nr_workers)：
 *   nr_workers == 1  →  等价 create_singlethread_workqueue（有序执行保证）
 *   nr_workers  > 1  →  等价 alloc_workqueue（并发执行）
 * ================================================================ */

static inline user_wq_t *uwq_alloc(const char *name, int nr_workers)
{
    if (nr_workers <= 0 || nr_workers > UWQ_MAX_WORKERS)
        return NULL;

    user_wq_t *wq = (user_wq_t *)calloc(1, sizeof(user_wq_t));
    if (!wq) return NULL;

    pthread_mutex_init(&wq->lock,       NULL);
    pthread_cond_init (&wq->work_avail, NULL);
    pthread_cond_init (&wq->work_done,  NULL);
    uwq_list_init(&wq->queue);
    strncpy(wq->name, name ? name : "user_wq", UWQ_NAME_LEN - 1);
    wq->nr_workers = nr_workers;
    wq->shutdown   = 0;

    for (int i = 0; i < nr_workers; i++) {
        wq->workers[i].wq = wq;
        pthread_create(&wq->workers[i].tid, NULL, uwq_worker_thread, &wq->workers[i]);
    }
    return wq;
}

/* ================================================================
 * 9. queue_work  —  对标 queue_work() / schedule_work()
 *    原文（cmwq 文档）：
 *    "a work item describing which function to execute is put on a queue"
 *    原文（workqueue.h）：
 *    "bool queue_work(struct workqueue_struct *wq, struct work_struct *work);"
 *    返回值：1=成功入队，0=已在队列中（pending，与内核语义一致）
 * ================================================================ */

static inline int uwq_queue_work(user_wq_t *wq, uwq_work_t *work)
{
    if (!wq || !work || !work->func) return 0;

    pthread_mutex_lock(&wq->lock);
    if (work->pending) {
        /* 已在队列中，忽略（内核语义：queue_work 对同一 work 幂等） */
        pthread_mutex_unlock(&wq->lock);
        return 0;
    }
    work->pending = 1;
    uwq_list_add_tail(&work->entry, &wq->queue);
    wq->nr_queued++;
    pthread_cond_signal(&wq->work_avail);
    pthread_mutex_unlock(&wq->lock);
    return 1;
}

/* ================================================================
 * 10. flush_workqueue  —  对标 flush_workqueue()
 *     原文（linux-kernel-labs）：
 *     "We can wait for a workqueue to complete running all of its work
 *      items by calling flush_scheduled_work(). This function is blocking."
 * ================================================================ */

static inline void uwq_flush(user_wq_t *wq)
{
    if (!wq) return;
    pthread_mutex_lock(&wq->lock);
    while (wq->nr_running > 0 || !uwq_list_empty(&wq->queue))
        pthread_cond_wait(&wq->work_done, &wq->lock);
    pthread_mutex_unlock(&wq->lock);
}

/* ================================================================
 * 11. destroy_workqueue  —  对标 destroy_workqueue()
 *     原文（cmwq 文档）：
 *     "void destroy_workqueue(struct workqueue_struct *wq) —
 *      Safely destroy a workqueue. All work currently pending will be done first."
 * ================================================================ */

static inline void uwq_destroy(user_wq_t *wq)
{
    if (!wq) return;

    /* 等待所有已提交工作完成（与内核语义一致） */
    uwq_flush(wq);

    pthread_mutex_lock(&wq->lock);
    wq->shutdown = 1;
    pthread_cond_broadcast(&wq->work_avail);  /* 唤醒所有 worker 退出 */
    pthread_mutex_unlock(&wq->lock);

    for (int i = 0; i < wq->nr_workers; i++)
        pthread_join(wq->workers[i].tid, NULL);

    pthread_cond_destroy (&wq->work_done);
    pthread_cond_destroy (&wq->work_avail);
    pthread_mutex_destroy(&wq->lock);
    free(wq);
}

/* ================================================================
 * 12. 便利宏：schedule_work 等价（使用进程默认工作队列概念）
 *     DECLARE_WORK 等价（栈上静态初始化）
 * ================================================================ */

#define UWQ_DECLARE_WORK(var, fn) \
    uwq_work_t var = { \
        .entry   = UWQ_LIST_HEAD_INIT((var).entry), \
        .func    = (fn), \
        .pending = 0 \
    }

/* ================================================================
 * 13. cancel_work_sync 等价
 *     原文（linux-kernel-labs）：
 *     "int cancel_work_sync(struct delayed_work *work) — The call only stops
 *      the subsequent execution... when these calls return, it is guaranteed
 *      that the task will no longer run."
 *     注意：本实现仅取消还未开始执行的工作（pending状态）。
 *     若工作已在执行中，则等待其完成（sync语义）。
 * ================================================================ */

static inline int uwq_cancel_work_sync(user_wq_t *wq, uwq_work_t *work)
{
    int cancelled = 0;
    pthread_mutex_lock(&wq->lock);
    /* 若在队列中（尚未被取走执行），则从链表中移除 */
    if (work->pending && work->entry.next != NULL) {
        uwq_list_del(&work->entry);
        wq->nr_queued--;
        work->pending = 0;
        cancelled = 1;
    }
    pthread_mutex_unlock(&wq->lock);
    /* 等待执行中的实例完成 */
    uwq_flush(wq);
    return cancelled;
}

#endif /* USER_WQ_H */
