#include "fifo.h"

#include <stdlib.h>
#include <string.h>

#include "atomic.h"
#include "utils.h"

/* 判断是否为 2 的幂（对应内核 is_power_of_2）。 */
#define UFIFO_IS_POWER_OF_2(n)  ((n) != 0 && (((n) & ((n) - 1)) == 0))

/*
 * 核心结构体（对应内核 struct __kfifo）。
 * in/out 只能通过 atomic.h 的 __atomic 封装访问，保证跨线程可见性。
 */
struct ufifo {
    unsigned int in;
    unsigned int out;
    unsigned int mask;
    unsigned int esize;
    void        *data;
};

static unsigned int ufifo_roundup_pow2(unsigned int size)
{
    unsigned int rounded = size;

    if (UFIFO_IS_POWER_OF_2(rounded)) {
        return rounded;
    }

    rounded--;
    rounded |= rounded >> 1;
    rounded |= rounded >> 2;
    rounded |= rounded >> 4;
    rounded |= rounded >> 8;
    rounded |= rounded >> 16;
    rounded++;

    return rounded;
}

static int ufifo_init(ufifo_t *restrict fifo,
                      void *restrict buf,
                      unsigned int size,
                      unsigned int esize)
{
    if (fifo == NULL) {
        LogError("fifo is NULL");
        return -1;
    }
    if (buf == NULL) {
        LogError("buffer is NULL");
        return -1;
    }
    if (!UFIFO_IS_POWER_OF_2(size) || size < 2) {
        LogError("fifo size must be a power of 2 and greater than 1");
        return -1;
    }
    if (esize == 0) {
        LogError("element size is zero");
        return -1;
    }

    ATOMIC_STORE_N(&fifo->in, 0, ATOMIC_RELAXED);
    ATOMIC_STORE_N(&fifo->out, 0, ATOMIC_RELAXED);
    fifo->mask = size - 1;
    fifo->esize = esize;
    fifo->data = buf;

    return 0;
}

static void ufifo_copy_in(ufifo_t *restrict fifo,
                          const void *restrict src,
                          unsigned int nelem,
                          unsigned int off)
{
    unsigned int size = fifo->mask + 1;
    unsigned int esize = fifo->esize;
    unsigned int len = 0;
    char *base = (char *)fifo->data;

    off &= fifo->mask;
    len = (nelem < size - off) ? nelem : (size - off);

    memcpy(base + (size_t)off * esize, src, (size_t)len * esize);
    memcpy(base,
           (const char *)src + (size_t)len * esize,
           (size_t)(nelem - len) * esize);

    /* 对应内核 smp_wmb：数据写入完成后，in 的更新才对消费者可见。 */
    ATOMIC_THREAD_FENCE(ATOMIC_RELEASE);
}

static void ufifo_copy_out(ufifo_t *restrict fifo,
                           void *restrict dst,
                           unsigned int nelem,
                           unsigned int off)
{
    unsigned int size = fifo->mask + 1;
    unsigned int esize = fifo->esize;
    unsigned int len = 0;
    const char *base = (const char *)fifo->data;

    off &= fifo->mask;
    len = (nelem < size - off) ? nelem : (size - off);

    memcpy(dst, base + (size_t)off * esize, (size_t)len * esize);
    memcpy((char *)dst + (size_t)len * esize,
           base,
           (size_t)(nelem - len) * esize);

    /* 对应内核 smp_mb：数据读取完成后，out 的更新才对生产者可见。 */
    ATOMIC_THREAD_FENCE(ATOMIC_SEQ_CST);
}

ufifo_t *ufifo_create(unsigned int size, unsigned int esize)
{
    ufifo_t *fifo = NULL;
    void    *buf = NULL;
    unsigned int alloc_size = 0;

    if (size < 2) {
        LogError("fifo size must be greater than 1");
        return NULL;
    }
    if (esize == 0) {
        LogError("element size is zero");
        return NULL;
    }

    alloc_size = ufifo_roundup_pow2(size);
    if (alloc_size < size) {
        LogError("fifo size overflow");
        return NULL;
    }
    if ((size_t)alloc_size > ((size_t)-1) / (size_t)esize) {
        LogError("fifo allocation size overflow");
        return NULL;
    }

    fifo = (ufifo_t *)calloc(1, sizeof(*fifo));
    if (fifo == NULL) {
        LogError("failed to allocate fifo object");
        return NULL;
    }

    buf = malloc((size_t)alloc_size * (size_t)esize);
    if (buf == NULL) {
        LogError("failed to allocate fifo buffer");
        free(fifo);
        fifo = NULL;
        return NULL;
    }

    if (ufifo_init(fifo, buf, alloc_size, esize) != 0) {
        free(buf);
        buf = NULL;
        free(fifo);
        fifo = NULL;
        return NULL;
    }

    return fifo;
}

void ufifo_destroy(ufifo_t *fifo)
{
    if (fifo == NULL) {
        return;
    }

    free(fifo->data);
    fifo->data = NULL;
    fifo->mask = 0;
    fifo->esize = 0;
    ATOMIC_STORE_N(&fifo->in, 0, ATOMIC_RELAXED);
    ATOMIC_STORE_N(&fifo->out, 0, ATOMIC_RELAXED);
    free(fifo);
}

unsigned int ufifo_len(const ufifo_t *fifo)
{
    unsigned int in = 0;
    unsigned int out = 0;

    if (fifo == NULL) {
        LogError("fifo is NULL");
        return 0;
    }

    in = ATOMIC_LOAD_N(&fifo->in, ATOMIC_ACQUIRE);
    out = ATOMIC_LOAD_N(&fifo->out, ATOMIC_ACQUIRE);

    return in - out;
}

unsigned int ufifo_size(const ufifo_t *fifo)
{
    if (fifo == NULL) {
        LogError("fifo is NULL");
        return 0;
    }

    return fifo->mask + 1;
}

unsigned int ufifo_avail(const ufifo_t *fifo)
{
    if (fifo == NULL) {
        LogError("fifo is NULL");
        return 0;
    }

    return ufifo_size(fifo) - ufifo_len(fifo);
}

int ufifo_is_empty(const ufifo_t *fifo)
{
    unsigned int in = 0;
    unsigned int out = 0;

    if (fifo == NULL) {
        LogError("fifo is NULL");
        return 1;
    }

    in = ATOMIC_LOAD_N(&fifo->in, ATOMIC_ACQUIRE);
    out = ATOMIC_LOAD_N(&fifo->out, ATOMIC_ACQUIRE);

    return in == out;
}

int ufifo_is_full(const ufifo_t *fifo)
{
    if (fifo == NULL) {
        LogError("fifo is NULL");
        return 0;
    }

    return ufifo_len(fifo) == ufifo_size(fifo);
}

unsigned int ufifo_in(ufifo_t *restrict fifo,
                      const void *restrict buf,
                      unsigned int nelem)
{
    unsigned int avail = 0;
    unsigned int in = 0;
    unsigned int out = 0;

    if (fifo == NULL) {
        LogError("fifo is NULL");
        return 0;
    }
    if (buf == NULL) {
        LogError("buffer is NULL");
        return 0;
    }

    out = ATOMIC_LOAD_N(&fifo->out, ATOMIC_ACQUIRE);
    in = ATOMIC_LOAD_N(&fifo->in, ATOMIC_RELAXED);
    avail = (fifo->mask + 1) - (in - out);
    if (nelem > avail) {
        nelem = avail;
    }
    if (nelem == 0) {
        return 0;
    }

    /* 生产者 先写数据，再发布 in */
    ufifo_copy_in(fifo, buf, nelem, in);
    ATOMIC_STORE_N(&fifo->in, in + nelem, ATOMIC_RELEASE);

    return nelem;
}

unsigned int ufifo_out(ufifo_t *restrict fifo,
                       void *restrict buf,
                       unsigned int nelem)
{
    unsigned int used = 0;
    unsigned int in = 0;
    unsigned int out = 0;

    if (fifo == NULL) {
        LogError("fifo is NULL");
        return 0;
    }
    if (buf == NULL) {
        LogError("buffer is NULL");
        return 0;
    }

    in = ATOMIC_LOAD_N(&fifo->in, ATOMIC_ACQUIRE);
    out = ATOMIC_LOAD_N(&fifo->out, ATOMIC_RELAXED);
    used = in - out;
    if (nelem > used) {
        nelem = used;
    }
    if (nelem == 0) {
        return 0;
    }

    /* 消费者 先读数据，再发布 out */
    ufifo_copy_out(fifo, buf, nelem, out);
    ATOMIC_STORE_N(&fifo->out, out + nelem, ATOMIC_RELEASE);

    return nelem;
}

unsigned int ufifo_peek(ufifo_t *restrict fifo,
                        void *restrict buf,
                        unsigned int nelem)
{
    unsigned int used = 0;
    unsigned int in = 0;
    unsigned int out = 0;

    if (fifo == NULL) {
        LogError("fifo is NULL");
        return 0;
    }
    if (buf == NULL) {
        LogError("buffer is NULL");
        return 0;
    }

    in = ATOMIC_LOAD_N(&fifo->in, ATOMIC_ACQUIRE);
    out = ATOMIC_LOAD_N(&fifo->out, ATOMIC_RELAXED);
    used = in - out;
    if (nelem > used) {
        nelem = used;
    }
    if (nelem == 0) {
        return 0;
    }

    ATOMIC_THREAD_FENCE(ATOMIC_ACQUIRE);
    ufifo_copy_out(fifo, buf, nelem, out);

    return nelem;
}

unsigned int ufifo_put(ufifo_t *restrict fifo, const void *restrict elem)
{
    return ufifo_in(fifo, elem, 1);
}

unsigned int ufifo_get(ufifo_t *restrict fifo, void *restrict elem)
{
    return ufifo_out(fifo, elem, 1);
}