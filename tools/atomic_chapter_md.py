#!/usr/bin/env python3
"""Generate skill reference/ch22.md from tmp/atomic.h."""

from __future__ import annotations

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ATOMIC_H = ROOT / "tmp" / "atomic.h"
GCC_ATOMIC_DOCS = "https://gcc.gnu.org/onlinedocs/gcc/_005f_005fatomic-Builtins.html"


def atomic_h_content() -> str:
    """Standalone atomic.h for skill reference/ (no project-local dependencies)."""
    text = ATOMIC_H.read_text(encoding="utf-8")
    text = text.replace('#include "vos.h"\n\n', "")
    return text


def write_atomic_header(ref_dir: Path) -> None:
    (ref_dir / "atomic.h").write_text(atomic_h_content(), encoding="utf-8")


def atomic_chapter_md() -> str:
    header = f"""# GCC 内置原子操作（用户态）

*22高级主题 · 并发*

参考来源：[atomic.h](atomic.h)、[GCC __atomic Builtins]({GCC_ATOMIC_DOCS})、[第18章 并发与线程安全](ch18.md)

> 本章规范适用于 **GCC/Clang 用户态** 程序。宏定义见同目录 [atomic.h](atomic.h)；依赖 `__atomic_*` 内置函数，非 C11 `<stdatomic.h>` 标准接口，但语义等价。

"""
    body = r"""## 22.1 使用场景与选型

### [建议] 多线程访问共享数据时，若操作对象适合原子语义，优先使用原子操作

**适用条件（同时满足时可优先原子操作）：**

1. **多线程**（或至少两个并发执行流）会读写**同一块共享内存**。
2. 共享数据是 **单个整型标量**（`atomic_t` 封装 `int counter`），或可用一次原子 RMW 完成的操作（加/减/位运算/比较交换）。
3. 不需要在持锁期间执行复杂逻辑或多字段一致性更新。

**优先顺序：**

| 场景 | 推荐方案 | 说明 |
| --- | --- | --- |
| 引用计数、统计计数器、简单标志位 | `atomic_*` | 无锁、开销低，见 22.3 |
| 多字段结构体、链表、复合状态机 | `pthread_mutex_t` + 普通字段 | 见 [ch18](ch18.md) |
| 信号处理函数与主线程共享 | `sig_atomic_t` 或原子操作 | 信号上下文限制更严，见 ch18 |
| 仅防止编译器优化、硬件寄存器 | `volatile` | **不能**替代原子或锁 |

**反面示例（未同步的共享计数 — 数据竞争，UB）**

```c
static int g_conn_count = 0;

void on_connect(void)
{
    g_conn_count++;   /* ❌ 多线程下非原子读-改-写 */
}

void on_disconnect(void)
{
    g_conn_count--;   /* ❌ 可能丢失更新 */
}
```

**反面示例（误用 volatile 充当线程同步）**

```c
volatile int g_ready = 0;

/* 线程 A */
g_ready = 1;          /* ❌ volatile 不保证原子 RMW，不保证可见性顺序 */

/* 线程 B */
while (!g_ready) { }  /* ❌ 仍可能数据竞争或未定义行为 */
```

**正面示例（共享整型计数 — 优先 atomic_inc / atomic_dec）**

```c
#include "atomic.h"

static atomic_t g_conn_count = ATOMIC_INIT(0);

void on_connect(void)
{
    atomic_inc(&g_conn_count);
}

void on_disconnect(void)
{
    atomic_dec(&g_conn_count);
}

int conn_count_get(void)
{
    return atomic_read(&g_conn_count);
}
```

**正面示例（引用计数归零触发释放 — atomic_dec_and_test）**

```c
static atomic_t g_ref = ATOMIC_INIT(1);

void obj_get(void)
{
    atomic_inc(&g_ref);
}

void obj_put(void)
{
    if (atomic_dec_and_test(&g_ref)) {
        obj_destroy();   /* 仅当减到 0 的线程执行销毁 */
    }
}
```

**正面示例（无锁标志位 — atomic_cmpxchg）**

```c
static atomic_t g_started = ATOMIC_INIT(0);

bool try_start_once(void)
{
    int expected = 0;
    /* 仅当当前为 0 时置 1，返回是否成功 */
    return atomic_cmpxchg(&g_started, expected, 1);
}
```

---

## 22.2 类型与初始化

### [必须] 共享整型计数/标志使用 `atomic_t`，静态初始化使用 `ATOMIC_INIT`

```c
typedef struct {
    int counter;
} atomic_t;

#define ATOMIC_INIT(i) { (i) }
```

| 用法 | 示例 |
| --- | --- |
| 静态初始化 | `static atomic_t n = ATOMIC_INIT(0);` |
| 运行期赋值 | `atomic_set(&n, 42);` |
| 运行期读取 | `int v = atomic_read(&n);` |

**反面示例**

```c
atomic_t n;
n.counter = 0;        /* ❌ 非原子写入，与其他线程竞争 */
int x = n.counter;    /* ❌ 非原子读取 */
```

**正面示例**

```c
static atomic_t n = ATOMIC_INIT(0);
atomic_set(&n, 0);
int x = atomic_read(&n);
```

---

## 22.3 API 参考（与 atomic.h 一致）

以下宏接口与 [atomic.h](atomic.h) 保持一致，底层为 GCC `__atomic_*` 内置。

### 读/写

| 宏 | 说明 | 内存序 |
| --- | --- | --- |
| `atomic_read(v)` | 读取当前值 | `__ATOMIC_RELAXED` |
| `atomic_set(v, i)` | 写入新值 | `__ATOMIC_RELAXED` |

### 加减（无返回值）

| 宏 | 说明 | 内存序 |
| --- | --- | --- |
| `atomic_add(i, v)` | `*v += i` | RELAXED |
| `atomic_sub(i, v)` | `*v -= i` | RELAXED |
| `atomic_inc(v)` | `*v += 1` | RELAXED |
| `atomic_dec(v)` | `*v -= 1` | RELAXED |

### 加减（返回新值）

| 宏 | 说明 | 内存序 |
| --- | --- | --- |
| `atomic_add_return(i, v)` | 加后返回新值 | `__ATOMIC_SEQ_CST` |
| `atomic_sub_return(i, v)` | 减后返回新值 | SEQ_CST |
| `atomic_inc_return(v)` | 自增后返回新值 | SEQ_CST |
| `atomic_dec_return(v)` | 自减后返回新值 | SEQ_CST |

### 加减（返回旧值）

| 宏 | 说明 | 内存序 |
| --- | --- | --- |
| `atomic_fetch_add(i, v)` | 加后返回旧值 | SEQ_CST |
| `atomic_fetch_sub(i, v)` | 减后返回旧值 | SEQ_CST |
| `atomic_fetch_inc(v)` | 自增后返回旧值 | SEQ_CST |
| `atomic_fetch_dec(v)` | 自减后返回旧值 | SEQ_CST |

### 位运算

| 宏 | 说明 | 内存序 |
| --- | --- | --- |
| `atomic_and(i, v)` / `atomic_or(i, v)` / `atomic_xor(i, v)` | 位运算，返回新值 | RELAXED |
| `atomic_fetch_and(i, v)` 等 | 位运算，返回旧值 | SEQ_CST |

### 比较交换与交换

| 宏 | 说明 | 内存序 |
| --- | --- | --- |
| `atomic_cmpxchg(v, old, new)` | 若 `*v == old` 则写 `new`，成功返回 true | SEQ_CST / RELAXED |
| `atomic_xchg(v, new)` | 写入 `new`，返回旧值 | SEQ_CST |

### 测试类（基于 return 语义）

| 宏 | 说明 |
| --- | --- |
| `atomic_dec_and_test(v)` | 减 1 后是否为 0 |
| `atomic_inc_and_test(v)` | 加 1 后是否为 0 |
| `atomic_sub_and_test(i, v)` | 减 `i` 后是否为 0 |
| `atomic_test_and_set(v)` | 值是否非 0（`atomic_read(v) != 0`） |

---

## 22.4 内存序说明

### [建议] 默认 API 已选型；跨线程发布/订阅语义需更强顺序

| 分类 | 使用的顺序 | 典型 API |
| --- | --- | --- |
| 简单统计、无顺序依赖 | `__ATOMIC_RELAXED` | `atomic_add`、`atomic_read` |
| 需与内存可见性、顺序一致 | `__ATOMIC_SEQ_CST` | `*_return`、`*_fetch_*`、`cmpxchg`、`xchg` |

**反面示例（用 RELAXED 读做复杂双重检查，却假设强顺序）**

```c
if (atomic_read(&g_flag) != 0) {   /* RELAXED 读 */
    use_resource();                /* ❌ 可能看不到其他线程 prior 写入的数据 */
}
```

**正面示例（标志 + 数据：写侧先写数据再 atomic_set；读侧先 atomic_read 再读数据，或改用 SEQ_CST / 锁）**

```c
/* 发布 */
prepare_payload();
atomic_set(&g_ready, 1);

/* 订阅 — 若仅 RELAXED 不足，对 g_ready 使用 cmpxchg/xchg 或配合 mutex */
while (atomic_read(&g_ready) == 0) { }
consume_payload();
```

复杂 happens-before 关系仍应使用 **互斥锁** 或显式 C11 内存序；见 [ch18](ch18.md)。

---

## 22.5 何时不应只用原子操作

### [必须] 下列场景使用互斥锁或其他同步原语，而非单独 atomic

- 更新 **多个关联字段** 必须同时可见（例如 `size` + `buffer` 指针）。
- **链表/队列/哈希表** 等结构修改。
- 需要在临界区内调用 **可能阻塞** 的函数（I/O、malloc 等）。
- 需要 **可重入、可调试** 的临界区（lock 便于工具分析）。

**反面示例**

```c
typedef struct {
    atomic_t len;
    uint8_t *buf;   /* ❌ 仅 len 原子，buf 与 len 仍可能不一致 */
} pkt_t;
```

**正面示例**

```c
typedef struct {
    pthread_mutex_t lock;
    size_t          len;
    uint8_t        *buf;
} pkt_t;

/* 所有字段变更在 lock 保护下进行 */
```

---

## 22.6 与第 18 章的关系

| 主题 | 第 18 章 | 本章 |
| --- | --- | --- |
| 互斥锁保护多字段 | ✅ 主方案 | 复杂状态仍用锁 |
| 禁止数据竞争 | ✅ 必须 | 单整型优先 atomic |
| `volatile` ≠ 同步 | ✅ | ✅ 重申 |
| 信号处理 | `sig_atomic_t` | 用户态线程优先 `atomic_t` |

编写或审查并发代码时：**先判断共享数据是否适合原子语义 → 适合则用本章 API → 否则回退 ch18 锁方案**。
"""
    return header + body


def write_atomic_chapter(ref_dir: Path) -> None:
    write_atomic_header(ref_dir)
    (ref_dir / "ch22.md").write_text(atomic_chapter_md(), encoding="utf-8")


if __name__ == "__main__":
    print(atomic_chapter_md())
