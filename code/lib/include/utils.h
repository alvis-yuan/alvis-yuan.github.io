#ifndef UTILS_H
#define UTILS_H

#include <stdarg.h>  /* va_list */
#include <stddef.h>  /* size_t */
#include <stdio.h>   /* FILE, fprintf, stderr, stdout, vfprintf */

static inline void utils_log_write(FILE *stream,
                                   const char *level,
                                   const char *func,
                                   int line,
                                   const char *fmt,
                                   ...)
{
    va_list ap;

    fprintf(stream, "[%s][%s-%d] ", level, func, line);
    va_start(ap, fmt);
    vfprintf(stream, fmt, ap);
    va_end(ap);
    fprintf(stream, "\n");
}

#define LogError(fmt, arg...) \
    utils_log_write(stderr, "ERROR", __func__, __LINE__, (fmt), ##arg)

#define LogInfo(fmt, arg...) \
    utils_log_write(stdout, "INFO", __func__, __LINE__, (fmt), ##arg)
#define REQUIRE(cond, msg) do { if (!(cond)) { LogError("%s", msg); goto out; } } while (0)

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

#endif /* UTILS_H */