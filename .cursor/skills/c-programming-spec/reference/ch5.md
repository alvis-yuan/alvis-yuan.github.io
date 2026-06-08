# 数据类型选择

*5类型与变量*

参考来源：[SEI CERT INT00-C](https://wiki.sei.cmu.edu/confluence/display/c/INT00-C)、
    ISO/IEC 9899:1999 §7.18（stdint.h）、
    [Barr Group — C99 Fixed-Width Integers](https://barrgroup.com/embedded-systems/how-to/c-fixed-width-integers-c99)、
    [cppreference — 整数类型](https://en.cppreference.com/w/c/types/integer)（5.3–5.18 扩展）

## 5.1 使用精确宽度整数类型

C99 在 `<stdint.h>` 中定义了精确宽度类型，消除了平台差异。在协议、硬件交互、数组索引等场景下，**必须**优先使用这些类型。

**图5.1 — C99 stdint.h 整数类型分类（精确宽度为首选）**

- C99 整数类型层次
- 精确宽度
- 最小宽度
- 最快最小
- 指针大小
- int8_t
- int16_t
- int32_t
- int64_t
- uint*_t
- int_least8_t
- int_least16_t
- int_least32_t
- int_least64_t
- int_fast8_t
- int_fast16_t
- int_fast32_t
- int_fast64_t
- intptr_t
- uintptr_t
- intmax_t
- size_t
- ptrdiff_t

| 场景 | 推荐类型 | 禁用类型 |
| --- | --- | --- |
| 硬件寄存器/协议字段 | `uint8_t`/`uint16_t`/`uint32_t` | `int`/`short`/`long` |
| 数组大小、循环计数 | `size_t` | `int`（可能为负） |
| 指针差值计算 | `ptrdiff_t` | `int`/`long` |
| 指针转整数 | `uintptr_t` | `int`（32/64位不同） |
| 布尔值 | `bool`（C99 `<stdbool.h>`） | `int` 当 bool 用 |
| 字符处理 | `unsigned char` 或 `uint8_t` | `char`（符号未定义） |


## 5.2 浮点类型使用规范

### [建议] 不可直接比较浮点数相等

**反面示例**

```c
/* ❌ 错误：浮点直接比较 */
if (f == 1.0f) { }
```

**正面示例**

```c
/* ✅ 正确：使用误差范围比较 */
#include <math.h>
#define FLOAT_EPSILON  (1e-6f)
if (fabsf(f - 1.0f) < FLOAT_EPSILON) { }
```

---

> 以下为 **C 语言数据类型选择完全指南** 扩展内容（5.3–5.18），参考 [cppreference — 整数类型](https://en.cppreference.com/w/c/types/integer) 与 [Barr Group — Fixed-Width Integers](https://barrgroup.com/embedded-systems/how-to/c-fixed-width-integers-c99)。

## 5.3 日常编程类型选择决策树

以下是经典的 C 数据类型决策树，覆盖最常见场景。后续章节将系统讲解所有标准类型及更深层的选型因素。

- **表示内存大小、数组索引、字符串长度？（`sizeof` 返回值、`strlen()`、`malloc()` 参数）** → `size_t`
- **表示两个指针的差值距离？（指针相减的结果）** → `ptrdiff_t`
- **需要把指针转换为整数进行算术运算（内存对齐计算等）？** → `uintptr_t`
- **硬件寄存器、网络协议包、二进制文件格式 —— 需要精确位宽？** → `uint8_t / int16_t / uint32_t …`
- **需要特定位宽但对内存占用不敏感，希望运行尽可能快？** → `int_fast32_t / uint_fast8_t …`
- **需要特定位宽且内存有限，可牺牲些许速度？** → `int_least16_t / uint_least8_t …`
- **纯粹的位掩码操作、移位操作？** → `uint32_t`（等无符号定宽）
- **普通循环计数器，不关心具体大小（如 `for(i=0; i<10; i++)`）？** → `int`（性能最好）
- **表示真 / 假逻辑值？** → `bool`（<stdbool.h>）
- **处理普通 ASCII 字符和字符串？** → `char`（⚠ 符号性实现定义）
- **处理宽字符（Unicode/多字节字符集）？** → `wchar_t`（<wchar.h>）
- **需要表示平台上最大的整数值（用于通用算法）？** → `intmax_t / uintmax_t`
- **存储时间值（自 1970-01-01 的秒数）？** → `time_t`（<time.h>）
- **处理信号（可原子读写的整数）？** → `sig_atomic_t`（<signal.h>）

**参考链接**

- [📖 Wikipedia: C data types](https://en.wikipedia.org/wiki/C_data_types)
- [📖 cppreference: <stdint.h>](https://en.cppreference.com/w/c/header/stdint)

## 5.4 C 语言基本算术类型

C 语言提供四种基本算术类型说明符：`char`、`int`、`float`、`double`，以及修饰符 `signed`、`unsigned`、`short`、`long`。标准只规定了**最小**位宽，具体大小依赖实现平台。


### 整数类型

| 类型 | 最小位宽 | 标准范围下限 | 格式说明符 | 说明 |
| --- | --- | --- | --- | --- |
| signed char | ≥ 8 bit | −127 ~ +127 | %hhd | 有符号 char，与 char 同大小 |
| unsigned char | ≥ 8 bit | 0 ~ 255 | %hhu | 无符号 char，常用于字节操作 |
| short / short int | ≥ 16 bit | −32767 ~ +32767 | %hd | 短整数 |
| unsigned short | ≥ 16 bit | 0 ~ 65535 | %hu | 无符号短整数 |
| int / signed int | ≥ 16 bit | −32767 ~ +32767 | %d / %i | 平台自然整数，通常为 32 bit |
| unsigned int | ≥ 16 bit | 0 ~ 65535 | %u | 无符号自然整数 |
| long / long int | ≥ 32 bit | −2147483647 ~ +2147483647 | %ld | 长整数（32/64 bit 视平台） |
| unsigned long | ≥ 32 bit | 0 ~ 4294967295 | %lu | 无符号长整数 |
| long long | ≥ 64 bit | −9223372036854775807 ~ +9223372036854775807 | %lld | C99 引入 |
| unsigned long long | ≥ 64 bit | 0 ~ 18446744073709551615 | %llu | C99 引入 |

> ⚠️ **标准只规定最小宽度**
>       ISO/IEC 9899:1999 §5.2.4.2.1 规定了以上最小范围。`int` 在 32/64 位系统通常是 32 bit，`long` 在 LP64（Linux/macOS 64-bit）是 64 bit，而在 LLP64（Windows 64-bit）仅 32 bit。**不要假设任何基本整数类型的具体大小**，需要精确大小时请使用 `<stdint.h>` 中的定宽类型。


### 浮点类型

| 类型 | 通常位宽 | 精度 | 格式说明符 | 说明 |
| --- | --- | --- | --- | --- |
| float | 32 bit | ~7 位十进制 | %f %g %e %a | 单精度，IEEE 754 binary32（通常） |
| double | 64 bit | ~15 位十进制 | %lf %lg %le %la | 双精度，IEEE 754 binary64（通常） |
| long double | 80/128 bit | ~18~34 位 | %Lf | 扩展精度，x87 80-bit 或 128-bit 四精度，平台相关 |

标准并不强制要求 IEEE 754，但实际上主流平台（x86/ARM/RISC-V）均采用 IEEE 754 格式。Annex F（可选）要求实现必须支持 IEC 60559（即 IEEE 754）浮点运算。


### 字符类型的符号性陷阱

**⚠️ char 的符号性是实现定义的！**

**反面示例**

```c
// 危险：char 可能是无符号，导致 c >= 0 永远为真
char c = get_byte();
if (c < 0) { /* 可能永远不执行 */ }
```

**正面示例**

```c
// 安全：明确使用 unsigned char 处理字节数据
unsigned char byte = get_byte();
if (byte > 127) { /* 处理高位字节 */ }
```


### 布尔类型

**✅ bool / _Bool（C99 / C23）**

```c
#include <stdbool.h>  // C99

bool found = false;
bool is_valid = (ptr != NULL);   // 任何非零值 → true(1)
_Bool flag = 42;                  // 存储为 1
```

**参考链接**

- [📖 ISO/IEC 9899:1999 TC3 §5.2.4.2.1 Sizes of integer types](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1256.pdf)

## 5.5 精确宽度整数类型（Exact-width）

定义于 `<stdint.h>`（C99），要求恰好 N 位、无填充位，且有符号类型采用二进制补码。这些类型是**可选的**——若平台不支持对应宽度，则不定义该 typedef。

| 有符号 | 无符号 | 精确位宽 | 范围（有符号） | 可用性 |
| --- | --- | --- | --- | --- |
| int8_t | uint8_t | 8 bit | −128 ~ +127 | 可选 |
| int16_t | uint16_t | 16 bit | −32768 ~ +32767 | 可选 |
| int32_t | uint32_t | 32 bit | −2147483648 ~ +2147483647 | 可选 |
| int64_t | uint64_t | 64 bit | −2⁶³ ~ +2⁶³−1 | 可选 |

> ℹ️ **为什么是可选的？**
>       C 标准 §7.18.1.1/3 规定：这些 typedef 只有在实现存在"恰好 N 位、无填充位、有符号类型为补码"的整数类型时才须定义。例如某些 12-bit DSP 平台不提供 `int8_t`。主流桌面/嵌入式 ARM/x86 平台均提供全部四个宽度。

```c
#include <stdint.h>

// 网络协议头（精确位宽，不允许填充）
struct IpHeader {
    uint8_t  version_ihl;    // 4+4 bit 字段
    uint8_t  dscp_ecn;
    uint16_t total_length;
    uint32_t src_ip;
    uint32_t dst_ip;
};

// 硬件寄存器映射
volatile uint32_t* const GPIO_OUT = (uint32_t*)0x40020014;
```

**参考链接**

- [📖 cppreference: Standard library header <stdint.h> (C99)](https://en.cppreference.com/w/c/header/stdint)
- [📖 ISO/IEC 9899:1999 TC3 §7.18 Integer types <stdint.h>](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1256.pdf)

## 5.6 最小宽度类型（Minimum-width）

`int_least<em>N</em>_t` / `uint_least<em>N</em>_t` 是**必须**存在的类型（C 标准要求实现提供），保证至少有 N 位，且在所有满足条件的类型中占用空间最小。

| 有符号 | 无符号 | 最小位宽 | 可用性 | 典型用途 |
| --- | --- | --- | --- | --- |
| int_least8_t | uint_least8_t | ≥ 8 bit | 必须 | 节省内存的字节计数器 |
| int_least16_t | uint_least16_t | ≥ 16 bit | 必须 | 嵌入式小范围计数 |
| int_least32_t | uint_least32_t | ≥ 32 bit | 必须 | 通用整数，跨平台安全 |
| int_least64_t | uint_least64_t | ≥ 64 bit | 必须 | 大数值、时间戳 |

**💡 least vs exact**

## 5.7 最快宽度类型（Fastest）

`int_fast<em>N</em>_t` / `uint_fast<em>N</em>_t` 是**必须**存在的类型，保证至少 N 位，且是该平台上所有满足位宽要求的整数类型中运算**最快**的。"最快"由实现定义，通常是平台寄存器原生宽度。

| 有符号 | 无符号 | 最小位宽 | 典型实际宽度（x86-64） | 说明 |
| --- | --- | --- | --- | --- |
| int_fast8_t | uint_fast8_t | ≥ 8 bit | 通常 64 bit（或 32 bit） | 快速字节级运算 |
| int_fast16_t | uint_fast16_t | ≥ 16 bit | 通常 64 bit | 快速短整数运算 |
| int_fast32_t | uint_fast32_t | ≥ 32 bit | 通常 32/64 bit | 快速通用整数 |
| int_fast64_t | uint_fast64_t | ≥ 64 bit | 64 bit | 快速大整数 |

- fast vs least vs exact — 选型哲学对比
- int32_t
- 精确 32 bit
- 可选，无填充，补码
- int_least32_t
- 至少 32 bit，最小空间
- 必须存在，节省内存
- int_fast32_t
- 至少 32 bit，最快速度
- 必须存在，优化性能
- 协议/硬件
- 嵌入式/内存受限
- 通用算法/循环
- 可移植性 ★★★
- 安全性 ★★★★★
- 可移植性 ★★★★★
- 内存 ★★★★★
- 速度 ★★★★★

## 5.8 最大宽度与指针整数类型

### intmax_t / uintmax_t

平台上能表示的**最大宽度**有符号/无符号整数类型，保证是必须存在的类型。适用于需要通用算法处理任意整数、格式化输出未知类型整数时使用 `%jd`/`%ju`。

```c
#include <stdint.h>
#include <inttypes.h>

intmax_t big = INTMAX_MAX;
printf("%" PRIdMAX "\n", big);  // 使用 inttypes.h 的格式宏
```


### intptr_t / uintptr_t

能够无损地存储**指针值**（`void*`）的整数类型。在进行内存对齐计算、哈希指针、调试打印地址时使用。这两个类型是**可选的**（标准不强制要求）。

| 类型 | 有符号 | 无符号 | 可用性 | 典型大小 | 用途 |
| --- | --- | --- | --- | --- | --- |
| intmax_t | ✅ | — | 必须 | 64 bit+ | 最大有符号整数 |
| uintmax_t | — | ✅ | 必须 | 64 bit+ | 最大无符号整数 |
| intptr_t | ✅ | — | 可选 | 32/64 bit | 有符号指针整数 |
| uintptr_t | — | ✅ | 可选 | 32/64 bit | 无符号指针整数，内存对齐 |

```c
// 内存对齐检测（uintptr_t 用于指针算术）
void* ptr = get_buffer();
uintptr_t addr = (uintptr_t)ptr;
if (addr % 16 != 0) {
    /* 未按 16 字节对齐 */
}

// 调试打印地址（正确方式）
printf("addr = 0x%" PRIxPTR "\n", (uintptr_t)ptr);
```

## 5.9 size_t 与 ptrdiff_t

定义于 `<stddef.h>`，这两个类型专为内存相关量而设计，其大小由目标处理器的**算术能力**决定（而非内存地址空间大小）。

| 类型 | 符号性 | 来源 | 32-bit 系统 | 64-bit 系统 | 典型用途 |
| --- | --- | --- | --- | --- | --- |
| size_t | 无符号 | stddef.h | 32 bit | 64 bit | sizeof 结果、数组大小、malloc 参数、strlen 返回值 |
| ptrdiff_t | 有符号 | stddef.h | 32 bit | 64 bit | 两指针之差、数组下标（有符号时） |
| ssize_t | 有符号 | POSIX unistd.h | 32 bit | 64 bit | read()/write() 返回值（含错误 −1），非 C 标准 |
| off_t | 有符号 | POSIX sys/types.h | 32/64 bit | 64 bit | 文件偏移量，非 C 标准 |

> 🚨 **size_t 是无符号的——混合运算有陷阱！**
>       `size_t i = 0; i - 1` 结果是 `SIZE_MAX`（下溢回绕），而非 −1。在 `for` 循环中用 `size_t` 做逆向迭代时极易出错。若需要有符号的数组索引，考虑用 `ptrdiff_t`。

```c
// 正确：size_t 用于 sizeof 和数组大小
size_t len = strlen(str);
size_t n   = sizeof(arr) / sizeof(arr[0]);
void*  buf  = malloc(n * sizeof(int));

// 正确：ptrdiff_t 用于指针差值
ptrdiff_t diff = end_ptr - start_ptr;
```

**反面示例**

```c
// 危险：逆向 size_t 循环
for (size_t i = n - 1; i >= 0; i--) { /* 死循环！i 永不 < 0 */ }
```

**正面示例**

```c
// 安全的逆向迭代写法
for (size_t i = n; i-- > 0; ) { arr[i] = 0; }
```

**参考链接**

- [📖 Wikipedia: C data types — size_t / ptrdiff_t](https://en.wikipedia.org/wiki/C_data_types#size_t)

## 5.10 其他重要标准库类型

C 标准库在基本类型之外，还在各头文件中定义了大量用于特定场景的类型。以下是最常用的一批：

| 类型 | 定义于 | C 标准 | 说明 |
| --- | --- | --- | --- |
| time_t | <time.h> | C89 | 表示时间值，通常为自 1970-01-01 00:00:00 UTC 的秒数；具体类型实现定义（通常 int64_t），可能在 2038 问题平台溢出 |
| clock_t | <time.h> | C89 | 表示处理器时钟计数，CLOCKS_PER_SEC 宏给出每秒计数，用于 clock() 函数 |
| wchar_t | <wchar.h> / <stddef.h> | C89 | 宽字符类型，用于多字节字符集（如 Unicode），大小实现定义（Windows 16 bit，Linux 32 bit） |
| wint_t | <wchar.h> | C95 | 可以持有任意宽字符值以及 WEOF 的整数类型，类似 char 与 int 的关系 |
| char8_t | <uchar.h> | C23 | UTF-8 字符类型，8 bit 无符号，用于 u8 字符/字符串字面量 |
| char16_t | <uchar.h> | C11 | UTF-16 字符类型，至少 16 bit，用于 u 字符/字符串字面量 |
| char32_t | <uchar.h> | C11 | UTF-32 字符类型，至少 32 bit，用于 U 字符/字符串字面量 |
| sig_atomic_t | <signal.h> | C89 | 可以在异步信号处理中原子读写的整数类型；在信号处理函数中仅允许对此类型进行读写 |
| fpos_t | <stdio.h> | C89 | 文件位置类型，用于 fgetpos()/fsetpos()，支持大文件偏移 |
| div_t / ldiv_t / lldiv_t | <stdlib.h> | C89/C99 | div()/ldiv()/lldiv() 整除结果结构体（包含商 quot 和余数 rem） |
| mbstate_t | <wchar.h> | C95 | 多字节字符转换状态，用于 mbsrtowcs() 等函数 |
| max_align_t | <stddef.h> | C11 | 对齐要求最严格的标量类型，用于通用内存分配对齐 |
| nullptr_t | <stddef.h> | C23 | nullptr 常量的类型，仅有唯一值 nullptr，可转换为任意对象/函数指针 |
| atomic_int 等 | <stdatomic.h> | C11 | _Atomic 限定整数类型的 typedef，用于无锁并发编程 |

> 📅 **2038 年问题（Y2K38）**
>       在 32-bit 系统上，若 `time_t` 为 `int32_t`（有符号 32 bit），则会在 2038-01-19 03:14:07 UTC 溢出。现代 Linux/glibc（64-bit）已将 `time_t` 定义为 64-bit 整数，但仍有旧嵌入式系统存在此问题。编写可移植代码时，应使用 `int64_t` 存储时间，或使用经过充分测试的时间库。

```c
#include <signal.h>

// 信号处理中只能安全访问 sig_atomic_t
volatile sig_atomic_t g_flag = 0;

void handler(int sig) {
    g_flag = 1;  // 安全：sig_atomic_t 保证原子读写
    // 不要在此使用 printf、malloc 等不可重入函数
}
```

**参考链接**

- [📖 Wikipedia: C data types — Additional types](https://en.wikipedia.org/wiki/C_data_types#Additional_types)
- [📖 Microsoft: Standard Types (C Runtime)](https://learn.microsoft.com/en-us/cpp/c-runtime-library/standard-types)

## 5.11 全面考量因素

选择 C 数据类型不只是"什么类型能存这个值"，还需要综合考虑以下多个维度：

### ⚠️ 溢出与未定义行为

有符号整数溢出是**未定义行为（UB）**；无符号整数溢出是**良定义**的模运算（mod 2ⁿ）。选错类型导致 UB 会被编译器以意想不到的方式优化。

### 🔄 整型提升与隐式转换

所有比 `int` 窄的类型（`char`、`short`、`uint8_t`等）在算术运算前会**提升为 `int` 或 `unsigned int`**，导致意外的符号转换。

### 🌐 平台可移植性

`int`/`long` 的大小在不同平台（LP64/LLP64/ILP32）不同。需要跨平台精确位宽时必须用 `<stdint.h>` 定宽类型。

### ⚡ 性能考量

平台原生整数（`int`）通常最快；`int_fast<em>N</em>_t` 是"至少 N 位中最快的"。小类型在运算时需要截断，有额外开销。

### 🔌 API 兼容性

标准库/系统调用的参数和返回值类型是固定的（如 `read()` 返回 `ssize_t`）。使用错误类型会引发警告或截断错误。

### 🗜️ 内存对齐与结构体填充

结构体中的类型大小影响对齐填充（padding）。使用 `uint8_t` 紧密打包时需注意访问对齐要求；`max_align_t` 表示最大对齐需求。

### 🔢 有符号 vs 无符号混合运算

C 的"通常算术转换"规则规定：有符号与无符号混合时，**有符号被转换为无符号**，导致 `-1 > 0u` 为真等反直觉结果。

### 🔒 并发与原子性

多线程共享数据需要 `_Atomic` 限定（C11 `<stdatomic.h>`）；信号处理仅允许 `sig_atomic_t`；`volatile` 不能代替原子操作。

## 5.12 溢出与未定义行为

> 🚨 **有符号整数溢出是 C 标准的未定义行为**
>       ISO C 标准明确指出，有符号整数算术溢出是 UB（Undefined Behavior）。现代编译器（GCC、Clang）会利用这一点进行激进优化，可能消除你精心写下的溢出检查代码。相关资料：cppreference 隐式转换章节明确指出"虽然有符号整数在算术运算中溢出是未定义行为，但在整数转换中溢出仅是未指定行为（unspecified behavior）"。

**反面示例**

```c
// ❌ 错误的溢出检测（UB！编译器可能优化掉 if 分支）
int a = INT_MAX, b = 1;
if (a + b < a) { /* 溢出 */ }  // GCC -O2 可能认为此条件永远为假
```

**正面示例**

```c
// ✅ 正确的溢出检测方法 1：先转换为更宽类型
if ((int64_t)a + b > INT_MAX) { /* 溢出 */ }
```

**正面示例**

```c
// ✅ 正确的溢出检测方法 2：GCC/Clang 内建函数（C23 也提供）
int result;
if (__builtin_add_overflow(a, b, &result)) { /* 溢出 */ }
```

**正面示例**

```c
// ✅ 无符号溢出是安全的模运算
uint32_t u = UINT32_MAX;
u++;  // u == 0，良定义
```

## 5.13 整型提升（Integer Promotion）规则

C 的**整型提升**规定：任何秩（rank）低于或等于 `int` 的整数类型（`char`、`short`、`bool`、位域等），在用于算术表达式之前，都会被隐式提升为 `int` 或 `unsigned int`。

**⚠️ 经典陷阱：uint8_t 减法结果不是 uint8_t！**

```c
unsigned char a = 0;
unsigned char b = 1;
// a - b 先提升为 int，结果是 int 类型的 -1（不是 255！）
printf("%d\n", a - b);   // 输出: -1

// 若要得到 255（uint8_t 回绕），需要强制转换
printf("%u\n", (unsigned char)(a - b));  // 输出: 255
```

这一规则来自 cppreference 隐式转换章节："整型提升是将任何秩不高于 `int` 的整数类型的值，或 `_Bool`、`int`、`signed int`、`unsigned int` 类型的位域，隐式转换为 `int` 或 `unsigned int` 类型的值。"

**参考链接**

- [📖 cppreference: Implicit conversions — Integer promotions](https://saco-evaluator.org.za/docs/cppreference/en/c/language/conversion.html)

## 5.14 平台可移植性与数据模型

不同平台（操作系统 + 编译器）对基本类型的实际大小遵循不同的数据模型，这是 `long` 大小在 Windows 和 Linux 不同的根本原因。

> 💡 **可移植编程原则**
>       
>         
> - 需要精确位宽 → 使用 `<stdint.h>` 定宽类型
>         
> - 需要表示对象大小 → 使用 `size_t`
>         
> - 需要表示指针差值 → 使用 `ptrdiff_t`
>         
> - 避免假设 `long` 是 64 bit（Windows 上是 32 bit！）
>         
> - 避免假设 `int` 大于 16 bit（嵌入式 C 仍有 16-bit int 平台）
>         
> - 使用 `CHAR_BIT`（来自 `<limits.h>`）而非假设字节是 8 bit

**参考链接**

- [📖 PVS-Studio: About size_t and ptrdiff_t (数据模型对比)](https://pvs-studio.com/en/blog/posts/cpp/a0050/)
- [📖 The Open Group: Why LP64?](http://www.unix.org/version2/whatsnew/lp64_wp.html)

## 5.15 性能考量

```c
// 性能最佳的简单循环计数器
for (int i = 0; i < 1000; i++) { ... }

// 大数组：用 size_t 避免有符号/无符号警告，且与平台匹配
for (size_t i = 0; i < count; i++) { arr[i] = 0; }

// 性能敏感循环：用 fast 类型
for (uint_fast32_t i = 0; i < n; i++) { process(data[i]); }

// 大型查找表（节省内存，提升 cache 效率）
uint_least8_t lut[65536];  // 每项最少 8 bit
```

## 5.16 API 兼容性与系统调用类型

使用标准库或系统调用时，必须使用**函数签名中规定的类型**，否则会引发截断、符号转换错误，或者平台相关的静默错误。

| API / 场景 | 正确类型 | 常见错误类型 | 风险 |
| --- | --- | --- | --- |
| malloc(size) 参数 | size_t | int / unsigned int | 64-bit 平台传入负值或截断大小 |
| strlen() 返回值 | size_t | int | 超长字符串截断，符号比较警告 |
| read() / write() 返回值 | ssize_t（POSIX） | int / size_t | -1 错误码被解读为巨大无符号值 |
| printf/scanf 格式 %d | int | short / char | 参数传递时已被整型提升，通常无问题，但 %hd 对应 short |
| fseek() / ftell() 返回 | long / fpos_t | int | 大文件（>2GB）时 long 在 Windows 上仍只有 32 bit，用 fgetpos 更安全 |
| 原子操作变量 | _Atomic int 等 | 普通 int | 非原子操作导致数据竞争（UB） |
| 信号处理共享变量 | volatile sig_atomic_t | volatile int | int 访问可能非原子，导致读到撕裂值 |

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
```

**正面示例**

```c
// ✅ 正确：遵守 API 规定的类型
size_t n    = strlen(input);      // strlen 返回 size_t
char*  buf   = malloc(n + 1);     // malloc 接受 size_t
size_t wrote = fwrite(buf, 1, n, fp); // fwrite 返回 size_t
```

**反面示例**

```c
// ❌ 错误：用 int 接收 size_t 返回值（64-bit 平台可能截断）
int len = strlen(very_long_string);  // len 可能为负数！
```

## 5.17 完整类型选型流程图

下图整合了所有考量因素，从最常见的需求出发，逐步缩小类型选择范围：

- 需要选择 C 类型
- 浮点数（分数/小数）？
- float / double / long double
- 表示真/假逻辑？
- bool
- 字符/字符串？
- char / wchar_t / char32_t
- 内存大小 / 数组索引 / 字符串长度？
- size_t
- 指针差值 / 有符号偏移？
- ptrdiff_t
- 需要精确位宽（硬件/协议/格式）？
- int8_t … uint64_t
- 性能优先 / 最快运算？
- int / int_fast32_t
- 内存受限 / 最小占用？
- int_least8_t … least64_t
- int（通用默认选择）
- 逐步收窄 →

## 5.18 类型速查总结表

| 类型 / 族 | 头文件 | 符号 | 大小保证 | 必须存在 | 典型场景 |
| --- | --- | --- | --- | --- | --- |
| char | — | 实现定义 | ≥8 bit | ✅ | ASCII 字符、字符串 |
| signed/unsigned char | — | 明确 | ≥8 bit | ✅ | 字节数据、小整数 |
| short | — | 有符号 | ≥16 bit | ✅ | 小范围整数 |
| int | — | 有符号 | ≥16 bit | ✅ | 通用循环计数器、通用整数 |
| long | — | 有符号 | ≥32 bit | ✅ | ⚠ 大小平台相关（LP64 vs LLP64） |
| long long | — | 有符号 | ≥64 bit | ✅ C99 | 大整数（C99+） |
| float / double / long double | — | — | IEEE 754（通常） | ✅ | 浮点运算 |
| bool / _Bool | stdbool.h | 无符号 | 1 bit 值域 | ✅ C99 | 逻辑真/假 |
| int8_t … int64_t | stdint.h | 有符号 | 精确 N bit | ⚠ 可选 | 硬件寄存器、协议、文件格式 |
| uint8_t … uint64_t | stdint.h | 无符号 | 精确 N bit | ⚠ 可选 | 位掩码、字节流、图像数据 |
| int_least8_t … least64_t | stdint.h | 有符号 | ≥N bit | ✅ | 嵌入式节省内存 |
| uint_least8_t … least64_t | stdint.h | 无符号 | ≥N bit | ✅ | 查找表、紧凑数组 |
| int_fast8_t … fast64_t | stdint.h | 有符号 | ≥N bit | ✅ | 性能敏感算法 |
| uint_fast8_t … fast64_t | stdint.h | 无符号 | ≥N bit | ✅ | 快速位操作 |
| intmax_t / uintmax_t | stdint.h | 两者 | 平台最大 | ✅ | 通用最大整数，%jd 格式 |
| intptr_t / uintptr_t | stdint.h | 两者 | ≥ pointer | ⚠ 可选 | 指针转整数、对齐计算 |
| size_t | stddef.h | 无符号 | = pointer | ✅ | sizeof 结果、数组大小、malloc 参数 |
| ptrdiff_t | stddef.h | 有符号 | = pointer | ✅ | 指针差值、有符号数组偏移 |
| wchar_t | wchar.h | 实现定义 | 实现定义 | ✅ | 宽字符（16 bit Win / 32 bit Linux） |
| char16_t / char32_t | uchar.h | 无符号 | ≥16/≥32 bit | ✅ C11 | UTF-16/UTF-32 字符 |
| time_t | time.h | 通常有符号 | 实现定义 | ✅ | 时间值（秒），注意 2038 问题 |
| sig_atomic_t | signal.h | 实现定义 | 实现定义 | ✅ | 信号处理中安全共享的变量 |
| max_align_t | stddef.h | — | 最严格对齐 | ✅ C11 | 通用内存对齐 |
| _Atomic T / atomic_int… | stdatomic.h | 依 T | 依 T | ✅ C11 | 多线程原子操作 |

标签：✅ 必须存在 · ⚠ 可选（平台相关） · C99 引入 · ⚡ 涉及 UB 风险

---

> 📌 **黄金法则**
>       
>         
> - 需要精确位宽 → `<stdint.h>` 定宽类型
>         
> - 需要跨平台保证 → `size_t`、`ptrdiff_t`、`intmax_t`
>         
> - 普通计数器 → `int`（最快、最简洁）
>         
> - 逻辑判断 → `bool`（语义最清晰）
>         
> - 永远不要假设 `int`、`long`、`char` 的具体大小和符号性
>         
> - 有符号溢出是 UB；无符号溢出是合法回绕
>         
> - 混合有符号/无符号运算时显式强制转换

**参考链接**

- [📖 ISO/IEC 9899:1999 TC3（C99 标准正文）](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1256.pdf)
- [📖 cppreference: <stdint.h>](https://en.cppreference.com/w/c/header/stdint)
- [📖 Wikipedia: C data types](https://en.wikipedia.org/wiki/C_data_types)
- [📖 ISO/IEC 9899:2023 draft（C23）](https://open-std.org/JTC1/SC22/WG14/www/docs/n3096.pdf)
- [📖 Microsoft: Standard Types](https://learn.microsoft.com/en-us/cpp/c-runtime-library/standard-types)
- [📖 PVS-Studio: size_t and ptrdiff_t](https://pvs-studio.com/en/blog/posts/cpp/a0050/)
