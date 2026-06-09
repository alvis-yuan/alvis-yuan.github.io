/**
 * @file kref.h
 * @brief 通用引用计数对象（用户态移植，源自 Linux include/linux/kref.h）
 *
 * 基于 refcount.h 实现，移除内核专用依赖：
 *   - 不含 spinlock.h / struct mutex
 *   - kref_put_mutex 使用 pthread_mutex_t
 *   - kref_put_lock 使用 atomic_flag_t 自旋锁（见 atomic.h）
 *   - 移除 __must_check / __cond_acquires 等内核编译器注解
 *
 * SPDX-License-Identifier: GPL-2.0
 */

#ifndef LIB_KREF_H
#define LIB_KREF_H

#include <pthread.h>
#include <stdbool.h>

#include "atomic.h"
#include "refcount.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 嵌入宿主结构体的引用计数器 */
struct kref {
    refcount_t refcount;
};

/** @brief 静态初始化宏，@p n 为初始引用数 */
#define KREF_INIT(n) { .refcount = REFCOUNT_INIT(n) }

/**
 * @brief 初始化引用计数为 1
 * @param kref 引用计数对象
 */
static inline void kref_init(struct kref *kref)
{
    refcount_set(&kref->refcount, 1);
}

/**
 * @brief 读取当前引用计数
 * @param kref 引用计数对象
 * @return 当前引用数（饱和时为负数）
 */
static inline unsigned int kref_read(const struct kref *kref)
{
    return (unsigned int)refcount_read(&kref->refcount);
}

/**
 * @brief 增加引用计数
 * @param kref 引用计数对象
 */
static inline void kref_get(struct kref *kref)
{
    refcount_inc(&kref->refcount);
}

/**
 * @brief 减少引用计数，归零时调用 release
 * @param kref 引用计数对象
 * @param release 最后一个引用释放时的回调（不可为 NULL，不可直接传 free）
 * @return 1 表示对象已被 release；0 表示仍有其他引用
 *
 * @note 返回 0 时也不能假定 kref 仍有效，仅用于判断是否已销毁。
 */
static inline int kref_put(struct kref *kref, void (*release)(struct kref *kref))
{
    if (refcount_dec_and_test(&kref->refcount)) {
        release(kref);
        return 1;
    }
    return 0;
}

/**
 * @brief 减少引用计数，归零时持 mutex 并调用 release
 * @param kref 引用计数对象
 * @param release 释放回调
 * @param lock 在最后一个引用路径上已加锁的 mutex
 * @return 1 表示已 release 且调用方仍持有 lock；0 表示未销毁
 */
static inline int kref_put_mutex(struct kref *kref,
                                 void (*release)(struct kref *kref),
                                 pthread_mutex_t *lock)
{
    if (refcount_dec_and_mutex_lock(&kref->refcount, lock)) {
        release(kref);
        return 1;
    }
    return 0;
}

/**
 * @brief 减少引用计数，归零时持自旋锁并调用 release
 * @param kref 引用计数对象
 * @param release 释放回调
 * @param lock atomic_flag_t 自旋锁（用户态替代内核 spinlock_t）
 * @return 1 表示已 release 且调用方仍持有 lock；0 表示未销毁
 */
static inline int kref_put_lock(struct kref *kref,
                                void (*release)(struct kref *kref),
                                atomic_flag_t *lock)
{
    if (refcount_dec_and_lock(&kref->refcount, lock)) {
        release(kref);
        return 1;
    }
    return 0;
}

/**
 * @brief 引用计数非零时增加，否则不增加
 * @param kref 引用计数对象
 * @return 非 0 表示增加成功；0 表示计数已为 0
 *
 * 典型用法：查找表 + 延迟加锁销毁路径，配合 kref_put 简化 RCU 式查找。
 */
static inline int __attribute__((warn_unused_result)) kref_get_unless_zero(struct kref *kref)
{
    return (int)refcount_inc_not_zero(&kref->refcount);
}

#ifdef __cplusplus
}
#endif

#endif /* LIB_KREF_H */
