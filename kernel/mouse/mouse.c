#include "mouse.h"
#include "../graphics/graphics.h"
#include "../io/io.h"
#include "../isr/isr.h"

#define PS2_DATA_PORT 0x60
#define PS2_STATUS_PORT 0x64
#define PS2_CMD_PORT 0x64

#define MOUSE_EVENT_BUFFER_SIZE 64

static volatile uint32_t event_read = 0;
static volatile uint32_t event_write = 0;
static struct mouse_event event_buffer[MOUSE_EVENT_BUFFER_SIZE];

static uint8_t packet[3];
static uint8_t packet_index = 0;
static int32_t mouse_x = 0;
static int32_t mouse_y = 0;
static uint8_t mouse_buttons = 0;

static int ps2_wait_read(void) {
    for (uint32_t i = 0; i < 100000; i++) {
        if (inb(PS2_STATUS_PORT) & 0x01) return 1;
    }
    return 0;
}

static int ps2_wait_write(void) {
    for (uint32_t i = 0; i < 100000; i++) {
        if ((inb(PS2_STATUS_PORT) & 0x02) == 0) return 1;
    }
    return 0;
}

static void ps2_flush_output(void) {
    while (inb(PS2_STATUS_PORT) & 0x01) {
        inb(PS2_DATA_PORT);
    }
}

static void mouse_write(uint8_t data) {
    if (!ps2_wait_write()) return;
    outb(PS2_CMD_PORT, 0xD4);
    if (!ps2_wait_write()) return;
    outb(PS2_DATA_PORT, data);
}

static uint8_t mouse_read(void) {
    if (!ps2_wait_read()) return 0;
    return inb(PS2_DATA_PORT);
}

static void push_event(const struct mouse_event *event) {
    uint32_t next = (event_write + 1) % MOUSE_EVENT_BUFFER_SIZE;
    if (next == event_read) return;
    event_buffer[event_write] = *event;
    event_write = next;
}

static void mouse_handle_packet(void) {
    uint8_t status = packet[0];
    int8_t dx = (int8_t)packet[1];
    int8_t dy = (int8_t)packet[2];

    if (status & 0x40) dx = 0;
    if (status & 0x80) dy = 0;

    mouse_buttons = status & 0x07;
    mouse_x += dx;
    mouse_y -= dy;

    uint32_t screen_w = graphics_get_width();
    uint32_t screen_h = graphics_get_height();
    if (screen_w) {
        if (mouse_x < 0) mouse_x = 0;
        if ((uint32_t)mouse_x >= screen_w) mouse_x = (int32_t)(screen_w - 1);
    }
    if (screen_h) {
        if (mouse_y < 0) mouse_y = 0;
        if ((uint32_t)mouse_y >= screen_h) mouse_y = (int32_t)(screen_h - 1);
    }

    struct mouse_event event = {
        .dx = dx,
        .dy = (int8_t)(-dy),
        .buttons = mouse_buttons,
        .x = mouse_x,
        .y = mouse_y,
    };
    push_event(&event);
}

static void mouse_irq(struct registers *regs) {
    (void)regs;

    uint8_t status = inb(PS2_STATUS_PORT);
    if ((status & 0x20) == 0) return;

    uint8_t data = inb(PS2_DATA_PORT);

    if (packet_index == 0 && (data & 0x08) == 0) {
        return;
    }

    packet[packet_index++] = data;
    if (packet_index < 3) return;
    packet_index = 0;

    mouse_handle_packet();
}

void mouse_init(void) {
    ps2_flush_output();

    if (ps2_wait_write()) outb(PS2_CMD_PORT, 0xA8);

    if (ps2_wait_write()) outb(PS2_CMD_PORT, 0x20);
    uint8_t cmd = mouse_read();
    cmd |= 0x02;
    cmd &= (uint8_t)~0x20;
    if (ps2_wait_write()) outb(PS2_CMD_PORT, 0x60);
    if (ps2_wait_write()) outb(PS2_DATA_PORT, cmd);

    mouse_write(0xF6);
    mouse_read();
    mouse_write(0xF4);
    mouse_read();

    register_irq_handler(12, mouse_irq);

    uint8_t mask = inb(0xA1);
    outb(0xA1, mask & (uint8_t)~0x10);
    mask = inb(0x21);
    outb(0x21, mask & (uint8_t)~0x04);

    uint32_t screen_w = graphics_get_width();
    uint32_t screen_h = graphics_get_height();
    if (screen_w) mouse_x = (int32_t)(screen_w / 2);
    if (screen_h) mouse_y = (int32_t)(screen_h / 2);
}

int mouse_has_event(void) {
    return event_read != event_write;
}

int mouse_read_event(struct mouse_event *event) {
    if (!event) return 0;
    if (event_read == event_write) return 0;
    *event = event_buffer[event_read];
    event_read = (event_read + 1) % MOUSE_EVENT_BUFFER_SIZE;
    return 1;
}

void mouse_get_position(int32_t *x, int32_t *y) {
    if (x) *x = mouse_x;
    if (y) *y = mouse_y;
}

uint8_t mouse_get_buttons(void) {
    return mouse_buttons;
}
