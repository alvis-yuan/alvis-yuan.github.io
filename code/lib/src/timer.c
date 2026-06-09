/* timer_heap.c */
#include "timer_heap.h"
#include <string.h>
#include <assert.h>

/* ── 内部辅助：交换两个堆槽位，并更新 heap_idx ── */
static void heap_swap(struct timer_heap *h, int i, int j) {
    struct timer_heap_node *tmp = h->data[i];
    h->data[i]          = h->data[j];
    h->data[j]          = tmp;
    h->data[i]->heap_idx = i;
    h->data[j]->heap_idx = j;
}

/* ── sift_up：O(log n)，节点上浮（参照 __min_heap_sift_up_inline）── */
static void sift_up(struct timer_heap *h, int i) {
    while (i > 0) {
        int parent = (i - 1) / 2;
        if (h->data[parent]->expires <= h->data[i]->expires)
            break;
        heap_swap(h, i, parent);
        i = parent;
    }
}

/* ── sift_down：O(log n)，节点下沉（参照 __min_heap_sift_down_inline）── */
static void sift_down(struct timer_heap *h, int i) {
    int n = h->count;
    for (;;) {
        int smallest = i;
        int l = 2 * i + 1;
        int r = 2 * i + 2;
        if (l < n && h->data[l]->expires < h->data[smallest]->expires)
            smallest = l;
        if (r < n && h->data[r]->expires < h->data[smallest]->expires)
            smallest = r;
        if (smallest == i) break;
        heap_swap(h, i, smallest);
        i = smallest;
    }
}

/* ── timer_heap_init ── */
void timer_heap_init(struct timer_heap *h) {
    memset(h, 0, sizeof(*h));
}

/* ── timer_heap_push：插入定时器，O(log n)
 *   参照 min_heap_push_inline：放末尾，sift_up
 * ── */
bool timer_heap_push(struct timer_heap *h,
                      struct timer_heap_node *node,
                      uint64_t delay_ns,
                      void (*cb)(struct timer_heap_node *))
{
    if (h->count >= TIMER_HEAP_MAX) return false;

    node->expires  = timer_now_ns() + delay_ns;
    node->callback = cb;
    node->heap_idx = h->count;

    h->data[h->count++] = node;  /* 放到末尾 */
    sift_up(h, h->count - 1);   /* 上浮到正确位置 */
    return true;
}

/* ── timer_heap_cancel：取消任意定时器，O(log n)
 *   参照 min_heap_del_inline：与末尾交换，sift_up + sift_down
 * ── */
void timer_heap_cancel(struct timer_heap *h,
                        struct timer_heap_node *node)
{
    int idx = node->heap_idx;
    if (idx < 0 || idx >= h->count) return;

    int last = --h->count;
    node->heap_idx = -1;

    if (idx == last) return;   /* 删的恰好是末尾 */

    h->data[idx] = h->data[last];
    h->data[idx]->heap_idx = idx;
    sift_up(h, idx);
    sift_down(h, h->data[idx]->heap_idx);
}

/* ── timer_next_ms：返回距堆顶到期的毫秒数（参照 min_heap_peek）── */
int64_t timer_next_ms(const struct timer_heap *h) {
    if (h->count == 0) return -1;  /* 无定时器，永久等待 */
    int64_t diff = (int64_t)(h->data[0]->expires - timer_now_ns());
    return diff < 0 ? 0 : diff / 1000000LL;
}

/* ── timer_process：处理所有已到期节点（类比 min_heap_pop 循环）── */
void timer_process(struct timer_heap *h) {
    uint64_t now = timer_now_ns();
    while (h->count > 0 && h->data[0]->expires <= now) {
        struct timer_heap_node *top = h->data[0];

        /* 移除堆顶（swap 末尾 → 位置 0，sift_down）*/
        int last = --h->count;
        top->heap_idx = -1;
        if (last > 0) {
            h->data[0] = h->data[last];
            h->data[0]->heap_idx = 0;
            sift_down(h, 0);
        }

        /* 调用用户回调 */
        if (top->callback) top->callback(top);
    }
}
