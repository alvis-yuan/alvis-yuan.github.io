# 防御性编程与断言设计

*Defensive Programming & Assertion Design*

参考来源：
- [🔗 SEI CERT C Standard](https://wiki.sei.cmu.edu/confluence/spaces/c/pages/87152064/Introduction)

### 防御性编程与断言设计

SEI CERT C 编码标准（卡内基梅隆大学软件工程研究所）的核心目标：**通过消除未定义行为来开发安全、可靠的系统**。其防御性思想体现在：永远不信任外部输入、校验所有参数、正确处理所有错误码。NASA JPL Rule 5 则要求：**每个函数平均至少有2处断言，断言不得有副作用**。

> "The goal of these rules and recommendations is to develop safe, reliable, and secure systems, for example by eliminating undefined behaviors that can lead to undefined program behaviors and exploitable vulnerabilities."
> — — SEI CERT C Coding Standard, Introduction

> "Use a minimum of two assertions per function. Assertions must be side-effect free and are best defined as Boolean tests. When an assertion fails, an explicit recovery action should be taken."
> — — NASA JPL Power of Ten, Rule 5

**✅ 设计优势**
- 在函数边界检查，错误在源头暴露
- 断言文档化了函数的前置/后置条件
- 减少"垃圾进垃圾出"问题
- 防止缓冲区溢出等安全漏洞
- 生产环境可禁用断言，开发期充分检测
- 对外 API 的参数校验（不可省略）
- 内部函数的前置条件检查（断言）
- 解析外部用户输入的边界值
- 整数运算的溢出检测
- 指针使用前的有效性验证

### 代码示例对比

**反例 — 信任输入，不做校验**

```c
/* ❌ 反例：不校验参数，
   盲目信任调用方 */
char* get_username(
    char **users, int idx) {
    /* idx 可能是负数或越界 */
    return users[idx]; /* 未定义行为! */
}

void copy_str(char *dst,
    const char *src) {
    /* dst 可能为 NULL，
       src 可能超长，缓冲区溢出 */
    strcpy(dst, src); /* 危险! */
}

int divide(int a, int b) {
    return a / b; /* b==0 未检查 */
}
```

**正例 — 防御性校验 + 断言**

```c
/* ✅ 正例：SEI CERT + JPL 风格 */

#define ASSERT(e) \
    ((e) ? (void)0 : \
     (log_assert(__FILE__,__LINE__,#e),\
      abort()))

char* get_username(
    char **users, int n, int idx) {
    ASSERT(users != NULL); /* 前置条件 */
    ASSERT(n > 0);
    if (idx < 0 || idx >= n)
        return NULL; /* 防御 */
    return users[idx];
}

int safe_copy(char *dst, size_t dsz,
    const char *src) {
    ASSERT(dst != NULL);
    ASSERT(src != NULL);
    ASSERT(dsz > 0);
    return snprintf(dst, dsz,
        "%s", src); /* 安全版本 */
}
```