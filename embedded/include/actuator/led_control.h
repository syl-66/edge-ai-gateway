/**
 * led_control.h — LED 灯控制 (GPIO sysfs)
 *
 * 硬件接线: GPIO1_IO04 → 220Ω 电阻 → LED+ → LED- → GND
 *   GPIO 高电平 (3.3V) → LED 亮
 *   GPIO 低电平 (0V)   → LED 灭
 *
 * GPIO 操作: sysfs (/sys/class/gpio)
 */

#ifndef LED_CONTROL_H
#define LED_CONTROL_H

int led_control_init(int gpio_pin);
int led_control_set(const char *action);
int led_control_get_state(void);
void led_control_cleanup(void);

#endif
