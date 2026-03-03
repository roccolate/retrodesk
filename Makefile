
CC := gcc
SRC_DIR := src
BUILD_DIR := build

# Root where PDCurses is located (can be overridden):
PDCURSES_ROOT ?= pdcurses

# Detect pdcurses headers in the given root; set flags if present
ifneq (,$(wildcard $(PDCURSES_ROOT)/include/curses.h))
PDCFLAGS := -I$(PDCURSES_ROOT)/include -DHAVE_PDCURSES
ifneq (,$(wildcard $(PDCURSES_ROOT)/lib/libpdcurses.*))
PDLDFLAGS := -L$(PDCURSES_ROOT)/lib -lpdcurses
else
PDLDFLAGS :=
endif
else
PDCFLAGS :=
PDLDFLAGS :=
endif

CFLAGS := $(PDCFLAGS) -Wall -O2
LDFLAGS := $(PDLDFLAGS)

SOURCES := $(shell find $(SRC_DIR) -name '*.c')
TARGET := $(BUILD_DIR)/retrodesk

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SOURCES)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -rf $(BUILD_DIR)
