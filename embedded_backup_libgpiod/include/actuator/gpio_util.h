/**
 * gpio_util.h — GPIO libgpiod 基础操作
 *
 * 通过 Linux libgpiod 字符设备接口控制 GPIO:
 *   /dev/gpiochipN  — GPIO 控制器芯片
 *   gpiod_chip_open / gpiod_line_request_output / gpiod_line_set_value
 *
 * 相比旧 sysfs 接口 (/sys/class/gpio) 的优势:
 *   - 原子操作: 一次 ioctl 完成方向+初值设置, 避免中间态
 *   - 无竞态: 进程退出时内核自动释放 GPIO, 不会残留 export
 *   - 支持批量操作: gpiod_line_set_value_bulk 一次操作多路 GPIO
 *   - 事件机制: 支持 GPIO 边沿中断 (poll/epoll)
 *   - sysfs GPIO 自 Linux 4.8 已废弃, libgpiod 是官方替代
 *
 * 适用场景: 常规 GPIO 控制 (LED/继电器/按键)
 * 不适用场景: 高速 GPIO bit-banging (DHT11/IR发射), 需用 /dev/mem + mmap
 *
 * 依赖: libgpiod (apt-get install libgpiod-dev)
 * 链接: -lgpiod
 */

#ifndef GPIO_UTIL_H
#define GPIO_UTIL_H

#include <gpiod.h>

/**
 * GPIO 线路句柄 (封装 chip + line)
 * 调用者通过 gpio_export_out() 填充, 用完后必须 gpio_release()
 */
typedef struct {
    struct gpiod_chip *chip;   /* GPIO 控制器 (如 gpiochip0) */
    struct gpiod_line *line;   /* 具体线路 (offset 0~31) */
    int pin;                   /* 全局 GPIO 编号 (调试用) */
} gpio_line_t;

/**
 * 导出 GPIO 并设为输出模式 (libgpiod 实现)
 *
 * 实现细节:
 *   1. 根据 pin 号定位 /dev/gpiochipN 和芯片内 offset
 *      i.MX6ULL: gpiochip0 = GPIO1 (pin 0~31), gpiochip1 = GPIO2 (pin 32~63)
 *   2. gpiod_chip_open() 打开芯片
 *   3. gpiod_chip_get_line() 获取线路
 *   4. gpiod_line_request_output() 请求为输出 (一次原子操作, 比 sysfs 两步安全)
 *
 * @param pin   GPIO 全局编号 (GPIO1_IOxx → xx, GPIO2_IOxx → 32+xx)
 * @param line  输出: 填充 chip/line 句柄, 后续操作传入
 * @return 0=成功, -1=失败 (打印错误原因到日志)
 */
int gpio_export_out(int pin, gpio_line_t *line);

/**
 * 写 GPIO 电平值 (libgpiod 实现)
 *
 * @param line   gpio_export_out() 填充的句柄
 * @param value  0=低电平, 非0=高电平
 * @return 0=成功, -1=失败
 */
int gpio_write_val(gpio_line_t *line, int value);

/**
 * 释放 GPIO 资源 (libgpiod 实现)
 *
 * 操作:
 *   1. 先设低电平 (安全状态)
 *   2. gpiod_line_release() 归还线路给内核
 *   3. gpiod_chip_close() 关闭芯片
 *
 * 注意: 调用此函数后 line 不再可用。
 *       进程异常退出时内核也会自动释放, 这是 libgpiod 相比 sysfs 的优势。
 *
 * @param line  要释放的 GPIO 句柄
 */
void gpio_release(gpio_line_t *line);

#endif /* GPIO_UTIL_H */
