/*
 * Phase D bring-up for Scanner Pocket v2: Sharp LS013B7DH05 memory LCD.
 *
 * Plain Zephyr, no LVGL. Draws a static test card (frame, horizontal
 * stripes, checkerboard) and then bounces a black bar up and down once a
 * second, so a working panel is obvious at a glance and a dead one is too.
 * The sharp,ls0xx driver owns EXTCOMIN (VCOM) and DISP; this file only
 * pushes pixels. Heartbeat on the RTT console.
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <zephyr/sys/printk.h>
#include <string.h>

#define W 144
#define H 168
#define BPL (W / 8)                 /* bytes per line: 18 */

/* Full frame, 1 bpp. With PIXEL_FORMAT_MONO01 a set bit is white (the
 * panel's reflective state); the driver ships it LSB-first as the LCD wants. */
static uint8_t frame[H * BPL];

static void set_px(int x, int y, bool white)
{
    uint8_t *b = &frame[y * BPL + x / 8];
    uint8_t m = 1u << (x % 8);          /* LSB = leftmost pixel on this panel */
    if (white) { *b |= m; } else { *b &= ~m; }
}

static void draw_test_card(void)
{
    memset(frame, 0xff, sizeof(frame));            /* all white */

    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            bool white = true;
            if (x < 2 || x >= W - 2 || y < 2 || y >= H - 2) {
                white = false;                      /* 2 px frame */
            } else if (y < 56) {
                white = ((y / 8) % 2) == 0;         /* 8 px horizontal stripes */
            } else if (y < 112) {
                white = (((x / 8) + (y / 8)) % 2) == 0; /* 8 px checkerboard */
            } else {
                white = ((x / 8) % 2) == 0;         /* 8 px vertical stripes */
            }
            set_px(x, y, white);
        }
    }
}

static int push_lines(const struct device *disp, int y0, int lines)
{
    struct display_buffer_descriptor desc = {
        .buf_size = lines * BPL,
        .width = W,
        .height = lines,
        .pitch = W,
    };
    return display_write(disp, 0, y0, &desc, &frame[y0 * BPL]);
}

int main(void)
{
    const struct device *disp = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));

    if (!device_is_ready(disp)) {
        printk("display not ready\n");
        return -ENODEV;
    }

    struct display_capabilities caps;
    display_get_capabilities(disp, &caps);
    printk("Scanner Pocket v2 phase D: %ux%u, formats 0x%x\n",
           caps.x_resolution, caps.y_resolution, caps.supported_pixel_formats);

    display_set_pixel_format(disp, PIXEL_FORMAT_MONO01);
    draw_test_card();
    int rc = push_lines(disp, 0, H);
    printk("full frame write rc=%d\n", rc);
    display_blanking_off(disp);

    /*
     * Diagnostic loop (panel stayed white on the first try):
     * alternate DISP every 2 s through the blanking API - the ls0xx driver
     * maps blanking_on/off straight onto the DISP GPIO - and rewrite the
     * test card each time DISP goes high.
     *   panel changes every 2 s  -> DISP wiring is right, suspect SCLK/SI/SCS
     *   panel never changes      -> DISP pin (P0.16) or LCD supply
     */
    bool blank = false;
    uint32_t n = 0;
    while (1) {
        k_msleep(2000);
        blank = !blank;
        if (blank) {
            display_blanking_on(disp);          /* DISP low  -> white */
        } else {
            draw_test_card();
            rc = push_lines(disp, 0, H);
            display_blanking_off(disp);         /* DISP high -> show memory */
        }
        printk("tick %u DISP=%s write rc=%d\n", ++n, blank ? "LOW (blank)" : "HIGH (show)", rc);
    }
    return 0;
}
