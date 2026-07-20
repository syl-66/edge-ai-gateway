/**
 * relay_control.c — 继电器控制实现 (GPIO → 三极管 → 继电器线圈)
 */

#define LOG_TAG "[relay]"

#include "logging.h"
#include "actuator/gpio_util.h"
#include "actuator/relay_control.h"

static int g_relay_pin = -1;
static int g_relay_state = 0;
static int g_relay_ok = 0;

int relay_control_init(int gpio_pin) {
    if (gpio_export_out(gpio_pin) < 0) {
        LOG_WARN("继电器初始化失败, 已禁用 (GPIO%d)", gpio_pin);
        return -1;
    }
    g_relay_pin = gpio_pin;
    g_relay_ok = 1;
    /* 初始拉高, 确保继电器默认断开 (兼容高低电平触发) */
    gpio_write_val(gpio_pin, 1);
    g_relay_state = 1;
    LOG_INFO("继电器就绪 (GPIO%d)", gpio_pin);
    return 0;
}

int device_relay_set(int id, int on) {
    (void)id;
    if (!g_relay_ok) return -1;
    /* 低电平触发: on=1 → 写0(吸合), on=0 → 写1(断开) */
    int level = on ? 0 : 1;
    gpio_write_val(g_relay_pin, level);
    g_relay_state = on;
    return 0;
}

void relay_control_cleanup(void) {
    /* 拉高 GPIO 断开继电器 */
    if (g_relay_ok) { gpio_write_val(g_relay_pin, 1); g_relay_state = 0; }
}
