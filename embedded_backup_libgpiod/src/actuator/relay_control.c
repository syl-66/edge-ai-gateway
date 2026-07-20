/**
 * relay_control.c — 继电器控制实现 (GPIO → 三极管 → 继电器线圈)
 *
 * GPIO 操作: libgpiod (字符设备 /dev/gpiochipN)
 */

#define LOG_TAG "[relay]"

#include "logging.h"
#include "actuator/gpio_util.h"
#include "actuator/relay_control.h"

static gpio_line_t g_relay_line;  /* libgpiod 线路句柄 */
static int          g_relay_state = 0;
static int          g_relay_ok    = 0;

int relay_control_init(int gpio_pin) {
    if (gpio_export_out(gpio_pin, &g_relay_line) < 0) {
        LOG_WARN("继电器初始化失败, 已禁用 (GPIO%d)", gpio_pin);
        return -1;
    }
    g_relay_ok = 1;
    LOG_INFO("继电器就绪 (GPIO%d, libgpiod)", gpio_pin);
    return 0;
}

int device_relay_set(int id, int on) {
    (void)id;
    if (!g_relay_ok) return -1;
    gpio_write_val(&g_relay_line, on);
    g_relay_state = on;
    return 0;
}

void relay_control_cleanup(void) {
    if (g_relay_ok) {
        gpio_release(&g_relay_line);  /* libgpiod: 拉低 + 释放线路 + 关闭芯片 */
        g_relay_ok = 0;
        g_relay_state = 0;
        LOG_INFO("继电器已释放");
    }
}
