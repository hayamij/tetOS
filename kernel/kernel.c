#include "vga/vga.h"
#include "graphics/graphics.h"
#include "gdt/gdt.h"
#include "idt/idt.h"
#include "isr/isr.h"
#include "timer/timer.h"
#include "keyboard/keyboard.h"
#include "mouse/mouse.h"
#include "wm/wm.h"
#include "desktop/desktop.h"
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
#include "io/serial.h"

extern uint32_t kernel_end;

void kernel_main(void) {
    serial_init();
    serial_puts("[SERIAL] kernel_main entered\r\n");

    // Direct VGA write test before vga_init
    volatile uint16_t* vga = (volatile uint16_t*)0xB8000;
    uint16_t entry = ('K' | (0x0F << 8));
    vga[0] = entry;
    vga[1] = ('E' | (0x0F << 8));
    vga[2] = ('R' | (0x0F << 8));
    vga[3] = ('N' | (0x0F << 8));
    vga[4] = ('E' | (0x0F << 8));
    vga[5] = ('L' | (0x0F << 8));
    vga[6] = ('1' | (0x0F << 8));  // Marker 1: kernel_main entered
    
    vga_init();
    serial_puts("[SERIAL] vga_init done\r\n");
    
    gdt_init();
    serial_puts("[SERIAL] gdt_init done\r\n");
    vga[7] = ('2' | (0x0F << 8));  // Marker 2: GDT done
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
    serial_puts("[SERIAL] idt_init done\r\n");
    vga[8] = ('3' | (0x0F << 8));  // Marker 3: IDT done
    vga_write_color("[OK]", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_write(" IDT initialized\n");
    
    isr_init();
    serial_puts("[SERIAL] isr_init done\r\n");
    vga[9] = ('4' | (0x0F << 8));  // Marker 4: ISR done
    vga_write_color("[OK]", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_write(" ISR initialized\n");

    syscall_init();
    serial_puts("[SERIAL] syscall_init done\r\n");
    vga_write_color("[OK]", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_write(" Syscall initialized (int 0x80)\n");

    pmm_init((uint32_t)&kernel_end);
    serial_puts("[SERIAL] pmm_init done\r\n");
    vga[10] = ('5' | (0x0F << 8));  // Marker 5: PMM done
    vga_write_color("[OK]", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_write(" PMM initialized (120 MB free)\n");

    vmm_init();
    serial_puts("[SERIAL] vmm_init done (paging on)\r\n");
    vga[11] = ('6' | (0x0F << 8));  // Marker 6: VMM done - PAGING ACTIVE NOW
    vga_write_color("[OK]", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_write(" VMM initialized (32 MB identity mapped, paging on)\n");

    graphics_init();
    serial_puts("[SERIAL] graphics_init done\r\n");
        kprintf("[SERIAL] gfx mode %ux%u bpp=%u pitch=%u fb=0x%x\r\n",
            graphics_get_width(), graphics_get_height(), (uint32_t)graphics_get_bpp(),
            graphics_get_pitch(), (uint32_t)graphics_framebuffer);
    vga_write_color("[OK]", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_write(" Graphics initialized (VESA 1024x768x32)\n");

    heap_init();
    serial_puts("[SERIAL] heap_init done\r\n");
    vga_write_color("[OK]", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_write(" Heap initialized (4 MB at 0x400000)\n");
    
    timer_init(100);
    serial_puts("[SERIAL] timer_init done\r\n");
    vga_write_color("[OK]", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_write(" Timer initialized\n");
    
    keyboard_init();
    serial_puts("[SERIAL] keyboard_init done\r\n");
    vga_write_color("[OK]", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_write(" Keyboard initialized\n");

    mouse_init();
    serial_puts("[SERIAL] mouse_init done\r\n");
    vga_write_color("[OK]", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_write(" Mouse initialized\n");

    wm_init();
    serial_puts("[SERIAL] wm_init done\r\n");
    vga_write_color("[OK]", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_write(" Window manager initialized\n");

    if (ata_init() == 0) {
        serial_puts("[SERIAL] ata_init ok\r\n");
        vga_write_color("[OK]", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        vga_write(" ATA disk detected\n");
    } else {
        serial_puts("[SERIAL] ata_init failed\r\n");
        vga_write_color("[--]", VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
        vga_write(" No ATA disk\n");
    }

    process_init();
    serial_puts("[SERIAL] process_init done\r\n");
    timer_set_scheduler(schedule);
    serial_puts("[SERIAL] timer_set_scheduler done\r\n");
    vga_write_color("[OK]", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_write(" Process manager initialized\n");

    fs_init();
    serial_puts("[SERIAL] fs_init done\r\n");
    if (tetfs_is_mounted()) {
        appseed_install();
        serial_puts("[SERIAL] appseed_install done\r\n");
        vga_write_color("[OK]", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        vga_write(" tetFS mounted\n");
    } else {
        serial_puts("[SERIAL] tetFS not mounted\r\n");
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
    
    serial_puts("[SERIAL] enabling interrupts (sti)\r\n");
    __asm__ __volatile__("sti");
    
    desktop_init();
    serial_puts("[SERIAL] desktop_init done\r\n");

    while(1) {
        desktop_update();
        desktop_render();
        __asm__ __volatile__("hlt");
    }
}
