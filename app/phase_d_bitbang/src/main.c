/*
 * Bit-banged LS013B7DH05 test for Scanner Pocket v2.
 *
 * Pins (KiCad-confirmed): SCLK P0.06, SI P0.07, SCS P0.08 (active HIGH),
 * EXTCOMIN P0.12, DISP P0.16. Protocol per the Sharp memory LCD family:
 *   SCS high, >=6 us, command byte, then per line: 8-bit line number
 *   (1-based), 144 pixel bits, 8 dummy bits; after the last line 16 dummy
 *   bits; >=2 us, SCS low, >=6 us low. Everything LSB-first on the wire,
 *   SI sampled on the rising SCLK edge. Pixel bit 1 = white, 0 = black.
 * Alternates an all-black frame and a CLEAR (all white) every 3 s and
 * toggles EXTCOMIN once per cycle for VCOM (EXTMODE is tied high).
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>

#define P_SCLK 6
#define P_SI   7
#define P_SCS  8
#define P_EXT  12
#define P_DISP 16

#define LINES 168
#define BYTES_PER_LINE 18   /* 144 px */

static const struct device *g;

static inline void clk_bit(int bit)
{
    gpio_pin_set_raw(g, P_SI, bit);
    k_busy_wait(1);
    gpio_pin_set_raw(g, P_SCLK, 1);
    k_busy_wait(1);
    gpio_pin_set_raw(g, P_SCLK, 0);
}

static void send_byte_lsb_first(uint8_t b)
{
    for (int i = 0; i < 8; i++) {
        clk_bit((b >> i) & 1);
    }
}

static void scs_begin(void)
{
    gpio_pin_set_raw(g, P_SCS, 1);
    k_busy_wait(10);               /* tsSCS >= 6 us */
}

static void scs_end(void)
{
    k_busy_wait(4);                /* thSCS >= 2 us */
    gpio_pin_set_raw(g, P_SCS, 0);
    k_busy_wait(10);               /* twSCSL >= 6 us */
}

/* M0=1: write lines. Fill every pixel with `pixel` (0 = black, 1 = white). */
static void write_solid_frame(int pixel)
{
    uint8_t data = pixel ? 0xff : 0x00;
    scs_begin();
    send_byte_lsb_first(0x01);                 /* M0 write, M1 vcom=0, M2=0 */
    for (int ln = 1; ln <= LINES; ln++) {
        send_byte_lsb_first((uint8_t)ln);      /* line address, 1-based */
        for (int i = 0; i < BYTES_PER_LINE; i++) {
            send_byte_lsb_first(data);
        }
        send_byte_lsb_first(0x00);             /* 8 dummy bits */
    }
    send_byte_lsb_first(0x00);                 /* 16 trailing dummy bits */
    send_byte_lsb_first(0x00);
    scs_end();
}

/* M2=1: clear all pixels to white. */
static void clear_frame(void)
{
    scs_begin();
    send_byte_lsb_first(0x04);
    send_byte_lsb_first(0x00);
    scs_end();
}

int main(void)
{
    g = DEVICE_DT_GET(DT_NODELABEL(gpio0));
    if (!device_is_ready(g)) {
        return -ENODEV;
    }
    gpio_pin_configure(g, P_SCLK, GPIO_OUTPUT_LOW);
    gpio_pin_configure(g, P_SI,   GPIO_OUTPUT_LOW);
    gpio_pin_configure(g, P_SCS,  GPIO_OUTPUT_LOW);
    gpio_pin_configure(g, P_EXT,  GPIO_OUTPUT_LOW);
    gpio_pin_configure(g, P_DISP, GPIO_OUTPUT_LOW);

    k_msleep(50);
    clear_frame();
    gpio_pin_set_raw(g, P_DISP, 1);
    printk("Scanner Pocket v2 bit-bang: cleared, DISP high\n");

    uint32_t n = 0;
    int ext = 0;
    while (1) {
        bool black = (n % 2) == 0;
        if (black) {
            write_solid_frame(0);
        } else {
            clear_frame();
        }
        ext = !ext;
        gpio_pin_set_raw(g, P_EXT, ext);           /* VCOM inversion */
        printk("tick %u: %s, EXTCOMIN=%d\n", n, black ? "ALL BLACK (line writes)" : "CLEAR (white)", ext);
        n++;
        k_msleep(3000);
    }
    return 0;
}
