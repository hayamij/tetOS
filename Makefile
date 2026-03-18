# tetOS Makefile

# Tools
ASM = nasm
CC = gcc
LD = ld

# Flags
ASMFLAGS = -f elf32
CFLAGS = -m32 -ffreestanding -nostdlib -nostdinc -fno-builtin -fno-stack-protector -fno-pic -fno-pie -nostartfiles -nodefaultlibs -Wall -Wextra -O2
LDFLAGS = -m elf_i386 -T linker.ld

# Directories
BUILD_DIR = build
BOOT_DIR = boot
KERNEL_DIR = kernel
USER_DIR = user

# Kernel module manifests
KERNEL_MODULE_MKS = $(shell find $(KERNEL_DIR) -mindepth 2 -maxdepth 2 -name "module.mk" | sort)

# Source files are declared in kernel/*/module.mk
KERNEL_C_SOURCES =
KERNEL_ASM_SOURCES =
KERNEL_ROOT_C_SOURCES = $(KERNEL_DIR)/kernel.c

-include $(KERNEL_MODULE_MKS)

# Output files
BOOTLOADER = $(BUILD_DIR)/boot.bin
KERNEL_BIN = $(BUILD_DIR)/kernel.bin
OS_IMAGE = $(BUILD_DIR)/tetos.bin
USER_ELF = $(BUILD_DIR)/user/hello.elf
USER_ELF_OBJ = $(BUILD_DIR)/user/hello_elf.o

# Source files
KERNEL_ENTRY_SOURCE = $(KERNEL_DIR)/entry.asm
KERNEL_OBJECTS = $(BUILD_DIR)/$(KERNEL_ENTRY_SOURCE:.asm=.o) \
				 $(patsubst %.c,$(BUILD_DIR)/%.o,$(KERNEL_ROOT_C_SOURCES)) \
				 $(patsubst %.c,$(BUILD_DIR)/%.o,$(KERNEL_C_SOURCES)) \
				 $(patsubst %.asm,$(BUILD_DIR)/%.o,$(KERNEL_ASM_SOURCES))
KERNEL_OBJECTS += $(USER_ELF_OBJ)

# Default target
.PHONY: all
all: $(OS_IMAGE)

$(USER_ELF): $(USER_DIR)/hello.c $(KERNEL_DIR)/user/libc.c $(USER_DIR)/linker.ld | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) -m32 -ffreestanding -nostdlib -nostdinc -fno-builtin -fno-stack-protector -fno-pic -fno-pie -c $(USER_DIR)/hello.c -o $(BUILD_DIR)/user/hello.o
	$(CC) -m32 -ffreestanding -nostdlib -nostdinc -fno-builtin -fno-stack-protector -fno-pic -fno-pie -c $(KERNEL_DIR)/user/libc.c -o $(BUILD_DIR)/user/libc.o
	$(LD) -m elf_i386 -T $(USER_DIR)/linker.ld -o $@ $(BUILD_DIR)/user/hello.o $(BUILD_DIR)/user/libc.o

$(USER_ELF_OBJ): $(USER_ELF)
	$(LD) -m elf_i386 -r -b binary -o $@ $(USER_ELF)

# Create build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Build bootloader (raw binary)
$(BOOTLOADER): $(BOOT_DIR)/boot.asm | $(BUILD_DIR)
	nasm -f bin $< -o $@

# Build kernel C files
$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Build kernel assembly files
$(BUILD_DIR)/%.o: %.asm | $(BUILD_DIR)
	@mkdir -p $(dir $@)
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
