/**
 * device_manager.h — 执行器统一管理
 *
 * 管理所有输出设备:
 *   - 继电器 (GPIO sysfs)    → relay_control.h
 *   - 红外发射 (NEC 协议)    → ir_control.h
 *
 * 统一接口: device_manager_init() → 全部初始化
 */

#ifndef DEVICE_MANAGER_H
#define DEVICE_MANAGER_H

#include "actuator/gpio_util.h"
#include "actuator/relay_control.h"
#include "actuator/ir_control.h"

typedef struct {
    int relay;             /* 继电器状态: 0=断开 1=吸合 */
    char ir_last_code[32]; /* 最后发送的红外编码 */
} device_status_t;

int  device_manager_init(void);
int  device_get_all_status(device_status_t *status);
void device_manager_cleanup(void);

#endif
