/*
 * Phase A bring-up for Scanner Pocket v2 (HY0020 / nRF52832).
 *
 * Toggles the two LCD control lines that are plain outputs on the finished
 * board - DISP (P0.16) at 1 Hz and EXTCOMIN (P0.12) at 2 Hz - so a meter or
 * scope on either pad proves: SWD flash worked, the reset vector is sane,
 * the clock is running, and GPIO works. Nothing else is touched.
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>

#define PIN_DISP     16  /* P0.16, LCD DISP     (HANDOVER 14.2) */
#define PIN_EXTCOMIN 12  /* P0.12, LCD EXTCOMIN (HANDOVER 14.2) */

int main(void)
{
    const struct device *gpio0 = DEVICE_DT_GET(DT_NODELABEL(gpio0));

    if (!device_is_ready(gpio0)) {
        return -ENODEV;
    }
    gpio_pin_configure(gpio0, PIN_DISP, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure(gpio0, PIN_EXTCOMIN, GPIO_OUTPUT_INACTIVE);

    printk("Scanner Pocket v2 phase A: alive, toggling P0.16 @1Hz / P0.12 @2Hz\n");

    uint32_t tick = 0;
    while (1) {
        k_msleep(250);
        tick++;
        gpio_pin_toggle(gpio0, PIN_EXTCOMIN);          /* every 250 ms -> 2 Hz */
        if ((tick & 1) == 0) {
            gpio_pin_toggle(gpio0, PIN_DISP);          /* every 500 ms -> 1 Hz */
        }
        if ((tick % 8) == 0) {
            printk("tick %u\n", tick / 4);             /* once a second */
        }
    }
    return 0;
}
