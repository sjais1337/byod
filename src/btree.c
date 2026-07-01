#include "btree.h"
#include "buffer.h"
#include "printer.h"
#include <stdint.h>

BTreeNodeHeader *get_header(void *node) { return (BTreeNodeHeader *)node; }

int64_t *leaf_node_key(void *node, uint32_t cell_index) {
  return (int64_t *)((uint8_t *)node + BTREE_HEADER_SIZE +
                     (cell_index * LEAF_NODE_KEY_SIZE));
}

RecordID *leaf_node_value(void *node, uint32_t cell_index) {
  return (RecordID *)((uint8_t *)node + BTREE_HEADER_SIZE +
                        (LEAF_NODE_MAX_CELLS * LEAF_NODE_KEY_SIZE) +
                        (cell_index * LEAF_NODE_VALUE_SIZE));
}

int64_t *internal_node_key(void *node, uint32_t cell_index) {
  return (int64_t *)((char *)node + BTREE_HEADER_SIZE +
                     (cell_index * INTERNAL_KEY_SIZE));
}

int32_t *internal_node_child(void *node, uint32_t child_index) {
  return (int32_t *)((char *)node + BTREE_HEADER_SIZE +
                     (INTERNAL_MAX_CELLS * INTERNAL_KEY_SIZE) +
                     (child_index * INTERNAL_CHILD_SIZE));
}

void initialize_leaf_node(void *node) {
  BTreeNodeHeader *header = get_header(node);
  header->type = NODE_LEAF;
  header->is_root = 0;
  header->num_keys = 0;
  header->parent_page_id = -1;
}

void initialize_internal_node(void *node) {
  BTreeNodeHeader *header = get_header(node);
  header->type = NODE_INTERNAL;
  header->is_root = 0;
  header->num_keys = 0;
  header->parent_page_id = INVALID_PAGE_ID;
}

uint32_t search_node(void *node, int64_t key) {
  BTreeNodeHeader *header = get_header(node);
  uint32_t num_keys = header->num_keys;
  uint32_t min_index = 0;
  uint32_t max_index = num_keys;

  while (min_index < max_index) {
    uint32_t mid_index = min_index + (max_index - min_index) / 2;
    int64_t key_at_index = (header->type == NODE_LEAF)
                               ? *leaf_node_key(node, mid_index)
                               : *internal_node_key(node, mid_index);
    if (key == key_at_index) {
      return mid_index;
    }
    if (key < key_at_index) {
      max_index = mid_index;
    } else {
      min_index = mid_index + 1;
    }
  }

  return min_index;
}

static int32_t find_leaf_node(BufferPool *pool, int32_t root_page_id,
                              int64_t key) {
  int32_t current_page_id = root_page_id;

  while (true) {
    void *node = buffer_pool_get_page(pool, current_page_id);
    BTreeNodeHeader *header = get_header(node);

    if (header->type == NODE_LEAF) {
      buffer_pool_unpin_page(pool, current_page_id, false);
      return current_page_id;
    }

    uint32_t child_index = search_node(node, key);
    if (child_index < header->num_keys &&
        *internal_node_key(node, child_index) == key) {
      child_index++;
    }
    int32_t next_page_id = *internal_node_child(node, child_index);

    if (next_page_id == current_page_id) {
      printf(
          "CRITICAL ERROR: Infinite loop detected. Node points to itself.\n");
      exit(1);
    }

    buffer_pool_unpin_page(pool, current_page_id, false);
    current_page_id = next_page_id;
  }
}

void btree_init(BufferPool *pool, int root_page_id) {
  void *root_node = buffer_pool_get_page(pool, root_page_id);
  initialize_leaf_node(root_node);
  BTreeNodeHeader *header = get_header(root_node);
  header->is_root = 1;
  buffer_pool_unpin_page(pool, root_page_id, true);
}

RecordID btree_find(BufferPool *pool, int32_t root_page_id, int64_t key) {
  int32_t leaf_page_id = find_leaf_node(pool, root_page_id, key);
  void *leaf_node = buffer_pool_get_page(pool, leaf_page_id);
  BTreeNodeHeader *header = get_header(leaf_node);
  uint32_t key_index = search_node(leaf_node, key);

  if (key_index < header->num_keys &&
      *leaf_node_key(leaf_node, key_index) == key) {
    RecordID result = *leaf_node_value(leaf_node, key_index);
    buffer_pool_unpin_page(pool, leaf_page_id, false);
    return result;
  }

  buffer_pool_unpin_page(pool, leaf_page_id, false);
  return (RecordID){.page_id = -1, .slot_id = -1};
}

static int split_leaf_node(BufferPool *pool, int32_t old_page_id, void *old_node,
                           int64_t key, RecordID value, int *root_page_id);
static int insert_into_parent(BufferPool *pool, int32_t left_child_id,
                              int64_t key, int32_t right_child_id,
                              int *root_page_id);

int btree_insert(BufferPool *pool, int *root_page_id, int64_t key,
                 RecordID value) {
  int32_t leaf_page_id = find_leaf_node(pool, *root_page_id, key);
  void *leaf_node = buffer_pool_get_page(pool, leaf_page_id);
  BTreeNodeHeader *header = get_header(leaf_node);

  if (header->num_keys >= LEAF_NODE_MAX_CELLS) {
    return split_leaf_node(pool, leaf_page_id, leaf_node, key, value,
                           root_page_id);
  }

  uint32_t insert_index = search_node(leaf_node, key);
  if (insert_index < header->num_keys &&
      *leaf_node_key(leaf_node, insert_index) == key) {
    buffer_pool_unpin_page(pool, leaf_page_id, false);
    return -1;
  }

  for (uint32_t i = header->num_keys; i > insert_index; i--) {
    *leaf_node_key(leaf_node, i) = *leaf_node_key(leaf_node, i - 1);
    *leaf_node_value(leaf_node, i) = *leaf_node_value(leaf_node, i - 1);
  }

  *leaf_node_key(leaf_node, insert_index) = key;
  *leaf_node_value(leaf_node, insert_index) = value;
  header->num_keys += 1;
  buffer_pool_unpin_page(pool, leaf_page_id, true);
  return 0;
}

int btree_delete(BufferPool *pool, int *root_page_id, int64_t key) {
  int32_t leaf_page_id = find_leaf_node(pool, *root_page_id, key);
  void *node = buffer_pool_get_page(pool, leaf_page_id);
  BTreeNodeHeader *header = get_header(node);
  uint32_t key_index = search_node(node, key);

  if (key_index >= header->num_keys ||
      *leaf_node_key(node, key_index) != key) {
    buffer_pool_unpin_page(pool, leaf_page_id, false);
    return -1;
  }

  for (uint32_t i = key_index; i < header->num_keys - 1; i++) {
    *leaf_node_key(node, i) = *leaf_node_key(node, i + 1);
    *leaf_node_value(node, i) = *leaf_node_value(node, i + 1);
  }

  header->num_keys--;
  buffer_pool_unpin_page(pool, leaf_page_id, true);
  return 0;
}

static int split_leaf_node(BufferPool *pool, int32_t old_page_id, void *old_node,
                           int64_t key, RecordID value, int *root_page_id) {
  int32_t new_page_id;
  void *new_node = buffer_pool_new_page(pool, &new_page_id);
  if (new_node == NULL) {
    print_red("Error: Unable to allocate new page for leaf node split.\n");
    return -1;
  }

  initialize_leaf_node(new_node);
  BTreeNodeHeader *old_header = get_header(old_node);
  BTreeNodeHeader *new_header = get_header(new_node);

  int64_t temp_keys[LEAF_NODE_MAX_CELLS + 1];
  RecordID temp_values[LEAF_NODE_MAX_CELLS + 1];
  uint32_t original_num_keys = old_header->num_keys;
  uint32_t source_index = 0;
  uint32_t temp_index = 0;

  while (source_index < original_num_keys &&
         *leaf_node_key(old_node, source_index) < key) {
    temp_keys[temp_index] = *leaf_node_key(old_node, source_index);
    temp_values[temp_index] = *leaf_node_value(old_node, source_index);
    source_index++;
    temp_index++;
  }

  temp_keys[temp_index] = key;
  temp_values[temp_index] = value;
  temp_index++;

  while (source_index < original_num_keys) {
    temp_keys[temp_index] = *leaf_node_key(old_node, source_index);
    temp_values[temp_index] = *leaf_node_value(old_node, source_index);
    source_index++;
    temp_index++;
  }

  uint32_t total_keys = LEAF_NODE_MAX_CELLS + 1;
  uint32_t split_index = total_keys / 2;

  old_header->num_keys = split_index;
  new_header->num_keys = total_keys - split_index;
  new_header->parent_page_id = old_header->parent_page_id;

  for (uint32_t i = 0; i < split_index; i++) {
    *leaf_node_key(old_node, i) = temp_keys[i];
    *leaf_node_value(old_node, i) = temp_values[i];
  }

  for (uint32_t i = 0; i < new_header->num_keys; i++) {
    *leaf_node_key(new_node, i) = temp_keys[split_index + i];
    *leaf_node_value(new_node, i) = temp_values[split_index + i];
  }

  int64_t promoted_key = *leaf_node_key(new_node, 0);
  buffer_pool_unpin_page(pool, new_page_id, true);
  buffer_pool_unpin_page(pool, old_page_id, true);

  return insert_into_parent(pool, old_page_id, promoted_key, new_page_id,
                            root_page_id);
}

static int create_new_root(BufferPool *pool, int32_t left_child_id, int64_t key,
                           int32_t right_child_id, int *root_page_id) {
  int32_t new_root_id;
  void *new_root_node = buffer_pool_new_page(pool, &new_root_id);
  BTreeNodeHeader *root_header = get_header(new_root_node);

  root_header->is_root = 1;
  root_header->num_keys = 1;
  *internal_node_key(new_root_node, 0) = key;
  *internal_node_child(new_root_node, 0) = left_child_id;
  *internal_node_child(new_root_node, 1) = right_child_id;

  void *left_child = buffer_pool_get_page(pool, left_child_id);
  get_header(left_child)->parent_page_id = new_root_id;
  get_header(left_child)->is_root = 0;
  buffer_pool_unpin_page(pool, left_child_id, true);

  void *right_child = buffer_pool_get_page(pool, right_child_id);
  get_header(right_child)->parent_page_id = new_root_id;
  get_header(right_child)->is_root = 0;
  buffer_pool_unpin_page(pool, right_child_id, true);

  *root_page_id = new_root_id;
  buffer_pool_unpin_page(pool, new_root_id, true);
  return 0;
}

static int split_internal_node(BufferPool *pool, int32_t old_page_id,
                               void *old_node, int32_t insert_index,
                               int64_t new_key, int32_t right_child_id,
                               int *root_page_id);
static int insert_into_internal(BufferPool *pool, int32_t parent_id, int64_t key,
                                int32_t left_child_id, int32_t right_child_id,
                                int *root_page_id);

static int insert_into_parent(BufferPool *pool, int32_t left_child_id,
                              int64_t key, int32_t right_child_id,
                              int *root_page_id) {
  void *left_node = buffer_pool_get_page(pool, left_child_id);
  BTreeNodeHeader *left_header = get_header(left_node);
  int32_t parent_id = left_header->parent_page_id;
  buffer_pool_unpin_page(pool, left_child_id, false);

  if (left_header->is_root) {
    return create_new_root(pool, left_child_id, key, right_child_id,
                           root_page_id);
  }

  return insert_into_internal(pool, parent_id, key, left_child_id,
                              right_child_id, root_page_id);
}

static int32_t find_left_child_index(void *parent_node, int32_t left_child_id) {
  BTreeNodeHeader *header = get_header(parent_node);

  for (int i = 0; i <= header->num_keys; i++) {
    if (*internal_node_child(parent_node, i) == left_child_id) {
      return i;
    }
  }

  return -1;
}

static int insert_into_internal(BufferPool *pool, int32_t parent_id, int64_t key,
                                int32_t left_child_id, int32_t right_child_id,
                                int *root_page_id) {
  void *parent_node = buffer_pool_get_page(pool, parent_id);
  BTreeNodeHeader *header = get_header(parent_node);
  int32_t left_index = find_left_child_index(parent_node, left_child_id);

  if (header->num_keys >= INTERNAL_MAX_CELLS) {
    return split_internal_node(pool, parent_id, parent_node, left_index, key,
                               right_child_id, root_page_id);
  }

  for (int i = header->num_keys; i > left_index; i--) {
    *internal_node_key(parent_node, i) = *internal_node_key(parent_node, i - 1);
  }

  for (int i = header->num_keys + 1; i > left_index; i--) {
    *internal_node_child(parent_node, i) =
        *internal_node_child(parent_node, i - 1);
  }

  *internal_node_key(parent_node, left_index) = key;
  *internal_node_child(parent_node, left_index + 1) = right_child_id;
  header->num_keys++;
  buffer_pool_unpin_page(pool, parent_id, true);
  return 0;
}

static int split_internal_node(BufferPool *pool, int32_t old_page_id,
                               void *old_node, int32_t insert_index,
                               int64_t new_key, int32_t right_child_id,
                               int *root_page_id) {
  int64_t temp_keys[INTERNAL_MAX_CELLS + 1];
  int32_t temp_children[INTERNAL_MAX_CELLS + 2];
  BTreeNodeHeader *old_header = get_header(old_node);
  int32_t num_existing_keys = old_header->num_keys;

  for (int i = 0, j = 0; i < num_existing_keys; i++, j++) {
    if (j == insert_index) {
      j++;
    }
    temp_keys[j] = *internal_node_key(old_node, i);
  }

  for (int i = 0, j = 0; i < num_existing_keys; i++, j++) {
    if (j == insert_index + 1) {
      j++;
    }
    temp_children[j] = *internal_node_child(old_node, i);
  }

  temp_keys[insert_index] = new_key;
  temp_children[insert_index + 1] = right_child_id;

  int32_t new_page_id;
  void *new_node = buffer_pool_new_page(pool, &new_page_id);
  initialize_internal_node(new_node);
  BTreeNodeHeader *new_header = get_header(new_node);

  int32_t mid_index = (INTERNAL_MAX_CELLS + 1) / 2;
  int64_t key_to_parent = temp_keys[mid_index];

  old_header->num_keys = mid_index;
  for (int i = 0; i < old_header->num_keys; i++) {
    *internal_node_key(old_node, i) = temp_keys[i];
    *internal_node_child(old_node, i) = temp_children[i];
  }
  *internal_node_child(old_node, old_header->num_keys) = temp_children[mid_index];

  new_header->num_keys = INTERNAL_MAX_CELLS - mid_index;
  for (int i = 0; i < new_header->num_keys; i++) {
    *internal_node_key(new_node, i) = temp_keys[mid_index + 1 + i];
    *internal_node_child(new_node, i) = temp_children[mid_index + 1 + i];
  }
  *internal_node_child(new_node, new_header->num_keys) =
      temp_children[INTERNAL_MAX_CELLS + 1];

  for (int i = 0; i <= new_header->num_keys; i++) {
    int32_t child_id = *internal_node_child(new_node, i);
    void *child_node = buffer_pool_get_page(pool, child_id);
    get_header(child_node)->parent_page_id = new_page_id;
    buffer_pool_unpin_page(pool, child_id, true);
  }

  new_header->parent_page_id = old_header->parent_page_id;
  buffer_pool_unpin_page(pool, new_page_id, true);
  buffer_pool_unpin_page(pool, old_page_id, true);

  return insert_into_parent(pool, old_page_id, key_to_parent, new_page_id,
                            root_page_id);
}

static void btree_scan_node(BufferPool *pool, int32_t page_id,
                            ScanCallback callback, void *ctx) {
  void *node = buffer_pool_get_page(pool, page_id);
  BTreeNodeHeader *header = get_header(node);

  if (header->type == NODE_LEAF) {
    for (int i = 0; i < header->num_keys; i++) {
      callback(*leaf_node_key(node, i), *leaf_node_value(node, i), ctx);
    }
  } else {
    for (int i = 0; i <= header->num_keys; i++) {
      btree_scan_node(pool, *internal_node_child(node, i), callback, ctx);
    }
  }

  buffer_pool_unpin_page(pool, page_id, false);
}

void btree_scan(BufferPool *pool, int root_page_id, ScanCallback callback,
                void *ctx) {
  btree_scan_node(pool, root_page_id, callback, ctx);
}

void print_btree(BufferPool *pool, int page_id, int level) {
  void *node = buffer_pool_get_page(pool, page_id);
  BTreeNodeHeader *header = get_header(node);

  for (int i = 0; i < level; i++) {
    printf("  ");
  }

  if (header->type == NODE_LEAF) {
    printf("LEAF page=%d keys=%u:", page_id, header->num_keys);
    for (uint32_t i = 0; i < header->num_keys; i++) {
      printf(" %lld", (long long)*leaf_node_key(node, i));
    }
    printf("\n");
  } else {
    printf("INTERNAL page=%d keys=%u\n", page_id, header->num_keys);
    for (int i = 0; i <= header->num_keys; i++) {
      print_btree(pool, *internal_node_child(node, i), level + 1);
    }
  }

  buffer_pool_unpin_page(pool, page_id, false);
}
