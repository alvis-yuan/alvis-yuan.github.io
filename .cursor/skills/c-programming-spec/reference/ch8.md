# 函数设计规范

*8函数与接口*

参考来源：[Linux Kernel §6 Functions、§16 Return Values](https://www.kernel.org/doc/Documentation/process/coding-style.rst)、
    [SEI CERT API00-C](https://wiki.sei.cmu.edu/confluence/display/c/API00-C)

## 8.1 函数单一职责与规模

### [必须] 函数只做一件事，长度不超过 50 行

**Linux Kernel 规定：**函数应短小，可在 80×24 屏幕的 1-2 屏内完整显示。
      最大行数与复杂度成反比：简单的 switch 可以更长，嵌套复杂的函数应更短。

      本地变量不超过 5-10 个；超出时应重构拆分子函数。


## 8.2 函数声明规范

### [必须] 存储类、返回类型、函数名同一行（K&R 风格）

函数声明与定义须遵循 [第 3.2 章](ch3.md#32-大括号与函数格式)：`static`/`inline`、返回类型、函数名写在**同一行**；`{` 另起一行。参数过多时仅参数列表换行。

**反面示例（不规范）**

```c
void
timer_heap_init(struct timer_heap *h)
{
    /* ... */
}
```

**示例：完整函数声明示例（按 Linux 内核顺序）**

```c
/**
 * @brief   发送数据到指定套接字
 * @param   sock_fd   套接字文件描述符（有效值 ≥ 0）
 * @param   data      待发送数据缓冲区（不可为 NULL）
 * @param   data_len  发送字节数（有效值 > 0）
 * @return  实际发送字节数（≥0），或错误码（<0）
 * @retval  -EINVAL   参数无效
 * @retval  -ENOTCONN 套接字未连接
 */
/* 顺序: 存储类 → 返回类型 → 函数名 → 参数 → 属性（均在首行起始） */
static int32_t net_socket_send(
    int32_t        sock_fd,
    const uint8_t *data,
    size_t         data_len
);
```


## 8.3 返回值约定

**图8.1 — 三种函数返回值语义约定（来源：Linux Kernel §16）**

- 函数返回值语义约定
- 命令型函数
- 动词命名: do_xxx()
- 0 = 成功
- -Exxx = 错误码
- 示例: write(), send()
- 谓词型函数
- is_/has_ 命名
- true/非0 = 是
- false/0 = 否
- 示例: is_connected()
- 计算型函数
- get_/calc_ 命名
- 有效值
- NULL/越界值=失败
- 示例: malloc(), find()

命令型与计算型函数的**失败返回路径**必须在返回前打印错误日志，见 [第 10.4 章](ch10.md#104-异常返回点日志)（**必须**）。


## 8.4 函数修饰符与属性

| 修饰符/属性 | 用途 | 示例 |
| --- | --- | --- |
| `static` | 限制为文件内部可见，不暴露给链接器 | `static int helper(void);` |
| `inline` | 建议内联（<3行，性能关键路径） | `static inline int max(int a, int b);` |
| `__attribute__((noreturn))` | 标注不会返回的函数（GCC/Clang） | `void fatal_error(void) __attribute__((noreturn));` |
| `__attribute__((deprecated))` | 标注废弃接口 | 触发编译器警告 |
| `__attribute__((warn_unused_result))` | 返回值必须被使用 | 防止忽略错误码 |
| `__attribute__((nonnull(1,2)))` | 标注不可为 NULL 的参数 | 编译器可优化并警告 |


## 8.5 指针形参修饰符

参考来源：ISO/IEC 9899:1999 §6.7.3（类型限定符）、§6.7.5（声明符）、
    [SEI CERT DCL00-C](https://wiki.sei.cmu.edu/confluence/display/c/DCL00-C)、
    C99 标准库 `memcpy`/`memmove`/`strcpy` 等接口惯例

### [必须] 指针形参必须按语义添加 `const` / `restrict`

所有指针形参须在声明中通过类型限定符明确**读写意图**与**别名假设**。
缺失或误用修饰符会导致：调用方数据被隐式修改、接口契约不清、以及因别名未知而无法安全优化的性能损失。

**修饰符书写顺序（C99）：**`存储类` → `类型限定符（const/restrict/volatile）` → `基础类型` → `*` → `形参名`。
同一指针上 `const` 与 `restrict` 可组合，常见形式为 `const T *restrict p` 或 `T *restrict p`。

#### `const` 选用规则

| 语义 | 形参写法 | 说明 |
| --- | --- | --- |
| 只读输入缓冲区 / 配置 / 字符串 | `const T *` | 函数内不得修改 `*p` |
| 输出或读写缓冲区 | `T *` | 函数会写入 `*p`；**禁止**对指向数据加 `const` |
| 只读对象句柄（仅读字段） | `const obj_t *` | 与上相同；若需改对象状态则用 `obj_t *` |
| 二级指针：填充输出指针 | `T **` | 函数写入 `*out` |
| 二级指针：只读指针数组 | `const T * const *` 或 `const T **` | 不修改各元素指向的数据 |

#### `restrict` 选用规则

`restrict` 承诺：在本函数体内，对该指针所指对象的访问**仅通过该指针（及其派生指针）进行**。
编译器据此可做别名无关优化；承诺不成立时行为未定义。

| 场景 | 是否加 `restrict` |
| --- | --- |
| 独占顺序读/写（`memcpy`、累加遍历、帧解析等），且同次调用中其他形参不会指向重叠区域 | **必须**加 `restrict` |
| 存在别名可能（同一缓冲区以多指针传入、`memmove` 式重叠拷贝、来源未知的外部指针） | **禁止**加 `restrict` |
| 仅解引用一次或简单标量输出（`int *out` 写单个值） | 可选；无性能收益时可省略 |

#### 决策流程（编写/审查函数声明时逐步核对）

1. 函数是否会修改指针**所指向的对象**？否 → `const T *`；是 → `T *`。
2. 是否对缓冲区做批量访问，且接口契约保证与同函数其他缓冲区形参无重叠？是 → 在对应指针上加 `restrict`。
3. 二级指针用于输出？使用 `T **` 且**不**对输出槽位加 `const`。
4. 头文件声明与源文件定义的类型限定符**必须完全一致**（含 `const`/`restrict` 顺序）。

**反面示例（不规范）**

```c
/* ❌ 只读输入却未加 const — 调用方无法确认数据不会被修改 */
int32_t pkt_parse(uint8_t *raw, size_t len, pkt_t *out);

/* ❌ 输出缓冲区误加 const — 无法写入 */
int32_t buf_fill(const uint8_t *dst, size_t cap);

/* ❌ 独占拷贝源/目的可证明不重叠，却未加 restrict — 丧失优化且契约不清 */
void frame_copy(uint8_t *dst, const uint8_t *src, size_t n);

/* ❌ 声明与定义限定符不一致 — 违反 C99 同一性要求 */
/* .h */ int hash(const uint8_t *data, size_t len);
/* .c */ int hash(uint8_t *restrict data, size_t len);
```

**正面示例（规范）**

```c
/* 只读解析 + 输出结构体 */
int32_t pkt_parse(const uint8_t *raw, size_t len, pkt_t *out);

/* 独占拷贝：目的可写、源只读、双方 restrict */
void frame_copy(uint8_t *restrict dst,
                const uint8_t *restrict src,
                size_t n);

/* 重叠区域移动：禁止 restrict */
void frame_move(uint8_t *dst, const uint8_t *src, size_t n);

/* 字符串与配置：只读 */
int32_t cfg_load(const char *path, cfg_t *out);

/* 数值输出：单点写入，无需 restrict */
int32_t sensor_read(sensor_t *dev, int32_t *out_value);

/* 头文件与 .c 定义一致 */
int32_t hash(const uint8_t *restrict data, size_t len);
```
