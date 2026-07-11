/**
 * relay_control.h — 继电器控制 (GPIO → 三极管 → 继电器线圈)
 *
 * 用于控制外接设备:
 *   - 风扇
 *   - 加湿器
 *   - 其他 AC/DC 设备
 *
 * 硬件: GPIO 输出 → 三极管放大 → 继电器线圈 → 触点吸合/断开
 */

#ifndef RELAY_CONTROL_H
#define RELAY_CONTROL_H

/* 继电器逻辑设备 ID */
#define RELAY_FAN           0

/**
 * 初始化继电器所接的 GPIO 引脚
 * @param gpio_pin  GPIO 编号
 * @return 0=成功
 */
int relay_control_init(int gpio_pin);

/**
 * 控制继电器吸合/断开
 * @param id  继电器 ID (RELAY_FAN)
 * @param on  1=吸合, 0=断开
 * @return 0=成功
 */
int device_relay_set(int id, int on);

/**
 * 断开所有继电器
 */
void relay_control_cleanup(void);

#endif /* RELAY_CONTROL_H */
