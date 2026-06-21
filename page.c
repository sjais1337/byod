#include "page.h"

Page* create_page() {
    Page* page = (Page*) malloc (sizeof(Page));
    return page;
}

void free_page(Page* page) {
    if (page){
        free(page);
    }
}
