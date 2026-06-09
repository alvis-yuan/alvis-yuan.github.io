# 参数检查机制

*9函数与接口*

参考来源：[SEI CERT API00-C — Functions should validate their parameters](https://wiki.sei.cmu.edu/confluence/display/c/API00-C)、
    [ARR38-C](https://wiki.sei.cmu.edu/confluence/display/c/ARR38-C)

## 9.1 编译期检查（_Static_assert）

C99 引入了 `_Static_assert`（C11 标准化），用于在编译阶段验证不变量。

**示例：编译期参数/类型检查示例**

```c
#include <stdint.h>
#include <assert.h>

/* 验证关键类型大小（移植性保障）*/
_Static_assert(sizeof(uint32_t) == 4,
               "uint32_t must be 4 bytes");
_Static_assert(sizeof(void *) >= 4,
               "Pointer must be at least 32-bit");

/* 验证结构体布局（协议帧匹配）*/
_Static_assert(sizeof(msg_header_t) == 12,
               "Protocol header size changed!");

/* 验证枚举值范围 */
_Static_assert(DEV_STATE_MAX <= 16,
               "State table too large");

/* C99 兼容的编译期断言替代方案 */
#define BUILD_ASSERT(cond) \
    typedef char _build_assert_##__LINE__[(cond) ? 1 : -1]

BUILD_ASSERT(sizeof(int) == 4);
```


## 9.2 运行期参数检查（防御性编程）

**示例：公共 API 参数验证完整模式**

```c
#include <errno.h>
#include <stdbool.h>
#include "raii.h"

/**
 * @brief 写入数据到缓冲区（含完整参数校验）
 */
int32_t buf_write(buf_ctx_t *ctx,
                  const uint8_t *data,
                  size_t         len)
{
    /* ① 指针非空检查 */
    if (ctx == NULL || data == NULL) {
        LogError("invalid argument: ctx=%p data=%p", (void *)ctx, (const void *)data);
        return -EINVAL;
    }

    /* ② 值域检查 */
    if (len == 0U || len > BUF_MAX_SIZE) {
        LogError("invalid len=%zu (max=%u)", len, (unsigned)BUF_MAX_SIZE);
        return -EINVAL;
    }

    /* ③ 状态检查 */
    if (ctx->state != BUF_STATE_READY) {
        LogError("ctx busy: state=%d", (int)ctx->state);
        return -EBUSY;
    }

    /* ④ 容量检查（防溢出）*/
    if (len > (ctx->capacity - ctx->write_pos)) {
        LogError("no space: len=%zu free=%zu",
                 len, ctx->capacity - ctx->write_pos);
        return -ENOSPC;
    }

    /* 实际操作 */
    memcpy(ctx->buf + ctx->write_pos, data, len);
    ctx->write_pos += len;
    return (int32_t)len;
}
```


## 9.3 assert 的正确使用

### [必须] 区分 assert 与错误处理的应用场景

`assert()` 用于检查**程序员错误**（不应该发生的条件）——在 Release 构建中会被 `-DNDEBUG` 禁用。

      **不要**用 `assert` 检查运行时输入、用户数据或系统资源——应使用显式错误返回。

**正面示例**

```c
/* ✅ 正确: 内部不变量检查（编程契约）*/
assert(ctx != NULL);       /* 内部调用时不应为NULL */
assert(index < ARRAY_SIZE); /* 索引越界是程序bug */
```

**反面示例**

```c
/* ❌ 错误: 用assert检查外部输入 */
assert(malloc(n) != NULL);  /* malloc失败是运行时错误！*/
```

> 除 `_Static_assert` 外，GCC **`__attribute__` 契约检查**（nonnull、format、must_check 等）与 **BUILD_BUG_ON 宏族**见 [第23章 GCC 扩展与编译期检查](ch23.md)；运行期参数校验仍以 9.2 为主。
> 参数校验失败等异常返回点的日志要求见 [第10.4 章](ch10.md#104-异常返回点日志)（**必须**）。
