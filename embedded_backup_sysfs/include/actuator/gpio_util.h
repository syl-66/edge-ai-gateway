/**
 * gpio_util.h — GPIO sysfs 基础操作
 *
 * 通过 Linux sysfs 接口控制 GPIO:
 *   /sys/class/gpio/export   — 导出 GPIO 到用户空间
 *   /sys/class/gpio/gpioN/   — 设置方向 (direction) 和电平 (value)
 *
 * 适用场景: 简单 GPIO 控制 (LED/继电器/按键)
 * 不适用场景: 高速 GPIO (如 bit-banging), 需用 /dev/mem + mmap
 */

#ifndef GPIO_UTIL_H
#define GPIO_UTIL_H

/**
 * 导出 GPIO 并设为输出模式
 * @param pin  GPIO 编号 (BCM)
 * @return 0=成功, -1=失败
 */
int gpio_export_out(int pin);

/**
 * 写 GPIO 电平值
 * @param pin    GPIO 编号
 * @param value  0=低电平, 非0=高电平
 * @return 0=成功, -1=失败
 */
int gpio_write_val(int pin, int value);

#endif /* GPIO_UTIL_H */
