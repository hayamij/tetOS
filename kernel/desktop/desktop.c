#include "desktop.h"
#include "../graphics/graphics.h"
#include "../mouse/mouse.h"
#include "../wm/wm.h"
#include "../keyboard/keyboard.h"
#include "../timer/timer.h"
#include "../pmm/pmm.h"
#include "../heap/heap.h"
#include "../process/process.h"
#include "../string/string.h"
#include "../io/io.h"
#include "../ata/ata.h"

#define TB_HEIGHT          32
#define TB_BTN_H           22
#define TB_BTN_PAD_Y       5
#define TB_BTN_GAP         4
#define TB_BTN_PAD_X       8

#define APP_COUNT          4

#define TERM_COLS          64
#define TERM_ROWS          20
#define TERM_HISTORY       128

#define EDIT_COLS          60
#define EDIT_ROWS          18
#define EDIT_BUF           (EDIT_COLS * EDIT_ROWS)

enum desktop_app_id {
    APP_TERMINAL = 0,
    APP_EDITOR,
    APP_TASKMGR,
    APP_NEOFETCH
};

struct taskbar_button {
    const char *label;
    uint32_t    width;
    int         app_id;
};

static struct taskbar_button tb_apps[APP_COUNT] = {
    { "Terminal",    96, APP_TERMINAL },
    { "Editor",      76, APP_EDITOR   },
    { "Task Mgr",    88, APP_TASKMGR  },
    { "Neofetch",    86, APP_NEOFETCH },
};

/* --- Theme --- */
static color_t th_bg_top;
static color_t th_bg_bottom;
static color_t th_tb_bg;
static color_t th_tb_top;
static color_t th_tb_text;
static color_t th_tb_btn;
static color_t th_tb_btn_active;
static color_t th_tb_btn_border;
static color_t th_cursor;
static color_t th_start_bg;
static color_t th_start_hover;
static color_t th_start_border;
static color_t th_term_bg;
static color_t th_term_fg;
static color_t th_term_cursor;

/* --- State --- */
static struct wm_window *app_wins[APP_COUNT];
static uint8_t desktop_last_buttons = 0;
static uint8_t start_open = 0;

/* Terminal state */
struct term_line {
    char text[TERM_COLS + 1];
    color_t color;
};
static struct term_line term_lines[TERM_HISTORY];
static uint32_t term_line_count = 0;
static char     term_input[TERM_COLS + 1];
static uint32_t term_input_len = 0;
static uint32_t term_scroll = 0;

/* Editor state */
static char     editor_buf[EDIT_BUF + 1];
static uint32_t editor_len = 0;
static uint32_t editor_cursor = 0;

/* Forward */
static void desktop_open_app(int id);
static void desktop_close_app(int id);
static void term_push(const char *s, color_t c);
static void term_execute(const char *cmd);

/* =====================================================================
 *  Power helpers (QEMU-compatible)
 * ===================================================================== */
static void power_shutdown(void) {
    /* QEMU acpi/isa shutdown ports. */
    outw(0x604, 0x2000);   /* QEMU >= 2.0 */
    outw(0xB004, 0x2000);  /* older QEMU / bochs */
    outw(0x4004, 0x3400);  /* VirtualBox */
    /* If nothing worked, halt. */
    for (;;) __asm__ __volatile__("cli; hlt");
}

static void power_reboot(void) {
    /* Pulse the 8042 reset line. */
    uint8_t good = 0x02;
    while (good & 0x02) good = inb(0x64);
    outb(0x64, 0xFE);
    for (;;) __asm__ __volatile__("cli; hlt");
}

static void power_sleep(void) {
    /* True S3 isn't available; emulate by halting and letting the user
     * wake via any interrupt (keyboard/mouse). */
    __asm__ __volatile__("sti; hlt");
}

/* =====================================================================
 *  Geometry helpers
 * ===================================================================== */
static int hit(int32_t px, int32_t py, int32_t x, int32_t y, int32_t w, int32_t h) {
    return (px >= x && py >= y && px < x + w && py < y + h);
}

static uint32_t screen_w(void) { return graphics_get_width();  }
static uint32_t screen_h(void) { return graphics_get_height(); }

static uint32_t taskbar_y(void) {
    uint32_t h = screen_h();
    return (h > TB_HEIGHT) ? h - TB_HEIGHT : 0;
}

static void start_btn_rect(int32_t *x, int32_t *y, int32_t *w, int32_t *h) {
    *x = TB_BTN_GAP;
    *y = (int32_t)taskbar_y() + TB_BTN_PAD_Y;
    *w = 72;
    *h = TB_BTN_H;
}

static void app_btn_rect(int idx, int32_t *x, int32_t *y, int32_t *w, int32_t *h) {
    int32_t sx, sy, sw, sh;
    start_btn_rect(&sx, &sy, &sw, &sh);
    int32_t cur = sx + sw + TB_BTN_GAP * 2;
    for (int i = 0; i < idx; i++) {
        cur += (int32_t)tb_apps[i].width + TB_BTN_GAP;
    }
    *x = cur;
    *y = sy;
    *w = (int32_t)tb_apps[idx].width;
    *h = sh;
}

static void clock_rect(int32_t *x, int32_t *y, int32_t *w, int32_t *h) {
    *w = 104;
    *h = TB_BTN_H;
    *x = (int32_t)screen_w() - *w - TB_BTN_GAP;
    *y = (int32_t)taskbar_y() + TB_BTN_PAD_Y;
}

#define START_W 180
#define START_ITEM_H 26
static void start_menu_rect(int32_t *x, int32_t *y, int32_t *w, int32_t *h) {
    int32_t sx, sy, sw, sh;
    start_btn_rect(&sx, &sy, &sw, &sh);
    int32_t items = 4; /* Shutdown / Restart / Sleep / Close */
    *w = START_W;
    *h = 10 + items * START_ITEM_H;
    *x = sx;
    *y = sy - *h;
    if (*y < 0) *y = 0;
}

static void start_item_rect(int idx, int32_t *x, int32_t *y, int32_t *w, int32_t *h) {
    int32_t mx, my, mw, mh;
    start_menu_rect(&mx, &my, &mw, &mh);
    *x = mx + 5;
    *y = my + 5 + idx * START_ITEM_H;
    *w = mw - 10;
    *h = START_ITEM_H - 2;
}

/* =====================================================================
 *  Background / Taskbar drawing
 * ===================================================================== */
static void draw_background(void) {
    uint32_t w = screen_w();
    uint32_t h = screen_h();
    if (!w || !h) return;

    int32_t tr = (int32_t)((th_bg_top    >> 16) & 0xFF);
    int32_t tg = (int32_t)((th_bg_top    >>  8) & 0xFF);
    int32_t tb = (int32_t)( th_bg_top           & 0xFF);
    int32_t br = (int32_t)((th_bg_bottom >> 16) & 0xFF);
    int32_t bg_ = (int32_t)((th_bg_bottom >>  8) & 0xFF);
    int32_t bb = (int32_t)( th_bg_bottom        & 0xFF);

    for (uint32_t y = 0; y < h; y++) {
        int32_t t = (int32_t)((y * 255u) / (h ? h : 1));
        int32_t r = tr + (br - tr) * t / 255;
        int32_t g = tg + (bg_ - tg) * t / 255;
        int32_t b = tb + (bb - tb) * t / 255;
        if (r < 0) r = 0; if (r > 255) r = 255;
        if (g < 0) g = 0; if (g > 255) g = 255;
        if (b < 0) b = 0; if (b > 255) b = 255;
        graphics_fill_rect(0, y, w, 1, graphics_rgb((uint8_t)r, (uint8_t)g, (uint8_t)b));
    }

    graphics_draw_string(18, 18, "tetOS Desktop - 1280x720", graphics_rgb(220, 230, 255), graphics_rgb(30, 70, 140));
    graphics_draw_string(18, 34, "\"my love, can you teach me to be real?\"", graphics_rgb(170, 200, 240), graphics_rgb(28, 64, 130));
}

static void draw_button(int32_t x, int32_t y, int32_t w, int32_t h,
                        color_t bg, color_t border, const char *label,
                        color_t fg, int centered) {
    if (w <= 0 || h <= 0) return;
    graphics_fill_rect((uint32_t)x, (uint32_t)y, (uint32_t)w, (uint32_t)h, bg);
    graphics_fill_rect((uint32_t)x, (uint32_t)y, (uint32_t)w, 1, border);
    graphics_fill_rect((uint32_t)x, (uint32_t)y + h - 1, (uint32_t)w, 1, border);
    graphics_fill_rect((uint32_t)x, (uint32_t)y, 1, (uint32_t)h, border);
    graphics_fill_rect((uint32_t)x + w - 1, (uint32_t)y, 1, (uint32_t)h, border);

    if (label) {
        uint32_t text_w = (uint32_t)strlen(label) * 8;
        uint32_t tx = (uint32_t)x + 8;
        uint32_t ty = (uint32_t)y + (uint32_t)((h - 8) / 2);
        if (centered && text_w + 16 < (uint32_t)w) {
            tx = (uint32_t)x + ((uint32_t)w - text_w) / 2;
        }
        graphics_draw_string(tx, ty, label, fg, bg);
    }
}

static void format_clock(char *out) {
    uint32_t secs = timer_ticks() / 100;
    uint32_t h = (secs / 3600) % 24;
    uint32_t m = (secs / 60) % 60;
    uint32_t s = secs % 60;
    out[0] = '0' + (char)((h / 10) % 10);
    out[1] = '0' + (char)(h % 10);
    out[2] = ':';
    out[3] = '0' + (char)((m / 10) % 10);
    out[4] = '0' + (char)(m % 10);
    out[5] = ':';
    out[6] = '0' + (char)((s / 10) % 10);
    out[7] = '0' + (char)(s % 10);
    out[8] = '\0';
}

static void draw_taskbar(void) {
    uint32_t w = screen_w();
    uint32_t h = screen_h();
    if (!w || !h) return;

    uint32_t y = taskbar_y();
    graphics_fill_rect(0, y, w, TB_HEIGHT, th_tb_bg);
    graphics_fill_rect(0, y, w, 2, th_tb_top);

    int32_t x, yy, bw, bh;
    start_btn_rect(&x, &yy, &bw, &bh);
    color_t sbg = start_open ? th_tb_btn_active : th_tb_btn;
    draw_button(x, yy, bw, bh, sbg, th_tb_btn_border, "tetOS", th_tb_text, 1);

    for (int i = 0; i < APP_COUNT; i++) {
        app_btn_rect(i, &x, &yy, &bw, &bh);
        int open = app_wins[i] && wm_is_visible(app_wins[i]);
        color_t bg = open ? th_tb_btn_active : th_tb_btn;
        draw_button(x, yy, bw, bh, bg, th_tb_btn_border, tb_apps[i].label, th_tb_text, 1);
    }

    clock_rect(&x, &yy, &bw, &bh);
    char buf[16];
    format_clock(buf);
    draw_button(x, yy, bw, bh, th_tb_btn, th_tb_btn_border, buf, th_tb_text, 1);
}

static void draw_start_menu(void) {
    if (!start_open) return;
    int32_t mx, my, mw, mh;
    start_menu_rect(&mx, &my, &mw, &mh);

    graphics_fill_rect((uint32_t)mx, (uint32_t)my, (uint32_t)mw, (uint32_t)mh, th_start_bg);
    graphics_fill_rect((uint32_t)mx, (uint32_t)my, (uint32_t)mw, 1, th_start_border);
    graphics_fill_rect((uint32_t)mx, (uint32_t)(my + mh - 1), (uint32_t)mw, 1, th_start_border);
    graphics_fill_rect((uint32_t)mx, (uint32_t)my, 1, (uint32_t)mh, th_start_border);
    graphics_fill_rect((uint32_t)(mx + mw - 1), (uint32_t)my, 1, (uint32_t)mh, th_start_border);

    const char *labels[4] = { "Shutdown", "Restart", "Sleep", "Close menu" };
    int32_t mouse_x = 0, mouse_y = 0;
    mouse_get_position(&mouse_x, &mouse_y);

    for (int i = 0; i < 4; i++) {
        int32_t ix, iy, iw, ih;
        start_item_rect(i, &ix, &iy, &iw, &ih);
        int hover = hit(mouse_x, mouse_y, ix, iy, iw, ih);
        color_t bg = hover ? th_start_hover : th_start_bg;
        graphics_fill_rect((uint32_t)ix, (uint32_t)iy, (uint32_t)iw, (uint32_t)ih, bg);
        graphics_draw_string((uint32_t)ix + 10, (uint32_t)iy + 9, labels[i], th_tb_text, bg);
    }
}

static void draw_cursor(void) {
    int32_t x = 0, y = 0;
    mouse_get_position(&x, &y);
    for (int32_t i = 0; i < 12; i++) {
        graphics_set_pixel((uint32_t)x + (uint32_t)i, (uint32_t)y, th_cursor);
        graphics_set_pixel((uint32_t)x, (uint32_t)y + (uint32_t)i, th_cursor);
    }
    for (int32_t i = 0; i < 6; i++) {
        graphics_set_pixel((uint32_t)x + (uint32_t)i, (uint32_t)y + (uint32_t)i, th_cursor);
    }
}

/* =====================================================================
 *  Terminal app
 * ===================================================================== */
static void u32_to_str(uint32_t v, char *out) {
    if (v == 0) { out[0] = '0'; out[1] = 0; return; }
    char tmp[12];
    int n = 0;
    while (v && n < 11) { tmp[n++] = '0' + (char)(v % 10); v /= 10; }
    int i = 0;
    while (n > 0) out[i++] = tmp[--n];
    out[i] = 0;
}

static void term_push(const char *s, color_t c) {
    if (!s) return;

    if (term_line_count == TERM_HISTORY) {
        for (uint32_t i = 1; i < TERM_HISTORY; i++) term_lines[i - 1] = term_lines[i];
        term_line_count--;
    }
    struct term_line *line = &term_lines[term_line_count++];
    line->color = c;

    uint32_t n = 0;
    while (s[n] && n < TERM_COLS) { line->text[n] = s[n]; n++; }
    line->text[n] = 0;
}

static void term_println(const char *s, color_t c) { term_push(s, c); }

static void term_cmd_help(void) {
    term_println("tetOS in-desktop shell", graphics_rgb(200, 230, 255));
    term_println("  help       show this message",          th_term_fg);
    term_println("  about      tetOS version and credits",  th_term_fg);
    term_println("  clear      clear the terminal",         th_term_fg);
    term_println("  uptime     running time",               th_term_fg);
    term_println("  mem        memory usage",               th_term_fg);
    term_println("  disk       ATA drive info",             th_term_fg);
    term_println("  ps         list processes",             th_term_fg);
    term_println("  echo ...   repeat text",                th_term_fg);
    term_println("  shutdown   power off",                  th_term_fg);
    term_println("  reboot     restart",                    th_term_fg);
}

static void term_cmd_about(void) {
    term_println("tetOS v0.1.0 - \"my love, can you teach me to be real?\"",
                 graphics_rgb(255, 180, 220));
    term_println("1280x720x32 VESA framebuffer desktop", th_term_fg);
    term_println("Built with NASM + GCC + a lot of love.", th_term_fg);
}

static void term_cmd_uptime(void) {
    uint32_t secs = timer_ticks() / 100;
    char buf[64];
    char n[12];
    int i = 0;
    const char *pre = "uptime: ";
    while (*pre) buf[i++] = *pre++;
    u32_to_str(secs / 3600, n); for (int j = 0; n[j]; j++) buf[i++] = n[j];
    buf[i++] = 'h'; buf[i++] = ' ';
    u32_to_str((secs / 60) % 60, n); for (int j = 0; n[j]; j++) buf[i++] = n[j];
    buf[i++] = 'm'; buf[i++] = ' ';
    u32_to_str(secs % 60, n); for (int j = 0; n[j]; j++) buf[i++] = n[j];
    buf[i++] = 's'; buf[i] = 0;
    term_println(buf, th_term_fg);
}

static void term_cmd_mem(void) {
    uint32_t total = pmm_total_frames();
    uint32_t freef = pmm_free_frames();
    uint32_t used  = total - freef;
    uint32_t heap  = heap_used_bytes();
    char buf[80];
    char n[12];
    int i;

    i = 0; const char *p = "total frames: "; while (*p) buf[i++] = *p++;
    u32_to_str(total, n); for (int j = 0; n[j]; j++) buf[i++] = n[j]; buf[i] = 0;
    term_println(buf, th_term_fg);

    i = 0; p = "used  frames: "; while (*p) buf[i++] = *p++;
    u32_to_str(used, n); for (int j = 0; n[j]; j++) buf[i++] = n[j]; buf[i] = 0;
    term_println(buf, th_term_fg);

    i = 0; p = "free  frames: "; while (*p) buf[i++] = *p++;
    u32_to_str(freef, n); for (int j = 0; n[j]; j++) buf[i++] = n[j]; buf[i] = 0;
    term_println(buf, th_term_fg);

    i = 0; p = "heap used (B): "; while (*p) buf[i++] = *p++;
    u32_to_str(heap, n); for (int j = 0; n[j]; j++) buf[i++] = n[j]; buf[i] = 0;
    term_println(buf, th_term_fg);
}

static void term_cmd_disk(void) {
    ata_drive_t *d = ata_get_drive();
    if (!d || !d->present) { term_println("no ATA disk detected", th_term_fg); return; }
    term_println(d->model, th_term_fg);
    char buf[40]; char n[12]; int i = 0;
    const char *p = "sectors: "; while (*p) buf[i++] = *p++;
    u32_to_str(d->sectors, n); for (int j = 0; n[j]; j++) buf[i++] = n[j]; buf[i] = 0;
    term_println(buf, th_term_fg);
}

static void term_cmd_ps(void) {
    process_t *list[MAX_PROCS];
    int count = process_list(list, MAX_PROCS);
    term_println("PID  STATE     NAME", graphics_rgb(200, 230, 255));
    for (int i = 0; i < count; i++) {
        char buf[80];
        char n[12];
        int k = 0;
        u32_to_str(list[i]->pid, n);
        for (int j = 0; n[j]; j++) buf[k++] = n[j];
        while (k < 5) buf[k++] = ' ';
        const char *st;
        switch (list[i]->state) {
            case PROC_READY:   st = "ready  "; break;
            case PROC_RUNNING: st = "running"; break;
            case PROC_BLOCKED: st = "blocked"; break;
            case PROC_ZOMBIE:  st = "zombie "; break;
            default:           st = "unknown"; break;
        }
        while (*st) buf[k++] = *st++;
        buf[k++] = ' '; buf[k++] = ' ';
        const char *nm = list[i]->name[0] ? list[i]->name : "?";
        while (*nm && k < 63) buf[k++] = *nm++;
        buf[k] = 0;
        term_println(buf, th_term_fg);
    }
    if (count == 0) term_println("(no processes)", th_term_fg);
}

static void term_execute(const char *cmd) {
    char echo[TERM_COLS + 4];
    int k = 0;
    echo[k++] = '$'; echo[k++] = ' ';
    while (cmd[k - 2] && k < TERM_COLS + 2) { echo[k] = cmd[k - 2]; k++; }
    echo[k] = 0;
    term_println(echo, graphics_rgb(140, 255, 180));

    if (cmd[0] == 0) return;

    if (strcmp(cmd, "help") == 0)           { term_cmd_help();    return; }
    if (strcmp(cmd, "about") == 0)          { term_cmd_about();   return; }
    if (strcmp(cmd, "clear") == 0)          { term_line_count = 0; term_scroll = 0; return; }
    if (strcmp(cmd, "uptime") == 0)         { term_cmd_uptime();  return; }
    if (strcmp(cmd, "mem") == 0)            { term_cmd_mem();     return; }
    if (strcmp(cmd, "disk") == 0)           { term_cmd_disk();    return; }
    if (strcmp(cmd, "ps") == 0)             { term_cmd_ps();      return; }
    if (strcmp(cmd, "shutdown") == 0)       { power_shutdown();   return; }
    if (strcmp(cmd, "reboot") == 0)         { power_reboot();     return; }
    if (strcmp(cmd, "sleep") == 0)          { power_sleep();      return; }
    if (strncmp(cmd, "echo ", 5) == 0)      { term_println(cmd + 5, th_term_fg); return; }
    if (strcmp(cmd, "echo") == 0)           { term_println("",    th_term_fg);   return; }

    char buf[TERM_COLS + 1];
    int i = 0;
    const char *p = "unknown command: ";
    while (*p && i < TERM_COLS) buf[i++] = *p++;
    int j = 0;
    while (cmd[j] && i < TERM_COLS) buf[i++] = cmd[j++];
    buf[i] = 0;
    term_println(buf, graphics_rgb(255, 140, 140));
}

static void term_on_draw(struct wm_window *win,
                         uint32_t cx, uint32_t cy, uint32_t cw, uint32_t ch,
                         void *user) {
    (void)win; (void)user;
    graphics_fill_rect(cx, cy, cw, ch, th_term_bg);

    uint32_t rows_visible = ch / 12;
    if (rows_visible == 0) return;
    if (rows_visible > TERM_ROWS) rows_visible = TERM_ROWS;

    uint32_t used = term_line_count;
    uint32_t first = 0;
    if (used + 1 > rows_visible) {
        first = used + 1 - rows_visible;
    }

    for (uint32_t i = 0; i < rows_visible - 1 && first + i < used; i++) {
        const struct term_line *line = &term_lines[first + i];
        graphics_draw_string(cx + 8, cy + 6 + i * 12, line->text, line->color, th_term_bg);
    }

    uint32_t input_row = (used >= rows_visible ? rows_visible - 1 : used);
    uint32_t iy = cy + 6 + input_row * 12;

    graphics_draw_string(cx + 8, iy, "$ ", graphics_rgb(140, 255, 180), th_term_bg);
    graphics_draw_string(cx + 8 + 16, iy, term_input, th_term_fg, th_term_bg);

    if ((timer_ticks() / 50) & 1) {
        uint32_t cur_x = cx + 8 + 16 + term_input_len * 8;
        graphics_fill_rect(cur_x, iy, 8, 10, th_term_cursor);
    }
}

static void term_on_key(struct wm_window *win, uint16_t key, void *user) {
    (void)win; (void)user;
    if (key == KEY_ENTER) {
        term_input[term_input_len] = 0;
        char cmd[TERM_COLS + 1];
        for (uint32_t i = 0; i <= term_input_len; i++) cmd[i] = term_input[i];
        term_input_len = 0;
        term_input[0] = 0;
        term_execute(cmd);
    } else if (key == KEY_BACKSPACE) {
        if (term_input_len) {
            term_input_len--;
            term_input[term_input_len] = 0;
        }
    } else if (key < 0x80 && key >= 32) {
        if (term_input_len < TERM_COLS - 1) {
            term_input[term_input_len++] = (char)key;
            term_input[term_input_len] = 0;
        }
    }
}

static void term_on_close(struct wm_window *win, void *user) {
    (void)user;
    wm_hide_window(win);
}

/* =====================================================================
 *  Editor app (simple append-only scratchpad)
 * ===================================================================== */
static void editor_on_draw(struct wm_window *win,
                           uint32_t cx, uint32_t cy, uint32_t cw, uint32_t ch,
                           void *user) {
    (void)win; (void)user;
    graphics_fill_rect(cx, cy, cw, ch, graphics_rgb(28, 30, 40));
    graphics_draw_string(cx + 8, cy + 6, "tetPad - type to edit, Backspace to delete",
                         graphics_rgb(180, 200, 230), graphics_rgb(28, 30, 40));
    graphics_fill_rect(cx + 4, cy + 22, cw - 8, 1, graphics_rgb(60, 70, 90));

    uint32_t col = 0;
    uint32_t row = 0;
    uint32_t text_y0 = cy + 30;
    for (uint32_t i = 0; i < editor_len; i++) {
        char c = editor_buf[i];
        if (c == '\n') { col = 0; row++; continue; }
        if (row >= EDIT_ROWS) break;
        if (col < EDIT_COLS) {
            char s[2] = { c, 0 };
            graphics_draw_string(cx + 8 + col * 8, text_y0 + row * 12, s,
                                 graphics_rgb(220, 230, 240), graphics_rgb(28, 30, 40));
        }
        col++;
        if (col >= EDIT_COLS) { col = 0; row++; }
    }

    if ((timer_ticks() / 50) & 1 && row < EDIT_ROWS) {
        graphics_fill_rect(cx + 8 + col * 8, text_y0 + row * 12, 8, 10,
                           graphics_rgb(255, 230, 100));
    }
}

static void editor_on_key(struct wm_window *win, uint16_t key, void *user) {
    (void)win; (void)user;
    if (key == KEY_BACKSPACE) {
        if (editor_len) { editor_len--; editor_buf[editor_len] = 0; editor_cursor = editor_len; }
    } else if (key == KEY_ENTER) {
        if (editor_len < EDIT_BUF) { editor_buf[editor_len++] = '\n'; editor_cursor = editor_len; editor_buf[editor_len] = 0; }
    } else if (key < 0x80 && key >= 32) {
        if (editor_len < EDIT_BUF) { editor_buf[editor_len++] = (char)key; editor_cursor = editor_len; editor_buf[editor_len] = 0; }
    }
}

static void editor_on_close(struct wm_window *win, void *user) {
    (void)user;
    wm_hide_window(win);
}

/* =====================================================================
 *  Task Manager app
 * ===================================================================== */
static void taskmgr_on_draw(struct wm_window *win,
                            uint32_t cx, uint32_t cy, uint32_t cw, uint32_t ch,
                            void *user) {
    (void)win; (void)user;
    graphics_fill_rect(cx, cy, cw, ch, graphics_rgb(18, 22, 32));
    graphics_draw_string(cx + 8, cy + 6, "Task Manager - kernel processes",
                         graphics_rgb(180, 220, 255), graphics_rgb(18, 22, 32));
    graphics_fill_rect(cx + 4, cy + 22, cw - 8, 1, graphics_rgb(40, 60, 90));

    graphics_draw_string(cx + 8,  cy + 30, "PID",   graphics_rgb(160, 200, 230), graphics_rgb(18, 22, 32));
    graphics_draw_string(cx + 64, cy + 30, "STATE", graphics_rgb(160, 200, 230), graphics_rgb(18, 22, 32));
    graphics_draw_string(cx + 160,cy + 30, "NAME",  graphics_rgb(160, 200, 230), graphics_rgb(18, 22, 32));

    process_t *list[MAX_PROCS];
    int n = process_list(list, MAX_PROCS);
    char buf[12];
    for (int i = 0; i < n; i++) {
        uint32_t ry = cy + 46 + (uint32_t)i * 12;
        u32_to_str(list[i]->pid, buf);
        graphics_draw_string(cx + 8,   ry, buf, graphics_rgb(230, 230, 230), graphics_rgb(18, 22, 32));
        const char *st;
        switch (list[i]->state) {
            case PROC_READY:   st = "ready";   break;
            case PROC_RUNNING: st = "running"; break;
            case PROC_BLOCKED: st = "blocked"; break;
            case PROC_ZOMBIE:  st = "zombie";  break;
            default:           st = "?";       break;
        }
        graphics_draw_string(cx + 64,  ry, st, graphics_rgb(200, 255, 200), graphics_rgb(18, 22, 32));
        const char *nm = list[i]->name[0] ? list[i]->name : "?";
        graphics_draw_string(cx + 160, ry, nm, graphics_rgb(230, 230, 230), graphics_rgb(18, 22, 32));
    }

    char line[64]; int k = 0;
    const char *p = "memory: used="; while (*p) line[k++] = *p++;
    uint32_t used_f = pmm_total_frames() - pmm_free_frames();
    u32_to_str(used_f * 4, buf);
    for (int j = 0; buf[j]; j++) line[k++] = buf[j];
    const char *q = " KB  free="; while (*q) line[k++] = *q++;
    u32_to_str(pmm_free_frames() * 4, buf);
    for (int j = 0; buf[j]; j++) line[k++] = buf[j];
    const char *r = " KB"; while (*r) line[k++] = *r++;
    line[k] = 0;
    graphics_draw_string(cx + 8, cy + ch - 16, line, graphics_rgb(200, 220, 255), graphics_rgb(18, 22, 32));
}

static void taskmgr_on_close(struct wm_window *win, void *user) {
    (void)user;
    wm_hide_window(win);
}

/* =====================================================================
 *  Neofetch app
 * ===================================================================== */
static const char *neofetch_teto_art[] = {
    "       .-.",
    "   .--/   \\--.",
    " ( ( ) | ^ ^ | ( ) )",
    " ( ~ ) |  v  | ( ~ )",
    "    (@)\\___/(@)",
    "        | |",
    "       / | \\",
    NULL
};

static void neofetch_on_draw(struct wm_window *win,
                             uint32_t cx, uint32_t cy, uint32_t cw, uint32_t ch,
                             void *user) {
    (void)win; (void)user;
    graphics_fill_rect(cx, cy, cw, ch, graphics_rgb(22, 22, 34));

    color_t pink = graphics_rgb(255, 150, 200);
    color_t label = graphics_rgb(160, 255, 200);
    color_t value = graphics_rgb(230, 230, 230);
    color_t title = graphics_rgb(255, 220, 230);
    color_t bg = graphics_rgb(22, 22, 34);

    for (int i = 0; neofetch_teto_art[i]; i++) {
        graphics_draw_string(cx + 12, cy + 16 + (uint32_t)i * 12,
                             neofetch_teto_art[i], pink, bg);
    }

    uint32_t info_x = cx + 220;
    uint32_t info_y = cy + 16;
    graphics_draw_string(info_x, info_y, "teto@tetOS", title, bg);
    info_y += 14;
    graphics_draw_string(info_x, info_y, "----------", title, bg);
    info_y += 14;

    char line[80];
    char num[12];
    int k;

    graphics_draw_string(info_x, info_y, "OS:       tetOS 0.1.0", value, bg);
    graphics_draw_string(info_x, info_y, "OS:       ",            label, bg);
    info_y += 14;

    graphics_draw_string(info_x, info_y, "Kernel:   C + NASM, i386 PM", value, bg);
    graphics_draw_string(info_x, info_y, "Kernel:   ",                   label, bg);
    info_y += 14;

    k = 0;
    const char *rp = "Resolution:";
    while (*rp) line[k++] = *rp++;
    while (k < 10) line[k++] = ' ';
    u32_to_str(screen_w(), num); for (int i = 0; num[i]; i++) line[k++] = num[i];
    line[k++] = 'x';
    u32_to_str(screen_h(), num); for (int i = 0; num[i]; i++) line[k++] = num[i];
    line[k++] = 'x'; line[k++] = '3'; line[k++] = '2';
    line[k] = 0;
    graphics_draw_string(info_x, info_y, line, value, bg);
    graphics_draw_string(info_x, info_y, "Resolution:", label, bg);
    info_y += 14;

    uint32_t secs = timer_ticks() / 100;
    k = 0;
    const char *up = "Uptime:";
    while (*up) line[k++] = *up++;
    while (k < 10) line[k++] = ' ';
    u32_to_str(secs / 60, num); for (int i = 0; num[i]; i++) line[k++] = num[i];
    line[k++] = 'm'; line[k++] = ' ';
    u32_to_str(secs % 60, num); for (int i = 0; num[i]; i++) line[k++] = num[i];
    line[k++] = 's'; line[k] = 0;
    graphics_draw_string(info_x, info_y, line, value, bg);
    graphics_draw_string(info_x, info_y, "Uptime:", label, bg);
    info_y += 14;

    k = 0;
    const char *mp = "Memory:";
    while (*mp) line[k++] = *mp++;
    while (k < 10) line[k++] = ' ';
    u32_to_str((pmm_total_frames() - pmm_free_frames()) * 4, num);
    for (int i = 0; num[i]; i++) line[k++] = num[i];
    const char *slash = " / ";
    while (*slash) line[k++] = *slash++;
    u32_to_str(pmm_total_frames() * 4, num);
    for (int i = 0; num[i]; i++) line[k++] = num[i];
    const char *unit = " KB";
    while (*unit) line[k++] = *unit++;
    line[k] = 0;
    graphics_draw_string(info_x, info_y, line, value, bg);
    graphics_draw_string(info_x, info_y, "Memory:", label, bg);
    info_y += 14;

    graphics_draw_string(info_x, info_y, "Shell:    tetOS terminal", value, bg);
    graphics_draw_string(info_x, info_y, "Shell:    ",               label, bg);
    info_y += 14;

    graphics_draw_string(info_x, info_y, "WM:       tetOS native",   value, bg);
    graphics_draw_string(info_x, info_y, "WM:       ",               label, bg);
}

static void neofetch_on_close(struct wm_window *win, void *user) {
    (void)user;
    wm_hide_window(win);
}

/* =====================================================================
 *  App bootstrapping
 * ===================================================================== */
static void desktop_create_app(int id) {
    uint32_t sw = screen_w();
    uint32_t sh = screen_h();
    uint32_t w, h;
    int32_t  x, y;

    switch (id) {
        case APP_TERMINAL: w = 640; h = 360; break;
        case APP_EDITOR:   w = 560; h = 360; break;
        case APP_TASKMGR:  w = 520; h = 320; break;
        case APP_NEOFETCH: w = 620; h = 260; break;
        default: return;
    }

    x = (int32_t)((sw > w) ? (sw - w) / 2 : 0);
    y = (int32_t)((sh > h + TB_HEIGHT + 40) ? (sh - h - TB_HEIGHT - 40) / 2 : 0);
    x += ((int32_t)id - 1) * 30;
    y += ((int32_t)id - 1) * 20;

    const char *title = tb_apps[id].label;
    struct wm_window *win = wm_create_window(x, y, w, h, title);
    if (!win) return;

    struct wm_window_ops ops = { 0 };
    switch (id) {
        case APP_TERMINAL:
            ops.draw  = term_on_draw;
            ops.key   = term_on_key;
            ops.close = term_on_close;
            wm_set_background(win, th_term_bg);
            break;
        case APP_EDITOR:
            ops.draw  = editor_on_draw;
            ops.key   = editor_on_key;
            ops.close = editor_on_close;
            wm_set_background(win, graphics_rgb(28, 30, 40));
            break;
        case APP_TASKMGR:
            ops.draw  = taskmgr_on_draw;
            ops.close = taskmgr_on_close;
            wm_set_background(win, graphics_rgb(18, 22, 32));
            break;
        case APP_NEOFETCH:
            ops.draw  = neofetch_on_draw;
            ops.close = neofetch_on_close;
            wm_set_background(win, graphics_rgb(22, 22, 34));
            break;
    }
    wm_set_ops(win, &ops, NULL);
    app_wins[id] = win;
}

static void desktop_open_app(int id) {
    if (id < 0 || id >= APP_COUNT) return;
    if (!app_wins[id]) desktop_create_app(id);
    if (!app_wins[id]) return;
    if (wm_is_visible(app_wins[id])) {
        wm_focus_window(app_wins[id]);
    } else {
        wm_show_window(app_wins[id]);
        wm_focus_window(app_wins[id]);
    }
}

static void desktop_close_app(int id) {
    if (id < 0 || id >= APP_COUNT) return;
    if (app_wins[id]) wm_hide_window(app_wins[id]);
}

static void desktop_toggle_app(int id) {
    if (id < 0 || id >= APP_COUNT) return;
    if (app_wins[id] && wm_is_visible(app_wins[id])) desktop_close_app(id);
    else desktop_open_app(id);
}

/* =====================================================================
 *  Init / Update / Render
 * ===================================================================== */
void desktop_init(void) {
    th_bg_top        = graphics_rgb(38, 98, 180);
    th_bg_bottom     = graphics_rgb(10, 26, 68);
    th_tb_bg         = graphics_rgb(18, 22, 36);
    th_tb_top        = graphics_rgb(80, 140, 220);
    th_tb_text       = graphics_rgb(244, 247, 255);
    th_tb_btn        = graphics_rgb(36, 48, 70);
    th_tb_btn_active = graphics_rgb(80, 130, 210);
    th_tb_btn_border = graphics_rgb(100, 140, 200);
    th_cursor        = graphics_rgb(255, 255, 255);
    th_start_bg      = graphics_rgb(26, 30, 44);
    th_start_hover   = graphics_rgb(70, 110, 190);
    th_start_border  = graphics_rgb(110, 150, 220);
    th_term_bg       = graphics_rgb(12, 14, 22);
    th_term_fg       = graphics_rgb(210, 220, 230);
    th_term_cursor   = graphics_rgb(140, 255, 180);

    for (int i = 0; i < APP_COUNT; i++) app_wins[i] = NULL;

    term_line_count = 0;
    term_input_len = 0;
    term_input[0] = 0;
    editor_len = 0;
    editor_cursor = 0;
    editor_buf[0] = 0;

    term_push("tetOS terminal ready. Type 'help'.", graphics_rgb(200, 230, 255));

    wm_set_reserved_bottom(TB_HEIGHT);
    desktop_create_app(APP_TERMINAL);
    wm_focus_window(app_wins[APP_TERMINAL]);

    desktop_last_buttons = 0;
    start_open = 0;
}

static void handle_start_menu_click(int32_t mx, int32_t my) {
    for (int i = 0; i < 4; i++) {
        int32_t ix, iy, iw, ih;
        start_item_rect(i, &ix, &iy, &iw, &ih);
        if (hit(mx, my, ix, iy, iw, ih)) {
            start_open = 0;
            switch (i) {
                case 0: power_shutdown(); break;
                case 1: power_reboot();   break;
                case 2: power_sleep();    break;
                case 3: break;
            }
            return;
        }
    }
    start_open = 0;
}

void desktop_update(void) {
    int32_t mx = 0, my = 0;
    mouse_get_position(&mx, &my);
    uint8_t buttons = mouse_get_buttons();
    uint8_t pressed = (buttons & MOUSE_BUTTON_LEFT) && !(desktop_last_buttons & MOUSE_BUTTON_LEFT);

    while (keyboard_has_input()) {
        uint16_t k = keyboard_getkey();
        wm_deliver_key(k);
    }

    if (pressed) {
        int32_t sx, sy, sw, sh;
        start_btn_rect(&sx, &sy, &sw, &sh);

        if (start_open) {
            handle_start_menu_click(mx, my);
        } else if (hit(mx, my, sx, sy, sw, sh)) {
            start_open = 1;
        } else {
            int handled = 0;
            for (int i = 0; i < APP_COUNT; i++) {
                int32_t bx, by, bw, bh;
                app_btn_rect(i, &bx, &by, &bw, &bh);
                if (hit(mx, my, bx, by, bw, bh)) {
                    desktop_toggle_app(i);
                    handled = 1;
                    break;
                }
            }
            (void)handled;
        }
    }

    desktop_last_buttons = buttons;
    wm_update();
}

void desktop_render(void) {
    if (!screen_w() || !screen_h()) return;
    draw_background();
    wm_render();
    draw_taskbar();
    draw_start_menu();
    draw_cursor();
    graphics_present();
}
