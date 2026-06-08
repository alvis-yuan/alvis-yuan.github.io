# 文件组织与头部注释

*1基础规范 · 文件组织*

参考来源：[Linux Kernel Coding Style](https://www.kernel.org/doc/Documentation/process/coding-style.rst)、
    [GNU Coding Standards](https://gnu.net.cn/prep/standards/standards.pdf)、
    [SEI CERT C Coding Standard](https://wiki.sei.cmu.edu/confluence/display/c/SEI+CERT+C+Coding+Standard)

## 1.1 文件命名规则

### [必须] 文件名使用小写字母与下划线，避免平台歧义

**规则：**源文件 `.c` 与头文件 `.h` 命名仅使用小写字母、数字、下划线，不使用连字符或空格。文件名应反映其主要内容，不超过 32 个字符。

| 文件类型 | 命名示例 | 说明 |
| --- | --- | --- |
| 源文件 | `network_utils.c` | 模块 + 功能描述 |
| 头文件 | `network_utils.h` | 与源文件同名 |
| 接口头文件 | `uart_driver.h` | 仅暴露公共 API |
| 私有头文件 | `uart_driver_priv.h` | _priv 后缀标注内部使用 |


## 1.2 文件头部注释格式

每个 `.c` 和 `.h` 文件都必须在顶部包含标准注释块，涵盖版权、功能描述、修订历史等信息。

**示例：标准文件头模板（.c 文件）**

```c
/**
 * @file        network_utils.c
 * @brief       网络工具函数库 —— TCP/UDP 封装与错误处理
 * @details     提供跨平台的套接字操作接口，兼容 POSIX.1-2008 标准。
 *              本模块不依赖任何动态内存分配。
 *
 * @author      张三 <zhangsan@example.com>
 * @date        2024-01-15
 * @version     1.2.3
 *
 * @copyright   Copyright (c) 2024 Example Corp. All rights reserved.
 * @license     MIT License — 详见根目录 LICENSE 文件
 *
 * @par 修订历史:
 * | 版本  | 日期       | 作者  | 说明              |
 * |-------|------------|-------|-------------------|
 * | 1.0.0 | 2023-06-01 | 张三  | 初始版本          |
 * | 1.1.0 | 2023-09-10 | 李四  | 增加 UDP 广播支持  |
 * | 1.2.3 | 2024-01-15 | 张三  | 修复 IPv6 对齐问题 |
 *
 * @note    编译要求: gcc -std=c99 -Wall -Wextra -pedantic
 * @warning 本模块在裸机环境下不可用，需要 OS 网络栈支持
 */
```

**示例：头文件头部（.h 文件）**

```c
/**
 * @file    network_utils.h
 * @brief   网络工具函数库公共接口声明
 * @ingroup network
 *
 * @par 使用示例:
 * @code
 *   net_socket_t sock;
 *   if (net_connect(&sock, "192.168.1.1", 8080) != NET_OK) {
 *       log_error("连接失败");
 *   }
 * @endcode
 */

#ifndef NETWORK_UTILS_H
#define NETWORK_UTILS_H

/* ... 头文件内容 ... */

#endif /* NETWORK_UTILS_H */
```


## 1.3 文件内部组织顺序

**图1.1 — C源文件与头文件内部组织结构**

- .c 源文件组织
- ① 文件头部注释块
- 版权 / 功能 / 作者 / 修订历史
- ② #include 系统头文件
- ③ #include 本项目头文件
- "network_utils.h" 等
- ④ 宏定义 / 常量
- 仅本文件使用的 #define
- ⑤ 类型定义（本文件私有）
- struct / enum / typedef
- .h 头文件组织
- ① 文件头部注释
- ② Include 守卫
- #ifndef / #define / #endif
- ③ 所需 #include
- 仅依赖项，保持最小化
- ④ 公共宏 / 常量
- ⑤ 类型 / 结构体声明
- 函数原型 → 最后放置


## 1.4 C与H文件的职责划分

### [必须] 头文件只声明，源文件才定义

头文件（`.h`）中：**只放声明**（函数原型、类型定义、常量宏、extern 变量声明）。

      源文件（`.c`）中：**放实现**（函数体、变量定义、静态函数）。

      **禁止**在头文件中定义非 `inline` 函数或定义（非 extern）全局变量。
