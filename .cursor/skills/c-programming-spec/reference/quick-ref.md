# C 编程规范速查表

*配合 SKILL.md 使用；完整示例与条文见 `ch*.md`。*

---

## 类型速查

| 用途 | 类型 |
| --- | --- |
| 数组大小、索引、strlen、malloc 参数 | `size_t` |
| 指针相减 | `ptrdiff_t` |
| 指针与整数互转 | `uintptr_t` / `intptr_t` |
| 协议/硬件/位域/文件格式 | `uint8_t`…`uint64_t` |
| 嵌入式省内存 | `int_leastN_t` / `uint_leastN_t` |
| 热循环性能 | `int_fastN_t` / `int` |
| 布尔 | `bool` |
| 字节流 | `unsigned char` / `uint8_t` |
| 宽字符 | `wchar_t` / `char16_t` / `char32_t` |
| 最大整数 / `%jd` | `intmax_t` / `uintmax_t` |
| 信号处理共享 | `sig_atomic_t` |

**陷阱：** `size_t` 逆向循环 `i>=0` 死循环；有符号溢出 UB；无符号溢出模 2ⁿ。

---

## const / restrict 对照

| 语义 | 写法 |
| --- | --- |
| 只读输入 | `const T *` |
| 可写输出 | `T *` |
| 只读句柄 | `const obj_t *` |
| 独占拷贝（memcpy 语义） | `uint8_t *restrict dst, const uint8_t *restrict src` |
| 重叠移动（memmove 语义） | 双方**禁** restrict |
| 单标量输出 | `int *out`（restrict 可选） |

---

## 返回值语义

| 类型 | 命名 | 成功 | 失败 |
| --- | --- | --- | --- |
| 命令型 | `do_xxx` / `send` | `0` | 负 errno |
| 谓词型 | `is_` / `has_` | 非 0 / true | `0` / false |
| 计算型 | `get_` / `find` | 有效值 | NULL / 哨兵 |

---

## 错误处理模式

```c
/* 系统调用 */
if (fd < 0) {
    int saved = errno;
    LogError("open(%s) failed: %s (errno=%d)", path, strerror(saved), saved);
    return -saved;
}

/* REQUIRE + cleanup（须 out: 标签）*/
REQUIRE(ptr != NULL, "ptr is NULL");
autofd int fd = open(...);
REQUIRE(fd >= 0, "open failed");
out:
    return ret;
```

**goto 链标签示例：** `out_free_buf` → `out_free_fd` → `out`

---

## 资源管理决策

| 条件 | 方案 |
| --- | --- |
| 单 return，无 fd/堆/锁 | 原生 close/free |
| ≥2 退出点 + fd/堆/锁 | `autofd` / `autofree` / `WITH_LOCK` + `REQUIRE` |
| 资源转移给调用者 | `autofd_steal` / `autoptr_steal` |
| 自定义析构 | `DEFINE_AUTOPTR_CLEANUP_FUNC` + `autoptr(T)` |
| 引用计数 | `DEFINE_AUTOREF_OPS` + `autoref(T)` |
| 无 GCC cleanup（MSVC） | ch10 手写 goto 链 |

---

## atomic vs 锁

| 场景 | 方案 |
| --- | --- |
| `g_count++`、引用计数归零 | `atomic_inc/dec` |
| 一次性启动标志 | `atomic_cmpxchg` |
| 多字段/链表/阻塞 I/O | `pthread_mutex_t` + `WITH_LOCK` |
| 硬件 MMIO | `volatile`（非同步） |

**atomic.h 常用：** `ATOMIC_INIT` · `atomic_read/set` · `atomic_inc/dec` · `atomic_add/sub` · `*_return` · `atomic_cmpxchg`

**禁：** 直接读写 `atomic_t.counter`；用 `volatile` 同步。

---

## 编译期检查

| 需求 | 用法 |
| --- | --- |
| 参数非空 | `__attribute__((nonnull(1,2)))` |
| 格式串 | `__attribute__((format(printf, 2, 3)))` |
| 变参以 NULL 结尾 | `__attribute__((sentinel))` |
| 返回值必用 | `__attribute__((warn_unused_result))` |
| 布局/条件 | `static_assert(expr, msg)` |
| 函数体内恒真即错 | `BUILD_BUG_ON(c)` |
| 表达式上下文 | `BUILD_BUG_ON_ZERO(c)` |
| 数组元素数 | `ARRAY_SIZE(arr)` |

---

## 15 条禁忌（速查）

1. 有符号溢出后检查
2. volatile 当同步
3. 多线程普通 int++
4. assert 检查外部输入
5. 头文件定义函数/全局变量
6. 无模块前缀全局符号
7. free 后不置 NULL / double-free
8. realloc 直接赋原指针
9. restrict 用于重叠缓冲区
10. 输出参数 const
11. 声明≠定义限定符
12. 函数宏含 return/副作用参数
13. size_t 逆向 for 循环
14. VLA 大数组
15. 信号处理中非 async-signal-safe 调用

---

## 配套头文件

| 文件 | 用途 |
| --- | --- |
| [raii.h](raii.h) | LogError、REQUIRE、autofd/autofree、WITH_LOCK、autoptr、autoref |
| [atomic.h](atomic.h) | atomic_t、RMW、cmpxchg |
| [compile_checks.h](compile_checks.h) | BUILD_BUG_ON、ARRAY_SIZE、ASSERT_SAME_TYPE |
