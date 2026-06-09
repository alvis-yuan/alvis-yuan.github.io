---
name: c-programming-spec
description: Applies a self-contained C99 programming standard — file organization, naming, types, local variable placement at block/function start, error handling, mandatory LogError on every abnormal return path, Chinese comments and English log messages, pointer parameter qualifiers (const/restrict per semantics), RAII resource cleanup (reference/raii.h), GCC atomics (reference/atomic.h), compile-time checks (reference/compile_checks.h), memory safety, and documentation. Use when writing or reviewing C code, managing resources with multiple exit paths, autofd/autofree/WITH_LOCK, or C programming standards.
---

# C 语言编程规范 (C99)

本 skill 为自包含的 C99 编程规范：核心章节见 `reference/ch1.md`–`ch21.md`；扩展含数据类型选择（第 5 章 5.3–5.18）、GCC 用户态原子操作（[atomic.h](reference/atomic.h)，第 22 章）、GCC 编译期检查（[compile_checks.h](reference/compile_checks.h)，第 23 章）、RAII 资源管理（[raii.h](reference/raii.h)，第 24 章）。权威外部资料见 [第 21 章](reference/ch21.md)。编写或审查 C 代码时，**必须先阅读相关 reference 文件**，并按规范执行。

## 使用方式

1. 根据任务类型，从下表定位对应章节并 **完整阅读** `reference/` 下的 markdown。
2. 每条规则包含：**级别标签**（必须/建议/可选）、**文字说明**、**正面示例**与**反面示例**（若源文档提供）。
3. 源文档仅有单一示例时，以该示例为准；同一代码块内含 ✅/❌ 标记时，分别作为正反面示例。
4. 规范冲突时优先级：**必须** > **建议** > **可选**。
5. **指针形参限定符（强制执行）**：编写或审查任何带指针形参的函数时，必须按 [第 8.5 章](reference/ch8.md#85-指针形参修饰符) 核对 `const`/`restrict`——只读输入加 `const`，独占无别名缓冲区加 `restrict`，输出缓冲区不得误加 `const`，且头文件与 `.c` 定义保持一致。
6. **异常返回点日志（强制执行）**：函数内每个失败/异常的 `return` 或 `goto` 退出路径，必须在返回前调用 `LogError`（或项目统一的 `LOG_ERROR`）；`REQUIRE` 宏已内置日志；**日志 format 字符串使用英文**。详见 [第 10.4 章](reference/ch10.md#104-异常返回点日志)。
7. **注释与日志语言（强制执行）**：源码注释与 Doxygen 使用**中文**；`LogError`/`LogInfo`/`LOG_*` 等运行期日志使用**英文**。详见 [第 20.4 章](reference/ch20.md#204-注释与日志语言)。
8. **函数声明与定义格式（强制执行）**：存储类、返回类型、函数名必须在**同一行**（K&R 风格）；左花括号 `{` 另起一行；`.h` 与 `.c` 一致。详见 [第 3.2 章](reference/ch3.md#32-大括号与函数格式)、[第 8.2 章](reference/ch8.md#82-函数声明规范)。

## 规范级别

| 标签 | 含义 |
| --- | --- |
| **必须** | 强制遵守，违反可能导致 bug、安全漏洞或不可移植 |
| **建议** | 强烈推荐，偏离须有充分理由并文档化 |
| **可选** | 团队可自定，但须保持一致 |

## 章节索引

- [第1章 文件组织与头部注释](reference/ch1.md)
- [第2章 命名规范](reference/ch2.md)
- [第3章 代码格式与排版](reference/ch3.md)
- [第4章 变量声明与修饰符](reference/ch4.md)
- [第5章 数据类型选择](reference/ch5.md)
- [第6章 结构体与联合体](reference/ch6.md)
- [第7章 枚举类型](reference/ch7.md)
- [第8章 函数设计规范](reference/ch8.md)
- [第9章 参数检查机制](reference/ch9.md)
- [第10章 错误处理机制](reference/ch10.md)
- [第11章 宏定义规范](reference/ch11.md)
- [第12章 头文件与包含守卫](reference/ch12.md)
- [第13章 条件编译](reference/ch13.md)
- [第14章 内存管理](reference/ch14.md)
- [第15章 指针安全](reference/ch15.md)
- [第16章 整数安全](reference/ch16.md)
- [第17章 字符串与缓冲区](reference/ch17.md)
- [第18章 并发与线程安全](reference/ch18.md)
- [第19章 可移植性](reference/ch19.md)
- [第20章 注释与文档](reference/ch20.md)
- [第21章 权威参考来源](reference/ch21.md)
- [第22章 GCC 内置原子操作](reference/ch22.md)
- [第23章 GCC 扩展与编译期检查](reference/ch23.md)
- [第24章 RAII 风格资源管理](reference/ch24.md)

## 配套头文件（reference/ 目录内）

- [atomic.h](reference/atomic.h) — GCC 用户态原子操作宏（第 22 章）
- [compile_checks.h](reference/compile_checks.h) — GCC 编译期检查宏（第 23 章）
- [raii.h](reference/raii.h) — RAII / cleanup 资源管理宏（第 24 章）

## 快速检查清单

- [ ] 文件名、头文件守卫、include 顺序符合第 1、12 章
- [ ] 命名带模块前缀，使用 snake_case（第 2 章）
- [ ] 数据类型选用 stdint/size_t，并参考第 5 章决策树与速查表（5.3–5.18）
- [ ] 变量声明即初始化；整函数变量在函数首、块变量在块首、定义与语句不混放（第 4 章 4.1 节）；结构体成员按对齐降序（第 6 章）
- [ ] 函数声明/定义：存储类 + 返回类型 + 函数名同一行，`{` 另起一行（第 3 章 3.2 节、第 8 章 8.2 节）
- [ ] 指针形参按语义加 `const`/`restrict`；声明与定义限定符一致（第 4、8 章 8.5 节）
- [ ] 每个异常返回点（错误码/NULL/失败 goto）在返回前有 `LogError`（**英文**）及足够上下文（第 10 章 10.4 节）
- [ ] 公共 API 有参数校验与明确返回值语义（第 8、9、10 章）
- [ ] 宏安全或改用 static inline；内存分配/释放配对（第 11、14 章）
- [ ] 指针、整数、字符串操作无 UB（第 15、16、17 章）
- [ ] 多线程共享整型计数/标志优先 atomic（第 22 章），复杂状态用锁（第 18 章）
- [ ] 公共 API 声明 nonnull/format/must_check；布局不变量用 static_assert 或 BUILD_BUG_ON（第 23 章）
- [ ] 多 exit 路径的资源释放用 raii.h 或 goto 清理；单 return 可用原生 close/free（第 24 章）
- [ ] 注释/Doxygen 使用**中文**说明「为什么」；`LogError`/`LogInfo` 等日志使用**英文**（第 20 章 20.4 节）；公共接口有 Doxygen

## 权威参考

完整参考链接见 [reference/ch21.md](reference/ch21.md)。
