/**
 * @file fifo.h
 * @brief 用户态 SPSC 无锁环形队列公共接口。
 *
 * 本模块基于 Linux kernel kfifo 的 in/out 单调递增索引模型改写，内部使用
 * atomic.h 中的 GCC __atomic 封装提供 acquire/release 可见性。
 *
 * @par SPSC 使用方式
 * - 仅允许一个 producer 线程调用 ufifo_in() / ufifo_put() 写入数据。
 * - 仅允许一个 consumer 线程调用 ufifo_out() / ufifo_get() 消费数据。
 * - consumer 可以调用 ufifo_peek() 查看数据，但 peek 不推进读偏移。
 * - ufifo_len() / ufifo_avail() / ufifo_is_empty() / ufifo_is_full() 是瞬时快照，
 *   多线程环境下只能用于观测或重试判断，不能作为跨线程复合条件的锁。
 * - 多 producer、多 consumer、或需要同时保护多个字段时，应在外层加锁或改用
 *   其他队列模型；本模块不提供 MPSC/MPMC 语义。
 * - ufifo_destroy() 必须在 producer 和 consumer 全部停止后调用。
 *
 * 生产者和消费者对数据的访问不乱序保证：
 * 1. 单生产者只写 in，单消费者只写 out，且 in/out 都是单调递增的索引，生产者和消费者通过读取对方的索引来判断数据状态。两个线程不同时写同一个索引，所以避免了最麻烦的多写者竞争。这个是 SPSC 成立的根基。
 *  - producer 只更新 in
 *  - consumer 只更新 out
 *  - producer 可以读取 out 来判断剩余空间
 *  - consumer 可以读取 in 来判断可读数量
 * 2. in/out 不重置，靠无符号回绕
 * 3. producer 先写数据，再发布 in
 * 4. consumer 先 acquire 读取 in，再读数据
 * 5. consumer 读完数据后，再发布 out
 */
#ifndef UFIFO_H
#define UFIFO_H

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 不透明 FIFO 句柄，内部结构只在 fifo.c 中定义。 */
typedef struct ufifo ufifo_t;

/**
 * @brief 创建 FIFO，并将容量向上取整到 2 的幂。
 * @param size 期望可容纳的元素个数，必须大于 1。
 * @param esize 单个元素的字节数，必须大于 0。
 * @return 成功返回 FIFO 句柄，失败返回 NULL。
 */
ufifo_t *ufifo_create(unsigned int size, unsigned int esize);

/**
 * @brief 销毁 FIFO 并释放内部缓冲区。
 * @param fifo FIFO 句柄；允许传入 NULL。
 *
 * @note 调用前必须保证 producer 和 consumer 线程已经退出。
 */
void ufifo_destroy(ufifo_t *fifo);

/**
 * @brief 获取 FIFO 总容量。
 * @param fifo FIFO 句柄。
 * @return 成功返回容量，参数无效返回 0。
 */
unsigned int ufifo_size(const ufifo_t *fifo);

/**
 * @brief 获取当前已使用元素数的瞬时快照。
 * @param fifo FIFO 句柄。
 * @return 成功返回已使用元素数，参数无效返回 0。
 */
unsigned int ufifo_len(const ufifo_t *fifo);

/**
 * @brief 获取当前剩余可写空间的瞬时快照。
 * @param fifo FIFO 句柄。
 * @return 成功返回剩余空间，参数无效返回 0。
 */
unsigned int ufifo_avail(const ufifo_t *fifo);

/**
 * @brief 判断 FIFO 当前是否为空。
 * @param fifo FIFO 句柄。
 * @return 为空返回 1，非空返回 0，参数无效返回 1。
 */
int ufifo_is_empty(const ufifo_t *fifo);

/**
 * @brief 判断 FIFO 当前是否已满。
 * @param fifo FIFO 句柄。
 * @return 已满返回 1，未满返回 0，参数无效返回 0。
 */
int ufifo_is_full(const ufifo_t *fifo);

/**
 * @brief producer 批量写入元素。
 * @param fifo FIFO 句柄。
 * @param buf 数据源缓冲区，至少包含 nelem 个元素。
 * @param nelem 期望写入的元素个数。
 * @return 实际写入的元素个数，空间不足时可能小于 nelem。
 *
 * @note 只能由唯一 producer 线程调用。
 */
unsigned int ufifo_in(ufifo_t *restrict fifo,
                      const void *restrict buf,
                      unsigned int nelem);

/**
 * @brief consumer 批量读取并消费元素。
 * @param fifo FIFO 句柄。
 * @param buf 目标缓冲区，至少可容纳 nelem 个元素。
 * @param nelem 期望读取的元素个数。
 * @return 实际读取的元素个数，数据不足时可能小于 nelem。
 *
 * @note 只能由唯一 consumer 线程调用。
 */
unsigned int ufifo_out(ufifo_t *restrict fifo,
                       void *restrict buf,
                       unsigned int nelem);

/**
 * @brief consumer 查看元素但不消费。
 * @param fifo FIFO 句柄。
 * @param buf 目标缓冲区，至少可容纳 nelem 个元素。
 * @param nelem 期望查看的元素个数。
 * @return 实际查看的元素个数，数据不足时可能小于 nelem。
 *
 * @note 只能由唯一 consumer 线程调用；调用后读偏移不变化。
 */
unsigned int ufifo_peek(ufifo_t *restrict fifo,
                        void *restrict buf,
                        unsigned int nelem);

/**
 * @brief producer 写入单个元素。
 * @param fifo FIFO 句柄。
 * @param elem 指向单个元素的地址。
 * @return 成功返回 1，队列满或参数无效返回 0。
 */
unsigned int ufifo_put(ufifo_t *restrict fifo, const void *restrict elem);

/**
 * @brief consumer 读取并消费单个元素。
 * @param fifo FIFO 句柄。
 * @param elem 输出单个元素的地址。
 * @return 成功返回 1，队列空或参数无效返回 0。
 */
unsigned int ufifo_get(ufifo_t *restrict fifo, void *restrict elem);

#ifdef __cplusplus
}
#endif

#endif /* UFIFO_H */