# 抽象接口与信息隐藏

*Abstract Interface & Information Hiding*

参考来源：
- [🔗 Linux Kernel Docs §5 Typedefs](https://www.kernel.org/doc/html/latest/process/coding-style.html#typedefs)

### 抽象接口与信息隐藏

Linux 内核第5章专门讨论 **typedef 与信息隐藏** 的权衡：只有当类型信息对调用者真正不透明时才使用 typedef（如 pte_t）。内核 VFS（虚拟文件系统）是信息隐藏与抽象接口最完美的体现：`struct file_operations` 定义了一组函数指针，每种文件系统（ext4、btrfs、proc）分别实现，上层无需感知。

> "When you see a 'vps_t a;' in the source, what does it mean? In contrast, if it says 'struct virtual_container *a;' you can actually tell what 'a' is."
> — — Linux Kernel Coding Style, §5 Typedefs

**图4：Linux VFS 通过函数指针表实现接口抽象（信息隐藏）**

- VFS 调用方
- vfs_read(file,...)
- vfs_write(file,...)
- struct file_operations
- → .read = ?
- → .write = ?
- → .open = ?
- ext4_fops
- btrfs_fops
- proc_fops
- 调用方只依赖 struct file_operations 接口，不感知具体文件系统实现
- 新增文件系统类型只需提供对应 fops 实现，调用方代码零修改

**✅ 设计优势**
- 调用方与实现方解耦，降低变更风险
- 支持多态行为（函数指针表）
- 可在不破坏接口的前提下替换实现
- 头文件即文档，清晰界定模块边界
- 内部状态不外露，防止误操作
- 操作系统内核驱动框架
- 插件化系统、动态加载模块
- 需要支持多种后端的基础库
- 硬件抽象层（HAL）设计
- 跨平台 SDK 接口设计

### 代码示例对比

**反例 — 暴露内部实现**

```c
/* ❌ 反例：外部可访问内部字段 */
/* hash_table.h */
struct hash_table {
    struct entry **buckets; /* 暴露 */
    int bucket_count;       /* 暴露 */
    int size;
    float load_factor;     /* 暴露 */
};

/* 调用方直接访问内部结构
   一旦 buckets 改为树，
   所有调用方都要修改 */
for (int i = 0;
     i < ht->bucket_count; i++) {
    entry = ht->buckets[i];
}
```

**正例 — 不透明指针 + 访问器**

```c
/* ✅ 正例：类 SQLite 不透明类型设计 */
/* hash_table.h — 公开头文件 */
typedef struct hash_table
    hash_table_t; /* 不透明指针 */

hash_table_t *ht_create(int cap);
void  ht_destroy(hash_table_t *ht);
int   ht_put(hash_table_t *ht,
    const char *k, void *v);
void* ht_get(hash_table_t *ht,
    const char *k);
int   ht_size(hash_table_t *ht);

/* hash_table.c — 实现细节完全隐藏
   内部可随意更改而不影响调用方 */
struct hash_table { /* 仅 .c 中可见 */
    struct entry **buckets;
    int n_buckets, n_entries;
};
```