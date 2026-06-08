#!/usr/bin/env python3
"""Generate skill reference/ch23.md from gcc-compile-time-checks.html."""

from __future__ import annotations

from pathlib import Path

from gcc_compile_checks_to_md import gcc_checks_body_md

ROOT = Path(__file__).resolve().parents[1]


def compile_checks_h_content() -> str:
    """Userland compile-time check macros from gcc-compile-time-checks.html."""
    return r"""/*
 * compile_checks.h — 用户态编译期检查工具集
 *
 * 基于：
 *   - GCC 官方文档: https://gcc.gnu.org/onlinedocs/gcc/Common-Function-Attributes.html
 *   - Linux kernel: include/linux/build_bug.h
 *                   include/linux/compiler_attributes.h
 *                   tools/include/linux/compiler.h
 *   - Linux kernel: https://github.com/torvalds/linux/blob/master/include/linux/build_bug.h
 *
 * 要求：gcc -std=gnu99 -O1 (或 -O2)
 *       对于 static_assert：gcc >= 4.6
 *       对于 compiletime_assert：gcc >= 4.3 且 __OPTIMIZE__
 */

#ifndef COMPILE_CHECKS_H
#define COMPILE_CHECKS_H

#include <stddef.h>  /* offsetof, size_t */

/* ── 1. 函数属性检查 ─────────────────────────────────── */

/* 指定参数不得为 NULL（1-indexed，不带参数=所有指针参数）
 * GCC: https://gcc.gnu.org/onlinedocs/gcc/Common-Function-Attributes.html
 */
#define __nonnull(...)    __attribute__((__nonnull__(__VA_ARGS__)))
#define __nonnull_all     __attribute__((__nonnull__))

/* 格式字符串检查
 * GCC: https://gcc.gnu.org/onlinedocs/gcc/Common-Function-Attributes.html
 */
#define __printf(fmt, args)  __attribute__((__format__(__printf__, fmt, args)))
#define __scanf(fmt, args)   __attribute__((__format__(__scanf__,  fmt, args)))

/* 变参哨兵检查
 * GCC: https://gcc.gnu.org/onlinedocs/gcc/Common-Function-Attributes.html
 */
#define __sentinel          __attribute__((__sentinel__))
#define __sentinel_at(pos)  __attribute__((__sentinel__(pos)))

/* 返回值必须被使用
 * kernel: include/linux/compiler_attributes.h
 * GCC: https://gcc.gnu.org/onlinedocs/gcc/Common-Function-Attributes.html
 */
#define __must_check        __attribute__((__warn_unused_result__))

/* 废弃声明
 * GCC: https://gcc.gnu.org/onlinedocs/gcc/Common-Function-Attributes.html
 */
#define __deprecated        __attribute__((__deprecated__))
#define __deprecated_msg(m) __attribute__((__deprecated__(m)))

/* 编译期 error/warning 属性（GCC >= 4.3）
 * kernel: include/linux/compiler_attributes.h
 * GCC: https://gcc.gnu.org/onlinedocs/gcc/Common-Function-Attributes.html
 */
#if defined(__GNUC__) && \
    (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 3))
# define __compiletime_error(msg) __attribute__((__error__(msg)))
# define __compiletime_warning(msg) __attribute__((__warning__(msg)))
#else
# define __compiletime_error(msg)
# define __compiletime_warning(msg)
#endif

#ifndef __noreturn
#define __noreturn __attribute__((__noreturn__))
#endif

/* ── 2. 类型检查与类型安全宏 ──────────────────────────── */

/* 两个变量/表达式是否是同一类型（忽略 const/volatile）
 * kernel: tools/include/linux/compiler.h
 * GCC: https://gcc.gnu.org/onlinedocs/gcc/Other-Builtins.html
 */
#define __same_type(a, b) \
    __builtin_types_compatible_p(__typeof__(a), __typeof__(b))

/* 类型安全的 swap 宏（内核风格）*/
#define swap(a, b)                              \
    do {                                        \
        __typeof__(a) __tmp = (a);              \
        (a) = (b); (b) = __tmp;                 \
    } while (0)

/* ── 3. 静态断言（compiletime_assert 机制）─────────────── */

/* kernel: tools/include/linux/compiler.h */
#ifdef __OPTIMIZE__
# define __compiletime_assert(condition, msg, prefix, suffix) \
    do {                                                        \
        __noreturn extern void prefix##suffix(void)           \
            __compiletime_error(msg);                           \
        if (!(condition))                                       \
            prefix##suffix();                                   \
    } while (0)
#else
# define __compiletime_assert(condition, msg, prefix, suffix) \
    do {} while (0)
#endif

#define _compiletime_assert(condition, msg, prefix, suffix) \
    __compiletime_assert(condition, msg, prefix, suffix)

#define compiletime_assert(condition, msg) \
    _compiletime_assert(condition, msg, __compiletime_assert_, __COUNTER__)

/* ── 4. BUILD_BUG_ON 宏族 ────────────────────────────── */

/* kernel: include/linux/build_bug.h */

/* 条件为真时编译错误，结果为 int 0（可用于表达式上下文）*/
#define BUILD_BUG_ON_ZERO(e) \
    ((int)(sizeof(struct { int:(-!!(e)); })))

/*
 * BUILD_BUG_ON_INVALID() permits the compiler to check the validity of the
 * expression but avoids the generation of any code, even if that expression
 * has side-effects.
 */
#define BUILD_BUG_ON_INVALID(e) ((void)(sizeof((long)(e))))

/* 条件为真时编译错误（带自定义消息，语句形式）*/
#define BUILD_BUG_ON_MSG(cond, msg) compiletime_assert(!(cond), msg)

/* 条件为真时编译错误（语句形式）*/
#define BUILD_BUG_ON(condition) \
    BUILD_BUG_ON_MSG(condition, "BUILD_BUG_ON failed: " #condition)

/* 总是编译错误（标记不应执行的死代码路径）*/
#define BUILD_BUG() BUILD_BUG_ON_MSG(1, "BUILD_BUG failed")

/* n 必须是 2 的幂 */
#define BUILD_BUG_ON_NOT_POWER_OF_2(n) \
    BUILD_BUG_ON((n) == 0 || (((n) & ((n) - 1)) != 0))

/* 编译期断言两表达式类型相同 */
#define ASSERT_SAME_TYPE(a, b) \
    BUILD_BUG_ON(!__same_type(a, b))

/* ── 5. static_assert 封装 ───────────────────────────── */

/* kernel: include/linux/build_bug.h
 * 注：_Static_assert 自 GCC 4.6 在 gnu99 下即可用
 */
#if defined(__GNUC__) && \
    (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6))
# define static_assert(expr, ...) __static_assert(expr, ##__VA_ARGS__, #expr)
# define __static_assert(expr, msg, ...) _Static_assert(expr, msg)
#endif

/* ── 6. 结构体字段偏移检查 ───────────────────────────── */

/* kernel: include/linux/build_bug.h */
#define ASSERT_STRUCT_OFFSET(type, field, expected_offset)       \
    BUILD_BUG_ON_MSG(offsetof(type, field) != (expected_offset),  \
        "Offset of " #field " in " #type " has changed.")

/* ── 7. 安全的 ARRAY_SIZE ────────────────────────────── */

#define ARRAY_SIZE(arr) \
    (sizeof(arr) / sizeof((arr)[0])                             \
     + BUILD_BUG_ON_ZERO(__same_type(arr, &(arr)[0])))
/* 若 arr 是指针（非数组），编译报错 */

#endif /* COMPILE_CHECKS_H */
"""


def write_compile_checks_header(ref_dir: Path) -> None:
    (ref_dir / "compile_checks.h").write_text(compile_checks_h_content(), encoding="utf-8")


def compile_checks_scenario_md() -> str:
    return """## 23.1 使用场景与选型

### [建议] 能在编译期发现的约束，优先用 GCC 扩展在编译期检查，而非留到运行期

**适用条件（满足任一类即应考虑本章机制）：**

1. **函数接口契约**可在声明中表达：指针非空、格式字符串类型、变参 NULL 终止、返回值不可忽略。
2. **不变量/布局约束**在编译期可确定：结构体大小、字段偏移、类型兼容性、常量表达式真假。
3. **API 演进**需引导调用方迁移：废弃函数/类型/变量。
4. 项目使用 **GCC/Clang** 且可接受 **gnu99** 扩展（与内核 `build_bug.h` / `compiler_attributes.h` 一致）。

**优先顺序：**

| 场景 | 推荐方案 | 说明 |
| --- | --- | --- |
| 公共 API 指针参数语义上非空 | `__nonnull` / `__nonnull(n…)` | 见 23.4；配合 `-Wnonnull` |
| 自定义 `printf`/`scanf` 风格日志 | `__printf` / `__scanf` | 见 23.5；配合 `-Wformat` |
| 变参列表必须以 NULL 结尾 | `__sentinel` | 见 23.6 |
| 错误码/关键返回值不可忽略 | `__must_check` | 见 23.7；见 [第10章](ch10.md) 错误处理 |
| 全局/文件作用域不变量 | `static_assert` / `_Static_assert` | 见 23.15；[第9章](ch9.md) 9.1 基础 |
| 函数体内或宏内布局/类型检查 | `BUILD_BUG_ON` / `BUILD_BUG_ON_ZERO` | 见 23.13–23.14 |
| 类型安全宏（防指针当数组） | `typeof` + `__same_type` + `ARRAY_SIZE` | 见 23.11–23.12 |
| 仅编译期可见的 NULL/常量误用 | `nonnull` + 静态可见 NULL | 运行时 NULL 仍须运行期检查，见 [第9章](ch9.md) 9.2 |

**反面示例（契约只写在注释里 — 编译器无法 enforcement）**

```c
/* 调用者必须检查返回值，path 不能为 NULL —— 仅文档，无检查 */
int open_file(const char *path, int flags);

void caller(void)
{
    open_file(NULL, 0);   /* ❌ 运行时才崩溃，编译无警告 */
    open_file("/tmp/x", 0);  /* ❌ 忽略返回值，静默失败 */
}
```

**反面示例（运行期 assert 检查本可在编译期捕获的不变量）**

```c
void init(void)
{
    assert(sizeof(struct wire_hdr) == 8);  /* ❌ Release 构建 -DNDEBUG 后消失 */
}
```

**正面示例（声明期契约 + 编译期布局 — 零运行开销）**

```c
#include "compile_checks.h"

static_assert(sizeof(struct wire_hdr) == 8, "协议头大小变更");

int open_file(const char *path, int flags)
    __nonnull(1) __must_check;

void send_msg(struct wire_hdr *h)
{
    BUILD_BUG_ON(offsetof(struct wire_hdr, flags) != 6);
    (void)h;
}
```

**正面示例（格式字符串与 sentinel — 调用点静态诊断）**

```c
void log_msg(int level, const char *fmt, ...)
    __printf(2, 3);

void build_cmd(const char *cmd, ...)
    __sentinel;

void ok(void)
{
    log_msg(1, "%d", 42);
    build_cmd("ls", "-l", "/tmp", NULL);
}
```

> 编译期属性与断言 **不能替代** 对用户输入、外部数据的运行期校验；二者互补，见 [第9章 参数检查机制](ch9.md)。

---

"""


def compile_checks_chapter_md() -> str:
    header = """# GCC 扩展与编译期检查

*23高级主题 · 静态分析与契约*

参考来源：[compile_checks.h](compile_checks.h)、[GCC Function Attributes](https://gcc.gnu.org/onlinedocs/gcc/Common-Function-Attributes.html)、[Linux kernel build_bug.h](https://github.com/torvalds/linux/blob/master/include/linux/build_bug.h)、[第9章 参数检查机制](ch9.md)

> 本章规范适用于 **GCC/Clang + gnu99** 用户态项目。宏定义见同目录 [compile_checks.h](compile_checks.h)；`compiletime_assert` / `BUILD_BUG_ON` 需 **-O1+** 优化才能可靠生效。

"""
    relationship = """

---

## 23.19 与第 9 章的关系

| 主题 | 第 9 章 | 本章 |
| --- | --- | --- |
| `_Static_assert` 基础用法 | ✅ 9.1 | 23.15 扩展与内核封装 |
| 运行期参数校验 | ✅ 9.2 主方案 | 属性不能替代 |
| `assert()` 用于不变量 | ✅ 9.3 | 编译期断言优先于 Release 消失的 assert |
| 函数契约（nonnull/format） | 部分涉及 | ✅ 23.4–23.7 系统说明 |
| 类型/布局编译期断言 | BUILD_ASSERT 示例 | ✅ BUILD_BUG_ON 族 + typeof |

编写或审查代码时：**先判断约束能否在编译期表达 → 能则用本章机制 → 外部输入与运行时条件仍用 ch9 运行期检查**。

"""
    return header + compile_checks_scenario_md() + gcc_checks_body_md(start_section=2) + relationship


def write_compile_checks_chapter(ref_dir: Path) -> None:
    write_compile_checks_header(ref_dir)
    (ref_dir / "ch23.md").write_text(compile_checks_chapter_md(), encoding="utf-8")


if __name__ == "__main__":
    print(compile_checks_chapter_md())
