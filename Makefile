# DolHook Makefile

# devkitPPC setup
DEVKITPPC ?= /opt/devkitpro/devkitPPC
DEVKITPRO ?= /opt/devkitpro

# Toolchains
PPC_PREFIX = $(DEVKITPPC)/bin/powerpc-eabi-
PPC_CC = $(PPC_PREFIX)gcc
PPC_AS = $(PPC_PREFIX)as
PPC_LD = $(PPC_PREFIX)ld
PPC_OBJCOPY = $(PPC_PREFIX)objcopy
PPC_NM = $(PPC_PREFIX)nm

# Host compiler
CXX = g++

# Directories
RUNTIME_DIR = runtime
PAYLOAD_DIR = payload
PATCHER_DIR = tools/patchiso
EXAMPLE_DIR = examples/target

# Flags
PPC_CFLAGS = -mcpu=750 -meabi -mhard-float -fno-exceptions -fno-asynchronous-unwind-tables \
             -Os -Wall -Wextra -Werror -I$(RUNTIME_DIR)/include
PPC_ASFLAGS = -mcpu=750 -meabi
PPC_LDFLAGS = -T $(RUNTIME_DIR)/link.ld -nostartfiles -nostdlib -nodefaultlibs

# Optional features
ifdef DOLHOOK_NO_BANNER
PPC_CFLAGS += -DDOLHOOK_NO_BANNER
endif

ifdef DOLHOOK_NO_PATTERN
PPC_CFLAGS += -DDOLHOOK_NO_PATTERN
endif

CXX_FLAGS = -std=c++17 -Wall -Wextra -Werror -O2 -I$(PATCHER_DIR)

# Runtime sources
RUNTIME_SRCS = \
    $(RUNTIME_DIR)/src/dolhook.c \
    $(RUNTIME_DIR)/src/vi_banner.c \
    $(RUNTIME_DIR)/src/pattern.c \
    $(RUNTIME_DIR)/src/entry.S

RUNTIME_OBJS = $(patsubst %.c,%.o,$(patsubst %.S,%.o,$(RUNTIME_SRCS)))

# Patcher sources
PATCHER_SRCS = \
    $(PATCHER_DIR)/main.cpp \
    $(PATCHER_DIR)/dol.cpp \
    $(PATCHER_DIR)/gcm.cpp

PATCHER_OBJS = $(PATCHER_SRCS:.cpp=.o)

# Targets
.PHONY: all runtime patcher clean test

all: runtime patcher

# Runtime (PPC)
runtime: $(PAYLOAD_DIR)/payload.bin $(PAYLOAD_DIR)/payload.sym

$(PAYLOAD_DIR)/payload.elf: $(RUNTIME_OBJS)
	@mkdir -p $(PAYLOAD_DIR)
	$(PPC_LD) $(PPC_LDFLAGS) -o $@ $^
	@echo "Runtime ELF size:"
	@$(PPC_PREFIX)size $@

$(PAYLOAD_DIR)/payload.bin: $(PAYLOAD_DIR)/payload.elf
	$(PPC_OBJCOPY) -O binary $< $@
	@echo "Payload binary size: $$(stat -f%z $@ 2>/dev/null || stat -c%s $@) bytes"

$(PAYLOAD_DIR)/payload.sym: $(PAYLOAD_DIR)/payload.elf
	$(PPC_NM) $< | grep -E '__dolhook_(entry|original_entry)' | \
		awk '{print $$3 " 0x" $$1}' > $@ || echo "__dolhook_entry 0x80400000" > $@

%.o: %.c
	$(PPC_CC) $(PPC_CFLAGS) -c $< -o $@

%.o: %.S
	$(PPC_AS) $(PPC_ASFLAGS) $< -o $@

# Patcher (host)
patcher: $(PATCHER_DIR)/patchiso

$(PATCHER_DIR)/patchiso: $(PATCHER_OBJS)
	$(CXX) $(CXX_FLAGS) -o $@ $^

$(PATCHER_DIR)/%.o: $(PATCHER_DIR)/%.cpp
	$(CXX) $(CXX_FLAGS) -c $< -o $@

# Convenience target
patchiso: patcher
	@ln -sf $(PATCHER_DIR)/patchiso patchiso

# Clean
clean:
	rm -f $(RUNTIME_OBJS)
	rm -f $(PATCHER_OBJS)
	rm -f $(PAYLOAD_DIR)/*.elf $(PAYLOAD_DIR)/*.bin $(PAYLOAD_DIR)/*.sym
	rm -f $(PATCHER_DIR)/patchiso
	rm -f patchiso

# Test
test:
	@echo "Running tests..."
	@echo "TODO: Implement unit tests"

# Help
help:
	@echo "DolHook Build System"
	@echo ""
	@echo "Targets:"
	@echo "  all        - Build runtime and patcher (default)"
	@echo "  runtime    - Build PPC runtime payload"
	@echo "  patcher    - Build ISO patcher tool"
	@echo "  clean      - Remove build artifacts"
	@echo "  test       - Run tests"
	@echo ""
	@echo "Options:"
	@echo "  DOLHOOK_NO_BANNER=1  - Disable banner"
	@echo "  DOLHOOK_NO_PATTERN=1 - Disable pattern scanning"