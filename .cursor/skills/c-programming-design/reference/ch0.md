# C 语言架构设计总览

*架构与模式*

参考来源：[Linux Kernel Coding Style](https://www.kernel.org/doc/html/latest/process/coding-style.html)、[SQLite Architecture](https://sqlite.org/arch.html)、[GoF Design Patterns](https://en.wikipedia.org/wiki/Design_Patterns)

> 本 skill 分两部分：**第 1–10 章**为架构与设计原则（函数职责、分层、错误处理、事件驱动等）；**第 11–36 章**为 GoF 设计模式在 C 中的落地与选型。编写或审查 C 模块/系统架构时，**先读相关 reference 章节**。

## 0.1 使用场景与选型

### [建议] 先确定架构层问题，再考虑具体设计模式

**两层决策：**

| 层次 | 典型问题 | 阅读章节 |
| --- | --- | --- |
| **架构原则** | 模块如何划分？错误如何传播？谁拥有内存？控制流是否过深？ | 第 1–10 章 |
| **设计模式** | 创建对象、组合结构、行为协作是否有成熟套路？ | 第 11–36 章 |

**优先顺序：**

1. **模块边界与接口**（分层、信息隐藏、所有权）未理清时，**不要**先套模式。
2. 架构原则满足后，按 [第 36 章 设计模式决策树](ch36.md) 选择模式。
3. C 中模式通常用 **函数指针 / 结构体 + 回调 / 表驱动** 实现，避免过度 OO 化。

**反面示例（未做架构直接套模式 — 过度设计）**

```c
/* ❌ 仅解析一行配置，却引入 Singleton + Factory + Observer */
typedef struct config_singleton config_singleton_t;
config_singleton_t *config_singleton_get(void);
/* ...200 行模式脚手架... */
int load_one_key(const char *line);  /* 实际需求 */
```

**正面示例（先架构后模式）**

```c
/* ✅ 小模块：分层 + 单一职责（第 1、2 章）*/
int config_parse_line(const char *line, struct config_entry *out);
int config_store_push(struct config_store *store, const struct config_entry *e);

/* ✅ 多种后端需运行时切换时，再用 Strategy（第 26 章）*/
struct io_backend {
    ssize_t (*read)(void *ctx, void *buf, size_t n);
    ssize_t (*write)(void *ctx, const void *buf, size_t n);
};
```

## 0.3 架构原则体系

**图1：C 语言设计原则总体架构体系**

- C 语言设计原则体系
- **架构层原则**：单一职责、分层模块化、抽象接口、数据驱动、事件驱动
- **控制流层原则**：简单控制流、集中错误处理、防御性编程
- **资源管理层原则**：内存所有权、可移植性设计、信息隐藏

---

## 0.4 权威来源标签

- [🐧 Linux Kernel Coding Style](https://www.kernel.org/doc/html/latest/process/coding-style.html)
- [🗄️ SQLite Architecture](https://sqlite.org/arch.html)
- [🐂 GNU Coding Standards](https://www.gnu.org/prep/standards/standards.html)
- [🚀 NASA JPL Power of Ten](https://spinroot.com/p10/)
- [🔒 SEI CERT C Standard](https://wiki.sei.cmu.edu/confluence/spaces/c/pages/87152064/Introduction)
