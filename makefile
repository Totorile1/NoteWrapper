CONFIG_DIR := $(HOME)/.config/notewrapper
CONFIG_FILE := $(CONFIG_DIR)/config.json
CC = gcc
SRC = startup.c
TARGET = notewrapper
CFLAGS = -Wall -O2 `pkg-config --cflags libcjson ncurses`
LDFLAGS = `pkg-config --libs libcjson ncurses`
DEBUG ?= 0
ifeq ($(DEBUG),1)
CFLAGS += -g
endif
.PHONY: all clean run install_config

# existing targets
all: install_config $(TARGET)

$(TARGET): $(SRC)
	@$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

run: $(TARGET)
	@./$(TARGET)

clean:
	@rm -f $(TARGET)

# new target for configuration
install_config:
	@mkdir -p $(CONFIG_DIR)
	@if [ ! -f $(CONFIG_FILE) ]; then \
		cp ./config.json $(CONFIG_FILE); \
		echo "config.json installed to $(CONFIG_FILE)"; \
	else \
		echo "$(CONFIG_FILE) already exists, skipping installing default config file"; \
	fi
