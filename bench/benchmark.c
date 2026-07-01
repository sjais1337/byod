#include "table.h"
#include "btree.h"
#include "buffer.h"
#include "pager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static double now_seconds(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec + ts.tv_nsec / 1e9;
}

static void remove_file(const char *path) {
  remove(path);
  char wal_path[512];
  snprintf(wal_path, sizeof(wal_path), "%s.wal", path);
  remove(wal_path);
}

static void seed_rows(Table *table, int count) {
  for (int i = 0; i < count; i++) {
    Row row = {
        .id = i,
    };
    snprintf(row.username, sizeof(row.username), "user_%d", i);
    snprintf(row.email, sizeof(row.email), "user_%d@example.com", i);
    if (!insert_row(table, &row)) {
      fprintf(stderr, "Failed to insert row %d during benchmark seed.\n", i);
      exit(1);
    }
  }
}

static double benchmark_lookup(Table *table, int count, int lookup_id,
                               int (*lookup_fn)(Table *, int, Row *)) {
  Row row;
  double start = now_seconds();

  for (int i = 0; i < count; i++) {
    int target_id = (lookup_id + i) % count;
    if (!lookup_fn(table, target_id, &row)) {
      fprintf(stderr, "Lookup failed for id=%d\n", target_id);
      exit(1);
    }
  }

  return now_seconds() - start;
}

int main(void) {
  const char *db_path = "/tmp/byod_benchmark.db";
  const int row_count = 5000;
  const int lookup_iterations = 5000;
  const int lookup_target = 4242;

  remove_file(db_path);

  Table *table = db_open(db_path);

  double insert_start = now_seconds();
  seed_rows(table, row_count);
  double insert_seconds = now_seconds() - insert_start;

  int indexed_lookups = lookup_iterations;
  double indexed_seconds =
      benchmark_lookup(table, indexed_lookups, lookup_target, search_row_by_id);
  double indexed_avg_us = (indexed_seconds / indexed_lookups) * 1e6;

  int scan_lookups = lookup_iterations;
  double scan_seconds =
      benchmark_lookup(table, scan_lookups, lookup_target, table_scan_find_row);
  double scan_avg_us = (scan_seconds / scan_lookups) * 1e6;

  int rows = table_count_rows(table);
  int pages = pager_get_num_pages(table->pool->pager);
  db_close(table);

  double speedup = scan_avg_us / indexed_avg_us;

  printf("BYOD Benchmark Results\n");
  printf("========================\n");
  printf("Rows inserted: %d\n", rows);
  printf("Database pages: %d\n", pages);
  printf("Insert throughput: %.0f rows/sec\n", row_count / insert_seconds);
  printf("Indexed lookup avg: %.2f us\n", indexed_avg_us);
  printf("Full-table scan lookup avg: %.2f us\n", scan_avg_us);
  printf("Indexed lookup speedup: %.1fx\n", speedup);
  printf("\n");
  printf("Resume snippets:\n");
  printf("- Indexed primary-key lookup averaged %.0f us vs %.0f us full-table scan at %d rows (%.0fx faster).\n",
         indexed_avg_us, scan_avg_us, row_count, speedup);
  printf("- Sustained insert throughput of %.0f rows/sec on a 4 KB paged storage engine with B+ tree indexing and WAL.\n",
         row_count / insert_seconds);
  printf("- Buffer pool caches %d frames (~%.0f KB); B+ tree leaf capacity is %zu keys/page.\n",
         POOL_SIZE, (POOL_SIZE * PAGE_SIZE) / 1024.0,
         (size_t)LEAF_NODE_MAX_CELLS);

  remove_file(db_path);
  return 0;
}
