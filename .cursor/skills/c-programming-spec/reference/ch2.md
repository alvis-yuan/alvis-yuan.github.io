# 命名规范

*2基础规范 · 命名*

参考来源：[Linux Kernel Coding Style §4 Naming](https://www.kernel.org/doc/Documentation/process/coding-style.rst)、
    [SEI CERT DCL02-C](https://wiki.sei.cmu.edu/confluence/display/c/DCL02-C)

## 2.1 命名风格总则

| 标识符类型 | 命名风格 | 示例 | 来源标准 |
| --- | --- | --- | --- |
| 局部变量 | snake_case（可短） | `count`、`buf_len` | Linux Kernel |
| 全局变量 | snake_case（须描述性） | `g_active_connections` | GNU / CERT |
| 函数 | module_verb_noun() | `net_socket_open()` | Linux / CERT |
| 宏常量 | UPPER_SNAKE_CASE | `MAX_BUF_SIZE` | C99 / Linux |
| 函数宏 | UPPER_SNAKE_CASE 或小写 | `MIN(a,b)`、`container_of` | Linux |
| struct 标签 | snake_case | `struct net_socket` | Linux |
| typedef 类型 | snake_case_t | `net_socket_t` | POSIX / CERT |
| 枚举类型 | snake_case_e | `net_state_e` | 通用 |
| 枚举值 | MODULE_UPPER_SNAKE | `NET_STATE_OPEN` | Linux / CERT |
| static 函数 | snake_case（加 s_ 前缀可选） | `parse_header()` | 通用 |


## 2.2 命名长度与可读性

**反面示例（不规范）**

```c
/* 过短 —— 含义不明 */
int x, y, z;
void p(int a, int b);

/* 过长且冗余 */
int theNumberOfActiveNetworkConnections;

/* 匈牙利命名法（已废弃）*/
int iCount;
char *szName;
bool bIsValid;
```

**正面示例（规范）**

```c
/* 循环变量可以短 */
for (int i = 0; i < n; i++) { }

/* 全局变量须描述性 */
static uint32_t active_conn_count;

/* 函数参数清晰 */
void net_send(int sock_fd,
              const uint8_t *buf,
              size_t buf_len);

/* 布尔语义用 is_/has_ 前缀 */
bool is_connected;
bool has_pending_data;
```


## 2.3 命名空间与前缀规则

### [必须] 公共符号必须带模块前缀以避免命名冲突

C 语言没有命名空间，所有非 `static` 符号共享全局链接空间。每个模块必须使用统一前缀：

      **格式：**`模块名_功能动词_对象名`

```c
/* 模块前缀: uart_ */
int  uart_init(uart_config_t *cfg);
int  uart_send(uart_handle_t *h, const uint8_t *data, size_t len);
void uart_deinit(uart_handle_t *h);

/* 禁止: 无前缀的全局符号 */
int  init();   /* ❌ 极易与其他模块冲突 */
```

> ⚠️ C99 标准规定：以下划线开头的标识符（`_Name` 或 `__name`）由实现保留，**用户代码禁止使用此类命名**。（ISO/IEC 9899:1999 §7.1.3）
