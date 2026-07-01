#ifndef PAGER_H
#define PAGER_H

#include "dbutils.h"
#include <stdint.h>
#include <stdio.h>
#define MAX_PAGES 100

typedef struct {
  int file_descriptor;
  uint32_t file_length;
  uint32_t num_pages;
} Pager;

Pager *pager_open(const char *filename);
void pager_close(Pager *pager);
void pager_read(Pager *pager, uint32_t page_num, void *dest);
void pager_write(Pager *pager, uint32_t page_num, const void *src);
int pager_get_num_pages(Pager *pager);
#endif
