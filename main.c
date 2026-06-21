#include "dbutils.h"
#include "input.h"
#include "printer.h"
#include "table.h"
#include <string.h>

int main(int argc, char *argv[]) {
  if (argc < 2) {
    printf("Must supply a database filename.\n");
    exit(EXIT_FAILURE);
  }

  InputBuffer *input_buffer = new_input_buffer();
  Table *table = db_open(argv[1]);

  while (true) {
    print_prompt();
    read_input(input_buffer);

    if (strcmp(input_buffer->buffer, ".exit") == 0) {
      close_input_buffer(input_buffer);
      db_close(table);
      print_blue("Exiting DB system.");
      exit(EXIT_SUCCESS);
    } else if (strcmp(input_buffer->buffer, ".insert") == 0) {
      Row row;

      print_yellow("Enter ID: ");
      read_input(input_buffer);
      if (sscanf(input_buffer->buffer, "%d", &row.id) != 1) {
        print_red("Invalid ID format. Please enter a number.\n");
        continue;
      }

      print_yellow("Enter username: ");
      read_input(input_buffer);
      char *username_input = strdup(input_buffer->buffer);

      print_yellow("Enter email: ");
      read_input(input_buffer);
      char *email_input = strdup(input_buffer->buffer);

      if (validate_table_insert_input(row.id, username_input, email_input)) {
        strcpy(row.username, username_input);
        strcpy(row.email, email_input);

        if (insert_row(table, &row)) {
          print_green("Row inserted successfully.\n");
        }
      }

      free(username_input);
      free(email_input);
    } else if (strcmp(input_buffer->buffer, ".delete") == 0) {
      print_yellow("Enter ID to delete: ");
      read_input(input_buffer);

      int delete_id;
      if (sscanf(input_buffer->buffer, "%d", &delete_id) != 1) {
        print_red("Invalid ID format. Please enter a number.\n");
        continue;
      }

      if (delete_row_by_id(table, delete_id)) {
        print_green("Row deleted successfully.\n");
      } else {
        print_red("Row with given ID not found.\n");
      }
    } else if (strcmp(input_buffer->buffer, ".selectall") == 0) {
      print_table(table);
    } else if (strcmp(input_buffer->buffer, ".select") == 0) {
      int select_id;
      int valid_id = 0;

      while (!valid_id) {
        print_yellow("Enter ID to select: ");
        read_input(input_buffer);
        if (sscanf(input_buffer->buffer, "%d", &select_id) == 1) {
          valid_id = 1;
        } else {
          print_red("Invalid ID format. Please enter a number.\n");
        }
      }

      Row selected_row;
      if (search_row_by_id(table, select_id, &selected_row)) {
        print_green("Row found:\n");
        printf("(%d, %s, %s)\n", selected_row.id, selected_row.username,
               selected_row.email);
      } else {
        print_red("Row with given ID not found.\n");
      }
    } else if (strcmp(input_buffer->buffer, ".help") == 0) {
      print_blue("Supported commands:\n");
      print_blue(".insert - Insert a new row into the table.\n");
      print_blue(".delete - Delete a row by ID from the table.\n");
      print_blue(".selectall - Display all rows in the table.\n");
      print_blue(".select - Display a row by ID.\n");
      print_blue(".printtree - Print the B+ tree structure.\n");
      print_blue(".exit - Exit the database system.\n");
    } else if (strcmp(input_buffer->buffer, ".printtree") == 0) {
      printf("\n");
    } else {
      print_red("Unrecognized command.\n");
    }
  }
}
