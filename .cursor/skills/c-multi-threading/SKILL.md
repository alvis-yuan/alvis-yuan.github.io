---
name: c-multi-threading
description: C 多线程集成技能：架构范式选型（Reactor、Dispatcher-Worker、线程池、Reactor+工作池、Share-Nothing、所有权转移、SPSC 无锁）、共享数据结构设计（按只读/读写/线程私有划分并与 mutex 同体）、工程决策路径（锁为最后手段）、POSIX/C11 规范与绝对禁忌、开源实践对照（nginx、Redis、Memcached、libuv、GLib、APR）。用于并发架构设计、pthread/原子/无锁编码、数据竞争与死锁排查。
---

# C 语言多线程设计

集成三份源文档的**决策路径 + 范式选型 + 编码禁忌**，不是按文档分章罗列。设计或审查 C 并发代码时**先读本 skill**，细节查 `reference/forbidden.md`（MT-Unsafe 与禁忌详表）。

**一句话：** 先选范式消灭不必要的共享 → 再用工程递进驯服不可避免的同步 → 最后用 POSIX 规范与 TSan 保证合规。

**互补 skill：** 编码规范见 **c-programming-spec**（第 18 章并发、第 22 章原子）；事件驱动架构见 **c-programming-design**（第 10 章）。

---

## 三层决策（按顺序，不可跳步）

```
① 范式选型 — 负载是什么？（I/O / CPU / 混合）
② 工程递进 — 能否不分片、不转移、不用无锁，直接上锁？（尽量别）
③ 编码合规 — 上了锁之后，mutex/cond/原子/fork/信号怎么写才安全？
```

| 层次 | 核心问题 | 典型结论 |
| --- | --- | --- |
| **范式** | 万级连接？阻塞 I/O？CPU 批量？ | Reactor / Dispatcher / 线程池 / Reactor+池 |
| **工程** | 数据能否分片或单向流动？ | Share-Nothing → 所有权转移 → SPSC → 批量锁 → 锁层级 |
| **合规** | 哪些 API 不能碰？ | MT-Unsafe 禁用、while 等 cond、查 pthread 返回值 |

---

## 范式选型（先定架构）

| 场景 | 推荐范式 | 参考 | 锁竞争 |
| --- | --- | --- | --- |
| 万级 I/O、低延迟 | **单线程 Reactor** | Redis ae、Nginx Worker | 无（单线程） |
| 多核 accept、连接分发 | **Dispatcher-Worker** | Memcached | 分发队列锁 |
| 事件循环 + 阻塞/DNS/文件 I/O | **Reactor + 工作线程池** | libuv | 工作池队列 |
| 批量 CPU、任务 ≫ 线程 | **线程池** | GLib GThreadPool | 队列锁 |
| 任务有优先级 0–255 | **优先级线程池** | APR | 优先级队列 |
| 数据可均匀分片 | **Share-Nothing** | Nginx 多进程 | 无（进程/线程隔离） |
| 对象在线程间单向流动 | **所有权转移** | Memcached CQ_ITEM | 仅交接队列 |
| 连接 <1000、逻辑简单 | 每连接一线程 | Apache 风格 | 高，扩展差 |

**快速路径：**

```
负载？→ I/O 密集 → 连接万级？→ Reactor : Dispatcher-Worker
     → 混合型   → 有阻塞 I/O？→ Reactor+工作池
     → CPU 密集 → 分阶段流水线？→ 流水线 : 线程池（≈ CPU 核数，勿盲目 80+）
```

**框架铁律：**

- **libuv**：每个 `uv_loop_t` 绑定一线程；跨线程用 `uv_async_send`（调用可能被合并，不能靠次数传精确语义）。
- **Reactor 线程绝不阻塞**：阻塞任务 offload 到工作池，结果经 async/回调回主线程。
- **线程数 ≈ CPU 核数**（GLib `g_get_num_processors()`）；连接数线性增长线程 → C10K 陷阱。

---

## 工程递进：锁是最后手段

按优先级尝试，**只有上一级不可行才进入下一级**：

| 级 | 策略 | 要点 | 参考 |
| --- | --- | --- | --- |
| **① Share-Nothing** | 按 fd/用户/哈希桶分片，每线程独享数据 | 无共享 = 无竞争 = 无锁 | Nginx Worker |
| **② 所有权转移** | 任意时刻仅一线程持有对象；队列只做交接 | push 后发送方**不得再访问**指针 | Memcached |
| **③ SPSC 无锁队列** | 严格 1 生产者 + 1 消费者 | 容量 2 幂；head/tail **分缓存行**；release/acquire 配对 | Disruptor |
| **④ 批量 + 锁** | 一次加锁取 N 项，锁外处理 | 禁「每项 lock→处理→unlock」 | Redis 6.0 I/O 线程 |
| **⑤ 锁层级** | 全局编号，低→高加锁，禁逆序 | 多锁必遵；开发期可 TLS 检测违规 | Linux Kernel |

**SPSC 硬约束：** `push()` 仅一线程、`pop()` 仅另一线程；MPMC 需 CAS，不能误用 SPSC API。

---

## 共享数据结构设计（按可见性划分）

设计多线程模块时，**先按数据是否跨线程、是否可变划分结构**，再决定要不要锁。不要把所有字段堆进一个「全局大结构」。

| 可见性 | 放置方式 | 是否需要锁 |
| --- | --- | --- |
| **不跨线程** | 栈、函数局部、`_Thread_local`、线程参数/上下文 | **否**；**禁止**放进共享结构 |
| **跨线程只读** | 单独只读结构；主线程 `pthread_create` 前完成初始化，之后各线程仅读 | **否**（发布后可无锁读） |
| **跨线程读写** | 与保护它的 `pthread_mutex_t`（或 rwlock/atomic）**同一结构体** | **是** |

**四条规则：**

1. **非共享数据不进共享结构**——线程私有状态（工作缓冲区、临时计数、解析 scratch）放在线程上下文或 `_Thread_local`，不要塞进带锁的全局 struct「图方便」。
2. **只读共享单独成结构**——配置、路由表、启动后不变的 lookup：主线程初始化完毕再创建子线程；之后只读访问，**不必**与 mutex 绑在一起。
3. **读写共享与锁同结构**——仅把真正需要互斥的字段与 `pthread_mutex_t` 放在同一 struct；锁只保护该 struct 内声明的字段，不保护结构外的「漏网字段」。
4. **mutex 必须注释保护范围**——在 mutex 字段旁注释列出（或引用）其保护的成员；审查时以注释为契约，缺注释视为违规。

```c
/* ✅ 只读共享：spawn 前 init，之后无锁读 */
struct app_config {
    const char *listen_addr;
    int max_workers;
};

/* ✅ 读写共享：锁 + 受保护字段 + 注释 */
struct server_state {
    pthread_mutex_t lock; /* protects: conn_count, clients[], shutdown */
    int conn_count;
    struct client *clients;
    int shutdown;
};

/* ✅ 线程私有：不进入 server_state */
struct worker_ctx {
    int worker_id;
    char parse_buf[4096];
};

/* ❌ 把 worker 私有 scratch、只读 config 与 conn 列表塞进同一带锁 struct */
```

**审查：** 带锁 struct 里是否混入了线程私有或只读字段？mutex 旁是否有保护成员注释？只读共享是否在 spawn 前完成初始化且之后无写？

---

## 同步原语速查

| 场景 | 原语 | 要点 |
| --- | --- | --- |
| 通用互斥 | `pthread_mutex_t` | 默认首选；调试用 ERRORCHECK |
| 读:写 ≥ 10:1 | `pthread_rwlock_t` | **禁**读锁升级写锁 |
| 生产者-消费者 | mutex + cond | **`while` 等待**；改条件/通知前持锁 |
| 资源池/限流 | `sem_t` | `sem_post` 可在信号处理器中用 |
| 纳秒级、多核、无阻塞 | `pthread_spinlock_t` | **禁** I/O/sleep/malloc/printf；**单核禁用** |
| 计数/标志（无顺序依赖） | `_Atomic` + relaxed | **禁** volatile 当同步 |
| 发布-消费 / SPSC | acquire/release | 对齐防 false sharing |
| 懒初始化 | `pthread_once_t` | — |
| 线程私有 | `_Thread_local` / `pthread_key_t` | 零同步 |
| 多锁 | 全局锁序 + trylock 回退 | 破坏循环等待 |

**性能粗序（无竞争）：** atomic relaxed < seq_cst < spinlock < rwlock < mutex。**高竞争下 mutex 可慢两个数量级**——优先减竞争而非换锁类型。

---

## 必须遵守（核心原则）

1. **锁是最后手段**——先消除/转移/分片共享，再考虑同步。
2. **决策起点是数据流**——谁生产、谁消费、是否可分片，再选范式与原语。
3. **`pthread_*` 查返回值**，不看 errno；`pthread_create` 失败必须处理。
4. **joinable 线程必须 join 或 detach**，否则资源泄露；已终止的 `pthread_t` 不可复用。
5. **条件变量用 `while` 等待**，防虚假唤醒；持锁修改条件，持锁后 signal/broadcast。
6. **临界区最短**——禁 I/O、sleep、未知第三方调用；可拷贝到局部再解锁处理。
7. **谁加锁谁解锁**（NORMAL mutex）；禁跨线程解锁、禁 memcpy mutex、禁持锁 `exit()`。
8. **原子优于 volatile**——用 C11 `_Atomic` / `__atomic_*`，按场景选 `memory_order`。
9. **信号**：工作线程 `pthread_sigmask` 屏蔽；专用线程 `sigwait`；**禁**工作线程用 `sigprocmask`。
10. **fork**：多线程进程 fork 后子进程**仅** async-signal-safe 操作后立刻 `exec`；否则复制他人持有的锁 → 死锁。
11. **持锁时禁调可能反向加锁的外部回调**——最常见死锁来源之一。
12. **共享数据按可见性分结构**——非共享不进共享体；只读共享单独 struct；读写共享与 mutex 同体；mutex 注释保护范围。
13. **开发/CI 开 TSan**（`-fsanitize=thread`）；结合 ERRORCHECK mutex、Helgrind。

---

## 绝对禁忌（FORBIDDEN）

> 完整列表与 MT-Unsafe 替代表见 [reference/forbidden.md](reference/forbidden.md)

### 生命周期与 mutex

| 禁忌 | 后果 |
| --- | --- |
| 不检查 `pthread_create` 返回值 | 静默失败、线程耗尽 |
| joinable 既不 join 也不 detach | 线程资源泄露 |
| 取消点持锁且无 `pthread_cleanup_push` | 取消后锁永不释放 |
| NORMAL mutex 递归加锁 | 自死锁 |
| 跨线程解锁 / memcpy mutex / double-free mutex | 未定义行为 |
| 持读锁升级写锁 | 死锁 |

### 条件变量与自旋锁

| 禁忌 | 后果 |
| --- | --- |
| **`if` 代替 `while` 等 cond** | 虚假唤醒 → 逻辑错误 |
| 自旋锁临界区内 sleep/I/O/malloc/printf | CPU 空转、系统卡顿 |
| 单核 CPU 用自旋锁 | 持锁线程被切走 → 永久自旋 |

### 内存序与 API 安全

| 禁忌 | 后果 |
| --- | --- |
| **`volatile` 当线程同步** | 数据竞争、乱序 |
| 多线程调用 MT-Unsafe（`strtok`、`localtime`、`rand`、`getenv`/`setenv` 等） | 数据竞争 |
| 运行期 `setenv`/`putenv`/`unsetenv` | 环境变量非线程安全 |
| 信号处理器中 `printf`/`malloc`/`pthread_mutex_lock` | 死锁/UB |
| fork 后子进程调用非 async-signal-safe 函数 | 立即死锁 |

### 架构反模式

| 禁忌 | 后果 |
| --- | --- |
| Reactor 回调中做文件 I/O 或 CPU 密集计算 | 阻塞全事件循环 |
| 依赖 `uv_async_send` 次数传递精确数据 | 合并导致丢语义 |
| 线程数随连接数线性增长 | C10K、上下文切换爆炸 |
| 持锁 A 时调用可能获取 ≤A 层级锁的外部代码 | 死锁 |
| 线程私有或只读字段与读写共享字段同 struct | 无谓加锁、临界区膨胀、语义不清 |
| mutex 无注释说明保护哪些成员 | 锁范围不明，易漏保护或过度加锁 |

---

## 关键代码模式

### 条件变量（标准）

```c
pthread_mutex_lock(&lock);
while (!ready) {          /* ✅ 必须 while，禁止 if */
    pthread_cond_wait(&cond, &lock);
}
/* 处理 */
pthread_mutex_unlock(&lock);

/* 通知方：持锁改条件 → signal → unlock */
pthread_mutex_lock(&lock);
ready = 1;
pthread_cond_signal(&cond);
pthread_mutex_unlock(&lock);
```

### 双锁死锁 vs 锁层级

```c
/* ❌ 线程 A: lock(a)→lock(b)；线程 B: lock(b)→lock(a) */

/* ✅ 全局顺序：始终 L1(a) → L2(b)；或 trylock 失败则释放已持锁并重试 */
```

### 所有权转移

```c
/* ❌ queue_push(item) 后 dispatcher 仍读写 item */

/* ✅ push 后发送方不再碰指针；接收方独享至 free */
```

### 原子发布（非 volatile）

```c
/* ❌ volatile int ready; */

/* ✅ atomic_store_explicit(&ready, 1, memory_order_release); */
/*    atomic_load_explicit(&ready, memory_order_acquire); */
```

---

## 审查清单

**架构：** 负载是否匹配范式？Reactor 线程是否无阻塞？线程数是否合理？

**工程：** 能否 Share-Nothing 或所有权转移？SPSC 是否严格单写单读？多锁是否有全局锁序？共享 struct 是否按只读/读写/线程私有划分？mutex 是否注释保护成员？

**编码：** pthread 返回值？cond 用 while？MT-Unsafe 已替换？fork/信号/fork 路径安全？

**工具：** TSan/Helgrind 是否跑过？ERRORCHECK mutex 是否在调试构建启用？

---

## 权威参考

- [POSIX pthreads](https://man7.org/linux/man-pages/man7/pthreads.7.html)
- [Signal safety](https://man7.org/linux/man-pages/man7/signal-safety.7.html)
- [MT-Safe attributes](https://man7.org/linux/man-pages/man7/attributes.7.html)
- [The Open Group Base Spec](https://pubs.opengroup.org/onlinepubs/9699919799/)
- [Nginx architecture (AOSA)](https://aosabook.org/en/v2/nginx.html)
- [Redis event loop](https://github.com/redis/redis/blob/unstable/src/ae.c)
- [Memcached thread.c](https://github.com/memcached/memcached/blob/master/thread.c)
- [libuv design](https://docs.libuv.org/en/v1.x/design.html)
- [Disruptor (SPSC 理论)](https://lmax-exchange.github.io/disruptor/disruptor.html)
