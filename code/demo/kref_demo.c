#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

#include "kref.h"
#include "utils.h"

struct my_obj {
    struct kref ref;
    int data;
};

/**
 * @brief kref_put 的 release 回调函数
 */
static void my_obj_release(struct kref *ref)
{
    struct my_obj *obj = container_of(ref, struct my_obj, ref);
    printf("[%lu] Releasing my_obj, data = %d...\n", pthread_self(), obj->data);
    free(obj);
}

/**
 * @brief 线程的工作函数
 */
static void *thread_func(void *arg)
{
    struct my_obj *obj = arg;

    printf("[%lu] Thread accessing obj: data = %d, refcount = %u\n",
           pthread_self(), obj->data, kref_read(&obj->ref));

    // 模拟访问、处理耗时操作
    usleep(100000); // 100ms

    printf("[%lu] Thread finished, dropping reference...\n", pthread_self());
    kref_put(&obj->ref, my_obj_release);

    return NULL;
}

int main(void)
{
    struct my_obj *obj = malloc(sizeof(struct my_obj));
    if (!obj) {
        perror("malloc");
        return -1;
    }

    // 初始化 kref 对象，初始引用数为 1
    kref_init(&obj->ref);
    obj->data = 100;

    printf("[%lu] Main thread: obj created, initial refcount = %u\n",
           pthread_self(), kref_read(&obj->ref));

    pthread_t t1, t2;

    // 分发给第一个线程前，增加引用
    kref_get(&obj->ref);
    if (pthread_create(&t1, NULL, thread_func, obj) != 0) {
        perror("pthread_create t1");
        kref_put(&obj->ref, my_obj_release); // 失败时抵消 get
    }

    // 分发给第二个线程前，增加引用
    kref_get(&obj->ref);
    if (pthread_create(&t2, NULL, thread_func, obj) != 0) {
        perror("pthread_create t2");
        kref_put(&obj->ref, my_obj_release); // 失败时抵消 get
    }

    // 主线程放弃自己的初始引用（从此处起，主线程不再安全访问 obj）
    printf("[%lu] Main thread: dropping initial reference...\n", pthread_self());
    kref_put(&obj->ref, my_obj_release);

    // 等待线程执行完毕
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

    printf("[%lu] Main thread: all threads done\n", pthread_self());
    return 0;
}
