/*
 * compile_checks.h — 用户态编译期检查工具集
 *
 * 基于：
 *   - GCC 官方文档: https://gcc.gnu.org/onlinedocs/gcc/Common-Function-Attributes.html
 *   - Linux kernel: include/linux/build_bug.h
 *                   include/linux/compiler_attributes.h
 *                   tools/include/linux/compiler.h
 *   - Linux kernel: https://github.com/torvalds/linux/blob/master/include/linux/build_bug.h
 *
 * 要求：gcc -std=gnu99 -O1 (或 -O2)
 *       对于 static_assert：gcc >= 4.6
 *       对于 compiletime_assert：gcc >= 4.3 且 __OPTIMIZE__
 */

#ifndef COMPILE_CHECKS_H
#define COMPILE_CHECKS_H

#include <stddef.h>  /* offsetof, size_t */

/* ── 1. 函数属性检查 ─────────────────────────────────── */

/* 指定参数不得为 NULL（1-indexed，不带参数=所有指针参数）
 * GCC: https://gcc.gnu.org/onlinedocs/gcc/Common-Function-Attributes.html
 */
#define __nonnull(...)    __attribute__((__nonnull__(__VA_ARGS__)))
#define __nonnull_all     __attribute__((__nonnull__))

/* 格式字符串检查
 * GCC: https://gcc.gnu.org/onlinedocs/gcc/Common-Function-Attributes.html
 */
#define __printf(fmt, args)  __attribute__((__format__(__printf__, fmt, args)))
#define __scanf(fmt, args)   __attribute__((__format__(__scanf__,  fmt, args)))

/* 变参哨兵检查
 * GCC: https://gcc.gnu.org/onlinedocs/gcc/Common-Function-Attributes.html
 */
#define __sentinel          __attribute__((__sentinel__))
#define __sentinel_at(pos)  __attribute__((__sentinel__(pos)))

/* 返回值必须被使用
 * kernel: include/linux/compiler_attributes.h
 * GCC: https://gcc.gnu.org/onlinedocs/gcc/Common-Function-Attributes.html
 */
#define __must_check        __attribute__((__warn_unused_result__))

/* 废弃声明
 * GCC: https://gcc.gnu.org/onlinedocs/gcc/Common-Function-Attributes.html
 */
#define __deprecated        __attribute__((__deprecated__))
#define __deprecated_msg(m) __attribute__((__deprecated__(m)))

/* 编译期 error/warning 属性（GCC >= 4.3）
 * kernel: include/linux/compiler_attributes.h
 * GCC: https://gcc.gnu.org/onlinedocs/gcc/Common-Function-Attributes.html
 */
#if defined(__GNUC__) && \
    (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 3))
# define __compiletime_error(msg) __attribute__((__error__(msg)))
# define __compiletime_warning(msg) __attribute__((__warning__(msg)))
#else
# define __compiletime_error(msg)
# define __compiletime_warning(msg)
#endif

#ifndef __noreturn
#define __noreturn __attribute__((__noreturn__))
#endif

/* ── 2. 类型检查与类型安全宏 ──────────────────────────── */

/* 两个变量/表达式是否是同一类型（忽略 const/volatile）
 * kernel: tools/include/linux/compiler.h
 * GCC: https://gcc.gnu.org/onlinedocs/gcc/Other-Builtins.html
 */
#define __same_type(a, b) \
    __builtin_types_compatible_p(__typeof__(a), __typeof__(b))

/* 类型安全的 swap 宏（内核风格）*/
#define swap(a, b)                              \
    do {                                        \
        __typeof__(a) __tmp = (a);              \
        (a) = (b); (b) = __tmp;                 \
    } while (0)

/* ── 3. 静态断言（compiletime_assert 机制）─────────────── */

/* kernel: tools/include/linux/compiler.h */
#ifdef __OPTIMIZE__
# define __compiletime_assert(condition, msg, prefix, suffix) \
    do {                                                        \
        __noreturn extern void prefix##suffix(void)           \
            __compiletime_error(msg);                           \
        if (!(condition))                                       \
            prefix##suffix();                                   \
    } while (0)
#else
# define __compiletime_assert(condition, msg, prefix, suffix) \
    do {} while (0)
#endif

#define _compiletime_assert(condition, msg, prefix, suffix) \
    __compiletime_assert(condition, msg, prefix, suffix)

#define compiletime_assert(condition, msg) \
    _compiletime_assert(condition, msg, __compiletime_assert_, __COUNTER__)

/* ── 4. BUILD_BUG_ON 宏族 ────────────────────────────── */

/* kernel: include/linux/build_bug.h */

/* 条件为真时编译错误，结果为 int 0（可用于表达式上下文）*/
#define BUILD_BUG_ON_ZERO(e) \
    ((int)(sizeof(struct { int:(-!!(e)); })))

/*
 * BUILD_BUG_ON_INVALID() permits the compiler to check the validity of the
 * expression but avoids the generation of any code, even if that expression
 * has side-effects.
 */
#define BUILD_BUG_ON_INVALID(e) ((void)(sizeof((long)(e))))

/* 条件为真时编译错误（带自定义消息，语句形式）*/
#define BUILD_BUG_ON_MSG(cond, msg) compiletime_assert(!(cond), msg)

/* 条件为真时编译错误（语句形式）*/
#define BUILD_BUG_ON(condition) \
    BUILD_BUG_ON_MSG(condition, "BUILD_BUG_ON failed: " #condition)

/* 总是编译错误（标记不应执行的死代码路径）*/
#define BUILD_BUG() BUILD_BUG_ON_MSG(1, "BUILD_BUG failed")

/* n 必须是 2 的幂 */
#define BUILD_BUG_ON_NOT_POWER_OF_2(n) \
    BUILD_BUG_ON((n) == 0 || (((n) & ((n) - 1)) != 0))

/* 编译期断言两表达式类型相同 */
#define ASSERT_SAME_TYPE(a, b) \
    BUILD_BUG_ON(!__same_type(a, b))

/* ── 5. static_assert 封装 ───────────────────────────── */

/* kernel: include/linux/build_bug.h
 * 注：_Static_assert 自 GCC 4.6 在 gnu99 下即可用
 */
#if defined(__GNUC__) && \
    (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6))
# define static_assert(expr, ...) __static_assert(expr, ##__VA_ARGS__, #expr)
# define __static_assert(expr, msg, ...) _Static_assert(expr, msg)
#endif

/* ── 6. 结构体字段偏移检查 ───────────────────────────── */

/* kernel: include/linux/build_bug.h */
#define ASSERT_STRUCT_OFFSET(type, field, expected_offset)       \
    BUILD_BUG_ON_MSG(offsetof(type, field) != (expected_offset),  \
        "Offset of " #field " in " #type " has changed.")

/* ── 7. 安全的 ARRAY_SIZE ────────────────────────────── */

#define ARRAY_SIZE(arr) \
    (sizeof(arr) / sizeof((arr)[0])                             \
     + BUILD_BUG_ON_ZERO(__same_type(arr, &(arr)[0])))
/* 若 arr 是指针（非数组），编译报错 */

#endif /* COMPILE_CHECKS_H */
