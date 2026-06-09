/**
 * user_list.h — Linux 内核双向链表的应用层移植版本
 *
 * 原始来源（必须遵守 GPL-2.0 许可证）：
 *   https://github.com/torvalds/linux/blob/master/include/linux/list.h
 *   https://docs.kernel.org/core-api/list.html
 *
 * 本文件从内核 include/linux/list.h 提取核心思想，
 * 移除内核专用依赖，适配 POSIX 用户空间 C 程序（C99/C11+GCC扩展）。
 *
 * 主要改动：
 *   1. 添加 offsetof / container_of（原位于 include/linux/stddef.h 等）
 *   2. 移除 WRITE_ONCE、prefetch()、RCU 相关函数
 *   3. 毒化指针改为用户空间安全的调试值
 *   4. 所有注释改为中文，便于阅读
 *
 * 使用限制：
 *   - 非线程安全，多线程访问需自行加锁
 *   - 需要 GCC 或 Clang 编译器（依赖 typeof() 扩展）
 *
 * SPDX-License-Identifier: GPL-2.0
 */

#ifndef _USER_LIST_H
#define _USER_LIST_H

#include <stddef.h>  /* size_t */

/* ================================================================
 * §1  基础宏：内存偏移与宿主结构体还原
 * ================================================================ */

#ifndef offsetof
/**
 * offsetof(TYPE, MEMBER)
 * 计算结构体成员的字节偏移量（C 标准已提供，此处作为备用）。
 * 原理：将地址 0 当作 TYPE 的起始地址，&(0)->MEMBER 的值即为偏移。
 */
#define offsetof(TYPE, MEMBER) \
    ((size_t)((char *) &((TYPE *)0)->MEMBER))
#endif

/**
 * container_of(ptr, type, member)
 * 通过结构体成员指针反推宿主结构体的起始指针。
 *
 * @ptr:    指向 struct list_head（或其他成员）的指针
 * @type:   宿主结构体类型（如 struct my_node）
 * @member: ptr 对应成员在 type 中的字段名（如 list）
 *
 * 原理：宿主地址 = 成员地址 - 成员偏移量
 *
 * typeof(*ptr) 编译期类型检查：若 ptr 类型与 member 类型不符，
 * 编译器会产生警告，有助于发现错误。
 *
 * 来源：内核 include/linux/container_of.h
 */
#define container_of(ptr, type, member) ({          \
    const typeof(((type *)0)->member) *__mptr = (ptr); \
    (type *)((char *)__mptr - offsetof(type, member)); \
})

/* ================================================================
 * §2  链表节点结构体
 * ================================================================ */

/**
 * struct list_head - 双向链表节点/头节点
 *
 * 这是内核链表设计的核心：不包含任何数据，只有两个指针。
 * 将此结构体嵌入到任意用户数据结构中，即可将该数据结构加入链表。
 *
 * 空链表（或独立节点）的状态：next == prev == &self（自环）
 *
 * 来源：include/linux/types.h (struct list_head)
 */
struct list_head {
    struct list_head *next; /* 指向下一个节点；空链表时指向自身 */
    struct list_head *prev; /* 指向上一个节点；空链表时指向自身 */
};

/* ================================================================
 * §3  初始化
 * ================================================================ */

/**
 * LIST_HEAD_INIT(name) — 静态初始化器
 * 用于在声明时直接初始化 struct list_head（全局/静态变量）。
 * 使 next 和 prev 均指向 name 自身，形成空的自环链表。
 *
 * 来源：include/linux/list.h #define LIST_HEAD_INIT
 * 原文：initialize a &struct list_head's links to point to itself
 *
 * 用法：
 *   static struct list_head my_list = LIST_HEAD_INIT(my_list);
 */
#define LIST_HEAD_INIT(name) { &(name), &(name) }

/**
 * LIST_HEAD(name) — 声明并初始化链表头（一步完成）
 * 等价于：struct list_head name = LIST_HEAD_INIT(name)
 *
 * 来源：include/linux/list.h #define LIST_HEAD
 * 原文：definition of a &struct list_head with initialization values
 *
 * 用法：
 *   LIST_HEAD(my_list);  // 等价于 struct list_head my_list = {...}
 */
#define LIST_HEAD(name) \
    struct list_head name = LIST_HEAD_INIT(name)

/**
 * INIT_LIST_HEAD(list) — 运行时初始化
 * 将 list 指向的节点初始化为空链表头（next = prev = self）。
 * 适用于动态分配的链表头或结构体成员在运行时初始化。
 *
 * 来源：include/linux/list.h - INIT_LIST_HEAD()
 * 原文：Initializes the list_head to point to itself.
 *       If it is a list header, the result is an empty list.
 *
 * @list: 指向 struct list_head 的指针
 */
static inline void INIT_LIST_HEAD(struct list_head *list)
{
    list->next = list; /* next 指向自身 */
    list->prev = list; /* prev 指向自身 */
}

/* ================================================================
 * §4  毒化指针（调试辅助）
 *
 * 被 list_del 删除的节点，其 next/prev 设为这两个非法地址。
 * 应用层访问这些地址会立即触发段错误（SIGSEGV），便于调试。
 * 如不需要此功能，可将其定义为 NULL。
 *
 * 来源：内核 include/linux/poison.h (LIST_POISON1/2)
 * ================================================================ */
#define LIST_POISON1  ((struct list_head *) 0xDEAD0001)
#define LIST_POISON2  ((struct list_head *) 0xDEAD0002)

/* ================================================================
 * §5  内部低层操作（双下划线前缀 = 内部使用，不直接调用）
 * ================================================================ */

/*
 * __list_add(new, prev, next)
 * 在已知相邻的 prev 和 next 节点之间插入 new 节点。
 * 是所有插入操作的底层实现，顺序依次修改4个指针。
 *
 * 来源：include/linux/list.h - __list_add()
 * 原文注释：
 *   This is only for internal list manipulation where we know
 *   the prev/next entries already!
 *
 * 步骤（每步操作均以注释标注）：
 *   ① next->prev = new   先让后继节点的 prev 指向新节点
 *   ② new->next  = next  新节点的 next 指向后继
 *   ③ new->prev  = prev  新节点的 prev 指向前驱
 *   ④ prev->next = new   前驱节点的 next 指向新节点（最后修改，完成插入）
 */
static inline void __list_add(struct list_head *new,
                               struct list_head *prev,
                               struct list_head *next)
{
    next->prev = new;   /* ① */
    new->next  = next;  /* ② */
    new->prev  = prev;  /* ③ */
    prev->next = new;   /* ④ */
}

/*
 * __list_del(prev, next)
 * 将 prev 和 next 直接相连，从而绕过（摘除）两者之间的节点。
 * 被摘除节点的指针不在此函数中处理，由调用方负责（置NULL或毒化）。
 *
 * 来源：include/linux/list.h - __list_del()
 */
static inline void __list_del(struct list_head *prev,
                               struct list_head *next)
{
    next->prev = prev;  /* next 的前驱改为 prev（跳过被删节点）*/
    prev->next = next;  /* prev 的后继改为 next（跳过被删节点）*/
}

/* ================================================================
 * §6  公开接口：插入操作
 * ================================================================ */

/**
 * list_add(new, head)
 * 在 head 之后插入新节点（头插法）。
 *
 * @new:  待插入的新节点（需已初始化，或至少 next/prev 可覆盖）
 * @head: 链表头节点（新节点将成为 head 的下一个节点）
 *
 * 特性：多次 list_add 后，最后加入的节点最先被遍历（LIFO/栈语义）。
 *
 * 来源：include/linux/list.h - list_add()
 * 原文：Insert a new entry after the specified head.
 *       This is good for implementing stacks.
 */
static inline void list_add(struct list_head *new,
                              struct list_head *head)
{
    __list_add(new, head, head->next);
}

/**
 * list_add_tail(new, head)
 * 在 head 之前插入新节点（尾插法）。
 * 由于是循环链表，head->prev 就是当前尾节点，
 * 因此等同于在链表末尾追加节点。
 *
 * @new:  待插入的新节点
 * @head: 链表头节点（新节点将成为 head 的上一个节点，即尾节点）
 *
 * 特性：多次 list_add_tail 后，最先加入的节点最先被遍历（FIFO/队列语义）。
 *
 * 来源：include/linux/list.h - list_add_tail()
 * 原文：Insert a new entry before the specified head.
 *       This is useful for implementing queues.
 */
static inline void list_add_tail(struct list_head *new,
                                   struct list_head *head)
{
    __list_add(new, head->prev, head);
}

/* ================================================================
 * §7  公开接口：删除操作
 * ================================================================ */

/**
 * list_del(entry)
 * 将节点从链表中摘除，并将其 next/prev 设为毒化指针。
 *
 * @entry: 要删除的节点
 *
 * 注意事项：
 *   - 删除后 list_empty(entry) 不会返回真（节点处于毒化状态）
 *   - 不要访问被删除节点的 next/prev，会触发段错误
 *   - 若需删除后继续使用该节点，请用 list_del_init()
 *
 * 来源：include/linux/list.h - list_del()
 * 原文：Note: list_empty() on entry does not return true after this,
 *       the entry is in an undefined state.
 */
static inline void list_del(struct list_head *entry)
{
    __list_del(entry->prev, entry->next);
    entry->next = LIST_POISON1; /* 毒化 next，防止悬挂访问 */
    entry->prev = LIST_POISON2; /* 毒化 prev，防止悬挂访问 */
}

/**
 * list_del_init(entry)
 * 将节点从链表中摘除，并重新初始化为空节点（自环）。
 *
 * @entry: 要删除的节点
 *
 * 与 list_del 的区别：
 *   - list_del：删除后节点处于毒化状态，不可安全使用
 *   - list_del_init：删除后节点重置为空链表头状态，可再次入链
 *
 * 来源：include/linux/list.h - list_del_init()
 * 原文：deletes entry from list and reinitialize it.
 */
static inline void list_del_init(struct list_head *entry)
{
    __list_del(entry->prev, entry->next);
    INIT_LIST_HEAD(entry); /* 重新自环，可重用 */
}

/* ================================================================
 * §8  公开接口：替换操作
 * ================================================================ */

/**
 * list_replace(old, new)
 * 用 new 节点原地替换链表中的 old 节点。
 * old 的所有邻居关系转移到 new；old 本身处于游离状态（需调用者处理）。
 *
 * @old: 被替换的节点（仍在链表中）
 * @new: 替换进来的新节点
 *
 * 来源：include/linux/list.h - list_replace()
 * 原文：replace old entry by new one
 */
static inline void list_replace(struct list_head *old,
                                  struct list_head *new)
{
    new->next       = old->next;
    new->next->prev = new;
    new->prev       = old->prev;
    new->prev->next = new;
}

/**
 * list_replace_init(old, new)
 * 替换 old 节点后，将 old 重新初始化为空节点，确保其可安全使用。
 *
 * 来源：include/linux/list.h - list_replace_init()
 */
static inline void list_replace_init(struct list_head *old,
                                       struct list_head *new)
{
    list_replace(old, new);
    INIT_LIST_HEAD(old);
}

/* ================================================================
 * §9  公开接口：节点移动
 * ================================================================ */

/**
 * list_move(list, head)
 * 将 list 节点从其当前链表中摘除，并插入到 head 之后（成为新的第一个元素）。
 *
 * @list: 要移动的节点
 * @head: 目标链表头
 *
 * 来源：include/linux/list.h - list_move()
 * 原文：delete from one list and add as another's head
 */
static inline void list_move(struct list_head *list,
                               struct list_head *head)
{
    __list_del(list->prev, list->next); /* 从原链表摘除 */
    list_add(list, head);               /* 插入目标链表头之后 */
}

/**
 * list_move_tail(list, head)
 * 将 list 节点从其当前链表中摘除，并追加到 head 链表的尾部。
 *
 * @list: 要移动的节点
 * @head: 目标链表头
 *
 * 来源：include/linux/list.h - list_move_tail()
 * 原文：delete from one list and add as another's tail
 */
static inline void list_move_tail(struct list_head *list,
                                    struct list_head *head)
{
    __list_del(list->prev, list->next);
    list_add_tail(list, head);
}

/* ================================================================
 * §10  公开接口：状态查询
 * ================================================================ */

/**
 * list_empty(head)
 * 判断链表是否为空（无数据节点）。
 *
 * @head: 链表头节点指针
 * @return: 非零（真）表示链表为空
 *
 * 原理：空链表时 head->next == head（自环），否则指向第一个数据节点。
 *
 * 来源：include/linux/list.h - list_empty()
 * 原文：tests whether a list is empty
 */
static inline int list_empty(const struct list_head *head)
{
    return head->next == head;
}

/**
 * list_is_singular(head)
 * 判断链表中是否恰好只有一个数据节点。
 *
 * @head: 链表头节点指针
 * @return: 非零（真）表示链表只有一个节点
 *
 * 原理：head->next != head（非空）且 head->next == head->prev（只有一个节点时两端相同）
 *
 * 来源：include/linux/list.h - list_is_singular()
 * 原文：tests whether a list has just one entry.
 */
static inline int list_is_singular(const struct list_head *head)
{
    return !list_empty(head) && (head->next == head->prev);
}

/**
 * list_is_head(list, head)
 * 判断 list 节点是否就是链表头 head。
 * 常用于遍历宏的循环终止判断。
 *
 * @list: 待检查的节点
 * @head: 链表头节点
 * @return: 非零（真）表示 list 即为 head
 *
 * 来源：include/linux/list.h - list_is_head()
 */
static inline int list_is_head(const struct list_head *list,
                                  const struct list_head *head)
{
    return list == head;
}

/* ================================================================
 * §11  公开接口：获取节点数据（entry 宏）
 * ================================================================ */

/**
 * list_entry(ptr, type, member)
 * 从 struct list_head 指针获取宿主结构体指针。
 * 是 container_of 的别名，语义更贴近"链表节点取数据"。
 *
 * @ptr:    指向 struct list_head 的指针
 * @type:   宿主结构体类型
 * @member: list_head 在宿主结构体中的字段名
 *
 * 来源：include/linux/list.h - list_entry()
 * 原文：get the struct for this entry
 *
 * 示例：
 *   struct my_node { int val; struct list_head link; };
 *   struct list_head *p = ...;
 *   struct my_node *n = list_entry(p, struct my_node, link);
 */
#define list_entry(ptr, type, member) \
    container_of(ptr, type, member)

/**
 * list_first_entry(ptr, type, member)
 * 获取链表中第一个节点的宿主结构体指针（head->next 对应的数据结构）。
 *
 * @ptr:    链表头指针
 * @type:   宿主结构体类型
 * @member: list_head 字段名
 *
 * ⚠️  调用前必须确认链表非空！空链表时行为未定义。
 *
 * 来源：include/linux/list.h - list_first_entry()
 * 原文：get the first element from a list
 *       Note, that list is expected to be not empty.
 */
#define list_first_entry(ptr, type, member) \
    list_entry((ptr)->next, type, member)

/**
 * list_last_entry(ptr, type, member)
 * 获取链表中最后一个节点的宿主结构体指针（head->prev 对应的数据结构）。
 *
 * ⚠️  调用前必须确认链表非空！
 *
 * 来源：include/linux/list.h - list_last_entry()
 * 原文：get the last element from a list
 */
#define list_last_entry(ptr, type, member) \
    list_entry((ptr)->prev, type, member)

/**
 * list_next_entry(pos, member)
 * 获取链表中 pos 之后的节点（宿主结构体）。
 *
 * @pos:    当前节点的宿主结构体指针
 * @member: list_head 字段名
 *
 * 来源：include/linux/list.h - list_next_entry()
 */
#define list_next_entry(pos, member) \
    list_entry((pos)->member.next, typeof(*(pos)), member)

/**
 * list_prev_entry(pos, member)
 * 获取链表中 pos 之前的节点（宿主结构体）。
 *
 * @pos:    当前节点的宿主结构体指针
 * @member: list_head 字段名
 *
 * 来源：include/linux/list.h - list_prev_entry()
 */
#define list_prev_entry(pos, member) \
    list_entry((pos)->member.prev, typeof(*(pos)), member)

/* ================================================================
 * §12  遍历宏
 * ================================================================ */

/**
 * list_for_each(pos, head)
 * 正向遍历链表中所有节点，pos 为 struct list_head * 游标。
 *
 * @pos:  循环游标（struct list_head *）
 * @head: 链表头节点
 *
 * ⚠️  遍历过程中不可删除 pos 节点，否则请使用 list_for_each_safe。
 *
 * 来源：include/linux/list.h - list_for_each()
 * 原文：iterate over a list
 */
#define list_for_each(pos, head) \
    for (pos = (head)->next; pos != (head); pos = pos->next)

/**
 * list_for_each_prev(pos, head)
 * 反向遍历链表（从尾节点到头节点方向）。
 *
 * @pos:  循环游标（struct list_head *）
 * @head: 链表头节点
 *
 * ⚠️  遍历过程中不可删除 pos 节点。
 *
 * 来源：include/linux/list.h - list_for_each_prev()
 * 原文：iterate over a list backwards
 */
#define list_for_each_prev(pos, head) \
    for (pos = (head)->prev; pos != (head); pos = pos->prev)

/**
 * list_for_each_safe(pos, n, head)
 * 正向遍历链表，遍历过程中可安全删除当前节点 pos。
 *
 * @pos:  循环游标（struct list_head *）
 * @n:    临时保存 pos->next 的辅助指针（struct list_head *）
 * @head: 链表头节点
 *
 * 原理：每次迭代开始前预先保存 pos->next 到 n，
 *        即使 pos 从链表中删除，迭代器仍可从 n 继续推进。
 *
 * 来源：include/linux/list.h - list_for_each_safe()
 * 原文：iterate over a list safe against removal of list entry
 */
#define list_for_each_safe(pos, n, head) \
    for (pos = (head)->next, n = pos->next; \
         pos != (head); \
         pos = n, n = pos->next)

/**
 * list_for_each_entry(pos, head, member)
 * 正向遍历链表，pos 直接是宿主结构体指针（最常用的遍历宏）。
 *
 * @pos:    循环游标（宿主结构体指针，如 struct foo *）
 * @head:   链表头节点
 * @member: struct list_head 在宿主结构体中的字段名
 *
 * ⚠️  遍历过程中不可删除 pos 节点，请使用 list_for_each_entry_safe。
 *
 * 来源：include/linux/list.h - list_for_each_entry()
 * 原文：iterate over list of given type
 */
#define list_for_each_entry(pos, head, member)                         \
    for (pos = list_entry((head)->next, typeof(*pos), member);       \
         &pos->member != (head);                                       \
         pos = list_entry(pos->member.next, typeof(*pos), member))

/**
 * list_for_each_entry_reverse(pos, head, member)
 * 反向遍历链表，pos 为宿主结构体指针。
 *
 * @pos:    循环游标（宿主结构体指针）
 * @head:   链表头节点
 * @member: struct list_head 字段名
 *
 * ⚠️  遍历过程中不可删除 pos 节点。
 *
 * 来源：include/linux/list.h - list_for_each_entry_reverse()
 * 原文：iterate backwards over list of given type.
 */
#define list_for_each_entry_reverse(pos, head, member)                 \
    for (pos = list_entry((head)->prev, typeof(*pos), member);       \
         &pos->member != (head);                                       \
         pos = list_entry(pos->member.prev, typeof(*pos), member))

/**
 * list_for_each_entry_safe(pos, n, head, member)
 * 正向安全遍历，遍历过程中可删除当前节点 pos。
 *
 * @pos:    循环游标（宿主结构体指针）
 * @n:      临时存储下一个宿主结构体指针（同 pos 类型）
 * @head:   链表头节点
 * @member: struct list_head 字段名
 *
 * ⚠️  不要在循环体中修改 n。
 * ⚠️  仅保护当前 pos 被删除的情况，并发修改仍需外部同步。
 *
 * 来源：include/linux/list.h - list_for_each_entry_safe()
 * 原文：iterate over list of given type safe against removal of list entry
 */
#define list_for_each_entry_safe(pos, n, head, member)                 \
    for (pos = list_entry((head)->next, typeof(*pos), member),       \
         n   = list_entry(pos->member.next, typeof(*pos), member);   \
         &pos->member != (head);                                       \
         pos = n, n = list_entry(n->member.next, typeof(*n), member))

/**
 * list_for_each_entry_safe_reverse(pos, n, head, member)
 * 反向安全遍历，遍历过程中可删除当前节ç
 *
 * @pos:    循环游标（宿主结构体指针）
 * @n:      临时存储上一个宿主结构体指针
 * @head:   链表头节点
 * @member: struct list_head 字段名
 *
 * 来源：include/linux/list.h - list_for_each_entry_safe_reverse()
 * 原文：iterate backwards over list safe against removal
 */
#define list_for_each_entry_safe_reverse(pos, n, head, member)         \
    for (pos = list_entry((head)->prev, typeof(*pos), member),       \
         n   = list_entry(pos->member.prev, typeof(*pos), member);   \
         &pos->member != (head);                                       \
         pos = n, n = list_entry(n->member.prev, typeof(*n), member))

/* ================================================================
 * §13  拼接操作
 * ================================================================ */

/*
 * __list_splice(list, prev, next)
 * 将 list 链表的所有节点移接到 prev 与 next 之间（内部函数）。
 * 调用方需确认 list 非空。
 *
 * 来源：include/linux/list.h - __list_splice()
 */
static inline void __list_splice(const struct list_head *list,
                                   struct list_head *prev,
                                   struct list_head *next)
{
    struct list_head *first = list->next; /* list 的第一个实际节点 */
    struct list_head *last  = list->prev; /* list 的最后一个实际节点 */

    first->prev = prev;  /* 第一节点的前驱连接到 prev */
    prev->next  = first; /* prev 的后继连接到第一节点 */

    last->next  = next;  /* 最后节点的后继连接到 next */
    next->prev  = last;  /* next 的前驱连接到最后节点 */
}

/**
 * list_splice(list, head)
 * 将 list 链表的全部节点插入到 head 之后（作为 head 的新的第一批节点）。
 * 操作完成后 list 处于未定义状态，如需继续使用应调用 list_splice_init。
 *
 * @list: 源链表头（其节点将被移走）
 * @head: 目标链表头（节点插入此头之后）
 *
 * 来源：include/linux/list.h - list_splice()
 * 原文：join two lists, this is designed for stacks
 */
static inline void list_splice(const struct list_head *list,
                                 struct list_head *head)
{
    if (!list_empty(list))
        __list_splice(list, head, head->next);
}

/**
 * list_splice_tail(list, head)
 * 将 list 链表的全部节点追加到 head 链表的尾部。
 *
 * @list: 源链表头
 * @head: 目标链表头
 *
 * 来源：include/linux/list.h - list_splice_tail()
 * 原文：join two lists, each list being a queue
 */
static inline void list_splice_tail(struct list_head *list,
                                       struct list_head *head)
{
    if (!list_empty(list))
        __list_splice(list, head->prev, head);
}

/**
 * list_splice_init(list, head)
 * 拼接后将 list 重新初始化为空链表头，避免 list 成为悬挂头节点。
 *
 * @list: 源链表头（操作后被重置为空）
 * @head: 目标链表头
 *
 * 来源：include/linux/list.h - list_splice_init()
 * 原文：join two lists and reinitialise the emptied list.
 *       The list at list is reinitialised.
 */
static inline void list_splice_init(struct list_head *list,
                                       struct list_head *head)
{
    if (!list_empty(list)) {
        __list_splice(list, head, head->next);
        INIT_LIST_HEAD(list); /* list 清空，避免悬挂 */
    }
}

/**
 * list_splice_tail_init(list, head)
 * 拼接到尾部后，将 list 重新初始化为空。
 *
 * 来源：include/linux/list.h - list_splice_tail_init()
 */
static inline void list_splice_tail_init(struct list_head *list,
                                            struct list_head *head)
{
    if (!list_empty(list)) {
        __list_splice(list, head->prev, head);
        INIT_LIST_HEAD(list);
    }
}

/* ================================================================
 * §14  切割操作
 * ================================================= */

/**
 * list_cut_position(list, head, entry)
 * 从 head 链表切出前段（从第一个节点到 entry，含 entry），移入 list 链表。
 * head 链表剩余 entry 之后的节点；list 原有内容被覆盖。
 *
 * @list:  目标链表头（接收切出的节点）
 * @head:  源链表头
 * @entry: 切割点（此节点及其之前所有节点移入 list）
 *
 * 来源：include/linux/list.h - list_cut_position()
 * 原文（官方文档）：
 *   list_cut_position() removes all list entrfrom head up to
 *   and including entry, placing them in list instead.
 *   参见：https://docs.kernel.org/core-api/list.html#cutting-a-list
 */
static inline void list_cut_position(struct list_head *list,
                                        struct list_head *head,
                                        struct list_head *entry)
{
    if (list_empty(head))
        return;
    if (list_is_singular(head) && head->next != entry)
        return;
    if (entry == head) {
        INIT_LIST_HEAD(list); /* e是 head，切出空段 */
    } else {
        struct list_head *new_first = entry->next;
        list->next       = head->next;
        list->next->prev = list;
        list->prev       = entry;
        entry->next      = list;
        head->next       = new_first;
        new_first->prev  = head;
    }
}

/**
 * list_cut_before(list, head, entry)
 * 从 head 链表切出前段（不含 entry 本身），移入 list 链表。
 * head 链表从 entry 开始保留；list 接收 entry 之前的所有节点ã @list:  目标链表头（接收切出的节点）
 * @head:  源链表头
 * @entry: 切割点（此节点保留在 head 中，之前的节点移入 list）
 *
 * 来源：include/linux/list.h - list_cut_before()
 * 原文（官方文档）：
 *   list_cut_before() removes all list entries from head up to
 *   but excluding entry, placing them in list instead.
 *   参见：https://docs.kernel.org/core-api/list.html#cutting-a-list
 */
static inline void list_cut_before(struct list_head *list,
                            struct list_head *head,
                                      struct list_head *entry)
{
    if (head->next == entry) {
        INIT_LIST_HEAD(list); /* entry 是第一个节点，没有"之前"的节点 */
        return;
    }
    list->next       = head->next;
    list->next->prev = list;
    list->prev       = entry->prev;
    list->prev->next = list;
    head->next       = entry;
    entry->prev      = head;
}

#endif /* _USER_LIST_H */
