# tetOS Makefile
# Build system for tetOS kernel

# Assembler
ASM = nasm
ASMFLAGS = -f bin

# Directories
BUILD_DIR = build
BOOT_DIR = boot

# Output
BOOTLOADER = $(BUILD_DIR)/boot.bin
KERNEL = $(BUILD_DIR)/tetos.bin

# Default target
.PHONY: all
all: $(KERNEL)

# Create build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Build bootloader
$(BOOTLOADER): $(BOOT_DIR)/boot.asm | $(BUILD_DIR)
	$(ASM) $(ASMFLAGS) $< -o $@

# For now, kernel is just the bootloader
$(KERNEL): $(BOOTLOADER)
	cp $(BOOTLOADER) $(KERNEL)

# Clean build artifacts
.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)

# Run in QEMU
.PHONY: run
run: $(KERNEL)
	qemu-system-i386 -drive format=raw,file=$(KERNEL)

# Print help
.PHONY: help
help:
	@echo "tetOS Build System"
	@echo "=================="
	@echo "make all    - Build tetOS"
	@echo "make run    - Build and run in QEMU"
	@echo "make clean  - Remove build artifacts"
	@echo "make help   - Show this help message"
