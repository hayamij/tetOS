#include "vga/vga.h"
#include "graphics/graphics.h"
#include "gdt/gdt.h"
#include "idt/idt.h"
#include "isr/isr.h"
#include "timer/timer.h"
#include "keyboard/keyboard.h"
#include "shell/shell.h"
#include "stdio/stdio.h"
#include "pmm/pmm.h"
#include "vmm/vmm.h"
#include "heap/heap.h"
#include "ata/ata.h"
#include "process/process.h"
#include "fs/tetfs.h"
#include "syscall/syscall.h"
#include "user/appseed.h"

extern uint32_t kernel_end;

void kernel_main(void) {
    vga_init();
    
    gdt_init();
    vga_write_color("[OK]", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_write(" GDT and TSS initialized\n");
    
    vga_write("\n");
    vga_write_color("          === tetOS v0.1.0 ===\n", VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_write_color("     Hello from tetOS C Kernel!\n\n", VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
    
    vga_write_color("          .-.          \n", VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
    vga_write_color("     .--/      \\--.     \n", VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
    vga_write_color("  ( ( ) | ^  ^ | ( ( )   ", VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
    vga_write("   < Kasane Teto says hi!\n");
    vga_write_color("  ( ~ ) |   v  | ( ~ )   \n", VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
    vga_write_color("     (@)\\______/(@) \n", VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
    vga_write("\n");
    
    vga_write_color("[OK]", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_write(" Protected Mode initialized\n");
    
    idt_init();
    vga_write_color("[OK]", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_write(" IDT initialized\n");
    
    isr_init();
    vga_write_color("[OK]", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_write(" ISR initialized\n");

    syscall_init();
    vga_write_color("[OK]", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_write(" Syscall initialized (int 0x80)\n");

    pmm_init((uint32_t)&kernel_end);
    vga_write_color("[OK]", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_write(" PMM initialized (120 MB free)\n");

    vmm_init();
    vga_write_color("[OK]", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_write(" VMM initialized (32 MB identity mapped, paging on)\n");

    graphics_init();
    vga_write_color("[OK]", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_write(" Graphics initialized (VGA mode 13h, 320x200)\n");
    
    graphics_fill_rect(10, 10, 300, 180, 1);
    graphics_fill_rect(20, 20, 280, 160, 2);
    graphics_fill_rect(30, 30, 40, 40, 15);
    graphics_fill_rect(270, 30, 40, 40, 15);
    graphics_draw_string(120, 80, "tetOS v0.1", 15, 2);
    graphics_draw_string(80, 100, "VGA Graphics Mode 13h", 15, 2);
    graphics_present();

    heap_init();
    vga_write_color("[OK]", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_write(" Heap initialized (4 MB at 0x400000)\n");
    
    timer_init(100);
    vga_write_color("[OK]", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_write(" Timer initialized\n");
    
    keyboard_init();
    vga_write_color("[OK]", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_write(" Keyboard initialized\n");

    if (ata_init() == 0) {
        vga_write_color("[OK]", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        vga_write(" ATA disk detected\n");
    } else {
        vga_write_color("[--]", VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
        vga_write(" No ATA disk\n");
    }

    process_init();
    timer_set_scheduler(schedule);
    vga_write_color("[OK]", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_write(" Process manager initialized\n");

    fs_init();
    if (tetfs_is_mounted()) {
        appseed_install();
        vga_write_color("[OK]", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        vga_write(" tetFS mounted\n");
    } else {
        vga_write_color("[--]", VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
        vga_write(" tetFS not formatted (run 'format')\n");
    }
    
    vga_write_color("[OK]", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_write(" VGA text mode active (80x50)\n");
    
    vga_write("\n");
    vga_set_color(VGA_COLOR_LIGHT_MAGENTA, VGA_COLOR_BLACK);
    vga_write("  \"my love, can you teach me to be real?\"\n");
    vga_set_color(VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
    vga_write("    - Machine Love (feat. Kasane Teto)\n\n");
    
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    
    __asm__ __volatile__("sti");
    
    shell_init();
    shell_run();
    
    while(1) {
        __asm__ __volatile__("hlt");
    }
}
