# 枚举类型

*7类型与变量*

参考来源：[Linux Kernel §12 Macros, Enums](https://www.kernel.org/doc/Documentation/process/coding-style.rst)、
    SEI CERT INT09-C、ISO/IEC 9899:1999 §6.7.2.2

## 7.1 枚举定义规范

**反面示例（不规范）**

```c
/* 无命名空间前缀 */
enum {
    IDLE,      /* 极易冲突 */
    RUNNING,
    ERROR
};

/* 无哨兵值 */
enum color { RED, GREEN, BLUE };
```

**正面示例（规范）**

```c
/** 设备状态枚举
 * 始终使用模块前缀 + 显式初始值
 */
typedef enum dev_state {
    DEV_STATE_UNINIT  = 0,  /* 明确基值 */
    DEV_STATE_IDLE    = 1,
    DEV_STATE_RUNNING = 2,
    DEV_STATE_ERROR   = 3,
    DEV_STATE_MAX           /* 哨兵：用于边界检查 */
} dev_state_e;
```


## 7.2 枚举的使用规范

| 规则 | 说明 |
| --- | --- |
| 使用模块前缀 | 所有枚举值加 `MODULE_ENUM_` 前缀，防止全局命名冲突 |
| 显式赋值基值 | 第一个值显式赋为 `0`（或特定值），防止编译器默认值混淆 |
| 添加 MAX 哨兵值 | 末尾加 `_MAX` / `_COUNT` 值用于循环上界和边界检查 |
| typedef 简化使用 | 配合 typedef 使用，类型名后缀 `_e` 标识枚举 |
| switch 须处理所有值 | switch 枚举时须有 `default` 或覆盖所有值，配合 `-Wswitch` |
| 不要强制转换枚举 | 避免将整数直接强转为枚举类型（CERT INT09-C） |

**示例：枚举结合 switch 的规范用法**

```c
void dev_handle_state(dev_state_e state)
{
    /* 运行时有效性检查 */
    if (state >= DEV_STATE_MAX) {
        log_error("Invalid state: %d", (int)state);
        return;
    }

    switch (state) {
    case DEV_STATE_IDLE:
        dev_do_idle();
        break;
    case DEV_STATE_RUNNING:
        dev_do_run();
        break;
    case DEV_STATE_ERROR:
        dev_do_recover();
        break;
    default:
        /* DEV_STATE_UNINIT 等未处理状态 */
        log_warn("Unhandled state: %d", (int)state);
        break;
    }
}
```
