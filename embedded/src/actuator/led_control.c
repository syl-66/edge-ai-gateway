/**
 * led_control.c — LED 灯控制实现 (GPIO sysfs)
 *
 * 硬件: GPIO → 220Ω 电阻 → LED → GND
 *       高电平亮, 低电平灭
 */

#define LOG_TAG "[led]"

#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include "logging.h"
#include "actuator/gpio_util.h"
#include "actuator/led_control.h"

static int g_led_pin   = -1;
static int g_led_state = 0;
static int g_led_ok    = 0;

int led_control_init(int gpio_pin) {
    if (gpio_export_out(gpio_pin) < 0) {
        LOG_WARN("LED 初始化失败, 已禁用 (GPIO%d)", gpio_pin);
        return -1;
    }
    gpio_write_val(gpio_pin, 0);  /* 初始灭 */
    g_led_pin   = gpio_pin;
    g_led_state = 0;
    g_led_ok    = 1;
    LOG_INFO("LED 就绪 (GPIO%d, sysfs)", gpio_pin);
    return 0;
}

int led_control_set(const char *action) {
    if (!g_led_ok) return -1;

    if (strcmp(action, "on") == 0 || strcmp(action, "1") == 0) {
        gpio_write_val(g_led_pin, 1);
        g_led_state = 1;
    } else if (strcmp(action, "off") == 0 || strcmp(action, "0") == 0) {
        gpio_write_val(g_led_pin, 0);
        g_led_state = 0;
    } else if (strcmp(action, "toggle") == 0) {
        g_led_state = !g_led_state;
        gpio_write_val(g_led_pin, g_led_state);
    } else {
        return -1;
    }

    LOG_INFO("LED: %s (GPIO%d)", g_led_state ? "on" : "off", g_led_pin);
    return 0;
}

int led_control_get_state(void) {
    if (!g_led_ok) return -1;
    return g_led_state;
}

void led_control_cleanup(void) {
    if (g_led_ok) {
        gpio_write_val(g_led_pin, 0);  /* 先灭 */
        g_led_ok    = 0;
        g_led_state = 0;
    }
}
