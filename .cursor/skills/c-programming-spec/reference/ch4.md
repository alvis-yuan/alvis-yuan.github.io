# 变量声明与修饰符

*4类型与变量*

参考来源：[SEI CERT DCL30-C](https://wiki.sei.cmu.edu/confluence/display/c/DCL30-C)、
    [DCL31-C](https://wiki.sei.cmu.edu/confluence/display/c/DCL31-C)、ISO/IEC 9899:1999 §6.7

## 4.1 变量声明规则

### [必须] 声明时必须初始化

C99 允许在代码块内声明变量，但必须在声明时初始化或在使用前明确赋值。
      未初始化的自动变量具有不确定值（undefined behavior），是常见 bug 来源。

### [必须] 局部变量定义位置（C99）

遵循 C99 块作用域声明能力，但**定义位置**须按作用域分层，禁止与可执行语句交错混放：

| 作用域 | 定义位置 |
| --- | --- |
| 整个函数都需要 | 函数体 `{` 之后、首条可执行语句之前 |
| 仅某个 `{ … }` 块需要 | 该块 `{` 之后、块内首条可执行语句之前 |
| `for` 循环计数器 | `for (int i = …)` 初始化子句（C99 标准写法） |

**规则摘要：**

1. 遵循 C99 规范（含 `for (int i = 0; …)` 等）
2. 代码块内部的定义，放在代码块开始处
3. 整函数使用的变量，放在函数开始处
4. 禁止把变量定义和可执行代码混合乱放——先定义区，后逻辑区

**反面示例（不规范）**

```c
int ret;            /* 未初始化！*/
char *buf;          /* 未初始化指针 */
int a, b, c;       /* 不要一行多变量 */

/* C89 风格：函数顶堆声明、远处再赋值 */
int i;
int result;
/* 很多代码 ... */
i = 0;

/* 定义与语句交错混放 */
ret = do_work();
int saved = errno;  /* 错：应放在函数/块开始处 */
if (ret < 0) {
    LogError("failed");
    size_t n = (size_t)(-ret);  /* 错：应放在 if 块开始处 */
}
```

**正面示例（规范（C99））**

```c
int32_t foo(const char *path)
{
    int32_t ret = 0;        /* 整函数使用 → 函数开始处 */
    char *buf = NULL;

    if (path == NULL) {
        int saved = EINVAL; /* 仅本块使用 → 块开始处 */
        LogError("path is NULL (errno=%d)", saved);
        return -saved;
    }

    for (int i = 0; i < n; i++) {   /* for 初始化子句 */
        int result = 0;             /* 循环体块开始处 */
        result = process(i);
        handle(result);
    }

    return ret;
}
```


## 4.2 存储类修饰符

| 修饰符 | 作用域 | 用途与规范 |
| --- | --- | --- |
| `static`（文件作用域） | 本翻译单元 | 模块私有变量/函数，**优先使用**来限制可见性 |
| `static`（函数内） | 函数内，持久 | 谨慎使用，会导致线程不安全；须注释说明用途 |
| `extern` | 跨翻译单元 | 只在头文件中声明，对应源文件中定义；全局变量尽量避免 |
| `const` | 限定符 | 所有只读参数和数据必须加 `const`；优先于 `#define` |
| `restrict` | 限定符 | 独占缓冲区访问且无别名时**必须**标注；有重叠可能时**禁止**使用（详见 [8.5 节](ch8.md#85-指针形参修饰符)） |
| `volatile` | 限定符 | 仅用于硬件寄存器、中断共享变量；不可替代同步原语 |
| `register` | 提示 | C99 中无实际意义，现代编译器自动优化；**不推荐使用** |

### [必须] 指针形参须按语义加 `const` / `restrict`

函数指针形参的限定符规则见 [第 8.5 章](ch8.md#85-指针形参修饰符)。摘要：

- 仅读取指向对象 → `const T *`
- 写入或读写缓冲区 → `T *`（不对指向数据加 `const`）
- 独占顺序访问且同次调用无重叠别名 → `T *restrict` / `const T *restrict`
- 头文件声明与源文件定义的限定符必须一致

**示例：修饰符综合示例**

```c
/* const 的正确用法 */
const uint8_t *buf;          /* 指向常量数据的指针 */
uint8_t * const ptr;          /* 常量指针（地址不变）*/
const uint8_t * const cptr;  /* 指针和数据都是常量 */

/* volatile: 硬件寄存器 */
volatile uint32_t * const UART_STATUS =
    (volatile uint32_t *)0x40011000U;

/* static: 模块私有计数器 */
static uint32_t s_error_count = 0U;

/* extern: 跨模块共享（在 .h 中声明）*/
extern const char * const g_version_str;
```
