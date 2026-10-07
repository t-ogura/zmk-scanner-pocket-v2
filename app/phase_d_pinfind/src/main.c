/*
 * DISP finder for Scanner Pocket v2.
 *
 * The HY0020 exposes only these GPIOs (HANDOVER 13.1). Excluding the three
 * lever inputs (P0.18/20/30, verified) and nRESET (P0.21), twelve remain and
 * the five LCD lines must be among them. Each candidate is driven high for
 * 3 s while all others are inputs; the RTT console names the live pin. When
 * the panel stops being white (random memory contents appear), the pin
 * named at that moment is DISP. If nothing ever changes across the whole
 * sweep, the LCD has no supply or its FPC is not making contact.
 *
 * Driving a pin that is really SCLK/SI/SCS/EXTCOMIN high is harmless.
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>

static const uint8_t cand[] = { 16, 12, 8, 7, 6, 2, 3, 4, 5, 9, 10, 28 };

int main(void)
{
    const struct device *g = DEVICE_DT_GET(DT_NODELABEL(gpio0));
    if (!device_is_ready(g)) {
        return -ENODEV;
    }
    for (size_t i = 0; i < ARRAY_SIZE(cand); i++) {
        gpio_pin_configure(g, cand[i], GPIO_INPUT);
    }
    printk("Scanner Pocket v2 DISP finder: %u candidates, 3 s each\n", (unsigned)ARRAY_SIZE(cand));

    for (uint32_t round = 1;; round++) {
        for (size_t i = 0; i < ARRAY_SIZE(cand); i++) {
            gpio_pin_configure(g, cand[i], GPIO_OUTPUT_HIGH);
            printk("round %u: P0.%02u HIGH\n", round, cand[i]);
            k_msleep(3000);
            gpio_pin_configure(g, cand[i], GPIO_INPUT);
            k_msleep(300);
        }
        printk("round %u done - all low for 3 s\n", round);
        k_msleep(3000);
    }
    return 0;
}
