# 单一职责与函数短小原则

*Single Responsibility & Compact Functions*

参考来源：
- [🔗 Linux Kernel Docs §6 Functions](https://www.kernel.org/doc/html/latest/process/coding-style.html#functions)

### 单一职责与函数短小原则

Linux 内核编码风格第6章明确指出：**函数应当短小、只做一件事、做好一件事**。函数长度应反比于其复杂度与缩进层次。这是整个 C 设计哲学中最核心的思想之一。

> "Functions should be short and sweet, and do just one thing. They should fit on one or two screenfuls of text (the ISO/ANSI screen size is 80×24, as we all know), and do one thing and do that well."
> — — Linux Kernel Coding Style, §6 Functions

内核还规定：**函数局部变量不应超过 5~10 个**，超出则说明函数职责不够单一，需要重构。人脑同时追踪的概念上限约为 7 个，这是认知科学给出的约束。

**✅ 设计优势**
- 单一职责让函数可独立测试、复用
- 减少认知负担，加快 code review 速度
- 缩短函数降低函数圈复杂度
- 修改局部影响最小，副作用可预测
- 便于静态分析工具做精准检查
- 操作系统内核驱动程序开发
- 嵌入式系统资源受限环境
- 需要高可靠性的工业控制软件
- 大型团队多人协作代码库
- 需要形式化验证的安全关键系统

**图2：大函数拆分为单一职责小函数的架构演进**

- ❌ 大函数 (反例)
- parse_data()
- • 解析输入
- • 校验格式
- • 转换数据
- • 写入数据库
- • 发送通知
- • 记录日志
- 重构
- parse_input()
- validate_format()
- transform_data()
- persist_record()
- ✅ 协调函数
- process_data()
- 编排调用各子函数

### 代码示例对比

**反例 — 混合职责的大函数**

```c
/* ❌ 反例：一个函数做多件事 */
int handle_user_request(char *input) {
    /* 解析、校验、存储、通知
       全混在一起，难以测试 */
    char buf[256];
    int user_id, status, db_ret;
    FILE *log_fp;
    char *token, *name, *email;
    /* ...局部变量超过10个... */

    /* 解析 */
    token = strtok(input, ",");
    /* 校验 */
    if (!token) return -1;
    /* 数据库写入 */
    db_ret = db_insert(token);
    /* 发邮件 */
    send_email(token);
    /* 写日志 */
    log_fp = fopen("app.log", "a");
    /* 200行混合逻辑... */
}
```

**正例 — 单一职责分解**

```c
/* ✅ 正例：每函数只做一件事 */

/* 只负责解析输入 */
static int parse_request(const char *in,
    struct request *req) {
    if (!in || !req) return -EINVAL;
    return tokenize(in, req);
}

/* 只负责校验 */
static int validate_request(
    const struct request *req) {
    return req->user_id > 0
        && req->email[0] != '\0';
}

/* 协调函数：组合调用 */
int handle_request(const char *input) {
    struct request req;
    int ret;

    ret = parse_request(input, &req);
    if (ret) return ret;

    if (!validate_request(&req))
        return -EINVAL;

    return persist_request(&req);
}
```