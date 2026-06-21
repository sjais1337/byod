#define _POSIX_C_SOURCE 200809L
#include "input.h"
#include "printer.h"
#include <stdio.h>

InputBuffer *new_input_buffer(void) {
  InputBuffer *input_buffer = malloc(sizeof(InputBuffer));
  input_buffer->buffer = NULL;
  input_buffer->buffer_length = 0;
  input_buffer->input_length = 0;
  return input_buffer;
}

void print_prompt(void) { printf("db > "); }

void read_input(InputBuffer *input_buffer) {
  ssize_t bytes_read =
      getline(&input_buffer->buffer, &input_buffer->buffer_length, stdin);

  if (bytes_read <= 0) {
    print_red("Error reading input.\n");
    exit(EXIT_FAILURE);
  }

  input_buffer->input_length = (size_t)(bytes_read - 1);
  input_buffer->buffer[bytes_read - 1] = '\0';
}

void close_input_buffer(InputBuffer *input_buffer) {
  free(input_buffer->buffer);
  free(input_buffer);
}
