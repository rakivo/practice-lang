SRC_DIR := src
BUILD_DIR := build
BIN_FILE := pracc
SRC_FILES := $(wildcard $(SRC_DIR)/*)
ROOT_FILE := $(SRC_DIR)/main.c
COMMON_H := $(SRC_DIR)/common.h
C_FILES := $(filter-out $(ROOT_FILE), $(wildcard $(SRC_DIR)/*.c))
OBJ_FILES = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(C_FILES))

CC := clang

WARN_FLAGS := -Wall -Wextra -Wpedantic -Wswitch-enum -Wno-gnu-zero-variadic-macro-arguments -Wno-gnu-folding-constant -Wno-gnu-empty-struct -Wno-excess-initializers -Wno-unsequenced
CFLAGS := -std=c11 -O0 -g

all: $(BUILD_DIR)/$(BIN_FILE)

$(BUILD_DIR):
	mkdir -p $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c $(SRC_FILES) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(WARN_FLAGS) -c $< -o $@

$(BUILD_DIR)/$(BIN_FILE): $(ROOT_FILE) $(OBJ_FILES) $(BUILD_DIR)
	$(CC) -o $@ $(CFLAGS) $(WARN_FLAGS) $(OBJ_FILES) $<
