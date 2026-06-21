#include "table.h"
#include "btree.h"
#include "buffer.h"
#include <stdlib.h>

Table *db_open(const char *filename) {
  Table *table = malloc(sizeof(Table));
  table->pool = buffer_pool_init(filename);

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

  return table;
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

int insert_row(Table *table, Row *row) {
  Row existing_row;
  if (search_row_by_id(table, row->id, &existing_row)) {
    print_red("Error: Duplicate ID. Insertion failed.\n");
    return 0;
  }

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

int delete_row_by_id(Table *table, int id) {
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

void db_close(Table *table) {
  buffer_pool_close(table->pool);
  free(table);
}
