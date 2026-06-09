/**
 * @file mem_frag_diag.h
 * @brief Linux glibc 堆内存碎片诊断接口（mallinfo2 / malloc_info / malloc_trim）
 *
 * 基于 glibc ptmalloc2 诊断 API，封装文档《Linux 内存碎片分析指南》中的
 * 指标计算、报告输出与缓解操作，便于集成到现有 C 进程。
 *
 * 平台要求：Linux + glibc。非 glibc 环境 API 返回 -ENOTSUP。
 *
 * 线程安全：
 *   - malloc_info / malloc_stats / malloc_trim：MT-Safe（glibc）
 *   - mallinfo2：MT-Unsafe，多线程场景请在外部加锁或仅用 malloc_info
 */

#ifndef MEM_FRAG_DIAG_H
#define MEM_FRAG_DIAG_H

#include <stdio.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 本模块自定义：当前平台不支持 glibc 诊断 API */
#define MEM_FRAG_DIAG_ENOTSUP  1001

/** mem_frag_diag_run() 行为标志 */
#define MEM_FRAG_DIAG_F_STATS_STDERR   (1u << 0) /**< 调用 malloc_stats() 输出到 stderr */
#define MEM_FRAG_DIAG_F_DUMP_XML       (1u << 1) /**< 追加 malloc_info XML 到 report_fp */
#define MEM_FRAG_DIAG_F_COMPACT_FIRST  (1u << 2) /**< 诊断前先 compact（M_MXFAST=0 + trim） */

/** 碎片严重级别（依据 keepcost / fordblks，见文档 §7） */
typedef enum mem_frag_severity {
    MEM_FRAG_SEVERITY_UNKNOWN = 0,
    MEM_FRAG_SEVERITY_HEALTHY,   /**< trim_ratio >= 60% */
    MEM_FRAG_SEVERITY_WARNING,   /**< 30% <= trim_ratio < 60% */
    MEM_FRAG_SEVERITY_CRITICAL   /**< trim_ratio < 30% */
} mem_frag_severity_t;

/** mallinfo2 原始字段（main arena，glibc 2.33+） */
typedef struct mem_frag_raw_stats {
    size_t arena;     /**< 非 mmap 堆总字节数 */
    size_t ordblks;   /**< 普通空闲块数量 */
    size_t smblks;    /**< fastbin 空闲块数量 */
    size_t hblks;     /**< mmap 区域数量 */
    size_t hblkhd;    /**< mmap 区域总字节数 */
    size_t fsmblks;   /**< fastbin 空闲总字节数 */
    size_t uordblks;  /**< 已分配字节数 */
    size_t fordblks;  /**< 空闲块总字节数 */
    size_t keepcost;  /**< 堆顶可 trim 字节数 */
    int from_malloc_info; /**< 1=来自 malloc_info XML（keepcost 可能不可用） */
} mem_frag_raw_stats_t;

/** 文档 §7 衍生指标 */
typedef struct mem_frag_metrics {
    double frag_ratio_pct;    /**< (fordblks - keepcost) / arena * 100 */
    double utilization_pct;   /**< uordblks / arena * 100 */
    double fastbin_hold_pct;  /**< fsmblks / fordblks * 100 */
    double trim_ratio_pct;    /**< keepcost / fordblks * 100 */
    mem_frag_severity_t severity;
} mem_frag_metrics_t;

/** 一次采集的完整快照 */
typedef struct mem_frag_snapshot {
    mem_frag_raw_stats_t raw;
    mem_frag_metrics_t metrics;
    int supported; /**< 1=成功采集 mallinfo2/mallinfo */
} mem_frag_snapshot_t;

/** mallopt 调优参数（文档 §2 / §9） */
typedef struct mem_frag_tune_opts {
    int apply_mxfast;
    int mxfast;               /**< M_MXFAST；0 表示禁用 fastbin */
    int apply_trim_threshold;
    int trim_threshold;       /**< M_TRIM_THRESHOLD；-1 禁用 trimming */
    int apply_mmap_threshold;
    int mmap_threshold;       /**< M_MMAP_THRESHOLD */
    int apply_arena_max;
    int arena_max;            /**< M_ARENA_MAX */
} mem_frag_tune_opts_t;

/** mem_frag_diag_compact_heap() 结果 */
typedef struct mem_frag_compact_result {
    int trim_released;   /**< malloc_trim 返回值：1=已归还内存 */
    int mxfast_disabled; /**< 是否已设置 M_MXFAST=0 */
} mem_frag_compact_result_t;

/**
 * @brief 检测当前进程是否可使用 glibc 诊断 API
 * @return 1 支持，0 不支持
 */
int mem_frag_diag_is_supported(void);

/**
 * @brief 采集 mallinfo2（或降级 mallinfo）原始统计
 * @param out 输出；不可为 NULL
 * @return 0 成功；负 errno；不支持时返回 -MEM_FRAG_DIAG_ENOTSUP
 */
int mem_frag_diag_collect_raw(mem_frag_raw_stats_t *out);

/**
 * @brief 由原始统计计算碎片指标
 * @param raw 输入；不可为 NULL
 * @param out 输出；不可为 NULL
 */
void mem_frag_diag_compute_metrics(const mem_frag_raw_stats_t *raw,
                                   mem_frag_metrics_t *out);

/**
 * @brief 采集并计算完整快照
 * @param out 输出；不可为 NULL
 * @return 同 mem_frag_diag_collect_raw()
 */
int mem_frag_diag_snapshot(mem_frag_snapshot_t *out);

/**
 * @brief 输出人类可读诊断报告
 * @param fp 输出流；不可为 NULL
 * @param snap 快照；不可为 NULL
 * @return 0 成功；负 errno
 */
int mem_frag_diag_print_report(FILE *fp, const mem_frag_snapshot_t *snap);

/**
 * @brief 调用 malloc_stats()（glibc 固定输出 stderr，无法重定向）
 */
void mem_frag_diag_malloc_stats_stderr(void);

/**
 * @brief 导出 malloc_info XML 到指定流（覆盖所有 arena）
 * @param fp 输出流；不可为 NULL
 * @return 0 成功；负 errno
 */
int mem_frag_diag_dump_malloc_info(FILE *fp);

/**
 * @brief 导出 malloc_info XML 到动态缓冲区
 * @param buf 输出指针，成功后需 caller free(*buf)；不可为 NULL
 * @param len 输出长度；不可为 NULL
 * @return 0 成功；负 errno
 */
int mem_frag_diag_dump_malloc_info_buf(char **buf, size_t *len);

/**
 * @brief 执行文档推荐的三步诊断流程
 *
 * 默认：可选 malloc_stats(stderr) → snapshot 报告 → 可选 XML。
 *
 * @param report_fp 文本报告输出；可为 NULL（跳过文本报告）
 * @param flags MEM_FRAG_DIAG_F_* 组合
 * @return 0 成功；负 errno
 */
int mem_frag_diag_run(FILE *report_fp, unsigned flags);

/**
 * @brief 缓解碎片：mallopt(M_MXFAST, 0) + malloc_trim(pad)
 * @param trim_pad malloc_trim 保留字节；0 表示最积极归还
 * @param out 可选结果；可为 NULL
 * @return 0 成功；负 errno
 */
int mem_frag_diag_compact_heap(size_t trim_pad,
                               mem_frag_compact_result_t *out);

/**
 * @brief 批量应用 mallopt 调优参数
 * @param opts 调优项；不可为 NULL
 * @return 0 全部成功；>0 失败项个数；负 errno
 */
int mem_frag_diag_apply_tune(const mem_frag_tune_opts_t *opts);

/**
 * @brief 注册 SIGUSR1：收到 kill -USR1 PID 时 dump malloc_info
 *
 * @param dump_path 输出文件路径；NULL 表示写到 stderr
 * @return 0 成功；负 errno
 *
 * @note 信号处理函数内调用 malloc_info/fopen 非 async-signal-safe，
 *       仅建议调试/运维场景使用，生产环境请改用 signalfd 或 flag 轮询。
 */
int mem_frag_diag_install_sigusr1(const char *dump_path);

/**
 * @brief 从 /proc/self/smaps 汇总 [heap] 区域 RSS（字节）
 * @return RSS 字节数；失败返回 -1 并设置 errno
 */
int64_t mem_frag_diag_read_heap_rss(void);

/**
 * @brief severity 转可读字符串
 */
const char *mem_frag_diag_severity_str(mem_frag_severity_t severity);

#ifdef __cplusplus
}
#endif

#endif /* MEM_FRAG_DIAG_H */
