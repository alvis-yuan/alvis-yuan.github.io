/*
 * 修改作者: GitHub Copilot
 * 修改日期: 2026-06-05
 * 修改原因: 规范化老版本 cJSON 适配辅助接口并补齐常用 getter
 */

#ifndef UTILS_CJSON_ADAPT_H
#define UTILS_CJSON_ADAPT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#if defined(__has_include)
#if __has_include(<cJSON.h>)
#include <cJSON.h>
#elif __has_include("cJSON.h")
#include "cJSON.h"
#elif __has_include(<cjson/cJSON.h>)
#include <cjson/cJSON.h>
#endif
#endif

#if !defined(cJSON__h) && !defined(CJSON__H) && !defined(CJSON_H)
typedef struct cJSON {
    struct cJSON *next;
    struct cJSON *prev;
    struct cJSON *child;
    int type;
    char *valuestring;
    int valueint;
    double valuedouble;
    char *string;
} cJSON;

enum {
    cJSON_False = 0,
    cJSON_True = 1,
    cJSON_NULL = 2,
    cJSON_Number = 3,
    cJSON_String = 4,
    cJSON_Array = 5,
    cJSON_Object = 6,
    cJSON_IsReference = 256,
    cJSON_StringIsConst = 512,
};

extern cJSON *cJSON_GetObjectItem(cJSON *object, const char *string);
extern int cJSON_GetArraySize(cJSON *array);
extern cJSON *cJSON_GetArrayItem(cJSON *array, int index);
#endif

/**
 * @brief 老版本 cJSON 的基础类型掩码。
 *
 * 旧版 cJSON 会在 type 字段上叠加 reference 等标志位，因此直接比较
 * type == cJSON_Object 可能误判，需要先取基础类型。
 */
#define CJSON_ADAPT_TYPE_MASK 0xFF

static inline int _cjson_adapt_get_type(const cJSON *item)
{
    if (item == NULL) {
        return -1;
    }

    return item->type & CJSON_ADAPT_TYPE_MASK;
}

static inline bool _cjson_adapt_type_is(const cJSON *item, int type)
{
    return _cjson_adapt_get_type(item) == type;
}

static inline cJSON *_cjson_adapt_get_item(cJSON *parent, const char *key)
{
    if (parent == NULL || key == NULL) {
        return NULL;
    }

    return cJSON_GetObjectItem(parent, key);
}

static inline bool _cjson_adapt_copy_string(const cJSON *item, char *out,
        size_t out_len)
{
    if (item == NULL || !_cjson_adapt_type_is(item, cJSON_String) ||
        item->valuestring == NULL || out == NULL || out_len == 0) {
        return false;
    }

    snprintf(out, out_len, "%s", item->valuestring);
    return true;
}

/**
 * @brief 获取对象字段，并要求字段类型为 object。
 *
 * @param parent 父对象。
 * @param key 字段名。
 *
 * @return 成功返回对象节点，失败返回 NULL。
 */
static inline cJSON *j_get_obj(cJSON *parent, const char *key)
{
    cJSON *item = _cjson_adapt_get_item(parent, key);
    return _cjson_adapt_type_is(item, cJSON_Object) ? item : NULL;
}

/**
 * @brief 获取对象字段，并要求字段类型为 array。
 *
 * @param parent 父对象。
 * @param key 字段名。
 *
 * @return 成功返回数组节点，失败返回 NULL。
 */
static inline cJSON *j_get_arr(cJSON *parent, const char *key)
{
    cJSON *item = _cjson_adapt_get_item(parent, key);
    return _cjson_adapt_type_is(item, cJSON_Array) ? item : NULL;
}

/**
 * @brief 获取对象字段，并要求字段类型为 string。
 *
 * @param parent 父对象。
 * @param key 字段名。
 * @param out 输出缓冲区。
 * @param out_len 输出缓冲区长度。
 *
 * @return 成功返回 true，失败返回 false。
 */
static inline bool j_get_str(cJSON *parent, const char *key, char *out,
                             size_t out_len)
{
    cJSON *item = _cjson_adapt_get_item(parent, key);
    return _cjson_adapt_copy_string(item, out, out_len);
}

/**
 * @brief 获取对象字段，并要求字段类型为 number，输出 int 值。
 *
 * @param parent 父对象。
 * @param key 字段名。
 * @param out 输出整数地址。
 *
 * @return 成功返回 true，失败返回 false。
 */
static inline bool j_get_int(cJSON *parent, const char *key, int *out)
{
    cJSON *item = _cjson_adapt_get_item(parent, key);

    if (!_cjson_adapt_type_is(item, cJSON_Number) || out == NULL) {
        return false;
    }

    *out = item->valueint;
    return true;
}

/**
 * @brief 获取对象字段，并要求字段类型为 number，输出 double 值。
 *
 * @param parent 父对象。
 * @param key 字段名。
 * @param out 输出浮点数地址。
 *
 * @return 成功返回 true，失败返回 false。
 */
static inline bool j_get_double(cJSON *parent, const char *key, double *out)
{
    cJSON *item = _cjson_adapt_get_item(parent, key);

    if (!_cjson_adapt_type_is(item, cJSON_Number) || out == NULL) {
        return false;
    }

    *out = item->valuedouble;
    return true;
}

/**
 * @brief 获取对象字段，并要求字段类型为 bool。
 *
 * @param parent 父对象。
 * @param key 字段名。
 * @param out 输出布尔值地址。
 *
 * @return 成功返回 true，失败返回 false。
 */
static inline bool j_get_bool(cJSON *parent, const char *key, bool *out)
{
    cJSON *item = _cjson_adapt_get_item(parent, key);

    if (out == NULL ||
        (!_cjson_adapt_type_is(item, cJSON_False) &&
         !_cjson_adapt_type_is(item, cJSON_True))) {
        return false;
    }

    *out = _cjson_adapt_type_is(item, cJSON_True);
    return true;
}

/**
 * @brief 获取数组字段的元素个数。
 *
 * @param parent 父对象。
 * @param arr_key 数组字段名。
 *
 * @return 成功返回数组长度，失败返回 -1。
 */
static inline int j_get_arr_size(cJSON *parent, const char *arr_key)
{
    cJSON *arr = j_get_arr(parent, arr_key);

    if (arr == NULL) {
        return -1;
    }

    return cJSON_GetArraySize(arr);
}

/**
 * @brief 获取数组字段的指定元素。
 *
 * @param parent 父对象。
 * @param arr_key 数组字段名。
 * @param index 元素下标。
 *
 * @return 成功返回元素节点，失败返回 NULL。
 */
static inline cJSON *j_get_arr_item(cJSON *parent, const char *arr_key,
                                    int index)
{
    cJSON *arr = NULL;

    if (index < 0) {
        return NULL;
    }

    arr = j_get_arr(parent, arr_key);

    if (arr == NULL || index >= cJSON_GetArraySize(arr)) {
        return NULL;
    }

    return cJSON_GetArrayItem(arr, index);
}

/**
 * @brief 获取数组字段中指定元素的字符串值。
 *
 * @param parent 父对象。
 * @param arr_key 数组字段名。
 * @param index 元素下标。
 * @param out 输出缓冲区。
 * @param out_len 输出缓冲区长度。
 *
 * @return 成功返回 true，失败返回 false。
 */
static inline bool j_get_arr_item_str(cJSON *parent, const char *arr_key,
                                      int index, char *out, size_t out_len)
{
    cJSON *item = j_get_arr_item(parent, arr_key, index);
    return _cjson_adapt_copy_string(item, out, out_len);
}

/**
 * @brief 获取数组字段中指定元素的整数值。
 *
 * @param parent 父对象。
 * @param arr_key 数组字段名。
 * @param index 元素下标。
 * @param out 输出整数地址。
 *
 * @return 成功返回 true，失败返回 false。
 */
static inline bool j_get_arr_item_int(cJSON *parent, const char *arr_key,
                                      int index, int *out)
{
    cJSON *item = j_get_arr_item(parent, arr_key, index);

    if (!_cjson_adapt_type_is(item, cJSON_Number) || out == NULL) {
        return false;
    }

    *out = item->valueint;
    return true;
}

/**
 * @brief 获取数组字段中指定元素的布尔值。
 *
 * @param parent 父对象。
 * @param arr_key 数组字段名。
 * @param index 元素下标。
 * @param out 输出布尔值地址。
 *
 * @return 成功返回 true，失败返回 false。
 */
static inline bool j_get_arr_item_bool(cJSON *parent, const char *arr_key,
                                       int index, bool *out)
{
    cJSON *item = j_get_arr_item(parent, arr_key, index);

    if (out == NULL ||
        (!_cjson_adapt_type_is(item, cJSON_False) &&
         !_cjson_adapt_type_is(item, cJSON_True))) {
        return false;
    }

    *out = _cjson_adapt_type_is(item, cJSON_True);
    return true;
}

/**
 * @brief 获取数组字段中指定元素，并要求元素类型为 object。
 *
 * @param parent 父对象。
 * @param arr_key 数组字段名。
 * @param index 元素下标。
 *
 * @return 成功返回对象节点，失败返回 NULL。
 */
static inline cJSON *j_get_arr_item_obj(cJSON *parent, const char *arr_key,
                                        int index)
{
    cJSON *item = j_get_arr_item(parent, arr_key, index);
    return _cjson_adapt_type_is(item, cJSON_Object) ? item : NULL;
}

/**
 * @brief 按 key 路径逐层向下获取对象节点。
 *
 * @param root 起始对象。
 * @param keys key 路径数组。
 * @param nkeys key 数量。
 *
 * @return 成功返回路径终点节点，失败返回 NULL。
 */
static inline cJSON *j_walk_obj(cJSON *root, const char *const keys[],
                                int nkeys)
{
    cJSON *cur = root;
    int i = 0;

    if (nkeys < 0 || (nkeys > 0 && keys == NULL)) {
        return NULL;
    }

    for (i = 0; i < nkeys; ++i) {
        if (!_cjson_adapt_type_is(cur, cJSON_Object) || keys[i] == NULL) {
            return NULL;
        }

        cur = cJSON_GetObjectItem(cur, keys[i]);
    }

    return cur;
}

#endif
