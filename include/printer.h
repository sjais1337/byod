#ifndef PRINTER_H
#define PRINTER_H

#include <stdio.h>

#define START_ANSI "\x1B["
#define END_ANSI "\x1B[0m"

static inline void print_red(const char *text) {
  printf(START_ANSI "31m%s" END_ANSI, text);
}

static inline void print_green(const char *text) {
  printf(START_ANSI "32m%s" END_ANSI, text);
}

static inline void print_yellow(const char *text) {
  printf(START_ANSI "33m%s" END_ANSI, text);
}

static inline void print_blue(const char *text) {
  printf(START_ANSI "34m%s" END_ANSI, text);
}

static inline void print_magenta(const char *text) {
  printf(START_ANSI "35m%s" END_ANSI, text);
}

static inline void print_cyan(const char *text) {
  printf(START_ANSI "36m%s" END_ANSI, text);
}

#endif
