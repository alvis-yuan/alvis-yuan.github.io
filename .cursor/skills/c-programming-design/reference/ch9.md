# 内存所有权语义

*Memory Ownership Semantics*

参考来源：
- [🔗 Linux Kernel Docs §14 Allocating Memory](https://www.kernel.org/doc/html/latest/process/coding-style.html#allocating-memory)

### 内存所有权语义

Linux 内核第14章阐述了内存分配的设计原则。C 语言没有自动内存管理，因此**明确的所有权语义**是架构设计中最重要的约定之一。谁分配谁释放、传入参数的生命周期约定、返回堆对象的所有权转移——这些规则必须在接口设计时明确，并通过命名和注释传达给调用方。

> "The preferred form for passing a size of a struct is: p = kmalloc(sizeof(*p), ...); The alternative form where struct name is spelled out hurts readability and introduces an opportunity for a bug when the pointer variable type is changed."
> — — Linux Kernel Coding Style, §14 Allocating Memory

**图9：C 语言内存所有权的三种设计模式及其约定**

- C 语言内存所有权三种模式
- 借用 (Borrow)
- void fn(const obj *p)
- 调用方拥有对象
- 函数不释放
- ✓ 最安全，最常见
- const 修饰表只读借用
- 适合: read_data(buf)
- 转入 (Transfer In)
- void consume(obj *p)
- 函数接管对象
- 函数负责释放
- ⚠ 必须注释说明
- 调用后不可再用 p
- 适合: list_add(node)
- 转出 (Transfer Out)
- obj* create(void)
- 返回新分配对象
- 调用方负责释放
- ✓ 需文档明确说明
- 配对 destroy() 函数
- 适合: ht_create()

**✅ 设计优势**
- 明确所有权规则从根本上消除内存泄漏
- 通过 const 修饰强制编译器检查只读借用
- 统一的 create/destroy 配对模式降低误用
- 接口文档化所有权转移，减少误解
- 便于内存检测工具（Valgrind）精准定位问题
- 所有 C 语言公开 API 设计（必选）
- 数据容器（链表、树、哈希表）
- 资源管理器（连接池、内存池）
- 跨线程传递对象时的所有权移交
- 回调函数中的对象生命周期管理

### 代码示例对比

**反例 — 所有权语义不清晰**

```c
/* ❌ 反例：所有权不明确，
   调用方不知道是否应该 free */

/* 返回值是静态的还是堆的？
   调用方应该 free 吗？ */
char* get_name(struct user *u) {
    return u->name; /* 借用？ */
}

/* 或者这个版本：
   返回了 malloc 的指针，
   但没有注释说明 */
char* get_name_copy(struct user *u) {
    return strdup(u->name); /* 转出？ */
}
/* 调用方：free() 还是不 free()？
   猜！ */
```

**正例 — 清晰的所有权语义**

```c
/* ✅ 正例：内核风格，所有权语义清晰 */

/**
 * user_get_name - 借用用户名
 * @u: 用户对象（调用方拥有）
 * 返回: 指向内部缓冲区的指针，
 *       调用方不得释放，生命周期
 *       与 @u 相同。
 */
const char* user_get_name(
    const struct user *u);

/**
 * user_dup_name - 返回用户名副本
 * 返回: 堆分配字符串，调用方
 *       负责用 kfree() 释放。
 */
char* user_dup_name(
    const struct user *u);

/* 分配使用 sizeof(*p) 而非类型名 */
struct user *u =
    kmalloc(sizeof(*u), GFP_KERNEL);
```