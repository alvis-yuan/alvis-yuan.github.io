/*
 * user_kfifo.h — User-space SPSC lock-free ring buffer
 *
 * Design based on Linux kernel kfifo (include/linux/kfifo.h, lib/kfifo.c)
 * Copyright (C) 2013 Stefani Seibold <stefani@seibold.net>
 * Original source: https://github.com/torvalds/linux/blob/master/include/linux/kfifo.h
 *
 * User-space adaptation using C11 stdatomic.h
 * - Single Producer, Single Consumer (SPSC) lock-free
 * - in/out indices never reset, rely on unsigned wrap-around
 * - size MUST be a power of 2
 * - Uses acquire/release semantics (matches kernel smp_rmb/smp_wmb)
 */
#ifndef USER_KFIFO_H
#define USER_KFIFO_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdatomic.h>  /* C11 — 替代内核 smp_wmb/smp_rmb */
#include <assert.h>

/* ────────────────────────────────────────────────────────────
 * 判断是否为 2 的幂（对应内核 is_power_of_2）
 * ──────────────────────────────────────────────────────────── */
#define KFIFO_IS_POWER_OF_2(n)  ((n) != 0 && (((n) & ((n) - 1)) == 0))

/* ────────────────────────────────────────────────────────────
 * 核心结构体（对应内核 struct __kfifo）
 *
 * 关键设计：in/out 使用 _Atomic，保证跨线程可见性；
 * 使用 C11 acquire/release 语义，精确匹配内核 smp_wmb/smp_rmb
 * ──────────────────────────────────────────────────────────── */
struct user_kfifo {
    _Atomic unsigned int  in;    /* 写入偏移（生产者更新）*/
    _Atomic unsigned int  out;   /* 读取偏移（消费者更新）*/
    unsigned int           mask;  /* size - 1（只读）*/
    unsigned int           esize; /* 元素字节大小（只读）*/
    void                  *data;  /* 数据缓冲区（只读）*/
};

/* ────────────────────────────────────────────────────────────
 * 初始化（对应内核 __kfifo_init）
 *
 * @fifo  : 指向 user_kfifo 结构的指针
 * @buf   : 用户提供的缓冲区（大小必须为 2 的幂次 * esize）
 * @size  : 缓冲区能容纳的元素个数（必须是 2 的幂）
 * @esize : 每个元素的字节大小
 * 返回 0 成功，-1 失败
 * ──────────────────────────────────────────────────────────── */
static inline int
user_kfifo_init(struct user_kfifo *fifo, void *buf,
                unsigned int size, unsigned int esize)
{
    if (!KFIFO_IS_POWER_OF_2(size) || size < 2 || !buf)
        return -1;
    atomic_init(&fifo->in,  0);
    atomic_init(&fifo->out, 0);
    fifo->mask  = size - 1;
    fifo->esize = esize;
    fifo->data  = buf;
    return 0;
}

/* ────────────────────────────────────────────────────────────
 * 动态分配（对应内核 kfifo_alloc）
 * ──────────────────────────────────────────────────────────── */
static inline int
user_kfifo_alloc(struct user_kfifo *fifo,
                  unsigned int size, unsigned int esize)
{
    /* roundup to next power of 2（对应内核 roundup_pow_of_two）*/
    if (!KFIFO_IS_POWER_OF_2(size)) {
        size--;
        size |= size >> 1;  size |= size >> 2;
        size |= size >> 4;  size |= size >> 8;
        size |= size >> 16; size++;
    }
    void *buf = malloc((size_t)size * esize);
    if (!buf) return -1;
    return user_kfifo_init(fifo, buf, size, esize);
}

/* 释放动态分配的缓冲区 */
static inline void
user_kfifo_free(struct user_kfifo *fifo)
{
    free(fifo->data);
    fifo->data = NULL;
    fifo->mask = fifo->esize = 0;
}

/* ────────────────────────────────────────────────────────────
 * 内部查询辅助
 * ──────────────────────────────────────────────────────────── */

/* 当前已使用的元素数（对应 kfifo_len）*/
static inline unsigned int
user_kfifo_len(const struct user_kfifo *fifo)
{
    /* 无符号减法：自然处理 wrap-around，与内核完全一致 */
    return atomic_load_explicit(&fifo->in, memory_order_acquire)
         - atomic_load_explicit(&fifo->out, memory_order_acquire);
}

/* 缓冲区总容量（元素数）*/
static inline unsigned int
user_kfifo_size(const struct user_kfifo *fifo) { return fifo->mask + 1; }

/* 剩余可写空间（元素数，对应 kfifo_avail）*/
static inline unsigned int
user_kfifo_avail(const struct user_kfifo *fifo)
{
    return user_kfifo_size(fifo) - user_kfifo_len(fifo);
}

/* 是否为空（对应 kfifo_is_empty）*/
static inline int
user_kfifo_is_empty(const struct user_kfifo *fifo)
{
    return atomic_load_explicit(&fifo->in, memory_order_acquire)
        == atomic_load_explicit(&fifo->out, memory_order_acquire);
}

/* 是否已满（对应 kfifo_is_full）*/
static inline int
user_kfifo_is_full(const struct user_kfifo *fifo)
{
    return user_kfifo_len(fifo) == user_kfifo_size(fifo);
}

/* ────────────────────────────────────────────────────────────
 * 内部：将 src 复制到环形缓冲区的 off 位置（对应 kfifo_copy_in）
 *
 * 对应内核实现：
 *   static void kfifo_copy_in(struct __kfifo *fifo, const void *src,
 *                             unsigned int len, unsigned int off)
 *   {
 *       unsigned int size = fifo->mask + 1;
 *       unsigned int l;
 *       off &= fifo->mask;
 *       l = min(len, size - off);
 *       memcpy(fifo->data + off, src, l);
 *       memcpy(fifo->data, src + l, len - l);
 *       smp_wmb();   ← 确保数据写入完成后再更新 in
 *   }
 * ──────────────────────────────────────────────────────────── */
static inline void
_kfifo_copy_in(struct user_kfifo *fifo, const void *src,
               unsigned int nelem, unsigned int off)
{
    unsigned int size = fifo->mask + 1;
    unsigned int esize = fifo->esize;
    unsigned int l;
    char *base = (char *)fifo->data;

    off &= fifo->mask;              /* 对应内核 off &= fifo->mask */
    l = (nelem < size - off) ? nelem : (size - off);  /* min(nelem, size-off) */

    /* 第一段 memcpy：from off to end */
    memcpy(base + off * esize, src, l * esize);
    /* 第二段 memcpy：wrap around（若有剩余）*/
    memcpy(base, (const char *)src + l * esize, (nelem - l) * esize);

    /* ★ 写屏障（对应内核 smp_wmb）
     * 保证数据写入完成后，in 的更新才对消费者可见 */
    atomic_thread_fence(memory_order_release);
}

/* 内部：从环形缓冲区的 off 位置复制到 dst（对应 kfifo_copy_out）*/
static inline void
_kfifo_copy_out(struct user_kfifo *fifo, void *dst,
                unsigned int nelem, unsigned int off)
{
    unsigned int size = fifo->mask + 1;
    unsigned int esize = fifo->esize;
    unsigned int l;
    const char *base = (const char *)fifo->data;

    off &= fifo->mask;
    l = (nelem < size - off) ? nelem : (size - off);

    memcpy(dst, base + off * esize, l * esize);
    memcpy((char *)dst + l * esize, base, (nelem - l) * esize);

    /* ★ 全内存屏障（对应内核 smp_mb）
     * 保证数据读取完成后，out 的更新才对生产者可见 */
    atomic_thread_fence(memory_order_seq_cst);
}

/* ────────────────────────────────────────────────────────────
 * user_kfifo_in — 写入 nelem 个元素（对应内核 __kfifo_in）
 *
 * @fifo  : 队列指针
 * @buf   : 数据源
 * @nelem : 要写入的元素个数
 * 返回实际写入的元素个数
 *
 * 对应内核：
 *   unsigned int __kfifo_in(struct __kfifo *fifo,
 *                           const void *buf, unsigned int len)
 *   仅生产者调用，无需锁（SPSC 模式）
 * ──────────────────────────────────────────────────────────── */
static inline unsigned int
user_kfifo_in(struct user_kfifo *fifo, const void *buf, unsigned int nelem)
{
    unsigned int avail, in;

    /* 1. 读取消费者的 out（acquire，对应内核 smp_wmb 前读 out）*/
    unsigned int out = atomic_load_explicit(&fifo->out, memory_order_acquire);
    in  = atomic_load_explicit(&fifo->in,  memory_order_relaxed);

    /* 2. 计算可写空间 */
    avail = (fifo->mask + 1) - (in - out);
    if (nelem > avail) nelem = avail;  /* 截断到可写上限 */
    if (nelem == 0) return 0;

    /* 3. 写入数据 + release fence（内部调用 _kfifo_copy_in）*/
    _kfifo_copy_in(fifo, buf, nelem, in);

    /* 4. 更新 in（release，消费者可见）
     *    对应内核：fifo->in += len; 配合前面的 smp_wmb() */
    atomic_store_explicit(&fifo->in, in + nelem, memory_order_release);

    return nelem;
}

/* ────────────────────────────────────────────────────────────
 * user_kfifo_out — 读取 nelem 个元素（对应内核 __kfifo_out）
 *
 * @fifo  : 队列指针
 * @buf   : 目标缓冲区
 * @nelem : 要读取的元素个数
 * 返回实际读取的元素个数
 * ──────────────────────────────────────────────────────────── */
static inline unsigned int
user_kfifo_out(struct user_kfifo *fifo, void *buf, unsigned int nelem)
{
    unsigned int used, out;

    /* 1. 读取生产者的 in（acquire，对应内核 smp_rmb 前读 in）*/
    unsigned int in = atomic_load_explicit(&fifo->in, memory_order_acquire);
    out = atomic_load_explicit(&fifo->out, memory_order_relaxed);

    /* 2. 计算可读数量 */
    used = in - out;  /* 无符号减法，自动处理溢出 */
    if (nelem > used) nelem = used;
    if (nelem == 0) return 0;

    /* 3. 读取数据 + seq_cst fence */
    _kfifo_copy_out(fifo, buf, nelem, out);

    /* 4. 更新 out（release，生产者可见）*/
    atomic_store_explicit(&fifo->out, out + nelem, memory_order_release);

    return nelem;
}

/* ────────────────────────────────────────────────────────────
 * user_kfifo_peek — 仅查看不消费（对应内核 kfifo_out_peek）
 * ──────────────────────────────────────────────────────────── */
static inline unsigned int
user_kfifo_peek(struct user_kfifo *fifo, void *buf, unsigned int nelem)
{
    unsigned int in = atomic_load_explicit(&fifo->in,  memory_order_acquire);
    unsigned int out= atomic_load_explicit(&fifo->out, memory_order_relaxed);
    unsigned int used = in - out;
    if (nelem > used) nelem = used;
    if (nelem == 0) return 0;
    atomic_thread_fence(memory_order_acquire);
    _kfifo_copy_out(fifo, buf, nelem, out);
    return nelem;  /* out 不更新 */
}

/* ────────────────────────────────────────────────────────────
 * 泛型便捷宏（对应内核 kfifo_put / kfifo_get 宏）
 *
 * KFIFO_PUT(fifo_ptr, val)  — 写入单个元素
 * KFIFO_GET(fifo_ptr, ptr)  — 读取单个元素到 *ptr
 * ──────────────────────────────────────────────────────────── */
#define KFIFO_PUT(fifo, val) \
    user_kfifo_in((fifo), &(val), 1)

#define KFIFO_GET(fifo, ptr) \
    user_kfifo_out((fifo), (ptr), 1)

#endif /* USER_KFIFO_H */
