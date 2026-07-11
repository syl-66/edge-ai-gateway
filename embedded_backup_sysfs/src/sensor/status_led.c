/**
 * status_led.c — 系统状态指示灯实现 (GPIO)
 *
 * 用于指示网关运行状态:
 *   - 常亮: 正常运行
 *   - 慢闪: 传感器异常
 *   - 快闪: 网络断开
 *
 * 当前为占位实现, 实际产品需配置具体 GPIO 并实现闪烁逻辑。
 */

#define LOG_TAG "[status_led]"

#include "sensor/status_led.h"

int status_led_init(void) {
    /* TODO: 初始化 GPIO 引脚为输出模式 */
    return 0;
}

int status_led_set(int on) {
    /* TODO: 设置 GPIO 电平 */
    (void)on;
    return 0;
}
