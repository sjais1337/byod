#include "dbutils.h"

int is_active(uint16_t *bitmap, int index) {
  return (bitmap[index / 16] >> (index % 16)) & 1;
}

void mark_active(uint16_t *bitmap, int index) {
  bitmap[index / 16] |= (1 << (index % 16));
}

void mark_inactive(uint16_t *bitmap, int index) {
  bitmap[index / 16] &= ~(1 << (index % 16));
}
