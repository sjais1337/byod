#ifndef BTREE_H
#define BTREE_H

#include "buffer.h"
#include "dbutils.h"
#include <stdint.h>
#include <stdlib.h>

typedef struct {
  int page_id;
  int slot_id;
} RecordID;

typedef enum NodeType { NODE_INTERNAL, NODE_LEAF } NodeType;

#define BTREE_PAGE_SIZE 4096
#define INVALID_PAGE_ID -1

typedef struct {
  uint8_t type;
  uint8_t is_root;
  uint16_t num_keys;
  int32_t parent_page_id;
} BTreeNodeHeader;

#define BTREE_HEADER_SIZE sizeof(BTreeNodeHeader)
#define LEAF_NODE_KEY_SIZE sizeof(int64_t)
#define LEAF_NODE_VALUE_SIZE sizeof(RecordID)
#define LEAF_NODE_CELL_SIZE (LEAF_NODE_KEY_SIZE + LEAF_NODE_VALUE_SIZE)
#define LEAF_NODE_SPACE_FOR_CELLS (BTREE_PAGE_SIZE - BTREE_HEADER_SIZE)
#define LEAF_NODE_MAX_CELLS (LEAF_NODE_SPACE_FOR_CELLS / LEAF_NODE_CELL_SIZE)

#define INTERNAL_KEY_SIZE sizeof(int64_t)
#define INTERNAL_CHILD_SIZE sizeof(int32_t)
#define INTERNAL_CELL_SIZE (INTERNAL_KEY_SIZE + INTERNAL_CHILD_SIZE)
#define INTERNAL_MAX_CELLS                                                     \
  ((BTREE_PAGE_SIZE - BTREE_HEADER_SIZE - INTERNAL_CHILD_SIZE) /               \
   INTERNAL_CELL_SIZE)
BTreeNodeHeader *get_header(void *node);
void initialize_leaf_node(void *node);
void btree_init(BufferPool *pool, int root_page_id);
RecordID btree_find(BufferPool *pool, int32_t root_page_id, int64_t key);
int btree_insert(BufferPool *pool, int *root_page_id, int64_t key,
                 RecordID value);

typedef void (*ScanCallback)(int64_t key, RecordID value, void *ctx);
void btree_scan(BufferPool *pool, int root_page_id, ScanCallback callback,
                void *ctx);
int btree_delete(BufferPool *pool, int *root_page_id, int64_t key);
void print_btree(BufferPool *pool, int page_id, int level);

#endif
