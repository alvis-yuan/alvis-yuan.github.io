# 分层架构与模块化设计

*Layered Architecture & Modularization*

参考来源：
- [🔗 SQLite Architecture](https://sqlite.org/arch.html)

### 分层架构与模块化设计

SQLite 的官方架构文档是 C 语言分层设计的经典范本。SQLite 将整个数据库引擎清晰地划分为：**接口层 → 编译器层（Tokenizer / Parser / CodeGen）→ 虚拟机层 → 存储引擎层（B-Tree / Page Cache）→ OS 抽象层**。每一层仅与相邻层通过定义良好的头文件接口通信。

> "SQLite has a highly layered architecture that can be separated into seven layers... The overall software architecture is highly layered."
> — — SQLite Architecture Document, sqlite.org/arch.html

**图3：SQLite 分层架构图（参考 sqlite.org/arch.html）**

- Interface / C-language API
- main.c · legacy.c · vdbeapi.c · sqlite3_prepare_v2() · sqlite3_step()
- SQL Compiler Layer
- Tokenizer (tokenize.c)
- Parser (parse.y / Lemon)
- Code Generator
- Bytecode Virtual Machine (VDBE)
- vdbe.c · vdbeaux.c · vdbemem.c — 执行 SQL 字节码指令
- B-Tree Engine
- btree.c — 每表/每索引独立B树
- Page Cache (Pager)
- pager.c · wal.c — 事务/缓存
- OS Abstraction Layer (VFS)
- os_unix.c · os_win.c — 统一的文件/锁接口，隐藏平台差异

**✅ 设计优势**
- 各层独立演化，修改 VFS 不影响 SQL 编译器
- 接口契约明确（头文件），协作成本低
- 便于替换实现：如 SQLite 可替换 VFS 后端
- 单元测试可以在任意层独立进行
- 新增功能只需在正确层次扩展
- 数据库引擎、编译器等复杂系统
- 需要多平台移植的基础库
- 网络协议栈（TCP/IP 分层模型）
- 设备驱动框架
- 中间件及 SDK 开发

### 代码示例对比

**反例 — 层次耦合**

```c
/* ❌ 反例：上层直接操作磁盘细节 */
int query_users(const char *sql) {
    /* SQL 解析层直接调用底层
       系统调用，绕过抽象层 */
    int fd = open("/data/users.db",
        O_RDWR);
    char buf[4096];
    pread(fd, buf, 4096, page_offset);
    /* 直接解析 B-Tree 页面结构 */
    struct btree_page *p =
        (struct btree_page *)buf;
    /* 层次混乱，无法跨平台 */
}
```

**正例 — 通过抽象层通信**

```c
/* ✅ 正例：通过 VFS 接口访问文件
   参考 SQLite os.h 设计 */

/* OS 抽象接口定义（类 SQLite VFS）*/
struct vfs_ops {
    int (*open)(const char *path,
               int flags, void **fd);
    int (*read)(void *fd, void *buf,
               int n, off_t off);
    void (*close)(void *fd);
};

/* 上层只依赖接口，不关心实现 */
int pager_read_page(
    struct pager *pg, int pgno,
    void *buf) {
    off_t offset = (off_t)pgno
        * pg->page_size;
    return pg->vfs->read(
        pg->fd, buf,
        pg->page_size, offset);
}
```