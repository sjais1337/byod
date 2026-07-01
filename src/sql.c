#include "sql.h"
#include "printer.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

static void trim(char *text) {
  char *start = text;
  while (*start && isspace((unsigned char)*start)) {
    start++;
  }

  if (start != text) {
    memmove(text, start, strlen(start) + 1);
  }

  size_t length = strlen(text);
  while (length > 0 && isspace((unsigned char)text[length - 1])) {
    text[--length] = '\0';
  }
}

static const char *find_ignore_case(const char *haystack, const char *needle) {
  size_t needle_length = strlen(needle);
  for (const char *cursor = haystack; *cursor; cursor++) {
    if (strncasecmp(cursor, needle, needle_length) == 0) {
      return cursor;
    }
  }
  return NULL;
}

static void uppercase_copy(const char *input, char *output, size_t size) {
  strncpy(output, input, size - 1);
  output[size - 1] = '\0';
  for (char *cursor = output; *cursor; cursor++) {
    *cursor = (char)toupper((unsigned char)*cursor);
  }
}

static SqlStatus execute_insert(Table *table, const char *query) {
  Row row = {0};
  char username[MAX_USERNAME_LENGTH];
  char email[MAX_EMAIL_LENGTH];
  const char *values_clause = find_ignore_case(query, "values");

  if (values_clause == NULL) {
    return SQL_SYNTAX_ERROR;
  }

  const char *open_paren = strchr(values_clause, '(');
  if (open_paren == NULL) {
    return SQL_SYNTAX_ERROR;
  }

  if (sscanf(open_paren, "(%d, '%31[^']', '%255[^']')", &row.id, username,
             email) != 3) {
    return SQL_SYNTAX_ERROR;
  }

  if (!validate_table_insert_input(row.id, username, email)) {
    return SQL_EXEC_ERROR;
  }

  strcpy(row.username, username);
  strcpy(row.email, email);

  if (!insert_row(table, &row)) {
    return SQL_EXEC_ERROR;
  }

  print_green("Row inserted successfully.\n");
  return SQL_OK;
}

static SqlStatus execute_select(Table *table, const char *query) {
  int id = 0;
  const char *where_clause = find_ignore_case(query, "where id =");

  if (where_clause == NULL) {
    return SQL_SYNTAX_ERROR;
  }

  where_clause += strlen("where id =");
  if (sscanf(where_clause, " %d", &id) != 1) {
    return SQL_SYNTAX_ERROR;
  }

  Row row;
  if (!search_row_by_id(table, id, &row)) {
    print_red("Row with given ID not found.\n");
    return SQL_EXEC_ERROR;
  }

  print_green("Row found:\n");
  printf("(%d, %s, %s)\n", row.id, row.username, row.email);
  return SQL_OK;
}

static SqlStatus execute_delete(Table *table, const char *query) {
  int id = 0;
  const char *where_clause = find_ignore_case(query, "where id =");

  if (where_clause == NULL) {
    return SQL_SYNTAX_ERROR;
  }

  where_clause += strlen("where id =");
  if (sscanf(where_clause, " %d", &id) != 1) {
    return SQL_SYNTAX_ERROR;
  }

  if (!delete_row_by_id(table, id)) {
    print_red("Row with given ID not found.\n");
    return SQL_EXEC_ERROR;
  }

  print_green("Row deleted successfully.\n");
  return SQL_OK;
}

SqlStatus sql_execute(Table *table, const char *query) {
  char normalized[512];
  char trimmed[512];

  strncpy(trimmed, query, sizeof(trimmed) - 1);
  trimmed[sizeof(trimmed) - 1] = '\0';
  trim(trimmed);
  uppercase_copy(trimmed, normalized, sizeof(normalized));

  if (strncmp(normalized, "INSERT INTO USERS", 17) == 0) {
    return execute_insert(table, trimmed);
  }

  if (strncmp(normalized, "SELECT * FROM USERS", 19) == 0) {
    return execute_select(table, trimmed);
  }

  if (strncmp(normalized, "DELETE FROM USERS", 17) == 0) {
    return execute_delete(table, trimmed);
  }

  return SQL_UNRECOGNIZED;
}
