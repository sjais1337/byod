#include "table.h"
#include "btree.h"
#include "buffer.h"
#include "wal.h"
#include <stdlib.h>

static void print_row_callback(int64_t key, RecordID record_id, void *ctx);
static int delete_row_internal(Table *table, int id);

Table *db_open(const char *filename) {
  Table *table = malloc(sizeof(Table));
  strncpy(table->db_path, filename, MAX_DB_PATH_LENGTH - 1);
  table->db_path[MAX_DB_PATH_LENGTH - 1] = '\0';
  table->pool = buffer_pool_init(filename);
  table->wal = wal_open(filename);

  if (pager_get_num_pages(table->pool->pager) == 0) {
    int root_page_id;
    void *root_node = buffer_pool_new_page(table->pool, &root_page_id);
    if (root_node == NULL) {
      printf("Error: Could not allocate root page.\n");
      exit(1);
    }

    initialize_leaf_node(root_node);
    BTreeNodeHeader *header = get_header(root_node);
    header->is_root = 1;
    buffer_pool_unpin_page(table->pool, root_page_id, true);
    table->index_root_page_id = root_page_id;

    int data_page_id;
    void *data_page = buffer_pool_new_page(table->pool, &data_page_id);
    memset(data_page, 0, PAGE_SIZE);
    buffer_pool_unpin_page(table->pool, data_page_id, true);
    table->current_data_page_id = data_page_id;
  } else {
    table->index_root_page_id = 0;
    int total_pages = pager_get_num_pages(table->pool->pager);
    table->current_data_page_id = total_pages - 1;
  }

  if (table->wal != NULL && wal_replay(table) != 0) {
    printf("Error replaying WAL.\n");
    exit(1);
  }

  return table;
}

void db_close(Table *table) {
  buffer_pool_close(table->pool);
  wal_checkpoint(table->wal);
  wal_close(table->wal);
  free(table);
}

int validate_table_insert_input(int id, const char *username,
                                const char *email) {
  if (strlen(username) + 1 > MAX_USERNAME_LENGTH) {
    print_red("Name too long. Insertion Aborted.\n");
    return 0;
  }
  if (strlen(email) + 1 > MAX_EMAIL_LENGTH) {
    print_red("Email too long. Insertion Aborted.\n");
    return 0;
  }
  if (id < 0) {
    print_red("ID cannot be negative. \n");
    return 0;
  }
  return 1;
}

int search_row_by_id(Table *table, int id, Row *out_row) {
  RecordID record_id =
      btree_find(table->pool, table->index_root_page_id, id);

  if (record_id.page_id == INVALID_PAGE_ID) {
    return 0;
  }

  void *page_data = buffer_pool_get_page(table->pool, record_id.page_id);
  Page *page = (Page *)page_data;
  if (!is_active((uint16_t *)page->header.bitmap, record_id.slot_id)) {
    buffer_pool_unpin_page(table->pool, record_id.page_id, false);
    return 0;
  }

  *out_row = page->rows[record_id.slot_id];
  buffer_pool_unpin_page(table->pool, record_id.page_id, false);
  return 1;
}

typedef struct {
  int target_id;
  Row *out_row;
  int found;
  Table *table;
} ScanFindContext;

static void scan_find_callback(int64_t key, RecordID record_id, void *ctx) {
  ScanFindContext *scan_ctx = (ScanFindContext *)ctx;
  if (scan_ctx->found || key != scan_ctx->target_id) {
    return;
  }

  void *page_data =
      buffer_pool_get_page(scan_ctx->table->pool, record_id.page_id);
  Page *page = (Page *)page_data;
  if (!is_active((uint16_t *)page->header.bitmap, record_id.slot_id)) {
    buffer_pool_unpin_page(scan_ctx->table->pool, record_id.page_id, false);
    return;
  }

  *scan_ctx->out_row = page->rows[record_id.slot_id];
  scan_ctx->found = 1;
  buffer_pool_unpin_page(scan_ctx->table->pool, record_id.page_id, false);
}

int table_scan_find_row(Table *table, int id, Row *out_row) {
  ScanFindContext scan_ctx = {
      .target_id = id,
      .out_row = out_row,
      .found = 0,
      .table = table,
  };

  btree_scan(table->pool, table->index_root_page_id, scan_find_callback,
             &scan_ctx);
  return scan_ctx.found;
}

typedef struct {
  int count;
} CountContext;

static void count_rows_callback(int64_t key, RecordID record_id, void *ctx) {
  (void)key;
  (void)record_id;
  CountContext *count_ctx = (CountContext *)ctx;
  count_ctx->count++;
}

int table_count_rows(Table *table) {
  CountContext count_ctx = {0};
  btree_scan(table->pool, table->index_root_page_id, count_rows_callback,
             &count_ctx);
  return count_ctx.count;
}

static int insert_row_internal(Table *table, Row *row) {
  int page_id = table->current_data_page_id;
  void *page_data = buffer_pool_get_page(table->pool, page_id);
  Page *page = (Page *)page_data;

  if (page->header.active_rows >= ROWS_PER_PAGE) {
    buffer_pool_unpin_page(table->pool, page_id, true);
    page_id = pager_get_num_pages(table->pool->pager);
    page_data = buffer_pool_new_page(table->pool, &page_id);
    page = (Page *)page_data;
    memset(page->header.bitmap, 0, BITMAP_SIZE);
    page->header.page_id = page_id;
    page->header.active_rows = 0;
    table->current_data_page_id = page_id;
  }

  int slot_id = 0;
  while (is_active((uint16_t *)page->header.bitmap, slot_id)) {
    slot_id++;
  }

  page->rows[slot_id] = *row;
  mark_active((uint16_t *)page->header.bitmap, slot_id);
  page->header.active_rows++;
  buffer_pool_unpin_page(table->pool, page_id, true);

  RecordID record_id = {page_id, slot_id};
  if (btree_insert(table->pool, &table->index_root_page_id, row->id,
                   record_id) != 0) {
    print_red("Error: B-Tree insertion failed.\n");
    return 0;
  }

  return 1;
}

int table_apply_insert(Table *table, const Row *row) {
  Row existing_row;
  if (search_row_by_id(table, row->id, &existing_row)) {
    return 1;
  }

  Row mutable_row = *row;
  return insert_row_internal(table, &mutable_row);
}

void table_apply_delete(Table *table, int id) {
  Row existing_row;
  if (!search_row_by_id(table, id, &existing_row)) {
    return;
  }

  delete_row_internal(table, id);
}

int insert_row(Table *table, Row *row) {
  Row existing_row;
  if (search_row_by_id(table, row->id, &existing_row)) {
    print_red("Error: Duplicate ID. Insertion failed.\n");
    return 0;
  }

  if (table->wal != NULL && wal_log_insert(table->wal, row) != 0) {
    print_red("Error: WAL write failed.\n");
    return 0;
  }

  return insert_row_internal(table, row);
}

static int delete_row_internal(Table *table, int id) {
  RecordID record_id =
      btree_find(table->pool, table->index_root_page_id, id);
  if (record_id.page_id == INVALID_PAGE_ID) {
    return 0;
  }

  void *page_data = buffer_pool_get_page(table->pool, record_id.page_id);
  Page *page = (Page *)page_data;

  if (!is_active((uint16_t *)page->header.bitmap, record_id.slot_id)) {
    buffer_pool_unpin_page(table->pool, record_id.page_id, false);
    return 0;
  }

  mark_inactive((uint16_t *)page->header.bitmap, record_id.slot_id);
  page->header.active_rows--;
  buffer_pool_unpin_page(table->pool, record_id.page_id, true);

  if (btree_delete(table->pool, &table->index_root_page_id, id) != 0) {
    print_red("Error: B-Tree deletion failed.\n");
    return 0;
  }

  return 1;
}

int delete_row_by_id(Table *table, int id) {
  Row existing_row;
  if (!search_row_by_id(table, id, &existing_row)) {
    return 0;
  }

  if (table->wal != NULL && wal_log_delete(table->wal, id) != 0) {
    print_red("Error: WAL write failed.\n");
    return 0;
  }

  return delete_row_internal(table, id);
}

static void print_row_callback(int64_t key, RecordID record_id, void *ctx) {
  (void)key;
  Table *table = (Table *)ctx;
  void *page_data = buffer_pool_get_page(table->pool, record_id.page_id);
  Page *page = (Page *)page_data;
  Row *row = &page->rows[record_id.slot_id];

  printf("(%d, %s, %s)\n", row->id, row->username, row->email);
  buffer_pool_unpin_page(table->pool, record_id.page_id, false);
}

void print_table(Table *table) {
  btree_scan(table->pool, table->index_root_page_id, print_row_callback,
             table);
}

void print_index_tree(Table *table) {
  print_btree(table->pool, table->index_root_page_id, 0);
}
