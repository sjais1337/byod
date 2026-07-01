#include "sql.h"
#include "table.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int tests_run = 0;
static int tests_failed = 0;

static void expect_true(const char *name, int condition) {
  tests_run++;
  if (condition) {
    printf("[PASS] %s\n", name);
  } else {
    printf("[FAIL] %s\n", name);
    tests_failed++;
  }
}

static void cleanup_paths(const char *db_path) {
  remove(db_path);
  char wal_path[512];
  snprintf(wal_path, sizeof(wal_path), "%s.wal", db_path);
  remove(wal_path);
}

static Row make_row(int id) {
  Row row = {.id = id};
  snprintf(row.username, sizeof(row.username), "user_%d", id);
  snprintf(row.email, sizeof(row.email), "user_%d@test.com", id);
  return row;
}

static void test_insert_select_delete(void) {
  const char *db_path = "/tmp/byod_test_basic.db";
  cleanup_paths(db_path);

  Table *table = db_open(db_path);
  Row row = make_row(42);
  expect_true("insert row", insert_row(table, &row));

  Row found = {0};
  expect_true("indexed select", search_row_by_id(table, 42, &found));
  expect_true("selected id matches", found.id == 42);
  expect_true("selected username matches",
              strcmp(found.username, row.username) == 0);

  expect_true("delete row", delete_row_by_id(table, 42));
  expect_true("row absent after delete", !search_row_by_id(table, 42, &found));
  db_close(table);

  table = db_open(db_path);
  expect_true("row still absent after reopen",
              !search_row_by_id(table, 42, &found));
  db_close(table);
  cleanup_paths(db_path);
}

static void test_wal_replay(void) {
  const char *db_path = "/tmp/byod_test_wal.db";
  cleanup_paths(db_path);

  Table *table = db_open(db_path);
  Row row = make_row(7);
  expect_true("wal insert", insert_row(table, &row));

  db_close(table);

  table = db_open(db_path);
  Row found = {0};
  expect_true("row persisted after clean close",
              search_row_by_id(table, 7, &found));
  db_close(table);

  table = db_open(db_path);
  expect_true("wal replay insert idempotent",
              table_apply_insert(table, &row));
  expect_true("row count remains one after replay", table_count_rows(table) == 1);
  db_close(table);
  cleanup_paths(db_path);
}

static void test_scan_vs_index_consistency(void) {
  const char *db_path = "/tmp/byod_test_scan.db";
  cleanup_paths(db_path);

  Table *table = db_open(db_path);
  for (int i = 0; i < 100; i++) {
    Row row = make_row(i);
    expect_true("bulk insert", insert_row(table, &row));
  }

  Row indexed = {0};
  Row scanned = {0};
  expect_true("indexed lookup", search_row_by_id(table, 55, &indexed));
  expect_true("scan lookup", table_scan_find_row(table, 55, &scanned));
  expect_true("scan/index agree on id", indexed.id == scanned.id);
  expect_true("row count", table_count_rows(table) == 100);
  db_close(table);
  cleanup_paths(db_path);
}

static void test_sql_interface(void) {
  const char *db_path = "/tmp/byod_test_sql.db";
  cleanup_paths(db_path);

  Table *table = db_open(db_path);
  expect_true("sql insert",
              sql_execute(table, "INSERT INTO users VALUES (10, 'alice', "
                                 "'alice@example.com');") == SQL_OK);

  Row found = {0};
  expect_true("row exists after sql insert", search_row_by_id(table, 10, &found));
  expect_true("sql select", sql_execute(table, "SELECT * FROM users WHERE id = 10;") ==
                                SQL_OK);
  expect_true("sql delete",
              sql_execute(table, "DELETE FROM users WHERE id = 10;") == SQL_OK);
  expect_true("row absent after sql delete",
              !search_row_by_id(table, 10, &found));
  db_close(table);
  cleanup_paths(db_path);
}

int main(void) {
  test_insert_select_delete();
  test_wal_replay();
  test_scan_vs_index_consistency();
  test_sql_interface();

  printf("\n%d/%d tests passed.\n", tests_run - tests_failed, tests_run);
  return tests_failed == 0 ? 0 : 1;
}
