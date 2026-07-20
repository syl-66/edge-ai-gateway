/**
 * status_led.h — 系统状态指示灯 (GPIO)
 *
 * 用于指示网关运行状态:
 *   - 常亮: 正常运行
 *   - 慢闪: 传感器异常
 *   - 快闪: 网络断开
 */

#ifndef STATUS_LED_H
#define STATUS_LED_H

/**
 * 初始化状态 LED 所接的 GPIO
 * @return 0=成功, -1=失败
 */
int status_led_init(void);

/**
 * 设置状态 LED 亮/灭
 * @param on  1=亮, 0=灭
 * @return 0=成功
 */
int status_led_set(int on);

#endif /* STATUS_LED_H */
