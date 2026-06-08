---
name: c-programming-spec
description: Applies a self-contained C99 programming standard — file organization, naming, types, error handling, mandatory LogError on every abnormal return path, pointer parameter qualifiers (const/restrict per semantics), RAII resource cleanup (reference/raii.h), GCC atomics (reference/atomic.h), compile-time checks (reference/compile_checks.h), memory safety, and documentation. Use when writing or reviewing C code, managing resources with multiple exit paths, autofd/autofree/WITH_LOCK, or C programming standards.
---

# C 语言编程规范 (C99)

集成 ch1–ch24 与三个配套头文件的**决策路径 + 必遵规则 + 禁忌**，不是逐章罗列。编写或审查 C 代码时**先读本 skill**；速查表见 [reference/quick-ref.md](reference/quick-ref.md)；完整条文与示例见 `reference/ch*.md`。

**一句话：** 先消灭 UB 与契约不清 → 类型/限定符/编译期检查兜底 → 统一错误码 + LogError + 资源清理保证每条失败路径可追踪、可释放。

**互补 skill：** 并发架构见 **c-multi-threading**；模块架构见 **c-programming-design**。

**完整章节索引版：** [reference/SKILL-full.md](reference/SKILL-full.md)（保留原逐章导航，不删 reference 正文）。

---

## 规范级别

| 标签 | 含义 |
| --- | --- |
| **必须** | 违反可能导致 bug、安全漏洞或不可移植 |
| **建议** | 强烈推荐，偏离须文档化 |
| **可选** | 团队自定，须一致 |

冲突时：**必须** > **建议** > **可选**。

---

## 审查决策顺序（不可跳步）

```
① 安全与契约 — 有无 UB？接口语义是否写进类型/属性？
② 类型与限定符 — 类型选对了吗？const/restrict 与声明一致吗？
③ 错误与资源 — 每条失败路径有 LogError 吗？资源能释放吗？
④ 并发（若涉及）— 共享能否消除？单整型 atomic，复杂状态 mutex
⑤ 风格与文档 — 命名/格式/注释是否一致？
```

**审查三问：** 失败时调用方能否从返回值 + 日志定位？头文件是否自包含、限定符是否一致？能否编译期捕获却留到运行期？

---

## 强制执行（两项）

### 指针形参 `const` / `restrict`

编写或审查**任何**带指针形参的函数，逐步核对：

1. 会改 `*p` 吗？否 → `const T *`；是 → `T *`（输出禁加 `const`）
2. 批量访问且与其他形参**无重叠**？是 → 加 `restrict`（memmove 式重叠 → **禁** restrict）
3. 输出二级指针？`T **`（不对槽位加 `const`）
4. `.h` 与 `.c` 限定符**逐字一致**

详例见 [ch8 §8.5](reference/ch8.md#85-指针形参修饰符)。

### 异常返回点日志

函数内每个失败/异常的 `return` 或 `goto` 退出路径，返回前必须 `LogError`（或项目 `LOG_ERROR`）；`REQUIRE` 已内置。禁 `printf` 替代。系统调用失败立即保存 `errno` 到局部变量再日志。

详例见 [ch10 §10.4](reference/ch10.md#104-异常返回点日志)。

---

## 必遵规则（按主题）

### 文件 / 头文件 / 命名

- `.h` 只声明、`.c` 才定义；头文件**自包含**；include 顺序：对应头 → 标准库 → 系统 → 第三方 → 本项目
- Include guard：`PROJECT_MODULE_H`；文件名小写+下划线
- 公共符号带**模块前缀** `module_verb_noun`；禁 `_` / `__` 开头

### 类型

- **声明即初始化**；指针初值 `NULL`；禁一行多变量
- 协议/硬件/位掩码 → `uint8_t`/`int32_t` 等定宽；**禁**假设 `int`/`long` 大小
- 内存量/索引/`strlen`/`malloc` → `size_t`；指针差 → `ptrdiff_t`；指针↔整数 → `uintptr_t`
- 布尔 → `bool`；字节流 → `unsigned char`/`uint8_t`
- 结构体成员按对齐**从大到小**；显式 `_pad` + `static_assert(sizeof(...))`

### 函数 / API

- 单一职责，≤50 行，嵌套 ≤3 层；公共 API 有 Doxygen
- 返回值语义：**命令型** 0/负 errno；**谓词型** bool；**计算型** 有效值/NULL 哨兵
- 公共 API **运行期**校验参数（NULL、值域、容量）；`assert` **仅**内部不变量

### 错误 / 资源

- 多退出路径 → goto 清理链（语义化标签）或 [raii.h](reference/raii.h)（`autofd`/`autofree`/`REQUIRE`/`WITH_LOCK`）
- 单 `return`、无 fd/堆/锁 → 原生 `close`/`free` 即可
- `malloc`/`calloc` 必查 NULL；`free` 后置 NULL；`realloc` 用临时指针
- `REQUIRE` 函数必须有 `out:` 标签

### 安全 / 宏 / 并发 / 文档

- 禁 `strcpy`/`sprintf`/`gets`；用 `snprintf`、带界 `memcpy`
- **有符号溢出 = UB**；分配前检查乘法溢出；移位量 `< 位宽`
- 复杂逻辑 → `static inline`；禁函数宏（副作用、无类型检查）
- 单整型共享计数/标志 → [atomic.h](reference/atomic.h)；多字段/链表/阻塞 → mutex（见 **c-multi-threading**）
- 注释写**为什么**；公共 API 文档化线程安全与所有权

---

## 绝对禁忌

| 禁忌 | 后果 |
| --- | --- |
| 有符号溢出后再检查 | UB；编译器可删分支 |
| `volatile` 当线程同步 | 数据竞争 |
| 普通 `int` 多线程 `++` | 非原子 RMW |
| `assert` 检查用户输入/malloc 失败 | Release 下消失 |
| 头文件定义函数/全局变量 | ODR/重复符号 |
| 无模块前缀的全局 `init()` | 链接冲突 |
| `free` 后不置 NULL / double-free | UAF、堆损坏 |
| `realloc` 直接赋原指针 | 失败泄漏 |
| `restrict` 用于可能重叠缓冲区 | UB |
| 输出参数加 `const` | 无法写入 |
| 声明与定义限定符不一致 | UB |
| 函数宏含 `return`/多次求值参数 | 控制流错乱 |
| `for (i=n-1; i>=0; i--)` 中 `i` 为 `size_t` | 无符号永不 `<0` → 死循环 |
| VLA 大数组 | 栈溢出 |
| 信号处理中 `printf`/`malloc`/加锁 | 死锁/UB |

更多见 [reference/quick-ref.md](reference/quick-ref.md)。

---

## 类型选型（决策树）

```
表示什么？
├─ sizeof / 索引 / strlen / malloc     → size_t
├─ 两指针之差                           → ptrdiff_t
├─ 指针 ↔ 整数                          → uintptr_t
├─ 协议/寄存器/位掩码/文件格式           → uint8_t…uint64_t
├─ 省内存 ≥N 位                         → int_leastN_t
├─ 热循环 ≥N 位                         → int_fastN_t / int
├─ 真/假                                → bool
├─ 字节流                               → unsigned char / uint8_t
├─ 时间戳                               → time_t / int64_t
└─ 信号处理共享                          → sig_atomic_t
```

**黄金法则：** 精确位宽用 `<stdint.h>`；对象大小用 `size_t`（注意无符号下溢）；API 返回值跟标准走（`read`→`ssize_t`）。

详表见 [ch5](reference/ch5.md)。

---

## 错误处理与资源管理

```
几个退出点 × 几种资源（fd / FILE* / 堆 / 锁）？
├─ 单 return，无中间失败           → 原生 close/free
├─ ≥2 退出点                       → raii.h 或 goto 链（out_free_fd → out_free → out）
└─ 系统调用失败                    → 存 errno → LogError → 返回负 errno
```

| 层 | 机制 |
| --- | --- |
| 编译期 | `__nonnull` / `__must_check` / `static_assert` |
| 入口 | 公共 API 参数校验 → `-EINVAL` 等 |
| 失败路径 | `LogError` + 错误码 + 清理 |

**raii.h 速用：** `REQUIRE` · `autofd`/`autofp`/`autofree` · `*_steal` · `autoptr(T)` · `WITH_LOCK` · `autoref(T)`

详例见 [ch10](reference/ch10.md)、[ch24](reference/ch24.md)。

---

## 编译期检查

```
约束能在编译期表达？
├─ 非空/格式串/返回值必用     → __nonnull / __printf / __must_check
├─ 全局布局不变量             → static_assert(expr)
├─ 函数体内/类型关系            → BUILD_BUG_ON(c) / BUILD_BUG_ON_ZERO(c)
├─ 数组元素个数               → ARRAY_SIZE(arr)
└─ 外部输入 / 运行时 NULL      → ch9 运行期校验（属性不能替代）
```

推荐编译：`gcc -std=gnu99 -O2 -Wall -Wextra -Werror -Wnonnull -Wformat=2`

详表见 [compile_checks.h](reference/compile_checks.h)、[ch23](reference/ch23.md)。

---

## 并发速查（编码层）

| 场景 | 方案 |
| --- | --- |
| 引用计数、统计、一次性标志 | `atomic.h` |
| `len` + `buf` 同时更新 | **mutex** |
| 硬件寄存器 / 防编译器优化 | `volatile`（**非**同步） |
| 信号 ↔ 主线程 | `sig_atomic_t` |

架构选型（Reactor、Share-Nothing 等）见 **c-multi-threading**，不在此重复。

---

## 审查清单

- [ ] 头文件自包含；include 顺序与 guard 正确
- [ ] 模块前缀 + snake_case；`.h` 无定义
- [ ] 类型选型正确（定宽/size_t/bool）；结构体对齐
- [ ] 指针形参 `const`/`restrict` 正确；声明=定义
- [ ] 每个异常返回点有 `LogError` 与足够上下文
- [ ] 公共 API 参数校验 + 明确返回值语义
- [ ] 无 strcpy/sprintf；整数/指针/字符串无 UB
- [ ] 宏安全或 static inline；malloc/free 配对
- [ ] 多 exit 路径用 raii.h 或 goto 链
- [ ] 共享整型用 atomic；复杂状态用锁
- [ ] 公共 API 有 nonnull/format/must_check；布局用 static_assert
- [ ] 注释说明为什么；公共接口有 Doxygen

---

## 深度参考

| 主题 | 文件 |
| --- | --- |
| 文件/命名/格式 | ch1–ch4 |
| 类型选型详解 | ch5 |
| 结构体/枚举/函数 | ch6–ch8 |
| 参数/错误/宏/头文件 | ch9–ch13 |
| 内存/指针/整数/字符串 | ch14–ch17 |
| 并发/可移植/注释 | ch18–ch21 |
| 原子/编译期检查/RAII | ch22–ch24 + `atomic.h` `compile_checks.h` `raii.h` |
| 速查合并表 | [quick-ref.md](reference/quick-ref.md) |
| 逐章索引（完整版） | [SKILL-full.md](reference/SKILL-full.md) |

权威外链见 [ch21](reference/ch21.md)。
