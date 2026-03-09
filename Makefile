# tetOS Makefile

# Tools
ASM = nasm
CC = gcc
LD = ld

# Flags
ASMFLAGS = -f elf32
CFLAGS = -m32 -ffreestanding -nostdlib -nostdinc -fno-builtin -fno-stack-protector -nostartfiles -nodefaultlibs -Wall -Wextra -O2
LDFLAGS = -m elf_i386 -T linker.ld

# Directories
BUILD_DIR = build
BOOT_DIR = boot
KERNEL_DIR = kernel

# Output files
BOOTLOADER = $(BUILD_DIR)/boot.bin
KERNEL_BIN = $(BUILD_DIR)/kernel.bin
OS_IMAGE = $(BUILD_DIR)/tetos.bin

# Source files (recursive search through subdirectories)
KERNEL_C_SOURCES = $(shell find $(KERNEL_DIR) -name "*.c")
KERNEL_ASM_SOURCES = $(filter-out $(KERNEL_DIR)/entry.asm, $(shell find $(KERNEL_DIR) -name "*.asm"))
KERNEL_OBJECTS = $(BUILD_DIR)/entry.o \
                 $(addprefix $(BUILD_DIR)/, $(patsubst %.c, %.o, $(notdir $(KERNEL_C_SOURCES)))) \
                 $(addprefix $(BUILD_DIR)/, $(patsubst %.asm, %.o, $(notdir $(KERNEL_ASM_SOURCES))))

# VPATH lets make find source files in subdirectories
VPATH = $(shell find $(KERNEL_DIR) -type d)

# Default target
.PHONY: all
all: $(OS_IMAGE)

# Create build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Build bootloader (raw binary)
$(BOOTLOADER): $(BOOT_DIR)/boot.asm | $(BUILD_DIR)
	nasm -f bin $< -o $@

# Build kernel entry point (explicit rule)
$(BUILD_DIR)/entry.o: $(KERNEL_DIR)/entry.asm | $(BUILD_DIR)
	$(ASM) $(ASMFLAGS) $< -o $@

# Build kernel C files (VPATH resolves source location)
$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Build kernel assembly files (VPATH resolves source location)
$(BUILD_DIR)/%.o: %.asm | $(BUILD_DIR)
	$(ASM) $(ASMFLAGS) $< -o $@

# Link kernel
$(KERNEL_BIN): $(KERNEL_OBJECTS)
	$(LD) $(LDFLAGS) -o $@ $^
	objcopy -O binary $@ $@

$(OS_IMAGE): $(BOOTLOADER) $(KERNEL_BIN)
	cat $(BOOTLOADER) $(KERNEL_BIN) > $@
	@TARGET=$$((2048 * 512)); \
	CURRENT=$$(stat -c%s $@); \
	if [ $$CURRENT -lt $$TARGET ]; then \
		dd if=/dev/zero bs=1 count=$$(($$TARGET - $$CURRENT)) >> $@ 2>/dev/null; \
	fi
	@echo "Image: $$(stat -c%s $@) bytes ($$(( $$(stat -c%s $@) / 512 )) sectors)"

# Clean build artifacts
.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)

# Run in QEMU
.PHONY: run
run: $(OS_IMAGE)
	qemu-system-i386 -drive format=raw,file=$(OS_IMAGE),index=0,media=disk

# Debug in QEMU with GDB
.PHONY: debug
debug: $(OS_IMAGE)
	qemu-system-i386 -drive format=raw,file=$(OS_IMAGE) -s -S

# Print help
.PHONY: help
help:
	@echo "tetOS Build System"
	@echo "=================="
	@echo "make all    - Build tetOS"
	@echo "make run    - Build and run in QEMU"
	@echo "make debug  - Run in QEMU with GDB server"
	@echo "make clean  - Remove build artifacts"
	@echo "make help   - Show this help message"
