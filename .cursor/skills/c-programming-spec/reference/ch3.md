# 代码格式与排版

*3基础规范 · 格式*

参考来源：[Linux Kernel Coding Style §1-3](https://www.kernel.org/doc/Documentation/process/coding-style.rst)、
    [GNU Coding Standards §5](https://gnu.net.cn/prep/standards/standards.pdf)

## 3.1 缩进与行宽

| 规则项 | 推荐值 | 说明 |
| --- | --- | --- |
| 缩进单位 | 4 个空格（或 1 Tab） | 嵌入式项目常用 4 空格；Linux 内核用 8 字符 Tab |
| 最大行宽 | 80～100 列 | 超 80 列须换行，特殊情况可到 120 |
| 最大嵌套深度 | ≤ 3 层 | 超过应重构拆函数（Linux Kernel §1 明确要求） |
| 每函数最大行数 | ≤ 50 行 | 不超过一屏（GNU 推荐 24 行屏幕 2 屏以内） |


## 3.2 大括号与函数格式

### [必须] K&R 函数声明与定义

**存储类、返回类型、函数名必须在同一行**；函数体左花括号 `{` 另起一行。头文件声明与源文件定义格式须一致。

| 元素 | 位置 |
| --- | --- |
| `static` / `inline`、返回类型、函数名 | **同一行** |
| 参数列表 | 同一行；过长时仅参数换行缩进 |
| 左花括号 `{` | 函数：另起一行；`if`/`for`/`while`：行尾 |

**反面示例（不规范）**

```c
/* 返回类型与函数名拆行 */
static void
module_do_work(struct ctx *ctx)
{
    /* ... */
}

int
main(void)
{
    return 0;
}
```

**正面示例（规范（K&R 风格））**

```c
/* 控制流：左花括号在行尾 */
if (err != 0) {
    handle_error(err);
    return -1;
}

/* 函数：类型 + 函数名同一行，左花括号另起一行 */
int net_open(int port)
{
    /* 函数体 */
    return 0;
}

/* 参数过长：仅参数换行，函数名仍在首行 */
static int32_t net_socket_send(
    int32_t        sock_fd,
    const uint8_t *data,
    size_t         data_len)
{
    return 0;
}

/* 头文件声明与 .c 定义格式一致 */
bool module_push(struct heap *h,
                 struct node *node,
                 uint64_t delay);
```


## 3.3 空格使用规则

**示例：空格规则示例**

```c
/* 关键字后加空格：if、for、while、switch、return */
if (x > 0) { }
for (int i = 0; i < n; i++) { }

/* 二元运算符两侧加空格 */
result = a + b * c;

/* 一元运算符不加空格 */
i++;
*ptr = 0;
~mask;

/* sizeof 不用加括号（类型除外）—— 但加了更清晰 */
size_t n = sizeof(uint32_t);
size_t m = sizeof(buf);

/* 函数调用不在括号内加空格 */
memcpy(dst, src, len);  /* 正确 */
memcpy ( dst, src, len );  /* 错误 */
```
