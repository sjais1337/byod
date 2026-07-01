#include "buffer.h"
#include "pager.h"
#include "printer.h"
#include <stdlib.h>
#include <string.h>

BufferPool *buffer_pool_init(const char *filename) {
  BufferPool *pool = malloc(sizeof(BufferPool));
  pool->pager = pager_open(filename);
  pool->clock_hand = 0;

  for (int i = 0; i < POOL_SIZE; i++) {
    pool->frames[i].page_id = -1;
    pool->frames[i].pin_count = 0;
    pool->frames[i].is_dirty = false;
    pool->frames[i].ref_bit = false;
  }

  return pool;
}

static int find_frame_id(BufferPool *pool, int page_id) {
  for (int i = 0; i < POOL_SIZE; i++) {
    if (pool->frames[i].page_id == page_id) {
      return i;
    }
  }
  return -1;
}

static int get_victim_frame(BufferPool *pool) {
  while (true) {
    Frame *frame = &pool->frames[pool->clock_hand];

    if (frame->page_id == -1) {
      return pool->clock_hand;
    }

    if (frame->pin_count > 0) {
      pool->clock_hand = (pool->clock_hand + 1) % POOL_SIZE;
      continue;
    }

    if (frame->ref_bit) {
      frame->ref_bit = false;
      pool->clock_hand = (pool->clock_hand + 1) % POOL_SIZE;
      continue;
    }

    return pool->clock_hand;
  }
}

void *buffer_pool_get_page(BufferPool *pool, int page_id) {
  int frame_id = find_frame_id(pool, page_id);
  if (frame_id != -1) {
    pool->frames[frame_id].pin_count++;
    pool->frames[frame_id].ref_bit = true;
    return pool->frames[frame_id].data;
  }

  int victim_id = get_victim_frame(pool);
  Frame *victim = &pool->frames[victim_id];
  if (victim->page_id != -1 && victim->is_dirty) {
    pager_write(pool->pager, victim->page_id, victim->data);
  }

  victim->page_id = page_id;
  victim->pin_count = 1;
  victim->is_dirty = false;
  victim->ref_bit = true;
  pager_read(pool->pager, page_id, victim->data);
  return victim->data;
}

void buffer_pool_unpin_page(BufferPool *pool, int page_id, bool is_dirty) {
  int frame_id = find_frame_id(pool, page_id);

  if (frame_id == -1) {
    print_red("Error: Trying to unpin page which is not in buffer.\n");
    return;
  }
  if (pool->frames[frame_id].pin_count <= 0) {
    print_red("Error: Pin count is already 0.\n");
    return;
  }

  pool->frames[frame_id].pin_count--;
  if (is_dirty) {
    pool->frames[frame_id].is_dirty = true;
  }
}

void buffer_pool_flush_all(BufferPool *pool) {
  for (int i = 0; i < POOL_SIZE; i++) {
    if (pool->frames[i].page_id != -1 && pool->frames[i].is_dirty) {
      pager_write(pool->pager, pool->frames[i].page_id, pool->frames[i].data);
      pool->frames[i].is_dirty = false;
    }
  }
}

void buffer_pool_close(BufferPool *pool) {
  buffer_pool_flush_all(pool);
  pager_close(pool->pager);
  free(pool);
}

void *buffer_pool_new_page(BufferPool *pool, int *page_id) {
  int new_page_id = pager_get_num_pages(pool->pager);
  pool->pager->num_pages++;

  int victim_id = get_victim_frame(pool);
  Frame *victim = &pool->frames[victim_id];

  if (victim->page_id != -1 && victim->is_dirty) {
    pager_write(pool->pager, victim->page_id, victim->data);
  }

  victim->page_id = new_page_id;
  victim->pin_count = 1;
  victim->is_dirty = true;
  victim->ref_bit = true;
  memset(victim->data, 0, PAGE_SIZE);

  *page_id = new_page_id;
  return victim->data;
}
