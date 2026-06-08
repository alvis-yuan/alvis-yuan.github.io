# 结构体与联合体

*6类型与变量*

参考来源：[The Lost Art of Structure Packing (Eric Raymond)](http://www.catb.org/esr/structure-packing/)、
    [Memfault — C Struct Padding](https://interrupt.memfault.com/blog/c-struct-padding-initialization)、
    ISO/IEC 9899:1999 §6.7.2.1

## 6.1 结构体命名与 typedef

**反面示例（不规范）**

```c
/* 无 typedef：每次都需要写 struct */
struct NetworkSocket {  /* 驼峰命名 */
    int FD;             /* 成员大写 */
    uint8_t State;
    char ip_buffer[16]; /* 小数组在大对齐后 */
    uint32_t port;
};
```

**正面示例（规范）**

```c
/**
 * @brief 网络套接字描述符
 * 成员按对齐降序排列以减少填充
 */
typedef struct net_socket {
    uint32_t port;        /* 4B: 大对齐在前 */
    int32_t  fd;           /* 4B */
    uint8_t  state;        /* 1B */
    uint8_t  ip_buf[16];   /* 小数组在小成员后 */
    uint8_t  _pad[3];      /* 显式填充，文档化 */
} net_socket_t;
```


## 6.2 内存对齐与 Cache 优化

> 💡 **自然对齐规则（C99 §6.7.2.1）：**编译器会在结构体成员之间插入填充字节，使每个成员位于其自身大小整数倍的地址。
>       正确排列成员顺序可大幅减少内存浪费，并提升 CPU Cache 命中率。

**图6.1 — 成员从大到小排列可消除多余填充（节省内存 33%）**

- 结构体内存布局对比
- ❌ 低效布局 (12字节)
- byte: 0
- char a
- pad×3
- int32 b
- char c
- struct bad { char a; int32_t b; char c; }; // sizeof = 12
- ✅ 优化布局 (8字节)
- pad×2
- struct good { int32_t b; char a; char c; }; // sizeof = 8
- 数据成员
- 填充字节(浪费)
- 32位整数


## 6.3 结构体编程规范汇总

### [必须] 成员排列：按对齐尺寸从大到小

基本规则：**64位 → 32位 → 16位 → 8位 → 位域**，可最大化减少编译器插入填充。

### [必须] 需要显式填充时，使用命名填充成员

用命名的 `_pad[]` 成员替代隐式填充，便于审计和移植。

```c
typedef struct {
    uint32_t id;
    uint8_t  type;
    uint8_t  _pad[3];  /* 显式填充到 4B 对齐 */
    uint32_t value;
} msg_header_t;

/* 编译时验证大小 */
_Static_assert(sizeof(msg_header_t) == 12, "msg_header_t size mismatch");
```

### [建议] 位域（bitfield）使用规范

位域应使用 `unsigned int` 或 `uint32_t` 类型，不使用 `int`（符号位语义不定）。
      位域不可跨越存储单元边界（C99 §6.7.2.1/10）。不应对位域使用 `sizeof` 或取地址操作。

```c
typedef struct {
    uint32_t mode   : 4;   /* 4 bits: 0-15 */
    uint32_t active : 1;   /* 1 bit */
    uint32_t error  : 1;   /* 1 bit */
    uint32_t _res   : 26;  /* 保留位 */
} ctrl_reg_t;
```
