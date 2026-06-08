# C 架构与设计模式速查表

*配合 SKILL.md 使用；完整示例见 `ch*.md`。*

---

## 架构十原则 · 一句话

| # | 原则 | 一句话 |
| --- | --- | --- |
| 1 | 单一职责 | 一函数一事，短小可测 |
| 2 | 分层 | 相邻层头文件接口，禁跨层 |
| 3 | 信息隐藏 | opaque + 最小 API |
| 4 | 集中错误 | 多步初始化 goto 单出口 |
| 5 | 数据驱动 | 表驱动，禁硬编码分支 |
| 6 | 简单控制流 | 扁平；安全关键禁递归 |
| 7 | 防御性 | 公共 API 校验；assert 仅内部 |
| 8 | 可移植 | VFS/抽象层收敛平台差异 |
| 9 | 所有权 | 借/转/出 API 文档必写 |
| 10 | 事件驱动 | 高并发 I/O → Reactor，非每连接线程 |

---

## 所有权速查

| 模式 | 签名示例 | 释放责任 |
| --- | --- | --- |
| 借用 | `void read(const obj *p)` | 调用方 |
| 转入 | `void queue_push(obj *p)` | 被调方（调用后勿再用 p） |
| 转出 | `obj *obj_create(void)` | 调用方（配对 destroy） |

---

## 模式决策速查表

| 需求 | 判断 | 模式 | C 案例 |
| --- | --- | --- | --- |
| 创建 | 全局唯一 | Singleton | Redis server |
| 创建 | 多步/多可选参数 | Builder | RESP 响应构建 |
| 创建 | 克隆代价高 | Prototype | fork、配置复制 |
| 创建 | 按类型创建 | Factory Method | libevent 后端 |
| 创建 | 兼容对象族 | Abstract Factory | Nginx 模块族 |
| 组合 | 接口不兼容 | Adapter | epoll/kqueue 适配 |
| 组合 | 简化复杂子系统 | Facade | ae、event_base |
| 组合 | 运行时加职责 | Decorator | HTTP 过滤链 |
| 组合 | 树形统一处理 | Composite | VFS、配置树 |
| 组合 | 控制访问 | Proxy | 系统调用 |
| 组合 | 大量相似对象 | Flyweight | Redis 共享整数 |
| 组合 | 抽象/实现独立扩展 | Bridge | DRM、event 后端 |
| 行为 | 一变多通知 | Observer | Pub/Sub、回调 |
| 行为 | 算法可互换 | Strategy | 编码策略、负载均衡 |
| 行为 | 请求封装/撤销/日志 | Command | AOF |
| 行为 | 多处理者依次 | Chain | HTTP 处理链 |
| 行为 | 状态驱动行为 | State | 连接状态机 |
| 行为 | 算法骨架固定 | Template Method | file_operations |
| 行为 | 统一遍历 | Iterator | dictIterator |
| 行为 | 集中协调 | Mediator | Redis 服务器 |
| 行为 | 新操作不改元素 | Visitor | AST 遍历 |
| 行为 | 快照恢复 | Memento | RDB |
| 行为 | 文法解释 | Interpreter | RESP/配置解析 |

---

## C 实现模式 · 惯用法

```c
/* Strategy / State / Template Method — 函数指针表 */
struct file_ops {
    ssize_t (*read)(struct file *f, void *buf, size_t n);
    ssize_t (*write)(struct file *f, const void *buf, size_t n);
};

/* Observer — 回调 + 上下文 */
typedef void (*event_cb)(void *ctx, int event, void *data);

/* Factory — 按类型返回不同 ops */
const struct file_ops *file_ops_for_type(enum file_type t);

/* Facade — 薄封装 */
int ae_mainloop(aeEventLoop *loop);  /* 隐藏 epoll 细节 */
```

---

## 模式类别 · 23 种分组

| 创建型 (5) | Singleton · Factory Method · Abstract Factory · Builder · Prototype |
| 结构型 (7) | Adapter · Bridge · Composite · Decorator · Facade · Flyweight · Proxy |
| 行为型 (11) | Observer · Strategy · Command · Chain · State · Template Method · Iterator · Mediator · Visitor · Memento · Interpreter |

---

## 架构禁忌 · 15 条

1. 边界未清就套模式
2. 跨层访问实现细节
3. 头文件暴露 struct 字段
4. 所有权 API 未文档化
5. 多处 return 各自清理
6. 高并发每连接一线程
7. 业务层硬编码 OS/平台
8. 全局变量一律 Singleton
9. 简单需求模式脚手架过重
10. Decorator 与 Proxy 混用
11. 模式对象无 destroy 配对
12. 模式无脑组合
13. 安全关键递归（JPL）
14. 深嵌套 if 代替表驱动/早返回
15. 用模式替代本应做的分层/所有权设计

---

## 开源对照

| 项目 | 架构/模式亮点 |
| --- | --- |
| SQLite | 分层七层、VFS、opaque API |
| Linux Kernel | 函数短小、goto 清理、§14 所有权 |
| Redis | 单线程 Reactor、Strategy 编码、Observer Pub/Sub |
| Nginx | Share-Nothing Worker、Decorator 过滤链、Composite 配置 |
| libevent | Adapter 多路复用、Factory 后端 |

权威外链见 [ch37.md](ch37.md)。
