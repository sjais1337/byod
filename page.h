#ifndef PAGE_H
#define PAGE_H

#include "dbutils.h"
#include <stdlib.h>

Page* create_page();
void free_page(Page* page);


#endif