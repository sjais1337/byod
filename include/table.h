#ifndef TABLE_H
#define TABLE_H

#include "dbutils.h"
#include "page.h"
#include "printer.h"
#include <string.h>

Table *db_open(const char *filename);
void db_close(Table *table);

int validate_table_insert_input(int id, const char *username, const char *email);
int insert_row(Table *table, Row *row);
int delete_row_by_id(Table *table, int id);
int search_row_by_id(Table *table, int id, Row *out_row);
int table_scan_find_row(Table *table, int id, Row *out_row);
int table_count_rows(Table *table);
void print_table(Table *table);
void print_index_tree(Table *table);

int table_apply_insert(Table *table, const Row *row);
void table_apply_delete(Table *table, int id);

#endif
