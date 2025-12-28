TARGET_EXEC := calc
BUILD_DIR := ./build
SRC_DIR := ./src

SRCS := $(wildcard $(SRC_DIR)/*.c)
OBJS := $(SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
DEPS := $(OBJS:.o=.d)

INC_DIRS := $(SRC_DIR)
INC_FLAGS := $(addprefix -I,$(INC_DIRS))

CC = gcc
GIT_VERSION := $(shell git describe --tags --always --dirty 2>/dev/null || echo "unknown")
CPPFLAGS := $(INC_FLAGS) -MMD
CFLAGS = -Wall -Wextra -std=c11 -DVERSION=\"$(GIT_VERSION)\"
LDLIBS = -lm

# Default target
all: $(BUILD_DIR)/$(TARGET_EXEC) ## Build the project

$(BUILD_DIR)/$(TARGET_EXEC): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS) $(LDLIBS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

run: $(BUILD_DIR)/$(TARGET_EXEC) ## Build and run the calculator
	$(BUILD_DIR)/$(TARGET_EXEC)

clean: ## Scan and remove build artifacts
	rm -rf $(BUILD_DIR)

# Install
PREFIX ?= /usr/local
BIN_DIR := $(PREFIX)/bin

install: $(BUILD_DIR)/$(TARGET_EXEC) ## Install the binary to PREFIX/bin (default /usr/local/bin)
	install -d $(BIN_DIR)
	install $(BUILD_DIR)/$(TARGET_EXEC) $(BIN_DIR)/$(TARGET_EXEC)

uninstall: ## Remove the binary from PREFIX/bin
	rm -f $(BIN_DIR)/$(TARGET_EXEC)

help: ## Show this help message
	@echo "Usage: make [target]"
	@echo ""
	@echo "Targets:"
	@cat $(MAKEFILE_LIST) | grep -E '^[a-zA-Z0-9_-]+:.*?## .*$$' | awk 'BEGIN {FS = ":.*?## "}; {printf "  %-20s %s\n", $$1, $$2}'

.PHONY: all clean run install uninstall help

-include $(DEPS)
