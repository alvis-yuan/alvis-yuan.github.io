# 禁忌与 MT-Unsafe 详表

*供 c-multi-threading skill 查阅；编写/审查时遇疑必查。*

---

## mutex 绝对禁止

- 跨线程解锁（A 加锁 B 解锁，NORMAL 类型 UB）
- 对已销毁 mutex 再次操作（double-free）
- `memcpy` 复制 `pthread_mutex_t`
- 持锁调用 `exit()`（其他等锁线程永久阻塞）
- 信号处理器中 `pthread_mutex_lock`（非 async-signal-safe）
- NORMAL 类型递归加锁（同线程第二次加锁死锁）

---

## 条件变量

- **`if (!cond) pthread_cond_wait(...)`** → 必须用 **`while`**
- 未持锁修改共享条件
- 未持锁调用 signal/broadcast（虽有时「看似可行」，规范要求持锁）

---

## 读写锁

- 持有读锁时升级为写锁 → 先释放读锁再取写锁

---

## 自旋锁禁用场景

| 场景 | 结论 |
| --- | --- |
| 临界区含 I/O、系统调用、sleep | 禁止 |
| 单核 CPU | 禁止（持锁线程被调度走 → 永久自旋） |
| 信号/中断处理器 | 禁止 |

临界区内禁止：sleep、I/O、malloc、printf 等可能阻塞或耗时的操作。

---

## volatile vs 原子

**禁止**用 `volatile` 实现线程同步。`volatile` 不保证原子性、不防止 CPU 乱序。

使用 C11 `_Atomic` 或 GCC `__atomic_*`，按场景选择 `memory_order`（relaxed / acquire / release / seq_cst）。

---

## MT-Unsafe 函数（禁止直接调用）

POSIX 标注为非线程安全；多线程中**禁止直接调用**，须用替代表或启动前完成。

### 字符串 / 解析

| 禁用 | 替代 |
| --- | --- |
| `strtok()` | `strtok_r()` |
| `strerror()` | `strerror_r()` |

### 时间 / 随机

| 禁用 | 替代 |
| --- | --- |
| `localtime()` | `localtime_r()` |
| `gmtime()` | `gmtime_r()` |
| `asctime()` | `asctime_r()` |
| `ctime()` | `ctime_r()` |
| `rand()` | 线程局部 PRNG 或 `random_r()` |

### 环境 / 进程

| 禁用 | 说明 |
| --- | --- |
| `getenv()` | 启动前读取并缓存；或文档化只读 |
| `setenv()` / `putenv()` / `unsetenv()` / `clearenv()` | **所有环境变量操作必须在多线程启动前完成** |

### DNS / 网络（部分实现 MT-Unsafe）

| 禁用 | 替代 |
| --- | --- |
| `gethostbyname()` 等旧 API | `getaddrinfo()` |

### 其他常见 MT-Unsafe

- `readdir()` → `readdir_r()`（注意可重入性差异）
- `getlogin()`、`ttyname()` 等——查 man attributes(7)
- 未文档化为线程安全的 libc 全局状态函数

完整列表见 [attributes(7)](https://man7.org/linux/man-pages/man7/attributes.7.html) 与 [pthreads(7)](https://man7.org/linux/man-pages/man7/pthreads.7.html)。

---

## 信号

- 工作线程使用 `sigprocmask` → 用 **`pthread_sigmask`**
- 信号处理器中调用非 async-signal-safe 函数（`printf`、`malloc`、`pthread_mutex_lock` 等）
- NPTL 内部信号 `SIGRTMIN`、`SIGRTMIN+1`——应用程序禁止使用

**推荐模式：** 创建线程前 `pthread_sigmask(SIG_BLOCK, ...)` 屏蔽工作线程；专用线程 `sigwait()` 处理。

---

## fork

多线程进程中 `fork()` 后，子进程仅复制调用线程；其他线程持有的锁在子进程中仍为「已锁定」状态。

**禁止：** fork 后子进程调用非 async-signal-safe 函数（含 `malloc`、`printf`、大部分 libc）。

**正确：** 子进程仅 async-signal-safe 操作后立即 `exec()`；或 `pthread_atfork` 处理（复杂，慎用）。

---

## 取消点

- 持锁跨越取消点且无 `pthread_cleanup_push` / `pthread_cleanup_pop`
- 持资源（锁、fd、堆内存）未注册 cleanup 处理器

---

## 架构层禁忌（补充）

- 事件循环线程执行阻塞 I/O 或长时间 CPU 计算
- 仅依赖 `uv_async_send` 调用次数传递业务语义（可能合并）
- Share-Nothing 模型下多 Worker 共享可变连接表
- 所有权转移后发送方继续访问对象
- SPSC 队列多生产者或多消费者混用同一 API
- 每项操作单独加锁（应批量 dequeue 后锁外处理）
- 多锁无全局顺序；持低层级锁时调用可能获取高层级锁的外部回调
- **线程私有或只读数据与读写共享数据混在同一带锁 struct**
- **mutex 未注释说明所保护的成员/字段**

---

## 共享数据结构 · 四条规则

| # | 规则 | 要点 |
| --- | --- | --- |
| 1 | 非共享不进共享体 | 线程私有放 `_Thread_local`/线程上下文，不塞进全局带锁 struct |
| 2 | 只读共享单独 struct | 主线程 init 完毕再 `pthread_create`；之后各线程仅读，无需 mutex |
| 3 | 读写共享与 mutex 同 struct | 仅可变跨线程字段与锁放在同一结构体；锁只保护该 struct 内成员 |
| 4 | mutex 注释保护范围 | 字段旁注释 `protects: foo, bar[]`；缺注释视为违规 |

**只读发布注意：** spawn 前完成写入；若指针指向堆对象，确保子线程不会与主线程并发写同一对象。
