---
name: c-programming-design
description: Guides C language software architecture and GoF design patterns in user-space/system code — layered modules, error handling, ownership, event-driven design, and pattern selection (Singleton, Factory, Observer, Strategy, etc.). Use when designing C modules, choosing architecture vs patterns, reviewing structure, or when the user mentions C architecture, design patterns, layering, or module boundaries.
---

# C 语言架构设计

集成 10 条架构原则与 GoF 23 种模式的**决策路径 + 必遵规则 + 禁忌**，不是逐章罗列。设计或审查 C 模块/系统结构时**先读本 skill**；模式速查见 [reference/quick-ref.md](reference/quick-ref.md)；完整条文与示例见 `reference/ch*.md`。

**一句话：** 先理清模块边界与所有权 → 满足架构原则 → 再按需求选模式；C 中用函数指针/不透明类型/表驱动实现，避免过度 OO。

**互补 skill：** 编码细节（命名、类型、LogError、RAII）见 **c-programming-spec**；多线程架构见 **c-multi-threading**。

**完整章节索引版：** [reference/SKILL-full.md](reference/SKILL-full.md)（保留原逐章导航，不删 reference 正文）。

---

## 规范级别

| 标签 | 含义 |
| --- | --- |
| **必须** | 架构契约；违反导致泄漏、不可维护或扩展失败 |
| **建议** | 强烈推荐；偏离须文档化 |
| **可选** | 团队自定，须一致 |

---

## 审查决策顺序（不可跳步）

```
① 模块边界 — 职责是否单一？层间是否只经头文件接口？
② 资源契约 — 所有权（借/转/出）是否在 API 文档写清？
③ 控制与错误 — 控制流是否扁平？多 exit 是否有集中清理？
④ 运行模型 — 事件驱动还是多线程？（与 c-multi-threading 衔接）
⑤ 设计模式 — 仅在 ①–④ 满足后，按需求选模式
```

**核心原则：先架构、后模式。** 模块划分、接口、所有权、错误策略未定时，**不要**先套 Singleton/Factory/Observer。

---

## 架构原则体系（10 条精华）

| 原则 | 要点 | 参考实践 |
| --- | --- | --- |
| **单一职责** | 函数只做一事，≤50 行；局部变量 ≤5–10 个 | Linux Kernel §6 |
| **分层模块化** | 层间仅相邻层通过头文件通信；禁跨层直接访问实现 | SQLite 七层 |
| **抽象与信息隐藏** | 对外 **opaque 类型** + 最小 API；禁暴露内部字段 | SQLite `sqlite3*` |
| **集中错误处理** | 多步骤初始化用 goto 清理单出口；编码层可用 raii.h | Linux Kernel §7 |
| **数据驱动** | 表驱动替代类型分支；数据与逻辑分离 | Redis robj |
| **简单控制流** | 扁平 if/循环；安全关键禁递归（JPL #1） | NASA Power of Ten |
| **防御性编程** | 公共 API 校验输入；assert 仅内部不变量 | SEI CERT |
| **可移植性** | 平台差异收敛到 VFS/抽象层；禁上层硬编码 OS 名 | SQLite VFS |
| **内存所有权** | 借（const）/ 转入（consume）/ 转出（create+destroy）须在 API 文档明确 | Linux §14 |
| **事件驱动** | 高并发 I/O 优先单线程 Reactor；禁每连接一线程 | Redis ae、Nginx |

详例见 `reference/ch1.md`–`ch10.md`。

---

## 内存所有权（API 设计必检）

| 模式 | 接口形态 | 谁释放 | 调用方注意 |
| --- | --- | --- | --- |
| **借用** | `void fn(const obj *p)` | 调用方 | 函数内不释放；`const` 表只读 |
| **转入** | `void consume(obj *p)` | 被调函数 | 调用后**不得再使用** `p` |
| **转出** | `obj *create(void)` | 调用方 | 须配对 `destroy()`；文档必写 |

**命名惯例：** `create`/`destroy`、`open`/`close`、`alloc`/`free` 成对；跨线程传递时所有权一并移交（见 **c-multi-threading** 所有权转移）。

---

## C 中实现模式的方式

GoF 思想与语言无关；C 中典型映射：

| OO 概念 | C 实现 |
| --- | --- |
| 多态 | 函数指针表（`struct ops { int (*read)(...); }`） |
| 封装 | 不透明指针 + `.c` 内完整 struct |
| 继承式扩展 | 结构体首成员嵌入 + 容器转换 |
| 回调/通知 | 函数指针 + `void *ctx` |
| 策略/状态切换 | ops 表或函数指针字段替换 |

**禁：** 为简单需求引入完整模式脚手架（见下方禁忌）。

---

## 设计模式选型（架构满足后再用）

### 快速决策

```
核心问题类型？
├─ 对象创建 → 全局唯一？Singleton | 多步构建？Builder | 按类型？Factory | 对象族？Abstract Factory | 克隆？Prototype
├─ 对象组合 → 接口不兼容？Adapter | 简化子系统？Facade | 动态加职责？Decorator | 树形统一？Composite | 控制访问？Proxy | 省内存？Flyweight | 抽象/实现分离？Bridge
└─ 行为通信 → 一变多通知？Observer | 算法可换？Strategy | 请求封装/撤销？Command | 多处理者链？Chain | 状态驱动行为？State | 算法骨架？Template Method | 统一遍历？Iterator | 集中协调？Mediator | 新操作不改元素？Visitor | 快照？Memento | 文法解释？Interpreter
```

### 高频模式 · 何时用

| 模式 | 用 | 不用 |
| --- | --- | --- |
| **Strategy** | 运行时切换算法/后端（I/O、编码） | 仅一种实现 |
| **Observer** | 事件/状态变更通知多方 | 简单回调足够 |
| **Factory** | 按参数创建不同实现 | 单一 `malloc`+初始化 |
| **Facade** | 隐藏复杂子系统（ae、event_base） | 子系统本就简单 |
| **Adapter** | 统一 epoll/kqueue/select 等差异 | 仅单一平台 |
| **State** | 连接/协议状态机行为大变 | 少量 if/enum 够用 |
| **Singleton** | 真全局唯一资源（server 状态） | 普通全局变量伪装 |

完整 23 种对照表见 [quick-ref.md](reference/quick-ref.md)；决策树详表见 [ch36](reference/ch36.md)。

---

## 绝对禁忌

### 架构层

| 禁忌 | 后果 |
| --- | --- |
| 模块边界未清就套模式 | 过度设计、难维护 |
| 上层直接操作下层实现（跨层访问 pager/磁盘） | 耦合、无法替换/VFS |
| 对外暴露 struct 内部字段 | 破坏封装、ABI 不稳定 |
| 所有权语义不在 API 文档说明 | 泄漏、UAF、双重释放 |
| 多处 return 各自清理资源 | 遗漏释放（应用 goto/raii） |
| 高并发下每连接一线程 | C10K、上下文切换爆炸 |
| 硬编码平台/OS 名到业务层 | 不可移植 |

### 模式层

| 禁忌 | 后果 |
| --- | --- |
| 把一切全局变量做成 Singleton | 隐藏依赖、难测试 |
| 简单解析一行配置却 Factory+Observer+Singleton | 脚手架大于业务 |
| 混淆 Decorator（加职责）与 Proxy（控访问） | 错误抽象 |
| 忽略 C 无 GC，模式对象无 create/destroy 配对 | 泄漏 |
| 模式可组合 ≠ 必须组合（Factory+Singleton 滥用） | 复杂度失控 |

### 控制流（安全关键）

| 禁忌 | 后果 |
| --- | --- |
| 飞行/安全关键代码用递归（JPL） | 栈深不可证 |
| 深度嵌套 if-else 代替早返回/表驱动 | 圈复杂度、难测 |

---

## 关键代码模式

### 先架构后模式

```c
/* ❌ 解析一行配置却引入 Singleton + Factory + Observer */

/* ✅ 分层 + 单一职责 */
int config_parse_line(const char *line, struct config_entry *out);
int config_store_push(struct config_store *store, const struct config_entry *e);

/* ✅ 多种后端运行时切换 → Strategy */
struct io_backend {
    ssize_t (*read)(void *ctx, void *buf, size_t n);
    ssize_t (*write)(void *ctx, const void *buf, size_t n);
};
```

### 不透明类型

```c
/* ❌ 头文件暴露内部字段 */
typedef struct { int fd; char buf[4096]; } conn_t;

/* ✅ 仅前向声明 + 最小 API */
typedef struct conn conn_t;
conn_t *conn_create(int fd);
void conn_destroy(conn_t *c);
```

### goto 集中清理（架构层；编码见 c-programming-spec）

```c
/* ❌ 多处 return，每处记得 free */

/* ✅ 单出口清理链 */
if (!buf) goto out;
if (init_foo() < 0) goto out_free_buf;
/* ... */
out_free_buf: free(buf);
out: return ret;
```

---

## 审查清单

**架构（必检）**
- [ ] 模块/函数单一职责；层间仅头文件接口
- [ ] 对外 opaque 类型；内部实现不泄露
- [ ] 所有权（借/转/出）在 API 文档明确；create/destroy 成对
- [ ] 多 exit 路径有 goto 清理或 raii.h（编码规范）
- [ ] 表驱动优于硬编码分支；控制流扁平
- [ ] 高并发 I/O 是否应用事件驱动而非每连接线程

**模式（按需）**
- [ ] 是否先满足架构再引入模式？
- [ ] 模式是否可用函数指针/表驱动更简单实现？
- [ ] 是否对照 [ch36 决策树](reference/ch36.md) 避免过度设计？

---

## 深度参考

| 主题 | 文件 |
| --- | --- |
| 架构原则 ch1–10 | `reference/ch1.md`–`ch10.md` |
| 模式概述与 23 种总览 | `reference/ch11.md`–`ch12.md` |
| 各模式详解 ch13–35 | `reference/ch13.md`–`ch35.md` |
| 模式决策树 | [ch36](reference/ch36.md) |
| 模式/架构速查表 | [quick-ref.md](reference/quick-ref.md) |
| 逐章索引（完整版） | [SKILL-full.md](reference/SKILL-full.md) |
| 权威外链 | [ch37](reference/ch37.md) |
