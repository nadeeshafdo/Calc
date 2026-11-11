TARGET_EXEC := calc
BUILD_DIR := ./build
SRC_DIR := ./src
TP_DIR := ./third_party/manatui

SRCS := $(wildcard $(SRC_DIR)/*.c) $(TP_DIR)/src/manatui.c
OBJS := $(SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
# handle third_party object path
OBJS := $(patsubst $(TP_DIR)/src/%.c,$(BUILD_DIR)/tp_%.o,$(OBJS))
DEPS := $(OBJS:.o=.d)

INC_DIRS := $(SRC_DIR) $(TP_DIR)
INC_FLAGS := $(addprefix -I,$(INC_DIRS))

CC = gcc
GIT_VERSION := $(shell git describe --tags --always --dirty 2>/dev/null || echo "unknown")
CPPFLAGS := $(INC_FLAGS) -MMD
CFLAGS = -Wall -Wextra -std=gnu11 -D_GNU_SOURCE -DVERSION=\"$(GIT_VERSION)\"
LDLIBS = -lm -lncursesw

# Default target
all: $(BUILD_DIR)/$(TARGET_EXEC)

$(BUILD_DIR)/$(TARGET_EXEC): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS) $(LDLIBS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/tp_%.o: $(TP_DIR)/src/%.c
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

run: $(BUILD_DIR)/$(TARGET_EXEC)
	$(BUILD_DIR)/$(TARGET_EXEC)

clean:
	rm -rf $(BUILD_DIR)

PREFIX ?= /usr/local
BIN_DIR := $(PREFIX)/bin

install: $(BUILD_DIR)/$(TARGET_EXEC)
	install -d $(BIN_DIR)
	install $(BUILD_DIR)/$(TARGET_EXEC) $(BIN_DIR)/$(TARGET_EXEC)

uninstall:
	rm -f $(BIN_DIR)/$(TARGET_EXEC)

help:
	@echo "Usage: make [target]"
	@echo ""
	@echo "Targets:"
	@cat $(MAKEFILE_LIST) | grep -E '^[a-zA-Z0-9_-]+:.*?## .*$$' | awk 'BEGIN{FS=":.*?## "}; {printf "  %-20s %s\n", $$1, $$2}'

.PHONY: all clean run install uninstall help

-include $(DEPS)