#ifndef BUFFER_H
#define BUFFER_H

#include "pager.h"
#include <stdbool.h>
#include <stdint.h>

#define POOL_SIZE 100

typedef struct {
  uint8_t data[PAGE_SIZE];
  int page_id;
  int pin_count;
  bool is_dirty;
  bool ref_bit;
} Frame;

typedef struct BufferPool {
  Pager *pager;
  Frame frames[POOL_SIZE];
  int clock_hand;
} BufferPool;

BufferPool *buffer_pool_init(const char *filename);
void *buffer_pool_get_page(BufferPool *pool, int page_id);
void buffer_pool_unpin_page(BufferPool *pool, int page_id, bool is_dirty);
void buffer_pool_flush_all(BufferPool *pool);
void buffer_pool_close(BufferPool *pool);
void *buffer_pool_new_page(BufferPool *pool, int *page_id);

#endif
