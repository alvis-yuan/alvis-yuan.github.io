/**
 * @file atomic.h
 * @brief GCC __atomic 内置函数的用户态封装（Linux atomic_t 风格 + 全场景覆盖）
 *
 * 依赖 GCC/Clang 的 __atomic 内置函数（GCC 4.7+），语义对齐 C11 内存模型。
 * 参考：program/gcc-atomic-built-in-analysis.html
 *
 * ============================================================================
 * 两层 API
 * ============================================================================
 *
 * 1. ATOMIC_*  — 底层直封装，第一个参数为标量/指针地址，可显式传入 memorder。
 *                覆盖 __atomic 全部分类：load/store、exchange、CAS、op_fetch、
 *                fetch_op、test_and_set/clear、fence、lock-free 查询。
 *
 * 2. atomic_*  — 基于 atomic_t / atomic64_t / atomic_flag_t 的便捷宏。
 *                默认内存序已按常见场景选型；需要精细控制时用 ATOMIC_* 或
 *                *_explicit 变体。
 *
 * ============================================================================
 * 接口选型（何时用哪个）
 * ============================================================================
 *
 * | 需求                         | 选用接口                          | 说明 |
 * |------------------------------|-----------------------------------|------|
 * | 只读共享变量                 | load_n / atomic_read              | 无同步：RELAXED；消费发布数据：ACQUIRE |
 * | 只写共享变量                 | store_n / atomic_set              | 无同步：RELAXED；发布数据：RELEASE |
 * | 读任意类型（含 struct）      | ATOMIC_LOAD                       | 大对象原子快照；可能退化为锁 |
 * | 写任意类型                   | ATOMIC_STORE                      | 同上 |
 * | 无条件读-改-写，要旧值       | fetch_add/sub/and/or/xor/nand     | ID 分配、信号量 V、引用计数减（看旧值） |
 * | 无条件读-改-写，要新值       | add/sub/and/or/xor/nand _fetch    | 减到 0 触发释放、阈值判断 |
 * | 条件更新（CAS 循环）         | compare_exchange_n(_weak)         | 无锁栈/队列、乐观锁、一次性初始化 |
 * | 整型锁变量抢锁               | exchange_n(ACQUIRE) + store(RELEASE) | 自旋锁方案 1 |
 * | bool/char 锁标志             | test_and_set + clear              | 自旋锁方案 2，最简 |
 * | 整型锁变量 CAS 抢锁          | compare_exchange_n                | 自旋锁方案 3，可感知竞争 |
 * | 引用计数 +1                  | fetch_add / add_fetch (RELAXED)   | 只需原子性 |
 * | 引用计数 -1 并可能销毁       | fetch_sub(RELEASE)+fence(ACQUIRE) | 见 atomic_ref_release |
 * | 统计计数无顺序要求           | *_RELAXED 系列                    | 性能最优 |
 * | 发布-订阅（写数据再写标志）  | store RELEASE + load ACQUIRE      | 或 fence + RELAXED |
 * | 独立内存屏障                 | thread_fence                      | 引用计数归零、fence+relaxed 模式 |
 * | 信号处理器与主线程           | signal_fence                      | 仅编译器屏障，同线程 |
 * | 编译期是否无锁               | always_lock_free                  | 条件编译、static_assert |
 * | 运行期是否无锁               | is_lock_free                      | 动态类型、库内部 |
 *
 * op_fetch（返回新值） vs fetch_op（返回旧值）：
 *   - 关心操作**之后**的值 → *_fetch（如 sub_fetch == 0 则释放）
 *   - 关心操作**之前**的值 → fetch_*（如 fetch_sub == 1 表示最后一个引用）
 *
 * CAS weak vs strong：
 *   - 循环体中优先 weak（允许伪失败，ARM 更高效）
 *   - 单次语义必须成功才继续 → strong
 *
 * 内存序原则：从 RELAXED 起选，仅在需要 happens-before 时增强为
 * ACQUIRE/RELEASE/ACQ_REL；不确定时用 SEQ_CST（默认 *_return / cmpxchg）。
 *
 * @note 禁止直接读写 atomic_t.counter；禁止用 volatile 替代原子或锁。
 * @note atomic_test_and_set(v) 仅测试整型计数是否非零，不是 GCC test-and-set；
 *       自旋锁请用 atomic_flag_test_and_set / atomic_flag_clear。
 */

#ifndef _USER_ATOMIC_H
#define _USER_ATOMIC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * 内存序别名（与 GCC __ATOMIC_* 等价，便于文档引用）
 * ============================================================================ */

/** @defgroup atomic_memorder 内存序
 * 约束强度：RELAXED < CONSUME ≈ ACQUIRE < RELEASE < ACQ_REL < SEQ_CST
 * @{
 */
#define ATOMIC_RELAXED  __ATOMIC_RELAXED
#define ATOMIC_CONSUME  __ATOMIC_CONSUME
#define ATOMIC_ACQUIRE  __ATOMIC_ACQUIRE
#define ATOMIC_RELEASE  __ATOMIC_RELEASE
#define ATOMIC_ACQ_REL  __ATOMIC_ACQ_REL
#define ATOMIC_SEQ_CST  __ATOMIC_SEQ_CST
/** @} */

/* ============================================================================
 * 类型
 * ============================================================================ */

/** @brief 32 位整型原子计数器（最常用共享计数/标志） */
typedef struct {
    int counter;
} atomic_t;

/** @brief 64 位整型原子计数器（大范围计数、部分 64 位统计） */
typedef struct {
    int64_t counter;
} atomic64_t;

/** @brief 指针宽度无符号原子（无锁栈顶、原子指针替换） */
typedef struct {
    uintptr_t value;
} atomic_ulong_t;

/** @brief 自旋锁标志（bool，配合 test_and_set / clear） */
typedef bool atomic_flag_t;

#define ATOMIC_INIT(i)       { (i) }
#define ATOMIC64_INIT(i)     { (i) }
#define ATOMIC_ULONG_INIT(v) { (uintptr_t)(v) }
#define ATOMIC_FLAG_INIT     false

/** @internal 取 atomic_t 底层地址 */
#define _ATOMIC_COUNTER_PTR(v) (&(v)->counter)

/** @internal 取 atomic64_t 底层地址 */
#define _ATOMIC64_COUNTER_PTR(v) (&(v)->counter)

/** @internal 取 atomic_ulong_t 底层地址 */
#define _ATOMIC_ULONG_PTR(v) (&(v)->value)

/* ============================================================================
 * 底层 ATOMIC_* 宏 — 直映射 GCC __atomic（可指定 memorder）
 * ============================================================================ */

/** @defgroup atomic_lowlevel 底层 __atomic 直封装
 * 参数 ptr 为标量或指针的地址；适用于 atomic_t 之外的裸变量与任意类型。
 * @{
 */

/** 原子加载，返回值。有效 order：RELAXED/CONSUME/ACQUIRE/SEQ_CST */
#define ATOMIC_LOAD_N(ptr, order) \
    __atomic_load_n((ptr), (order))

/** 泛型原子加载，结果写入 ret。适用于 struct 等任意可拷贝类型 */
#define ATOMIC_LOAD(ptr, ret, order) \
    __atomic_load((ptr), (ret), (order))

/** 原子存储。有效 order：RELAXED/RELEASE/SEQ_CST */
#define ATOMIC_STORE_N(ptr, val, order) \
    __atomic_store_n((ptr), (val), (order))

/** 泛型原子存储，源值为 val 指针 */
#define ATOMIC_STORE(ptr, val, order) \
    __atomic_store((ptr), (val), (order))

/** 原子交换：写入 val，返回旧值。RMW，全 memorder 有效 */
#define ATOMIC_EXCHANGE_N(ptr, val, order) \
    __atomic_exchange_n((ptr), (val), (order))

/** 泛型原子交换 */
#define ATOMIC_EXCHANGE(ptr, val, ret, order) \
    __atomic_exchange((ptr), (val), (ret), (order))

/** CAS strong：相等则写入 desired；expected 失败时被更新为当前值 */
#define ATOMIC_CMPXCHG_STRONG_N(ptr, expected, desired, succ, fail) \
    __atomic_compare_exchange_n( \
        (ptr), (expected), (desired), 0, (succ), (fail))

/** CAS weak：循环中优先使用 */
#define ATOMIC_CMPXCHG_WEAK_N(ptr, expected, desired, succ, fail) \
    __atomic_compare_exchange_n( \
        (ptr), (expected), (desired), 1, (succ), (fail))

/** 泛型 CAS，desired 为指针 */
#define ATOMIC_CMPXCHG_STRONG(ptr, expected, desired, succ, fail) \
    __atomic_compare_exchange( \
        (ptr), (expected), (desired), 0, (succ), (fail))

#define ATOMIC_CMPXCHG_WEAK(ptr, expected, desired, succ, fail) \
    __atomic_compare_exchange( \
        (ptr), (expected), (desired), 1, (succ), (fail))

/** op_fetch：*ptr op= val; return *ptr（返回新值） */
#define ATOMIC_ADD_FETCH(ptr, val, order) \
    __atomic_add_fetch((ptr), (val), (order))
#define ATOMIC_SUB_FETCH(ptr, val, order) \
    __atomic_sub_fetch((ptr), (val), (order))
#define ATOMIC_AND_FETCH(ptr, val, order) \
    __atomic_and_fetch((ptr), (val), (order))
#define ATOMIC_OR_FETCH(ptr, val, order) \
    __atomic_or_fetch((ptr), (val), (order))
#define ATOMIC_XOR_FETCH(ptr, val, order) \
    __atomic_xor_fetch((ptr), (val), (order))
#define ATOMIC_NAND_FETCH(ptr, val, order) \
    __atomic_nand_fetch((ptr), (val), (order))

/** fetch_op：tmp = *ptr; *ptr op= val; return tmp（返回旧值） */
#define ATOMIC_FETCH_ADD(ptr, val, order) \
    __atomic_fetch_add((ptr), (val), (order))
#define ATOMIC_FETCH_SUB(ptr, val, order) \
    __atomic_fetch_sub((ptr), (val), (order))
#define ATOMIC_FETCH_AND(ptr, val, order) \
    __atomic_fetch_and((ptr), (val), (order))
#define ATOMIC_FETCH_OR(ptr, val, order) \
    __atomic_fetch_or((ptr), (val), (order))
#define ATOMIC_FETCH_XOR(ptr, val, order) \
    __atomic_fetch_xor((ptr), (val), (order))
#define ATOMIC_FETCH_NAND(ptr, val, order) \
    __atomic_fetch_nand((ptr), (val), (order))

/** test-and-set / clear：仅用于 bool 或 char，实现自旋锁 */
#define ATOMIC_FLAG_TEST_AND_SET(ptr, order) \
    __atomic_test_and_set((void *)(ptr), (order))

#define ATOMIC_FLAG_CLEAR(ptr, order) \
    __atomic_clear((bool *)(ptr), (order))

/** 线程间 fence；须与具体原子位置配合才能建立 happens-before */
#define ATOMIC_THREAD_FENCE(order) \
    __atomic_thread_fence(order)

/** 线程与信号处理器间 fence（仅编译器屏障） */
#define ATOMIC_SIGNAL_FENCE(order) \
    __atomic_signal_fence(order)

/** 编译期：给定字节数在此目标上是否总是无锁 */
#define ATOMIC_ALWAYS_LOCK_FREE(size) \
    __atomic_always_lock_free((size), 0)

/** 运行期：给定字节数是否无锁（不确定时调运行时例程） */
#define ATOMIC_IS_LOCK_FREE(size) \
    __atomic_is_lock_free((size), 0)

/** 给定对象是否无锁（含对齐因素） */
#define ATOMIC_IS_LOCK_FREE_OBJ(obj) \
    __atomic_is_lock_free(sizeof(*(obj)), (obj))

/** @} */ /* atomic_lowlevel */

/* ============================================================================
 * atomic_t 读 / 写
 * ============================================================================ */

/** @defgroup atomic_load_store 加载与存储（atomic_t）
 * @{
 *
 * | 宏 | 默认 order | 场景 |
 * |----|------------|------|
 * | atomic_read | RELAXED | 统计、无顺序依赖的读取 |
 * | atomic_read_acquire | ACQUIRE | 读取发布标志后访问关联数据 |
 * | atomic_set | RELAXED | 普通写入 |
 * | atomic_set_release | RELEASE | 写完 payload 后发布标志 |
 */

#define atomic_read(v) \
    ATOMIC_LOAD_N(_ATOMIC_COUNTER_PTR(v), ATOMIC_RELAXED)

#define atomic_read_acquire(v) \
    ATOMIC_LOAD_N(_ATOMIC_COUNTER_PTR(v), ATOMIC_ACQUIRE)

#define atomic_read_explicit(v, order) \
    ATOMIC_LOAD_N(_ATOMIC_COUNTER_PTR(v), (order))

#define atomic_set(v, i) \
    ATOMIC_STORE_N(_ATOMIC_COUNTER_PTR(v), (i), ATOMIC_RELAXED)

#define atomic_set_release(v, i) \
    ATOMIC_STORE_N(_ATOMIC_COUNTER_PTR(v), (i), ATOMIC_RELEASE)

#define atomic_set_explicit(v, i, order) \
    ATOMIC_STORE_N(_ATOMIC_COUNTER_PTR(v), (i), (order))

/** @} */

/* ============================================================================
 * atomic_t 算术 / 位运算 — 无返回值（RELAXED，纯计数）
 * ============================================================================ */

/** @defgroup atomic_rmw_relaxed 读-改-写（无返回值，RELAXED）
 * 场景：连接数、字节统计等只需原子性、不需顺序约束的计数。
 * @{
 */

#define atomic_add(i, v) \
    (void)ATOMIC_ADD_FETCH(_ATOMIC_COUNTER_PTR(v), (i), ATOMIC_RELAXED)

#define atomic_sub(i, v) \
    (void)ATOMIC_SUB_FETCH(_ATOMIC_COUNTER_PTR(v), (i), ATOMIC_RELAXED)

#define atomic_inc(v) atomic_add(1, v)
#define atomic_dec(v) atomic_sub(1, v)

#define atomic_and(i, v) \
    (void)ATOMIC_AND_FETCH(_ATOMIC_COUNTER_PTR(v), (i), ATOMIC_RELAXED)

#define atomic_or(i, v) \
    (void)ATOMIC_OR_FETCH(_ATOMIC_COUNTER_PTR(v), (i), ATOMIC_RELAXED)

#define atomic_xor(i, v) \
    (void)ATOMIC_XOR_FETCH(_ATOMIC_COUNTER_PTR(v), (i), ATOMIC_RELAXED)

#define atomic_nand(i, v) \
    (void)ATOMIC_NAND_FETCH(_ATOMIC_COUNTER_PTR(v), (i), ATOMIC_RELAXED)

/** @} */

/* ============================================================================
 * atomic_t 算术 / 位运算 — 返回新值（SEQ_CST 默认，便于阈值判断）
 * ============================================================================ */

/** @defgroup atomic_op_fetch 返回新值（op_fetch）
 * 场景：减 1 后是否为 0、加后是否越界、位操作后检查新状态。
 * 默认 SEQ_CST；需要 RELEASE 释放语义时用 *_explicit。
 * @{
 */

#define atomic_add_return(i, v) \
    ATOMIC_ADD_FETCH(_ATOMIC_COUNTER_PTR(v), (i), ATOMIC_SEQ_CST)

#define atomic_sub_return(i, v) \
    ATOMIC_SUB_FETCH(_ATOMIC_COUNTER_PTR(v), (i), ATOMIC_SEQ_CST)

#define atomic_inc_return(v) atomic_add_return(1, v)
#define atomic_dec_return(v) atomic_sub_return(1, v)

#define atomic_and_return(i, v) \
    ATOMIC_AND_FETCH(_ATOMIC_COUNTER_PTR(v), (i), ATOMIC_SEQ_CST)

#define atomic_or_return(i, v) \
    ATOMIC_OR_FETCH(_ATOMIC_COUNTER_PTR(v), (i), ATOMIC_SEQ_CST)

#define atomic_xor_return(i, v) \
    ATOMIC_XOR_FETCH(_ATOMIC_COUNTER_PTR(v), (i), ATOMIC_SEQ_CST)

#define atomic_nand_return(i, v) \
    ATOMIC_NAND_FETCH(_ATOMIC_COUNTER_PTR(v), (i), ATOMIC_SEQ_CST)

#define atomic_add_return_explicit(i, v, order) \
    ATOMIC_ADD_FETCH(_ATOMIC_COUNTER_PTR(v), (i), (order))

#define atomic_sub_return_explicit(i, v, order) \
    ATOMIC_SUB_FETCH(_ATOMIC_COUNTER_PTR(v), (i), (order))

/** @} */

/* ============================================================================
 * atomic_t 算术 / 位运算 — 返回旧值（SEQ_CST 默认）
 * ============================================================================ */

/** @defgroup atomic_fetch_op 返回旧值（fetch_op）
 * 场景：fetch_add 分配唯一 ID；fetch_sub 引用计数（旧值==1 表示最后一个）；
 *       fetch_and/or 检查操作前位状态。
 * @{
 */

#define atomic_fetch_add(i, v) \
    ATOMIC_FETCH_ADD(_ATOMIC_COUNTER_PTR(v), (i), ATOMIC_SEQ_CST)

#define atomic_fetch_sub(i, v) \
    ATOMIC_FETCH_SUB(_ATOMIC_COUNTER_PTR(v), (i), ATOMIC_SEQ_CST)

#define atomic_fetch_inc(v) atomic_fetch_add(1, v)
#define atomic_fetch_dec(v) atomic_fetch_sub(1, v)

#define atomic_fetch_and(i, v) \
    ATOMIC_FETCH_AND(_ATOMIC_COUNTER_PTR(v), (i), ATOMIC_SEQ_CST)

#define atomic_fetch_or(i, v) \
    ATOMIC_FETCH_OR(_ATOMIC_COUNTER_PTR(v), (i), ATOMIC_SEQ_CST)

#define atomic_fetch_xor(i, v) \
    ATOMIC_FETCH_XOR(_ATOMIC_COUNTER_PTR(v), (i), ATOMIC_SEQ_CST)

#define atomic_fetch_nand(i, v) \
    ATOMIC_FETCH_NAND(_ATOMIC_COUNTER_PTR(v), (i), ATOMIC_SEQ_CST)

#define atomic_fetch_add_explicit(i, v, order) \
    ATOMIC_FETCH_ADD(_ATOMIC_COUNTER_PTR(v), (i), (order))

#define atomic_fetch_sub_explicit(i, v, order) \
    ATOMIC_FETCH_SUB(_ATOMIC_COUNTER_PTR(v), (i), (order))

/** @} */

/* ============================================================================
 * 比较交换 / 交换
 * ============================================================================ */

/** @defgroup atomic_cas_exchange 比较交换与交换
 * cmpxchg：无锁数据结构、一次性启动标志、状态机转换。
 * xchg：自旋锁（exchange_n 抢锁 + store 释放）、原子替换指针。
 *
 * atomic_cmpxchg(v, old, new)：old 必须为左值，失败时更新为当前值。
 * @{
 */

#define atomic_cmpxchg(v, old, new_val) \
    ATOMIC_CMPXCHG_STRONG_N( \
        _ATOMIC_COUNTER_PTR(v), &(old), (new_val), ATOMIC_SEQ_CST, ATOMIC_RELAXED)

#define atomic_cmpxchg_weak(v, old, new_val) \
    ATOMIC_CMPXCHG_WEAK_N( \
        _ATOMIC_COUNTER_PTR(v), &(old), (new_val), ATOMIC_SEQ_CST, ATOMIC_RELAXED)

#define atomic_cmpxchg_explicit(v, old, new_val, succ, fail) \
    ATOMIC_CMPXCHG_STRONG_N( \
        _ATOMIC_COUNTER_PTR(v), &(old), (new_val), (succ), (fail))

#define atomic_cmpxchg_weak_explicit(v, old, new_val, succ, fail) \
    ATOMIC_CMPXCHG_WEAK_N( \
        _ATOMIC_COUNTER_PTR(v), &(old), (new_val), (succ), (fail))

#define atomic_xchg(v, new_val) \
    ATOMIC_EXCHANGE_N(_ATOMIC_COUNTER_PTR(v), (new_val), ATOMIC_SEQ_CST)

#define atomic_xchg_explicit(v, new_val, order) \
    ATOMIC_EXCHANGE_N(_ATOMIC_COUNTER_PTR(v), (new_val), (order))

/** @} */

/* ============================================================================
 * 复合测试
 * ============================================================================ */

/** @defgroup atomic_test 复合测试宏
 * @{
 */

#define atomic_dec_and_test(v) (atomic_sub_return(1, v) == 0)
#define atomic_inc_and_test(v) (atomic_add_return(1, v) == 0)
#define atomic_sub_and_test(i, v) (atomic_sub_return(i, v) == 0)
#define atomic_add_and_test(i, v) (atomic_add_return(i, v) == 0)

/** 测试整型计数是否非零（非 GCC test-and-set，勿用于自旋锁） */
#define atomic_non_zero(v) (atomic_read(v) != 0)

/** 与 atomic_non_zero 相同；历史命名，易与 atomic_flag_test_and_set 混淆 */
#define atomic_test_and_set(v) atomic_non_zero(v)

/** @} */

/* ============================================================================
 * 引用计数释放模式（fetch_sub RELEASE + fence ACQUIRE）
 * ============================================================================ */

/** @defgroup atomic_refcount 引用计数
 * retain：fetch_add / add_fetch RELAXED 即可。
 * release：fetch_sub RELEASE，若旧值为 1 则 thread_fence ACQUIRE 后销毁。
 * @{
 */

#define atomic_thread_fence_acquire() ATOMIC_THREAD_FENCE(ATOMIC_ACQUIRE)
#define atomic_thread_fence_release() ATOMIC_THREAD_FENCE(ATOMIC_RELEASE)
#define atomic_thread_fence_seq_cst() ATOMIC_THREAD_FENCE(ATOMIC_SEQ_CST)

/**
 * @brief 引用计数减 1；若减前值为 1（最后一个引用），执行 ACQUIRE fence 并调用 destroy
 * @param v atomic_t 指针
 * @param destroy 销毁回调，可为函数调用表达式
 *
 * 等价于：
 *   if (atomic_fetch_sub_explicit(1, v, RELEASE) == 1) { fence(ACQUIRE); destroy; }
 */
#define atomic_ref_release(v, destroy) \
    do { \
        if (atomic_fetch_sub_explicit(1, (v), ATOMIC_RELEASE) == 1) { \
            atomic_thread_fence_acquire(); \
            (destroy); \
        } \
    } while (0)

/** @} */

/* ============================================================================
 * atomic_flag_t — 自旋锁（test_and_set / clear）
 * ============================================================================ */

/** @defgroup atomic_flag 自旋锁标志
 * 仅用于 bool/char。加锁：test_and_set ACQUIRE 返回 true 表示已占用需自旋；
 * 解锁：clear RELEASE。
 * @{
 */

#define atomic_flag_test_and_set(flag) \
    ATOMIC_FLAG_TEST_AND_SET((flag), ATOMIC_ACQUIRE)

#define atomic_flag_test_and_set_explicit(flag, order) \
    ATOMIC_FLAG_TEST_AND_SET((flag), (order))

#define atomic_flag_clear(flag) \
    ATOMIC_FLAG_CLEAR((flag), ATOMIC_RELEASE)

#define atomic_flag_clear_explicit(flag, order) \
    ATOMIC_FLAG_CLEAR((flag), (order))

/** 自旋直至获得锁 */
#define atomic_spin_lock(flag) \
    do { \
        while (atomic_flag_test_and_set(flag)) { \
        } \
    } while (0)

/** 释放锁 */
#define atomic_spin_unlock(flag) atomic_flag_clear(flag)

/** @} */

/* ============================================================================
 * atomic64_t — 64 位计数（接口子集，语义同 atomic_t）
 * ============================================================================ */

/** @defgroup atomic64 64 位原子计数
 * 用于 int64_t 范围统计；在 32 位平台上可能非无锁，可用 ATOMIC_ALWAYS_LOCK_FREE 检查。
 * @{
 */

#define atomic64_read(v) \
    ATOMIC_LOAD_N(_ATOMIC64_COUNTER_PTR(v), ATOMIC_RELAXED)

#define atomic64_set(v, i) \
    ATOMIC_STORE_N(_ATOMIC64_COUNTER_PTR(v), (i), ATOMIC_RELAXED)

#define atomic64_inc(v) \
    (void)ATOMIC_ADD_FETCH(_ATOMIC64_COUNTER_PTR(v), 1, ATOMIC_RELAXED)

#define atomic64_dec(v) \
    (void)ATOMIC_SUB_FETCH(_ATOMIC64_COUNTER_PTR(v), 1, ATOMIC_RELAXED)

#define atomic64_add_return(i, v) \
    ATOMIC_ADD_FETCH(_ATOMIC64_COUNTER_PTR(v), (i), ATOMIC_SEQ_CST)

#define atomic64_fetch_add(i, v) \
    ATOMIC_FETCH_ADD(_ATOMIC64_COUNTER_PTR(v), (i), ATOMIC_SEQ_CST)

#define atomic64_cmpxchg(v, old, new_val) \
    ATOMIC_CMPXCHG_STRONG_N( \
        _ATOMIC64_COUNTER_PTR(v), &(old), (new_val), ATOMIC_SEQ_CST, ATOMIC_RELAXED)

/** @} */

/* ============================================================================
 * atomic_ulong_t — 指针宽度原子（无锁栈顶、原子指针）
 * ============================================================================ */

/** @defgroup atomic_ulong 指针宽度原子
 * 场景：无锁链表栈顶、单次指针发布。值为 uintptr_t，存指针时强转。
 * @{
 */

#define atomic_ulong_read(v) \
    ATOMIC_LOAD_N(_ATOMIC_ULONG_PTR(v), ATOMIC_RELAXED)

#define atomic_ulong_read_acquire(v) \
    ATOMIC_LOAD_N(_ATOMIC_ULONG_PTR(v), ATOMIC_ACQUIRE)

#define atomic_ulong_set_release(v, val) \
    ATOMIC_STORE_N(_ATOMIC_ULONG_PTR(v), (uintptr_t)(val), ATOMIC_RELEASE)

#define atomic_ulong_xchg(v, val) \
    ATOMIC_EXCHANGE_N(_ATOMIC_ULONG_PTR(v), (uintptr_t)(val), ATOMIC_SEQ_CST)

#define atomic_ulong_cmpxchg(v, old, new_val) \
    ATOMIC_CMPXCHG_STRONG_N( \
        _ATOMIC_ULONG_PTR(v), &(old), (uintptr_t)(new_val), \
        ATOMIC_SEQ_CST, ATOMIC_RELAXED)

#define atomic_ulong_cmpxchg_weak(v, old, new_val) \
    ATOMIC_CMPXCHG_WEAK_N( \
        _ATOMIC_ULONG_PTR(v), &(old), (uintptr_t)(new_val), \
        ATOMIC_SEQ_CST, ATOMIC_RELAXED)

/** @} */

/* ============================================================================
 * 无锁能力查询便捷宏
 * ============================================================================ */

/** @defgroup atomic_lockfree 无锁能力查询
 * always_lock_free：编译期常量 size，用于 #if / _Static_assert。
 * is_lock_free：可运行期 size，库内部动态判断。
 * @{
 */

#define atomic_always_lock_free(size) ATOMIC_ALWAYS_LOCK_FREE(size)
#define atomic_is_lock_free(size)     ATOMIC_IS_LOCK_FREE(size)

/** 常用：int / int64_t / 指针 在本目标上是否保证无锁 */
#define atomic_int_always_lock_free() \
    ATOMIC_ALWAYS_LOCK_FREE(sizeof(int))

#define atomic_ptr_always_lock_free() \
    ATOMIC_ALWAYS_LOCK_FREE(sizeof(void *))

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* _USER_ATOMIC_H */
