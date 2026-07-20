/**
 * device_manager.c — 执行器统一管理 (协调各子模块)
 * 子模块初始化失败自动跳过, 不阻塞整体运行
 */

#define LOG_TAG "[actuator]"

#include "string.h"
#include "logging.h"
#include "config.h"
#include "actuator/device_manager.h"
#include "actuator/relay_control.h"
#include "actuator/led_control.h"
#include "actuator/ir_control.h"
<<<<<<< HEAD
#include "actuator/fan_pwm.h"
=======
>>>>>>> temp-remote

int device_manager_init(void) {
    LOG_INFO("===== 执行器初始化 =====");

    if (relay_control_init(RELAY_FAN_GPIO) < 0)
        LOG_WARN("继电器不可用 (未连接 GPIO%d)", RELAY_FAN_GPIO);

<<<<<<< HEAD
    if (fan_pwm_init(FAN_PWM_CHIP, FAN_PWM_CHANNEL) < 0)
        LOG_WARN("风扇 PWM 不可用");

=======
>>>>>>> temp-remote
    if (led_control_init(LED_GPIO) < 0)
        LOG_WARN("LED 不可用 (未连接 GPIO%d)", LED_GPIO);

    if (ir_control_init(IR_TX_GPIO) < 0)
        LOG_WARN("红外发射不可用 (未连接 GPIO%d)", IR_TX_GPIO);

    LOG_INFO("===== 执行器初始化完成 =====");
    return 0;
}

int device_get_all_status(device_status_t *status) {
    memset(status, 0, sizeof(*status));
    strncpy(status->ir_last_code, ir_get_last_code(), sizeof(status->ir_last_code)-1);
    return 0;
}

void device_manager_cleanup(void) {
    relay_control_cleanup();
<<<<<<< HEAD
    fan_pwm_cleanup();
=======
>>>>>>> temp-remote
    led_control_cleanup();
    LOG_INFO("执行器已全部关闭");
}
