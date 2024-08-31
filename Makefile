SRC_DIR := src
BUILD_DIR := build
INCLUDE_DIR := include
SRC_FILES := $(wildcard $(SRC_DIR)/*)
ROOT_FILE := $(SRC_DIR)/main.c
C_FILES := $(filter-out $(ROOT_FILE), $(wildcard $(SRC_DIR)/*.c))
OBJ_FILES = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(C_FILES))

CC := clang

WARN_FLAGS := -Wall -Wextra -Wpedantic -Wswitch-enum -Wno-gnu-zero-variadic-macro-arguments -Wno-gnu-folding-constant -Wno-gnu-empty-struct -Wno-excess-initializers -Wno-unsequenced
INCLUDE_FLAGS := -I./$(INCLUDE_DIR)
CFLAGS := -std=c11 -O0 -g

all: $(BUILD_DIR)/langc

$(BUILD_DIR):
	mkdir -p $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c $(SRC_FILES) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDE_FLAGS) $(WARN_FLAGS) -c $< -o $@

$(BUILD_DIR)/langc: $(ROOT_FILE) $(OBJ_FILES) $(BUILD_DIR)
	$(CC) -o $@ $(CFLAGS) $(INCLUDE_FLAGS) $(WARN_FLAGS) $(OBJ_FILES) $<
