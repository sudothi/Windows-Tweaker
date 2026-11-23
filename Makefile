# Makefile for MinGW-w64 / Clang on Windows
CC      = gcc
CFLAGS  = -std=c11 -Wall -Wextra -O2 -Iinclude -D_CRT_SECURE_NO_WARNINGS
LDFLAGS = -ladvapi32 -luser32 -lkernel32

SRCS    = src/main.c src/ui.c src/storage.c src/tweaks.c src/hardware.c
TARGET  = bin/windows-tweaker.exe
TEST_BIN= bin/test_suite.exe

all: $(TARGET)

$(TARGET): $(SRCS)
	@if not exist bin mkdir bin
	$(CC) $(CFLAGS) $(SRCS) -o $(TARGET) $(LDFLAGS)

test: $(TEST_BIN)
	$(TEST_BIN)

$(TEST_BIN): tests/test_suite.c src/ui.c src/storage.c src/tweaks.c src/hardware.c
	@if not exist bin mkdir bin
	$(CC) $(CFLAGS) tests/test_suite.c src/ui.c src/storage.c src/tweaks.c src/hardware.c -o $(TEST_BIN) $(LDFLAGS)

clean:
	@if exist bin rmdir /s /q bin

.PHONY: all test clean
