#include "vga.h"

void kernel_main(void) {
    vga_init();
    
    vga_write("\n");
    vga_write_color("          === tetOS v0.1.0 ===\n", VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_write_color("     Hello from tetOS C Kernel!\n\n", VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
    
    vga_write_color("          .-.          \n", VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
    vga_write_color("     .--/      \\--.     \n", VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
    vga_write_color("  ( ( ) | ^  ^ | ( ( )   ", VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK); vga_write("   < Kasane Teto says hi!\n");
    vga_write_color("  ( ~ ) |   v  | ( ~ )   \n", VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
    vga_write_color("     (@)\\______/(@) \n", VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
    
    vga_write_color("[OK]", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_write(" Protected Mode initialized\n");
    
    vga_write_color("[OK]", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_write(" VGA text mode active (80x25)\n");
    
    vga_write_color("[OK]", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_write(" C kernel running\n\n");
    
    vga_set_color(VGA_COLOR_LIGHT_MAGENTA, VGA_COLOR_BLACK);
    vga_write("  \"my love, can you teach me to be real?\"\n");
    vga_set_color(VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
    vga_write("    - Machine Love (feat. Kasane Teto)\n\n");
    
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_write("  Kernel initialization complete.\n");
    vga_write("  System halted. Press CTRL+C to exit QEMU.\n");
    
    while(1) {
        __asm__ __volatile__("hlt");
    }
}
