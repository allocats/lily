CC = clang
CF = -std=c23 -Isrc -Wall -O3 -flto -march=native -g -fno-omit-frame-pointer
DF = -DDEBUG_MODE

SRC_DIR = src
BUILD_DIR = build
BIN_DIR = $(BUILD_DIR)/bin

BINARY = $(BIN_DIR)/lilyc

SRCS = $(shell find $(SRC_DIR) -name "*.c")
OBJS = $(SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)

$(BINARY): $(OBJS) | $(BIN_DIR)
	$(CC) $(CF) -o $@ $(OBJS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	mkdir -p $(dir $@)
	$(CC) $(CF) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $@

$(BIN_DIR): | $(BUILD_DIR)
	mkdir -p $@

.PHONY: debug
debug: CF += $(DF)
debug: $(BINARY)

.PHONY: clean
clean:
	rm -rvf $(BUILD_DIR)
	rm -rvf $(BIN_DIR)
