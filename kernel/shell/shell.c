#include "shell.h"
#include "../keyboard/keyboard.h"
#include "../vga/vga.h"
#include "../stdio/stdio.h"
#include "../string/string.h"
#include "../timer/timer.h"
#include "../pmm/pmm.h"
#include "../heap/heap.h"
#include "../ata/ata.h"
#include "../process/process.h"
#include "../fs/tetfs.h"
#include "../exec/exec.h"
#include "../syscall/syscall.h"
#include "../user/usermode.h"
#include "../user/appseed.h"

#define COMMAND_BUFFER_SIZE 256

static char     command_buffer[COMMAND_BUFFER_SIZE];
static uint32_t command_pos = 0;

static uint16_t cwd_inode = TETFS_ROOT_INODE;
static char     cwd_path[128] = "/";

static int parse_u32(const char *s, uint32_t *out) {
    uint32_t v = 0;
    int i = 0;
    if (!s || !s[0]) return -1;
    while (s[i]) {
        if (s[i] < '0' || s[i] > '9') return -1;
        v = v * 10 + (uint32_t)(s[i] - '0');
        i++;
    }
    *out = v;
    return 0;
}

static void demo_proc_entry(void) {
    volatile uint16_t *pos = (volatile uint16_t *)0xB8000 + 79;
    const char spin[] = "-\\|/";
    uint32_t i = 0;
    volatile uint32_t delay;
    while (i < 400) {
        *pos = (uint16_t)((0x0A << 8) | spin[i % 4]);
        i++;
        for (delay = 0; delay < 200000; delay++)
            __asm__ volatile("nop");
    }
    *pos = (uint16_t)((0x07 << 8) | ' ');
    process_exit();
}

static void user_demo_ring3(void) {
    static const char msg[] = "[ring3] hello from user mode via int 0x80\n";
    __asm__ __volatile__(
        "movl %0, %%ebx\n"
        "movl %1, %%ecx\n"
        "movl %2, %%eax\n"
        "int $0x80\n"
        "movl %3, %%eax\n"
        "int $0x80\n"
        :
        : "r"(msg), "r"((uint32_t)(sizeof(msg) - 1)), "i"(SYS_WRITE), "i"(SYS_EXIT)
        : "eax", "ebx", "ecx", "memory"
    );
    while (1) {
        __asm__ __volatile__("hlt");
    }
}

static void user_demo_entry(void) {
    uint8_t *stack = (uint8_t *)kmalloc(4096);
    if (!stack) {
        process_exit();
    }
    enter_user_mode(user_demo_ring3, (uint32_t)(stack + 4096));
    process_exit();
}

static void shell_print_prompt(void) {
    vga_write_color("teto", VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_write_color("@", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_write_color("tetOS", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_write_color(":", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_write_color(cwd_path, VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
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
        vga_write("  disk   - Show disk info\n");
        vga_write("  ps     - List running processes\n");
        vga_write("  spawn  - Spawn a demo background process\n");
        vga_write("  kill   - Kill a process by PID\n");
        vga_write("  utest  - Spawn a ring3 user-mode test process\n");
        vga_write("  run    - Run ELF file from tetFS root\n");
        vga_write("  seed   - Install bundled hello.elf\n");
        vga_write("  format - Format tetFS on disk\n");
        vga_write("  ls     - List files\n");
        vga_write("  cd     - Change directory\n");
        vga_write("  pwd    - Print working directory\n");
        vga_write("  touch  - Create empty file\n");
        vga_write("  mkdir  - Create directory\n");
        vga_write("  cat    - Concatenate and print file contents\n");
        vga_write("  write  - Write text to file\n");
        vga_write("  rm     - Delete file or directory\n");
        vga_write("  about  - About tetOS\n");
        vga_write("  teto   - Show Kasane Teto\n");
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
    else if (strcmp(cmd, "disk") == 0) {
        ata_drive_t* d = ata_get_drive();
        if (!d->present) {
            vga_write("No disk detected.\n");
        } else {
            kprintf("Model:   %s\n", d->model);
            kprintf("Sectors: %u\n", d->sectors);
            kprintf("Size:    %u MB\n", d->sectors / 2048);
        }
    }
    else if (strcmp(cmd, "ps") == 0) {
        process_t *list[MAX_PROCS];
        int count = process_list(list, MAX_PROCS);
        int i;
        vga_write("PID  STATE    NAME\n");
        vga_write("---  -------  ----\n");
        for (i = 0; i < count; i++) {
            const char *s;
            if      (list[i]->state == PROC_READY)   s = "READY  ";
            else if (list[i]->state == PROC_RUNNING) s = "RUNNING";
            else if (list[i]->state == PROC_DEAD)    s = "DEAD   ";
            else                                     s = "???????";
            kprintf("%u    %s  %s\n", list[i]->pid, s, list[i]->name);
        }
    }
    else if (strcmp(cmd, "spawn") == 0) {
        process_t *p = process_create(demo_proc_entry, "demo");
        if (p) {
            kprintf("Spawned PID %u: %s\n", p->pid, p->name);
            vga_write("Watch the top-right corner...\n");
        } else {
            vga_write("Failed: no free process slots\n");
        }
    }
    else if (strncmp(cmd, "kill ", 5) == 0) {
        uint32_t pid;
        int r;
        if (parse_u32(cmd + 5, &pid) != 0) {
            vga_write("Usage: kill <pid>\n");
        } else {
            r = process_kill(pid);
            if (r == 0)
                kprintf("Killed PID %u\n", pid);
            else if (r == -2)
                vga_write("Cannot kill current process.\n");
            else
                vga_write("PID not found.\n");
        }
    }
    else if (strcmp(cmd, "utest") == 0) {
        process_t *p = process_create(user_demo_entry, "userdemo");
        if (p)
            kprintf("Spawned PID %u: %s\n", p->pid, p->name);
        else
            vga_write("Failed: no free process slots\n");
    }
    else if (strncmp(cmd, "run ", 4) == 0) {
        if (!tetfs_is_mounted()) {
            vga_write("No filesystem mounted. Run 'format' first.\n");
        } else {
            int pid = exec_elf_from_disk(cmd + 4);
            if (pid >= 0) {
                kprintf("ELF started PID %u: %s\n", (uint32_t)pid, cmd + 4);
            } else {
                kprintf("Failed to run ELF (%d).\n", pid);
            }
        }
    }
    else if (strcmp(cmd, "format") == 0) {
        vga_write("Formatting tetFS...");
        if (tetfs_format() == 0) {
            vga_write(" done.\n");
            if (appseed_install() == 0)
                vga_write("Bundled hello.elf installed.\n");
            else
                vga_write("Bundled hello.elf install failed.\n");
        } else
            vga_write(" FAILED.\n");
    }
    else if (strcmp(cmd, "seed") == 0) {
        if (appseed_install() == 0)
            vga_write("Bundled hello.elf installed.\n");
        else
            vga_write("Bundled hello.elf install failed.\n");
    }
    else if (strcmp(cmd, "ls") == 0) {
        if (!tetfs_is_mounted()) { vga_write("No filesystem mounted. Run 'format' first.\n"); }
        else {
            tetfs_inode_t list[TETFS_MAX_INODES];
            int n = tetfs_list(cwd_inode, list, TETFS_MAX_INODES);
            int i;
            if (n == 0) { vga_write("(empty)\n"); }
            for (i = 0; i < n; i++) {
                if (list[i].type == TETFS_TYPE_DIR) {
                    vga_write_color(list[i].name, VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
                    int idx = tetfs_find(list[i].name, cwd_inode);
                    int children = (idx >= 0) ? tetfs_count_children((uint16_t)idx) : 0;
                    kprintf("  (%d items)\n", children);
                } else {
                    vga_write(list[i].name);
                    kprintf("  (%u B)\n", list[i].size);
                }
            }
        }
    }
    else if (strcmp(cmd, "pwd") == 0) {
        vga_write(cwd_path);
        vga_write("\n");
    }
    else if (strncmp(cmd, "cd", 2) == 0 && (cmd[2] == '\0' || cmd[2] == ' ')) {
        if (!tetfs_is_mounted()) { vga_write("No filesystem.\n"); }
        else {
            const char *target = (cmd[2] == ' ') ? cmd + 3 : "/";
            if (strcmp(target, "/") == 0 || target[0] == '\0') {
                cwd_inode = TETFS_ROOT_INODE;
                cwd_path[0] = '/';
                cwd_path[1] = '\0';
            } else if (strcmp(target, "..") == 0) {
                if (cwd_inode == TETFS_ROOT_INODE) {
                } else {
                    tetfs_inode_t node;
                    tetfs_read_inode(cwd_inode, &node);
                    uint16_t parent = node.parent;
                    if (parent == 0xFFFF) parent = TETFS_ROOT_INODE;
                    cwd_inode = parent;
                    uint32_t len = (uint32_t)strlen(cwd_path);
                    if (len > 1 && cwd_path[len-1] == '/') { cwd_path[--len] = '\0'; }
                    while (len > 1 && cwd_path[len-1] != '/') len--;
                    cwd_path[len] = '\0';
                    if (len == 0) { cwd_path[0] = '/'; cwd_path[1] = '\0'; }
                }
            } else {
                int idx = tetfs_find(target, cwd_inode);
                if (idx < 0) {
                    vga_write("cd: no such directory: ");
                    vga_write(target);
                    vga_write("\n");
                } else {
                    tetfs_inode_t node;
                    tetfs_read_inode((uint16_t)idx, &node);
                    if (node.type != TETFS_TYPE_DIR) {
                        vga_write("cd: not a directory: ");
                        vga_write(target);
                        vga_write("\n");
                    } else {
                        cwd_inode = (uint16_t)idx;
                        int len = (int)strlen(cwd_path);
                        if (len > 1) cwd_path[len++] = '/';
                        int j = 0;
                        while (target[j] && len < 126)
                            cwd_path[len++] = target[j++];
                        cwd_path[len] = '\0';
                    }
                }
            }
        }
    }
    else if (strncmp(cmd, "touch ", 6) == 0) {
        if (!tetfs_is_mounted()) { vga_write("No filesystem.\n"); }
        else {
            int r = tetfs_create(cmd + 6, cwd_inode, TETFS_TYPE_FILE);
            if (r >= 0) vga_write("Created.\n");
            else        vga_write("Failed (exists or full).\n");
        }
    }
    else if (strncmp(cmd, "mkdir ", 6) == 0) {
        if (!tetfs_is_mounted()) { vga_write("No filesystem.\n"); }
        else {
            int r = tetfs_create(cmd + 6, cwd_inode, TETFS_TYPE_DIR);
            if (r >= 0) vga_write("Created.\n");
            else        vga_write("Failed (exists or full).\n");
        }
    }
    else if (strncmp(cmd, "cat ", 4) == 0) {
        if (!tetfs_is_mounted()) { vga_write("No filesystem.\n"); }
        else {
            int idx = tetfs_find(cmd + 4, cwd_inode);
            if (idx < 0) { vga_write("File not found.\n"); }
            else {
                tetfs_inode_t node;
                tetfs_read_inode((uint16_t)idx, &node);
                if (node.size == 0) { vga_write("(empty file)\n"); }
                else {
                    char  buf[513];
                    uint32_t off = 0;
                    while (off < node.size) {
                        int n = tetfs_read((uint16_t)idx, buf, off, 512);
                        if (n <= 0) break;
                        buf[n] = '\0';
                        vga_write(buf);
                        off += (uint32_t)n;
                    }
                    vga_write("\n");
                }
            }
        }
    }
    else if (strncmp(cmd, "write ", 6) == 0) {
        if (!tetfs_is_mounted()) { vga_write("No filesystem.\n"); }
        else {
            const char *rest = cmd + 6;
            int i = 0;
            while (rest[i] && rest[i] != ' ') i++;
            if (!rest[i]) { vga_write("Usage: write <file> <content>\n"); }
            else {
                char name[52];
                int j;
                for (j = 0; j < i && j < 50; j++) name[j] = rest[j];
                name[j] = '\0';
                const char *content = rest + i + 1;
                uint32_t len = (uint32_t)strlen(content);

                int idx = tetfs_find(name, cwd_inode);
                if (idx < 0)
                    idx = tetfs_create(name, cwd_inode, TETFS_TYPE_FILE);
                if (idx < 0) { vga_write("Cannot create file.\n"); }
                else if (tetfs_write((uint16_t)idx, content, len) == 0)
                    kprintf("Wrote %u bytes to '%s'.\n", len, name);
                else
                    vga_write("Write failed.\n");
            }
        }
    }
    else if (strncmp(cmd, "rm ", 3) == 0) {
        if (!tetfs_is_mounted()) { vga_write("No filesystem.\n"); }
        else {
            int idx = tetfs_find(cmd + 3, cwd_inode);
            if (idx < 0) { vga_write("Not found.\n"); }
            else if (tetfs_delete((uint16_t)idx) == 0)
                vga_write("Deleted.\n");
            else
                vga_write("Delete failed.\n");
        }
    }
    else if (strcmp(cmd, "about") == 0) {
        vga_write_color("\n=== tetOS v0.1.0 ===\n", VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
        vga_write("A simple operating system inspired by Kasane Teto\n");
        vga_write("Built with love and dedication\n");
        vga_write_color("\"my love, can you teach me to be real?\"\n\n", 
                       VGA_COLOR_LIGHT_MAGENTA, VGA_COLOR_BLACK);
    }
    else if (strcmp(cmd, "teto") == 0) {
        vga_write_color("          .-.          \n", VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        vga_write_color("     .--/      \\--.     \n", VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        vga_write_color("  ( ( ) | ^  ^ | ( ( )   ", VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        vga_write("   < Kasane Teto says hi!\n");
        vga_write_color("  ( ~ ) |   v  | ( ~ )   \n", VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        vga_write_color("     (@)\\______/(@) \n", VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
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
