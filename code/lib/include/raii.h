/*
 * 修改作者: GitHub Copilot
 * 修改日期: 2026-06-05
 * 修改原因: 按 C 代码规范重写 cleanup 宏头文件
 */

#ifndef RAII_H
#define RAII_H

#define _GNU_SOURCE
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

/**
 * @brief 为局部变量绑定 cleanup 回调。
 *
 * @param cleanup_func 作用域结束时调用的清理函数。
 */
#define auto_cleanup(cleanup_func) __attribute__((cleanup(cleanup_func)))

static inline void _macro_autofd_cleanup(int *fd)
{
    if (fd != NULL && *fd >= 0) {
        close(*fd);
        *fd = -1;
    }
}

/**
 * @brief 为文件描述符声明自动关闭属性。
 */
#define autofd auto_cleanup(_macro_autofd_cleanup)

/**
 * @brief 转移文件描述符所有权，避免 cleanup 自动关闭。
 *
 * @param fd 文件描述符地址。
 *
 * @return 返回原始文件描述符，若入参为空则返回 -1。
 */
static inline int autofd_steal(int *fd)
{
    int stolen_fd = -1;

    if (fd != NULL) {
        stolen_fd = *fd;
        *fd = -1;
    }

    return stolen_fd;
}

static inline void _macro_autofp_cleanup(FILE **fp)
{
    if (fp != NULL && *fp != NULL) {
        fclose(*fp);
        *fp = NULL;
    }
}

/**
 * @brief 为文件指针声明自动关闭属性。
 */
#define autofp auto_cleanup(_macro_autofp_cleanup)

/**
 * @brief 转移文件指针所有权，避免 cleanup 自动关闭。
 *
 * @param fp 文件指针地址。
 *
 * @return 返回原始文件指针，若入参为空则返回 NULL。
 */
static inline FILE *autofp_steal(FILE **fp)
{
    FILE *stolen_fp = NULL;

    if (fp != NULL) {
        stolen_fp = *fp;
        *fp = NULL;
    }

    return stolen_fp;
}

static inline void *_macro_autoptr_steal_impl(void *pointer)
{
    void **object = pointer;
    void *stolen_object = NULL;

    if (object != NULL) {
        stolen_object = *object;
        *object = NULL;
    }

    return stolen_object;
}

/**
 * @brief 转移指针所有权，避免 cleanup 自动释放。
 *
 * @param pointer_ptr 指针变量地址。
 *
 * @return 返回被转移的原始指针。
 */
#define autoptr_steal(pointer_ptr)                                      \
    _macro_autoptr_steal_impl((void *)(pointer_ptr))

typedef void _macro_autofree_t;

/**
 * @brief 为指定类型声明自动清理函数。
 *
 * @param TypeName 类型名，通常为 typedef 名称。
 */
#define DECLARE_AUTOPTR_CLEANUP_FUNC(TypeName)                               \
    static inline void autoptr_cleanup_##TypeName(void *object)

/**
 * @brief 为指定类型实现自动清理函数。
 *
 * @param TypeName 类型名，通常为 typedef 名称。
 * @param cleanup_func 类型对应的释放函数。
 */
#define DEFINE_AUTOPTR_CLEANUP_FUNC_IMPL(TypeName, cleanup_func)             \
    DECLARE_AUTOPTR_CLEANUP_FUNC(TypeName)                                   \
    {                                                                        \
        TypeName **typed_object = (TypeName **)object;                       \
        if (typed_object != NULL && *typed_object != NULL) {                 \
            cleanup_func(*typed_object);                                     \
            *typed_object = NULL;                                            \
        }                                                                    \
    }

/**
 * @brief 为指定类型生成自动清理函数声明和实现。
 *
 * @param TypeName 类型名，通常为 typedef 名称。
 * @param cleanup_func 类型对应的释放函数。
 */
#define DEFINE_AUTOPTR_CLEANUP_FUNC(TypeName, cleanup_func)                  \
    DEFINE_AUTOPTR_CLEANUP_FUNC_IMPL(TypeName, cleanup_func)

/**
 * @brief 为指定类型生成 cleanup 属性。
 *
 * @param TypeName 类型名。
 */
#define autoptr_cleanup(TypeName) auto_cleanup(autoptr_cleanup_##TypeName)

/**
 * @brief 为自定义类型指针声明自动清理属性。
 *
 * @param TypeName 类型名，需先调用 DEFINE_AUTOPTR_CLEANUP_FUNC 定义。
 */
#define autoptr(TypeName) autoptr_cleanup(TypeName) TypeName *

/**
 * @brief 为引用计数对象声明自动 unref 属性。
 *
 * @param TypeName 类型名，需先调用 DEFINE_AUTOREF_OPS 定义。
 */
#define autoref(TypeName) autoptr(TypeName)

/**
 * @brief 初始化对象的引用计数为 1。
 *
 * @param object_ptr 对象地址，要求存在 ref_count 字段。
 */
#define AUTOREF_INIT(object_ptr)                                             \
    __atomic_store_n(&(object_ptr)->ref_count, 1, __ATOMIC_RELEASE)

/**
 * @brief 读取对象当前的引用计数。
 *
 * @param object_ptr 对象地址，要求存在 ref_count 字段。
 *
 * @return 返回当前引用计数值。
 */
#define AUTOREF_COUNT(object_ptr)                                            \
    __atomic_load_n(&(object_ptr)->ref_count, __ATOMIC_ACQUIRE)

/**
 * @brief 增加对象的引用计数。
 *
 * @param object_ptr 对象地址，要求存在 ref_count 字段。
 *
 * @return 返回增加前引用计数值。
 */
#define AUTOREF_INC(object_ptr)                                             \
    __atomic_add_fetch(&(object_ptr)->ref_count, 1, __ATOMIC_RELAXED)

/**
 * @brief 减少对象的引用计数。
 *
 * @param object_ptr 对象地址，要求存在 ref_count 字段。
 *
 * @return 返回减少后的引用计数值。
 */
#define AUTOREF_DEC(object_ptr)                                             \
    __atomic_sub_fetch(&(object_ptr)->ref_count, 1, __ATOMIC_ACQ_REL)

/**
 * @brief 为带 ref_count 字段的类型声明 ref/unref/cleanup 接口。
 *
 * @param TypeName 类型名，要求包含 ref_count 字段。
 */
#define DECLARE_AUTOREF_OPS(TypeName)                                        \
    static inline TypeName *TypeName##_ref(TypeName *object);                \
    static inline void TypeName##_unref(TypeName *object);                   \
    DECLARE_AUTOPTR_CLEANUP_FUNC(TypeName)

/**
 * @brief 为带 ref_count 字段的类型实现 ref/unref/cleanup 接口。
 *
 * @param TypeName 类型名，要求包含 ref_count 字段。
 * @param destroy_func 引用计数归零时调用的最终销毁函数。
 */
#define DEFINE_AUTOREF_OPS_IMPL(TypeName, destroy_func)                      \
    static inline TypeName *TypeName##_ref(TypeName *object)                 \
    {                                                                        \
        if (object != NULL) {                                                \
            AUTOREF_INC(object);                                             \
        }                                                                    \
        return object;                                                       \
    }                                                                        \
    \
    static inline void TypeName##_unref(TypeName *object)                    \
    {                                                                        \
        if (object != NULL &&                                                \
            AUTOREF_DEC(object) == 0) {                                      \
            destroy_func(object);                                            \
        }                                                                    \
    }                                                                        \
    \
    DEFINE_AUTOPTR_CLEANUP_FUNC_IMPL(TypeName, TypeName##_unref)

/**
 * @brief 为带 ref_count 字段的类型生成 ref/unref/cleanup 声明和实现。
 *
 * @param TypeName 类型名，要求包含 ref_count 字段。
 * @param destroy_func 引用计数归零时调用的最终销毁函数。
 */
#define DEFINE_AUTOREF_OPS(TypeName, destroy_func)                           \
    DECLARE_AUTOREF_OPS(TypeName);                                           \
    DEFINE_AUTOREF_OPS_IMPL(TypeName, destroy_func)

DEFINE_AUTOPTR_CLEANUP_FUNC(_macro_autofree_t, free)

/**
 * @brief 为堆指针声明自动 free 属性。
 */
#define autofree autoptr_cleanup(_macro_autofree_t)

static inline void _macro_scoped_mutex_unlock(pthread_mutex_t **mutex_ptr)
{
    if (mutex_ptr != NULL && *mutex_ptr != NULL) {
        pthread_mutex_unlock(*mutex_ptr);
    }
}

/**
 * @brief 以单次作用域方式持有互斥锁，离开作用域时自动解锁。
 *
 * @param mutex_addr pthread_mutex_t 对象地址。
 */
#define WITH_LOCK(mutex_addr)                                                \
    for (int __with_lock_once =                                              \
         (pthread_mutex_lock((mutex_addr)), 1);                              \
         __with_lock_once;                                                   \
         __with_lock_once = 0)                                               \
        for (pthread_mutex_t *__guard                                        \
             __attribute__((cleanup(_macro_scoped_mutex_unlock))) =          \
             (mutex_addr);                                                   \
             __with_lock_once;                                               \
             __with_lock_once = 0)


#endif /* RAII_H */
