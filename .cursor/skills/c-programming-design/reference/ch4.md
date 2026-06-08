# 集中式错误处理 (goto 清理模式)

*Centralized Exit with Cleanup goto*

参考来源：
- [🔗 Linux Kernel Docs §7 Centralized Exiting](https://www.kernel.org/doc/html/latest/process/coding-style.html#centralized-exiting-of-functions)

### 集中式错误处理 (goto 清理模式)

Linux 内核第7章明确为 `goto` 正名：在 C 语言中，使用 goto 实现**单一出口、集中清理**是经过大量工程实践验证的正确模式。这解决了 C 语言没有 RAII / try-finally 的痛点，避免了多个 return 语句导致的资源泄漏。

> "The goto statement comes in handy when a function exits from multiple locations and some common work such as cleanup has to be done... nesting is reduced; errors by not updating individual exit points when making modifications are prevented."
> — — Linux Kernel Coding Style, §7 Centralized exiting of functions

**✅ 设计优势**
- 资源释放代码集中在一处，不易遗漏
- 新增错误路径时无需在多处添加清理
- 减少深度嵌套 if-else（金字塔问题）
- 易于设置调试断点（单一 return 点）
- 代码可读性远优于重复的清理代码
- 需要多步骤资源分配的初始化函数
- 内核驱动中的设备探测函数
- 需要加锁/解锁的临界区函数
- 数据库事务中的多步操作
- 复杂的文件操作序列

**图5：goto 集中清理模式的错误流示意**

- goto 清理模式：错误路径汇聚到统一清理标签
- malloc buf
- malloc foo
- do work...
- out_free_foo: / out_free_buf:
- 分配失败
- goto
- 任意步骤失败
- → 跳转统一清理
- ✓ 资源不泄漏

### 代码示例对比

**反例 — 多处 return 导致泄漏**

```c
/* ❌ 反例：多处提前 return，
   容易遗漏资源释放 */
int do_work(int n) {
    char *buf = malloc(n);
    if (!buf)
        return -ENOMEM;

    struct foo *f = malloc(sizeof(*f));
    if (!f) {
        /* ❌ 忘记 free(buf) */
        return -ENOMEM;
    }

    if (condition1) {
        /* ❌ 两处都要记得释放 */
        free(buf);
        free(f);
        return -EINVAL;
    }
    free(buf);
    free(f);
    return 0;
}
```

**正例 — 内核风格 goto 清理**

```c
/* ✅ 正例：Linux 内核推荐风格 */
int fun(int a) {
    int result = 0;
    char *buffer;
    struct foo *foo = NULL;

    buffer = kmalloc(SIZE, GFP_KERNEL);
    if (!buffer)
        return -ENOMEM;

    foo = kmalloc(sizeof(*foo),
                  GFP_KERNEL);
    if (!foo) {
        result = -ENOMEM;
        goto out_free_buf;
    }

    if (condition1) {
        result = -EINVAL;
        goto out_free_foo; /* 清晰! */
    }
    result = 1;

out_free_foo:
    kfree(foo);
out_free_buf:
    kfree(buffer);
    return result; /* 唯一出口 */
}
```