#define _GNU_SOURCE
#include "mem_frag_diag.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

int
main(void)
{
    mem_frag_snapshot_t snap;
    char *xml;
    size_t xml_len;
    int rc;
    int i;
    void *blocks[500];

    if (!mem_frag_diag_is_supported()) {
        fprintf(stderr, "glibc diagnostics not supported\n");
        return 1;
    }

    for (i = 0; i < 1000; ++i) {
        blocks[i % 500] = malloc(100);

        if (i % 2 == 0) {
            free(blocks[i % 500]);
            blocks[i % 500] = NULL;
        }
    }

    rc = mem_frag_diag_run(stdout,
                           MEM_FRAG_DIAG_F_STATS_STDERR |
                               MEM_FRAG_DIAG_F_DUMP_XML);
    if (rc != 0) {
        fprintf(stderr, "run failed: %d\n", rc);
        return 1;
    }

    rc = mem_frag_diag_snapshot(&snap);
    if (rc == 0) {
        printf("severity=%s frag=%.1f%%\n",
               mem_frag_diag_severity_str(snap.metrics.severity),
               snap.metrics.frag_ratio_pct);
    }

    xml = NULL;
    xml_len = 0;
    rc = mem_frag_diag_dump_malloc_info_buf(&xml, &xml_len);
    if (rc == 0) {
        printf("xml bytes=%zu\n", xml_len);
        free(xml);
    }

    printf("heap rss=%" PRId64 "\n", mem_frag_diag_read_heap_rss());
    return 0;
}
