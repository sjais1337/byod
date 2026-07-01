#include "dbutils.h"
#include "input.h"
#include "btree.h"
#include "buffer.h"
#include "pager.h"
#include "printer.h"
#include "sql.h"
#include "table.h"
#include <string.h>

static void print_help(void) {
  print_blue("Supported commands:\n");
  print_blue("SQL:\n");
  print_blue("  INSERT INTO users VALUES (id, 'username', 'email');\n");
  print_blue("  SELECT * FROM users WHERE id = <id>;\n");
  print_blue("  DELETE FROM users WHERE id = <id>;\n");
  print_blue("Meta:\n");
  print_blue("  .selectall - Display all rows in the table.\n");
  print_blue("  .printtree - Print the B+ tree structure.\n");
  print_blue("  .stats - Show database statistics.\n");
  print_blue("  .help - Show this help message.\n");
  print_blue("  .exit - Exit the database system.\n");
}

static void print_stats(Table *table) {
  printf("Database statistics\n");
  printf("  Rows: %d\n", table_count_rows(table));
  printf("  Pages: %d\n", pager_get_num_pages(table->pool->pager));
  printf("  Page size: %d bytes\n", PAGE_SIZE);
  printf("  Rows per data page: %zu\n", (size_t)ROWS_PER_PAGE);
  printf("  Buffer pool frames: %d\n", POOL_SIZE);
  printf("  B+ tree leaf capacity: %zu keys/page\n",
         (size_t)LEAF_NODE_MAX_CELLS);
  printf("  Index root page: %d\n", table->index_root_page_id);
  printf("  Current data page: %d\n", table->current_data_page_id);
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    printf("Must supply a database filename.\n");
    exit(EXIT_FAILURE);
  }

  InputBuffer *input_buffer = new_input_buffer();
  Table *table = db_open(argv[1]);

  while (true) {
    print_prompt();
    read_input(input_buffer);

    if (strcmp(input_buffer->buffer, ".exit") == 0) {
      close_input_buffer(input_buffer);
      db_close(table);
      print_blue("Exiting DB system.");
      exit(EXIT_SUCCESS);
    }

    if (strcmp(input_buffer->buffer, ".help") == 0) {
      print_help();
      continue;
    }

    if (strcmp(input_buffer->buffer, ".selectall") == 0) {
      print_table(table);
      continue;
    }

    if (strcmp(input_buffer->buffer, ".printtree") == 0) {
      print_index_tree(table);
      continue;
    }

    if (strcmp(input_buffer->buffer, ".stats") == 0) {
      print_stats(table);
      continue;
    }

    SqlStatus status = sql_execute(table, input_buffer->buffer);
    if (status == SQL_OK) {
      continue;
    }

    if (status == SQL_SYNTAX_ERROR) {
      print_red("SQL syntax error.\n");
      continue;
    }

    if (status == SQL_EXEC_ERROR) {
      continue;
    }

    print_red("Unrecognized command. Type .help for supported commands.\n");
  }
}
