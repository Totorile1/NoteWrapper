# Configuration
CONFIG_DIR := $(HOME)/.config/notewrapper
CONFIG_FILE := $(CONFIG_DIR)/config.json

# Compiler and flags
CC := gcc
CFLAGS := -Wall -O2 `pkg-config --cflags libcjson ncurses`
LDFLAGS := `pkg-config --libs libcjson ncurses`
DEBUG ?= 0

ifeq ($(DEBUG),1)
    CFLAGS += -g
endif

# Sources and target
SRC_DIR := src
SRC := $(SRC_DIR)/main.c $(SRC_DIR)/ui.c $(SRC_DIR)/utils.c
TARGET := notewrapper

# Phony targets
.PHONY: all clean run install_config

# Default target
all: install_config $(TARGET)

# Build target
$(TARGET): $(SRC)
	@$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)
	@echo "Built $(TARGET) successfully."

# Run program
run: $(TARGET)
	@./$(TARGET)

# Clean build artifacts
clean:
	@rm -f $(TARGET)
	@echo "Cleaned $(TARGET)."

# Install default config if it does not exist
install_config:
	@mkdir -p $(CONFIG_DIR)
	@if [ ! -f $(CONFIG_FILE) ]; then \
		cp ./config.json $(CONFIG_FILE); \
		echo "config.json installed to $(CONFIG_FILE)"; \
	else \
		echo "$(CONFIG_FILE) already exists, skipping installing default config file"; \
	fi
