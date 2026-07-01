#ifndef DBUTILS_H
#define DBUTILS_H

#include <stdbool.h>
#include <stdint.h>

#define PAGE_SIZE 4096
#define MAX_USERNAME_LENGTH 32
#define MAX_EMAIL_LENGTH 255
#define MAX_DB_PATH_LENGTH 512

typedef struct BufferPool BufferPool;
typedef struct Wal Wal;

typedef struct {
  int id;
  char username[MAX_USERNAME_LENGTH];
  char email[MAX_EMAIL_LENGTH];
} Row;

#define ROW_SIZE sizeof(Row)
#define ROWS_PER_PAGE ((PAGE_SIZE - sizeof(uint8_t) - sizeof(int)) / ROW_SIZE)
#define BITMAP_SIZE ((ROWS_PER_PAGE + 7) / 8)

typedef struct {
  uint8_t active_rows;
  uint8_t bitmap[BITMAP_SIZE];
  int page_id;
} Header;

typedef struct {
  Header header;
  Row rows[ROWS_PER_PAGE];
} Page;

typedef struct {
  BufferPool *pool;
  Wal *wal;
  char db_path[MAX_DB_PATH_LENGTH];
  int index_root_page_id;
  int current_data_page_id;
} Table;

int is_active(uint16_t *bitmap, int index);
void mark_active(uint16_t *bitmap, int index);
void mark_inactive(uint16_t *bitmap, int index);

#endif
