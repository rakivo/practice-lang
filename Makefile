SRC_DIR := src
BUILD_DIR := build
INCLUDE_DIR := include
SRC_FILES := $(wildcard $(SRC_DIR)/*)
ROOT_CPP_FILE := $(SRC_DIR)/main.cpp
CPP_FILES := $(filter-out $(ROOT_CPP_FILE), $(wildcard $(SRC_DIR)/*.cpp))

CC := clang++

WARN_FLAGS := -Wall -Wextra -Wpedantic -Wno-c99-designator -Wswitch-enum
INCLUDE_FLAGS := -I./$(INCLUDE_DIR)
CFLAGS := -std=c++11 -O0 -g

all: $(BUILD_DIR)/langc

$(BUILD_DIR):
	mkdir -p $@

$(BUILD_DIR)/langc: $(ROOT_CPP_FILE) $(SRC_FILES) $(BUILD_DIR)
	$(CC) -o $@ $(CFLAGS) $(INCLUDE_FLAGS) $(WARN_FLAGS) $< $(CPP_FILES)
