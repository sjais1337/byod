CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -g -D_GNU_SOURCE -Iinclude

BUILD_DIR = build
BIN_DIR = bin

CORE_SRCS = \
	src/btree.c \
	src/buffer.c \
	src/dbutils.c \
	src/input.c \
	src/page.c \
	src/pager.c \
	src/sql.c \
	src/table.c \
	src/wal.c

CORE_OBJS = $(patsubst src/%.c,$(BUILD_DIR)/%.o,$(CORE_SRCS))
MAIN_OBJ = $(BUILD_DIR)/main.o
BENCH_OBJ = $(BUILD_DIR)/benchmark.o
TEST_OBJ = $(BUILD_DIR)/test_runner.o

BYOD = $(BIN_DIR)/byod
BENCHMARK = $(BIN_DIR)/benchmark
TEST_RUNNER = $(BIN_DIR)/test_runner

all: $(BYOD) $(BENCHMARK) $(TEST_RUNNER)

$(BYOD): $(CORE_OBJS) $(MAIN_OBJ) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^

$(BENCHMARK): $(CORE_OBJS) $(BENCH_OBJ) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^

$(TEST_RUNNER): $(CORE_OBJS) $(TEST_OBJ) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^

$(BUILD_DIR)/%.o: src/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/benchmark.o: bench/benchmark.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/test_runner.o: tests/test_runner.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR) $(BIN_DIR):
	mkdir -p $@

test: $(TEST_RUNNER)
	./$(TEST_RUNNER)

bench: $(BENCHMARK)
	./$(BENCHMARK)

run: $(BYOD)
	./$(BYOD)

clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)

rebuild: clean all

.PHONY: all clean rebuild run test bench
