/**
 * @file mem_frag_diag.c
 * @brief Linux glibc 堆内存碎片诊断实现
 *
 * 编译示例：
 *   gcc -std=gnu99 -O2 -Wall -Wextra -D_GNU_SOURCE \
 *       -c mem_frag_diag.c -o mem_frag_diag.o
 *
 * 说明：glibc 2.33 以下无 mallinfo2()，本模块降级解析 malloc_info XML，
 *       以避免 GCC -O2 下 mallinfo() 与 __builtin_malloc 的兼容问题。
 */

#define _GNU_SOURCE

#include "mem_frag_diag.h"

#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <malloc.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__linux__) && defined(__GLIBC__)
#define MEM_FRAG_DIAG_HAVE_GLIBC 1
#else
#define MEM_FRAG_DIAG_HAVE_GLIBC 0
#endif

#if MEM_FRAG_DIAG_HAVE_GLIBC && defined(__GLIBC__) && defined(__GLIBC_MINOR__)
#if (__GLIBC__ > 2) || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 33)
#define MEM_FRAG_DIAG_HAVE_MALLINFO2 1
#endif
#endif

#ifndef MEM_FRAG_DIAG_HAVE_MALLINFO2
#define MEM_FRAG_DIAG_HAVE_MALLINFO2 0
#endif

static const char *g_sigusr1_dump_path;

static int
mem_frag_diag_neg_errno(int err)
{
    if (err <= 0) {
        err = EINVAL;
    }
    return -err;
}

#if MEM_FRAG_DIAG_HAVE_MALLINFO2
static void
mem_frag_diag_fill_raw_from_mallinfo2(mem_frag_raw_stats_t *out,
                                      const struct mallinfo2 *mi)
{
    out->arena     = mi->arena;
    out->ordblks   = mi->ordblks;
    out->smblks    = mi->smblks;
    out->hblks     = mi->hblks;
    out->hblkhd    = mi->hblkhd;
    out->fsmblks   = mi->fsmblks;
    out->uordblks  = mi->uordblks;
    out->fordblks  = mi->fordblks;
    out->keepcost  = mi->keepcost;
    out->from_malloc_info = 0;
}
#endif

static void
mem_frag_diag_raw_clear(mem_frag_raw_stats_t *out)
{
    memset(out, 0, sizeof(*out));
}

#if MEM_FRAG_DIAG_HAVE_GLIBC
static int
mem_frag_diag_parse_xml_size_attr(const char *line, const char *attr,
                                  size_t *value)
{
    char pattern[48];
    const char *p;
    unsigned long v;

    snprintf(pattern, sizeof(pattern), "%s=\"", attr);
    p = strstr(line, pattern);
    if (p == NULL) {
        return -1;
    }
    p += strlen(pattern);
    if (sscanf(p, "%lu", &v) != 1) {
        return -1;
    }
    *value = (size_t)v;
    return 0;
}

static const char *
mem_frag_diag_xml_last_tag(const char *xml, const char *prefix)
{
    const char *pos;
    const char *last;

    last = NULL;
    pos  = xml;
    while ((pos = strstr(pos, prefix)) != NULL) {
        last = pos;
        pos += strlen(prefix);
    }
    return last;
}

static int
mem_frag_diag_parse_xml_total(const char *xml, const char *type,
                              size_t *count, size_t *size)
{
    char prefix[40];
    const char *line;
    unsigned long c;
    unsigned long s;

    snprintf(prefix, sizeof(prefix), "<total type=\"%s\"", type);
    line = mem_frag_diag_xml_last_tag(xml, prefix);
    if (line == NULL) {
        return -1;
    }
    if (sscanf(line, "<total type=\"%*[^\"]\" count=\"%lu\" size=\"%lu\"/>",
               &c, &s) != 2) {
        return -1;
    }
    *count = (size_t)c;
    *size  = (size_t)s;
    return 0;
}

static int
mem_frag_diag_parse_xml_system_current(const char *xml, size_t *system_bytes)
{
    const char *line;

    line = mem_frag_diag_xml_last_tag(xml, "<system type=\"current\"");
    if (line == NULL) {
        return -1;
    }
    return mem_frag_diag_parse_xml_size_attr(line, "size", system_bytes);
}

static int
mem_frag_diag_collect_from_malloc_info(mem_frag_raw_stats_t *out)
{
    char *xml;
    size_t xml_len;
    size_t fast_count;
    size_t fast_size;
    size_t rest_count;
    size_t rest_size;
    size_t mmap_count;
    size_t mmap_size;
    size_t system_bytes;
    int rc;

    rc = mem_frag_diag_dump_malloc_info_buf(&xml, &xml_len);
    if (rc != 0) {
        return rc;
    }

    mem_frag_diag_raw_clear(out);
    out->from_malloc_info = 1;

    if (mem_frag_diag_parse_xml_total(xml, "fast", &fast_count, &fast_size) != 0 ||
        mem_frag_diag_parse_xml_total(xml, "rest", &rest_count, &rest_size) != 0 ||
        mem_frag_diag_parse_xml_system_current(xml, &system_bytes) != 0) {
        free(xml);
        return mem_frag_diag_neg_errno(EIO);
    }

    if (mem_frag_diag_parse_xml_total(xml, "mmap", &mmap_count, &mmap_size) != 0) {
        mmap_count = 0U;
        mmap_size  = 0U;
    }

    out->smblks   = fast_count;
    out->fsmblks  = fast_size;
    out->ordblks  = rest_count;
    out->hblks    = mmap_count;
    out->hblkhd   = mmap_size;
    out->fordblks = fast_size + rest_size;
    out->arena    = system_bytes;
    out->keepcost = 0U;

    if (system_bytes >= out->fordblks) {
        out->uordblks = system_bytes - out->fordblks;
    }

    free(xml);
    return 0;
}
#endif /* MEM_FRAG_DIAG_HAVE_GLIBC */

int
mem_frag_diag_is_supported(void)
{
    return MEM_FRAG_DIAG_HAVE_GLIBC;
}

int
mem_frag_diag_collect_raw(mem_frag_raw_stats_t *out)
{
    int rc;

    if (out == NULL) {
        return mem_frag_diag_neg_errno(EINVAL);
    }

    if (!MEM_FRAG_DIAG_HAVE_GLIBC) {
        return -MEM_FRAG_DIAG_ENOTSUP;
    }

#if MEM_FRAG_DIAG_HAVE_MALLINFO2
    {
        struct mallinfo2 mi;

        mem_frag_diag_raw_clear(out);
        mi = mallinfo2();
        mem_frag_diag_fill_raw_from_mallinfo2(out, &mi);
    }
    return 0;
#else
    rc = mem_frag_diag_collect_from_malloc_info(out);
    return rc;
#endif
}

void
mem_frag_diag_compute_metrics(const mem_frag_raw_stats_t *raw,
                              mem_frag_metrics_t *out)
{
    mem_frag_severity_t severity;

    if (raw == NULL || out == NULL) {
        return;
    }

    out->frag_ratio_pct   = 0.0;
    out->utilization_pct  = 0.0;
    out->fastbin_hold_pct = 0.0;
    out->trim_ratio_pct   = 0.0;
    out->severity         = MEM_FRAG_SEVERITY_UNKNOWN;

    if (raw->arena > 0U) {
        if (raw->fordblks > raw->keepcost) {
            out->frag_ratio_pct =
                (double)(raw->fordblks - raw->keepcost) / (double)raw->arena *
                100.0;
        }
        out->utilization_pct =
            (double)raw->uordblks / (double)raw->arena * 100.0;
    }

    if (raw->fordblks > 0U) {
        out->fastbin_hold_pct =
            (double)raw->fsmblks / (double)raw->fordblks * 100.0;
        if (raw->keepcost > 0U || raw->from_malloc_info == 0) {
            out->trim_ratio_pct =
                (double)raw->keepcost / (double)raw->fordblks * 100.0;
        }
    }

    if (raw->fordblks == 0U) {
        severity = MEM_FRAG_SEVERITY_HEALTHY;
    } else if (raw->from_malloc_info != 0 && raw->keepcost == 0U) {
        if (raw->ordblks > 100U && out->frag_ratio_pct > 20.0) {
            severity = MEM_FRAG_SEVERITY_CRITICAL;
        } else if (raw->ordblks > 20U && out->frag_ratio_pct > 10.0) {
            severity = MEM_FRAG_SEVERITY_WARNING;
        } else {
            severity = MEM_FRAG_SEVERITY_UNKNOWN;
        }
    } else if (out->trim_ratio_pct >= 60.0) {
        severity = MEM_FRAG_SEVERITY_HEALTHY;
    } else if (out->trim_ratio_pct >= 30.0) {
        severity = MEM_FRAG_SEVERITY_WARNING;
    } else {
        severity = MEM_FRAG_SEVERITY_CRITICAL;
    }

    out->severity = severity;
}

int
mem_frag_diag_snapshot(mem_frag_snapshot_t *out)
{
    int rc;

    if (out == NULL) {
        return mem_frag_diag_neg_errno(EINVAL);
    }

    memset(out, 0, sizeof(*out));

    rc = mem_frag_diag_collect_raw(&out->raw);
    if (rc != 0) {
        return rc;
    }

    out->supported = 1;
    mem_frag_diag_compute_metrics(&out->raw, &out->metrics);
    return 0;
}

const char *
mem_frag_diag_severity_str(mem_frag_severity_t severity)
{
    switch (severity) {
    case MEM_FRAG_SEVERITY_HEALTHY:
        return "healthy";
    case MEM_FRAG_SEVERITY_WARNING:
        return "warning";
    case MEM_FRAG_SEVERITY_CRITICAL:
        return "critical";
    default:
        return "unknown";
    }
}

static int
mem_frag_diag_fprintf_kb(FILE *fp, const char *label, size_t bytes)
{
    int n;

    n = fprintf(fp, "%-22s %8zu KB (%zu B)\n", label, bytes / 1024U, bytes);
    if (n < 0) {
        return mem_frag_diag_neg_errno(errno);
    }
    return 0;
}

int
mem_frag_diag_print_report(FILE *fp, const mem_frag_snapshot_t *snap)
{
    const mem_frag_raw_stats_t *raw;
    const mem_frag_metrics_t *m;
    int rc;

    if (fp == NULL || snap == NULL) {
        return mem_frag_diag_neg_errno(EINVAL);
    }

    raw = &snap->raw;
    m   = &snap->metrics;

    if (fprintf(fp, "=== glibc heap fragmentation report ===\n") < 0) {
        return mem_frag_diag_neg_errno(errno);
    }

    if (raw->from_malloc_info != 0) {
        if (fprintf(fp,
                    "note: stats parsed from malloc_info XML (all arenas); "
                    "keepcost unavailable on this glibc version\n") < 0) {
            return mem_frag_diag_neg_errno(errno);
        }
    } else if (fprintf(fp,
                       "note: mallinfo2 covers main arena only; "
                       "use malloc_info for all arenas\n")
               < 0) {
        return mem_frag_diag_neg_errno(errno);
    }

    rc = mem_frag_diag_fprintf_kb(fp, "arena (heap total):", raw->arena);
    if (rc != 0) {
        return rc;
    }
    rc = mem_frag_diag_fprintf_kb(fp, "in-use (uordblks):", raw->uordblks);
    if (rc != 0) {
        return rc;
    }
    rc = mem_frag_diag_fprintf_kb(fp, "free (fordblks):", raw->fordblks);
    if (rc != 0) {
        return rc;
    }
    rc = mem_frag_diag_fprintf_kb(fp, "trim-able (keepcost):", raw->keepcost);
    if (rc != 0) {
        return rc;
    }
    rc = mem_frag_diag_fprintf_kb(fp, "fastbin bytes (fsmblks):", raw->fsmblks);
    if (rc != 0) {
        return rc;
    }
    rc = mem_frag_diag_fprintf_kb(fp, "mmap bytes (hblkhd):", raw->hblkhd);
    if (rc != 0) {
        return rc;
    }

    if (fprintf(fp, "%-22s %8zu\n", "free chunks (ordblks):", raw->ordblks) < 0 ||
        fprintf(fp, "%-22s %8zu\n", "fastbin chunks (smblks):", raw->smblks) < 0 ||
        fprintf(fp, "%-22s %8zu\n", "mmap regions (hblks):", raw->hblks) < 0) {
        return mem_frag_diag_neg_errno(errno);
    }

    if (fprintf(fp, "--- metrics ---\n") < 0) {
        return mem_frag_diag_neg_errno(errno);
    }
    if (fprintf(fp, "fragmentation ratio:   %6.1f%%  (fordblks-keepcost)/arena\n",
                m->frag_ratio_pct) < 0 ||
        fprintf(fp, "arena utilization:     %6.1f%%  uordblks/arena\n",
                m->utilization_pct) < 0 ||
        fprintf(fp, "fastbin hold ratio:    %6.1f%%  fsmblks/fordblks\n",
                m->fastbin_hold_pct) < 0 ||
        fprintf(fp, "trim ratio:            %6.1f%%  keepcost/fordblks\n",
                m->trim_ratio_pct) < 0 ||
        fprintf(fp, "severity:              %s\n",
                mem_frag_diag_severity_str(m->severity)) < 0) {
        return mem_frag_diag_neg_errno(errno);
    }

    if (m->severity == MEM_FRAG_SEVERITY_CRITICAL) {
        if (fprintf(fp,
                    "action: severe fragmentation (trim_ratio < 30%%); "
                    "consider malloc_info, M_MXFAST=0, malloc_trim\n") < 0) {
            return mem_frag_diag_neg_errno(errno);
        }
    } else if (m->severity == MEM_FRAG_SEVERITY_WARNING) {
        if (fprintf(fp,
                    "action: moderate fragmentation; monitor trend and inspect malloc_info XML\n")
            < 0) {
            return mem_frag_diag_neg_errno(errno);
        }
    } else if (m->fastbin_hold_pct > 40.0) {
        if (fprintf(fp,
                    "action: fastbin holds >40%% of free memory; try mallopt(M_MXFAST, 0)\n")
            < 0) {
            return mem_frag_diag_neg_errno(errno);
        }
    }

    return 0;
}

void
mem_frag_diag_malloc_stats_stderr(void)
{
#if MEM_FRAG_DIAG_HAVE_GLIBC
    malloc_stats();
#else
    fprintf(stderr, "mem_frag_diag: malloc_stats unavailable (not glibc)\n");
#endif
}

int
mem_frag_diag_dump_malloc_info(FILE *fp)
{
    if (fp == NULL) {
        return mem_frag_diag_neg_errno(EINVAL);
    }

#if MEM_FRAG_DIAG_HAVE_GLIBC
    if (malloc_info(0, fp) != 0) {
        return mem_frag_diag_neg_errno(errno);
    }
    return 0;
#else
    (void)fp;
    return -MEM_FRAG_DIAG_ENOTSUP;
#endif
}

int
mem_frag_diag_dump_malloc_info_buf(char **buf, size_t *len)
{
    FILE *fp;
    int rc;

    if (buf == NULL || len == NULL) {
        return mem_frag_diag_neg_errno(EINVAL);
    }

    *buf = NULL;
    *len = 0;

#if !MEM_FRAG_DIAG_HAVE_GLIBC
    return -MEM_FRAG_DIAG_ENOTSUP;
#else
    fp = open_memstream(buf, len);
    if (fp == NULL) {
        return mem_frag_diag_neg_errno(errno);
    }

    rc = mem_frag_diag_dump_malloc_info(fp);
    if (fclose(fp) != 0 && rc == 0) {
        rc = mem_frag_diag_neg_errno(errno);
    }
    if (rc != 0) {
        free(*buf);
        *buf = NULL;
        *len = 0;
    }
    return rc;
#endif
}

int
mem_frag_diag_compact_heap(size_t trim_pad,
                           mem_frag_compact_result_t *out)
{
    mem_frag_compact_result_t local;
    int mxfast_ok;
    int trim_ret;

#if !MEM_FRAG_DIAG_HAVE_GLIBC
    (void)trim_pad;
    (void)out;
    return -MEM_FRAG_DIAG_ENOTSUP;
#else
    local.trim_released   = 0;
    local.mxfast_disabled = 0;

    mxfast_ok = mallopt(M_MXFAST, 0);
    if (mxfast_ok != 0) {
        local.mxfast_disabled = 1;
    }

    trim_ret = malloc_trim(trim_pad);
    local.trim_released = (trim_ret != 0) ? 1 : 0;

    if (out != NULL) {
        *out = local;
    }
    return 0;
#endif
}

int
mem_frag_diag_apply_tune(const mem_frag_tune_opts_t *opts)
{
    int failures;

    if (opts == NULL) {
        return mem_frag_diag_neg_errno(EINVAL);
    }

#if !MEM_FRAG_DIAG_HAVE_GLIBC
    (void)opts;
    return -MEM_FRAG_DIAG_ENOTSUP;
#else
    failures = 0;

    if (opts->apply_mxfast != 0 && mallopt(M_MXFAST, opts->mxfast) == 0) {
        failures++;
    }
    if (opts->apply_trim_threshold != 0 &&
        mallopt(M_TRIM_THRESHOLD, opts->trim_threshold) == 0) {
        failures++;
    }
    if (opts->apply_mmap_threshold != 0 &&
        mallopt(M_MMAP_THRESHOLD, opts->mmap_threshold) == 0) {
        failures++;
    }
    if (opts->apply_arena_max != 0 &&
        mallopt(M_ARENA_MAX, opts->arena_max) == 0) {
        failures++;
    }

    return failures;
#endif
}

static void
mem_frag_diag_sigusr1_handler(int signo)
{
    FILE *fp;

    (void)signo;

#if MEM_FRAG_DIAG_HAVE_GLIBC
    if (g_sigusr1_dump_path != NULL) {
        fp = fopen(g_sigusr1_dump_path, "w");
        if (fp != NULL) {
            (void)malloc_info(0, fp);
            (void)fclose(fp);
        }
    } else {
        (void)malloc_info(0, stderr);
    }
    malloc_stats();
#endif
}

int
mem_frag_diag_install_sigusr1(const char *dump_path)
{
#if !MEM_FRAG_DIAG_HAVE_GLIBC
    (void)dump_path;
    return -MEM_FRAG_DIAG_ENOTSUP;
#else
    struct sigaction sa;

    g_sigusr1_dump_path = dump_path;

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = mem_frag_diag_sigusr1_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;

    if (sigaction(SIGUSR1, &sa, NULL) != 0) {
        return mem_frag_diag_neg_errno(errno);
    }
    return 0;
#endif
}

int64_t
mem_frag_diag_read_heap_rss(void)
{
    FILE *fp;
    char line[512];
    int in_heap;
    int64_t rss_kb;
    int64_t total_rss_kb;

    fp = fopen("/proc/self/smaps", "r");
    if (fp == NULL) {
        return -1;
    }

    in_heap = 0;
    total_rss_kb = 0;

    while (fgets(line, (int)sizeof(line), fp) != NULL) {
        if (strstr(line, "[heap]") != NULL) {
            in_heap = 1;
            continue;
        }

        if (in_heap && strncmp(line, "Rss:", 4) == 0) {
            if (sscanf(line + 4, "%" SCNd64, &rss_kb) == 1) {
                total_rss_kb += rss_kb;
            }
            continue;
        }

        if ((line[0] >= '0' && line[0] <= '9') || line[0] == '\0') {
            in_heap = 0;
        }
    }

    if (fclose(fp) != 0) {
        errno = EIO;
        return -1;
    }

    return total_rss_kb * 1024;
}

int
mem_frag_diag_run(FILE *report_fp, unsigned flags)
{
    mem_frag_snapshot_t snap;
    int rc;

    if (!MEM_FRAG_DIAG_HAVE_GLIBC) {
        return -MEM_FRAG_DIAG_ENOTSUP;
    }

    if ((flags & MEM_FRAG_DIAG_F_COMPACT_FIRST) != 0U) {
        rc = mem_frag_diag_compact_heap(0, NULL);
        if (rc != 0) {
            return rc;
        }
    }

    if ((flags & MEM_FRAG_DIAG_F_STATS_STDERR) != 0U) {
        mem_frag_diag_malloc_stats_stderr();
    }

    rc = mem_frag_diag_snapshot(&snap);
    if (rc != 0) {
        return rc;
    }

    if (report_fp != NULL) {
        rc = mem_frag_diag_print_report(report_fp, &snap);
        if (rc != 0) {
            return rc;
        }
    }

    if ((flags & MEM_FRAG_DIAG_F_DUMP_XML) != 0U && report_fp != NULL) {
        if (fprintf(report_fp, "\n--- malloc_info XML ---\n") < 0) {
            return mem_frag_diag_neg_errno(errno);
        }
        rc = mem_frag_diag_dump_malloc_info(report_fp);
        if (rc != 0) {
            return rc;
        }
    }

    return 0;
}
