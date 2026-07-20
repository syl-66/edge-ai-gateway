/**
 * status_led.c — 系统状态指示灯实现 (GPIO, libgpiod)
 *
 * 用于指示网关运行状态:
 *   - 常亮: 正常运行
 *   - 慢闪: 传感器异常 (1s 周期)
 *   - 快闪: 网络断开 (200ms 周期)
 *
 * GPIO 操作: libgpiod (字符设备 /dev/gpiochipN)
 */

#define LOG_TAG "[status_led]"

#include "logging.h"
#include "actuator/gpio_util.h"
#include "sensor/status_led.h"
#include "config.h"

static gpio_line_t g_led_line;
static int          g_led_ok = 0;

int status_led_init(void) {
    if (gpio_export_out(STATUS_LED_GPIO, &g_led_line) < 0) {
        LOG_WARN("状态 LED 初始化失败, 已禁用 (GPIO%d)", STATUS_LED_GPIO);
        return -1;
    }
    g_led_ok = 1;
    /* 初始点亮 — 表示设备已上电 */
    gpio_write_val(&g_led_line, 1);
    LOG_INFO("状态 LED 就绪 (GPIO%d, libgpiod)", STATUS_LED_GPIO);
    return 0;
}

int status_led_set(int on) {
    if (!g_led_ok) return -1;
    return gpio_write_val(&g_led_line, on ? 1 : 0);
}
