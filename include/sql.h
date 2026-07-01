#ifndef SQL_H
#define SQL_H

#include "table.h"

typedef enum {
  SQL_OK = 0,
  SQL_UNRECOGNIZED = 1,
  SQL_SYNTAX_ERROR = 2,
  SQL_EXEC_ERROR = 3
} SqlStatus;

SqlStatus sql_execute(Table *table, const char *query);

#endif
