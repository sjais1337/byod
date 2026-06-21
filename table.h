#ifndef TABLE_H
#define TABLE_H

#include "dbutils.h"
#include "page.h"
#include "printer.h"
#include "string.h"

Table *db_open(const char *filename);

int validate_table_insert_input(int id, const char *username,
                                const char *email);
void print_table(Table *table);
int search_row_by_id(Table *table, int id, Row *out_row);
int insert_row(Table *table, Row *row);
int delete_row_by_id(Table *table, int id);
void db_close(Table *table);
#endif
