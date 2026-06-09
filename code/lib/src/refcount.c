/**
 * @file refcount.c
 * @brief 引用计数器实现（无锁基于 atomic CAS）
 */

#include "refcount.h"

#include "utils.h"

/* 定义最大引用计数与饱和检测阈值（Linux 内核惯用法：达到饱和状态强制负数或 INT_MIN/2）*/
#define REFCOUNT_MAX_VAL        (0x7FFFFFFF)
#define REFCOUNT_SATURATED_VAL  (-1073741824) /* 相当于 INT_MIN / 2 */

/**
 * @brief 置为饱和状态，并输出日志信息
 * @param r 引用计数对象的指针
 */
static void refcount_warn_saturate(refcount_t *r)
{
    atomic_set(&r->refs, REFCOUNT_SATURATED_VAL);
    LogError("refcount saturated: potential overflow, use-after-free or double-free detected");
}

bool refcount_add_not_zero(int i, refcount_t *r)
{
    int old = atomic_read(&r->refs);
    int new_val;

    do {
        if (!old) {
            return false;
        }
        if (old == REFCOUNT_SATURATED_VAL || old < 0) {
            return true;
        }

        if (old > REFCOUNT_MAX_VAL - i) {
            refcount_warn_saturate(r);
            return true;
        }

        new_val = old + i;
    } while (!atomic_cmpxchg(&r->refs, old, new_val));

    return true;
}

void refcount_add(int i, refcount_t *r)
{
    int old = atomic_read(&r->refs);
    int new_val;

    do {
        if (old == REFCOUNT_SATURATED_VAL || old < 0) {
            return;
        }
        if (old == 0) {
            refcount_warn_saturate(r);
            return;
        }

        if (old > REFCOUNT_MAX_VAL - i) {
            refcount_warn_saturate(r);
            return;
        }

        new_val = old + i;
    } while (!atomic_cmpxchg(&r->refs, old, new_val));
}

bool refcount_inc_not_zero(refcount_t *r)
{
    return refcount_add_not_zero(1, r);
}

void refcount_inc(refcount_t *r)
{
    refcount_add(1, r);
}

bool refcount_sub_and_test(int i, refcount_t *r)
{
    int old = atomic_read(&r->refs);
    int new_val;

    do {
        if (old == REFCOUNT_SATURATED_VAL || old < 0) {
            return false;
        }
        if (old == 0 || old < i) {
            refcount_warn_saturate(r);
            return false;
        }

        new_val = old - i;
    } while (!atomic_cmpxchg(&r->refs, old, new_val));

    return new_val == 0;
}

bool refcount_dec_and_test(refcount_t *r)
{
    return refcount_sub_and_test(1, r);
}

void refcount_dec(refcount_t *r)
{
    int old = atomic_read(&r->refs);
    int new_val;

    do {
        if (old == REFCOUNT_SATURATED_VAL || old < 0) {
            return;
        }
        if (old == 0 || old < 1) {
            refcount_warn_saturate(r);
            return;
        }
        if (old == 1) {
            LogError("refcount_dec on 1: missing release semantics, use refcount_dec_and_test");
            refcount_warn_saturate(r);
            return;
        }

        new_val = old - 1;
    } while (!atomic_cmpxchg(&r->refs, old, new_val));
}

bool refcount_dec_not_one(refcount_t *r)
{
    int old = atomic_read(&r->refs);
    int new_val;

    do {
        if (old == REFCOUNT_SATURATED_VAL || old < 0) {
            return true;
        }
        if (old == 1) {
            return false;
        }

        new_val = old - 1;
    } while (!atomic_cmpxchg(&r->refs, old, new_val));

    return true;
}

bool refcount_dec_and_mutex_lock(refcount_t *r, pthread_mutex_t *lock)
{
    if (refcount_dec_not_one(r)) {
        return false;
    }

    pthread_mutex_lock(lock);
    if (!refcount_dec_and_test(r)) {
        pthread_mutex_unlock(lock);
        return false;
    }

    return true;
}

bool refcount_dec_and_lock(refcount_t *r, atomic_flag_t *lock)
{
    if (refcount_dec_not_one(r)) {
        return false;
    }

    atomic_spin_lock(lock);
    if (!refcount_dec_and_test(r)) {
        atomic_spin_unlock(lock);
        return false;
    }

    return true;
}
