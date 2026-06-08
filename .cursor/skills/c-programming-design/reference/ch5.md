# 数据驱动设计

*Data-Driven Design*

参考来源：
- [🔗 Redis 内部架构](https://redis.io/docs/latest/operate/oss_and_stack/reference/internals/internals-vm/)

### 数据驱动设计

Redis 源码是数据驱动设计的杰出范例：其核心键值存储使用**统一的 dict（哈希表）**存储所有类型的数据，通过 `redisObject` 封装类型信息和编码方式，而非为每种数据类型写一套独立逻辑。GNU 编码标准也强调：**设计者应思考数据结构，而不是过程**（"Show me your tables and I won't need your flowchart"）。

> "The top-level keyspace is a dict (hash table in dict.c). Each value is a redisObject — a tagged union carrying type + encoding + pointer. Core design choices that enabled Redis's long-term success: in-memory model, simple/well-chosen data structures..."
> — — Redis Internal Architecture Documentation

**图6：Redis 数据驱动设计 — 统一 redisObject 通过 encoding 字段多态分发**

- keyspace dict
- "user:1" →
- "session:x" →
- "counter" →
- "rank" →
- "msgs" →
- redisObject
- type | encoding | *ptr
- OBJ_STRING / OBJ_LIST / ...
- embstr / raw sds
- ziplist / listpack
- hashtable
- skiplist + hashtable
- quicklist
- 统一入口
- 多态分发

**✅ 设计优势**
- 数据结构决定程序复杂度，好的数据结构让算法自然简洁
- 统一接口处理多种类型，减少 if-else 类型判断
- 新增数据类型只需实现接口，不改变核心逻辑
- 内存布局可优化，CPU 缓存友好
- 缓存系统、键值存储
- 编译器符号表管理
- 协议解析器（多种报文类型）
- 配置系统（多种值类型）
- 事件系统（多种事件类型）

### 代码示例对比

**反例 — 硬编码类型分支**

```c
/* ❌ 反例：为每种类型写独立逻辑
   新增类型需修改所有 switch */
void print_value(int type, void *v) {
    switch (type) {
    case TYPE_INT:
        printf("%d",
            *(int *)v);
        break;
    case TYPE_STR:
        printf("%s",
            (char *)v);
        break;
    /* 每新增类型都要改这里 */
    }
}
void free_value(int type, void *v) {
    switch (type) { /* 再重复一遍 */
    }
}
```

**正例 — 数据驱动 + 函数指针表**

```c
/* ✅ 正例：类 Redis robj 设计 */
struct val_ops {
    void (*print)(void *ptr);
    void (*release)(void *ptr);
    int  (*compare)(void *a,void *b);
};

struct robj {
    int type;
    const struct val_ops *ops; /* 关键 */
    void *ptr;
};

/* 操作无需 switch，通过表分发 */
void robj_print(struct robj *o) {
    o->ops->print(o->ptr);
}
void robj_free(struct robj *o) {
    o->ops->release(o->ptr);
    free(o);
}
/* 新增类型只需定义新 val_ops，
   不改任何现有代码 */
```