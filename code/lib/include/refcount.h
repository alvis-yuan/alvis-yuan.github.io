/**
 * @file refcount.h
 * @brief 引用计数器（用户态实现）防溢出、防 UAF 的饱和保护机制
 */

#ifndef LIB_REFCOUNT_H
#define LIB_REFCOUNT_H

#include <stdbool.h>

#include <pthread.h>

#include "atomic.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 表示一个引用计数对象（带有饱和保护状态机）
 */
typedef struct {
    atomic_t refs;
} refcount_t;

/** @brief 引用计数静态初始化宏 */
#define REFCOUNT_INIT(n) { ATOMIC_INIT(n) }

/**
 * @brief 初始化引用计数为指定值
 * @param r 引用计数指针
 * @param n 初始值
 */
static inline void refcount_set(refcount_t *r, int n)
{
    atomic_set(&r->refs, n);
}

/**
 * @brief 读取当前引用计数
 * @param r 引用计数指针
 * @return 引用计数值，如果遇到饱和则返回负数
 */
static inline int refcount_read(const refcount_t *r)
{
    return atomic_read((atomic_t *)&r->refs);
}

bool refcount_add_not_zero(int i, refcount_t *r);
void refcount_add(int i, refcount_t *r);

bool refcount_inc_not_zero(refcount_t *r);
void refcount_inc(refcount_t *r);

bool refcount_sub_and_test(int i, refcount_t *r);
bool refcount_dec_and_test(refcount_t *r);
void refcount_dec(refcount_t *r);

bool refcount_dec_not_one(refcount_t *r);
bool refcount_dec_and_mutex_lock(refcount_t *r, pthread_mutex_t *lock);
bool refcount_dec_and_lock(refcount_t *r, atomic_flag_t *lock);

#ifdef __cplusplus
}
#endif

#endif /* LIB_REFCOUNT_H */
