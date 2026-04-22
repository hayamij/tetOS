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
#include "../graphics/graphics.h"
#include "../syscall/syscall.h"
#include "../user/usermode.h"
#include "../user/appseed.h"
#include "../graphics/bmp.h"

#define COMMAND_BUFFER_SIZE 256
#define SHELL_HISTORY_SIZE 32

static char     command_buffer[COMMAND_BUFFER_SIZE];
static uint32_t command_len = 0;
static uint32_t command_cursor = 0;
static uint32_t command_view = 0;
static uint32_t prompt_row = 1;

static char command_history[SHELL_HISTORY_SIZE][COMMAND_BUFFER_SIZE];
static uint32_t history_count = 0;
static int history_index = -1;
static char history_saved_current[COMMAND_BUFFER_SIZE];
static uint8_t history_saved_valid = 0;

static uint16_t cwd_inode = TETFS_ROOT_INODE;
static char     cwd_path[128] = "/";

static void shell_print_prompt(void);
static void shell_update_history_banner(void);

static void shell_write_prompt_prefix(void) {
    vga_write_color("teto", VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_write_color("@", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_write_color("tetOS", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_write_color(":", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_write_color(cwd_path, VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
    vga_write(" $ ");
}

static uint32_t shell_prompt_cols(void) {
    return 14u + (uint32_t)strlen(cwd_path);
}

static void shell_render_input_line(void) {
    uint32_t prompt_cols = shell_prompt_cols();
    uint32_t visible_cols = (prompt_cols < VGA_WIDTH) ? (VGA_WIDTH - prompt_cols) : 1;
    uint32_t i;

    if (command_cursor < command_view) command_view = command_cursor;
    if (command_cursor > command_view + visible_cols) command_view = command_cursor - visible_cols;
    if (command_cursor == command_view + visible_cols) command_view = command_cursor - visible_cols + 1;

    vga_set_cursor(0, prompt_row);
    shell_write_prompt_prefix();

    for (i = 0; i < visible_cols; i++) {
        uint32_t idx = command_view + i;
        if (idx < command_len) vga_putchar(command_buffer[idx]);
        else vga_putchar(' ');
    }

    vga_set_cursor(prompt_cols + (command_cursor - command_view), prompt_row);
}

static void shell_set_input(const char *line) {
    uint32_t i = 0;
    while (line[i] && i < COMMAND_BUFFER_SIZE - 1) {
        command_buffer[i] = line[i];
        i++;
    }
    command_buffer[i] = '\0';
    command_len = i;
    command_cursor = i;
    command_view = 0;
    shell_render_input_line();
}

static void shell_history_push(const char *cmd) {
    if (!cmd || !cmd[0]) return;
    if (history_count > 0 && strcmp(command_history[history_count - 1], cmd) == 0) return;

    if (history_count < SHELL_HISTORY_SIZE) {
        strcpy(command_history[history_count], cmd);
        history_count++;
    } else {
        uint32_t i;
        for (i = 1; i < SHELL_HISTORY_SIZE; i++) {
            strcpy(command_history[i - 1], command_history[i]);
        }
        strcpy(command_history[SHELL_HISTORY_SIZE - 1], cmd);
    }

    shell_update_history_banner();
}

static void shell_history_up(void) {
    if (history_count == 0) return;

    if (!history_saved_valid) {
        strcpy(history_saved_current, command_buffer);
        history_saved_valid = 1;
    }

    if (history_index < 0) history_index = (int)history_count - 1;
    else if (history_index > 0) history_index--;

    shell_set_input(command_history[history_index]);
}

static void shell_history_down(void) {
    if (history_index < 0) return;

    if (history_index < (int)history_count - 1) {
        history_index++;
        shell_set_input(command_history[history_index]);
        return;
    }

    history_index = -1;
    if (history_saved_valid) shell_set_input(history_saved_current);
    history_saved_valid = 0;
}

static void shell_help_topic(const char *topic) {
    if (!topic || topic[0] == '\0' || strcmp(topic, "all") == 0) {
        vga_write("Help topics:\n");
        vga_write("  help core  - System info, display, basic shell\n");
        vga_write("  help fs    - Filesystem create/read/write/delete\n");
        vga_write("  help proc  - Process, user-mode, and ELF execution\n");
        vga_write("  help bmp   - Bitmap inspection and rendering\n");
        vga_write("  help keys  - Input editor, history, and shortcuts\n");
        return;
    }

    if (strcmp(topic, "core") == 0) {
        vga_write("Core commands:\n");
        vga_write("  help [topic]        Show help topic (core/fs/proc/bmp/keys)\n");
        vga_write("  clear               Clear screen and redraw prompt\n");
        vga_write("  echo <text>         Print text exactly as typed\n");
        vga_write("  uptime              Show running time (hh:mm:ss)\n");
        vga_write("  mem                 Show PMM and kernel heap usage\n");
        vga_write("  disk                Show ATA model, sectors, and size\n");
        vga_write("  about               Show project info\n");
        vga_write("  teto                Show teto ascii art\n");
        return;
    }

    if (strcmp(topic, "fs") == 0) {
        vga_write("Filesystem commands:\n");
        vga_write("  format              Create fresh tetFS layout on disk\n");
        vga_write("  seed                Install bundled files (hello.elf, demo.bmp)\n");
        vga_write("  ls                  List files in current directory\n");
        vga_write("  cd <dir>            Change directory\n");
        vga_write("  pwd                 Print current path\n");
        vga_write("  touch <file>        Create empty file\n");
        vga_write("  mkdir <dir>         Create directory\n");
        vga_write("  cat <file>          Print file content\n");
        vga_write("  write <f> <text>    Overwrite file with text\n");
        vga_write("  rm <name>           Delete file or directory\n");
        return;
    }

    if (strcmp(topic, "proc") == 0) {
        vga_write("Process/exec commands:\n");
        vga_write("  ps                  List process table\n");
        vga_write("  spawn               Start kernel demo process\n");
        vga_write("  kill <pid>          Kill process by PID\n");
        vga_write("  utest               Spawn ring3 syscall demo\n");
        vga_write("  run <file.elf>      Execute ELF from tetFS\n");
        return;
    }

    if (strcmp(topic, "bmp") == 0) {
        vga_write("Bitmap commands:\n");
        vga_write("  bmpinfo <file.bmp>  Show width/height/bpp/compression\n");
        vga_write("  bmpview <file.bmp>  Render BMP to graphics buffer\n");
        vga_write("  Supported: uncompressed 8-bit and 24-bit BMP\n");
        return;
    }

    if (strcmp(topic, "keys") == 0) {
        vga_write("Keyboard shortcuts:\n");
        vga_write("  Up/Down              Browse command history\n");
        vga_write("  Left/Right/Home/End  Move cursor inside current input\n");
        vga_write("  Backspace/Delete     Delete before/at cursor\n");
        vga_write("  Ctrl+L or F2         Clear screen\n");
        vga_write("  F1                   Open this keys help\n");
        vga_write("  F3                   Load last command into input\n");
        vga_write("  Long input lines auto-scroll horizontally\n");
        vga_write("  Top line shows recent history summary\n");
        return;
    }

    vga_write("Unknown help topic. Use: help core|fs|proc|bmp|keys\n");
}

static void shell_update_history_banner(void) {
    char line[VGA_WIDTH + 1];
    uint32_t cx, cy;
    uint32_t n = 0;
    uint32_t i;

    for (i = 0; i < VGA_WIDTH; i++) line[i] = ' ';
    line[VGA_WIDTH] = '\0';

    line[n++] = 'H'; line[n++] = 'I'; line[n++] = 'S'; line[n++] = 'T';
    line[n++] = ':'; line[n++] = ' ';

    if (history_count == 0) {
        const char *s = "(empty)";
        i = 0;
        while (s[i] && n < VGA_WIDTH) line[n++] = s[i++];
    } else {
        uint32_t start = (history_count > 3) ? (history_count - 3) : 0;
        for (i = start; i < history_count && n < VGA_WIDTH; i++) {
            const char *s = command_history[i];
            uint32_t j = 0;
            if (i != start && n + 3 < VGA_WIDTH) {
                line[n++] = ' ';
                line[n++] = '|';
                line[n++] = ' ';
            }
            while (s[j] && n < VGA_WIDTH) line[n++] = s[j++];
        }
    }

    vga_get_cursor(&cx, &cy);
    vga_set_cursor(0, 0);
    vga_write_color(line, VGA_COLOR_BLACK, VGA_COLOR_LIGHT_GREY);
    vga_set_cursor(cx, cy);
}

static void shell_print_bmp_error(const char *prefix, int code) {
    if (code == -5) {
        kprintf("%s: file too small or empty.\n", prefix);
    } else if (code == -7) {
        kprintf("%s: not a BMP file.\n", prefix);
    } else if (code == -10 || code == -11) {
        kprintf("%s: unsupported BMP format (need uncompressed 8-bit or 24-bit).\n", prefix);
    } else {
        kprintf("%s failed (%d).\n", prefix, code);
    }
}

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
    uint32_t x, y;
    vga_get_cursor(&x, &y);
    if (y == 0) {
        vga_set_cursor(0, 1);
        vga_get_cursor(&x, &y);
    }
    if (x != 0) {
        vga_newline();
        vga_get_cursor(&x, &y);
    }
    if (y == 0) {
        vga_newline();
        vga_get_cursor(&x, &y);
    }
    prompt_row = y;
    command_view = 0;
    shell_write_prompt_prefix();
}

static void shell_execute_command(const char* cmd) {
    if (strncmp(cmd, "help", 4) == 0 && (cmd[4] == '\0' || cmd[4] == ' ')) {
        const char *topic = (cmd[4] == ' ') ? cmd + 5 : "";
        shell_help_topic(topic);
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
    else if (strncmp(cmd, "bmpinfo", 7) == 0 && (cmd[7] == '\0' || cmd[7] == ' ')) {
        if (!tetfs_is_mounted()) {
            vga_write("No filesystem mounted. Run 'format' first.\n");
        } else if (cmd[7] == '\0' || cmd[8] == '\0') {
            vga_write("Usage: bmpinfo <file.bmp>\n");
        } else {
            struct bmp_info info;
            int r = bmp_info_from_file(cmd + 8, cwd_inode, &info);
            if (r != 0) {
                shell_print_bmp_error("bmpinfo", r);
            } else {
                kprintf("BMP: %s\n", cmd + 8);
                kprintf("  Size: %ux%u\n", info.width, info.height);
                kprintf("  BPP: %u\n", (uint32_t)info.bpp);
                kprintf("  Compression: %u\n", info.compression);
                kprintf("  Data offset: %u\n", info.data_offset);
                kprintf("  Image size: %u\n", info.image_size);
            }
        }
    }
    else if (strncmp(cmd, "bmpview", 7) == 0 && (cmd[7] == '\0' || cmd[7] == ' ')) {
        if (!tetfs_is_mounted()) {
            vga_write("No filesystem mounted. Run 'format' first.\n");
        } else if (cmd[7] == '\0' || cmd[8] == '\0') {
            vga_write("Usage: bmpview <file.bmp>\n");
        } else {
            struct bmp_info info;
            int ir = bmp_info_from_file(cmd + 8, cwd_inode, &info);
            if (ir != 0) {
                kprintf("bmpview failed (%d).\n", ir);
            } else {
                uint32_t x = 0;
                uint32_t y = 0;
                uint32_t screen_w = graphics_get_width();
                uint32_t screen_h = graphics_get_height();
                if (info.width < screen_w) x = (screen_w - info.width) / 2;
                if (info.height < screen_h) y = (screen_h - info.height) / 2;
                int vr = bmp_draw_from_file(cmd + 8, cwd_inode, x, y);
                if (vr != 0) shell_print_bmp_error("bmpview", vr);
                else kprintf("BMP rendered: %s\n", cmd + 8);
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
    command_len = 0;
    command_cursor = 0;
    command_view = 0;
    prompt_row = 1;
    history_count = 0;
    history_index = -1;
    history_saved_valid = 0;
    memset(command_buffer, 0, COMMAND_BUFFER_SIZE);
    shell_update_history_banner();
}

void shell_run(void) {
    vga_write("\n");
    vga_write_color("Welcome to tetOS Shell!\n", VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
    vga_write("Type 'help' for available commands.\n\n");
    shell_update_history_banner();
    
    shell_print_prompt();
    
    while (1) {
        uint16_t key = keyboard_getkey();

        if (key == KEY_ENTER) {
            vga_write("\n");
            command_buffer[command_len] = '\0';
            shell_history_push(command_buffer);
            shell_execute_command(command_buffer);
            command_len = 0;
            command_cursor = 0;
            command_view = 0;
            history_index = -1;
            history_saved_valid = 0;
            memset(command_buffer, 0, COMMAND_BUFFER_SIZE);
            shell_update_history_banner();
            shell_print_prompt();
        }
        else if (key == KEY_BACKSPACE) {
            if (command_cursor > 0) {
                uint32_t i;
                for (i = command_cursor - 1; i < command_len; i++) {
                    command_buffer[i] = command_buffer[i + 1];
                }
                command_cursor--;
                command_len--;
                shell_render_input_line();
            }
        }
        else if (key == KEY_DELETE) {
            if (command_cursor < command_len) {
                uint32_t i;
                for (i = command_cursor; i < command_len; i++) {
                    command_buffer[i] = command_buffer[i + 1];
                }
                command_len--;
                shell_render_input_line();
            }
        }
        else if (key == KEY_LEFT) {
            if (command_cursor > 0) {
                command_cursor--;
                shell_render_input_line();
            }
        }
        else if (key == KEY_RIGHT) {
            if (command_cursor < command_len) {
                command_cursor++;
                shell_render_input_line();
            }
        }
        else if (key == KEY_HOME) {
            command_cursor = 0;
            shell_render_input_line();
        }
        else if (key == KEY_END) {
            command_cursor = command_len;
            shell_render_input_line();
        }
        else if (key == KEY_UP) {
            shell_history_up();
        }
        else if (key == KEY_DOWN) {
            shell_history_down();
        }
        else if (key == KEY_F1) {
            vga_write("\n");
            shell_help_topic("keys");
            shell_print_prompt();
            shell_render_input_line();
        }
        else if (key == KEY_F2) {
            vga_clear();
            shell_update_history_banner();
            shell_print_prompt();
            shell_render_input_line();
        }
        else if (key == KEY_F3) {
            if (history_count > 0) {
                shell_set_input(command_history[history_count - 1]);
            }
        }
        else if (key < 0x100) {
            char c = (char)key;
            if (keyboard_ctrl_down() && (c == 'l' || c == 'L')) {
                vga_clear();
                shell_update_history_banner();
                shell_print_prompt();
                shell_render_input_line();
            } else if (c >= 32 && c <= 126 && command_len < COMMAND_BUFFER_SIZE - 1) {
                uint32_t i;
                for (i = command_len + 1; i > command_cursor; i--) {
                    command_buffer[i] = command_buffer[i - 1];
                }
                command_buffer[command_cursor] = c;
                command_cursor++;
                command_len++;
                command_buffer[command_len] = '\0';
                shell_render_input_line();
                history_index = -1;
                history_saved_valid = 0;
            }
        }
    }
}
