#include "wm.h"
#include "../graphics/graphics.h"
#include "../heap/heap.h"
#include "../mouse/mouse.h"
#include "../string/string.h"

#define WM_TITLE_MAX_LEN   31
#define WM_MIN_WIDTH       120
#define WM_MIN_HEIGHT      80
#define WM_TITLE_HEIGHT    22
#define WM_BORDER_SIZE     1
#define WM_BTN_SIZE        14
#define WM_BTN_MARGIN      4

struct wm_window {
    uint32_t id;
    int32_t  x;
    int32_t  y;
    uint32_t width;
    uint32_t height;
    color_t  bg_color;
    uint8_t  visible;
    uint8_t  dragging;
    uint8_t  maximized;
    int32_t  drag_offset_x;
    int32_t  drag_offset_y;
    int32_t  saved_x;
    int32_t  saved_y;
    uint32_t saved_w;
    uint32_t saved_h;
    char     title[WM_TITLE_MAX_LEN + 1];
    struct wm_window_ops ops;
    void    *user;
    struct wm_window *prev;
    struct wm_window *next;
};

static struct wm_window *wm_head = NULL;
static struct wm_window *wm_tail = NULL;
static struct wm_window *wm_focused = NULL;
static struct wm_window *wm_dragging = NULL;
static uint32_t wm_next_id = 1;

static color_t wm_border_color = 0;
static color_t wm_title_color = 0;
static color_t wm_title_color_inactive = 0;
static color_t wm_title_text = 0;
static color_t wm_btn_close = 0;
static color_t wm_btn_max = 0;
static color_t wm_btn_glyph = 0;

static uint32_t wm_screen_w = 0;
static uint32_t wm_screen_h = 0;
static uint32_t wm_reserved_bottom = 0;
static uint8_t  wm_last_buttons = 0;

static void wm_refresh_screen(void) {
    wm_screen_w = graphics_get_width();
    wm_screen_h = graphics_get_height();
}

static int32_t wm_clamp_i32(int32_t v, int32_t lo, int32_t hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static uint32_t wm_clamp_u32(uint32_t v, uint32_t lo, uint32_t hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static uint32_t wm_work_h(void) {
    if (wm_reserved_bottom && wm_screen_h > wm_reserved_bottom) {
        return wm_screen_h - wm_reserved_bottom;
    }
    return wm_screen_h;
}

static void wm_copy_title(struct wm_window *win, const char *title) {
    if (!win) return;
    if (!title) { win->title[0] = '\0'; return; }
    size_t len = strlen(title);
    if (len > WM_TITLE_MAX_LEN) len = WM_TITLE_MAX_LEN;
    for (size_t i = 0; i < len; i++) win->title[i] = title[i];
    win->title[len] = '\0';
}

static void wm_list_detach(struct wm_window *win) {
    if (!win) return;
    if (win->prev) win->prev->next = win->next; else wm_head = win->next;
    if (win->next) win->next->prev = win->prev; else wm_tail = win->prev;
    win->prev = NULL;
    win->next = NULL;
}

static void wm_list_attach_tail(struct wm_window *win) {
    if (!win) return;
    win->prev = wm_tail;
    win->next = NULL;
    if (wm_tail) wm_tail->next = win; else wm_head = win;
    wm_tail = win;
}

static void wm_focus_internal(struct wm_window *win) {
    if (!win || win == wm_tail) {
        wm_focused = win;
        return;
    }
    wm_list_detach(win);
    wm_list_attach_tail(win);
    wm_focused = win;
}

static int wm_point_in_window(const struct wm_window *win, int32_t x, int32_t y) {
    if (!win || !win->visible) return 0;
    if (x < win->x || y < win->y) return 0;
    if (x >= win->x + (int32_t)win->width) return 0;
    if (y >= win->y + (int32_t)win->height) return 0;
    return 1;
}

static int wm_point_in_title(const struct wm_window *win, int32_t x, int32_t y) {
    if (!wm_point_in_window(win, x, y)) return 0;
    return y < win->y + WM_TITLE_HEIGHT;
}

static void wm_button_rects(const struct wm_window *win,
                            int32_t *close_x, int32_t *max_x, int32_t *btn_y) {
    int32_t right = win->x + (int32_t)win->width - WM_BTN_MARGIN;
    *close_x = right - WM_BTN_SIZE;
    *max_x   = *close_x - WM_BTN_SIZE - WM_BTN_MARGIN;
    *btn_y   = win->y + (WM_TITLE_HEIGHT - WM_BTN_SIZE) / 2;
}

static int wm_hit_point(int32_t px, int32_t py, int32_t rx, int32_t ry, int32_t rw, int32_t rh) {
    return (px >= rx && py >= ry && px < rx + rw && py < ry + rh);
}

static int wm_hit_close(const struct wm_window *win, int32_t x, int32_t y) {
    int32_t cx, mx, by;
    wm_button_rects(win, &cx, &mx, &by);
    return wm_hit_point(x, y, cx, by, WM_BTN_SIZE, WM_BTN_SIZE);
}

static int wm_hit_max(const struct wm_window *win, int32_t x, int32_t y) {
    int32_t cx, mx, by;
    wm_button_rects(win, &cx, &mx, &by);
    return wm_hit_point(x, y, mx, by, WM_BTN_SIZE, WM_BTN_SIZE);
}

static struct wm_window *wm_find_topmost(int32_t x, int32_t y) {
    struct wm_window *win = wm_tail;
    while (win) {
        if (wm_point_in_window(win, x, y)) return win;
        win = win->prev;
    }
    return NULL;
}

static void wm_apply_bounds(struct wm_window *win) {
    if (!win) return;
    uint32_t work_h = wm_work_h();
    uint32_t min_w = WM_MIN_WIDTH;
    uint32_t min_h = WM_MIN_HEIGHT;
    if (wm_screen_w && min_w > wm_screen_w) min_w = wm_screen_w;
    if (work_h && min_h > work_h) min_h = work_h;

    win->width  = wm_clamp_u32(win->width, min_w, wm_screen_w ? wm_screen_w : win->width);
    win->height = wm_clamp_u32(win->height, min_h, work_h ? work_h : win->height);

    if (wm_screen_w) {
        int32_t max_x = (int32_t)wm_screen_w - (int32_t)win->width;
        win->x = wm_clamp_i32(win->x, 0, max_x < 0 ? 0 : max_x);
    }
    if (work_h) {
        int32_t max_y = (int32_t)work_h - (int32_t)win->height;
        win->y = wm_clamp_i32(win->y, 0, max_y < 0 ? 0 : max_y);
    }
}

static void wm_draw_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, color_t color) {
    if (!w || !h) return;
    for (uint32_t i = 0; i < w; i++) {
        graphics_set_pixel(x + i, y, color);
        graphics_set_pixel(x + i, y + h - 1, color);
    }
    for (uint32_t i = 0; i < h; i++) {
        graphics_set_pixel(x, y + i, color);
        graphics_set_pixel(x + w - 1, y + i, color);
    }
}

static void wm_draw_title_button(int32_t x, int32_t y, color_t bg, char glyph) {
    graphics_fill_rect((uint32_t)x, (uint32_t)y, WM_BTN_SIZE, WM_BTN_SIZE, bg);
    wm_draw_rect((uint32_t)x, (uint32_t)y, WM_BTN_SIZE, WM_BTN_SIZE, graphics_rgb(20, 24, 36));
    char s[2] = { glyph, 0 };
    graphics_draw_string((uint32_t)x + 3, (uint32_t)y + 3, s, wm_btn_glyph, bg);
}

static void wm_draw_window(const struct wm_window *win, int focused) {
    if (!win || !win->visible) return;
    uint32_t x = (uint32_t)win->x;
    uint32_t y = (uint32_t)win->y;
    uint32_t w = win->width;
    uint32_t h = win->height;

    graphics_fill_rect(x, y, w, h, win->bg_color);
    color_t tc = focused ? wm_title_color : wm_title_color_inactive;
    graphics_fill_rect(x, y, w, WM_TITLE_HEIGHT, tc);
    wm_draw_rect(x, y, w, h, wm_border_color);
    graphics_fill_rect(x, y + WM_TITLE_HEIGHT, w, 1, wm_border_color);

    if (win->title[0]) {
        graphics_draw_string(x + 8, y + 7, win->title, wm_title_text, tc);
    }

    int32_t cx, mx, by;
    wm_button_rects(win, &cx, &mx, &by);
    wm_draw_title_button(mx, by, wm_btn_max,   win->maximized ? 'r' : '+');
    wm_draw_title_button(cx, by, wm_btn_close, 'x');

    if (win->ops.draw) {
        uint32_t cxu, cyu, cw, ch;
        wm_get_client_rect((struct wm_window *)win, &cxu, &cyu, &cw, &ch);
        win->ops.draw((struct wm_window *)win, cxu, cyu, cw, ch, win->user);
    }
}

void wm_init(void) {
    wm_head = NULL;
    wm_tail = NULL;
    wm_focused = NULL;
    wm_dragging = NULL;
    wm_next_id = 1;
    wm_last_buttons = 0;
    wm_reserved_bottom = 0;

    wm_refresh_screen();
    wm_border_color = graphics_rgb(18, 22, 32);
    wm_title_color = graphics_rgb(40, 56, 92);
    wm_title_color_inactive = graphics_rgb(46, 52, 66);
    wm_title_text = graphics_rgb(230, 230, 240);
    wm_btn_close = graphics_rgb(210, 80, 80);
    wm_btn_max   = graphics_rgb(80, 140, 200);
    wm_btn_glyph = graphics_rgb(245, 245, 245);
}

struct wm_window *wm_create_window(int32_t x, int32_t y, uint32_t width, uint32_t height, const char *title) {
    struct wm_window *win = (struct wm_window *)kmalloc(sizeof(struct wm_window));
    if (!win) return NULL;
    memset(win, 0, sizeof(struct wm_window));

    win->id = wm_next_id++;
    win->x = x;
    win->y = y;
    win->width  = width  ? width  : WM_MIN_WIDTH;
    win->height = height ? height : WM_MIN_HEIGHT;
    win->bg_color = graphics_rgb(40, 44, 58);
    win->visible = 1;
    wm_copy_title(win, title);

    wm_refresh_screen();
    wm_apply_bounds(win);
    wm_list_attach_tail(win);
    wm_focus_internal(win);
    return win;
}

void wm_destroy_window(struct wm_window *win) {
    if (!win) return;
    if (wm_dragging == win) wm_dragging = NULL;
    if (wm_focused == win) wm_focused = NULL;
    wm_list_detach(win);
    kfree(win);
}

void wm_show_window(struct wm_window *win) { if (win) win->visible = 1; }
void wm_hide_window(struct wm_window *win) {
    if (!win) return;
    win->visible = 0;
    if (wm_focused == win) wm_focused = NULL;
    if (wm_dragging == win) wm_dragging = NULL;
}
int wm_is_visible(const struct wm_window *win) { return win ? win->visible : 0; }

void wm_set_title(struct wm_window *win, const char *title) { wm_copy_title(win, title); }

void wm_set_position(struct wm_window *win, int32_t x, int32_t y) {
    if (!win) return;
    win->x = x;
    win->y = y;
    wm_refresh_screen();
    wm_apply_bounds(win);
}

void wm_set_size(struct wm_window *win, uint32_t width, uint32_t height) {
    if (!win) return;
    win->width = width;
    win->height = height;
    wm_refresh_screen();
    wm_apply_bounds(win);
}

void wm_set_background(struct wm_window *win, color_t color) {
    if (win) win->bg_color = color;
}

void wm_set_ops(struct wm_window *win, const struct wm_window_ops *ops, void *user) {
    if (!win) return;
    if (ops) win->ops = *ops;
    else memset(&win->ops, 0, sizeof(win->ops));
    win->user = user;
}

void wm_focus_window(struct wm_window *win) { wm_focus_internal(win); }
struct wm_window *wm_get_focused(void) { return wm_focused; }

void wm_set_reserved_bottom(uint32_t height) {
    wm_reserved_bottom = height;
    wm_refresh_screen();
    struct wm_window *win = wm_head;
    while (win) { wm_apply_bounds(win); win = win->next; }
}

void wm_get_window_rect(struct wm_window *win, int32_t *x, int32_t *y, uint32_t *width, uint32_t *height) {
    if (!win) return;
    if (x) *x = win->x;
    if (y) *y = win->y;
    if (width)  *width  = win->width;
    if (height) *height = win->height;
}

void wm_get_client_rect(struct wm_window *win, uint32_t *x, uint32_t *y, uint32_t *width, uint32_t *height) {
    if (!win) return;
    uint32_t cx = (uint32_t)win->x + WM_BORDER_SIZE;
    uint32_t cy = (uint32_t)win->y + WM_TITLE_HEIGHT + 1;
    uint32_t cw = (win->width  > 2 * WM_BORDER_SIZE) ? win->width  - 2 * WM_BORDER_SIZE : 0;
    uint32_t ch = (win->height > WM_TITLE_HEIGHT + WM_BORDER_SIZE + 1)
                    ? win->height - WM_TITLE_HEIGHT - WM_BORDER_SIZE - 1 : 0;
    if (x) *x = cx;
    if (y) *y = cy;
    if (width)  *width  = cw;
    if (height) *height = ch;
}

void wm_maximize_toggle(struct wm_window *win) {
    if (!win) return;
    wm_refresh_screen();
    if (!win->maximized) {
        win->saved_x = win->x;
        win->saved_y = win->y;
        win->saved_w = win->width;
        win->saved_h = win->height;
        win->x = 0;
        win->y = 0;
        win->width  = wm_screen_w ? wm_screen_w : win->width;
        win->height = wm_work_h() ? wm_work_h() : win->height;
        win->maximized = 1;
    } else {
        win->x = win->saved_x;
        win->y = win->saved_y;
        win->width  = win->saved_w ? win->saved_w : win->width;
        win->height = win->saved_h ? win->saved_h : win->height;
        win->maximized = 0;
    }
    wm_apply_bounds(win);
}

int wm_is_maximized(const struct wm_window *win) { return win ? win->maximized : 0; }

void wm_deliver_key(uint16_t key) {
    if (wm_focused && wm_focused->visible && wm_focused->ops.key) {
        wm_focused->ops.key(wm_focused, key, wm_focused->user);
    }
}

void wm_update(void) {
    struct mouse_event event;

    while (mouse_read_event(&event)) {
        uint8_t pressed  = (event.buttons & MOUSE_BUTTON_LEFT) && !(wm_last_buttons & MOUSE_BUTTON_LEFT);
        uint8_t released = !(event.buttons & MOUSE_BUTTON_LEFT) && (wm_last_buttons & MOUSE_BUTTON_LEFT);

        if (pressed) {
            struct wm_window *target = wm_find_topmost(event.x, event.y);
            if (target) {
                wm_focus_internal(target);

                if (wm_hit_close(target, event.x, event.y)) {
                    if (target->ops.close) {
                        target->ops.close(target, target->user);
                    } else {
                        target->visible = 0;
                    }
                } else if (wm_hit_max(target, event.x, event.y)) {
                    wm_maximize_toggle(target);
                } else if (wm_point_in_title(target, event.x, event.y)) {
                    if (!target->maximized) {
                        wm_dragging = target;
                        target->dragging = 1;
                        target->drag_offset_x = event.x - target->x;
                        target->drag_offset_y = event.y - target->y;
                    }
                } else if (target->ops.click) {
                    int32_t cx = event.x - target->x - WM_BORDER_SIZE;
                    int32_t cy = event.y - target->y - WM_TITLE_HEIGHT - 1;
                    target->ops.click(target, cx, cy, MOUSE_BUTTON_LEFT, 1, target->user);
                }
            }
        }

        if (released) {
            if (wm_dragging) wm_dragging->dragging = 0;
            wm_dragging = NULL;
            if (wm_focused && wm_focused->ops.click) {
                int32_t cx = event.x - wm_focused->x - WM_BORDER_SIZE;
                int32_t cy = event.y - wm_focused->y - WM_TITLE_HEIGHT - 1;
                wm_focused->ops.click(wm_focused, cx, cy, MOUSE_BUTTON_LEFT, 0, wm_focused->user);
            }
        }

        if (wm_dragging) {
            wm_dragging->x = event.x - wm_dragging->drag_offset_x;
            wm_dragging->y = event.y - wm_dragging->drag_offset_y;
            wm_apply_bounds(wm_dragging);
        }

        wm_last_buttons = event.buttons;
    }
}

void wm_render(void) {
    if (!graphics_get_width() || !graphics_get_height()) return;
    struct wm_window *win = wm_head;
    while (win) {
        wm_draw_window(win, win == wm_focused);
        win = win->next;
    }
}
