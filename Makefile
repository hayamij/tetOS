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

# Source files
KERNEL_C_SOURCES = $(wildcard $(KERNEL_DIR)/*.c)
KERNEL_ASM_SOURCES = $(filter-out $(KERNEL_DIR)/entry.asm, $(wildcard $(KERNEL_DIR)/*.asm))
KERNEL_OBJECTS = $(BUILD_DIR)/entry.o \
                 $(patsubst $(KERNEL_DIR)/%.c, $(BUILD_DIR)/%.o, $(KERNEL_C_SOURCES)) \
                 $(patsubst $(KERNEL_DIR)/%.asm, $(BUILD_DIR)/%.o, $(KERNEL_ASM_SOURCES))

# Default target
.PHONY: all
all: $(OS_IMAGE)

# Create build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Build bootloader (raw binary)
$(BOOTLOADER): $(BOOT_DIR)/boot.asm | $(BUILD_DIR)
	nasm -f bin $< -o $@

# Build kernel C files
$(BUILD_DIR)/%.o: $(KERNEL_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Build kernel assembly files (ELF format)
$(BUILD_DIR)/%.o: $(KERNEL_DIR)/%.asm | $(BUILD_DIR)
	$(ASM) $(ASMFLAGS) $< -o $@

# Link kernel
$(KERNEL_BIN): $(KERNEL_OBJECTS)
	$(LD) $(LDFLAGS) -o $@ $^
	objcopy -O binary $@ $@

$(OS_IMAGE): $(BOOTLOADER) $(KERNEL_BIN)
	cat $(BOOTLOADER) $(KERNEL_BIN) > $@
	@SIZE=$$(stat -c%s $@); \
	SECTORS=$$((($${SIZE} + 511) / 512)); \
	BYTES=$$(($$SECTORS * 512)); \
	if [ $$SIZE -lt $$BYTES ]; then \
		dd if=/dev/zero bs=1 count=$$(($$BYTES - $$SIZE)) >> $@ 2>/dev/null; \
	fi

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
