/**
 * @file timer.c
 * @brief 用户态侵入式最小堆定时器实现
 */

#include "timer.h"

#include <string.h>

#include "raii.h"

/* ── 内部辅助：交换两个堆槽位，并更新 internal_idx ── */
static void
timer_swap(struct timer_context *ctx, int i, int j)
{
    struct timer_node *tmp = NULL;

    tmp = ctx->data[i];
    ctx->data[i]          = ctx->data[j];
    ctx->data[j]          = tmp;
    ctx->data[i]->internal_idx = i;
    ctx->data[j]->internal_idx = j;
}

/* ── sift_up：O(log n)，节点上浮（参照 __min_heap_sift_up_inline）── */
static void
sift_up(struct timer_context *ctx, int i)
{
    while (i > 0) {
        int parent = (i - 1) / 2;

        if (ctx->data[parent]->expires <= ctx->data[i]->expires) {
            break;
        }
        timer_swap(ctx, i, parent);
        i = parent;
    }
}

/* ── sift_down：O(log n)，节点下沉（参照 __min_heap_sift_down_inline）── */
static void
sift_down(struct timer_context *ctx, int i)
{
    int n = ctx->count;

    for (;;) {
        int smallest = i;
        int l = 2 * i + 1;
        int r = 2 * i + 2;

        if (l < n && ctx->data[l]->expires < ctx->data[smallest]->expires) {
            smallest = l;
        }
        if (r < n && ctx->data[r]->expires < ctx->data[smallest]->expires) {
            smallest = r;
        }
        if (smallest == i) {
            break;
        }
        timer_swap(ctx, i, smallest);
        i = smallest;
    }
}

/* ── timer_context_init ── */
void
timer_context_init(struct timer_context *ctx)
{
    if (ctx == NULL) {
        LogError("timer_context_init: ctx is NULL");
        return;
    }

    memset(ctx, 0, sizeof(*ctx));
}

/* ── timer_add：插入定时器，O(log n)
 *   参照 min_heap_push_inline：放末尾，sift_up
 * ── */
bool
timer_add(struct timer_context *ctx,
                struct timer_node *node,
                uint64_t delay_ns,
                void (*cb)(struct timer_node *))
{
    if (ctx == NULL || node == NULL || cb == NULL) {
        LogError("timer_add: invalid argument ctx=%p node=%p cb=%p",
                 (void *)ctx, (void *)node, (void *)cb);
        return false;
    }

    if (ctx->count >= TIMER_MAX) {
        LogError("timer_add: heap full (count=%d max=%d)",
                 ctx->count, TIMER_MAX);
        return false;
    }

    node->expires  = timer_now_ns() + delay_ns;
    node->callback = cb;
    node->internal_idx = ctx->count;

    ctx->data[ctx->count++] = node; /* 放到末尾 */
    sift_up(ctx, ctx->count - 1);   /* 上浮到正确位置 */
    return true;
}

/* ── timer_cancel：取消任意定时器，O(log n)
 *   参照 min_heap_del_inline：与末尾交换，sift_up + sift_down
 * ── */
void
timer_cancel(struct timer_context *ctx,
                  struct timer_node *node)
{
    int idx = 0;
    int last = 0;

    if (ctx == NULL || node == NULL) {
        LogError("timer_cancel: invalid argument ctx=%p node=%p",
                 (void *)ctx, (void *)node);
        return;
    }

    idx = node->internal_idx;
    if (idx < 0 || idx >= ctx->count) {
        return;
    }

    last = --ctx->count;
    node->internal_idx = -1;

    if (idx == last) {
        return; /* 删的恰好是末尾 */
    }

    ctx->data[idx] = ctx->data[last];
    ctx->data[idx]->internal_idx = idx;
    sift_up(ctx, idx);
    sift_down(ctx, ctx->data[idx]->internal_idx);
}

/* ── timer_next_ms：返回距堆顶到期的毫秒数（参照 min_heap_peek）── */
int64_t
timer_next_ms(const struct timer_context *ctx)
{
    int64_t diff = 0;

    if (ctx == NULL) {
        LogError("timer_next_ms: ctx is NULL");
        return -1;
    }

    if (ctx->count == 0) {
        return -1; /* 无定时器，永久等待 */
    }

    diff = (int64_t)(ctx->data[0]->expires - timer_now_ns());
    return diff < 0 ? 0 : diff / 1000000LL;
}

/* ── timer_process：处理所有已到期节点（类比 min_heap_pop 循环）── */
void
timer_process(struct timer_context *ctx)
{
    uint64_t               now = 0;
    struct timer_node *top = NULL;
    int                    last = 0;

    if (ctx == NULL) {
        LogError("timer_process: ctx is NULL");
        return;
    }

    now = timer_now_ns();
    while (ctx->count > 0 && ctx->data[0]->expires <= now) {
        top = ctx->data[0];

        /* 移除堆顶（swap 末尾 → 位置 0，sift_down）*/
        last = --ctx->count;
        top->internal_idx = -1;
        if (last > 0) {
            ctx->data[0] = ctx->data[last];
            ctx->data[0]->internal_idx = 0;
            sift_down(ctx, 0);
        }

        /* 调用用户回调 */
        if (top->callback != NULL) {
            top->callback(top);
        }
    }
}
