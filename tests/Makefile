# Host-only tests for synth_core and preset_store. Needs gcc and make; no devkitPPC.
#   make -C tests          build and run unit tests and the callback guard
#   make -C tests SAN=1    same, with UndefinedBehaviorSanitizer (needs libubsan)
#   make -C tests clean    remove tests/build

CC = gcc
SAN ?= 0

CFLAGS = -std=c99 -Wall -Wextra -Werror -O2 -ffp-contract=off
ifeq ($(SAN),1)
CFLAGS += -fsanitize=undefined -fno-sanitize-recover=all
endif
LDLIBS = -lm

BUILD = build
TEST_BIN = $(BUILD)/test_synth_core
PRESET_BIN = $(BUILD)/test_preset
IO_BIN = $(BUILD)/test_preset_io
STDIO_BIN = $(BUILD)/test_preset_stdio
CLASSIC_BIN = $(BUILD)/test_classic_input
CORE_SRCS = ../synth_core.c
CORE_HDRS = ../synth_core.h test_synth_core.c
PRESET_SRCS = ../synth_core.c ../preset_store.c
PRESET_HDRS = ../synth_core.h ../preset_store.h test_preset.c
IO_SRCS = $(PRESET_SRCS) ../preset_io.c
IO_HDRS = ../preset_io.h ../preset_store.h ../synth_core.h
STDIO_SRCS = $(IO_SRCS) ../preset_fs_stdio.c
STDIO_HDRS = $(IO_HDRS) ../preset_fs_stdio.h
CLASSIC_SRCS = ../classic_input.c
CLASSIC_HDRS = ../classic_input.h test_classic_input.c

.PHONY: all test clean

all: test

test: $(TEST_BIN) $(PRESET_BIN) $(IO_BIN) $(STDIO_BIN) $(CLASSIC_BIN)
	./$(TEST_BIN)
	./$(PRESET_BIN)
	./$(IO_BIN)
	./$(STDIO_BIN)
	./$(CLASSIC_BIN)
	sh check_callback.sh ../main.c
	sh check_startup_silence.sh ../main.c

$(TEST_BIN): $(CORE_SRCS) $(CORE_HDRS) | $(BUILD)
	$(CC) $(CFLAGS) -I.. -o $@ test_synth_core.c $(CORE_SRCS) $(LDLIBS)

$(PRESET_BIN): $(PRESET_SRCS) $(PRESET_HDRS) | $(BUILD)
	$(CC) $(CFLAGS) -I.. -o $@ test_preset.c $(PRESET_SRCS) $(LDLIBS)

$(IO_BIN): $(IO_SRCS) $(IO_HDRS) test_preset_io.c | $(BUILD)
	$(CC) $(CFLAGS) -I.. -o $@ test_preset_io.c $(IO_SRCS) $(LDLIBS)

$(STDIO_BIN): $(STDIO_SRCS) $(STDIO_HDRS) test_preset_stdio.c | $(BUILD)
	$(CC) $(CFLAGS) -I.. -o $@ test_preset_stdio.c $(STDIO_SRCS) $(LDLIBS)

$(CLASSIC_BIN): $(CLASSIC_SRCS) $(CLASSIC_HDRS) | $(BUILD)
	$(CC) $(CFLAGS) -I.. -o $@ test_classic_input.c $(CLASSIC_SRCS) $(LDLIBS)

$(BUILD):
	mkdir -p $(BUILD)

clean:
	rm -rf $(BUILD)
