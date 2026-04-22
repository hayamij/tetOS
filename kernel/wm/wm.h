#ifndef WM_H
#define WM_H

#include "../graphics/graphics.h"
#include "../types/types.h"

struct wm_window;

typedef void (*wm_draw_fn)(struct wm_window *win,
                           uint32_t client_x, uint32_t client_y,
                           uint32_t client_w, uint32_t client_h,
                           void *user);
typedef void (*wm_key_fn)(struct wm_window *win, uint16_t key, void *user);
typedef void (*wm_click_fn)(struct wm_window *win,
                            int32_t client_x, int32_t client_y,
                            uint8_t button, uint8_t pressed,
                            void *user);
typedef void (*wm_close_fn)(struct wm_window *win, void *user);

struct wm_window_ops {
    wm_draw_fn  draw;
    wm_key_fn   key;
    wm_click_fn click;
    wm_close_fn close;
};

void wm_init(void);
struct wm_window *wm_create_window(int32_t x, int32_t y, uint32_t width, uint32_t height, const char *title);
void wm_destroy_window(struct wm_window *win);
void wm_show_window(struct wm_window *win);
void wm_hide_window(struct wm_window *win);
int  wm_is_visible(const struct wm_window *win);
void wm_set_title(struct wm_window *win, const char *title);
void wm_set_position(struct wm_window *win, int32_t x, int32_t y);
void wm_set_size(struct wm_window *win, uint32_t width, uint32_t height);
void wm_set_background(struct wm_window *win, color_t color);
void wm_set_ops(struct wm_window *win, const struct wm_window_ops *ops, void *user);
void wm_focus_window(struct wm_window *win);
struct wm_window *wm_get_focused(void);
void wm_get_window_rect(struct wm_window *win, int32_t *x, int32_t *y, uint32_t *width, uint32_t *height);
void wm_get_client_rect(struct wm_window *win, uint32_t *x, uint32_t *y, uint32_t *width, uint32_t *height);
void wm_set_reserved_bottom(uint32_t height);
void wm_maximize_toggle(struct wm_window *win);
int  wm_is_maximized(const struct wm_window *win);
void wm_deliver_key(uint16_t key);
void wm_update(void);
void wm_render(void);

#endif
