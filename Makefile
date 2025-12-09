CFLAGS ?= -Wall -ggdb -W -O
INCLUDES ?= -Iinclude
CC ?= gcc
LIBS ?=
TEST_LIBS ?=
LDFLAGS ?=
VERSION = 2.0
TMPDIR = /tmp/tinyhttpd2-$(VERSION)
TARGET_DIR = ./target
TARGET_TEST_DIR = $(TARGET_DIR)/test
TARGET = tinyhttpd2

.PHONY: clean, all, $(TARGET), prepare

all: clean prepare $(TARGET)

prepare:
	mkdir -p $(TARGET_DIR)
	mkdir -p $(TARGET_TEST_DIR)

clean:
	rm -rf $(TARGET_DIR)
	@echo "Clean up all generated files."

options.o: prepare include/options.h src/options.c
	$(CC) $(CFLAGS) $(INCLUDES) -c src/options.c -o $(TARGET_DIR)/options.o

$(TARGET).o: prepare src/tinyhttpd2.c include/options.h
	$(CC) $(CFLAGS) $(INCLUDES) -c src/tinyhttpd2.c -o $(TARGET_DIR)/$(TARGET).o

$(TARGET): prepare options.o $(TARGET).o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $(TARGET_DIR)/$(TARGET) $(TARGET_DIR)/options.o $(TARGET_DIR)/$(TARGET).o $(LIBS)
	@echo "Tinyhttpd2 compiled successfully."
