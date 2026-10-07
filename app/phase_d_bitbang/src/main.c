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
#include <zephyr/dt-bindings/gpio/nordic-nrf-gpio.h>

#define P_SCLK 6
#define P_SI   7
#define P_SCS  8
#define P_EXT  12
#define P_DISP 16

#define LINES 168
#define BYTES_PER_LINE 18   /* 144 px */

static const struct device *g;

static unsigned int half_us = 1;   /* clock half-period; 1 ~ 300 kHz, 5 ~ 60 kHz */

static inline void clk_bit(int bit)
{
    gpio_pin_set_raw(g, P_SI, bit);
    k_busy_wait(half_us);
    gpio_pin_set_raw(g, P_SCLK, 1);
    k_busy_wait(half_us);
    gpio_pin_set_raw(g, P_SCLK, 0);
}

static void send_byte_lsb_first(uint8_t b)
{
    for (int i = 0; i < 8; i++) {
        clk_bit((b >> i) & 1);
    }
}

static void send_byte_msb_first(uint8_t b)
{
    for (int i = 7; i >= 0; i--) {
        clk_bit((b >> i) & 1);
    }
}

static bool addr_msb_first;   /* line-address bit order under test */
static unsigned int settle_us; /* pause after each line address before pixel data */
static unsigned int line_gap_ms; /* pause after each full line (lets a sagging rail recover) */

static inline void send_line_addr(uint8_t ln)
{
    if (addr_msb_first) {
        send_byte_msb_first(ln);
    } else {
        send_byte_lsb_first(ln);
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

/*
 * M0=1: write every line. `kind` selects the frame content:
 *   0  solid black
 *   1  horizontal bands: lines 1-8 black, 9-16 white, 17-24 black, ...
 *   2  vertical bands:   byte 0 black (px 1-8), byte 1 white, byte 2 black, ...
 * Pixel bit 1 = white, 0 = black. Data bytes go LSB-first (Zephyr driver order).
 */
static void write_frame(int kind)
{
    scs_begin();
    send_byte_lsb_first(0x01);                 /* M0 write, M1 vcom=0, M2=0 */
    for (int ln = 1; ln <= LINES; ln++) {
        send_line_addr((uint8_t)ln);           /* line address, 1-based */
        if (settle_us) {
            k_busy_wait(settle_us);            /* let SI settle low after the address bits */
        }
        for (int i = 0; i < BYTES_PER_LINE; i++) {
            uint8_t data;
            switch (kind) {
            case 1:  data = (((ln - 1) / 8) % 2) ? 0xff : 0x00; break;
            case 2:  data = (i % 2) ? 0xff : 0x00; break;
            default: data = 0x00; break;
            }
            send_byte_lsb_first(data);
        }
        send_byte_lsb_first(0x00);             /* 8 dummy bits */
        if (line_gap_ms) {
            k_msleep(line_gap_ms);             /* SCS stays high; panel tolerates idle SCLK */
        }
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

    /*
     * Supply-sag test, 6 steps of 3 s:
     *   0: solid black, standard drive                      (baseline = photo)
     *   1: CLEAR
     *   2: solid black, HIGH drive + 50 us settle            (half black last time)
     *   3: CLEAR
     *   4: solid black, HIGH drive + 50 us settle + 2 ms pause after every line
     *   5: CLEAR
     * Step 4 stretches the frame to ~0.4 s with the bus idle most of the time.
     * If the panel's supply sags under write current, the pauses let it
     * recover and step 4 comes out solid black.
     */
    half_us = 1;
    addr_msb_first = false;
    uint32_t n = 0;
    int ext = 0;
    while (1) {
        int step = n % 6;
        if ((step % 2) == 0) {
            int variant = step / 2;
            gpio_flags_t ds = variant ? NRF_GPIO_DRIVE_H0H1 : NRF_GPIO_DRIVE_S0S1;
            gpio_pin_configure(g, P_SCLK, GPIO_OUTPUT_LOW | ds);
            gpio_pin_configure(g, P_SI,   GPIO_OUTPUT_LOW | ds);
            gpio_pin_configure(g, P_SCS,  GPIO_OUTPUT_LOW | ds);
            settle_us = variant ? 50 : 0;
            line_gap_ms = (variant == 2) ? 2 : 0;
            write_frame(0);
            printk("tick %u: SOLID BLACK, %s\n", n,
                   variant == 0 ? "standard drive" :
                   variant == 1 ? "HIGH drive + settle" : "HIGH drive + settle + 2 ms/line gap");
        } else {
            clear_frame();
            printk("tick %u: CLEAR (white)\n", n);
        }
        ext = !ext;
        gpio_pin_set_raw(g, P_EXT, ext);           /* VCOM inversion */
        n++;
        k_msleep(3000);
    }
    return 0;
}
