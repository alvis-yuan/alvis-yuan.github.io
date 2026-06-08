# 变量声明与修饰符

*4类型与变量*

参考来源：[SEI CERT DCL30-C](https://wiki.sei.cmu.edu/confluence/display/c/DCL30-C)、
    [DCL31-C](https://wiki.sei.cmu.edu/confluence/display/c/DCL31-C)、ISO/IEC 9899:1999 §6.7

## 4.1 变量声明规则

### [必须] 声明时必须初始化

C99 允许在代码中任意位置声明变量，但必须在声明时初始化或在使用前明确赋值。
      未初始化的自动变量具有不确定值（undefined behavior），是常见 bug 来源。

**反面示例（不规范）**

```c
int ret;            /* 未初始化！*/
char *buf;          /* 未初始化指针 */
int a, b, c;       /* 不要一行多变量 */

/* C89风格：所有声明堆在顶部 */
int i;
int result;
/* 很多代码 ... */
i = 0;
```

**正面示例（规范（C99））**

```c
int ret = 0;        /* 声明即初始化 */
char *buf = NULL;   /* 指针初始化为 NULL */

/* C99: 就近声明，可读性更好 */
for (int i = 0; i < n; i++) {
    int result = process(i);
    handle(result);
}
```


## 4.2 存储类修饰符

| 修饰符 | 作用域 | 用途与规范 |
| --- | --- | --- |
| `static`（文件作用域） | 本翻译单元 | 模块私有变量/函数，**优先使用**来限制可见性 |
| `static`（函数内） | 函数内，持久 | 谨慎使用，会导致线程不安全；须注释说明用途 |
| `extern` | 跨翻译单元 | 只在头文件中声明，对应源文件中定义；全局变量尽量避免 |
| `const` | 限定符 | 所有只读参数和数据必须加 `const`；优先于 `#define` |
| `volatile` | 限定符 | 仅用于硬件寄存器、中断共享变量；不可替代同步原语 |
| `register` | 提示 | C99 中无实际意义，现代编译器自动优化；**不推荐使用** |

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
