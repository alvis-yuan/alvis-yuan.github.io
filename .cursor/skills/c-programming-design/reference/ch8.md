# 可移植性设计原则

*Portability-First Design*

参考来源：
- [🔗 GNU Coding Standards — Portability](https://www.gnu.org/prep/standards/standards.html)

### 可移植性设计原则

GNU 编码标准将**可移植性**列为核心设计目标。SQLite 通过 VFS（虚拟文件系统）抽象层实现跨 Unix/Windows 的移植。GNU 标准要求：检查 CPU 特性而非操作系统名称，避免依赖特定字长或字节序。GCC 编码规范要求所有代码在最严格的 pedantic 模式下编译无警告。

> "Machine-independent files may contain conditionals on features of a particular system, but should never contain conditionals such as '#ifdef __hpux__' on the name or version of a particular system."
> — — GCC Coding Conventions, GNU Project

**图8：通过抽象层隔离平台差异实现可移植性设计**

- 应用代码（平台无关）
- 平台抽象层（VFS / HAL / Autoconf）
- Linux / Unix
- Windows
- macOS / BSD
- RTOS

**✅ 设计优势**
- 一套代码运行多平台，减少维护分支
- 通过 Autoconf/CMake 自动探测特性
- 避免平台名称 ifdef 污染代码
- 新平台适配只需实现抽象层接口
- 提高代码长期维护性
- 跨平台工具库（如 SQLite、libevent）
- GNU 工具链及系统工具
- 需要同时支持 Linux/Windows 的 SDK
- 嵌入式系统 HAL 层设计
- 开源库（需要社区在多平台使用）

### 代码示例对比

**反例 — 平台名称硬编码**

```c
/* ❌ 反例：GNU 明确禁止此风格
   依赖具体平台名，难以移植 */
#ifdef __linux__
    int fd = epoll_create1(0);
#elif defined(__APPLE__)
    int fd = kqueue();
#elif defined(_WIN32)
    HANDLE fd = CreateIoCompletionPort(
        INVALID_HANDLE_VALUE,
        NULL, 0, 0);
#elif defined(__FreeBSD__)
    int fd = kqueue();
#endif
/* 新增平台需到处修改 */
```

**正例 — 特性检测 + 抽象封装**

```c
/* ✅ 正例：类 SQLite VFS 风格
   通过特性检测而非平台名 */
/* config.h（由 Autoconf 生成）*/
#ifdef HAVE_EPOLL     /* 特性，非平台 */
# define EVLOOP_IMPL epoll
#elif defined(HAVE_KQUEUE)
# define EVLOOP_IMPL kqueue
#elif defined(HAVE_SELECT)
# define EVLOOP_IMPL select
#endif

/* 统一抽象接口，上层无需感知 */
struct event_loop {
    int (*wait)(struct event_loop*,
                int timeout_ms);
    int (*add)(struct event_loop*,
               int fd, int events);
    void (*destroy)(struct event_loop*);
};
```