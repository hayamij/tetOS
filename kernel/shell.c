#include "shell.h"
#include "keyboard.h"
#include "vga.h"
#include "stdio.h"
#include "string.h"
#include "timer.h"
#include "pmm.h"
#include "heap.h"

#define COMMAND_BUFFER_SIZE 256

static char command_buffer[COMMAND_BUFFER_SIZE];
static uint32_t command_pos = 0;

static void shell_print_prompt(void) {
    vga_write_color("teto", VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_write_color("@", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_write_color("tetOS", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_write(" $ ");
}

static void shell_execute_command(const char* cmd) {
    if (strcmp(cmd, "help") == 0) {
        vga_write("Available commands:\n");
        vga_write("  help   - Show this help message\n");
        vga_write("  clear  - Clear the screen\n");
        vga_write("  echo   - Print text\n");
        vga_write("  uptime - Show system uptime\n");
        vga_write("  mem    - Show memory info\n");
        vga_write("  about  - About tetOS\n");
    }
    else if (strcmp(cmd, "clear") == 0) {
        vga_clear();
    }
    else if (strncmp(cmd, "echo ", 5) == 0) {
        vga_write(cmd + 5);
        vga_write("\n");
    }
    else if (strcmp(cmd, "uptime") == 0) {
        uint32_t ticks = timer_ticks();
        uint32_t seconds = ticks / 100;
        uint32_t minutes = seconds / 60;
        uint32_t hours = minutes / 60;
        
        kprintf("Uptime: %u hours, %u minutes, %u seconds\n", 
                hours, minutes % 60, seconds % 60);
    }
    else if (strcmp(cmd, "mem") == 0) {
        uint32_t free_f  = pmm_free_frames();
        uint32_t total_f = pmm_total_frames();
        uint32_t used_f  = total_f - free_f;
        uint32_t heap_used = heap_used_bytes();
        kprintf("Physical Memory:\n");
        kprintf("  Total: %u MB (%u frames)\n", total_f / 256, total_f);
        kprintf("  Used:  %u MB (%u frames)\n", used_f / 256, used_f);
        kprintf("  Free:  %u MB (%u frames)\n", free_f / 256, free_f);
        kprintf("Kernel Heap (4 MB at 0x400000):\n");
        kprintf("  Used:  %u bytes\n", heap_used);
        kprintf("  Free:  %u bytes\n", 4 * 1024 * 1024 - heap_used);
    }
    else if (strcmp(cmd, "about") == 0) {
        vga_write_color("\n=== tetOS v0.1.0 ===\n", VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
        vga_write("A simple operating system inspired by Kasane Teto\n");
        vga_write("Built with love and dedication\n");
        vga_write_color("\"my love, can you teach me to be real?\"\n\n", 
                       VGA_COLOR_LIGHT_MAGENTA, VGA_COLOR_BLACK);
    }
    else if (cmd[0] == '\0') {
    }
    else {
        vga_write("Unknown command: ");
        vga_write(cmd);
        vga_write("\nType 'help' for available commands.\n");
    }
}

void shell_init(void) {
    command_pos = 0;
    memset(command_buffer, 0, COMMAND_BUFFER_SIZE);
}

void shell_run(void) {
    vga_write("\n");
    vga_write_color("Welcome to tetOS Shell!\n", VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
    vga_write("Type 'help' for available commands.\n\n");
    
    shell_print_prompt();
    
    while (1) {
        char c = keyboard_getchar();
        
        if (c == '\n') {
            vga_write("\n");
            command_buffer[command_pos] = '\0';
            shell_execute_command(command_buffer);
            command_pos = 0;
            memset(command_buffer, 0, COMMAND_BUFFER_SIZE);
            shell_print_prompt();
        }
        else if (c == '\b') {
            if (command_pos > 0) {
                command_pos--;
                command_buffer[command_pos] = '\0';
                vga_write("\b \b");
            }
        }
        else if (command_pos < COMMAND_BUFFER_SIZE - 1) {
            command_buffer[command_pos++] = c;
            vga_putchar(c);
        }
    }
}
