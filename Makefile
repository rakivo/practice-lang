SRC_DIR := src
BUILD_DIR := build
BIN_FILE := pracc
SRC_FILES := $(wildcard $(SRC_DIR)/*)
OPT_LEVEL := -O3
ROOT_FILE := $(SRC_DIR)/main.c
GENERATED_BIN_FILE_NAME := out
COMMON_H := $(SRC_DIR)/common.h
C_FILES := $(filter-out $(ROOT_FILE), $(wildcard $(SRC_DIR)/*.c))
OBJ_FILES = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(C_FILES))

CC := clang

WFLAGS := -Wall -Wextra -Wpedantic -Wswitch-enum -Wno-gnu-zero-variadic-macro-arguments -Wno-gnu-folding-constant -Wno-gnu-empty-struct -Wno-excess-initializers -Wno-unsequenced
CFLAGS := -std=c11 $(OPT_LEVEL) -g

ifeq ($(DEBUG), 1)
	CFLAGS += -DDEBUG
endif

all: $(BUILD_DIR)/$(BIN_FILE)

$(BUILD_DIR):
	mkdir -p $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c $(SRC_FILES) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(WFLAGS) -c $< -o $@

$(BUILD_DIR)/$(BIN_FILE): $(ROOT_FILE) $(OBJ_FILES) $(BUILD_DIR)
	$(CC) -o $@ $(CFLAGS) $(WFLAGS) $(OBJ_FILES) $<

link_with_raylib: $(GENERATED_BIN_FILE_NAME)
$(GENERATED_BIN_FILE_NAME): $(GENERATED_BIN_FILE_NAME).o all $(GENERATED_BIN_FILE_NAME).asm
	ld -o $@ $< -dynamic-linker /lib64/ld-linux-x86-64.so.2 -lraylib -lc -lm
